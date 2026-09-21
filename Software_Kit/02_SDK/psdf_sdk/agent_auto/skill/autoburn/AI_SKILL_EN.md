---
name: autoburn
description: "AgentAuto English AI instructions: discover, prepare/build, enter download mode, burn/reset, monitor, HIF parsing, and installation."
lang: en
---

# AgentAuto AI Skill (English)

> **Language: English**
> `lang: en`

## Purpose

`AgentAuto` is the public command name. `autoburn` is the Skill package name. Do not hard-code SDK paths, project paths, COM ports, or baud rates. Discover or confirm them before touching project files or hardware.

The simplest flow is defined by the first section of `README_EN.md`: extract `agent_auto` into the SDK project directory, then let the AI read the English README and this skill, confirm the required values, and automatically compile, flash, reset, and monitor the board.

## Required Confirmation

Before `prepare`, `build`, `burn`, or `workflow`, confirm the following with the user:

- Project folder path, for example `D:\psic_sdk3\project\hello_word`.
- CDK directory: the folder that contains `project.cdkproj`, for example `...\cdk`, `...\cdk_6130_1812_cpus`, or `...\cdk_6240_cpuf`.
- Machine-recognized `COMx` ports used for flashing, console/Shell, startup, and monitoring.
- PC SPI device/interface used by the HIF flow; HIF upgrade entry, HIF collection, and HIF exporting do not use `COMx` by default.
- Baud rates: burn defaults to `921600`, Shell/LOG monitor defaults to `115200`, and HIF PHY follows the project setting.
- Whether the first download requires the user to enter download mode manually.
- Whether to reset/start the firmware automatically after flashing.
- Whether to monitor after startup, including port, duration, `--hex`, and output file.

When the user does not know a value, run `AgentAuto discover`, `doctor`, and `ports`, report the real output, then ask for confirmation. If a required value is missing or ambiguous, ask one concise question listing only the missing values.

## Automatic Demo Flow

For the README-approved automatic flow:

1. Read `README_EN.md` and this `AI_SKILL_EN.md`.
2. Discover and confirm the SDK root, project folder path, CDK directory, and COM ports.
3. Run `prepare`/`build` and require `BUILD_OK=1`.
4. When the firmware supports automatic entry, use `upgrade --mode shell`, `upgrade --mode hif`, or `workflow --enter-mode shell/hif`.
5. On the first firmware download, if automatic entry is unavailable or fails, stop, ask the user to enter download mode manually, then continue.
6. Flash and reset/start the firmware.
7. Monitor the console or LOG UART to confirm the firmware is running.

The README-first-section request is treated as pre-approved. Do not repeat every confirmation unless a value is still unknown or a serial monitor is about to start.

## Environment Discovery

```powershell
AgentAuto discover --json
AgentAuto doctor
AgentAuto ports
```

Checks:

- `SDK_ROOT` must contain both `project/` and `platform/`.
- `CDK_MAKE` must point to `cdk-make.exe`; never use MINGW `make`, `mingw32-make`, or `gcc`.
- `DOWNLOAD_DLL` must point to an existing `DownloadLib.dll`.
- `PROJECT_PATHS` must contain the project folder to compile.
- `PORTS` must contain the confirmed COM ports.

`discover --json` prints `project_paths`; prefer the full project path.

## Burn File Copy Policy

File copying is disabled by default. `AgentAuto burn` uses the SDK
`DownloadLib.dll` and the original firmware image path directly. Do not copy
files for a normal burn, and do not select a copy path yourself.

Only copy when the user explicitly allows it and confirms a local destination.
In that case pass:

```powershell
AgentAuto burn --project %PROJECT_PATH% --config %CDK_DIR% --port %COMPORT% --baud 921600 --copy-dir D:\path\to\local\staging
```

If `DownloadLib.dll` or the firmware image is on a UNC/network path, or the
user's PC reports a DownloadLib network-location/load error, stop before
burning. Ask the user whether copying is allowed and which local path to use,
then run `burn` with `--copy-dir`. Never copy automatically and never invent
the destination path.

## Common Commands

```powershell
AgentAuto prepare --project %PROJECT_PATH% --config %CDK_DIR%
AgentAuto build --project %PROJECT_PATH% --config %CDK_DIR%
AgentAuto workflow --project %PROJECT_PATH% --config %CDK_DIR% --port %COMPORT% --baud 921600
AgentAuto burn --project %PROJECT_PATH% --config %CDK_DIR% --port %COMPORT% --baud 921600
AgentAuto monitor --port %COMPORT% --baud 115200 --duration 10
```

Parameter notes:

