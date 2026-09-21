#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AgentAuto upgrade module: enter download mode from shell or HIF."""

from __future__ import annotations

import argparse


COMMAND = "upgrade"


def add_parser(sub, engine) -> None:
    parser = sub.add_parser(
        COMMAND,
        help="Enter download/upgrade mode using shell upgrade or HIF reboot.",
    )
    parser.add_argument(
        "--mode",
        choices=("shell", "hif"),
        default="shell",
        help="Use the shell command or the HIF reboot/upgrade frame.",
    )
    parser.add_argument("--port", required=True, help="Console or HIF COM port.")
    parser.add_argument(
        "--baud",
        default=None,
        help=(
            "Serial baud. Defaults to 115200 for shell and 921600 for HIF "
            "when this option is omitted."
        ),
    )
    parser.add_argument("--shell-command", default="upgrade")
    parser.add_argument("--enter-hex", default=engine.HIF_UPGRADE_ENTER_HEX)
    parser.add_argument(
        "--line-ending",
        choices=sorted(engine.DEFAULT_LINE_ENDINGS),
        default="CRLF",
    )
    parser.add_argument(
        "--verify",
        choices=("none", "auto", "shell", "hif"),
        default="auto",
        help="Require serial evidence after sending the upgrade request.",
    )
    parser.add_argument("--read-timeout-ms", type=int, default=4000)
    parser.add_argument("--dry-run", action="store_true")
    parser.set_defaults(
        handler=lambda args: run_upgrade(args, engine)
    )


def run_upgrade(args: argparse.Namespace, engine) -> int:
    baud_text = args.baud
    if not baud_text:
        baud_text = (
            engine.DEFAULT_BURN_BAUD
            if args.mode == "hif"
            else engine.DEFAULT_SHELL_BAUD
        )
    try:
        baud = int(baud_text)
    except ValueError:
        return engine.fail("--baud must be numeric.", 1)

    print(f"UPGRADE_MODE={args.mode}")
    print(f"UPGRADE_PORT={args.port}")
    print(f"UPGRADE_BAUD={baud}")
    print(f"UPGRADE_VERIFY={args.verify}")

    if args.mode == "hif":
        result = engine.send_hif_enter_handshake(
            args.port,
            baud,
            args.enter_hex,
            args.read_timeout_ms,
            dry_run=args.dry_run,
            verify_mode=args.verify,
        )
    else:
        result = engine.send_serial_command(
            args.port,
            baud,
            "shell",
            args.shell_command,
            args.enter_hex,
            args.line_ending,
            args.read_timeout_ms,
            dry_run=args.dry_run,
            verify_mode=args.verify,
        )
    if result == 0:
        print("UPGRADE_OK=1")
    return result
