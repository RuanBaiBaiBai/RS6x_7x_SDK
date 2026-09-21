#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AgentAuto unified modular dispatcher.

Each feature lives under ``modules/<name>/``. A folder contains:

- ``module.py``: optional command implementation. It should expose
  ``COMMAND`` and ``add_parser(sub, engine)``.

The ``core`` folder is reserved for this dispatcher and is not a module.
"""

from __future__ import annotations

import argparse
import configparser
import importlib.util
import os
import sys
from pathlib import Path


PROG = "AgentAuto"
SCRIPT_PATH = Path(__file__).resolve()
if (SCRIPT_PATH.parent / "modules").is_dir():
    RELEASE_ROOT = SCRIPT_PATH.parent
else:
    RELEASE_ROOT = SCRIPT_PATH.parents[2]
MODULES_ROOT = RELEASE_ROOT / "modules"
ENGINE_CANDIDATES = [
    RELEASE_ROOT / "runtime" / "agentauto_engine.py",
    RELEASE_ROOT / "agentauto_engine.py",
]

RESERVED_MODULES = {
    "datarecord": "Record raw UART/HIF reports to files.",
    "dataparse": "Parse previously recorded UART/HIF reports.",
}

CONFIG_FILE_NAME = "AgentAuto.conf"
GLOBAL_OPTION_KEYS = {"sdk", "cdk", "dll"}
GLOBAL_VALUE_OPTIONS = {"--sdk", "--cdk", "--dll"}

BOOLEAN_OPTIONS = {
    "auto-reset": ("--auto-reset", "--no-auto-reset"),
    "kill-before-start": ("--kill-before-start", "--no-kill-before-start"),
    "read-back-check": ("--read-back-check", "--no-read-back-check"),
    "download-finish-start": (
        "--download-finish-start",
        "--no-download-finish-start",
    ),
    "hex": ("--hex", None),
    "raw": ("--raw", None),
    "bins": ("--bins", None),
    "json": ("--json", None),
    "no-timestamp": ("--no-timestamp", None),
    "dry-run": ("--dry-run", None),
    "skip-prepare": ("--skip-prepare", None),
    "skip-shell": ("--skip-shell", None),
    "skip-cdkproj": ("--skip-cdkproj", None),
    "skip-modules": ("--skip-modules", None),
    "hif-wake": ("--hif-wake", None),
    "verbose": ("--verbose", None),
    "add-path": ("--add-path", None),
}

COMMAND_OPTIONS = {
    "upgrade": {
        "mode",
        "port",
        "baud",
        "shell-command",
        "enter-hex",
        "line-ending",
        "verify",
        "read-timeout-ms",
        "dry-run",
    },
    "burn": {
        "project",
        "config",
        "image",
        "port",
        "console-port",
        "baud",
        "shell-baud",
        "retry",
        "wait-ack-timeout",
        "read-back-check",
        "download-finish-start",
        "auto-reset",
        "kill-before-start",
        "copy-dir",
        "enter-mode",
        "enter-verify",
        "enter-shell-command",
        "enter-line-ending",
        "enter-hex",
        "enter-wait-ms",
        "enter-read-timeout-ms",
        "post-reset-mode",
        "post-reset-shell-command",
        "post-reset-line-ending",
        "post-reset-hex",
        "post-reset-wait-ms",
        "dry-run",
    },
    "monitor": {
        "port",
        "baud",
        "duration",
        "idle-timeout",
        "until-text",
        "out",
        "no-timestamp",
        "hex",
        "max-bytes",
        "dry-run",
    },
    "hif": {"baud", "read-timeout-ms", "verify", "line-ending"},
    "hif-send": {
        "port",
        "baud",
        "text",
        "hex",
        "verify",
        "hif-wake",
        "line-ending",
        "read-timeout-ms",
        "dry-run",
    },
    "hif-enter": {
        "port",
        "baud",
        "hex",
        "verify",
        "read-timeout-ms",
        "dry-run",
    },
    "hif-parse": {"input", "msg-id", "raw", "bins", "json"},
    "send": {
        "port",
        "baud",
        "text",
        "hex",
        "verify",
        "hif-wake",
        "line-ending",
        "read-timeout-ms",
        "dry-run",
    },
    "brom-send": {
        "port",
        "baud",
        "cmd",
        "mode",
        "addr",
        "len",
        "data",
        "read-timeout-ms",
        "expected-ack",
        "verbose",
        "dry-run",
    },
    "parse-hif": {"input", "msg-id", "raw", "bins", "json"},
    "prepare": {"project", "config", "board", "skip-shell", "skip-cdkproj", "skip-modules"},
    "refresh-sources": {"project", "config", "skip-modules"},
    "exclude": {"project", "config", "source"},
    "unexclude": {"project", "config", "source"},
    "excluded": {"project", "config"},
    "build": {
        "project",
        "config",
        "board",
        "skip-prepare",
        "skip-shell",
        "skip-cdkproj",
        "skip-modules",
        "exclude",
        "exclude-glob",
        "exclude-on-error",
        "dry-run",
    },
    "discover": {"json"},
    "ports": {"json"},
    "install": {"prefix", "add-path"},
    "workflow": {
        "project",
        "port",
        "console-port",
        "burn-baud",
        "shell-baud",
        "monitor-port",
        "monitor-baud",
        "monitor-duration",
        "monitor-out",
        "config",
        "auto-reset",
        "read-back-check",
        "board",
    },
}


def config_file_path() -> Path:
    override = os.environ.get("AGENT_AUTO_CONFIG")
    if override:
        return Path(override).expanduser().resolve()
    return RELEASE_ROOT / CONFIG_FILE_NAME


def load_config() -> configparser.ConfigParser:
    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    path = config_file_path()
    if path.is_file():
        with path.open("r", encoding="utf-8-sig") as config_file:
            config.read_file(config_file)
    else:
        config.add_section("default")
    return config


def first_command_info(argv):
    skip_value = False
    for index, token in enumerate(argv):
        if skip_value:
            skip_value = False
            continue
        if token.startswith("-"):
            if token in GLOBAL_VALUE_OPTIONS or any(
                token.startswith(option + "=") for option in GLOBAL_VALUE_OPTIONS
            ):
                if "=" not in token:
                    skip_value = True
            continue
        return index, token
    return None, None


def command_key(argv):
    index, first = first_command_info(argv)
    if first is None:
        return None
    if first != "hif":
        return first
    for token in argv[index + 1 :]:
        if not token.startswith("-"):
            return "hif-" + token.lower()
    return "hif"


def option_present(argv, option):
    if any(token == option or token.startswith(option + "=") for token in argv):
        return True
    if option == "--shell-baud":
        return any(
            token == "--console-baud" or token.startswith("--console-baud=")
            for token in argv
        )
    return False


def config_sections(config, command):
    sections = ["default"]
    base = command.split("-", 1)[0]
    if config.has_section(base) and base != "default":
        sections.append(base)
    if command != base and config.has_section(command):
        sections.append(command)
    return sections


def config_boolean_value(value: str) -> bool:
    return str(value).strip().lower() in ("1", "true", "yes", "on")


def build_config_arguments(config, argv, command):
    allowed = COMMAND_OPTIONS.get(command, set())
    global_args = []
    command_args = []
    merged_values = {}
    for section in config_sections(config, command):
        merged_values.update(dict(config.items(section)))

    for option_key, raw_value in merged_values.items():
        value = str(raw_value).strip()
        if not value:
            continue
        if option_key in GLOBAL_OPTION_KEYS:
            option = "--" + option_key
            if not option_present(argv, option):
                global_args.extend([option, value])
            continue
        if option_key not in allowed:
            continue

        option = "--" + option_key
        is_boolean = option_key in BOOLEAN_OPTIONS
        if option_key == "hex" and command not in ("monitor",):
            is_boolean = False

        if is_boolean:
            true_option, false_option = BOOLEAN_OPTIONS[option_key]
            if config_boolean_value(value):
                if not option_present(argv, true_option):
                    command_args.append(true_option)
            elif false_option and not option_present(argv, false_option):
                command_args.append(false_option)
        elif not option_present(argv, option):
            command_args.extend([option, value])

    return global_args, command_args


def insert_config_arguments(argv, global_args, command_args, command):
    index, _ = first_command_info(argv)
    if command_args:
        has_hif_subcommand = any(
            not token.startswith("-") for token in argv[index + 1 :]
        )
        if command == "hif" and not has_hif_subcommand:
            return global_args + argv
        insert_index = index + 1
        if command.startswith("hif-"):
            for nested_index in range(index + 1, len(argv)):
                if not argv[nested_index].startswith("-"):
                    insert_index = nested_index + 1
                    break
        argv = argv[:insert_index] + command_args + argv[insert_index:]
    return global_args + argv


def apply_config_defaults(argv):
    argv = list(argv)
    command = command_key(argv)
    if command is None:
        return argv
    config = load_config()
    global_args, command_args = build_config_arguments(
        config, argv, command
    )
    return insert_config_arguments(argv, global_args, command_args, command)


def load_engine():
    for candidate in ENGINE_CANDIDATES:
        if not candidate.is_file():
            continue
        spec = importlib.util.spec_from_file_location(
            "agent_auto_engine", candidate
        )
        engine = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = engine
        assert spec.loader is not None
        spec.loader.exec_module(engine)
        return engine
    raise RuntimeError(
        "AgentAuto engine was not found. Expected one of: "
        + ", ".join(str(path) for path in ENGINE_CANDIDATES)
    )


def load_module_folder(module_dir: Path):
    module_file = module_dir / "module.py"
    if not module_file.is_file():
        return None
    spec = importlib.util.spec_from_file_location(
        "agent_auto_module_" + module_dir.name, module_file
    )
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def cmd_reserved(args: argparse.Namespace, module_name: str) -> int:
    print(f"{module_name.upper()}_MODULE=reserved")
    print(
        "This module folder is reserved for a future implementation. "
        "No release content is bundled yet."
    )
    return 0


def add_reserved_parser(sub, module_name: str, description: str) -> None:
    parser = sub.add_parser(
        module_name,
        help=f"{description} (reserved)",
    )
    parser.add_argument("--dry-run", action="store_true")
    parser.set_defaults(
        handler=lambda args: cmd_reserved(args, module_name)
    )


def add_legacy_parsers(sub, engine) -> None:
    discover = sub.add_parser("discover", help="Show discovered environment.")
    discover.add_argument("--json", action="store_true")
    discover.set_defaults(handler=engine.cmd_discover)

    ports = sub.add_parser("ports", help="List available COM ports.")
    ports.add_argument("--json", action="store_true")
    ports.set_defaults(handler=engine.cmd_ports)
    sub.add_parser("doctor", help="Check the environment.").set_defaults(
        handler=engine.cmd_doctor
    )

    engine.add_prepare_parser(sub)
    engine.add_refresh_sources_parser(sub)
    engine.add_exclude_parser(sub)
    engine.add_unexclude_parser(sub)
    engine.add_excluded_parser(sub)
    engine.add_build_parser(sub)
    engine.add_send_parser(sub)
    engine.add_brom_send_parser(sub)
    engine.add_install_parser(sub)
    engine.add_parse_hif_parser(sub)


def add_module_parsers(sub, engine) -> None:
    if not MODULES_ROOT.is_dir():
        return
    for module_dir in sorted(MODULES_ROOT.iterdir()):
        if (
            not module_dir.is_dir()
            or module_dir.name.startswith("_")
            or module_dir.name == "core"
        ):
            continue
        module = load_module_folder(module_dir)
        add_parser = getattr(module, "add_parser", None) if module else None
        command = (
            getattr(module, "COMMAND", module_dir.name)
            if module
            else module_dir.name
        )
        if callable(add_parser):
            add_parser(sub, engine)
        elif command in RESERVED_MODULES:
            add_reserved_parser(sub, command, RESERVED_MODULES[command])


def build_parser(engine) -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog=PROG,
        description=(
            "Compile, upgrade, burn, reset, monitor, and decode HIF data "
            "for PSIC/C-Sky SDK boards through one modular command."
        ),
    )
    parser.add_argument("--sdk", dest="sdk_root", default=None, help="SDK root directory.")
    parser.add_argument("--cdk", dest="cdk_make", default=None, help="cdk-make.exe path.")
    parser.add_argument("--dll", dest="dll_path", default=None, help="DownloadLib.dll path.")
    sub = parser.add_subparsers(dest="command", required=True)

    add_module_parsers(sub, engine)
    add_legacy_parsers(sub, engine)
    return parser


def main(argv=None) -> int:
    engine = load_engine()
    engine.configure_console()
    if argv is None:
        argv = sys.argv[1:]
    argv = apply_config_defaults(argv)
    parser = build_parser(engine)
    args = parser.parse_args(argv)

    if args.command in ("discover", "ports"):
        args.json = getattr(args, "json", False)

    try:
        return int(args.handler(args))
    except engine.AutoburnError as exc:
        return engine.fail(str(exc), 1)


if __name__ == "__main__":
    raise SystemExit(main())
