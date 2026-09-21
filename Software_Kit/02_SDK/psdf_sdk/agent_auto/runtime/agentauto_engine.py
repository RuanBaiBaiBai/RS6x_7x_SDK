#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
agentauto_engine.py

Single-file runtime engine for AgentAuto on PSIC-style SDK development boards.

The file combines the responsibilities that used to live in three separate
tool sets:

  - build: invoke the C-Sky CDK command line for a project
  - burn:  call DownloadLib.dll directly by default; optional local staging with --copy-dir
  - monitor: capture UART output through a temporary PowerShell adapter
  - reset: enter download mode and send the official BROM reset command
  - send:  write a shell text command or raw HIF bytes to a COM port
  - discover/doctor/install: make the tool portable to a new SDK environment

Path discovery is explicit and environment-driven.  Do not hardcode SDK paths
or COM ports when using this skill.
"""

from __future__ import annotations

import argparse
import base64
import fnmatch
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from xml.etree import ElementTree


PROG = "AgentAuto"
SCRIPT_DIR = Path(__file__).resolve().parent

DEFAULT_BURN_BAUD = "921600"
DEFAULT_SHELL_BAUD = "115200"
DEFAULT_LINE_ENDINGS = {
    "CRLF": b"\r\n",
    "LF": b"\n",
    "CR": b"\r",
    "NONE": b"",
}

HIF_HEAD_MAGIC = 0xA5
HIF_MSG_TYPE_TO_DEVICE = 0x01
HIF_MSG_TYPE_TO_HOST = 0x02
HIF_MSG_FLAG_REQ_BIT = 0x01
HIF_MSG_FLAG_CHECK_BIT = 0x04
HIF_MSG_FLAG_MORE_BIT = 0x08
HIF_MSG_FLAG_EXTEND_BIT = 0x10
HIF_MSG_FLAG_MAC32_BIT = 0x20
HIF_MSG_ID_REBOOT = 0x04
HIF_REBOOT_MODE_UPGRADE = 0x02
HIF_MSG_ID_CUBE_DATA = 0xC1
HIF_MSG_ID_FFT_DATA = 0xC2
HIF_DATACUBE_MSG_IDS = (HIF_MSG_ID_CUBE_DATA, HIF_MSG_ID_FFT_DATA)
FRAME_UPLOAD_LEN = 12
TL_LEN = 12
REPORT_HEAD_LEN = FRAME_UPLOAD_LEN + TL_LEN

# Backwards-compatible aliases used by the bundled live/demo scripts.
C2_FRAME_UPLOAD_LEN = FRAME_UPLOAD_LEN
C2_TL_LEN = TL_LEN
C2_REPORT_HEAD_LEN = REPORT_HEAD_LEN
HIF_WAKE_MAGIC_HEX = "55 FF 55 FF"
HIF_WAKE_ACK_MAGIC = 0x79
HIF_WAKE_ACK_HEX = "79 79 79 79"
HIF_UPGRADE_ENTER_HEX = "A5 40 15 04 01 00 02 E8 FB FE FF"
HIF_UPGRADE_ACK_HEX = "A5 43 12 04 01 00 00 ED FB FE FF"

CDK_PROJECT_NAME = "project.cdkproj"
CDK_EXCLUDE_NAME = "autoburn_exclude.txt"
PROJECT_SOURCE_SUFFIXES = {".c", ".h", ".S", ".s", ".ld", ".yml"}
PROJECT_SKIP_DIRS = {
    ".cache",
    ".cdk",
    ".git",
    "Lst",
    "Obj",
    "__pycache__",
    "cdk",
    "output",
}
# CONFIG_SHELL and CONFIG_SYS_SHELL are interlocked: prepare adds or keeps
# both together and never enables only one.
SHELL_MACROS = {
    "CONFIG_SHELL": "1",
    "CONFIG_SYS_SHELL": "1",
    "CONFIG_SHELL_CMD_SYS": "1",
    "CONFIG_SHELL_UART_NUM": "0",
    "CONFIG_SHELL_UART_BAUDRATE": "115200",
}

MODULE_MACROS = {
    "hif": {
        "CONFIG_HIF": "1",
        "CONFIG_HIF_APP_DATA_POOL": "1",
        "CONFIG_HIF_PHY_TYPE": "1",
        "CONFIG_HIF_PHY_UART_DEF_NUM": "1",
        "CONFIG_HIF_PHY_UART_DEF_RATE": "921600",
        "CONFIG_DRIVER_UART_0": "1",
    },
    "mmw": {
        "CONFIG_HIF": "1",
        "CONFIG_HIF_APP_DATA_POOL": "1",
        "CONFIG_HIF_PHY_TYPE": "1",
        "CONFIG_HIF_PHY_UART_DEF_NUM": "1",
        "CONFIG_HIF_PHY_UART_DEF_RATE": "921600",
        "CONFIG_DRIVER_UART_0": "1",
        "CONFIG_MMW_CALIB_DATA_LOAD": "1",
        "CONFIG_MMW_CTRL": "1",
        "CONFIG_MMW_DRIVER": "1",
        "CONFIG_MMW_PRESENCE_POINT_CLOUD": "1",
        "CONFIG_MMW_SHELL": "1",
    },
}

MODULE_SOURCE_DIRS = {
    "hif": [
        "subsys/hif/inc",
        "subsys/hif/src",
    ],
    "mmw": [
        "subsys/mmw",
        "subsys/mmw/mmw_algorithm",
        "subsys/mmw/mmw_application",
        "subsys/mmw/mmw_cmd/inc",
        "subsys/mmw/mmw_cmd/src",
        "subsys/mmw/mmw_ctrl/inc",
        "subsys/mmw/mmw_ctrl/src",
        "subsys/mmw/mmw_mdsp",
    ],
}

MODULE_INCLUDE_DIRS = {
    "hif": [
        "subsys/hif/inc",
    ],
    "mmw": [
        "subsys/hif/inc",
        "subsys/mmw",
        "subsys/mmw/mmw_algorithm",
        "subsys/mmw/mmw_application",
        "subsys/mmw/mmw_cmd/inc",
        "subsys/mmw/mmw_ctrl/inc",
        "subsys/mmw/mmw_mdsp",
        "subsys/mmw/mmw_psic_lib",
    ],
}

DOWNLOAD_SUCCESS_CODE = 0x59
DOWNLOAD_FAILURE_HINTS = {
    1: "Firmware checksum is invalid.",
    2: "DownloadLib rejected a command sent to the board.",
    3: "The image contains an illegal flash address.",
    4: "The board returned an unknown command.",
    5: "Flash erase failed; confirm the board is in update/burn mode.",
    6: "Flash write failed; check the image and flash protection.",
    7: "Flash read failed during the operation.",
    8: "Firmware data length is invalid.",
    9: "The firmware image could not be opened.",
    10: "The serial port could not be opened.",
    11: "Baud-rate synchronization timed out.",
    12: "Timed out waiting for ACK.",
    13: "Firmware image size is not aligned for flashing.",
    14: "Read-back verification failed.",
    15: "The serial port reported an abnormal state.",
    16: "The flash erase address is not 4K aligned.",
    28: "Flash erase ACK timeout.",
    44: "Download ACK timeout.",
    60: "Read-back ACK timeout.",
    76: "Reset/start Set-PC ACK timeout.",
}


class AutoburnError(Exception):
    """Raised for expected CLI/environment failures."""


def _env_alias(*names: str) -> str | None:
    for name in names:
        value = os.environ.get(name)
        if value:
            return value
    return None


def configure_console() -> None:
    """Use UTF-8 console output when the running Python supports it."""
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass


def fail(message: str, code: int = 1) -> int:
    print(f"ERROR: {message}", file=sys.stderr, flush=True)
    return code


def is_network_path(path: Path) -> bool:
    return str(path).startswith("\\\\")


def is_sdk_root(path: Path, explicit: bool = False) -> bool:
    if not path.is_dir():
        return False
    project_dir = path / "project"
    platform_dir = path / "platform"
    marker = path / "tools" / "psdf.config"
    if explicit:
        return project_dir.is_dir() and platform_dir.is_dir()
    return marker.is_file() and project_dir.is_dir() and platform_dir.is_dir()


def find_sdk_root(explicit: str | None = None, required: bool = True) -> Path | None:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit).expanduser().resolve())
    env_root = os.environ.get("AGENT_AUTO_SDK_ROOT")
    if env_root:
        candidates.append(Path(env_root).expanduser().resolve())

    for start in (Path.cwd(), SCRIPT_DIR):
        current = start.resolve()
        while True:
            candidates.append(current)
            if current.parent == current:
                break
            current = current.parent

    seen: set[str] = set()
    for candidate in candidates:
        key = str(candidate).lower()
        if key in seen:
            continue
        seen.add(key)
        if is_sdk_root(candidate, explicit=bool(explicit and candidate == Path(explicit).expanduser().resolve())):
            return candidate

    if required:
        raise AutoburnError(
            "SDK root not found. Run from inside the SDK, export "
            "AGENT_AUTO_SDK_ROOT, or pass --sdk <sdk-root>. An SDK root should "
            "contain project/ and platform/ and usually tools/psdf.config."
        )
    return None


def find_project_dirs(sdk_root: Path) -> list[Path]:
    root = sdk_root / "project"
    if not root.is_dir():
        return []
    projects: list[Path] = []
    for child in sorted(root.iterdir()):
        if not child.is_dir() or child.name.startswith("."):
            continue
        cdk_dir = child / "cdk"
        if cdk_dir.is_dir() and (
            list(cdk_dir.glob("*.cdkproj")) or list(cdk_dir.glob("*.cdkws"))
        ):
            projects.append(child)
    return projects


def find_project(sdk_root: Path, project_name: str | None) -> Path:
    if project_name:
        rel = project_name.replace("\\", "/")
        direct = Path(project_name).expanduser()
        if direct.is_absolute():
            resolved = direct.resolve()
            if resolved.is_dir():
                return resolved
            raise AutoburnError(f"Project folder not found: {resolved}")
        if rel.startswith("project/"):
            rel = rel[len("project/"):]
        rel = rel.strip("./")
        if rel:
            candidate = sdk_root / "project" / rel
            if candidate.is_dir():
                return candidate
        for project_dir in find_project_dirs(sdk_root):
            if project_dir.name.lower() == project_name.lower():
                return project_dir
        names = ", ".join(p.name for p in find_project_dirs(sdk_root)) or "none"
        raise AutoburnError(f"Project '{project_name}' not found. Available: {names}")

    for current in (Path.cwd().resolve(), *[p for p in Path.cwd().resolve().parents]):
        parts = current.parts
        if len(parts) >= 2 and parts[-2].lower() == "project":
            name = parts[-1]
            candidate = sdk_root / "project" / name
            if candidate.is_dir():
                return candidate
    projects = find_project_dirs(sdk_root)
    if len(projects) == 1:
        return projects[0]
    names = ", ".join(p.name for p in projects) or "none"
    raise AutoburnError(
        "Cannot infer a project. Pass --project <name>. Available: " + names
    )


def _as_posix(value: str) -> str:
    return value.replace("\\", "/")


def _normalize_cdk_path(value: str) -> str:
    posix = _as_posix(value.strip().strip('"'))
    if posix.startswith("/"):
        posix = posix.lstrip("/")
    while posix.startswith("./"):
        posix = posix[2:]
    return posix


def resolve_source_path(cdk_dir: Path, raw: str) -> Path:
    raw = raw.strip().strip('"')
    if not raw:
        return cdk_dir
    path = Path(raw)
    if path.is_absolute():
        return path
    parts = [part for part in _as_posix(raw).split("/") if part not in ("", ".")]
    return cdk_dir.joinpath(*parts)


def _cdk_relative_name(path: Path, cdk_dir: Path) -> str:
    try:
        return path.resolve().relative_to(cdk_dir.resolve()).as_posix()
    except ValueError:
        return _as_posix(
            os.path.relpath(str(path.resolve()), str(cdk_dir.resolve()))
        )


def _matches_any_glob(name: str, globs: list[str]) -> bool:
    return any(fnmatch.fnmatchcase(name, pattern) for pattern in globs)


def find_cdk_project_file(cdk_dir: Path) -> Path:
    project_file = cdk_dir / CDK_PROJECT_NAME
    if project_file.is_file():
        return project_file
    projects = sorted(cdk_dir.glob("*.cdkproj"))
    if len(projects) == 1:
        return projects[0]
    if not projects:
        raise AutoburnError(f"No .cdkproj file found under {cdk_dir}.")
    raise AutoburnError(
        f"Multiple .cdkproj files found under {cdk_dir}; rename the active one to "
        f"{CDK_PROJECT_NAME} or remove the stale files."
    )


def _project_cdk_dirs(project_dir: Path) -> list[Path]:
    return [
        path
        for path in sorted(project_dir.glob("cdk*"))
        if path.is_dir() and not path.name.lower().endswith("output")
    ]


def resolve_project_cdk_dir(
    project_dir: Path,
    config: str | None = None,
) -> Path:
    if config:
        config_text = str(config).strip()
        config_path = Path(config_text).expanduser()
        if config_path.is_file() and config_path.suffix.lower() == ".cdkproj":
            config_path = config_path.parent
        if config_path.is_dir():
            project_file = config_path / CDK_PROJECT_NAME
            if project_file.is_file() or list(config_path.glob("*.cdkproj")):
                return config_path.resolve()
        config_candidate = project_dir / config_text.replace("\\", "/").split("/")[-1]
        if config_candidate.is_dir():
            project_file = config_candidate / CDK_PROJECT_NAME
            if project_file.is_file() or list(config_candidate.glob("*.cdkproj")):
                return config_candidate.resolve()

    legacy_dir = project_dir / "cdk"
    if legacy_dir.is_dir():
        return legacy_dir

    cdk_dirs = _project_cdk_dirs(project_dir)
    if len(cdk_dirs) == 1:
        return cdk_dirs[0]
    if len(cdk_dirs) > 1:
        names = ", ".join(path.name for path in cdk_dirs)
        raise AutoburnError(
            f"Project has multiple CDK directories ({names}). "
            "Pass --config <absolute path to the CDK directory containing "
            "project.cdkproj>, or set config in AgentAuto.conf."
        )
    raise AutoburnError(
        f"No cdk project directory found under {project_dir}. "
        "Expected project/cdk or a directory such as "
        "cdk_6130_1812_cpus / cdk_6240_cpuf."
    )


def _write_project_cdkws(cdk_dir: Path) -> Path:
    cdkproj = find_cdk_project_file(cdk_dir)
    try:
        tree = ElementTree.parse(cdkproj)
    except (ElementTree.ParseError, OSError) as exc:
        raise AutoburnError(f"Cannot parse {cdkproj} while generating .cdkws: {exc}")
    root = tree.getroot()
    project_name = root.attrib.get("Name") or cdk_dir.name
    configs = [
        node.attrib["Name"]
        for node in root.iter()
        if _element_local_name(node) == "BuildConfig"
        and node.attrib.get("Name")
    ]
    config = configs[0] if configs else "psdf"
    cdkws = cdk_dir / "project.cdkws"
    cdkws.write_text(
        "\r\n".join(
            [
                '<?xml version="1.0" encoding="UTF-8"?>',
                '<CDK_Workspace Name="project" Database="LanguageSever" DoubleClick="Yes">',
                "  <DefaultPackPath>$(CDKWS)\\__workspace_pack__</DefaultPackPath>",
                f'  <Project Name="{project_name}" Path="project.cdkproj" RootPath="" Active="Yes"/>',
                "  <BuildMatrix>",
                '    <WorkspaceConfiguration Name="Debug" Selected="yes">',
                "      <Environment/>",
                f'      <Project Name="{project_name}" ConfigName="{config}"/>',
                "    </WorkspaceConfiguration>",
                "  </BuildMatrix>",
                "</CDK_Workspace>",
                "",
            ]
        ),
        encoding="utf-8",
    )
    print(f"CDKWS_CREATED={cdkws}")
    return cdkws


def find_cdk_workspace(cdk_dir: Path) -> Path:
    cdkws = cdk_dir / "project.cdkws"
    if cdkws.is_file():
        return cdkws
    workspaces = sorted(cdk_dir.glob("*.cdkws"))
    if len(workspaces) == 1:
        return workspaces[0]
    if len(workspaces) > 1:
        raise AutoburnError(
            f"Multiple .cdkws files found under {cdk_dir}; "
            "rename the active one to project.cdkws."
        )
    return _write_project_cdkws(cdk_dir)


def find_project_header(project_dir: Path) -> Path | None:
    candidates = sorted(
        project_dir.rglob("prj_config.h"),
        key=lambda p: (len(p.parts), str(p).lower()),
    )
    for candidate in candidates:
        if "cdk" not in {part.lower() for part in candidate.parts}:
            return candidate
    return candidates[0] if candidates else None


def _set_config_macros(
    header: Path,
    macros: dict[str, str],
    comment: str,
    action_prefix: str,
    dry_run: bool,
    force_values: bool = False,
) -> list[str]:
    try:
        text = header.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        raise AutoburnError(f"Cannot read project header {header}: {exc}")

    newline = "\r\n" if "\r\n" in text else "\n"
    lines = text.splitlines()
    namespace = set(macros)
    present: set[str] = set()
    actions: list[str] = []
    pattern = r"^\s*#\s*define\s+(" + "|".join(
        re.escape(name) for name in sorted(namespace, key=len, reverse=True)
    ) + r")\b"
    regex = re.compile(pattern)

    for index, line in enumerate(lines):
        match = regex.match(line)
        if not match:
            continue
        name = match.group(1)
        if name not in namespace:
            continue
        present.add(name)
        if force_values:
            wanted = macros[name]
            replacement = f"#define {name} {wanted}"
            if line.strip() != replacement:
                actions.append(
                    f"{action_prefix}_UPDATE: {name}={wanted} ({header.name})"
                )
            lines[index] = replacement

    missing = sorted(namespace - present)
    if missing:
        insert_at = len(lines)
        for index in range(len(lines) - 1, -1, -1):
            if re.search(r"^\s*#\s*endif\b", lines[index]):
                insert_at = index
                break

        comment_idx = next(
            (
                index
                for index, line in enumerate(lines)
                if index < insert_at and line.strip() == comment
            ),
            None,
        )
        if comment_idx is not None:
            block = [f"#define {name:<36} {macros[name]}" for name in missing]
            insertion_index = comment_idx + 1
        else:
            block = ["", comment]
            for name in missing:
                block.append(f"#define {name:<36} {macros[name]}")
            block.append("")
            insertion_index = insert_at
        lines[insertion_index:insertion_index] = block
        actions.append(f"{action_prefix}_ADD: {', '.join(missing)} -> {header.name}")

    if actions and not dry_run:
        rendered = newline.join(lines)
        if text.endswith("\n"):
            rendered += "\n"
        backup = header.with_name(header.name + ".autoburn.bak")
        if not backup.exists():
            shutil.copy2(header, backup)
        header.write_text(rendered, encoding="utf-8", errors="replace")
    return actions


def _set_shell_macros(header: Path, dry_run: bool) -> list[str]:
    """Enable the shell macro pair together; they must stay interlocked."""
    return _set_config_macros(
        header,
        SHELL_MACROS,
        "/* AgentAuto shell auto-enable: make `upgrade` and HIF/shell automation available. */",
        "SHELL",
        dry_run,
        force_values=True,
    )


def _set_module_macros(header: Path, modules: set[str], dry_run: bool) -> list[str]:
    macros: dict[str, str] = {}
    for module in sorted(modules):
        macros.update(MODULE_MACROS.get(module, {}))
    if not macros:
        return []
    return _set_config_macros(
        header,
        macros,
        "/* AgentAuto mmw/hif auto-enable: compile MMW/HIF subsystem code. */",
        "MODULE",
        dry_run,
        force_values=False,
    )


def _detect_mmw_hif_modules(project_dir: Path) -> set[str]:
    patterns = {
        "hif": [
            re.compile(
                r"#\s*include\s*[<\"](?:hif|hif_config|hif_mem|hif_phy|hif_tl)\.h[>\"]",
                re.I,
            ),
            re.compile(r"\bCONFIG_HIF\b"),
        ],
        "mmw": [
            re.compile(
                r"#\s*include\s*[<\"](?:mmw_config|mmw_ctrl|mmw_hif|mmw)\.h[>\"]",
                re.I,
            ),
            re.compile(r"\bCONFIG_MMW_"),
            re.compile(r"\bmmw_[a-z][a-z0-9_]+\b"),
        ],
    }
    modules: set[str] = set()
    chunks: list[str] = []
    for path in _iter_project_source_files(project_dir):
        if path.suffix.lower() not in (".c", ".h"):
            continue
        try:
            chunks.append(path.read_text(encoding="utf-8", errors="replace"))
        except OSError:
            continue
    source_text = "\n".join(chunks)
    for module, module_patterns in patterns.items():
        if any(pattern.search(source_text) for pattern in module_patterns):
            modules.add(module)
    if "mmw" in modules:
        modules.add("hif")
    return modules


def _read_exclude_rules(cdk_dir: Path) -> tuple[set[str], list[str]]:
    paths: set[str] = set()
    globs: list[str] = []
    path = cdk_dir / CDK_EXCLUDE_NAME
    if not path.is_file():
        return paths, globs
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        raise AutoburnError(f"Cannot read {path}: {exc}")
    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith("# glob:"):
            glob_value = line[len("# glob:"):].strip()
            if glob_value:
                globs.append(_normalize_cdk_path(glob_value))
            continue
        if line.startswith("#"):
            continue
        norm = _normalize_cdk_path(line)
        if norm:
            paths.add(norm)
    return paths, sorted(globs)


def _write_exclude_rules(
    cdk_dir: Path,
    paths: set[str] | list[str],
    globs: list[str] | None = None,
    dry_run: bool = False,
) -> None:
    path = cdk_dir / CDK_EXCLUDE_NAME
    lines = [
        "# Autoburn temporary exclusion list.",
        "# Paths are relative to the cdk/ directory and use POSIX separators.",
        "# `AgentAuto refresh-sources --project NAME` reapplies these rules.",
        "",
    ]
    for pattern in sorted(set(globs or [])):
        lines.append(f"# glob: {_normalize_cdk_path(pattern)}")
    lines.extend(sorted({_normalize_cdk_path(p) for p in paths}))
    lines.append("")
    rendered = "\n".join(lines)
    if not dry_run:
        path.write_text(rendered, encoding="utf-8", errors="replace")


def _normalize_user_source_ref(
    sdk_root: Path,
    project_dir: Path,
    cdk_dir: Path,
    raw: str,
    glob: bool = False,
) -> str:
    raw = _as_posix(raw.strip().strip('"'))
    if not raw:
        return ""
    abs_path = Path(raw)
    if abs_path.is_absolute():
        try:
            return abs_path.resolve().relative_to(cdk_dir.resolve()).as_posix()
        except ValueError:
            return _normalize_cdk_path(raw)

    for base in (cdk_dir, project_dir, sdk_root):
        candidate = base.joinpath(*raw.split("/"))
        if candidate.exists():
            try:
                return candidate.resolve().relative_to(cdk_dir.resolve()).as_posix()
            except ValueError:
                break

    first = raw.split("/", 1)[0]
    if glob:
        if raw.startswith((".", "..", "/")):
            try:
                return (
                    cdk_dir.relative_to(cdk_dir.resolve()).as_posix() + "/" + raw
                ).lstrip("./")
            except ValueError:
                pass
        if first in ("subsys", "platform", "kernel", "libs"):
            try:
                return (
                    sdk_root.relative_to(cdk_dir.resolve()).as_posix() + "/" + raw
                ).lstrip("./")
            except ValueError:
                return _normalize_cdk_path(raw)
        if first in ("app", "boot", "cfg", "scripts") or first == project_dir.name:
            try:
                return (
                    project_dir.relative_to(cdk_dir.resolve()).as_posix() + "/" + raw
                ).lstrip("./")
            except ValueError:
                return _normalize_cdk_path(raw)
        return _normalize_cdk_path(raw)

    if first in ("subsys", "platform", "kernel", "libs"):
        try:
            return (sdk_root.relative_to(cdk_dir.resolve()).as_posix() + "/" + raw).lstrip("./")
        except ValueError:
            return _normalize_cdk_path(raw)
    if first in ("app", "boot", "cfg", "scripts") or first == project_dir.name:
        try:
            return (project_dir.relative_to(cdk_dir.resolve()).as_posix() + "/" + raw).lstrip("./")
        except ValueError:
            return _normalize_cdk_path(raw)
    return _normalize_cdk_path(raw)


def _iter_project_source_files(project_dir: Path):
    for root_dir, dirs, files in os.walk(str(project_dir)):
        dirs[:] = sorted(name for name in dirs if name not in PROJECT_SKIP_DIRS)
        root_path = Path(root_dir)
        for name in sorted(files):
            path = root_path / name
            if path.suffix.lower() in PROJECT_SOURCE_SUFFIXES:
                yield path


def _xml_escape_text(value: str) -> str:
    return (
        value.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
    )


def _xml_escape_attr(value: str) -> str:
    return _xml_escape_text(value).replace('"', "&quot;").replace("'", "&apos;")


def _element_to_lines(elem: ElementTree.Element, level: int = 0) -> list[str]:
    tag = elem.tag.rsplit("}", 1)[-1] if isinstance(elem.tag, str) else str(elem.tag)
    attributes = "".join(
        f' {name}="{_xml_escape_attr(value)}"'
        for name, value in elem.attrib.items()
    )
    indent = "  " * level
    children = list(elem)
    text = (elem.text or "").strip()
    if not children:
        if text:
            return [f"{indent}<{tag}{attributes}>{_xml_escape_text(text)}</{tag}>"]
        return [f"{indent}<{tag}{attributes}/>"]
    lines = [f"{indent}<{tag}{attributes}>"]
    if text:
        lines.append(f"{indent}  {_xml_escape_text(text)}")
    for child in children:
        lines.extend(_element_to_lines(child, level + 1))
    lines.append(f"{indent}</{tag}>")
    return lines


def _write_cdkproj(root: ElementTree.Element, cdkproj: Path, dry_run: bool) -> bool:
    lines = ['<?xml version="1.0" encoding="UTF-8"?>']
    lines.extend(_element_to_lines(root, 0))
    rendered = "\n".join(lines) + "\n"
    if dry_run:
        return False
    try:
        old = cdkproj.read_text(encoding="utf-8", errors="replace")
    except OSError:
        old = ""
    if old == rendered:
        return False
    backup = cdkproj.with_name(cdkproj.name + ".autoburn.bak")
    if not backup.exists():
        shutil.copy2(cdkproj, backup)
    cdkproj.write_text(rendered, encoding="utf-8", errors="replace")
    return True


def _element_local_name(elem: ElementTree.Element) -> str:
    tag = elem.tag
    if isinstance(tag, str):
        return tag.rsplit("}", 1)[-1]
    return str(tag)


def _local_child(
    parent: ElementTree.Element, name: str
) -> ElementTree.Element | None:
    for child in parent:
        if _element_local_name(child) == name:
            return child
    return None


def _read_board_config_macros(sdk_root: Path) -> list[str]:
    board_config = sdk_root / "platform" / "boards" / "board_config.h"
    if not board_config.is_file():
        return []
    try:
        text = board_config.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []
    pattern = re.compile(
        r"^\s*#\s*define\s+(CONFIG_BOARD_[A-Z0-9_]+)\b", re.I | re.MULTILINE
    )
    return sorted({match.group(1).upper() for match in pattern.finditer(text)})


def _resolve_board_macro(sdk_root: Path, board: str) -> str:
    raw = board.strip()
    if not raw:
        raise AutoburnError("--board cannot be empty.")
    requested = raw.split("=", 1)[0].strip().upper()
    if not requested.startswith("CONFIG_BOARD_"):
        requested = "CONFIG_BOARD_" + requested
    valid = _read_board_config_macros(sdk_root)
    if requested in valid:
        return requested
    if not valid:
        if re.fullmatch(r"CONFIG_BOARD_[A-Z0-9_]+", requested):
            return requested
        raise AutoburnError(
            "Cannot find platform/boards/board_config.h; pass the full "
            "CONFIG_BOARD_* macro name with --board."
        )

    target_norm = re.sub(r"[^A-Z0-9]", "", requested)
    matches = [
        macro
        for macro in valid
        if target_norm
        and target_norm in re.sub(r"[^A-Z0-9]", "", macro.upper())
    ]
    if len(matches) == 1:
        return matches[0]
    if len(matches) > 1:
        raise AutoburnError(
            f"--board '{raw}' is ambiguous. Candidates: {', '.join(matches)}"
        )
    raise AutoburnError(
        f"--board '{raw}' was not found in platform/boards/board_config.h. "
        f"Valid macros: {', '.join(valid) if valid else 'none'}"
    )


def _set_cdkproj_board_defines(
    cdkproj: Path, macro: str, dry_run: bool
) -> list[str]:
    try:
        tree = ElementTree.parse(cdkproj)
    except (ElementTree.ParseError, OSError) as exc:
        raise AutoburnError(f"Cannot parse {cdkproj}: {exc}")
    root = tree.getroot()
    selected = f"{macro}=1"
    actions: list[str] = []
    board_define_pattern = re.compile(
        r"^\s*CONFIG_BOARD_[A-Z0-9_]+(?:\s*=\s*(?:1|0)|)\s*$", re.I
    )

    for config_elem in root.iter("BuildConfig"):
        for tool_name in ("Compiler", "Asm"):
            tool = _local_child(config_elem, tool_name)
            if tool is None:
                tool = ElementTree.SubElement(config_elem, tool_name)
                tool.text = "\n  "
                actions.append(
                    f"BOARD_CDKPROJ_ADD: created {tool_name} in "
                    f"{cdkproj.name}"
                )

            defines = [
                child
                for child in tool
                if _element_local_name(child) == "Define"
            ]
            board_defines = [
                child
                for child in defines
                if child.text
                and board_define_pattern.match(child.text.strip())
            ]
            if board_defines:
                first = board_defines[0]
                if (first.text or "").strip() != selected:
                    first.text = selected
                    actions.append(
                        f"BOARD_CDKPROJ_UPDATE: {macro}=1 in BuildConfig/"
                        f"{tool_name} ({cdkproj.name})"
                    )
                for extra in board_defines[1:]:
                    tool.remove(extra)
                    actions.append(
                        f"BOARD_CDKPROJ_REMOVE: {extra.text.strip()} in "
                        f"BuildConfig/{tool_name} ({cdkproj.name})"
                    )
            else:
                empty_define = next(
                    (child for child in defines if not (child.text or "").strip()),
                    None,
                )
                if empty_define is not None:
                    empty_define.text = selected
                    actions.append(
                        f"BOARD_CDKPROJ_ADD: {macro}=1 in BuildConfig/"
                        f"{tool_name} ({cdkproj.name})"
                    )
                else:
                    new_define = ElementTree.SubElement(tool, "Define")
                    new_define.text = selected
                    actions.append(
                        f"BOARD_CDKPROJ_ADD: {macro}=1 in BuildConfig/"
                        f"{tool_name} ({cdkproj.name})"
                    )

    if actions and not dry_run:
        _write_cdkproj(root, cdkproj, dry_run)
    return actions


def _neutralize_header_board_macros(
    header: Path, board_macros: list[str], dry_run: bool
) -> list[str]:
    if not board_macros:
        return []
    try:
        text = header.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        raise AutoburnError(f"Cannot read project header {header}: {exc}")

    pattern = re.compile(
        r"^(\s*)#\s*define\s+(" + "|".join(
            re.escape(name) for name in sorted(board_macros, key=len, reverse=True)
        ) + r")\b"
    )
    newline = "\r\n" if "\r\n" in text else "\n"
    lines = text.splitlines()
    actions: list[str] = []
    for index, line in enumerate(lines):
        stripped = line.lstrip()
        if stripped.startswith("//") or stripped.startswith("/*"):
            continue
        match = pattern.match(line)
        if not match:
            continue
        indent = match.group(1)
        lines[index] = f"{indent}// {line.strip()}"
        actions.append(
            f"BOARD_PRJ_CONFIG_NEUTRALIZE: commented {match.group(2)} in "
            f"{header.name}"
        )

    if actions and not dry_run:
        rendered = newline.join(lines)
        if text.endswith("\n"):
            rendered += "\n"
        backup = header.with_name(header.name + ".autoburn.bak")
        if not backup.exists():
            shutil.copy2(header, backup)
        header.write_text(rendered, encoding="utf-8", errors="replace")
    return actions


def _ensure_virtual_dir_path(
    root: ElementTree.Element, path_parts: list[str]
) -> ElementTree.Element:
    parent: ElementTree.Element = root
    for part in path_parts:
        child = next(
            (
                candidate
                for candidate in parent
                if _element_local_name(candidate) == "VirtualDirectory"
                and candidate.attrib.get("Name") == part
            ),
            None,
        )
        if child is None:
            child = ElementTree.SubElement(parent, "VirtualDirectory")
            child.set("Name", part)
        parent = child
    return parent


def _virtual_dir_path_exists(
    root: ElementTree.Element, path_parts: list[str]
) -> bool:
    parent: ElementTree.Element = root
    for part in path_parts:
        child = next(
            (
                candidate
                for candidate in parent
                if _element_local_name(candidate) == "VirtualDirectory"
                and candidate.attrib.get("Name") == part
            ),
            None,
        )
        if child is None:
            return False
        parent = child
    return True


def _find_virtual_dir(
    parent: ElementTree.Element, path_parts: list[str]
) -> ElementTree.Element | None:
    current = parent
    for part in path_parts:
        child = next(
            (
                candidate
                for candidate in current
                if _element_local_name(candidate) == "VirtualDirectory"
                and candidate.attrib.get("Name") == part
            ),
            None,
        )
        if child is None:
            return None
        current = child
    return current


def _backup_virtual_dir_attr(
    cdkproj: Path, path_parts: list[str], attr_name: str
) -> str | None:
    backup = cdkproj.with_name(cdkproj.name + ".autoburn.bak")
    if not backup.is_file():
        return None
    try:
        backup_root = ElementTree.parse(backup).getroot()
    except (ElementTree.ParseError, OSError):
        return None
    node = _find_virtual_dir(backup_root, path_parts)
    if node is None:
        return None
    return node.attrib.get(attr_name)


def _excluded_virtual_dir_ids(root: ElementTree.Element) -> set[int]:
    excluded: set[int] = set()

    def visit(node: ElementTree.Element) -> None:
        if (
            _element_local_name(node) == "VirtualDirectory"
            and "ExcludeProjConfig" in node.attrib
        ):
            for child in node.iter():
                excluded.add(id(child))
            return
        for child in node:
            visit(child)

    visit(root)
    return excluded


def _module_source_dirs(sdk_root: Path, modules: set[str]) -> list[str]:
    rels: list[str] = []
    seen: set[str] = set()
    for module in sorted(modules):
        for rel in MODULE_SOURCE_DIRS.get(module, []):
            if rel in seen or not (sdk_root / rel).is_dir():
                continue
            seen.add(rel)
            rels.append(rel)
    return rels


def _module_include_dirs(sdk_root: Path, modules: set[str]) -> list[str]:
    rels: list[str] = []
    seen: set[str] = set()
    for module in sorted(modules):
        for rel in MODULE_INCLUDE_DIRS.get(module, []):
            if rel in seen or not (sdk_root / rel).is_dir():
                continue
            seen.add(rel)
            rels.append(rel)
    return rels


def _ensure_module_virtual_dirs(
    root: ElementTree.Element,
    sdk_root: Path,
    modules: set[str],
    cdkproj: Path,
) -> tuple[set[int], list[str], bool]:
    module_ids: set[int] = set()
    enabled: list[str] = []
    changed = False
    for rel in _module_source_dirs(sdk_root, modules):
        if not _virtual_dir_path_exists(root, rel.split("/")):
            changed = True
        node = _ensure_virtual_dir_path(root, rel.split("/"))
        module_ids.add(id(node))
        enabled.append(rel)

    subsys = next(
        (
            candidate
            for candidate in root
            if _element_local_name(candidate) == "VirtualDirectory"
            and candidate.attrib.get("Name") == "subsys"
        ),
        None,
    )
    if subsys is not None:
        for name in ("hif", "mmw"):
            module_node = next(
                (
                    candidate
                    for candidate in subsys
                    if _element_local_name(candidate) == "VirtualDirectory"
                    and candidate.attrib.get("Name") == name
                ),
                None,
            )
            if module_node is not None:
                if name in modules and "ExcludeProjConfig" in module_node.attrib:
                    module_node.attrib.pop("ExcludeProjConfig")
                    changed = True
                elif name not in modules:
                    backup_exclude = _backup_virtual_dir_attr(
                        cdkproj, ["subsys", name], "ExcludeProjConfig"
                    )
                    if (
                        backup_exclude is not None
                        and module_node.attrib.get("ExcludeProjConfig")
                        != backup_exclude
                    ):
                        module_node.attrib["ExcludeProjConfig"] = backup_exclude
                        changed = True
    return module_ids, enabled, changed


def _ensure_include_paths(
    root: ElementTree.Element,
    cdk_dir: Path,
    sdk_root: Path,
    modules: set[str],
) -> bool:
    rels = _module_include_dirs(sdk_root, modules)
    if not rels:
        return False
    changed = False
    for include_elem in root.iter():
        if _element_local_name(include_elem) != "IncludePath":
            continue
        parts: list[str] = []
        for raw in (include_elem.text or "").split(";"):
            item = raw.strip()
            if item:
                parts.append(item)
        for rel in rels:
            target = sdk_root / rel
            candidate = "$(ProjectPath)/" + _cdk_relative_name(target, cdk_dir)
            if candidate not in parts:
                parts.append(candidate)
                changed = True
        include_elem.text = ";".join(parts)
    return changed


def _pick_mmw_point_cloud_dir(
    sdk_root: Path, project_dir: Path
) -> Path | None:
    base = sdk_root / "libs" / "mmw" / "mmw_alg" / "point_cloud"
    if not base.is_dir():
        return None
    candidates: dict[str, Path] = {}
    for candidate in sorted(base.iterdir()):
        if candidate.is_dir() and (candidate / "libpoint_cloud.a").is_file():
            candidates[candidate.name.lower()] = candidate
    if not candidates:
        return None

    chunks: list[str] = []
    for path in _iter_project_source_files(project_dir):
        if path.suffix.lower() not in (".c", ".h"):
            continue
        try:
            chunks.append(path.read_text(encoding="utf-8", errors="replace"))
        except OSError:
            continue
    search_text = "\n".join(chunks).lower()
    for name in sorted(candidates, key=len, reverse=True):
        if name in search_text or name.lstrip("rs") in search_text:
            return candidates[name]
    mapping = {"6130": "rs6130", "6240": "rs6240", "7241": "rs7241"}
    for needle, name in mapping.items():
        if needle in search_text and name in candidates:
            return candidates[name]
    if len(candidates) == 1:
        return next(iter(candidates.values()))
    return None


def _ensure_mmw_linker(
    root: ElementTree.Element,
    cdk_dir: Path,
    sdk_root: Path,
    project_dir: Path,
) -> bool:
    if not _module_source_dirs(sdk_root, {"mmw"}):
        return False

    dsp_dir = sdk_root / "libs" / "dsp"
    point_dir = _pick_mmw_point_cloud_dir(sdk_root, project_dir)
    if not (dsp_dir / "libdsp.a").is_file():
        dsp_dir = None
    if dsp_dir is None and point_dir is None:
        return False

    sym_path: Path | None = None
    if point_dir is not None:
        sym_candidate = (
            sdk_root
            / "libs"
            / "mmw"
            / "mmw_ctrl"
            / point_dir.name
            / "mmw_ctrl.sym"
        )
        if sym_candidate.is_file():
            sym_path = sym_candidate

    changed = False
    for linker in root.iter():
        if _element_local_name(linker) != "Linker":
            continue

        lib_name = _local_child(linker, "LibName")
        if lib_name is not None:
            names = [item.strip() for item in (lib_name.text or "").split(";")]
            names = [item for item in names if item]
            for wanted in ("dsp", "point_cloud"):
                if wanted not in names:
                    names.append(wanted)
                    changed = True
            lib_name.text = ";".join(names)

        lib_path = _local_child(linker, "LibPath")
        if lib_path is not None:
            paths: list[str] = []
            seen_paths: set[str] = set()
            for raw in (lib_path.text or "").split(";"):
                item = raw.strip().rstrip("/\\")
                key = item.lower()
                if item and key not in seen_paths:
                    seen_paths.add(key)
                    paths.append(item)
                elif item and key in seen_paths:
                    changed = True
            for directory in (dsp_dir, point_dir):
                if directory is None or not directory.is_dir():
                    continue
                item = _cdk_relative_name(directory, cdk_dir).rstrip("/\\")
                key = item.lower()
                if key not in seen_paths:
                    seen_paths.add(key)
                    paths.append(item)
                    changed = True
            lib_path.text = ";".join(paths)

        other_flags = _local_child(linker, "OtherFlags")
        if other_flags is not None and sym_path is not None:
            flags = other_flags.text or ""
            sym_token = "$(ProjectPath)/" + _cdk_relative_name(sym_path, cdk_dir)
            if sym_token not in flags:
                flags = (flags.rstrip() + " " + sym_token).strip()
                other_flags.text = flags
                changed = True

    return changed


def _restore_linker_settings(
    root: ElementTree.Element, cdkproj: Path
) -> list[str]:
    """Restore link fields lost by an older refresh pass from the first backup."""
    backup = cdkproj.with_name(cdkproj.name + ".autoburn.bak")
    if not backup.is_file():
        return []
    try:
        backup_root = ElementTree.parse(backup).getroot()
    except (ElementTree.ParseError, OSError):
        return []

    linker = next(
        (elem for elem in root.iter() if _element_local_name(elem) == "Linker"),
        None,
    )
    backup_linker = next(
        (
            elem
            for elem in backup_root.iter()
            if _element_local_name(elem) == "Linker"
        ),
        None,
    )
    if linker is None or backup_linker is None:
        return []

    restored: list[str] = []
    previously: ElementTree.Element | None = None
    for child in backup_linker:
        name = _element_local_name(child)
        current = _local_child(linker, name)
        if current is not None:
            previously = current
            continue
        if name not in ("LDFile", "AutoLDFile"):
            continue
        elem = ElementTree.Element(name)
        elem.text = child.text or ""
        if previously is not None:
            index = list(linker).index(previously) + 1
            linker.insert(index, elem)
        else:
            linker.insert(0, elem)
        previously = elem
        restored.append(name)
    return restored


def _compute_physical_dirs(
    root: ElementTree.Element,
    cdk_dir: Path,
    project_dir: Path,
    sdk_root: Path,
) -> dict[int, Path]:
    physical: dict[int, Path] = {}
    roots = {
        "kernel": sdk_root / "kernel",
        "libs": sdk_root / "libs",
        "platform": sdk_root / "platform",
        "project": project_dir,
        "subsys": sdk_root / "subsys",
    }

    def walk(node: ElementTree.Element, base: Path | None) -> None:
        if not node.tag.endswith("VirtualDirectory"):
            return
        name = node.attrib.get("Name", "")
        phys: Path | None = None
        if base is not None:
            candidate = base / name
            if candidate.is_dir():
                phys = candidate
        if phys is None and name in roots and roots[name].is_dir():
            phys = roots[name]
        if phys is None:
            for child in node.findall("File"):
                source = resolve_source_path(
                    cdk_dir, child.attrib.get("Name", "")
                ).resolve()
                if source.parent.is_dir():
                    phys = source.parent
                    break
        if phys is not None:
            physical[id(node)] = phys
        for child in node.findall("VirtualDirectory"):
            walk(child, phys if phys is not None else base)

    for child in root.findall("VirtualDirectory"):
        walk(child, None)
    return physical


def refresh_cdkproj(
    sdk_root: Path,
    project_dir: Path,
    cdk_dir: Path,
    dry_run: bool = False,
    excluded_paths: set[str] | list[str] | None = None,
    exclude_globs: list[str] | None = None,
    modules: set[str] | None = None,
) -> dict[str, list[str]]:
    excluded_paths = {
        _normalize_cdk_path(path) for path in (excluded_paths or [])
    }
    exclude_globs = [
        _normalize_cdk_path(pattern) for pattern in (exclude_globs or [])
    ]
    if modules is None:
        modules = _detect_mmw_hif_modules(project_dir)
    modules = set(modules)
    removed: list[str] = []
    added: list[str] = []

    cdkproj = find_cdk_project_file(cdk_dir)
    try:
        tree = ElementTree.parse(cdkproj)
    except (ElementTree.ParseError, OSError) as exc:
        raise AutoburnError(f"Cannot parse {cdkproj}: {exc}")
    root = tree.getroot()
    module_dir_ids, module_dirs, module_dirs_changed = _ensure_module_virtual_dirs(
        root, sdk_root, modules, cdkproj
    )
    excluded_subtrees = _excluded_virtual_dir_ids(root)
    restored_linker = _restore_linker_settings(root, cdkproj)

    existing: dict[str, list[ElementTree.Element]] = {}
    parents: dict[int, ElementTree.Element] = {}
    for parent in root.iter():
        for child in parent:
            parents[id(child)] = parent
            if _element_local_name(child) == "File":
                norm = _normalize_cdk_path(child.attrib.get("Name", ""))
                existing.setdefault(norm, []).append(child)

    for norm, elements in list(existing.items()):
        source = resolve_source_path(cdk_dir, norm).resolve()
        should_remove = (
            not source.is_file()
            or norm in excluded_paths
            or _matches_any_glob(norm, exclude_globs)
        )
        if not should_remove:
            continue
        for element in elements:
            parent = parents.get(id(element))
            if parent is not None:
                parent.remove(element)
        removed.append(norm)

    physical = _compute_physical_dirs(root, cdk_dir, project_dir, sdk_root)
    known = set(existing)
    for node in root.iter("VirtualDirectory"):
        if id(node) in excluded_subtrees:
            continue
        direct_files = [
            child
            for child in node
            if _element_local_name(child) == "File" and child.attrib.get("Name")
        ]
        if not direct_files and id(node) not in module_dir_ids:
            continue
        phys = physical.get(id(node))
        if phys is None or not phys.is_dir():
            continue
        direct_names = {
            _normalize_cdk_path(child.attrib.get("Name", ""))
            for child in direct_files
        }
        suffixes = {
            Path(child.attrib.get("Name", "")).suffix.lower()
            for child in direct_files
        }
        if not suffixes:
            suffixes = set(PROJECT_SOURCE_SUFFIXES)
        for source in sorted(phys.iterdir()):
            if not source.is_file() or source.suffix.lower() not in suffixes:
                continue
            rel = _cdk_relative_name(source, cdk_dir)
            if (
                rel in known
                or rel in direct_names
                or rel in excluded_paths
                or _matches_any_glob(rel, exclude_globs)
            ):
                continue
            file_elem = ElementTree.SubElement(node, "File")
            file_elem.set("Name", rel)
            ElementTree.SubElement(file_elem, "FileOption")
            known.add(rel)
            added.append(rel)

    project_node = next(
        (
            child
            for child in root.findall("VirtualDirectory")
            if child.attrib.get("Name") == "project"
        ),
        None,
    )
    if project_node is None:
        project_node = ElementTree.SubElement(root, "VirtualDirectory")
        project_node.set("Name", "project")

    for source in _iter_project_source_files(project_dir):
        rel = _cdk_relative_name(source, cdk_dir)
        if (
            rel in known
            or rel in excluded_paths
            or _matches_any_glob(rel, exclude_globs)
        ):
            continue
        project_rel = source.relative_to(project_dir)
        node = project_node
        for part in project_rel.parts[:-1]:
            child = next(
                (
                    candidate
                    for candidate in node
                    if candidate.tag.endswith("VirtualDirectory")
                    and candidate.attrib.get("Name") == part
                ),
                None,
            )
            if child is None:
                child = ElementTree.SubElement(node, "VirtualDirectory")
                child.set("Name", part)
            node = child
        file_elem = ElementTree.SubElement(node, "File")
        file_elem.set("Name", rel)
        ElementTree.SubElement(file_elem, "FileOption")
        known.add(rel)
        added.append(rel)

    include_path_changed = _ensure_include_paths(
        root, cdk_dir, sdk_root, modules
    )
    linker_changed = (
        _ensure_mmw_linker(root, cdk_dir, sdk_root, project_dir)
        if "mmw" in modules
        else False
    )
    changed = bool(
        removed
        or added
        or restored_linker
        or module_dirs_changed
        or include_path_changed
        or linker_changed
    )
    if changed:
        _write_cdkproj(root, cdkproj, dry_run)
    return {
        "removed": removed,
        "added": added,
        "restored": restored_linker,
        "modules": sorted(modules),
        "module_dirs": module_dirs,
        "include_paths_changed": include_path_changed,
        "linker_changed": linker_changed,
        "changed": changed,
    }


def find_cdk_make(explicit: str | None = None) -> Path | None:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit).expanduser().resolve())
    for env_name in ("AGENT_AUTO_CDK", "CDK_MAKE"):
        env_value = os.environ.get(env_name)
        if env_value:
            candidates.append(Path(env_value).expanduser().resolve())

    common = (
        Path("C:/C-Sky/CDK/cdk-make.exe"),
        Path("C:/C-Sky/CDKRepo/cdk-make.exe"),
    )
    candidates.extend(common)

    for candidate in candidates:
        if candidate.is_file():
            return candidate

    found = shutil.which("cdk-make") or shutil.which("cdk-make.exe")
    if found:
        return Path(found).resolve()
    return None


def find_download_dll(
    explicit: str | None = None,
    sdk_root: Path | None = None,
    required: bool = True,
) -> Path | None:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit).expanduser().resolve())
    env_dll = os.environ.get("AGENT_AUTO_DLL")
    if env_dll:
        candidates.append(Path(env_dll).expanduser().resolve())

    if sdk_root is not None:
        sdk_candidates = (
            sdk_root / "tools" / "DownloadLib" / "DownloadLib.dll",
            sdk_root / "tools" / "download_cli" / "resources" / "DownloadLib.dll",
            sdk_root / "auto_burn" / "resources" / "DownloadLib.dll",
            sdk_root / "03_Tool" / "DownloadLib" / "DownloadLib.dll",
            sdk_root / "02_SDK" / "psdf_sdk" / "tools" / "DownloadLib" / "DownloadLib.dll",
        )
        candidates.extend(sdk_candidates)

    candidates.extend(
        (
            SCRIPT_DIR.parent / "assets" / "DownloadLib.dll",
            SCRIPT_DIR.parent / "DownloadLib.dll",
            SCRIPT_DIR / "DownloadLib.dll",
        )
    )

    seen: set[str] = set()
    for candidate in candidates:
        key = str(candidate).lower()
        if key in seen:
            continue
        seen.add(key)
        if candidate.is_file():
            return candidate

    if required:
        raise AutoburnError(
            "DownloadLib.dll not found. The skill bundles a copy in "
            "assets/DownloadLib.dll; it can also be supplied with --dll or "
            "AGENT_AUTO_DLL."
        )
    return None


def discover_project_configs(project_file: Path) -> list[str]:
    configs: set[str] = set()
    try:
        tree = ElementTree.parse(project_file)
    except (ElementTree.ParseError, OSError):
        return ["psdf"]
    for node in tree.iter():
        local_name = _element_local_name(node)
        if local_name == "BuildConfig" and node.attrib.get("Name"):
            configs.add(node.attrib["Name"])
        elif local_name == "Project" and node.attrib.get("ConfigName"):
            config = node.attrib.get("ConfigName")
            if config:
                configs.add(config)
    return sorted(configs)


def find_firmware_image(
    project_dir: Path,
    explicit: str | None = None,
    required: bool = True,
    cdk_dir: Path | None = None,
) -> Path | None:
    if explicit:
        image = Path(explicit).expanduser().resolve()
        if not image.is_file():
            raise AutoburnError(f"Firmware image not found: {image}")
        return image

    candidates: list[Path] = []
    cdk_dirs: list[Path]
    if cdk_dir is not None:
        cdk_dirs = [cdk_dir]
    else:
        cdk_dirs = _project_cdk_dirs(project_dir)

    for cdk_dir in cdk_dirs:
        if not cdk_dir.is_dir():
            continue
        output_dir = cdk_dir / "output"
        if output_dir.is_dir():
            candidates.extend(
                (path for path in output_dir.glob("*.img") if path.is_file())
            )
    if candidates:
        return sorted(candidates, key=lambda p: p.stat().st_mtime, reverse=True)[0]

    if required:
        raise AutoburnError(
            f"No .img found under {project_dir} (expected cdk*/output). "
            "Build the project first or "
            "pass --image <path>."
        )
    return None


def list_ports() -> list[str]:
    script = (
        "[Console]::OutputEncoding = [System.Text.Encoding]::UTF8; "
        "[System.IO.Ports.SerialPort]::GetPortNames() | "
        "ForEach-Object { [Console]::Out.WriteLine($_) }"
    )
    try:
        proc = subprocess.run(
            [
                "powershell.exe",
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-Command",
                script,
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
    except FileNotFoundError:
        raise AutoburnError("PowerShell was not found on PATH.")
    if proc.returncode != 0:
        raise AutoburnError(
            f"Failed to list COM ports. PowerShell stderr: {proc.stderr.strip()}"
        )
    return [line.strip() for line in proc.stdout.splitlines() if line.strip()]


def pick_port(explicit: str | None) -> str:
    if explicit:
        return explicit
    ports = list_ports()
    if len(ports) == 1:
        return ports[0]
    if not ports:
        raise AutoburnError("No COM ports found. Connect the board and retry.")
    raise AutoburnError(
        "Multiple COM ports found; pass --port explicitly. Ports: "
        + ", ".join(ports)
    )


def write_temp_ps(script: str) -> Path:
    fd, raw_path = tempfile.mkstemp(prefix="autoburn_", suffix=".ps1")
    os.close(fd)
    path = Path(raw_path)
    # PowerShell 5 needs a BOM to parse non-ASCII literals correctly; the
    # DownloadLib adapter contains translated Chinese source strings.
    path.write_text(script, encoding="utf-8-sig", errors="replace")
    return path


def _power_shell_file_args(path: Path, params: dict[str, object]) -> list[str]:
    cmd = [
        "powershell.exe",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        str(path),
    ]
    for name, value in params.items():
        if isinstance(value, bool):
            cmd.append(f"-{name}:{str(value)}")
        else:
            cmd.extend([f"-{name}", str(value)])
    return cmd


def run_ps_capture(script: str, params: dict[str, object]) -> subprocess.CompletedProcess[str]:
    path = write_temp_ps(script)
    try:
        return subprocess.run(
            _power_shell_file_args(path, params),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
    except FileNotFoundError:
        raise AutoburnError("PowerShell was not found on PATH.")
    finally:
        try:
            path.unlink(missing_ok=True)
        except OSError:
            pass


def run_ps_live(script: str, params: dict[str, object]) -> int:
    path = write_temp_ps(script)
    try:
        proc = subprocess.Popen(
            _power_shell_file_args(path, params),
            stdout=sys.stdout,
            stderr=sys.stderr,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        return proc.wait()
    except FileNotFoundError:
        print("ERROR: PowerShell was not found on PATH.", file=sys.stderr)
        return 2
    finally:
        try:
            path.unlink(missing_ok=True)
        except OSError:
            pass


PS_SEND = r"""
param(
    [Parameter(Mandatory = $true)][string]$ComPort,
    [Parameter(Mandatory = $true)][int]$BaudRate,
    [Parameter(Mandatory = $true)][string]$HexData,
    [int]$ReadTimeoutMs = 1500
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$tokens = @($HexData.Trim() -split '\s+' | Where-Object { $_ -ne '' })
if ($tokens.Count -eq 0) {
    Write-Output "ERROR: HexData is empty."
    exit 1
}

$byteList = New-Object System.Collections.Generic.List[byte]
try {
    foreach ($token in $tokens) {
        $byteList.Add([Convert]::ToByte($token, 16))
    }
} catch {
    Write-Output "ERROR: Invalid HexData."
    exit 1
}

$bytes = $byteList.ToArray()
$rx = New-Object System.Collections.Generic.List[byte]
$serial = $null

try {
    $serial = New-Object System.IO.Ports.SerialPort(
        $ComPort,
        $BaudRate,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One
    )
    $serial.ReadTimeout = 200
    $serial.WriteTimeout = 2000
    $serial.DtrEnable = $false
    $serial.RtsEnable = $false
    $serial.Open()
    $serial.DiscardInBuffer()
    $serial.Write($bytes, 0, $bytes.Length)

    $deadline = [Environment]::TickCount + $ReadTimeoutMs
    while ([Environment]::TickCount -lt $deadline) {
        try {
            $value = $serial.ReadByte()
            $rx.Add([byte]$value)
        } catch [System.TimeoutException] {
            Start-Sleep -Milliseconds 20
        }
    }
} catch {
    $message = $_.Exception.Message
    if ($message -match 'Access to the port.*denied|access.*denied') {
        Write-Output "ERROR: Serial port $ComPort is already in use."
        exit 3
    }
    if ($message -match 'does not exist') {
        Write-Output "ERROR: Serial port $ComPort does not exist."
        exit 3
    }
    Write-Output "ERROR: Serial send failed on $ComPort : $message"
    exit 2
} finally {
    if ($null -ne $serial) {
        try { $serial.DiscardInBuffer() } catch { }
    }
}

$rxHex = ($rx.ToArray() | ForEach-Object { $_.ToString('X2') }) -join ' '
$rxText = -join ($rx.ToArray() | ForEach-Object {
    if ($_ -ge 32 -and $_ -lt 127) { [char]$_ } else { '.' }
})
Write-Output "RX_HEX=$rxHex"
Write-Output "RX_TEXT=$rxText"
Write-Output "SERIAL_WRITE_OK=$($rx.Count)"
exit 0
"""


PS_MONITOR = r"""
param(
    [Parameter(Mandatory = $true)][string]$Port,
    [string]$Baud = "115200",
    [double]$Duration = 10,
    [double]$IdleTimeout = 3,
    [string]$UntilText = "",
    [string]$OutPath = "",
    [string]$Timestamp = "True",
    [string]$HexOutput = "False",
    [int]$MaxBytes = 0
)

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$timestampEnabled = [bool]::Parse($Timestamp)
$hexOutputEnabled = [bool]::Parse($HexOutput)

function Write-Output-Line {
    param([string]$Line)
    if ($Line.EndsWith("`r")) {
        $Line = $Line.Substring(0, $Line.Length - 1)
    }
    if ($timestampEnabled) {
        $formatted = "[{0}] {1}" -f [DateTime]::Now.ToString("HH:mm:ss.fff"), $Line
    } else {
        $formatted = $Line
    }
    [Console]::Out.WriteLine($formatted)
    if ($OutPath) {
        [System.IO.File]::AppendAllText($OutPath, $formatted + [Environment]::NewLine, [System.Text.Encoding]::UTF8)
    }
}

$exitCode = 0
$sp = $null
try {
    try { Add-Type -AssemblyName System.IO.Ports -ErrorAction Stop } catch { }
    $sp = New-Object System.IO.Ports.SerialPort
    $sp.PortName = $Port
    $sp.BaudRate = [int]$Baud
    $sp.Parity = [System.IO.Ports.Parity]::None
    $sp.DataBits = 8
    $sp.StopBits = [System.IO.Ports.StopBits]::One
    $sp.Handshake = [System.IO.Ports.Handshake]::None
    $sp.ReadTimeout = 500
    $sp.DtrEnable = $false
    $sp.RtsEnable = $false
    $sp.Encoding = [System.Text.Encoding]::UTF8
    $sp.Open()

    if ($OutPath) {
        [System.IO.File]::WriteAllText($OutPath, "", [System.Text.Encoding]::UTF8)
    }

    $allText = New-Object System.Text.StringBuilder
    $lineBuffer = ""
    $hexLine = ""
    $hexLineBytes = 0
    $bytePool = New-Object byte[] 4096
    $capturedCount = 0
    $lastActivity = $null
    $deadline = [DateTime]::Now.AddSeconds($Duration)
    $matched = $false
    $stopReason = "duration"

    while ([DateTime]::Now -lt $deadline -and ($MaxBytes -le 0 -or $capturedCount -lt $MaxBytes)) {
        if ($hexOutputEnabled) {
            $bytesRead = 0
            try {
                $bytesRead = $sp.BaseStream.Read($bytePool, 0, $bytePool.Length)
            } catch [System.TimeoutException] {
                $bytesRead = 0
            }
            if ($bytesRead -gt 0) {
                if ($MaxBytes -gt 0 -and ($capturedCount + $bytesRead) -gt $MaxBytes) {
                    $bytesRead = $MaxBytes - $capturedCount
                }
                $capturedCount += $bytesRead
                $lastActivity = [DateTime]::Now
                for ($i = 0; $i -lt $bytesRead; $i++) {
                    $hexToken = $bytePool[$i].ToString("X2")
                    if ($UntilText) { [void]$allText.Append($hexToken) }
                    if ($hexLineBytes -gt 0) { $hexLine += " " }
                    $hexLine += $hexToken
                    $hexLineBytes++
                    if ($hexLineBytes -ge 16) {
                        Write-Output-Line -Line $hexLine
                        $hexLine = ""
                        $hexLineBytes = 0
                    }
                }
                if ($UntilText -and $allText.ToString().Contains($UntilText)) {
                    $matched = $true
                    $stopReason = "until-text"
                    break
                }
            } elseif ($IdleTimeout -gt 0 -and $lastActivity) {
                $idleSeconds = ([DateTime]::Now - $lastActivity).TotalSeconds
                if ($idleSeconds -ge $IdleTimeout) {
                    $stopReason = "idle-timeout"
                    break
                }
            }
        } else {
            $data = $sp.ReadExisting()
            if (-not [string]::IsNullOrEmpty($data)) {
                $capturedCount += $data.Length
                $lastActivity = [DateTime]::Now
                [void]$allText.Append($data)
                $lineBuffer += $data
                $parts = $lineBuffer -split "`n"
                if ($parts.Count -gt 1) {
                    $lineBuffer = [string]$parts[$parts.Count - 1]
                    for ($i = 0; $i -lt ($parts.Count - 1); $i++) {
                        Write-Output-Line -Line $parts[$i]
                    }
                }
                if ($UntilText -and $allText.ToString().Contains($UntilText)) {
                    $matched = $true
                    $stopReason = "until-text"
                    break
                }
                if ($MaxBytes -gt 0 -and $capturedCount -ge $MaxBytes) {
                    $stopReason = "max-bytes"
                    break
                }
            } elseif ($IdleTimeout -gt 0 -and $lastActivity) {
                $idleSeconds = ([DateTime]::Now - $lastActivity).TotalSeconds
                if ($idleSeconds -ge $IdleTimeout) {
                    $stopReason = "idle-timeout"
                    break
                }
            }
        }
        Start-Sleep -Milliseconds 50
    }

    if ($hexOutputEnabled) {
        if ($hexLine) { Write-Output-Line -Line $hexLine }
    } elseif ($lineBuffer) {
        Write-Output-Line -Line $lineBuffer
    }

    [Console]::Error.WriteLine(
        "serial monitor summary: port=$Port baud=$Baud duration=$Duration stop=$stopReason captured_count=$capturedCount hex_output=$hexOutputEnabled until_text_match=$matched"
    )
    if ($UntilText -and -not $matched) {
        $exitCode = 3
    }
} catch {
    [Console]::Error.WriteLine("ERROR: $($_.Exception.Message)")
    $exitCode = 2
} finally {
    if ($sp -ne $null) {
        try { $sp.DtrEnable = $false } catch { }
        try { $sp.RtsEnable = $false } catch { }
    }
}
exit $exitCode
"""


PS_DOWNLOAD = r"""
param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [Parameter(Mandatory = $true)][string]$ComPort,
    [Parameter(Mandatory = $true)][string]$BaudRate,
    [Parameter(Mandatory = $true)][string]$ImagePath,
    [int]$Retry = 3,
    [uint32]$WaitAckTimeout = 5000,
    [string]$ReadBackCheck = "True",
    [string]$DownloadFinishStart = "True"
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

function Write-Err {
    param([string]$Message)
    [Console]::Error.WriteLine("ERROR: $Message")
}

function Convert-ToEnglishLine {
    param([string]$Line)

    if ([string]::IsNullOrWhiteSpace($Line)) {
        return ""
    }
    if ($Line -match '尝试第\s*(\d+)\s*次同步连接') {
        return "SYNC: Trying connection $($matches[1])..."
    }
    if ($Line -match '同步连接') {
        return "SYNC: Synchronizing serial connection..."
    }
    if ($Line -match '擦除Flash|Flash擦除') {
        return "FLASH: Erasing flash..."
    }
    if ($Line -match 'Flash烧录') {
        return "FLASH: Writing firmware..."
    }
    if ($Line -match '回读校验') {
        return "VERIFY: Running read-back verification..."
    }
    if ($Line -match '下载成功') {
        return "SUCCESS: Download completed."
    }
    if ($Line -match '正在中止线程') {
        return "INFO: Stopping background worker thread."
    }
    if ($Line -match '串口打开失败') {
        return "ERROR: Failed to open the serial port."
    }
    if ($Line -match '串口关闭异常') {
        return "ERROR: Failed to close the serial port cleanly."
    }
    if ($Line -match 'Brom调试异常') {
        return "ERROR: BROM debug reported an exception."
    }
    if ($Line -match '等待ACK') {
        return "ERROR: Timed out waiting for ACK."
    }
    if ($Line -match '擦除失败|擦除错误') {
        return "ERROR: Flash erase failed."
    }
    if ($Line -match '烧录失败|烧录错误') {
        return "ERROR: Flash write failed."
    }
    if ($Line -match '校验不一致|校验失败') {
        return "ERROR: Read-back verification failed."
    }
    if ($Line -match '[^\u0000-\u007F]') {
        return "INFO: DownloadLib emitted an internal non-English message."
    }
    return $Line.Trim()
}

function Show-CapturedMessages {
    param([string]$Text, [string]$StreamName)

    if ([string]::IsNullOrWhiteSpace($Text)) {
        return
    }
    foreach ($rawLine in $Text -split "`r?`n") {
        $cleanLine = $rawLine.Trim()
        if ([string]::IsNullOrWhiteSpace($cleanLine)) {
            continue
        }
        $englishLine = Convert-ToEnglishLine $cleanLine
        if ($StreamName -eq "ERR") {
            Write-Err $englishLine
        } else {
            [Console]::Out.WriteLine($englishLine)
        }
    }
}

function Get-ErrorHint {
    param([int]$Code)

    switch ($Code) {
        1   { return "The image checksum is invalid. Rebuild the firmware image and retry." }
        2   { return "DownloadLib rejected a command sent to the board." }
        3   { return "The image contains an illegal flash address." }
        4   { return "The board returned an unknown command." }
        5   { return "Flash erase failed. Confirm the board is in update/burn mode and the wiring is stable." }
        6   { return "Flash write failed. Confirm the firmware image is valid and the flash is not write-protected." }
        7   { return "Flash read failed during the operation." }
        8   { return "The firmware data length is invalid." }
        9   { return "DownloadLib could not open the firmware file. Check --image and the local copy path." }
        10  { return "The serial port could not be opened. Check the COM number and close any terminal that already uses it." }
        11  { return "Baud-rate synchronization timed out. Put the board into update/burn mode and retry." }
        12  { return "Timed out waiting for ACK. Confirm the board is powered and in update/burn mode." }
        13  { return "The firmware image size is not correctly aligned for flashing." }
        14  { return "Read-back verification failed. The board contents do not match the image." }
        15  { return "The serial port reported an abnormal state. Check the adapter, driver, and whether another program owns the port." }
        16  { return "The flash erase address is not 4K aligned. The image is not suitable for this loader." }
        28  { return "Flash erase ACK timeout. Put the board into update/burn mode and retry." }
        44  { return "Download ACK timeout. Confirm the serial link is stable and the board is in update/burn mode." }
        60  { return "Read-back ACK timeout. Confirm the serial link is stable." }
        76  { return "Reset/start (Set-PC) ACK timeout. Confirm the board is ready for download." }
        default { return "Unknown DownloadLib error. Check hardware, image validity, and serial connectivity." }
    }
}

try {
    if (-not (Test-Path -LiteralPath $DllPath)) {
        Write-Err "DownloadLib.dll not found: $DllPath"
        exit 2
    }
    if (-not (Test-Path -LiteralPath $ImagePath)) {
        Write-Err "Firmware image not found: $ImagePath"
        exit 1
    }

    Unblock-File -LiteralPath $DllPath -ErrorAction SilentlyContinue
    [System.Reflection.Assembly]::LoadFile($DllPath) | Out-Null
    $download = New-Object -TypeName "DownloadLib.Download"
    $download.SetTryAgainTimes([int]$Retry)
    $download.SetWaitAckTimeout([uint32]$WaitAckTimeout)
    $readBackEnabled = [bool]::Parse($ReadBackCheck)
    $startAfterDownload = [bool]::Parse($DownloadFinishStart)

    [Console]::Out.WriteLine("Starting download: port=$ComPort baud=$BaudRate image=$ImagePath retry=$Retry")
    [Console]::Out.WriteLine("READ_BACK_CHECK=" + $(if ($readBackEnabled) { "enabled" } else { "disabled" }))

    $originalOut = [Console]::Out
    $originalErr = [Console]::Error
    $capturedOut = [System.IO.StringWriter]::new()
    $capturedErr = [System.IO.StringWriter]::new()
    [Console]::SetOut($capturedOut)
    [Console]::SetError($capturedErr)
    try {
        $result = $download.StartDownloadFw(
            [string]$ComPort,
            [string]$BaudRate,
            [string]$ImagePath,
            $readBackEnabled,
            $startAfterDownload
        )
    } finally {
        [Console]::SetOut($originalOut)
        [Console]::SetError($originalErr)
    }

    Show-CapturedMessages -Text $capturedOut.ToString() -StreamName "OUT"
    Show-CapturedMessages -Text $capturedErr.ToString() -StreamName "ERR"

    $code = [int]$result
    if ($code -eq 0x59) {
        $progress = $download.GetDownloadProgressValue()
        [Console]::Out.WriteLine("DOWNLOAD SUCCESS: Firmware was written successfully.")
        [Console]::Out.WriteLine("STATUS: DownloadLib code=0x59, final_progress=$progress")
        if ($readBackEnabled) {
            [Console]::Out.WriteLine("READ_BACK_CHECK_OK=1")
        }
        if ($startAfterDownload) {
            [Console]::Out.WriteLine("APPLICATION: Reset and auto-start after download was enabled.")
        }
        exit 0
    }

    $message = switch ($code) {
        0   { "unknown error" }
        1   { "checksum error" }
        2   { "command error" }
        3   { "illegal address" }
        4   { "unknown command" }
        5   { "flash erase error" }
        6   { "flash burn error" }
        7   { "flash read error" }
        8   { "data length error" }
        9   { "firmware file open error" }
        10  { "serial port open error" }
        11  { "baud rate sync timeout" }
        12  { "wait ack timeout" }
        13  { "firmware size padding error" }
        14  { "read-back check error" }
        15  { "serial port abnormal" }
        16  { "erase address not 4K aligned" }
        28  { "flash erase wait ack timeout" }
        44  { "download wait ack timeout" }
        60  { "read-back check wait ack timeout" }
        76  { "set PC wait ack timeout" }
        default { "unmapped DownloadLib error" }
    }
    $hint = Get-ErrorHint -Code $code
    Write-Err "DownloadLib returned $code ($message). $hint"

    $serialCodes = @(10, 11, 12, 13, 14, 15, 16, 28, 44, 60, 76)
    if ($serialCodes -contains $code) {
        exit 3
    }
    exit 4
} catch {
    $exceptionMessage = $_.Exception.Message
    if ($exceptionMessage -match 'network location|loadFromRemoteSources') {
        Write-Err "DownloadLib.dll was loaded from a network/UNC path. Use --copy-dir with a local folder path and retry."
        exit 2
    }
    if ($exceptionMessage -match 'Access to the port.*denied|access.*denied') {
        Write-Err "Serial port $ComPort is already in use by another program. Close the terminal or serial monitor holding it, then retry."
        exit 3
    }
    if ($exceptionMessage -match 'does not exist') {
        Write-Err "Serial port $ComPort does not exist on this system. Check Device Manager and the COM number."
        exit 3
    }
    Write-Err "Unexpected PowerShell adapter error: $exceptionMessage"
    exit 2
}
"""


PS_BROM_SEND = r"""
param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [Parameter(Mandatory = $true)][string]$ComPort,
    [Parameter(Mandatory = $true)][string]$BaudRate,
    [Parameter(Mandatory = $true)][string]$CmdId,
    [string]$Mode = "sram-write",
    [string]$Addr = "",
    [string]$Len = "",
    [string]$DataHex = "",
    [int]$ReadTimeoutMs = 5000,
    [string]$DebugOutput = "False",
    [string]$DryRun = "False",
    [string]$ExpectedAckHex = ""
)

