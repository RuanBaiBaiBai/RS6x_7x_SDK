# AgentAuto Release Note (English)

> **Language: English**
> `lang: en`

## This Version

- `AgentAuto` is the unified entry; first-level commands invoke the modules.
- `--project` accepts a project directory name or an absolute project folder path.
- `--config` selects the CDK directory that contains `project.cdkproj`.
- `discover --json` prints `project_paths` so the full project path can be used.
- All content Markdown documents are split into Chinese and English versions.
- Compilation targets `.cdkproj` and uses the system CDK toolchain only.
- `CONFIG_SHELL` and `CONFIG_SYS_SHELL` are enabled together.
- Source refresh preserves the project's `ExcludeProjConfig` directories and enables only detected HIF/MMW modules.
- On the first firmware download, if the board firmware cannot auto-enter download mode, the user must enter download mode manually.

## Bundled Modules

```text
modules/
  workflow/     standard release flow
  upgrade/      enter download mode
  burn/         flash firmware
  hif/          send, enter, parse HIF
  monitor/      UART monitor
  datarecord/   
  dataparse/    
```

## Document Map

- [User Guide (Chinese)](README_CN.md)
- [User Guide (English)](README_EN.md)
- [AI Skill (Chinese)](skill/autoburn/AI_SKILL_CN.md)
- [AI Skill (English)](skill/autoburn/AI_SKILL_EN.md)
- [Default config](AgentAuto.conf)