- `--project`: project directory name or absolute project folder path.
- `--config`: CDK directory containing `project.cdkproj`, or a CDK build configuration name.
- `--port`: DownloadLib download COM port.
- `--console-port`: console COM port when it differs from the download port.
- `--baud`: DownloadLib baud, default `921600`.
- `--shell-baud`: console/HIF baud for automatic download entry.
- `--enter-mode shell`: use the shell `upgrade` command for download entry.
- `--enter-mode hif`: use the HIF wake and reboot handshake for download entry.
- `--dry-run`: validate only, without touching project files or opening serial ports.

`--config` can be omitted when the project has exactly one CDK directory. When it has multiple CDK directories, first match the user-specified board to a directory name; ask for `--config` only when no unique match exists.

## COM And SPI Channel Responsibilities

Resolve the channel before running the flow. This rule does not change when moving to another PC, board, or tool copy:

1. Use machine-recognized `COMx` ports for:
   - Flashing firmware (UART mode only).
   - Entering upgrade/download mode through Shell.
   - Resetting firmware with the fixed radar reset flow: `0x7E` sync -> write `0x40009070=0x00000000` -> `0x13` BROM reset.
   - Serial monitoring (Shell/LOG/console).
2. Use the PC's SPI device/interface (for example CH347T SPI or the `HifMsgDataCollectionLib` SPI API) for:
   - Entering upgrade/download mode through HIF.
   - Collecting HIF device data.
   - Exporting collected HIF data to document/table files.
     Do not mistakenly run these HIF steps over a `COMx` UART.
3. When the project uses HIF SPI:
   - HIF wake example frame: `55 FF 55 FF`
   - HIF upgrade example frame: `A5 30 15 04 01 10 02 E8 FB FE EF`
   - HIF collection uses an SPI DLL/API, for example `HifMsgDataCollectionApi.OpenSpiDevice` / `StartCollectingData`.
4. On a new environment, rediscover:
   - `COMx` devices for flashing, Shell upgrade entry, `0x13` reset, and monitoring.
   - SPI devices for HIF upgrade entry, HIF collection, and HIF exporting.
     Never mix these channels just because the environment changed.

## Download Mode BROM Commands

After the board is in download mode, use `brom-send` when a single BROM command
must be sent and its reply read. `--dry-run` only validates and prints the
frame that would be sent; it does not open the COM port.

Write SRAM with command ID `0x20`:

```powershell
AgentAuto brom-send --port %COMPORT% --baud 921600 --cmd 0x20 --mode sram-write --addr 0x20000000 --len 4 --data "12 34 56 78"
```

Radar reset must run the following sequence in order. Missing any step means
the reset is not considered successful:

1. Send `0x7E` to synchronize with the board BROM and receive the sync reply; success marker: `BROM_SYNC_OK=1` (the DownloadLib path also reports `CONNECT_CODE=89`).
2. Use SRAM write `0x20` at fixed address `0x40009070` with fixed 4-byte data `0x00000000`; receive the header ACK and then the data ACK; success markers: `BROM_RESET_SRAM_ADDR=0x40009070`, `BROM_RESET_SRAM_DATA=00 00 00 00`, `BROM_SRAM_WRITE_OK=1`.
3. Only then send BROM reset command ID `0x13`; receive the matching ACK `50 53 49 43 02 03 64 66 00 00 00 00`; success markers: `BROM_RESET_ACK_MATCH=1`, `BROM_RESET_OK=1`.

Use the integrated command below. `brom-reset` runs steps 1 -> 2 -> 3 inside
the script and enforces `--cmd 0x13`:

```powershell
AgentAuto brom-send --port %COMPORT% --baud 921600 --cmd 0x13 --mode brom-reset
```

The old flow that sends only `0x13` without `0x7E` sync and without writing
`0x40009070=0x00000000` is deprecated and must not be used as evidence of a
successful radar reset. The reset frame is
`50 53 49 43 01 02 4D 66 05 00 00 00 13 01 00 00 00`, with default validation
`50 53 49 43 02 03 64 66 00 00 00 00`; use `--expected-ack` only to override
the final `0x13` expected reply.

`brom-send` sends through `DownloadLib.BromSendData` and hooks the download
thread ACK callback to report `BROM_RX_HEX`; `BROM_RX_PAYLOAD_HEX` is also
reported when the reply contains data. Before a real send, confirm that the
board is in download mode and that the address, length, and data are safe for
the target being debugged.

## Manual Steps

After environment discovery:

```powershell
AgentAuto prepare --project %PROJECT_PATH% --config %CDK_DIR%
AgentAuto build --project %PROJECT_PATH% --config %CDK_DIR%
AgentAuto burn --project %PROJECT_PATH% --config %CDK_DIR% --port %COMPORT% --baud 921600
```

Read-back verification is enabled by default. Only pass
`--no-read-back-check` when the user explicitly chooses to skip it. A
successful enabled run prints `READ_BACK_CHECK=enabled` and
`READ_BACK_CHECK_OK=1`; a failing comparison is reported as a DownloadLib
read-back error and must not be treated as a successful burn.

