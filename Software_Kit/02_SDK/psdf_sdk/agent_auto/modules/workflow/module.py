#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AgentAuto workflow module: run the standard release workflow in sequence."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


COMMAND = "workflow"
CLI_PATH = Path(__file__).resolve().parents[1] / "core" / "cli.py"


def _format_command(argv: list[str]) -> str:
    return " ".join(
        f'"{item}"' if " " in item or "\t" in item else item for item in argv
    )


def _run_step(step_name: str, argv: list[str]) -> int:
    if not CLI_PATH.is_file():
        print(f"WORKFLOW_ERROR: AgentAuto CLI was not found at {CLI_PATH}", file=sys.stderr)
        return 2
    print(f"WORKFLOW_STEP={step_name}")
    print(f"WORKFLOW_COMMAND={_format_command(argv)}")
    result = subprocess.run(
        [sys.executable, str(CLI_PATH)] + argv,
        cwd=Path.cwd(),
        check=False,
    )
    if result.returncode != 0:
        print(f"WORKFLOW_FAILED_STEP={step_name}")
    return result.returncode


def run_workflow(args: argparse.Namespace) -> int:
    base_args: list[str] = []
    if args.sdk_root:
        base_args += ["--sdk", args.sdk_root]
    if args.cdk_make:
        base_args += ["--cdk", args.cdk_make]
    if args.dll_path:
        base_args += ["--dll", args.dll_path]

    project_args = ["--project", args.project]
    if args.config:
        project_args += ["--config", args.config]
    if args.board:
        project_args += ["--board", args.board]
    if args.dry_run:
        project_args.append("--dry-run")

    steps = [
        ("discover", base_args + ["discover"]),
        ("prepare", base_args + ["prepare"] + project_args),
        ("build", base_args + ["build"] + project_args),
    ]

    burn_args = ["burn", "--project", args.project, "--port", args.port]
    if args.config:
        burn_args += ["--config", args.config]
    if args.burn_baud:
        burn_args += ["--baud", args.burn_baud]
    if args.shell_baud:
        burn_args += ["--shell-baud", args.shell_baud]
    if args.console_port:
        burn_args += ["--console-port", args.console_port]
    if args.enter_mode:
        burn_args += ["--enter-mode", args.enter_mode]
    if args.enter_shell_command:
        burn_args += ["--enter-shell-command", args.enter_shell_command]
    if args.enter_hex:
        burn_args += ["--enter-hex", args.enter_hex]
    if args.enter_verify:
        burn_args += ["--enter-verify", args.enter_verify]
    if args.enter_wait_ms is not None:
        burn_args += ["--enter-wait-ms", str(args.enter_wait_ms)]
    if args.enter_read_timeout_ms is not None:
        burn_args += ["--enter-read-timeout-ms", str(args.enter_read_timeout_ms)]
    if not args.auto_reset:
        burn_args.append("--no-auto-reset")
    if not args.read_back_check:
        burn_args.append("--no-read-back-check")
    if args.dry_run:
        burn_args.append("--dry-run")
    steps.append(("burn", base_args + burn_args))

    monitor_port = args.monitor_port or args.port
    monitor_args = ["monitor", "--port", monitor_port]
    if args.monitor_baud:
        monitor_args += ["--baud", args.monitor_baud]
    if args.monitor_duration is not None:
        monitor_args += ["--duration", str(args.monitor_duration)]
    if args.monitor_out:
        monitor_args += ["--out", args.monitor_out]
    if args.dry_run:
        monitor_args.append("--dry-run")
    steps.append(("monitor", monitor_args))

    for step_name, argv in steps:
        rc = _run_step(step_name, argv)
        if rc != 0:
            return rc

    print("WORKFLOW_OK=1")
    return 0


def add_parser(sub, engine) -> None:
    parser = sub.add_parser(
        COMMAND,
        help=(
            "Run discover, prepare, build, burn with download entry/reset, "
            "and monitor with one command."
        ),
    )
    parser.add_argument(
        "--project",
        required=True,
        help="Project directory name or absolute project folder path.",
    )
    parser.add_argument("--port", required=True, help="Download/shell COM port.")
    parser.add_argument(
        "--console-port",
        default=None,
        help="Console COM port used by burn; defaults to --port.",
    )
    parser.add_argument(
        "--burn-baud",
        default=None,
        help="DownloadLib baud used by burn; defaults to AgentAuto.conf.",
    )
    parser.add_argument(
        "--shell-baud",
        default=None,
        help="Console shell baud used by burn; defaults to 115200.",
    )
    parser.add_argument(
        "--enter-mode",
        choices=("none", "shell", "hif"),
        default=None,
        help="Automatically enter download mode before burn when supported.",
    )
    parser.add_argument(
        "--enter-shell-command",
        default=None,
        help="Shell command used for automatic download entry.",
    )
    parser.add_argument(
        "--enter-hex",
        default=None,
        help="HIF hex frame used for automatic download entry.",
    )
    parser.add_argument(
        "--enter-verify",
        choices=("auto", "shell", "hif", "none"),
        default=None,
        help="Serial verification mode for automatic download entry.",
    )
    parser.add_argument(
        "--enter-wait-ms",
        type=int,
        default=None,
        help="Wait before burn after automatic download entry.",
    )
    parser.add_argument(
        "--enter-read-timeout-ms",
        type=int,
        default=None,
        help="Serial read timeout for automatic download entry.",
    )
    parser.add_argument(
        "--monitor-port",
        default=None,
        help="Port used by the final monitor step; defaults to --port.",
    )
    parser.add_argument(
        "--monitor-baud",
        default=None,
        help="Baud used by the final monitor step; defaults to AgentAuto.conf.",
    )
    parser.add_argument(
        "--monitor-duration",
        type=float,
        default=None,
        help="Monitor duration in seconds; defaults to AgentAuto.conf.",
    )
    parser.add_argument("--monitor-out", default=None, help="Optional monitor log file.")
    parser.add_argument(
        "--config",
        default=None,
        help="CDK directory containing .cdkproj, or CDK build configuration name.",
    )
    parser.add_argument(
        "--board",
        default="",
        help="CONFIG_BOARD_* macro written to project.cdkproj, for example CONFIG_BOARD_MRS6240_P2512_CPUF.",
    )
    parser.add_argument(
        "--auto-reset",
        dest="auto_reset",
        action="store_true",
        default=True,
        help="Reset/start the board after a successful burn.",
    )
    parser.add_argument(
        "--no-auto-reset",
        dest="auto_reset",
        action="store_false",
        help="Do not reset/start the board after burn.",
    )
    parser.add_argument(
        "--read-back-check",
        dest="read_back_check",
        action="store_true",
        default=True,
        help="Enable the DownloadLib read-back verification after flashing.",
    )
    parser.add_argument(
        "--no-read-back-check",
        dest="read_back_check",
        action="store_false",
        help="Disable the DownloadLib read-back verification after flashing.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Validate each step without opening hardware or writing project files.",
    )
    parser.set_defaults(handler=run_workflow)
