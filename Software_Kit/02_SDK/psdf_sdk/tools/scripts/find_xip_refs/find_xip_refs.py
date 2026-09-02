# -*- coding: utf-8 -*-
"""
在 RISC-V 反汇编（.asm/.lst）文件中，从指定【函数名】出发，
递归列出其 jal / jalr 调用链：进入每个被调用的函数继续检索，
直到没有 jal/jalr 调用为止。每条调用标注 XIP / SRAM。

规则：
- 反汇编每行格式: `地址 : 机器码 指令 操作数`
- 函数以  `<addr> <func_name>:`  行开始，以空行结束。
- 只关心 jal / jalr 两类指令，目标以 `地址 <符号>` 形式出现，
  例如:  jal 507804 <mmw_point_cloud_get_xxx>
- 目标地址去掉前导 0 后，首位为 '5' => SRAM 区，首位为 '9' => XIP 区。
- 递归: 对每个被调函数，若其在本文件中有定义，则进入继续检索；
  已访问过的函数不再展开（防止递归环），标记 (已展开)。
- 排除列表: 通过配置文件 [exclude] 段指定不展开的函数（一行一个，
  # 注释，支持 fnmatch 通配如 printf*）。命中的函数直接跳过，不打印
  不计数不递归。

配置文件 (默认脚本同目录 config.ini, 可用 --config 指定):
    [input]
    asm  = 反汇编文件绝对路径
    [output]
    csv  = 输出 CSV 路径 (留空则用 <函数名>_xip.csv)
    [exclude]
    functions =
        memset
        printf*

用法:
    python find_xip_refs.py 函数名
    python find_xip_refs.py 函数名 --config myconfig.ini
    python find_xip_refs.py 函数名 --asm 路径.asm --csv out.csv   (覆盖配置)
    python find_xip_refs.py 函数名 --no-csv --max-depth 10
"""

import argparse
import configparser
import csv
import fnmatch
import os
import re
import sys

# 脚本所在目录, 用于定位默认配置文件
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# ===== 默认配置文件 (脚本同目录 config.ini) =====
DEFAULT_CONFIG = os.path.join(SCRIPT_DIR, 'config.ini')

# ===== 配置缺省时的回退反汇编文件路径 =====
DEFAULT_ASM = 'dump.asm'

# 函数头:  0005076c6 <mmw_point_cloud_process>:
FUNC_HEADER_RE = re.compile(r'^\s*0*([0-9a-fA-F]+)\s+<([^>]+)>:\s*$')

# 指令行:  5076de :   221d   jal 507804 <symbol>
INSN_LINE_RE = re.compile(r'^\s*([0-9a-fA-F]+)\s*:\s*(.*)$')

# 只匹配 jal / jalr 指令，并取出目标地址与符号。
# 目标地址带可选 0x 前缀, 至少 3 位十六进制。
JALR_RE = re.compile(
    r'\b(jalr|jal)\b.*?\b(?:0x)?([0-9a-fA-F]{3,})\s+<([^>]+)>')


def region_of(addr_hex):
    """根据地址首个有效十六进制位判断区域。返回 'XIP' / 'SRAM' / 'OTHER'。"""
    stripped = addr_hex.lstrip('0')
    if not stripped:  # 地址全 0
        return 'OTHER'
    first = stripped[0].lower()
    if first == '9':
        return 'XIP'
    if first == '5':
        return 'SRAM'
    return 'OTHER'


def load_config(path):
    """
    读取 INI 配置文件，返回 dict(func, asm, csv, excludes)。
    - [input] func : 起始函数名
    - [input] asm  : 反汇编文件路径
    - [output] csv : 输出 CSV 路径 (留空表示用默认 <函数名>_xip.csv)
    - [exclude] functions : 多行, 一行一个函数名/通配模式, # 注释
    文件不存在则返回全空 (各项为 None / [])。
    """
    cfg = {'func': None, 'asm': None, 'csv': None, 'excludes': []}
    if not path or not os.path.isfile(path):
        return cfg

    parser = configparser.ConfigParser()
    # 保留大小写 (函数名区分大小写)
    parser.optionxform = str
    parser.read(path, encoding='utf-8-sig')

    if parser.has_option('input', 'func'):
        v = parser.get('input', 'func').strip()
        cfg['func'] = v or None
    if parser.has_option('input', 'asm'):
        v = parser.get('input', 'asm').strip()
        cfg['asm'] = v or None
    if parser.has_option('output', 'csv'):
        v = parser.get('output', 'csv').strip()
        cfg['csv'] = v or None
    if parser.has_option('exclude', 'functions'):
        raw = parser.get('exclude', 'functions')
        for line in raw.splitlines():
            s = line.strip()
            if not s or s.startswith('#'):
                continue
            cfg['excludes'].append(s)
    return cfg


def is_excluded(name, patterns):
    """函数名是否命中任一排除模式 (精确或 fnmatch 通配)。"""
    for p in patterns:
        if name == p or fnmatch.fnmatch(name, p):
            return True
    return False