if ($Mode -ne "sram-write" -and $Mode -ne "brom-reset") {
    Write-Output "ERROR: Only --mode sram-write and --mode brom-reset are supported."
    exit 3
}

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
Unblock-File -LiteralPath $DllPath -ErrorAction SilentlyContinue
[System.Reflection.Assembly]::LoadFile($DllPath) | Out-Null

$flags = [System.Reflection.BindingFlags]::Instance -bor [System.Reflection.BindingFlags]::Public -bor [System.Reflection.BindingFlags]::NonPublic -bor [System.Reflection.BindingFlags]::Static
$asm = [System.Reflection.Assembly]::LoadFile($DllPath)
$bt = $asm.GetType("DownloadLib.BromCommand")
$downloadType = $asm.GetType("DownloadLib.Download")
$apiType = $asm.GetType("DownloadLib.DownloadApi")

function Convert-HexBytes {
    param([string]$Hex)
    $tokens = @($Hex.Trim() -split '\s+' | Where-Object { $_ -ne '' })
    $bytes = New-Object System.Collections.Generic.List[byte]
    foreach ($token in $tokens) {
        $bytes.Add([Convert]::ToByte($token, 16))
    }
    return ,$bytes.ToArray()
}

function Format-Hex {
    param($Bytes)
    if ($null -eq $Bytes -or $Bytes.Length -eq 0) {
        return ""
    }
    return (($Bytes | ForEach-Object { $_.ToString("X2") }) -join " ")
}

