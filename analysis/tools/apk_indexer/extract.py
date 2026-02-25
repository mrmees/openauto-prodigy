from __future__ import annotations

from pathlib import Path
import re


UUID_RE = re.compile(
    r"\b[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\b"
)
HEX_RE = re.compile(r"\b0x[0-9A-Fa-f]{4,}\b")
PROTO_ACCESS_RE = re.compile(r"\b(set|get|has|clear)[A-Z][A-Za-z0-9_]*\s*\(")
CALL_EDGE_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\.([A-Za-z_][A-Za-z0-9_]*)\s*\(")


def _iter_text_files(root: Path):
    for path in root.rglob("*"):
        if path.is_file():
            yield path


def extract_signals(root: Path) -> dict[str, list[dict[str, object]]]:
    uuids: list[dict[str, object]] = []
    constants: list[dict[str, object]] = []
    proto_accesses: list[dict[str, object]] = []
    call_edges: list[dict[str, object]] = []

    for path in _iter_text_files(root):
        try:
            text = path.read_text(errors="ignore")
        except OSError:
            continue

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
        "call_edges": sorted(
            call_edges, key=lambda row: (row["target"], row["file"], row["line"])
        ),
    }
