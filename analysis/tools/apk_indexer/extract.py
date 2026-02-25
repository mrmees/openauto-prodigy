from __future__ import annotations

from pathlib import Path
import re


UUID_RE = re.compile(
    r"\b[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\b"
)
HEX_RE = re.compile(r"\b0x[0-9A-Fa-f]{4,}\b")
PROTO_ACCESS_RE = re.compile(r"\b(set|get|has|clear)[A-Z][A-Za-z0-9_]*\s*\(")
CALL_EDGE_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\.([A-Za-z_][A-Za-z0-9_]*)\s*\(")
PROTO_WRITE_OR_RE = re.compile(
    r"\b([A-Za-z_][A-Za-z0-9_]*\.[A-Za-z_][A-Za-z0-9_]*)\s*(\|=)\s*([^;]+);"
)
PROTO_WRITE_ASSIGN_RE = re.compile(
    r"\b([A-Za-z_][A-Za-z0-9_]*\.[A-Za-z_][A-Za-z0-9_]*)\s*(?<![=!<>])=(?!=)\s*([^;]+);"
)
ENUM_CLASS_RE = re.compile(r"\b(?:public\s+)?enum\s+([A-Za-z_][A-Za-z0-9_]*)\b")
SWITCH_RE = re.compile(r"\bswitch\s*\(([^)]+)\)")
CASE_RE = re.compile(r"\bcase\s+([^:]+):")


def _in_scope(path: Path, root: Path, scope: str) -> bool:
    if scope == "all":
        return True
    rel = path.relative_to(root).as_posix()
    if scope == "projection":
        return rel.startswith("sources/com/google/android/projection/")
    return True


def _iter_text_files(root: Path, scope: str):
    for path in root.rglob("*"):
        if path.is_file() and _in_scope(path, root, scope):
            yield path


def _extract_enum_maps_from_text(path: Path, text: str) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    enum_match = ENUM_CLASS_RE.search(text)
    if not enum_match:
        return rows
    enum_class = enum_match.group(1)

    lines = text.splitlines()
    pending_case: tuple[int, int] | None = None
    for line_no, line in enumerate(lines, 1):
        case_match = CASE_RE.search(line)
        if case_match:
            value_raw = case_match.group(1).strip()
            if value_raw.isdigit():
                pending_case = (line_no, int(value_raw))
            else:
                pending_case = None
            continue
        if pending_case is None:
            continue

        return_match = re.search(r"\breturn\s+([A-Z][A-Z0-9_]+)\s*;", line)
        if return_match:
            rows.append(
                {
                    "file": str(path),
                    "line": pending_case[0],
                    "enum_class": enum_class,
                    "int_value": pending_case[1],
                    "enum_name": return_match.group(1),
                }
            )
            pending_case = None
    return rows


def _extract_switch_maps_from_text(path: Path, text: str) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    lines = text.splitlines()
    current_switch: str | None = None
    current_case: tuple[int, str] | None = None

    for line_no, line in enumerate(lines, 1):
        switch_match = SWITCH_RE.search(line)
        if switch_match:
            current_switch = switch_match.group(1).strip()
            current_case = None
            continue
        if current_switch is None:
            continue
        case_match = CASE_RE.search(line)
        if case_match:
            current_case = (line_no, case_match.group(1).strip())
            continue
        if current_case is not None:
            target_match = CALL_EDGE_RE.search(line)
            if target_match:
                rows.append(
                    {
                        "file": str(path),
                        "line": current_case[0],
                        "switch_expr": current_switch,
                        "case_value": current_case[1],
                        "target": f"{target_match.group(1)}.{target_match.group(2)}",
                    }
                )
                current_case = None
                continue
        if line.strip() == "}":
            current_switch = None
            current_case = None
    return rows


def extract_signals(root: Path, scope: str = "all") -> dict[str, list[dict[str, object]]]:
    uuids: list[dict[str, object]] = []
    constants: list[dict[str, object]] = []
    proto_accesses: list[dict[str, object]] = []
    proto_writes: list[dict[str, object]] = []
    call_edges: list[dict[str, object]] = []
    enum_maps: list[dict[str, object]] = []
    switch_maps: list[dict[str, object]] = []

    for path in _iter_text_files(root, scope):
        try:
            text = path.read_text(errors="ignore")
        except OSError:
            continue
        enum_maps.extend(_extract_enum_maps_from_text(path, text))
        switch_maps.extend(_extract_switch_maps_from_text(path, text))

        for line_no, line in enumerate(text.splitlines(), 1):
            for match in UUID_RE.finditer(line):
                uuids.append(
                    {
                        "file": str(path),
                        "line": line_no,
                        "value": match.group(0),
                    }
                )
            for match in HEX_RE.finditer(line):
                constants.append(
                    {
                        "file": str(path),
                        "line": line_no,
                        "value": match.group(0),
                    }
                )
            for match in PROTO_ACCESS_RE.finditer(line):
                accessor = match.group(0).split("(", 1)[0].strip()
                proto_accesses.append(
                    {
                        "file": str(path),
                        "line": line_no,
                        "accessor": accessor,
                    }
                )
            for match in PROTO_WRITE_OR_RE.finditer(line):
                proto_writes.append(
                    {
                        "file": str(path),
                        "line": line_no,
                        "target": match.group(1),
                        "op": match.group(2),
                        "value": match.group(3).strip(),
                    }
                )
            for match in PROTO_WRITE_ASSIGN_RE.finditer(line):
                proto_writes.append(
                    {
                        "file": str(path),
                        "line": line_no,
                        "target": match.group(1),
                        "op": "=",
                        "value": match.group(2).strip(),
                    }
                )
            for match in CALL_EDGE_RE.finditer(line):
                target = f"{match.group(1)}.{match.group(2)}"
                call_edges.append(
                    {
                        "file": str(path),
                        "line": line_no,
                        "target": target,
                    }
                )

    return {
        "uuids": sorted(uuids, key=lambda row: (row["value"], row["file"], row["line"])),
        "constants": sorted(
            constants, key=lambda row: (row["value"], row["file"], row["line"])
        ),
        "proto_accesses": sorted(
            proto_accesses, key=lambda row: (row["accessor"], row["file"], row["line"])
        ),
        "proto_writes": sorted(
            proto_writes,
            key=lambda row: (row["target"], row["op"], row["file"], row["line"]),
        ),
        "enum_maps": sorted(
            enum_maps,
            key=lambda row: (row["enum_class"], row["int_value"], row["file"], row["line"]),
        ),
        "switch_maps": sorted(
            switch_maps,
            key=lambda row: (row["switch_expr"], row["case_value"], row["file"], row["line"]),
        ),
        "call_edges": sorted(
            call_edges, key=lambda row: (row["target"], row["file"], row["line"])
        ),
    }
