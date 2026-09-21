# AgentAuto 发布说明（中文）

> 语言：中文
> 语种标记：`lang: zh-CN`

## 本版本要点

- `AgentAuto` 统一入口，通过一级命令调用不同模块。
- 工程通过 `--project` 接收工程目录名或工程文件夹绝对路径。
- CDK 工程目录通过 `--config` 确认，即 `project.cdkproj` 所在文件夹。
- `discover --json` 输出 `project_paths`，优先使用完整工程路径。
- 所有 Markdown 内容文档均拆分为中文版和英文版。
- 编译以 `.cdkproj` 为工程目标，仅使用系统 CDK 工具链。
- `CONFIG_SHELL` 与 `CONFIG_SYS_SHELL` 绑定开启。
- 源码刷新会保留工程原有的 `ExcludeProjConfig` 排除目录，只启用检测到的 HIF/MMW 模块。
- 第一次下载固件时，如果开发板固件不支持自动进入烧录模式，需要用户手动进入。

## 内置模块

```text
modules/
  workflow/     标准发布流程
  upgrade/      进入烧录模式
  burn/         烧录固件
  hif/          HIF 发送、进入、解析
  monitor/      串口监控
  datarecord/   
  dataparse/    
```

## 文档索引

- [用户手册（中文）](README_CN.md)
- [用户手册（英文）](README_EN.md)
- [Agent 使用规则（中文）](skill/autoburn/AI_SKILL_CN.md)
- [Agent 使用规则（英文）](skill/autoburn/AI_SKILL_EN.md)
- [默认配置](AgentAuto.conf)