function Get-BromMethod {
    param([string]$Name, [int]$ParameterCount)
    return $bt.GetMethods($flags) |
        Where-Object { $_.Name -eq $Name -and $_.GetParameters().Count -eq $ParameterCount } |
        Select-Object -First 1
}

function Invoke-BromMethod {
    param($Object, $Method, $SingleParameter)
    $argument = [object[]]::new(1)
    $argument[0] = $SingleParameter
    return $Method.Invoke($Object, $argument)
}

function New-BromCommand {
    param([uint32]$Size)
    $ctor = $bt.GetConstructor(
        $flags,
        $null,
        @([byte], [uint16], [uint32]),
        $null
    )
    return $ctor.Invoke(@([byte]0, [uint16]0, $Size))
}

function Build-RawCommand {
    param([byte]$Cmd, [byte[]]$Data)
    $size = [Math]::Max(1, 1 + $Data.Length)
    $cmdObj = New-BromCommand -Size ([uint32]$size)
    [void](Invoke-BromMethod -Object $cmdObj -Method (Get-BromMethod -Name "WriteCmd" -ParameterCount 1) -SingleParameter ([byte]$Cmd))
    if ($Data.Length -gt 0) {
        [void](Invoke-BromMethod -Object $cmdObj -Method (Get-BromMethod -Name "AppendDataLimit" -ParameterCount 1) -SingleParameter ([byte[]]$Data))
    }
    $bytes = [byte[]](Invoke-BromMethod -Object $cmdObj -Method (Get-BromMethod -Name "ToBytes" -ParameterCount 1) -SingleParameter ([bool]$true))
    return ,$bytes
}

