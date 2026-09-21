#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AgentAuto hif module: send, enter, and parse HIF frames."""

from __future__ import annotations

import argparse


COMMAND = "hif"


def add_parser(sub, engine) -> None:
    parser = sub.add_parser(
        COMMAND,
        help="Send HIF bytes, enter upgrade mode over HIF, or parse captures.",
    )
    hif_sub = parser.add_subparsers(dest="hif_command", required=True)

    send = hif_sub.add_parser(
        "send",
        help="Send shell text or raw HIF bytes on the HIF UART.",
    )
    send.add_argument("--port", required=True)
    send.add_argument("--baud", default=engine.DEFAULT_BURN_BAUD)
    send.add_argument("--text")
    send.add_argument("--hex")
    send.add_argument(
        "--verify",
        choices=("none", "auto", "shell", "hif", "wake"),
        default="none",
    )
    send.add_argument(
        "--hif-wake",
        action="store_true",
        help="Send 55 FF 55 FF first, wait for 0x79 ACK, then send --hex.",
    )
    send.add_argument(
        "--line-ending",
        choices=sorted(engine.DEFAULT_LINE_ENDINGS),
        default="CRLF",
    )
    send.add_argument("--read-timeout-ms", type=int, default=1500)
    send.add_argument("--dry-run", action="store_true")
    send.set_defaults(
        handler=lambda args: run_hif_send(args, engine)
    )

    enter = hif_sub.add_parser(
        "enter",
        help="Wake the HIF UART, then send the reboot/upgrade frame.",
    )
    enter.add_argument("--port", required=True)
    enter.add_argument("--baud", default=engine.DEFAULT_BURN_BAUD)
    enter.add_argument("--hex", default=engine.HIF_UPGRADE_ENTER_HEX)
    enter.add_argument(
        "--verify",
        choices=("none", "auto", "hif"),
        default="auto",
    )
    enter.add_argument("--read-timeout-ms", type=int, default=4000)
    enter.add_argument("--dry-run", action="store_true")
    enter.set_defaults(
        handler=lambda args: run_hif_enter(args, engine)
    )

    parse = hif_sub.add_parser(
        "parse",
        help="Decode a HIF hex log, RX_HEX log, or raw binary capture.",
    )
    parse.add_argument("input", help="Monitor hex log, RX_HEX log, or binary capture.")
    parse.add_argument(
        "--msg-id",
        default="auto",
        help="Decode only this message id, for example 0xC2 or 194.",
    )
    parse.add_argument("--raw", action="store_true")
    parse.add_argument("--bins", action="store_true")
    parse.add_argument("--json", action="store_true")
    parse.set_defaults(handler=engine.cmd_parse_hif)


def run_hif_send(args: argparse.Namespace, engine) -> int:
    if bool(args.text) == bool(args.hex):
        return engine.fail("Provide exactly one of --text or --hex.", 1)
    try:
        baud = int(args.baud)
    except ValueError:
        return engine.fail("--baud must be numeric.", 1)
    if args.hif_wake and not args.hex:
        return engine.fail("--hif-wake requires --hex.", 1)

    if args.hif_wake:
        wake_result = engine.send_serial_command(
            args.port,
            baud,
            "hif",
            "upgrade",
            engine.HIF_WAKE_MAGIC_HEX,
            "NONE",
            args.read_timeout_ms,
            dry_run=args.dry_run,
            verify_mode="wake",
        )
        if wake_result != 0:
            return wake_result

    return engine.send_serial_command(
        args.port,
        baud,
        "shell" if args.text else "hif",
        args.text or "upgrade",
        args.hex or "",
        args.line_ending,
        args.read_timeout_ms,
        dry_run=args.dry_run,
        verify_mode=args.verify,
    )


def run_hif_enter(args: argparse.Namespace, engine) -> int:
    try:
        baud = int(args.baud)
    except ValueError:
        return engine.fail("--baud must be numeric.", 1)
    print(f"HIF_ENTER_PORT={args.port}")
    print(f"HIF_ENTER_BAUD={baud}")
    print(f"HIF_ENTER_HEX={args.hex}")
    result = engine.send_hif_enter_handshake(
        args.port,
        baud,
        args.hex,
        args.read_timeout_ms,
        dry_run=args.dry_run,
        verify_mode=args.verify,
    )
    if result == 0:
        print("HIF_ENTER_OK=1")
    return result
