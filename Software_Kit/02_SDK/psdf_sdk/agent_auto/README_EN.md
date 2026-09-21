# AgentAuto User Guide (English)

> Language: English
> Language tag: `lang: en`

## Introduction

AgentAuto is a unified automation toolkit for Possumic SDK development boards. It is used for project compilation, firmware release, serial debugging, and radar HIF data analysis. The toolkit organizes different function modules through a unified entry point and provides companion Python tools and Agent Skill documents, without depending on fixed SDK paths, project paths, COM ports, or baud rates.

### Supported Features

- Environment assistance: automatically discovers the SDK, projects, CDK, DownloadLib, and available COM ports, then checks and diagnoses the environment.

- Project preparation: enables or maintains board-specific Shell/HIF build settings and refreshes the CDK project source list.

- Project build: compiles the specified project with the system CDK toolchain and can optionally exclude or restore sources that temporarily do not participate in compilation.

- Release flow: supports step-by-step operation or one-run completion of the full flow: preparation, compilation, device download/upgrade-mode entry, flashing, reset/start, and monitoring.

- Firmware flashing: writes firmware through DownloadLib, with optional read-back verification and reset/start after flashing, and handles cases where the first download requires manual download-mode entry.

- Serial debugging: configures and sends Shell text or raw HIF bytes, and monitors console, LOG, or HIF serial output.

- Download-mode debugging: sends BROM commands while the board is in download mode and obtains the ACK or returned data.

- HIF data transmission: supports waking HIF, sending raw bytes, entering upgrade/download mode, and parsing HIF hex logs, RX_HEX logs, and raw binary captures.

- HIF data parsing: scans HIF frames, validates frame headers and checksums, and reassembles fragmented HIF messages and fragmented DataCube data.

- DataCube data processing: collects HIF data from UART/COM or CH347T SPI sessions and exports C1/C2 DataCube reports as raw files, IQ files, NumPy arrays, and session manifests; offline parsing of existing captures is also supported.

- Point cloud data processing: converts point cloud reports into motion/presence point cloud manifests and CSV data for further analysis.

- Visualization and analysis: supports DataCube range spectra, point cloud overview/frame plots, live 2D-FFT heatmaps, and offline 2D-FFT/HIF data analysis and demos.

- Data table export: exports HIF collection sessions to debug-tool-style one-dimensional frame info, motion point cloud, and presence point cloud tables.

## Usage Features

### Agent Usage

1. Place `agent_auto` in the current SDK directory and set the SDK folder as the agent chat workspace.
2. Describe the project, board, flashing port, console/HIF ports, and baud rates to the agent. If details are unknown, use environment discovery, environment checks, and COM port enumeration first to confirm them.
3. The agent validates the environment and parameters, then completes preparation, build, download-mode entry, flashing, reset/start, and monitoring according to the user-approved flow.

#### Agent Usage Template

```text
Please use AgentAuto to process project <project path>, board <board>, CDK project directory <CDK directory>,
and flashing COM port <COM port>. Use <Shell or HIF> to enter flashing mode and check the corresponding build macro configuration.
If changes to project.cdkproj, prj_config.h, source code, or build configuration are required, explain the changes and impact first,
then proceed only after confirmation. After confirmation, complete compilation, enter flashing mode, flash, reset/start, and monitor, then report logs and results.
```

#### Agent Usage Example

```text
Please use AgentAuto to process project RS6x_7x_mmWave_sdk_V2.1.2\Software_Kit\02_SDK\
psdf_sdk\project\mmwave\mmwave_application\Posture, board 6240,
CDK project directory RS6x_7x_mmWave_sdk_V2.1.2\Software_Kit\02_SDK\psdf_sdk\
project\mmwave\mmwave_application\Posture\cdk_6240_cpuf,
and flashing COM port COM109. Use HIF to enter flashing mode and check the corresponding build macro configuration.
If changes to project.cdkproj, prj_config.h, source code, or build configuration are required, explain the changes and impact first,
then proceed only after confirmation. After confirmation, complete compilation, enter flashing mode, flash, reset/start, and monitor, then report logs and results.
```

# Release Directory

```text
agent_auto/
  README_CN.md
  README_EN.md
  ReleaseNote_CN.md
  ReleaseNote_EN.md
  AgentAuto.conf
  AgentAuto.cmd
  agentcore.cmd
  install.cmd
  assets/
    DownloadLib.dll
  modules/
    core/cli.py
    workflow/module.py        one-click release flow (discover, prepare, build, flash, reset, monitor)
    upgrade/module.py         enter download/upgrade mode (Shell or HIF)
    burn/module.py            firmware flashing and read-back verification
    hif/module.py             HIF data sending, upgrade entry, log parsing
    monitor/module.py         console/LOG/HIF serial monitoring
    datarecord/               raw UART/HIF data recording
    dataparse/                HIF, DataCube, point cloud parsing
  runtime/
    agentauto_engine.py
  skill/autoburn/
    AI_SKILL_CN.md
    AI_SKILL_EN.md
  tools/
    agentauto.py              offline 2D-FFT/HIF data analysis
    hif_2dfft_live.py         live 2D-FFT heatmap
    hif_1dfft_live.py         1D-FFT live processing
    hif_pointcloud_sample.py  point cloud HIF sample processing
```