function Build-WriteSramHeader {
    param([byte]$Cmd, [uint32]$Addr, [uint32]$Len, [byte[]]$Data)
    $wt = $asm.GetType("DownloadLib.TCmdWrSramHeader")
    $h = [Activator]::CreateInstance($wt)
    $wt.GetField("cmdId").SetValue($h, $Cmd)
    $wt.GetField("addr").SetValue($h, $Addr)
    $wt.GetField("len").SetValue($h, $Len)
    $checksumMethod = Get-BromMethod -Name "BromCheckSum16" -ParameterCount 1
    $checksumArgs = [object[]]::new(1)
    $checksumArgs[0] = $Data
    $dchecksum = [uint16]$checksumMethod.Invoke($null, $checksumArgs)
    $wt.GetField("dCheckSum").SetValue($h, $dchecksum)

    $buildMethod = $apiType.GetMethods($flags) |
        Where-Object {
            $_.Name -eq "buildBromCmd" -and
            $_.GetParameters().Count -eq 1 -and
            $_.GetParameters()[0].ParameterType -eq [object]
        } |
        Select-Object -First 1
    $cmdObj = $buildMethod.Invoke($api, [object[]]@([object]$h))
    $bytes = [byte[]](Invoke-BromMethod -Object $cmdObj -Method (Get-BromMethod -Name "ToBytes" -ParameterCount 1) -SingleParameter ([bool]$true))
    return ,$bytes
}

function Build-ReadSramHeader {
    param([byte]$Cmd, [uint32]$Addr, [uint32]$Len)
    $rt = $asm.GetType("DownloadLib.TCmdRdSramHeader")
    $h = [Activator]::CreateInstance($rt)
    $rt.GetField("cmdId").SetValue($h, $Cmd)
    $rt.GetField("addr").SetValue($h, $Addr)
    $rt.GetField("len").SetValue($h, $Len)

    $buildMethod = $apiType.GetMethods($flags) |
        Where-Object {
            $_.Name -eq "buildBromCmd" -and
            $_.GetParameters().Count -eq 1 -and
            $_.GetParameters()[0].ParameterType -eq [object]
        } |
        Select-Object -First 1
    $cmdObj = $buildMethod.Invoke($api, [object[]]@([object]$h))
    $bytes = [byte[]](Invoke-BromMethod -Object $cmdObj -Method (Get-BromMethod -Name "ToBytes" -ParameterCount 1) -SingleParameter ([bool]$true))
    return ,$bytes
}

function Wait-BromAck {
    param([int]$TimeoutMs)
    $deadline = [Environment]::TickCount + $TimeoutMs
    while ($null -eq [BromAckSink]::Ack -and [Environment]::TickCount -lt $deadline) {
        Start-Sleep -Milliseconds 10
    }
    if ($null -eq [BromAckSink]::Ack) {
        return $null
    }
    $ack = [BromAckSink]::Ack
    return ,$ack
}