def parse_all(path):
    """
    解析整个文件，返回 dict: func_name -> {'addr': 地址, 'calls': [call,...]}。
    call 每项: dict(insn, insn_addr, target, symbol, region, lineno, raw)
    """
    funcs = {}
    cur_name = None
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.rstrip('\n')

            # 空行 => 当前函数结束
            if line.strip() == '':
                cur_name = None
                continue

            # 函数头
            m = FUNC_HEADER_RE.match(line)
            if m:
                cur_name = m.group(2)
                funcs[cur_name] = {'addr': m.group(1), 'calls': []}
                continue

            if cur_name is None:
                continue

            # 指令行（仅在某函数内）
            m = INSN_LINE_RE.match(line)
            if not m:
                continue
            insn_addr = m.group(1)
            rest = m.group(2)

            jm = JALR_RE.search(rest)
            if not jm:
                continue
            insn, ref_addr, sym = jm.group(1), jm.group(2), jm.group(3)
            funcs[cur_name]['calls'].append({
                'insn': insn,
                'insn_addr': insn_addr,
                'target': ref_addr,
                'symbol': sym,
                'region': region_of(ref_addr),
                'lineno': lineno,
                'raw': line.strip(),
            })
    return funcs


def main():
    ap = argparse.ArgumentParser(
        description='从函数名出发递归列出 jal/jalr 调用链，并标注 XIP/SRAM')
    ap.add_argument('func', nargs='?',
                    help='起始函数名 (省略则用配置 [input] func)，例如 mmw_point_cloud_process')
    ap.add_argument('--config', default=DEFAULT_CONFIG,
                    help='配置文件路径 (默认: 脚本同目录 config.ini)')
    ap.add_argument('--asm', help='反汇编文件路径 (覆盖配置 [input] asm)')
    ap.add_argument('--csv', help='CSV 输出路径 (覆盖配置 [output] csv)')
    ap.add_argument('--no-csv', action='store_true', help='不输出 CSV 文件')
    ap.add_argument('--max-depth', type=int, default=50, help='最大递归深度 (默认 50)')
    args = ap.parse_args()

    # 加载配置, 命令行参数优先级高于配置, 配置缺省再回退内置默认
    cfg = load_config(args.config)
    func_name = args.func or cfg['func']
    asm_path = args.asm or cfg['asm'] or DEFAULT_ASM
    csv_opt = args.csv or cfg['csv']      # None 表示用 <函数名>_xip.csv
    excludes = cfg['excludes']

    if not func_name:
        print('未指定起始函数: 请在命令行传入函数名, 或在配置 [input] func 中填写。')
        sys.exit(2)

    print('起始函数  : %s' % func_name)
    print('指令关键字: jal / jalr (递归展开被调函数)')
    if os.path.isfile(args.config):
        print('配置文件  : %s' % args.config)
    print('反汇编文件: %s' % asm_path)
    if excludes:
        print('排除列表  : %d 项' % len(excludes))
    print('-' * 64)

    try:
        funcs = parse_all(asm_path)
    except FileNotFoundError:
        print('找不到文件: %s' % asm_path)
        sys.exit(1)

    if func_name not in funcs:
        print('未在文件中找到函数: %s' % func_name)
        sys.exit(2)

    flat = []          # 扁平记录, 供 CSV 输出: (caller, depth, call)
    visited = set()    # 已展开的函数名, 防递归环
    xip_total = [0]

    def walk(name, depth):
        info = funcs.get(name)
        indent = '    ' * depth

        # 函数已展开过 => 不再深入
        if name in visited:
            print('%s<%s> (已展开, 略)' % (indent, name))
            return
        # 函数无定义(外部/库) => 叶子
        if info is None:
            print('%s<%s> (文件中无定义)' % (indent, name))
            return

        visited.add(name)
        # 仅统计排除后的有效调用 (与下方实际列出的条数一致)
        calls = [c for c in info['calls'] if not is_excluded(c['symbol'], excludes)]
        print('%s<%s> @ 0x%s  (%d 处 jal/jalr)' % (indent, name, info['addr'], len(calls)))

        if depth >= args.max_depth:
            print('%s    [达到最大深度 %d, 停止]' % (indent, args.max_depth))
            return

        for c in calls:
            flag = '  <== XIP!' if c['region'] == 'XIP' else ''
            if c['region'] == 'XIP':
                xip_total[0] += 1
            print('%s    %s @ %-8s -> 0x%s <%s>  [%s]%s'
                  % (indent, c['insn'], c['insn_addr'], c['target'],
                     c['symbol'], c['region'], flag))
            flat.append((name, depth, c))
            # 递归进入被调函数
            walk(c['symbol'], depth + 1)

    walk(func_name, 0)

    print('-' * 64)
    if xip_total[0]:
        print('调用链中共发现 %d 处 XIP 引用 (标记 XIP! 处)。' % xip_total[0])
    else:
        print('调用链中未发现 XIP 引用。')

    if not args.no_csv:
        csv_path = csv_opt if csv_opt else '%s_xip.csv' % func_name
        with open(csv_path, 'w', newline='', encoding='utf-8-sig') as f:
            w = csv.writer(f)
            w.writerow(['调用者', '深度', '指令', '指令地址', '目标地址', '目标符号', '区域', '行号', '原始行'])
            for caller, depth, c in flat:
                w.writerow([caller, depth, c['insn'], c['insn_addr'], c['target'],
                            c['symbol'], c['region'], c['lineno'], c['raw']])
        print('已写出 CSV: %s' % csv_path)


if __name__ == '__main__':
    main()
