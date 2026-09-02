import os, stat
import subprocess
import shutil
import re
import argparse
import time
import locale
import sys
import json
import glob
import traceback
from pathlib import Path

from collections import Counter

import xml.etree.ElementTree as ET

from packaging import version

from colorama import init, Fore, Style




class PathNode:
    def __init__(self, name: str, is_dir: bool = False, parent: "PathNode" = None):
        self.name = name
        self.is_dir = is_dir
        self.parent = parent
        self.children = {}
        self.sram_code_size = 0
        self.sram_data_size = 0
        self.flash_size = 0
        self.size = 0
        self.size_human = ""
        self.path = ""
        self.rel_path = ""
        self.depth = 0
        self.info = dict()

class DirectoryTreeBuilder:
    def __init__(self, root_path: str = "."):
        self.root = PathNode(root_path, is_dir=True)
        self.root.path = os.path.abspath(root_path)
        self.all_nodes: list[PathNode] = [self.root]

    # @staticmethod
    def add_path(self, rel_path: str, info: dict):
        if not rel_path:
            return

        full_path = os.path.abspath(rel_path)
        parts = Path(rel_path).parts
        current = self.root
        current_path_parts = []

        for i, part in enumerate(parts):
            current_path_parts.append(part)
            current_rel_path = os.path.normpath(os.path.join(*current_path_parts))
            is_last = i == len(parts) - 1
            is_dir = not is_last or os.path.isdir(full_path)

            # print(f'part: {part}, {is_dir}')

            if part not in current.children:
                node = PathNode(part, is_dir=is_dir, parent=current)
                node.depth = current.depth + 1
                current.children[part] = node
                self.all_nodes.append(node)
            else:
                node = current.children[part]

            if is_last:
                node.rel_path = rel_path
                node.path = full_path

                node.sram_code_size = info['sram-code-size']
                node.sram_data_size = info['sram-data-size']
                node.flash_size = info['flash-size']

            current = node

    def add_paths(self, rel_paths: dict):
        for name, p in rel_paths.items():
            self.add_path(p['path'], p)

    def sum_children_node_size(self, node: PathNode):
        # print(f'enter node {node.name}')
        children = list(node.children.values())
        for i, child in enumerate(children):
            # print(f'node {node.name}, chaild {child.name}')
            self.sum_children_node_size(child)
            node.sram_code_size += child.sram_code_size
            node.sram_data_size += child.sram_data_size
            node.flash_size += child.flash_size

    def get_root_node(self):
        self.sum_children_node_size(self.root)
        return self.root

    # ===================== =====================
    def _print_tree(self, node: PathNode, prefix: str = "", is_last: bool = True):
        connector = "\\--- " if is_last else "+--- "
        file_tree = f"{prefix}{connector}{node.name}"

        sram_code_size = f"{'':>17}"
        if node.sram_code_size > 0:
            if node.sram_code_size > 1024:
                sram_code_size = f"{node.sram_code_size/1024:>4.2f}K / {node.sram_code_size:>6}"
            else:
                sram_code_size = f"{node.sram_code_size:>17}"

        sram_data_size = f"{'':>17}"
        if node.sram_data_size > 0:
            if node.sram_data_size > 1024:
                sram_data_size = f"{node.sram_data_size/1024:>4.2f}K / {node.sram_data_size:>6}"
            else:
                sram_data_size = f"{node.sram_data_size:>17}"

        flash_size = f"{'':>17}"
        if node.flash_size > 0:
            if node.flash_size > 1024:
                flash_size = f"{node.flash_size/1024:>4.2f}K / {node.flash_size:>6}"
            else:
                flash_size = f"{node.flash_size:>17}"

        print(f"{file_tree:64} {sram_code_size:>17} | {sram_data_size:>17} | {flash_size:>17} |")

        children = list(node.children.values())
        for i, child in enumerate(children):
            new_prefix = prefix + ("     " if is_last else "|    ")
            self._print_tree(child, new_prefix, i == len(children) - 1)

    def print_tree(self):
        self._print_tree(self.root)