function Open-RawPort {
    param([string]$Port, [int]$Baud, [int]$TimeoutMs)
    $sp = New-Object System.IO.Ports.SerialPort($Port, $Baud, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    $sp.Handshake = [System.IO.Ports.Handshake]::None
    $sp.ReadTimeout = $TimeoutMs
    $sp.WriteTimeout = $TimeoutMs
    $sp.DtrEnable = $true
    $sp.RtsEnable = $true
    $sp.Open()
    $sp.DiscardInBuffer()
    $sp.DiscardOutBuffer()
    return $sp
}

function Send-RawSync {
    param($Port, [int]$TimeoutMs)
    $deadline = [Environment]::TickCount + $TimeoutMs
    $syncByte = [byte[]]@(0x7E)
    while ([Environment]::TickCount -lt $deadline) {
        try { $Port.Write($syncByte, 0, 1) } catch { }
        $windowDeadline = [Environment]::TickCount + 60
        while ([Environment]::TickCount -lt $windowDeadline) {
            if ($Port.BytesToRead -gt 0) {
                $b = [int]$Port.ReadByte()
                if ($b -eq 0x59) {
                    Write-Output "BROM_SYNC_OK=1"
                    return $true
                }
            }
            Start-Sleep -Milliseconds 5
        }
    }
    return $false
}

function Send-RawBytes {
    param($Port, [byte[]]$Bytes)
    if ($null -eq $Bytes -or $Bytes.Length -eq 0) {
        return
    }
    $Port.Write($Bytes, 0, $Bytes.Length)
    $Port.BaseStream.Flush()
}

function Read-RawResponse {
    param([int]$TimeoutMs)
    $bytes = New-Object System.Collections.Generic.List[byte]
    $deadline = [Environment]::TickCount + $TimeoutMs
    $lastDataTick = [Environment]::TickCount
    while ([Environment]::TickCount -lt $deadline) {
        if ($global:RawPort.BytesToRead -gt 0) {
            $count = [Math]::Min($global:RawPort.BytesToRead, 4096)
            $buf = New-Object byte[] $count
            [void]$global:RawPort.Read($buf, 0, $count)
            foreach ($b in $buf) {
                $bytes.Add($b)
            }
            $lastDataTick = [Environment]::TickCount
        } elseif ($bytes.Count -gt 0 -and ([Environment]::TickCount - $lastDataTick) -ge 100) {
            break
        }
        Start-Sleep -Milliseconds 10
    }
    return ,$bytes.ToArray()
}

$bromResetTxHex = "50 53 49 43 01 02 4D 66 05 00 00 00 13 01 00 00 00"
$bromResetAckHex = "50 53 49 43 02 03 64 66 00 00 00 00"
$cmd = [Convert]::ToByte($CmdId.Trim(), 16)
$dataBytes = [byte[]]@()
if ($DataHex) {
    $dataBytes = Convert-HexBytes -Hex $DataHex
}
$addrValue = [uint32]0
$lenValue = [uint32]0
if ($Addr) {
    $addrValue = [Convert]::ToUInt32($Addr.Trim(), 16)
}
if ($Len) {
    $lenValue = [Convert]::ToUInt32($Len.Trim(), 16)
}

$download = New-Object -TypeName "DownloadLib.Download"
$downloadApiField = $downloadType.GetField("downloadApi", $flags)
$api = $downloadApiField.GetValue($download)
$hasBromSendData = $null -ne $downloadType.GetMethod("BromSendData", $flags)
$bromSendMethod = $downloadType.GetMethod("BromSendData", $flags)

if ($DebugOutput -eq "True") {
    Write-Output "BROM_CMD=0x$($cmd.ToString('X2'))"
    Write-Output "BROM_MODE=$Mode"
}

if ($DryRun -eq "True") {
    if ($Mode -eq "sram-write") {
        $header = Build-WriteSramHeader -Cmd $cmd -Addr $addrValue -Len $lenValue -Data $dataBytes
        Write-Output ("BROM_TX_HEADER_HEX=" + (Format-Hex $header))
        Write-Output ("BROM_TX_DATA_HEX=" + (Format-Hex $dataBytes))
        Write-Output "DRY_RUN: Sequence = ConnectDevice -> SRAMWR header(0x20) -> ACK -> data -> ACK. No hardware touched."
    } elseif ($Mode -eq "sram-read") {
        $header = Build-ReadSramHeader -Cmd $cmd -Addr $addrValue -Len $lenValue
        Write-Output ("BROM_TX_HEADER_HEX=" + (Format-Hex $header))
        Write-Output "DRY_RUN: Sequence = ConnectDevice -> SRAMRD header(0x21) -> read ACK+data. No hardware touched."
    } elseif ($Mode -eq "brom-reset") {
        Write-Output "BROM_RESET_SRAM_ADDR=0x40009070"
        Write-Output "BROM_RESET_SRAM_DATA=00 00 00 00"
        Write-Output ("BROM_TX_HEX=" + $bromResetTxHex)
        $expectedPrintHex = $bromResetAckHex
        if ($ExpectedAckHex) {
            $expectedPrintHex = Format-Hex (Convert-HexBytes -Hex $ExpectedAckHex)
        }
        Write-Output ("BROM_EXPECTED_ACK_HEX=" + $expectedPrintHex)
        Write-Output "DRY_RUN: Sequence = 0x7E sync -> SRAMWR 0x20 @0x40009070=00 00 00 00 -> ACK -> data ACK -> BROM reset(0x13) -> expected ACK. No hardware touched."
    } else {
        $packet = Build-RawCommand -Cmd $cmd -Data $dataBytes
        Write-Output ("BROM_TX_HEX=" + (Format-Hex $packet))
        Write-Output "DRY_RUN: Sequence = ConnectDevice -> BROM frame -> ACK. No hardware touched."
    }
    exit 0
}

if (-not $hasBromSendData) {
    Write-Output "BROM_TRANSPORT=serial"
    $global:RawPort = Open-RawPort -Port $ComPort -Baud ([int]$BaudRate) -TimeoutMs $ReadTimeoutMs
    try {
        if (-not (Send-RawSync -Port $global:RawPort -TimeoutMs ([Math]::Max(5000, $ReadTimeoutMs * 4)))) {
            Write-Output "ERROR: 0x7E baud-rate synchronization timed out; confirm the board is in download mode."
            exit 3
        }
        $global:RawPort.DiscardInBuffer()
        if ($Mode -eq "sram-write") {
            $header = Build-WriteSramHeader -Cmd $cmd -Addr $addrValue -Len $lenValue -Data $dataBytes
            if ($DebugOutput -eq "True") {
                Write-Output ("BROM_TX_HEADER_HEX=" + (Format-Hex $header))
                Write-Output ("BROM_TX_DATA_HEX=" + (Format-Hex $dataBytes))
            }
            Send-RawBytes -Port $global:RawPort -Bytes $header
            $rx = Read-RawResponse -TimeoutMs $ReadTimeoutMs
            if ($rx.Length -eq 0) {
                Write-Output "ERROR: SRAMWR header ACK timeout."
                exit 3
            }
            Write-Output ("BROM_RX_HEADER_HEX=" + (Format-Hex $rx))
            Send-RawBytes -Port $global:RawPort -Bytes $dataBytes
            $rx = Read-RawResponse -TimeoutMs $ReadTimeoutMs
            if ($rx.Length -eq 0) {
                Write-Output "ERROR: SRAMWR data ACK timeout."
                exit 3
            }
            Write-Output ("BROM_RX_DATA_HEX=" + (Format-Hex $rx))
            Write-Output "BROM_SRAM_WRITE_OK=1"
        } elseif ($Mode -eq "sram-read") {
            $header = Build-ReadSramHeader -Cmd $cmd -Addr $addrValue -Len $lenValue
            if ($DebugOutput -eq "True") {
                Write-Output ("BROM_TX_HEADER_HEX=" + (Format-Hex $header))
            }
            Send-RawBytes -Port $global:RawPort -Bytes $header
            $rx = Read-RawResponse -TimeoutMs $ReadTimeoutMs
            if ($rx.Length -eq 0) {
                Write-Output "ERROR: SRAMRD response timeout."
                exit 3
            }
            Write-Output ("BROM_RX_HEX=" + (Format-Hex $rx))
            if ($rx.Length -gt 12) {
                $payload = [byte[]]::new($rx.Length - 12)
                [Array]::Copy($rx, 12, $payload, 0, $payload.Length)
                Write-Output ("BROM_RX_PAYLOAD_HEX=" + (Format-Hex $payload))
            }
            Write-Output "BROM_SRAM_READ_OK=1"
        } elseif ($Mode -eq "brom-reset") {
            $resetSramAddr = [uint32]0x40009070
            $resetSramData = Convert-HexBytes -Hex "00 00 00 00"
            $resetSramLen = [uint32]4
            Write-Output ("BROM_RESET_SRAM_ADDR=0x" + $resetSramAddr.ToString("X8"))
            Write-Output ("BROM_RESET_SRAM_DATA=" + (Format-Hex $resetSramData))
            $sramHeader = Build-WriteSramHeader -Cmd ([byte]0x20) -Addr $resetSramAddr -Len $resetSramLen -Data $resetSramData
            if ($DebugOutput -eq "True") {
                Write-Output ("BROM_RESET_SRAM_HEADER_TX_HEX=" + (Format-Hex $sramHeader))
                Write-Output ("BROM_RESET_SRAM_DATA_TX_HEX=" + (Format-Hex $resetSramData))
            }
            Send-RawBytes -Port $global:RawPort -Bytes $sramHeader
            $rx = Read-RawResponse -TimeoutMs $ReadTimeoutMs
            if ($rx.Length -eq 0) {
                Write-Output "ERROR: SRAMWR reset prerequisite header ACK timeout."
                exit 3
            }
            Write-Output ("BROM_RESET_SRAM_HEADER_ACK_HEX=" + (Format-Hex $rx))
            Send-RawBytes -Port $global:RawPort -Bytes $resetSramData
            $rx = Read-RawResponse -TimeoutMs $ReadTimeoutMs
            if ($rx.Length -eq 0) {
                Write-Output "ERROR: SRAMWR reset prerequisite data ACK timeout."
                exit 3
            }
            Write-Output ("BROM_RESET_SRAM_DATA_ACK_HEX=" + (Format-Hex $rx))
            Write-Output "BROM_SRAM_WRITE_OK=1"
            $resetBytes = Convert-HexBytes -Hex $bromResetTxHex
            if ($ExpectedAckHex) {
                $expectedAckBytes = Convert-HexBytes -Hex $ExpectedAckHex
            } else {
                $expectedAckBytes = Convert-HexBytes -Hex $bromResetAckHex
            }
            if ($DebugOutput -eq "True") {
                Write-Output ("BROM_TX_HEX=" + (Format-Hex $resetBytes))
                Write-Output ("BROM_EXPECTED_ACK_HEX=" + (Format-Hex $expectedAckBytes))
            }
            Send-RawBytes -Port $global:RawPort -Bytes $resetBytes
            $rx = Read-RawResponse -TimeoutMs $ReadTimeoutMs
            if ($rx.Length -eq 0) {
                Write-Output "ERROR: BROM reset response timeout."
                exit 3
            }
            $rxHex = Format-Hex $rx
            $expectedHex = Format-Hex $expectedAckBytes
            Write-Output ("BROM_RX_HEX=" + $rxHex)
            if ($rxHex -ne $expectedHex) {
                Write-Output ("ERROR: BROM reset ACK mismatch. Expected=" + $expectedHex + " Received=" + $rxHex)
                exit 3
            }
            Write-Output "BROM_RESET_ACK_MATCH=1"
            Write-Output "BROM_RESET_OK=1"
        } else {
            $packet = Build-RawCommand -Cmd $cmd -Data $dataBytes
            if ($DebugOutput -eq "True") {
                Write-Output ("BROM_TX_HEX=" + (Format-Hex $packet))
            }
            Send-RawBytes -Port $global:RawPort -Bytes $packet
            $rx = Read-RawResponse -TimeoutMs $ReadTimeoutMs
            if ($rx.Length -eq 0) {
                Write-Output "ERROR: BROM response timeout."
                exit 3
            }
            Write-Output ("BROM_RX_HEX=" + (Format-Hex $rx))
            if ($rx.Length -gt 12) {
                $payload = [byte[]]::new($rx.Length - 12)
                [Array]::Copy($rx, 12, $payload, 0, $payload.Length)
                Write-Output ("BROM_RX_PAYLOAD_HEX=" + (Format-Hex $payload))
            }
            Write-Output "BROM_SEND_OK=1"
        }
    } finally {
        try { $global:RawPort.Close() } catch { }
    }
    exit 0
}

$connect = [int]$download.ConnectDevice([string]$ComPort, [string]$BaudRate)
Write-Output "CONNECT_CODE=$connect"
if ($connect -ne 0x59) {
    Write-Output "ERROR: ConnectDevice failed. The board may not be in download mode."
    try { $download.DisConnectDevice() } catch { }
    exit 3
}
Write-Output "BROM_SYNC_OK=1"

Add-Type -TypeDefinition @"
using System;
public static class BromAckSink
{
    public static byte[] Ack;
    public static System.Delegate Forward;
    public static void Capture(byte[] data)
    {
        if (data == null) { Ack = new byte[0]; }
        else { Ack = (byte[])data.Clone(); }
        if (Forward != null) { Forward.DynamicInvoke(data); }
    }
}
"@

$ackField = $apiType.GetField("ackHandler", $flags)
$originalAck = $ackField.GetValue($api)
$delegateType = $asm.GetType("DownloadLib.DownloadApi+BromCmdThreadAckHandler")
$captureMethod = [BromAckSink].GetMethod("Capture")
$captureDelegate = [Delegate]::CreateDelegate($delegateType, $captureMethod)
[BromAckSink]::Forward = $originalAck
$ackField.SetValue($api, $captureDelegate)

try {
    if ($Mode -eq "sram-write") {
        $header = Build-WriteSramHeader -Cmd $cmd -Addr $addrValue -Len $lenValue -Data $dataBytes
        if ($DebugOutput -eq "True") {
            Write-Output ("BROM_TX_HEADER_HEX=" + (Format-Hex $header))
            Write-Output ("BROM_TX_DATA_HEX=" + (Format-Hex $dataBytes))
        }
        [BromAckSink]::Ack = $null
        $bromSendArgs = [object[]]::new(1)
        $bromSendArgs[0] = [byte[]]$header
        [void]$bromSendMethod.Invoke($download, $bromSendArgs)
        $rx = Wait-BromAck -TimeoutMs $ReadTimeoutMs
        if ($null -eq $rx) {
            Write-Output "ERROR: SRAMWR header ACK timeout."
            exit 3
        }
        Write-Output ("BROM_RX_HEADER_HEX=" + (Format-Hex $rx))
        [BromAckSink]::Ack = $null
        $bromSendArgs = [object[]]::new(1)
        $bromSendArgs[0] = [byte[]]$dataBytes
        [void]$bromSendMethod.Invoke($download, $bromSendArgs)
        $rx = Wait-BromAck -TimeoutMs $ReadTimeoutMs
        if ($null -eq $rx) {
            Write-Output "ERROR: SRAMWR data ACK timeout."
            exit 3
        }
        Write-Output ("BROM_RX_DATA_HEX=" + (Format-Hex $rx))
        Write-Output "BROM_SRAM_WRITE_OK=1"
    } elseif ($Mode -eq "sram-read") {
        $header = Build-ReadSramHeader -Cmd $cmd -Addr $addrValue -Len $lenValue
        if ($DebugOutput -eq "True") {
            Write-Output ("BROM_TX_HEADER_HEX=" + (Format-Hex $header))
        }
        [BromAckSink]::Ack = $null
        $bromSendArgs = [object[]]::new(1)
        $bromSendArgs[0] = [byte[]]$header
        [void]$bromSendMethod.Invoke($download, $bromSendArgs)
        $rx = Wait-BromAck -TimeoutMs $ReadTimeoutMs
        if ($null -eq $rx) {
            Write-Output "ERROR: SRAMRD response timeout."
            exit 3
        }
        Write-Output ("BROM_RX_HEX=" + (Format-Hex $rx))
        if ($rx.Length -gt 12) {
            $payload = [byte[]]::new($rx.Length - 12)
            [Array]::Copy($rx, 12, $payload, 0, $payload.Length)
            Write-Output ("BROM_RX_PAYLOAD_HEX=" + (Format-Hex $payload))
        }
        Write-Output "BROM_SRAM_READ_OK=1"
    } elseif ($Mode -eq "brom-reset") {
        $resetSramAddr = [uint32]0x40009070
        $resetSramData = Convert-HexBytes -Hex "00 00 00 00"
        $resetSramLen = [uint32]4
        Write-Output ("BROM_RESET_SRAM_ADDR=0x" + $resetSramAddr.ToString("X8"))
        Write-Output ("BROM_RESET_SRAM_DATA=" + (Format-Hex $resetSramData))
        $sramHeader = Build-WriteSramHeader -Cmd ([byte]0x20) -Addr $resetSramAddr -Len $resetSramLen -Data $resetSramData
        if ($DebugOutput -eq "True") {
            Write-Output ("BROM_RESET_SRAM_HEADER_TX_HEX=" + (Format-Hex $sramHeader))
            Write-Output ("BROM_RESET_SRAM_DATA_TX_HEX=" + (Format-Hex $resetSramData))
        }
        [BromAckSink]::Ack = $null
        $bromSendArgs = [object[]]::new(1)
        $bromSendArgs[0] = [byte[]]$sramHeader
        [void]$bromSendMethod.Invoke($download, $bromSendArgs)
        $rx = Wait-BromAck -TimeoutMs $ReadTimeoutMs
        if ($null -eq $rx) {
            Write-Output "ERROR: SRAMWR reset prerequisite header ACK timeout."
            exit 3
        }
        Write-Output ("BROM_RESET_SRAM_HEADER_ACK_HEX=" + (Format-Hex $rx))
        [BromAckSink]::Ack = $null
        $bromSendArgs = [object[]]::new(1)
        $bromSendArgs[0] = [byte[]]$resetSramData
        [void]$bromSendMethod.Invoke($download, $bromSendArgs)
        $rx = Wait-BromAck -TimeoutMs $ReadTimeoutMs
        if ($null -eq $rx) {
            Write-Output "ERROR: SRAMWR reset prerequisite data ACK timeout."
            exit 3
        }
        Write-Output ("BROM_RESET_SRAM_DATA_ACK_HEX=" + (Format-Hex $rx))
        Write-Output "BROM_SRAM_WRITE_OK=1"
        $resetBytes = Convert-HexBytes -Hex $bromResetTxHex
        if ($ExpectedAckHex) {
            $expectedAckBytes = Convert-HexBytes -Hex $ExpectedAckHex
        } else {
            $expectedAckBytes = Convert-HexBytes -Hex $bromResetAckHex
        }
        if ($DebugOutput -eq "True") {
            Write-Output ("BROM_TX_HEX=" + (Format-Hex $resetBytes))
            Write-Output ("BROM_EXPECTED_ACK_HEX=" + (Format-Hex $expectedAckBytes))
        }
        [BromAckSink]::Ack = $null
        $bromSendArgs = [object[]]::new(1)
        $bromSendArgs[0] = [byte[]]$resetBytes
        [void]$bromSendMethod.Invoke($download, $bromSendArgs)
        $rx = Wait-BromAck -TimeoutMs $ReadTimeoutMs
        if ($null -eq $rx) {
            Write-Output "ERROR: BROM reset response timeout."
            exit 3
        }
        $rxHex = Format-Hex $rx
        $expectedHex = Format-Hex $expectedAckBytes
        Write-Output ("BROM_RX_HEX=" + $rxHex)
        if ($rxHex -ne $expectedHex) {
            Write-Output ("ERROR: BROM reset ACK mismatch. Expected=" + $expectedHex + " Received=" + $rxHex)
            exit 3
        }
        Write-Output "BROM_RESET_ACK_MATCH=1"
        Write-Output "BROM_RESET_OK=1"
    } else {
        $packet = Build-RawCommand -Cmd $cmd -Data $dataBytes
        if ($DebugOutput -eq "True") {
            Write-Output ("BROM_TX_HEX=" + (Format-Hex $packet))
        }
        [BromAckSink]::Ack = $null
        $bromSendArgs = [object[]]::new(1)
        $bromSendArgs[0] = [byte[]]$packet
        [void]$bromSendMethod.Invoke($download, $bromSendArgs)
        $rx = Wait-BromAck -TimeoutMs $ReadTimeoutMs
        if ($null -eq $rx) {
            Write-Output "ERROR: BROM response timeout."
            exit 3
        }
        Write-Output ("BROM_RX_HEX=" + (Format-Hex $rx))
        if ($rx.Length -gt 12) {
            $payload = [byte[]]::new($rx.Length - 12)
            [Array]::Copy($rx, 12, $payload, 0, $payload.Length)
            Write-Output ("BROM_RX_PAYLOAD_HEX=" + (Format-Hex $payload))
        }
        Write-Output "BROM_SEND_OK=1"
    }
} finally {
    $ackField.SetValue($api, $originalAck)
    try { $download.DisConnectDevice() } catch { }
}
exit 0
"""


def _compact_hex(value: str) -> str:
    return re.sub(r"\s+", "", value).upper()


def hif_reboot_upgrade_frame(mode: int = HIF_REBOOT_MODE_UPGRADE) -> str:
    """Build the HIF reboot request used to enter upgrade/download mode."""
    flags = HIF_MSG_FLAG_REQ_BIT | HIF_MSG_FLAG_CHECK_BIT
    control = (
        HIF_MSG_TYPE_TO_DEVICE
        | (flags << 2)
        | (HIF_MSG_ID_REBOOT << 8)
        | (1 << 16)
    )
    header = control.to_bytes(4, "little")
    head_sum = (HIF_HEAD_MAGIC + sum(header)) & 0xFF
    checksum = (~head_sum) & 0xFF
    check32 = (~((int.from_bytes(header, "little") + mode) & 0xFFFFFFFF)) & 0xFFFFFFFF
    frame = (
        bytes((HIF_HEAD_MAGIC, checksum))
        + header
        + bytes((mode,))
        + check32.to_bytes(4, "little")
    )
    return " ".join(f"{byte:02X}" for byte in frame)


def hif_reboot_ack_frame(status: int = 0) -> str:
    """Build the host-side ACK frame expected from msghdl_host_reboot()."""
    control = (
        0x02
        | (HIF_MSG_FLAG_CHECK_BIT << 2)
        | (HIF_MSG_ID_REBOOT << 8)
        | (1 << 16)
    )
    header = control.to_bytes(4, "little")
    head_sum = (HIF_HEAD_MAGIC + sum(header)) & 0xFF
    checksum = (~head_sum) & 0xFF
    check32 = (
        ~((int.from_bytes(header, "little") + (status & 0xFF)) & 0xFFFFFFFF)
    ) & 0xFFFFFFFF
    frame = (
        bytes((HIF_HEAD_MAGIC, checksum))
        + header
        + bytes((status & 0xFF,))
        + check32.to_bytes(4, "little")
    )
    return " ".join(f"{byte:02X}" for byte in frame)


def _parse_send_rx(stdout: str) -> tuple[str, str]:
    rx_hex = ""
    rx_text = ""
    for line in stdout.splitlines():
        if line.startswith("RX_HEX="):
            rx_hex = line[len("RX_HEX="):]
        elif line.startswith("RX_TEXT="):
            rx_text = line[len("RX_TEXT="):]
    return rx_hex, rx_text


def _find_hif_reboot_ack(rx_hex: str) -> tuple[str, int, str] | None:
    """Find a valid HIF reboot ACK without depending on the seq nibble."""
    compact_rx = _compact_hex(rx_hex)
    pattern = re.compile(
        r"A5([0-9A-F]{2})([0-9A-F]{8})([0-9A-F]{2})([0-9A-F]{8})"
    )
    for match in pattern.finditer(compact_rx):
        frame = match.group(0)
        phy_checksum = int(match.group(1), 16)
        header = bytes.fromhex(match.group(2))
        status = int(match.group(3), 16)
        check32 = int.from_bytes(bytes.fromhex(match.group(4)), "little")

        expected_phy = (~((HIF_HEAD_MAGIC + sum(header)) & 0xFF)) & 0xFF
        if phy_checksum != expected_phy:
            continue

        ctrl0 = header[0]
        msg_type = ctrl0 & 0x03
        flags = (ctrl0 >> 2) & 0x3F
        msg_id = header[1]
        length = header[2] | ((header[3] & 0x0F) << 8)
        frag = (header[3] >> 7) & 0x01

        if (
            msg_type != HIF_MSG_TYPE_TO_HOST
            or msg_id != HIF_MSG_ID_REBOOT
            or length != 1
            or frag != 0
            or (flags & HIF_MSG_FLAG_CHECK_BIT) == 0
            or (flags & HIF_MSG_FLAG_REQ_BIT) != 0
        ):
            continue

        header_int = int.from_bytes(header, "little")
        if ((check32 + status + header_int) & 0xFFFFFFFF) != 0xFFFFFFFF:
            continue

        formatted = " ".join(
            frame[index:index + 2] for index in range(0, len(frame), 2)
        )
        return formatted, status, "checksum32 valid"

    return None


def _find_hif_wake_ack(rx_hex: str) -> bool:
    """Return True when the wake-up ACK magic 0x79 is present."""
    compact_rx = _compact_hex(rx_hex)
    return "79" in compact_rx


def _s16(value: int) -> int:
    return value - 0x10000 if value >= 0x8000 else value


def _hif_checksum32(data: bytes) -> int:
    """Match host-side checksum32: little-endian dword words plus trailing bytes."""
    total = 0
    offset = 0
    while offset + 4 <= len(data):
        total = (total + int.from_bytes(data[offset:offset + 4], "little")) & 0xFFFFFFFF
        offset += 4
    remaining = len(data) - offset
    if remaining:
        mask = (1 << (remaining * 8)) - 1
        total = (
            total + (int.from_bytes(data[offset:offset + 4], "little") & mask)
        ) & 0xFFFFFFFF
    return total


def _parse_msg_id_filter(value: str | None) -> int | None:
    if not value or value.strip().lower() in ("auto", "all"):
        return None
    return int(value.strip(), 0)


def _read_hif_capture(path: Path, raw: bool = False) -> bytes:
    if not path.is_file():
        raise AutoburnError(f"HIF capture file does not exist: {path}")
    if path.stat().st_size == 0:
        raise AutoburnError(
            f"HIF capture is empty: {path} has 0 bytes. Check the board power, "
            "HIF UART/baud, and monitor duration before parsing."
        )
    if raw or path.suffix.lower() in (".bin", ".dat"):
        return path.read_bytes()

    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        raise AutoburnError(f"Cannot read HIF capture {path}: {exc}")
    if not text.replace("\ufeff", "").strip():
        raise AutoburnError(
            f"HIF capture is empty: {path} has no bytes. Check the board power, "
            "HIF UART/baud, and monitor duration before parsing."
        )

    tokens: list[str] = []
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("serial monitor summary"):
            continue
        if stripped.startswith("RX_HEX="):
            stripped = stripped[len("RX_HEX="):].strip()
        timestamp = re.match(
            r"^\[[0-9]{1,2}:[0-9]{2}:[0-9]{2}(?:\.[0-9]{1,6})?\]\s*(.*)$",
            stripped,
        )
        if timestamp:
            stripped = timestamp.group(1).strip()
        if not stripped:
            continue
        if not re.fullmatch(
            r"(?:[0-9A-Fa-f]{2}\s+)*[0-9A-Fa-f]{2}\s*", stripped
        ):
            continue
        tokens.extend(re.findall(r"[0-9A-Fa-f]{2}", stripped))

    compact = re.sub(r"\s+", "", text)
    if not tokens and compact and not re.search(r"[^0-9A-Fa-f]", compact):
        try:
            return bytes.fromhex(compact)
        except ValueError:
            pass
    if not tokens:
        raise AutoburnError(
            "No hex bytes found in capture. Expected `AgentAuto monitor --hex` "
            "output, RX_HEX= lines, or a raw binary file (use --raw)."
        )
    try:
        return bytes.fromhex("".join(tokens))
    except ValueError as exc:
        raise AutoburnError(f"Capture contains invalid hex bytes: {exc}")


def _parse_hif_frames(data: bytes) -> tuple[list[dict[str, object]], int, int]:
    frames: list[dict[str, object]] = []
    invalid_candidates = 0
    partial_frames = 0
    index = 0
    while index + 5 < len(data):
        if data[index] != HIF_HEAD_MAGIC:
            index += 1
            continue

        header = data[index + 2:index + 6]
        expected_phy = (~((HIF_HEAD_MAGIC + sum(header)) & 0xFF)) & 0xFF
        if data[index + 1] != expected_phy:
            index += 1
            continue

        control = int.from_bytes(header, "little")
        msg_type = control & 0x03
        flags = (control >> 2) & 0x3F
        msg_id = (control >> 8) & 0xFF
        length = (control >> 16) & 0xFFF
        seq = (control >> 28) & 0x7
        frag = (control >> 31) & 0x1
        ext_len = 4 if flags & HIF_MSG_FLAG_EXTEND_BIT else 0
        tail_len = 4 if flags & HIF_MSG_FLAG_CHECK_BIT else 0
        total_len = 6 + ext_len + length + tail_len
        if index + total_len > len(data):
            partial_frames += 1
            index += 1
            continue

        body_start = index + 6 + ext_len
        body = data[body_start:body_start + length]
        tail = data[body_start + length:body_start + length + tail_len]
        check32 = None
        check32_ok = None
        check32_note = ""
        if tail_len:
            check32 = int.from_bytes(tail, "little")
            if flags & HIF_MSG_FLAG_MAC32_BIT:
                check32_note = "MAC32"
            else:
                checksum_input = header + data[index + 6:body_start] + body
                check32_ok = (
                    (check32 + _hif_checksum32(checksum_input)) & 0xFFFFFFFF
                ) == 0xFFFFFFFF
                if not check32_ok:
                    invalid_candidates += 1
                    index += 1
                    continue

        frames.append(
            {
                "index": index,
                "raw_hex": " ".join(f"{byte:02X}" for byte in data[index:index + total_len]),
                "header": header,
                "type": msg_type,
                "flags": flags,
                "msg_id": msg_id,
                "length": length,
                "seq": seq,
                "frag": frag,
                "body": body,
                "check8": "OK" if data[index + 1] == expected_phy else "FAIL",
                "check32": check32,
                "check32_ok": check32_ok,
                "check32_note": check32_note,
            }
        )
        index += total_len
    return frames, invalid_candidates, partial_frames


def _decode_datacube_fragment(
    body: bytes,
    msg_id: int = HIF_MSG_ID_FFT_DATA,
) -> dict[str, object]:
    """Decode one MMW datacube HIF fragment.

    0xC2 is the radar_framework FFT datacube used by ReportDataCube2D. Its
    first fragment has MMW_FRAME_UPLOAD + MMW_FRAME_TL; later fragments have
    MMW_FRAME_UPLOAD + IQ payload only, and frame_len/offset are byte counts.

    0xC1 is the legacy mmw_cmd datacube. It has no MMW_FRAME_TL and its
    frame_len/offset are FFT DWORD counts.
    """
    result: dict[str, object] = {
        "valid": False,
        "reason": "",
    }
    if len(body) < FRAME_UPLOAD_LEN:
        result["reason"] = "datacube payload is shorter than MMW_FRAME_UPLOAD"
        return result

    frame_idx = int.from_bytes(body[0:4], "little")
    raw_frame_len = int.from_bytes(body[4:8], "little")
    raw_data_offset = int.from_bytes(body[8:12], "little")
    byte_units = msg_id == HIF_MSG_ID_FFT_DATA
    frame_len = raw_frame_len if byte_units else raw_frame_len * 4
    data_offset = raw_data_offset if byte_units else raw_data_offset * 4
    is_first = data_offset == 0
    result.update(
        {
            "msg_id": msg_id,
            "byte_units": byte_units,
            "frame_idx": frame_idx,
            "frame_len": frame_len,
            "frame_len_raw": raw_frame_len,
            "data_offset": data_offset,
            "data_offset_raw": raw_data_offset,
        }
    )

    fft_type = None
    tl_total_length = None
    tx_num = None
    rx_num = None
    range_num = None
    dop_num = None
    data_start = FRAME_UPLOAD_LEN
    placement = data_offset

    if byte_units and is_first:
        if len(body) < REPORT_HEAD_LEN:
            result["reason"] = (
                "first 0xC2 FFT fragment is missing the 12-byte MMW_FRAME_TL"
            )
            return result
        tl_word = int.from_bytes(body[FRAME_UPLOAD_LEN:REPORT_HEAD_LEN], "little")
        fft_type = tl_word & 0xFF
        tl_total_length = (tl_word >> 8) & 0xFFFFFF
        tx_num = body[16]
        rx_num = body[17]
        range_num = int.from_bytes(body[20:22], "little")
        dop_num = int.from_bytes(body[22:24], "little")
        data_start = REPORT_HEAD_LEN
        placement = REPORT_HEAD_LEN

    fft_data = body[data_start:]
    bin_count = len(fft_data) // 4
    bins: list[dict[str, int]] = []
    for pos in range(0, len(fft_data) - 3, 4):
        dword = int.from_bytes(fft_data[pos:pos + 4], "little")
        if byte_units:
            bin_offset = (placement + pos - REPORT_HEAD_LEN) // 4
        else:
            bin_offset = raw_data_offset + (pos // 4)
        bins.append(
            {
                "bin_offset": bin_offset,
                "offset_bytes": placement + pos,
                "imag_u16": dword & 0xFFFF,
                "real_u16": (dword >> 16) & 0xFFFF,
                "imag_s16": _s16(dword & 0xFFFF),
                "real_s16": _s16((dword >> 16) & 0xFFFF),
            }
        )

    cumulative_incl_tl = data_offset + len(fft_data)
    if byte_units and is_first:
        cumulative_incl_tl += TL_LEN
    complete = frame_len > 0 and cumulative_incl_tl == frame_len

    result.update(
        {
            "format": "0xC2-radar-framework" if byte_units else "0xC1-legacy-mmw",
            "fft_type": fft_type,
            "tl_total_length": tl_total_length,
            "tx_num": tx_num,
            "rx_num": rx_num,
            "range_num": range_num,
            "dop_num": dop_num,
            "data_start": data_start,
            "fft_data_bytes": len(fft_data),
            "bin_count": bin_count,
            "bins": bins,
            "cumulative_incl_tl": cumulative_incl_tl,
            "complete": complete,
            "valid": True,
        }
    )
    if byte_units and is_first:
        result["expected_tl_total_length"] = (
            (range_num * dop_num * tx_num * rx_num * 4 + 8)
            if all(v is not None for v in (range_num, dop_num, tx_num, rx_num))
            else None
        )
    return result


def _decode_c2_fragment(body: bytes) -> dict[str, object]:
    """Backwards-compatible wrapper for the radar_framework 0xC2 decoder."""
    return _decode_datacube_fragment(body, HIF_MSG_ID_FFT_DATA)


HIF_MSG_ID_NAMES = {
    0x00: "VERSION",
    0x01: "CHIPINFO",
    0x02: "HIF_PHY",
    0x04: "REBOOT",
    0x0E: "DOWNLOAD_CTRL",
    0x0F: "DOWNLOAD_DATA",
    0xC0: "ADC_DATA",
    0xC1: "CUBE_DATA_LEGACY",
    0xC2: "FFT_DATA",
    0xC3: "OBJECTS",
    0xC5: "HPS_REPT",
    0xC6: "MOTION_DATA",
    0xF0: "DBG_PRINT",
    0xF4: "SYSTEM_INFO",
    0xFF: "STARTUP",
}


def _format_msg_id(value: int) -> str:
    name = HIF_MSG_ID_NAMES.get(value)
    if name:
        return f"0x{value:02X} ({name})"
    return f"0x{value:02X}"


def _print_parse_frame(
    frame: dict[str, object],
    number: int,
    args: argparse.Namespace,
) -> None:
    body = frame["body"]
    print(f"FRAME={number}")
    print(f"FRAME_BYTE={frame['index']}")
    print(f"MSG_ID={_format_msg_id(int(frame['msg_id']))}")
    print(f"RAW={frame['raw_hex']}")
    print(f"TYPE=0x{int(frame['type']):X}")
    print(f"FLAGS=0x{int(frame['flags']):02X}")
    print(f"LENGTH={frame['length']}")
    print(f"SEQ={frame['seq']}")
    print(f"FRAG={frame['frag']}")
    print(f"CHECK8={frame['check8']}")
    if frame["check32"] is not None:
        note = frame["check32_note"]
        status = "OK" if frame["check32_ok"] else "FAIL"
        if note:
            status = note
        print(f"CHECKSUM32={status}")

    msg_id = int(frame["msg_id"])
    if isinstance(body, bytes) and msg_id in HIF_DATACUBE_MSG_IDS:
        decoded = _decode_datacube_fragment(body, msg_id)
        prefix = "C2" if msg_id == HIF_MSG_ID_FFT_DATA else "C1"
        if decoded.get("valid"):
            print(f"{prefix}_FRAME_IDX={decoded['frame_idx']}")
            print(f"{prefix}_FRAME_LEN={decoded['frame_len']}")
            print(f"{prefix}_DATA_OFFSET={decoded['data_offset']}")
            if decoded.get("fft_type") is not None:
                print(f"{prefix}_TL_TYPE={decoded['fft_type']}")
            if decoded.get("tl_total_length") is not None:
                print(f"{prefix}_TL_TOTAL_LENGTH={decoded['tl_total_length']}")
                if decoded.get("expected_tl_total_length") is not None:
                    print(
                        f"{prefix}_TL_EXPECTED_LENGTH="
                        f"{decoded['expected_tl_total_length']}"
                    )
            if decoded.get("tx_num") is not None:
                print(f"{prefix}_TX_NUM={decoded['tx_num']}")
                print(f"{prefix}_RX_NUM={decoded['rx_num']}")
                print(f"{prefix}_RANGE_BINS={decoded['range_num']}")
                print(f"{prefix}_DOP_BINS={decoded['dop_num']}")
            print(f"{prefix}_BIN_COUNT={decoded['bin_count']}")
            print(f"{prefix}_FFT_DATA_BYTES={decoded['fft_data_bytes']}")
            print(f"{prefix}_CUMULATIVE_INCL_TL={decoded['cumulative_incl_tl']}")
            print(f"{prefix}_COMPLETE={'YES' if decoded['complete'] else 'NO'}")
            if args.bins:
                for item in decoded.get("bins", []):
                    print(
                        f"{prefix}_BIN={decoded['frame_idx']}|"
                        f"{item['bin_offset']}|"
                        f"{item['imag_s16']}|{item['real_s16']}"
                    )
        else:
            print(f"{prefix}_DECODE_ERROR={decoded.get('reason', 'unknown')}")
    elif isinstance(body, bytes) and len(body) <= 64:
        print(f"PAYLOAD={' '.join(f'{byte:02X}' for byte in body)}")
    elif isinstance(body, bytes):
        print(f"PAYLOAD_BYTES={len(body)}")


def cmd_parse_hif(args: argparse.Namespace) -> int:
    path = Path(args.input).expanduser().resolve()
    data = _read_hif_capture(path, raw=bool(args.raw))
    frames, invalid, partial = _parse_hif_frames(data)
    msg_filter = _parse_msg_id_filter(args.msg_id)
    selected = [
        frame for frame in frames
        if msg_filter is None or int(frame["msg_id"]) == msg_filter
    ]

    datacube_entries: list[dict[str, object]] = []
    datacube_complete = 0
    datacube_bin_count = 0
    for frame in selected:
        body = frame["body"]
        if not isinstance(body, bytes) or int(frame["msg_id"]) not in HIF_DATACUBE_MSG_IDS:
            continue
        decoded = _decode_datacube_fragment(body, int(frame["msg_id"]))
        if not decoded.get("valid"):
            continue
        datacube_entries.append(decoded)
        datacube_complete += 1 if decoded.get("complete") else 0
        datacube_bin_count += int(decoded.get("bin_count") or 0)

    if args.json:
        payload: list[dict[str, object]] = []
        for frame in selected:
            item = dict(frame)
            if isinstance(item.get("body"), bytes):
                item["body_hex"] = " ".join(
                    f"{byte:02X}" for byte in item["body"]
                )
            item.pop("body", None)
            if isinstance(item.get("header"), bytes):
                item["header_hex"] = " ".join(
                    f"{byte:02X}" for byte in item["header"]
                )
            item.pop("header", None)
            if int(frame["msg_id"]) in HIF_DATACUBE_MSG_IDS:
                decoded = _decode_datacube_fragment(
                    bytes.fromhex(item.get("body_hex", "").replace(" ", "")),
                    int(frame["msg_id"]),
                )
                if decoded.get("valid") and not args.bins:
                    decoded.pop("bins", None)
                item["datacube"] = decoded
            payload.append(item)
        print(json.dumps(payload, ensure_ascii=False, indent=2))
    else:
        for number, frame in enumerate(selected, start=1):
            _print_parse_frame(frame, number, args)

    unique_frames = len({entry.get("frame_idx") for entry in datacube_entries})
    if not args.json:
        print(f"PARSE_OK=1")
        print(f"INPUT={path}")
        print(f"BYTES_READ={len(data)}")
        print(f"VALID_FRAMES={len(frames)}")
        print(f"SELECTED_FRAMES={len(selected)}")
        print(f"INVALID_CANDIDATES={invalid}")
        print(f"PARTIAL_FRAMES={partial}")
        print(f"DATACUBE_MESSAGES={len(datacube_entries)}")
        print(f"DATACUBE_COMPLETE_FRAMES={datacube_complete}")
        print(f"DATACUBE_UNIQUE_FRAME_IDX={unique_frames}")
        print(f"DATACUBE_BINS={datacube_bin_count}")
    if not frames:
        return fail("No valid HIF frames found in capture.", 1)
    if not selected:
        return fail(
            f"No frames matched msg_id filter {args.msg_id or 'auto'}.",
            1,
        )
    return 0


def _verify_enter_evidence(
    mode: str,
    rx_hex: str,
    rx_text: str,
    shell_command: str,
) -> tuple[bool, str, str]:
    shell_negative = [
        "command not found",
        "unknown command",
        "no such command",
        "sub command not found",
        "command failed",
        "param error",
        "parameter error",
        "invalid param",
        "command is too long",
    ]
    if mode == "shell":
        marker = (shell_command or "upgrade").strip().lower()
        text = rx_text.lower()
        negative = next((item for item in shell_negative if item in text), None)
        if negative:
            evidence = (
                f"shell rejected command (found '{negative}'); "
                f"RX_TEXT={rx_text[:128]}"
            )
            return False, "shell", evidence

        typed_echo = bool(marker) and marker in text
        command_success = any(
            phrase in text for phrase in ("command success", "return:")
        )
        if typed_echo or command_success:
            if typed_echo and command_success:
                detail = f"echo '{marker}' + Command Success"
            elif typed_echo:
                detail = f"echo '{marker}'"
            else:
                detail = "Command Success"
            evidence = f"{detail} (RX_TEXT={rx_text[:128]})"
            return True, "shell", evidence

        evidence = (
            f"need typed '{marker}' echo or Command Success; "
            f"RX_TEXT={rx_text[:128]}"
        )
        return False, "shell", evidence

    if mode == "hif":
        found = _find_hif_reboot_ack(rx_hex)
        if found is None:
            evidence = (
                "HIF reboot ACK not found; expected "
                f"{HIF_UPGRADE_ACK_HEX} with status 0x00 "
                f"(RX_HEX={rx_hex[:128]})"
            )
            return False, "hif", evidence
        frame, status, check_detail = found
        evidence = (
            f"HIF reboot ACK status=0x{status:02X}, {check_detail} "
            f"(frame={frame})"
        )
        return status == 0, "hif", evidence

    if mode == "wake":
        if _find_hif_wake_ack(rx_hex):
            evidence = f"HIF wake ACK found (RX_HEX={rx_hex[:128]})"
            return True, "wake", evidence
        evidence = (
            "HIF wake ACK not found; expected "
            f"{HIF_WAKE_ACK_HEX} after sending {HIF_WAKE_MAGIC_HEX} "
            f"(RX_HEX={rx_hex[:128]})"
        )
        return False, "wake", evidence

    return False, "unknown", "no enter verification mode"


def send_serial_command(
    port: str,
    baud_rate: int,
    mode: str,
    shell_command: str,
    hex_data: str,
    line_ending: str,
    read_timeout_ms: int = 1500,
    dry_run: bool = False,
    verify_mode: str = "none",
) -> int:
    line_end = DEFAULT_LINE_ENDINGS.get(line_ending)
    if line_end is None:
        return fail(f"Unknown line ending: {line_ending}")

    if verify_mode == "auto":
        verify_mode = mode if mode in ("shell", "hif") else "none"

    if mode == "shell":
        payload = shell_command.encode("utf-8") + line_end
        description = f"shell command '{shell_command}'"
    elif mode == "hif":
        try:
            payload = bytes.fromhex(re.sub(r"\s+", "", hex_data))
        except ValueError:
            return fail(f"Invalid hex data: {hex_data}")
        description = f"HIF bytes [{hex_data}]"
    else:
        return 0

    hex_payload = " ".join(f"{byte:02X}" for byte in payload)
    if dry_run:
        print(f"DRY_RUN: Would send {description} on {port} at {baud_rate} baud")
        print(f"DRY_RUN: Payload hex: {hex_payload}")
        if verify_mode in ("shell", "hif", "wake"):
            if verify_mode == "hif":
                expected = f"HIF ACK {HIF_UPGRADE_ACK_HEX}"
            elif verify_mode == "wake":
                expected = f"HIF wake ACK {HIF_WAKE_ACK_HEX}"
            else:
                expected = f"shell echo/prompt for '{shell_command}'"
            print(f"DRY_RUN: Enter verify will expect {expected}")
        return 0

    print(f"SERIAL_SEND: {description} on {port} at {baud_rate} baud")
    proc = run_ps_capture(
        PS_SEND,
        {
            "ComPort": port,
            "BaudRate": baud_rate,
            "HexData": hex_payload,
            "ReadTimeoutMs": read_timeout_ms,
        },
    )
    if proc.returncode != 0:
        message = (
            proc.stdout.strip()
            or proc.stderr.strip()
            or "Serial command failed."
        )
        if message.upper().startswith("ERROR:"):
            message = message.split(":", 1)[1].strip()
        return fail(message, proc.returncode or 3)
    rx_hex, rx_text = _parse_send_rx(proc.stdout or "")
    if proc.stdout:
        print(proc.stdout, end="")
    if proc.stderr:
        print(proc.stderr, end="", file=sys.stderr)
    if verify_mode in ("shell", "hif", "wake"):
        verified, source, evidence = _verify_enter_evidence(
            verify_mode,
            rx_hex,
            rx_text,
            shell_command,
        )
        if not verified:
            return fail(
                f"Enter verification failed for {verify_mode} mode: {evidence}",
                3,
            )
        print("ENTER_VERIFY=OK")
        print(f"ENTER_SOURCE={source}")
        print(f"ENTER_EVIDENCE={evidence}")
    return 0


def send_hif_enter_handshake(
    port: str,
    baud_rate: int,
    enter_hex: str,
    read_timeout_ms: int = 4000,
    dry_run: bool = False,
    verify_mode: str = "auto",
) -> int:
    """Send the HIF wake magic first; send reboot only after 0x79 ACK."""
    wake_result = send_serial_command(
        port,
        baud_rate,
        "hif",
        "upgrade",
        HIF_WAKE_MAGIC_HEX,
        "NONE",
        read_timeout_ms=read_timeout_ms,
        dry_run=dry_run,
        verify_mode="wake",
    )
    if wake_result != 0:
        return wake_result
    return send_serial_command(
        port,
        baud_rate,
        "hif",
        "upgrade",
        enter_hex,
        "NONE",
        read_timeout_ms=read_timeout_ms,
        dry_run=dry_run,
        verify_mode=verify_mode,
    )


def force_kill_com_port_users(port: str) -> int:
    script = r"""
$port = '__PORT__'
$startPid = [int]'__PID__'
$parentPid = [int]'__PARENT_PID__'
$names = 'python|powershell|cmd|putty|termite|sscom|mobaxterm|xshell|securecrt|serial|tty'
$portPattern = [regex]::Escape($port)
$ancestors = @{}
$current = Get-CimInstance Win32_Process -Filter "ProcessId=$startPid" -ErrorAction SilentlyContinue
while ($null -ne $current) {
    $ancestors[[int]$current.ProcessId] = $true
    $parentId = [int]$current.ParentProcessId
    if ($parentId -le 0 -or $parentId -eq [int]$current.ProcessId) { break }
    $parent = Get-CimInstance Win32_Process -Filter "ProcessId=$parentId" -ErrorAction SilentlyContinue
    if ($null -eq $parent) { break }
    $current = $parent
}
$result = @()
$candidates = @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessId -ne $PID -and
    $_.ProcessId -ne $startPid -and
    $_.ProcessId -ne $parentPid -and
    -not $ancestors.ContainsKey([int]$_.ProcessId) -and
    $_.Name -match $names -and
    ([string]$_.CommandLine) -match $portPattern
})
foreach ($candidate in $candidates) {
    try {
        Stop-Process -Id $candidate.ProcessId -Force -ErrorAction Stop
        $result += [pscustomobject]@{ Pid=[int]$candidate.ProcessId; Name=[string]$candidate.Name; Status='KILLED'; Error='' }
    } catch {
        $result += [pscustomobject]@{ Pid=[int]$candidate.ProcessId; Name=[string]$candidate.Name; Status='KILL_FAIL'; Error=$_.Exception.Message }
    }
}
Write-Output ($result | ConvertTo-Json -Compress)
""".replace("__PORT__", re.escape(port)).replace("__PID__", str(os.getpid())).replace(
        "__PARENT_PID__", str(os.getppid())
    )

    try:
        proc = subprocess.run(
            [
                "powershell.exe",
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-Command",
                script,
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=60,
            check=False,
        )
    except FileNotFoundError:
        return fail("PowerShell was not found on PATH; cannot force-kill COM users.", 2)
    except subprocess.TimeoutExpired:
        return fail("COM cleanup timed out.", 3)
    if proc.returncode != 0:
        return fail(
            f"COM cleanup failed: stdout={proc.stdout.strip()} stderr={proc.stderr.strip()}",
            2,
        )
    raw = proc.stdout.strip()
    if not raw:
        return 0
    try:
        items = json.loads(raw)
    except json.JSONDecodeError:
        return fail("Could not parse COM cleanup result.", 2)
    if isinstance(items, dict):
        items = [items]
    for item in items:
        if item.get("Status") == "KILLED":
            print(f"FORCE_KILL: Killed {item.get('Pid')} ({item.get('Name')}) for {port}.")
        else:
            return fail(
                f"Could not kill process {item.get('Pid')} ({item.get('Name')}): {item.get('Error')}",
                3,
            )
    return 0


def _staging_dir(copy_dir: str | None, port: str) -> tuple[Path, bool]:
    if copy_dir:
        path = Path(copy_dir).expanduser().resolve()
        if is_network_path(path):
            raise AutoburnError(
                "--copy-dir must be local. DownloadLib.dll cannot be loaded "
                "from a UNC/network path."
            )
        path.mkdir(parents=True, exist_ok=True)
        return path, False

    local_dir = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local"))
    autoburn_dir = local_dir / "AgentAuto" / "staged"
    autoburn_dir.mkdir(parents=True, exist_ok=True)
    path = Path(
        tempfile.mkdtemp(
            prefix=f"stage_{re.sub(r'[^A-Za-z0-9]', '_', port)}_",
            dir=autoburn_dir,
        )
    )
    return path, True


def _apply_exclude_args(
    sdk_root: Path,
    project_dir: Path,
    cdk_dir: Path,
    args: argparse.Namespace,
) -> tuple[set[str], list[str], bool]:
    paths, globs = _read_exclude_rules(cdk_dir)
    changed = False
    for raw in getattr(args, "exclude", None) or []:
        norm = _normalize_user_source_ref(sdk_root, project_dir, cdk_dir, raw)
        if norm and norm not in paths:
            paths.add(norm)
            changed = True
    for raw in getattr(args, "exclude_glob", None) or []:
        norm = _normalize_user_source_ref(
            sdk_root, project_dir, cdk_dir, raw, glob=True
        )
        if norm and norm not in globs:
            globs.append(norm)
            changed = True
    for raw in getattr(args, "unexclude", None) or []:
        norm = _normalize_user_source_ref(sdk_root, project_dir, cdk_dir, raw)
        if norm and norm in paths:
            paths.discard(norm)
            changed = True
    if changed:
        _write_exclude_rules(cdk_dir, paths, globs, dry_run=args.dry_run)
    return paths, globs, changed


def cmd_prepare(args: argparse.Namespace) -> int:
    sdk_root = find_sdk_root(args.sdk_root)
    project_dir = find_project(sdk_root, args.project)
    cdk_dir = resolve_project_cdk_dir(
        project_dir, getattr(args, "config", None)
    )
    print(f"CDK_DIR={cdk_dir}")
    board_value = getattr(args, "board", None) or ""
    if board_value:
        if args.skip_cdkproj:
            return fail(
                "--board configures project.cdkproj; remove --skip-cdkproj.",
                1,
            )
        board_macro = _resolve_board_macro(sdk_root, board_value)
        print(f"BOARD_MACRO={board_macro}")
        cdkproj = find_cdk_project_file(cdk_dir)
        board_actions = _set_cdkproj_board_defines(
            cdkproj, board_macro, args.dry_run
        )
        for action in board_actions:
            print(action)
        if not board_actions:
            print(f"BOARD_CDKPROJ_OK: {board_macro} already selected")

        header = find_project_header(project_dir)
        if header is not None:
            header_actions = _neutralize_header_board_macros(
                header,
                _read_board_config_macros(sdk_root),
                args.dry_run,
            )
            for action in header_actions:
                print(action)

    excluded_paths, exclude_globs, _ = _apply_exclude_args(
        sdk_root, project_dir, cdk_dir, args
    )
    modules: set[str] = set()
    if not getattr(args, "skip_modules", False):
        modules = _detect_mmw_hif_modules(project_dir)
        if modules:
            print("MODULE_ENABLE: " + ", ".join(sorted(modules)))
        else:
            print("MODULE_OK: no MMW/HIF usage detected in project sources")
    else:
        print("MODULE_OK: auto module enable skipped by --skip-modules")

    if not args.skip_shell:
        header = find_project_header(project_dir)
        if header is None:
            print(
                "WARN: No prj_config.h found under the project; shell auto-enable skipped."
            )
        else:
            actions = _set_shell_macros(header, args.dry_run)
            if actions:
                for action in actions:
                    print(action)
            else:
                print(f"SHELL_OK: shell macros already enabled ({header.name})")

    if modules:
        header = find_project_header(project_dir)
        if header is None:
            print(
                "WARN: No prj_config.h found under the project; MMW/HIF macros skipped."
            )
        else:
            actions = _set_module_macros(header, modules, args.dry_run)
            if actions:
                for action in actions:
                    print(action)
            else:
                print("MODULE_OK: MMW/HIF macros already enabled")

    if not args.skip_cdkproj:
        result = refresh_cdkproj(
            sdk_root,
            project_dir,
            cdk_dir,
            dry_run=args.dry_run,
            excluded_paths=excluded_paths,
            exclude_globs=exclude_globs,
            modules=modules,
        )
        if result["removed"]:
            print(
                "CDKPROJ_REMOVED: "
                + ", ".join(result["removed"][:20])
                + ("..." if len(result["removed"]) > 20 else "")
            )
        if result["added"]:
            print(
                "CDKPROJ_ADDED: "
                + ", ".join(result["added"][:20])
                + ("..." if len(result["added"]) > 20 else "")
            )
        if result.get("restored"):
            print("CDKPROJ_RESTORED: " + ", ".join(result["restored"]))
        if result.get("module_dirs"):
            print("CDKPROJ_MODULE_DIRS: " + ", ".join(result["module_dirs"]))
        if result.get("include_paths_changed"):
            print("CDKPROJ_INCLUDE_PATH: added MMW/HIF include paths")
        if result.get("linker_changed"):
            print("CDKPROJ_LINKER: added dsp/point_cloud libraries and mmw_ctrl.sym")
        if (
            not result["removed"]
            and not result["added"]
            and not result.get("restored")
            and not result.get("module_dirs")
            and not result.get("include_paths_changed")
            and not result.get("linker_changed")
        ):
            print("CDKPROJ_OK: source list is current")
        if result["changed"]:
            print(
                "CDKPROJ_UPDATED: "
                + ("dry-run preview" if args.dry_run else "wrote project.cdkproj")
            )
    return 0


def cmd_refresh_sources(args: argparse.Namespace) -> int:
    sdk_root = find_sdk_root(args.sdk_root)
    project_dir = find_project(sdk_root, args.project)
    cdk_dir = resolve_project_cdk_dir(
        project_dir, getattr(args, "config", None)
    )
    print(f"CDK_DIR={cdk_dir}")
    excluded_paths, exclude_globs, _ = _apply_exclude_args(
        sdk_root, project_dir, cdk_dir, args
    )
    result = refresh_cdkproj(
        sdk_root,
        project_dir,
        cdk_dir,
        dry_run=args.dry_run,
        excluded_paths=excluded_paths,
        exclude_globs=exclude_globs,
        modules=(
            set()
            if getattr(args, "skip_modules", False)
            else _detect_mmw_hif_modules(project_dir)
        ),
    )
    print(f"CDKPROJ_REMOVED={result['removed']}")
    print(f"CDKPROJ_ADDED={result['added']}")
    print(f"CDKPROJ_RESTORED={result.get('restored', [])}")
    print(f"CDKPROJ_MODULES={result.get('modules', [])}")
    print(f"CDKPROJ_MODULE_DIRS={result.get('module_dirs', [])}")
    print(f"CDKPROJ_INCLUDE_PATH_CHANGED={int(result.get('include_paths_changed', False))}")
    print(f"CDKPROJ_LINKER_CHANGED={int(result.get('linker_changed', False))}")
    print(f"CDKPROJ_CHANGED={int(result['changed'])}")
    if result["changed"] and not args.dry_run:
        print("CDKPROJ_UPDATED: wrote project.cdkproj")
    return 0


def cmd_exclude(args: argparse.Namespace) -> int:
    sdk_root = find_sdk_root(args.sdk_root)
    project_dir = find_project(sdk_root, args.project)
    cdk_dir = resolve_project_cdk_dir(
        project_dir, getattr(args, "config", None)
    )
    if getattr(args, "source", None):
        args.exclude = getattr(args, "source", None)
    paths, globs, changed = _apply_exclude_args(
        sdk_root, project_dir, cdk_dir, args
    )
    result = refresh_cdkproj(
        sdk_root,
        project_dir,
        cdk_dir,
        dry_run=args.dry_run,
        excluded_paths=paths,
        exclude_globs=globs,
    )
    if not args.dry_run and changed:
        print(f"EXCLUDE_OK: wrote {cdk_dir / CDK_EXCLUDE_NAME}")
    print(f"EXCLUDED={len(paths)}")
    for path in sorted(paths):
        print(f"  {path}")
    if result["removed"]:
        print("CDKPROJ_REMOVED_FROM_BUILD=" + ", ".join(result["removed"]))
    return 0


def cmd_unexclude(args: argparse.Namespace) -> int:
    sdk_root = find_sdk_root(args.sdk_root)
    project_dir = find_project(sdk_root, args.project)
    cdk_dir = resolve_project_cdk_dir(
        project_dir, getattr(args, "config", None)
    )
    if getattr(args, "source", None):
        args.unexclude = getattr(args, "source", None)
    paths, globs, changed = _apply_exclude_args(
        sdk_root, project_dir, cdk_dir, args
    )
    result = refresh_cdkproj(
        sdk_root,
        project_dir,
        cdk_dir,
        dry_run=args.dry_run,
        excluded_paths=paths,
        exclude_globs=globs,
    )
    if not args.dry_run and changed:
        print(f"UNEXCLUDE_OK: updated {cdk_dir / CDK_EXCLUDE_NAME}")
    print(f"EXCLUDED={len(paths)}")
    if result["added"]:
        print("CDKPROJ_READDED=" + ", ".join(result["added"]))
    return 0


def cmd_excluded(args: argparse.Namespace) -> int:
    sdk_root = find_sdk_root(args.sdk_root)
    project_dir = find_project(sdk_root, args.project)
    cdk_dir = resolve_project_cdk_dir(
        project_dir, getattr(args, "config", None)
    )
    print(f"CDK_DIR={cdk_dir}")
    paths, globs = _read_exclude_rules(cdk_dir)
    for pattern in globs:
        print(f"GLOB={pattern}")
    for path in sorted(paths):
        print(path)
    print(f"EXCLUDED_COUNT={len(paths)}")
    return 0


def cmd_discover(args: argparse.Namespace) -> int:
    sdk_root = find_sdk_root(args.sdk_root, required=False)
    cdk_make = find_cdk_make(args.cdk_make)
    dll = find_download_dll(args.dll_path, sdk_root, required=False)
    python_exe = sys.executable
    project_dirs = find_project_dirs(sdk_root) if sdk_root else []
    projects = [p.name for p in project_dirs]
    project_paths = [str(p) for p in project_dirs]
    try:
        ports = list_ports()
    except AutoburnError:
        ports = []

    info = {
        "sdk_root": str(sdk_root) if sdk_root else None,
        "cdk_make": str(cdk_make) if cdk_make else None,
        "download_dll": str(dll) if dll else None,
        "python": python_exe,
        "projects": projects,
        "project_paths": project_paths,
        "ports": ports,
    }
    if args.json:
        print(json.dumps(info, ensure_ascii=False, indent=2))
        return 0
    for key, value in info.items():
        if isinstance(value, list):
            print(f"{key.upper()}={','.join(value) if value else 'none'}")
        else:
            print(f"{key.upper()}={value}")
    return 0


def cmd_ports(args: argparse.Namespace) -> int:
    try:
        ports = list_ports()
    except AutoburnError as exc:
        return fail(str(exc), 2)
    if args.json:
        print(json.dumps(ports, ensure_ascii=False))
    else:
        print("\n".join(ports) if ports else "No COM ports found.")
    return 0


def _run_cdk_make(
    cdk_make: Path, cdkws: Path, cdk_dir: Path, config: str
) -> tuple[int, str]:
    cdkproj = find_cdk_project_file(cdk_dir)
    project_name = cdk_dir.name
    try:
        tree = ElementTree.parse(cdkproj)
        project_name = tree.getroot().attrib.get("Name") or project_name
    except (ElementTree.ParseError, OSError):
        pass
    cmd = [
        str(cdk_make),
        "/w",
        str(cdkws),
        "/p",
        project_name,
        "/c",
        config,
        "/d",
        "build",
        "/v",
    ]
    print(f"BUILD_PROJECT={cdkproj}")
    print("BUILD_CMD: " + subprocess.list2cmdline(cmd))
    try:
        proc = subprocess.run(
            cmd,
            cwd=str(cdk_dir),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
    except OSError as exc:
        return 2, f"Failed to run cdk-make: {exc}"
    if proc.stdout:
        sys.stdout.write(proc.stdout)
    if proc.stderr:
        sys.stderr.write(proc.stderr)
    combined = proc.stdout + "\n" + proc.stderr
    return proc.returncode, combined


def _parse_build_error_sources(
    output: str, sdk_root: Path, project_dir: Path, cdk_dir: Path
) -> list[str]:
    errors: set[str] = set()
    patterns = (
        r"^\s*([A-Za-z]:[\\/][^:\r\n]+?\.c):\d+(?::\d+)?:\s+(?:fatal\s+)?error:",
        r"^\s*((?:\.\./)+[^:\r\n]+?\.c):\d+(?::\d+)?:\s+(?:fatal\s+)?error:",
        r"^\s*([^:\r\n]+?\.c):\d+(?::\d+)?:\s+(?:fatal\s+)?error:",
    )

    def normalize(raw: str) -> str | None:
        path = Path(raw)
        if path.is_absolute():
            if path.suffix.lower() == ".c" and path.is_file():
                return _cdk_relative_name(path, cdk_dir)
            return None
        candidate = resolve_source_path(cdk_dir, raw).resolve()
        if candidate.suffix.lower() == ".c" and candidate.is_file():
            return _cdk_relative_name(candidate, cdk_dir)
        for base in (sdk_root, project_dir):
            candidate = base.joinpath(*_as_posix(raw).split("/")).resolve()
            if candidate.suffix.lower() == ".c" and candidate.is_file():
                return _cdk_relative_name(candidate, cdk_dir)
        return None

    for pattern in patterns:
        for match in re.finditer(pattern, output):
            norm = normalize(match.group(1))
            if norm:
                errors.add(norm)
    return sorted(errors)


def cmd_build(args: argparse.Namespace) -> int:
    sdk_root = find_sdk_root(args.sdk_root)
    project_dir = find_project(sdk_root, args.project)
    cdk_make = find_cdk_make(args.cdk_make)
    if cdk_make is None:
        return fail(
            "cdk-make.exe not found. Set AGENT_AUTO_CDK/CDK_MAKE, add CDK to "
            "PATH, or pass --cdk.",
            2,
        )

    config_requested = args.config
    cdk_dir = resolve_project_cdk_dir(project_dir, config_requested)
    print(f"CDK_DIR={cdk_dir}")
    cdkws = find_cdk_workspace(cdk_dir)
    cdkproj = find_cdk_project_file(cdk_dir)
    configs = discover_project_configs(cdkproj)
    config_is_dir = bool(
        config_requested
        and (
            Path(config_requested).expanduser().is_dir()
            or (project_dir / config_requested.replace("\\", "/").split("/")[-1]).is_dir()
        )
    )
    config = (
        (None if config_is_dir else config_requested)
        or (configs[0] if configs else "psdf")
    )
    print(f"BUILD_CONFIG={config}")

    if not args.skip_prepare:
        rc = cmd_prepare(args)
        if rc != 0:
            return rc

    if args.dry_run:
        print("DRY_RUN: Validated CDK command; no build was started.")
        return 0

    excluded_paths, exclude_globs = _read_exclude_rules(cdk_dir)
    attempts = 0
    while True:
        returncode, output = _run_cdk_make(cdk_make, cdkws, cdk_dir, config)
        if returncode == 0:
            break
        if not args.exclude_on_error:
            return returncode or 1
        error_sources = _parse_build_error_sources(
            output, sdk_root, project_dir, cdk_dir
        )
        new_errors = [path for path in error_sources if path not in excluded_paths]
        if not new_errors or attempts >= 2:
            if error_sources:
                print(
                    "BUILD_ERROR_SOURCES=" + ", ".join(error_sources),
                    file=sys.stderr,
                )
            return fail(
                "Build failed and no additional source file could be excluded.",
                returncode or 1,
            )
        excluded_paths.update(new_errors)
        _write_exclude_rules(cdk_dir, excluded_paths, exclude_globs)
        refresh_cdkproj(
            sdk_root,
            project_dir,
            cdk_dir,
            excluded_paths=excluded_paths,
            exclude_globs=exclude_globs,
        )
        print("AUTO_EXCLUDE: " + ", ".join(new_errors))
        attempts += 1

    image = find_firmware_image(
        project_dir, None, required=False, cdk_dir=cdk_dir
    )
    if image is None:
        return fail(
            f"Build finished but no .img was produced under {cdk_dir / 'output'}.",
            1,
        )
    print(f"BUILD_OK=1")
    print(f"IMAGE={image}")
    return 0


def cmd_burn(args: argparse.Namespace) -> int:
    sdk_root = find_sdk_root(args.sdk_root)
    project_dir = find_project(sdk_root, args.project)
    cdk_dir = resolve_project_cdk_dir(
        project_dir, getattr(args, "config", None)
    )
    image = find_firmware_image(
        project_dir, args.image, cdk_dir=cdk_dir
    )
    dll = find_download_dll(args.dll_path, sdk_root)
    port = pick_port(args.port)
    console_port = pick_port(args.console_port) if args.console_port else port
    try:
        baud_value = int(args.baud)
        shell_baud_value = int(args.shell_baud)
    except ValueError:
        return fail("--baud and --shell-baud must be numeric.", 1)

    if args.retry <= 0 or args.wait_ack_timeout < 0:
        return fail("--retry must be >= 1 and --wait-ack-timeout must be >= 0.", 1)

    copy_mode = "copy" if args.copy_dir else "direct"
    network_paths = [
        str(path) for path in (dll, image) if is_network_path(path)
    ]

    print("BURN_PLAN:")
    print(f"  sdk={sdk_root}")
    print(f"  project={project_dir.name}")
    print(f"  cdk_dir={cdk_dir}")
    print(f"  port={port}")
    print(f"  console_port={console_port}")
    print(f"  download_baud={baud_value}")
    print(f"  shell_baud={shell_baud_value}")
    print(f"  image={image}")
    print(f"  dll={dll}")
    print(f"  copy_mode={copy_mode}")
    if args.copy_dir:
        print(f"  copy_dir={args.copy_dir}")
    print(f"  retry={args.retry}")
    print(f"  wait_ack_timeout_ms={args.wait_ack_timeout}")
    print(f"  read_back_check={args.read_back_check}")
    print(f"  download_finish_start={args.download_finish_start}")
    print(f"  auto_reset={args.auto_reset}")
    print(f"  enter_mode={args.enter_mode}")
    print(f"  enter_verify={args.enter_verify}")
    print(f"  enter_read_timeout_ms={args.enter_read_timeout_ms}")

    if copy_mode == "direct" and network_paths:
        return fail(
            "DownloadLib.dll or the firmware image is on a UNC/network path. "
            "Direct burn may fail; ask the user for permission and a local "
            "--copy-dir path before burning. Detected: "
            + ", ".join(network_paths),
            1,
        )

    if args.dry_run:
        print("DRY_RUN: No hardware and no download were performed.")
        return 0

    if args.kill_before_start:
        kill_result = force_kill_com_port_users(port)
        if kill_result != 0:
            return kill_result

    if args.enter_mode != "none":
        if args.enter_mode == "hif":
            enter_result = send_hif_enter_handshake(
                console_port,
                shell_baud_value,
                args.enter_hex,
                args.enter_read_timeout_ms,
                dry_run=False,
                verify_mode=args.enter_verify,
            )
        else:
            enter_result = send_serial_command(
                console_port,
                shell_baud_value,
                args.enter_mode,
                args.enter_shell_command,
                args.enter_hex,
                args.enter_line_ending,
                dry_run=False,
                verify_mode=args.enter_verify,
            )
        if enter_result != 0:
            return enter_result
        if args.enter_wait_ms > 0:
            print(f"AWAITING_UPGRADE_MODE: Waiting {args.enter_wait_ms} ms")
            time.sleep(args.enter_wait_ms / 1000.0)

    stage_dir = None
    remove_later = False
    dll_for_burn = dll
    image_for_burn = image
    if args.copy_dir:
        stage_dir, remove_later = _staging_dir(args.copy_dir, port)
        staged_dll = stage_dir / "DownloadLib.dll"
        staged_image = stage_dir / image.name
        shutil.copy2(dll, staged_dll)
        shutil.copy2(image, staged_image)
        dll_for_burn = staged_dll
        image_for_burn = staged_image
        print(f"COPY: {dll} -> {staged_dll}")
        print(f"COPY: {image} -> {staged_image}")
    else:
        print(f"COPY_MODE=direct: using {dll} and {image} directly.")

    try:
        rc = run_ps_live(
            PS_DOWNLOAD,
            {
                "DllPath": dll_for_burn,
                "ComPort": port,
                "BaudRate": baud_value,
                "ImagePath": image_for_burn,
                "Retry": args.retry,
                "WaitAckTimeout": args.wait_ack_timeout,
                "ReadBackCheck": str(args.read_back_check),
                "DownloadFinishStart": str(
                    False if args.auto_reset else args.download_finish_start
                ),
            },
        )
        if rc != 0:
            return rc
    finally:
        if remove_later and stage_dir is not None:
            shutil.rmtree(stage_dir, ignore_errors=True)

    if args.auto_reset:
        print("AUTO_RESET: Sending DownloadLib BROM reset 0x13 after flash write.")
        reset_rc = run_ps_live(
            PS_BROM_SEND,
            {
                "DllPath": dll,
                "ComPort": port,
                "BaudRate": baud_value,
                "CmdId": "0x13",
                "Mode": "brom-reset",
                "Addr": "",
                "Len": "",
                "DataHex": "",
                "ReadTimeoutMs": 5000,
                "DebugOutput": False,
                "DryRun": False,
                "ExpectedAckHex": "",
            },
        )
        if reset_rc != 0:
            return reset_rc
        print("AUTO_RESET_OK=1 brom_reset=0x13")

    if args.post_reset_mode != "none":
        if args.post_reset_mode == "hif":
            reset_result = send_hif_enter_handshake(
                console_port,
                shell_baud_value,
                args.post_reset_hex,
                args.enter_read_timeout_ms,
                dry_run=False,
                verify_mode="none",
            )
        else:
            reset_result = send_serial_command(
                console_port,
                shell_baud_value,
                args.post_reset_mode,
                args.post_reset_shell_command,
                args.post_reset_hex,
                args.post_reset_line_ending,
                dry_run=False,
            )
        if reset_result != 0:
            return reset_result
        if args.post_reset_wait_ms > 0:
            print(f"AWAITING_RESET: Waiting {args.post_reset_wait_ms} ms")
            time.sleep(args.post_reset_wait_ms / 1000.0)
    return 0


def cmd_monitor(args: argparse.Namespace) -> int:
    try:
        baud_value = int(args.baud)
    except ValueError:
        return fail("--baud must be numeric.", 1)
    if args.duration <= 0 or args.idle_timeout < 0 or args.max_bytes < 0:
        return fail("Invalid duration/idle-timeout/max-bytes.", 1)

    out_path = ""
    if args.out:
        out = Path(args.out).expanduser().resolve()
        if not out.parent.is_dir():
            return fail(f"Output directory does not exist: {out.parent}", 1)
        out.write_text("", encoding="utf-8")
        out_path = str(out)

    params = {
        "Port": args.port,
        "Baud": baud_value,
        "Duration": args.duration,
        "IdleTimeout": args.idle_timeout,
        "UntilText": args.until_text,
        "OutPath": out_path,
        "Timestamp": args.timestamp,
        "HexOutput": args.hex,
        "MaxBytes": args.max_bytes,
    }
    if args.dry_run:
        print("DRY_RUN: Validated monitor parameters; no serial port was opened.")
        for key, value in params.items():
            print(f"  {key}={value}")
        return 0
    return run_ps_live(PS_MONITOR, params)


def cmd_send(args: argparse.Namespace) -> int:
    if bool(args.text) == bool(args.hex):
        return fail("Provide exactly one of --text or --hex.", 1)
    try:
        baud_value = int(args.baud)
    except ValueError:
        return fail("--baud must be numeric.", 1)
    if args.hif_wake and not args.hex:
        return fail("--hif-wake requires --hex.", 1)
    if args.hif_wake:
        wake_result = send_serial_command(
            args.port,
            baud_value,
            "hif",
            "upgrade",
            HIF_WAKE_MAGIC_HEX,
            "NONE",
            args.read_timeout_ms,
            dry_run=args.dry_run,
            verify_mode="wake",
        )
        if wake_result != 0:
            return wake_result
    return send_serial_command(
        args.port,
        baud_value,
        "shell" if args.text else "hif",
        args.text or "upgrade",
        args.hex or "",
        args.line_ending,
        args.read_timeout_ms,
        dry_run=args.dry_run,
        verify_mode=args.verify,
    )


def cmd_brom_send(args: argparse.Namespace) -> int:
    try:
        cmd_id = int(args.cmd, 0)
    except ValueError:
        return fail(f"Invalid --cmd value: {args.cmd}", 1)
    if not 0 <= cmd_id <= 0xFF:
        return fail("--cmd must be a one-byte BROM command id, for example 0x20 or 0x13.", 1)

    mode = args.mode
    if mode not in ("sram-write", "brom-reset"):
        return fail("--mode supports only sram-write or brom-reset.", 1)
    if mode == "brom-reset" and cmd_id != 0x13:
        return fail("--mode brom-reset requires command id 0x13.", 1)
    if mode == "sram-write" and not args.data:
        return fail("--mode sram-write requires --data with the bytes to write.", 1)
    if mode == "sram-write" and not (args.addr and args.len):
        return fail("--mode sram-write requires --addr and --len.", 1)

    expected_ack_hex = _compact_hex(args.expected_ack or "")
    if expected_ack_hex:
        if len(expected_ack_hex) % 2 != 0:
            return fail("--expected-ack must contain whole bytes.", 1)
        try:
            bytes.fromhex(expected_ack_hex)
        except ValueError:
            return fail("--expected-ack contains invalid hex bytes.", 1)

    data_hex = _compact_hex(args.data or "")
    if data_hex:
        if len(data_hex) % 2 != 0:
            return fail("--data must contain whole bytes, for example '12 34 56 78'.", 1)
        try:
            bytes.fromhex(data_hex)
        except ValueError:
            return fail("--data contains invalid hex bytes.", 1)

    addr_value = ""
    len_value = ""
    if args.addr:
        try:
            addr_value = f"0x{int(args.addr, 0):X}"
        except ValueError:
            return fail(f"Invalid --addr value: {args.addr}", 1)
    if args.len:
        try:
            parsed_len = int(args.len, 0)
        except ValueError:
            return fail(f"Invalid --len value: {args.len}", 1)
        if parsed_len <= 0:
            return fail("--len must be greater than zero.", 1)
        len_value = f"0x{parsed_len:X}"

    try:
        baud_value = int(args.baud)
    except ValueError:
        return fail("--baud must be numeric.", 1)
    if args.read_timeout_ms <= 0:
        return fail("--read-timeout-ms must be greater than zero.", 1)

    sdk_root = find_sdk_root(args.sdk_root, required=False)
    dll = find_download_dll(args.dll_path, sdk_root)
    port = pick_port(args.port)

    print("BROM_SEND_PLAN:")
    print(f"  port={port}")
    print(f"  download_baud={baud_value}")
    print(f"  cmd=0x{cmd_id:02X}")
    print(f"  mode={mode}")
    if addr_value:
        print(f"  addr={addr_value}")
    if len_value:
        print(f"  len={len_value}")
    if data_hex:
        print(f"  data={args.data.strip()}")
    if mode == "brom-reset":
        print("  reset_sequence=0x7E sync -> 0x20 SRAMWR 0x40009070=00 00 00 00 -> 0x13 reset")
        print("  tx=50 53 49 43 01 02 4D 66 05 00 00 00 13 01 00 00 00")
        print("  expected_ack=50 53 49 43 02 03 64 66 00 00 00 00")
    print(f"  read_timeout_ms={args.read_timeout_ms}")
    print(f"  dll={dll}")
    print(f"  dry_run={bool(args.dry_run)}")

    params = {
        "DllPath": dll,
        "ComPort": port,
        "BaudRate": baud_value,
        "CmdId": f"0x{cmd_id:02X}",
        "Mode": mode,
        "Addr": addr_value,
        "Len": len_value,
        "DataHex": args.data or "",
        "ReadTimeoutMs": args.read_timeout_ms,
        "DebugOutput": bool(args.verbose),
        "DryRun": bool(args.dry_run),
        "ExpectedAckHex": args.expected_ack or "",
    }
    return run_ps_live(PS_BROM_SEND, params)


def encoded_power_shell(script: str) -> str:
    encoded = script.encode("utf-16-le")
    return base64.b64encode(encoded).decode("ascii")


def read_user_path() -> str:
    script = "$p = [Environment]::GetEnvironmentVariable('Path','User'); Write-Output $p"
    try:
        proc = subprocess.run(
            [
                "powershell.exe",
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-EncodedCommand",
                encoded_power_shell(script),
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
    except FileNotFoundError:
        return ""
    return proc.stdout.strip()


def power_shell_quote(value: str) -> str:
    return "'" + value.replace("'", "''") + "'"


def collapse_duplicate_backslashes(value: str) -> str:
    while "\\\\" in value:
        value = value.replace("\\\\", "\\")
    return value


def add_user_path(directory: Path) -> int:
    current = read_user_path()
    normalized = str(directory).rstrip("\\")
    canonical = normalized.lower()
    clean_parts: list[str] = []
    found = False

    for raw_item in current.split(";"):
        item = raw_item.strip()
        if not item:
            continue
        key = item.rstrip("\\").lower()
        if key == canonical or collapse_duplicate_backslashes(key) == canonical:
            if not found:
                clean_parts.append(normalized)
                found = True
            continue
        clean_parts.append(item)

    new_path = ";".join(clean_parts)
    if found and new_path == current:
        print(f"PATH_OK: {normalized} is already in the user PATH.")
        return 0
    if found:
        wrote = write_user_path(new_path)
        if wrote != 0:
            return wrote
        print(f"PATH_CLEANED: {normalized}")
        return 0

    if clean_parts:
        new_path += ";" + normalized
    else:
        new_path = normalized
    wrote = write_user_path(new_path)
    if wrote != 0:
        return wrote
    print(f"PATH_UPDATED: {normalized}")
    print("PATH_WARNING: Open a new terminal before using the bare command `AgentAuto`.")
    return 0


def write_user_path(new_path: str) -> int:
    quoted = power_shell_quote(new_path)
    script = (
        "[Environment]::SetEnvironmentVariable('Path', "
        + quoted
        + ", 'User')"
    )
    try:
        proc = subprocess.run(
            [
                "powershell.exe",
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-EncodedCommand",
                encoded_power_shell(script),
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
    except FileNotFoundError:
        return fail("PowerShell was not found; cannot update PATH.", 2)
    if proc.returncode != 0:
        return fail(f"Could not update PATH: {proc.stderr.strip()}", 2)
    return 0


def launcher_script(python_exe: str = "python") -> str:
    return "\r\n".join(
        [
            "@echo off",
            'chcp 65001 >nul',
            "setlocal",
            f'set "PYTHON={python_exe}"',
            'if defined AGENT_AUTO_PYTHON set "PYTHON=%AGENT_AUTO_PYTHON%"',
            '"%PYTHON%" "%~dp0AgentAuto_cli.py" %*',
            "exit /b %ERRORLEVEL%",
            "",
        ]
    )


def _release_root() -> Path:
    script = Path(__file__).resolve()
    candidates = [script.parent]
    for index in range(4):
        if index < len(script.parents):
            candidates.append(script.parents[index])
    for candidate in candidates:
        if (candidate / "modules").is_dir():
            return candidate
    return script.parent


def cmd_install(args: argparse.Namespace) -> int:
    if args.prefix:
        target_dir = Path(args.prefix).expanduser().resolve()
    else:
        local = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local"))
        target_dir = local / "AgentAuto"
    target_dir.mkdir(parents=True, exist_ok=True)
    runtime_dir = target_dir / "runtime"
    runtime_dir.mkdir(parents=True, exist_ok=True)

    target_py = target_dir / "AgentAuto_cli.py"
    engine_target_py = runtime_dir / "agentauto_engine.py"
    target_cmd = target_dir / "AgentAuto.cmd"
    target_dll = target_dir / "DownloadLib.dll"
    target_config = target_dir / "AgentAuto.conf"
    release_root = _release_root()
    config_src = release_root / "AgentAuto.conf"
    unified_cli = release_root / "modules" / "core" / "cli.py"
    modules_src = release_root / "modules"
    modules_target = target_dir / "modules"

    if target_py.exists():
        target_py.unlink()
    if unified_cli.is_file():
        shutil.copy2(unified_cli, target_py)
    else:
        shutil.copy2(__file__, target_py)
    shutil.copy2(__file__, engine_target_py)

    if modules_src.is_dir() and modules_src.resolve() == modules_target.resolve():
        print(f"INSTALL_OK: {modules_target} (already current)")
    elif modules_src.is_dir():
        resolved_modules_target = modules_target.resolve()
        if modules_target.exists():
            if target_dir.resolve() not in resolved_modules_target.parents:
                return fail(
                    "Refusing to replace modules outside the install prefix.",
                    2,
                )
            shutil.rmtree(resolved_modules_target)
        shutil.copytree(
            modules_src,
            modules_target,
            ignore=shutil.ignore_patterns("__pycache__", "*.pyc"),
        )

    if config_src.is_file() and not target_config.exists():
        shutil.copy2(config_src, target_config)

    dll = find_download_dll(args.dll_path, None, required=False)
    if dll is not None:
        shutil.copy2(dll, target_dll)
    target_cmd.write_text(
        launcher_script(python_exe=sys.executable),
        encoding="ascii",
        errors="replace",
    )

    print(f"INSTALL_OK: {target_py}")
    print(f"INSTALL_OK: {engine_target_py}")
    print(f"INSTALL_OK: {modules_target}")
    print(f"INSTALL_OK: {target_cmd}")
    if target_config.exists():
        print(f"INSTALL_OK: {target_config}")
    if target_dll.exists():
        print(f"INSTALL_OK: {target_dll}")

    if args.add_path:
        if not target_dll.exists():
            return fail(
                "DownloadLib.dll was not bundled, so PATH install was skipped. "
                "Copy DownloadLib.dll next to AgentAuto_cli.py and rerun.",
                2,
            )
        return add_user_path(target_dir)
    print(
        f"PATH_NEXT: add '{target_dir}' to PATH, or run "
        f"`{target_cmd} --help` now by full path."
    )
    return 0


def cmd_doctor(args: argparse.Namespace) -> int:
    print(f"OS={sys.platform}")
    print(f"PYTHON={sys.executable}")
    sdk_root = find_sdk_root(args.sdk_root, required=False)
    print(f"SDK_ROOT={sdk_root or 'not found'}")
    print(f"CDK_MAKE={find_cdk_make(args.cdk_make) or 'not found'}")
    dll = find_download_dll(args.dll_path, sdk_root, required=False)
    print(f"DOWNLOAD_DLL={dll or 'not found'}")
    projects = find_project_dirs(sdk_root) if sdk_root else []
    print(f"PROJECTS={','.join(p.name for p in projects) if projects else 'none'}")
    try:
        ports = list_ports()
        print(f"PORTS={','.join(ports) if ports else 'none'}")
    except AutoburnError as exc:
        print(f"PORTS_ERROR={exc}")
    return 0


def add_prepare_parser(sub) -> None:
    parser = sub.add_parser(
        "prepare", help="Enable shell macros and refresh the CDK source list."
    )
    parser.add_argument(
        "--project",
        help="Project directory name or absolute project folder path.",
    )
    parser.add_argument(
        "--config",
        help="CDK directory containing .cdkproj, or CDK build configuration name.",
    )
    parser.add_argument(
        "--board",
        default="",
        help="Board selection macro, for example CONFIG_BOARD_MRS6240_P2512_CPUF; written to project.cdkproj.",
    )
    parser.add_argument("--skip-shell", action="store_true")
    parser.add_argument("--skip-cdkproj", action="store_true")
    parser.add_argument(
        "--skip-modules",
        action="store_true",
        help="Skip automatic MMW/HIF source, include, linker, and macro enablement.",
    )
    parser.add_argument("--exclude", action="append", default=[], help="Temporarily exclude a source path.")
    parser.add_argument("--exclude-glob", action="append", default=[], help="Temporarily exclude sources matching a glob.")
    parser.add_argument("--unexclude", action="append", default=[], help="Restore a previously excluded source path.")
    parser.add_argument("--dry-run", action="store_true")
    parser.set_defaults(handler=cmd_prepare)


def add_refresh_sources_parser(sub) -> None:
    parser = sub.add_parser(
        "refresh-sources",
        help="Refresh valid source entries in the active .cdkproj.",
    )
    parser.add_argument(
        "--project",
        help="Project directory name or absolute project folder path.",
    )
    parser.add_argument(
        "--config",
        help="CDK directory containing .cdkproj, or CDK build configuration name.",
    )
    parser.add_argument("--exclude", action="append", default=[], help="Temporarily exclude a source path.")
    parser.add_argument("--exclude-glob", action="append", default=[], help="Temporarily exclude sources matching a glob.")
    parser.add_argument("--unexclude", action="append", default=[], help="Restore a previously excluded source path.")
    parser.add_argument(
        "--skip-modules",
        action="store_true",
        help="Skip automatic MMW/HIF source/include/linker enablement.",
    )
    parser.add_argument("--dry-run", action="store_true")
    parser.set_defaults(handler=cmd_refresh_sources)


def add_exclude_parser(sub) -> None:
    parser = sub.add_parser(
        "exclude", help="Temporarily remove source files from CDK compilation."
    )
    parser.add_argument(
        "--project",
        help="Project directory name or absolute project folder path.",
    )
    parser.add_argument(
        "--config",
        help="CDK directory containing .cdkproj, or CDK build configuration name.",
    )
    parser.add_argument("--source", action="append", default=[], required=True)
    parser.add_argument("--exclude", action="append", default=[], help=argparse.SUPPRESS)
    parser.add_argument("--dry-run", action="store_true")
    parser.set_defaults(handler=cmd_exclude)


def add_unexclude_parser(sub) -> None:
    parser = sub.add_parser(
        "unexclude", help="Restore source files excluded from CDK compilation."
    )
    parser.add_argument(
        "--project",
        help="Project directory name or absolute project folder path.",
    )
    parser.add_argument(
        "--config",
        help="CDK directory containing .cdkproj, or CDK build configuration name.",
    )
    parser.add_argument("--source", action="append", default=[], required=True)
    parser.add_argument("--unexclude", action="append", default=[], help=argparse.SUPPRESS)
    parser.add_argument("--dry-run", action="store_true")
    parser.set_defaults(handler=cmd_unexclude)


def add_excluded_parser(sub) -> None:
    parser = sub.add_parser(
        "excluded", help="Show the current temporary exclusion list."
    )
    parser.add_argument(
        "--project",
        help="Project directory name or absolute project folder path.",
    )
    parser.add_argument(
        "--config",
        help="CDK directory containing .cdkproj, or CDK build configuration name.",
    )
    parser.set_defaults(handler=cmd_excluded)


def add_build_parser(sub) -> None:
    parser = sub.add_parser("build", help="Build a CDK project.")
    parser.add_argument(
        "--project",
        help="Project directory name or absolute project folder path.",
    )
    parser.add_argument(
        "--config",
        help="CDK directory containing .cdkproj, or CDK build configuration name.",
    )
    parser.add_argument(
        "--board",
        default="",
        help="Board selection macro, for example CONFIG_BOARD_MRS6240_P2512_CPUF; written to project.cdkproj.",
    )
    parser.add_argument(
        "--skip-prepare",
        action="store_true",
        help="Do not enable shell macros or refresh the CDK source list before build.",
    )
    parser.add_argument(
        "--skip-shell",
        action="store_true",
        help="Skip the shell macro auto-enable step during prepare.",
    )
    parser.add_argument(
        "--skip-cdkproj",
        action="store_true",
        help="Skip the .cdkproj source refresh step during prepare.",
    )
    parser.add_argument(
        "--skip-modules",
        action="store_true",
        help="Skip automatic MMW/HIF source, include, linker, and macro enablement.",
    )
    parser.add_argument("--exclude", action="append", default=[])
    parser.add_argument("--exclude-glob", action="append", default=[])
    parser.add_argument(
        "--exclude-on-error",
        dest="exclude_on_error",
        action="store_true",
        default=True,
        help="Exclude source files that fail compilation, then rebuild.",
    )
    parser.add_argument(
        "--no-exclude-on-error",
        dest="exclude_on_error",
        action="store_false",
        help="Stop on the first compile error without excluding files.",
    )
    parser.add_argument("--dry-run", action="store_true", help="Validate only.")
    parser.set_defaults(handler=cmd_build)


def add_burn_parser(sub) -> None:
    parser = sub.add_parser("burn", help="Burn a firmware image with DownloadLib.")
    parser.add_argument(
        "--project",
        help="Project directory name or absolute project folder path.",
    )
    parser.add_argument(
        "--config",
        help="CDK directory containing .cdkproj used to select the matching output image.",
    )
    parser.add_argument("--image", help="Explicit firmware .img path.")
    parser.add_argument("--port", default=None, help="Download/shell COM port.")
    parser.add_argument(
        "--console-port",
        default=None,
        help="Console COM port used by --enter-mode/--post-reset-mode; defaults to --port.",
    )
    parser.add_argument("--baud", default=DEFAULT_BURN_BAUD, help="DownloadLib baud.")
    parser.add_argument(
        "--shell-baud",
        "--console-baud",
        dest="shell_baud",
        default=DEFAULT_SHELL_BAUD,
        help="Console/HIF baud for --enter-mode/--post-reset-mode (shell default 115200; HIF sets its PHY baud).",
    )
    parser.add_argument("--retry", type=int, default=3, help="DownloadLib retries.")
    parser.add_argument(
        "--wait-ack-timeout", type=int, default=5000, help="DownloadLib ACK timeout ms."
    )
    parser.add_argument(
        "--read-back-check",
        dest="read_back_check",
        action="store_true",
        default=True,
    )
    parser.add_argument(
        "--no-read-back-check",
        dest="read_back_check",
        action="store_false",
    )
    parser.add_argument(
        "--download-finish-start",
        dest="download_finish_start",
        action="store_true",
        default=True,
    )
    parser.add_argument(
        "--no-download-finish-start",
        dest="download_finish_start",
        action="store_false",
    )
    parser.add_argument(
        "--auto-reset",
        dest="auto_reset",
        action="store_true",
        default=True,
        help="After a successful flash write, send DownloadLib BROM reset 0x13.",
    )
    parser.add_argument(
        "--no-auto-reset",
        dest="auto_reset",
        action="store_false",
        help="Keep DownloadLib's download_finish_start behavior and skip the explicit 0x13 BROM reset.",
    )
    parser.add_argument(
        "--kill-before-start",
        dest="kill_before_start",
        action="store_true",
        default=True,
    )
    parser.add_argument(
        "--no-kill-before-start",
        dest="kill_before_start",
        action="store_false",
    )
    parser.add_argument(
        "--copy-dir",
        default=None,
        help="Optional local staging directory. Copy is disabled by default.",
    )
    parser.add_argument("--enter-mode", choices=("none", "shell", "hif"), default="none")
    parser.add_argument(
        "--enter-verify",
        choices=("none", "auto", "shell", "hif"),
        default="auto",
        help="Require serial evidence after --enter-mode. shell expects the "
        "typed command echo; hif expects the reboot ACK "
        "A5 43 12 04 01 00 00 ED FB FE FF.",
    )
    parser.add_argument("--enter-shell-command", default="upgrade")
    parser.add_argument("--enter-line-ending", choices=sorted(DEFAULT_LINE_ENDINGS), default="CRLF")
    parser.add_argument("--enter-hex", default=HIF_UPGRADE_ENTER_HEX)
    parser.add_argument("--enter-wait-ms", type=int, default=3000)
    parser.add_argument(
        "--enter-read-timeout-ms",
        type=int,
        default=4000,
        help="Read timeout for each HIF wake/reboot enter exchange.",
    )
    parser.add_argument("--post-reset-mode", choices=("none", "shell", "hif"), default="none")
    parser.add_argument("--post-reset-shell-command", default="reset")
    parser.add_argument("--post-reset-line-ending", choices=sorted(DEFAULT_LINE_ENDINGS), default="CRLF")
    parser.add_argument("--post-reset-hex", default=HIF_UPGRADE_ENTER_HEX)
    parser.add_argument("--post-reset-wait-ms", type=int, default=3000)
    parser.add_argument("--dry-run", action="store_true")
    parser.set_defaults(handler=cmd_burn)


def add_monitor_parser(sub) -> None:
    parser = sub.add_parser("monitor", help="Capture UART output.")
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", default=DEFAULT_SHELL_BAUD)
    parser.add_argument("--duration", type=float, default=10.0)
    parser.add_argument("--idle-timeout", type=float, default=3.0)
    parser.add_argument("--until-text", default="")
    parser.add_argument("--out", default="")
    parser.add_argument("--no-timestamp", dest="timestamp", action="store_false", default=True)
    parser.add_argument("--hex", action="store_true")
    parser.add_argument("--max-bytes", type=int, default=0)
    parser.add_argument("--dry-run", action="store_true")
    parser.set_defaults(handler=cmd_monitor)


def add_send_parser(sub) -> None:
    parser = sub.add_parser("send", help="Send shell text or raw HIF bytes.")
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", default=DEFAULT_SHELL_BAUD)
    parser.add_argument("--text")
    parser.add_argument("--hex")
    parser.add_argument(
        "--verify",
        choices=("none", "auto", "shell", "hif", "wake"),
        default="none",
        help="Verify shell echo/prompt, HIF wake ACK 0x79, or the HIF reboot ACK.",
    )
    parser.add_argument(
        "--hif-wake",
        action="store_true",
        help="Send 55 FF 55 FF first, wait for 0x79 ACK, then send --hex.",
    )
    parser.add_argument("--line-ending", choices=sorted(DEFAULT_LINE_ENDINGS), default="CRLF")
    parser.add_argument("--read-timeout-ms", type=int, default=1500)
    parser.add_argument("--dry-run", action="store_true")
    parser.set_defaults(handler=cmd_send)


def add_brom_send_parser(sub) -> None:
    parser = sub.add_parser(
        "brom-send",
        help="Send one DownloadLib BROM command in download mode and capture its ACK/reply.",
    )
    parser.add_argument("--cmd", required=True, help="BROM command id, for example 0x20 or 0x13.")
    parser.add_argument(
        "--mode",
        choices=("sram-write", "brom-reset"),
        default="sram-write",
        help="sram-write writes SRAM (0x20); brom-reset runs 0x7E sync, writes 0x40009070=0, then sends the fixed 0x13 reset frame.",
    )
    parser.add_argument("--port", required=True, help="DownloadLib COM port.")
    parser.add_argument("--baud", default=DEFAULT_BURN_BAUD, help="DownloadLib baud.")
    parser.add_argument("--addr", default="", help="SRAM address as hex, for example 0x20000000.")
    parser.add_argument("--len", default="", help="SRAM byte length as decimal or hex.")
    parser.add_argument("--data", default="", help="Hex payload for sram-write.")
    parser.add_argument("--read-timeout-ms", type=int, default=5000)
    parser.add_argument(
        "--expected-ack",
        default="",
        help="Expected reply hex for brom-reset; defaults to 50 53 49 43 02 03 64 66 00 00 00 00.",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print BROM_TX_HEADER_HEX/BROM_TX_DATA_HEX before sending.",
    )
    parser.add_argument("--dry-run", action="store_true", help="Build the frame and validate without opening the COM port.")
    parser.set_defaults(handler=cmd_brom_send)


def add_install_parser(sub) -> None:
    parser = sub.add_parser("install", help="Install an `AgentAuto` command.")
    parser.add_argument("--prefix", default=None, help="Installation directory.")
    parser.add_argument("--add-path", action="store_true", help="Append prefix to user PATH.")
    parser.set_defaults(handler=cmd_install)


def add_parse_hif_parser(sub) -> None:
    parser = sub.add_parser(
        "parse-hif",
        help="Decode HIF hex captures and report 0xC2 FFT/0xC1 legacy datacube fragments.",
    )
    parser.add_argument("input", help="Monitor hex log, RX_HEX log, or binary capture.")
    parser.add_argument(
        "--msg-id",
        default="auto",
        help="Decode only this message id, for example 0xC2 or 194.",
    )
    parser.add_argument("--raw", action="store_true", help="Treat input as raw binary bytes.")
    parser.add_argument(
        "--bins",
        action="store_true",
        help="Print every 0xC2/0xC1 datacube bin as frame_idx|bin|imag_s16|real_s16.",
    )
    parser.add_argument("--json", action="store_true", help="Print parsed frames as JSON.")
    parser.set_defaults(handler=cmd_parse_hif)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog=PROG,
        description="Build, burn, reset, and monitor PSIC SDK boards from one CLI.",
    )
    parser.add_argument("--sdk", dest="sdk_root", default=None, help="SDK root directory.")
    parser.add_argument("--cdk", dest="cdk_make", default=None, help="cdk-make.exe path.")
    parser.add_argument("--dll", dest="dll_path", default=None, help="DownloadLib.dll path.")
    sub = parser.add_subparsers(dest="command", required=True)

    discover = sub.add_parser("discover", help="Show discovered environment.")
    discover.add_argument("--json", action="store_true")
    discover.set_defaults(handler=cmd_discover)

    ports = sub.add_parser("ports", help="List available COM ports.")
    ports.add_argument("--json", action="store_true")
    ports.set_defaults(handler=cmd_ports)
    sub.add_parser("doctor", help="Check the environment.").set_defaults(
        handler=cmd_doctor
    )
    add_prepare_parser(sub)
    add_refresh_sources_parser(sub)
    add_exclude_parser(sub)
    add_unexclude_parser(sub)
    add_excluded_parser(sub)
    add_build_parser(sub)
    add_burn_parser(sub)
    add_monitor_parser(sub)
    add_send_parser(sub)
    add_brom_send_parser(sub)
    add_install_parser(sub)
    add_parse_hif_parser(sub)
    return parser


def main(argv: list[str] | None = None) -> int:
    configure_console()
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.command in ("discover", "ports"):
        args.json = getattr(args, "json", False)

    try:
        return int(args.handler(args))
    except AutoburnError as exc:
        return fail(str(exc), 1)


if __name__ == "__main__":
    raise SystemExit(main())