Add `--dry-run` before the first burn unless the user explicitly ordered a direct flash. After a successful burn, expect `AUTO_RESET_OK=1`. To confirm runtime behavior, run:

```powershell
AgentAuto monitor --port %COMPORT% --baud 115200 --duration 10
```

Confirm the monitor port, baud, duration, and output parameters with the user first. Do not open a serial port automatically just because build or burn finished.

## Macro Rules

`CONFIG_SHELL` and `CONFIG_SYS_SHELL` are interlocked: `prepare` always adds or keeps both together and never enables only one.

- This rule overrides any README automatic-flow language that says `prepare` enables Shell/HIF macros by default.
- By default, do not add Shell or HIF features to a project. Unless the user explicitly asks to add either feature, do not write `CONFIG_SHELL`, `CONFIG_SYS_SHELL`, `CONFIG_HIF`, HIF PHY macros, matching sources, include paths, or linker settings.
- Additional project features include but are not limited to: `CONFIG_SHELL_*`/Shell commands, `CONFIG_SYS_SHELL`, `CONFIG_HIF_*`/HIF PHY, MMW/HIF source modules, and CDK include/linker settings.
- Only edit `prj_config.h`, `.cdkproj`, or related project files to add Shell/HIF functionality after the user explicitly asks.
- Choosing `upgrade --mode shell` or `upgrade --mode hif` only selects how to enter download mode; it is not authorization to add feature macros.
- If the current project/firmware lacks the selected feature, stop and ask the user; do not run `prepare` to add the Shell/HIF integration without authorization.
- In interactive work, ask the user once before modifying `prj_config.h`.
- If the user declines shell macros, continue `prepare`/`build` without enabling the shell pair.
- For HIF entry, also confirm `CONFIG_HIF` and the HIF PHY UART configuration.

## Board And CDK Rules

- When a project stores `project.cdkproj` in a `cdk` directory, the board is selected by the `CONFIG_BOARD_*` compile macros in that `project.cdkproj`. Use `platform/boards/board_config.h` as the source of valid macro names.
- After the user describes the board, resolve one matching `CONFIG_BOARD_*` macro and write it into the `Define` entries under `BuildConfig/Compiler` and `BuildConfig/Asm` in `project.cdkproj`; do not put board macros in `prj_config.h`. AgentAuto comments out old board macros in `app/inc/prj_config.h` to prevent duplicate definitions. Feature macros such as `CONFIG_SHELL`, `CONFIG_SYS_SHELL`, and `CONFIG_HIF` stay in `prj_config.h`.
- When the project has multiple board-specific `cdk*` directories, match the user board to exactly one directory and compile with that directory as `--config` without modifying the shared `prj_config.h`.
- If no unique macro or CDK directory matches, multiple directories map to the same board, or the board is outside the project's supported set, list the candidates and ask the user.

## Configuration And Priority

`AgentAuto.conf` stores machine defaults under `[default]`:

```ini
[default]
sdk = D:\psic_sdk3
cdk = C:\C-Sky\CDK\cdk-make.exe
dll = D:\psic_sdk3\agent_auto\assets\DownloadLib.dll
project = D:\psic_sdk3\project\hello_word
config = D:\psic_sdk3\project\hello_word\cdk
port = COM109
console-port = COM109
```

Effective priority:

1. Command-line arguments.
2. `AgentAuto.conf`, merged as `[default]`, base module section, then subcommand section.
3. Environment variables.
4. Automatic discovery from cwd, SDK parents, standard CDK install paths, and bundled assets.

Common environment variables:

| Variable              | Purpose                                  |
| --------------------- | ---------------------------------------- |
| `AGENT_AUTO_SDK_ROOT` | SDK root                                 |
| `AGENT_AUTO_CDK`      | absolute `cdk-make.exe` path             |
| `CDK_MAKE`            | compatibility alias for `AGENT_AUTO_CDK` |
| `AGENT_AUTO_DLL`      | absolute `DownloadLib.dll` path          |
| `AGENT_AUTO_PYTHON`   | `python.exe` used by launchers           |
| `AGENT_AUTO_CONFIG`   | alternate config path                    |
| `AGENT_AUTO_SKILLS`   | Skill install destination                |

## Path Resolution

| Placeholder         | Meaning                                                                            |
| ------------------- | ---------------------------------------------------------------------------------- |
| `%SDK_ROOT%`        | SDK root                                                                           |
| `%PROJECT_PATH%`    | absolute project folder path                                                       |
| `%CDK_DIR%`         | folder containing `project.cdkproj`                                                |
| `%COMPORT%`         | download COM port                                                                  |
| `%CONSOLE_COMPORT%` | console COM port                                                                   |
| `%HIF_COMPORT%`     | HIF COM port (only when the user explicitly requests HIF UART and confirms `COMx`) |
| `%IMAGE%`           | firmware image path                                                                |