class SizeReport():

    def get_file_list(self, project_dir, sdk_dir, project_name):
        project_mk = os.path.normpath(os.path.join(project_dir, str(project_name) + '.mk'))

        file_list = []

        with open(project_mk, "r") as f:
            found_start = False
            while True:
                line = f.readline()
                if not line:
                    break

                if "Always_Link:" in line:
                    found_start = True

                if found_start:
                    if line.startswith("$(IntermediateDirectory)"):
                        build_file = line.split(': ', 1)[1]
                        build_file = build_file.replace(" ", "")
                        build_file = build_file.replace("\n", "")
                        file_list.append(build_file)
                        # print(build_file)

        file_path_list = []
        for build_file in file_list:
            file_path = os.path.normpath(os.path.join(project_dir, build_file))
            file_rel_path = os.path.relpath(file_path, sdk_dir)
            file_path_list.append(file_rel_path)
            # print(file_rel_path)

        return file_path_list



    def get_section_info(self, map_file_path):

        section_list = []
        first_section_name = None
        last_section_name = None

        with open(map_file_path, "r") as f:
            found_start = False
            found_idx = 0

            while True:
                line = f.readline()
                if not line:
                    break

                if "Section Headers:" in line:
                    found_start = True
                    found_idx = 0

                if found_start:
                    found_idx += 1
                    if found_idx > 15:
                        break

                    if found_idx < 5:
                        continue

                    section_line = line[7:]
                    section_item = section_line.split()

                    section_name = section_item[0]
                    section_addr = int(section_item[2], 16)
                    section_size = int(section_item[4], 16)

                    if last_section_name == None:
                        last_section_name = section_name

                    if section_size != 0 and section_addr != 0:
                        if first_section_name == None:
                            first_section_name = section_name
                        section_ = {'name': section_name,
                                    'addr': section_addr,
                                    'size': section_size}
                        section_list.append(section_)

                        last_section_name = None

        section_info = {"first_symbol": first_section_name,
                        "last_symbol": last_section_name,
                        "section_list": section_list}

        return section_info



    def get_symbols_info(self, map_file_path, first_symbol, last_symbol):
        symbols_list = list()
        fill_list = list()
        miss_list = list()

        with open(map_file_path, "r") as f:
            found_start = False
            symbol_group = list()
            symbol_group_size = 0
            while True:
                line = f.readline()
                if not line:
                    break

                if not found_start:
                    if line.startswith(first_symbol):
                        found_start = True
                    continue

                else:
                    if line.startswith(last_symbol):
                        break

                if not line.startswith('  '):
                    # process prev symbols
                    # size == 1:
                    #       1. only name: abandon, *startup.o(*.text)
                    #       2. name + addr + size,
                    #           2.1 fill : *fill*         0x0050005c       0x24
                    #           2.2 section size: isram_text      0x00500000     0xa6f0
                    #       3. name + addr + size + obj:
                    # size > 1:
                    #       1. only name:
                    #           1.1 second line: addr + size + obj
                    #       2. name + addr + size + obj:
                    # size > 1
                    if symbol_group_size > 0:
                        # print('-' * 16)
                        # print(symbol_group)

                        if symbol_group_size == 1:
                            line_sym = symbol_group[0].lstrip()
                            line_item = line_sym.split()
                            line_item_len = len(line_item)
                            if line_item_len >= 4:
                                if line_item[1].startswith('0x') and line_item[2].startswith('0x'):
                                    sym_obj = line_item[3]
                                    sym_obj = re.sub(r"\(.*?\)$", "", sym_obj)
                                    sym_obj = os.path.basename(sym_obj)
                                    sym_size = int(line_item[2], 16)
                                    sym_addr = int(line_item[1], 16)
                                    sym_dict = {"name": sym_obj,
                                                "addr": sym_addr,
                                                "size": sym_size}
                                    symbols_list.append(sym_dict)
                                    # print(f'sym: {sym_obj}, addr: {sym_addr}, size: {sym_size}')
                                else:
                                    miss_list.append(line_item)
                                    # print(f'miss {line_item}')
                            elif line_item_len == 3:
                                if '*fill*' == line_item[0]:
                                    sym_dict = {"name": line_item[0],
                                                "addr": int(line_item[1], 16),
                                                "size": int(line_item[2], 16)}
                                    fill_list.append(sym_dict)
                                    # print(f'fill: {line_item}')
                        else:
                            line_sym = symbol_group[0].lstrip()
                            line_item = line_sym.split()
                            line_item_len = len(line_item)
                            if line_item_len >= 4:
                                if line_item[1].startswith('0x') and line_item[2].startswith('0x'):
                                    sym_obj = line_item[3]
                                    sym_obj = re.sub(r"\(.*?\)$", "", sym_obj)
                                    sym_obj = os.path.basename(sym_obj)
                                    sym_size = int(line_item[2], 16)
                                    sym_addr = int(line_item[1], 16)
                                    sym_dict = {"name": sym_obj,
                                                "addr": sym_addr,
                                                "size": sym_size}
                                    symbols_list.append(sym_dict)
                                    # print(f'sym: {sym_obj}, addr: {sym_addr}, size: {sym_size}')
                                else:
                                    miss_list.append(line_item)
                                    # print(f'miss {line_item}')
                            elif line_item_len == 1:
                                next_line_sym = symbol_group[1].lstrip()
                                next_line_item = next_line_sym.split()
                                next_line_item_len = len(next_line_item)
                                if next_line_item_len >= 3:
                                    if next_line_item[0].startswith('0x') and next_line_item[1].startswith('0x'):
                                        sym_obj = next_line_item[2]
                                        sym_obj = re.sub(r"\(.*?\)$", "", sym_obj)
                                        sym_obj = os.path.basename(sym_obj)
                                        sym_size = int(next_line_item[1], 16)
                                        sym_addr = int(next_line_item[0], 16)
                                        sym_dict = {"name": sym_obj,
                                                    "addr": sym_addr,
                                                    "size": sym_size}
                                        symbols_list.append(sym_dict)
                                        # print(f'sym: {sym_obj}, addr: {sym_addr}, size: {sym_size}')
                                    else:
                                        miss_list.append(next_line_item)
                                        # print(f'miss {next_line_item}')

                    symbol_group.clear()
                    symbol_group.append(line)
                    symbol_group_size = 1
                else:
                    if symbol_group_size > 0:
                        symbol_group.append(line)
                        symbol_group_size += 1

        symbols_info = {"sym": symbols_list,
                        "fill": fill_list}

        return symbols_info


    def report(self):
        # get path
        # ---------------------------------------------------------------
        project_path = os.environ.get("CURRENT_PRJ_PATH")
        sdk_path = os.environ.get("SDK_ROOT_DIR")

        project_cdkws = os.path.normpath(os.path.join(project_path, 'project.cdkws'))

        tree = ET.parse(project_cdkws)
        root = tree.getroot()

        project_name = None
        cfg_name = None
        for sub in root.findall('BuildMatrix'):
            for cfg in sub.findall('WorkspaceConfiguration'):
                for prj in cfg.findall('Project'):
                    project_name = prj.get('Name')
                    cfg_name = prj.get('ConfigName')

        map_file_path = os.path.normpath(os.path.join(project_path, 'Lst', str(cfg_name) + '.map'))

        # print(f'sdk_path: {sdk_path}')
        # print(f'project_path: {project_path}')
        # print(f'project_name: {project_name}, cfg_name: {cfg_name}')
        # print(f'map_file_path: {map_file_path}')



        # get section info
        # ---------------------------------------------------------------
        section_info = self.get_section_info(map_file_path)
        # print(section_info)
        section_list = section_info['section_list']
        mem_section = {
                        'sram-code':    {'start': 0,
                                        'end': 0,
                                        'size': 0},
                        'sram-data':    {'start': 0,
                                        'end': 0,
                                        'size': 0},
                        'sram-stack':   {'start': 0,
                                        'end': 0,
                                        'size': 0},
                        'flash':        {'start': 0,
                                        'end': 0,
                                        'size': 0}
                     }
        for section in section_list:
            if section['name'] == 'isram_text':
                mem_section['sram-code']['start'] = section['addr']
            elif section['name'] == 'isram_rodata':
                mem_section['sram-code']['end'] = section['addr'] + section['size']
                mem_section['sram-code']['size'] = mem_section['sram-code']['end'] - mem_section['sram-code']['start']
            elif section['name'] == 'ixip_text':
                mem_section['flash']['start'] = section['addr']
            elif section['name'] == 'ixip_rodata':
                mem_section['flash']['end'] = section['addr'] + section['size']
                mem_section['flash']['size'] = mem_section['flash']['end'] - mem_section['flash']['start']
            elif section['name'] == 'dsram_data':
                mem_section['sram-data']['start'] = section['addr']
            elif section['name'] == 'dsram_bss':
                mem_section['sram-data']['end'] = section['addr'] + section['size']
                mem_section['sram-data']['size'] = mem_section['sram-data']['end'] - mem_section['sram-data']['start']
            elif section['name'] == 'dsram_stack':
                mem_section['sram-stack']['start'] = section['addr']
                mem_section['sram-stack']['size'] = section['size']
                mem_section['sram-stack']['end'] = section['addr'] + section['size']



        # get symbol info
        # ---------------------------------------------------------------
        # symbol: name.o / name.a
        # addr: start addr
        # size:
        # section name:
        # memory_type:
        symbols_info = self.get_symbols_info(map_file_path, section_info['first_symbol'], section_info['last_symbol'])
        # print(symbols_info)

        # get fill symbol info
        # ---------------------------------------------------------------
        fill_sym_list = symbols_info['fill']
        fill_stat = {
                        'sram-code': 0,
                        'sram-data': 0,
                        'sram-stack': 0,
                        'flash': 0,
                        'other': 0
                     }
        for fill in fill_sym_list:
            fill_addr = fill['addr']
            if fill_addr >= mem_section['sram-code']['start'] and fill_addr <= mem_section['sram-code']['end']:
                fill_stat['sram-code'] += fill['size']
            elif fill_addr >= mem_section['sram-data']['start'] and fill_addr <= mem_section['sram-data']['end']:
                fill_stat['sram-data'] += fill['size']
            elif fill_addr >= mem_section['sram-stack']['start'] and fill_addr <= mem_section['sram-stack']['end']:
                fill_stat['sram-stack'] += fill['size']
            elif fill_addr >= mem_section['flash']['start'] and fill_addr <= mem_section['flash']['end']:
                fill_stat['flash'] += fill['size']
            else:
                fill_stat['other'] += fill['size']


        sym_list = symbols_info['sym']
        sym_file_list = list()
        sym_lib_list = list()
        for sym in sym_list:
            if sym['name'].endswith('.o'):
                sym_file_list.append(sym)
            elif sym['name'].endswith('.a'):
                sym_lib_list.append(sym)

        # get libs info
        # ---------------------------------------------------------------
        libs_stat = dict()
        for sym_lib in sym_lib_list:
            if sym_lib['name'] not in libs_stat:
                # sym_lib_inf = dict()
                sym_lib_inf = { 'sram-code' : 0,
                                'sram-data': 0,
                                'flash': 0}
                libs_stat[sym_lib['name']] = sym_lib_inf

            if sym_lib['addr'] >= mem_section['sram-code']['start'] and sym_lib['addr'] <= mem_section['sram-code']['end']:
                libs_stat[sym_lib['name']]['sram-code'] += sym_lib['size']
            elif sym_lib['addr'] >= mem_section['sram-data']['start'] and sym_lib['addr'] <= mem_section['sram-data']['end']:
                libs_stat[sym_lib['name']]['sram-data'] += sym_lib['size']
            elif sym_lib['addr'] >= mem_section['flash']['start'] and sym_lib['addr'] <= mem_section['flash']['end']:
                libs_stat[sym_lib['name']]['flash'] += sym_lib['size']
            else:
                print(f'error')



        # get file list
        # ---------------------------------------------------------------
        # name: file name
        # path: file path
        # sram-code size:
        # sram-data size
        # flash size:
        # symbol set:
        #               symbol name:
        #               symbol size:
        #               symbol addr:
        #               symbol section:
        file_list = self.get_file_list(project_path, sdk_path, project_name)
        build_file_dict = dict()
        for file in file_list:
            file_name = str(os.path.basename(file))
            file_parent_name = Path(file).parent.name
            out_file_name = re.sub(r"\..$", ".o", file_name)
            build_file_name = str(file_parent_name) + '_' + out_file_name
            if build_file_name in build_file_dict:
                print(f'sample file: {build_file_name}. \n\t\t+{build_file_dict[build_file_name]['path']}\n\t\t+{file}\n')
            else:
                entry = {'name': build_file_name,
                         'path': file,
                         'sram-code-size': 0,
                         'sram-data-size': 0,
                         'flash-size': 0}

                build_file_dict[build_file_name] = entry

        # print(build_file_dict)

        # add sym to file
        # print(sym_file_list)
        for sym_file in sym_file_list:
            sram_code_size = 0
            sram_data_size = 0
            flash_size = 0

            if sym_file['addr'] >= mem_section['sram-code']['start'] and sym_file['addr'] <= mem_section['sram-code']['end']:
                sram_code_size = sym_file['size']
            elif sym_file['addr'] >= mem_section['sram-data']['start'] and sym_file['addr'] <= mem_section['sram-data']['end']:
                sram_data_size = sym_file['size']
            elif sym_file['addr'] >= mem_section['flash']['start'] and sym_file['addr'] <= mem_section['flash']['end']:
                 flash_size = sym_file['size']
            # else:
                # print(f'error: {sym_file['name']}, {hex(sym_file['addr'])}')
                # continue

            if sym_file['name'] in build_file_dict:
                build_file_dict[sym_file['name']]['sram-code-size'] += sram_code_size
                build_file_dict[sym_file['name']]['sram-data-size'] += sram_data_size
                build_file_dict[sym_file['name']]['flash-size'] += flash_size
            else:
                print(f'miss {sym_file['name']}')

        # get file tree
        # ---------------------------------------------------------------
        tree = DirectoryTreeBuilder()
        tree.add_paths(build_file_dict)
        tree_root = tree.get_root_node()



        # show
        # ---------------------------------------------------------------
        print('\n+ file stat')
        print(f'{"file tree":64}[{"sram-code size":>17} | {"sram-data size":>17} | {"xip-code size":>17}] (Bytes)')
        print('-' * 128)
        tree.print_tree()
        print('-' * 128)
        print(f'{"Total":64} {tree_root.sram_code_size/1024:>16.2f} | {tree_root.sram_data_size/1024:>16.2f} | {tree_root.flash_size/1024:>16.2f}')

        print('\n+ memory stat')
        print(f'{'memory-type':>16}: {'start-addr':>16} | {'end-addr':>16} | {'size':>16} (KB / B)')
        print('-' * 128)
        for section_name, section_size in mem_section.items():
            print(f'{section_name:>16}: {hex(section_size['start']):>16} | {hex(section_size['end']):>16} | {section_size['size']/1024:>6.2f}KB / {section_size['size']:>8}')

        print('\n+ lib stat')
        print(f'{'lib-name':>16}: {'sram-code-size':>16} | {'sram-data-size':>16} | {'flash-size':>16} (Bytes)')
        print('-' * 128)
        for lib_name, lib_size in libs_stat.items():
            print(f'{lib_name:>16}: {lib_size['sram-code']:>16} | {lib_size['sram-data']:>16} | {lib_size['flash']:>16}')

        print('\n+ fill stat')
        print(f'{'fill-memory':>16}: {'sram-code-size':>16} | {'sram-data-size':>16} | {'flash-size':>16} (Bytes)')
        print('-' * 128)
        print(f'{' ':>16}: {fill_stat['sram-code']:>16} | {fill_stat['sram-data']:>16} | {fill_stat['flash']:>16}')