Project resolution:

1. Use the absolute folder directly when `--project` is an absolute path.
2. Use the current directory when inside `project/<name>`.
3. Use the only project when exactly one exists.
4. List `PROJECT_PATHS` and ask the user when multiple projects exist.

CDK resolution:

1. Use the `project.cdkproj` directory passed with `--config`.
2. Use the `cdk` directory under the project.
3. Use the only `cdk*` directory under the project.
4. With multiple `cdk*` directories, first match the user-specified board to a directory name; ask the user for `--config` only when no unique match exists.

Firmware image resolution:

1. Use `--image`.
2. Use the newest `%PROJECT_PATH%/%CDK_DIR%/output/*.img`.

## Build Rules

- `.cdkproj` is the compile source of truth; `.cdkws` is only the CDK workspace container.
- Invoke the system CDK tool, for example `cdk-make.exe /w project.cdkws /p project.cdkproj /c <config> /d build /v`.
- Pass `/p` explicitly to the active `.cdkproj`.
- Never use MINGW `gcc`, `make`, or `mingw32-make` as the build chain.

## New Project From hello_word

```powershell
Copy-Item "%SDK_ROOT%\project\hello_word" "%SDK_ROOT%\project\hello_word_mmw_hif_demo" -Recurse
```

Update the project identity in `cdk/project.cdkws` and `cdk/project.cdkproj`, then:

```powershell
AgentAuto prepare --project "%SDK_ROOT%\project\hello_word_mmw_hif_demo" --config "%SDK_ROOT%\project\hello_word_mmw_hif_demo\cdk"
AgentAuto build --project "%SDK_ROOT%\project\hello_word_mmw_hif_demo" --config "%SDK_ROOT%\project\hello_word_mmw_hif_demo\cdk"
```

## Source Refresh And Exclusion

```powershell
AgentAuto refresh-sources --project %PROJECT_PATH% --config %CDK_DIR%
AgentAuto exclude --project %PROJECT_PATH% --config %CDK_DIR% --source app\src\bad_source.c
AgentAuto excluded --project %PROJECT_PATH% --config %CDK_DIR%
AgentAuto unexclude --project %PROJECT_PATH% --config %CDK_DIR% --source app\src\bad_source.c
```

The exclusion list is stored in `autoburn_exclude.txt` inside the selected CDK directory. Keep excluded files on disk.

## Troubleshooting

| Problem                                 | Resolution                                                                                             |
| --------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| New sources are not compiled            | Run `refresh-sources`, then rebuild                                                                    |
| MMW/HIF symbols are undefined           | Run `prepare` to add macros, sources, and linker settings                                              |
| CDK not found or MINGW selected         | Run `doctor`; confirm `CDK_MAKE=` points to `cdk-make.exe`                                             |
| Multiple CDK directories                | Pass the `project.cdkproj` directory with `--config`                                                   |
| Board cannot auto-enter download mode   | Ask the user to enter download mode manually, then run `burn`                                          |
| No UART output after burn               | Confirm monitor port and baud, then flash the matching project image                                   |
| DownloadLib network-location/load error | Ask the user whether copying is allowed and which local path to use, then run `burn --copy-dir <path>` |

## Guardrails

- Do not invent SDK paths, project paths, COM ports, baud rates, durations, or log markers.
- Do not open serial ports, modify project files, or flash before the required values are confirmed.
- `CONFIG_SHELL` and `CONFIG_SYS_SHELL` must be enabled or disabled together.
- Unless the user explicitly asks to add Shell/HIF functionality, never use `prepare` or another step to write `CONFIG_SHELL`, `CONFIG_SYS_SHELL`, `CONFIG_HIF`, or related source/linker configuration into the project.
- COM/SPI channel rules are fixed: flashing, Shell upgrade entry, `0x13` reset, and monitoring use `COMx`; HIF upgrade entry, HIF collection, and HIF exporting use the PC SPI device and must not be mixed with `COMx`.
- The radar reset flow is fixed: send `0x7E` sync and receive a reply, use `0x20` to write `0x40009070=0x00000000` and receive ACK(s), then send the `0x13` BROM reset and verify its ACK.
- Burn file copying is disabled by default; never copy automatically or choose the copy destination without user confirmation.
- On the first firmware download, if the board firmware cannot auto-enter download mode, require the user to enter download mode manually before `burn`.
- Compile only with the CDK toolchain and `.cdkproj` as the project target.
- `prepare`, `build`, and `refresh-sources` modify project files; run them only when requested or required by the workflow.
- Do not reuse an old `.img` after a failed build or burn.
- Keep the external command name `AgentAuto`; do not introduce `autoburn` as a new public command.
