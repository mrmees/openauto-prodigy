from __future__ import annotations

from pathlib import Path
import json
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

# Proto-lite class patterns
EXTENDS_ZYT_RE = re.compile(r"\bextends\s+defpackage\.zyt\b")
DEPRECATED_RE = re.compile(r"@Deprecated")
# Field declarations: public <type> <name>
FIELD_DECL_RE = re.compile(
    r"^\s+public\s+(?!static|final)(\S+)\s+([a-z][a-zA-Z0-9]*)\s*[;=]",
    re.MULTILINE,
)
# aaaj constructor with descriptor string and field name array
AAAJ_RE = re.compile(
    r'new\s+defpackage\.aaaj\s*\(\s*a\s*,\s*"([^"]*?)"\s*,\s*'
    r"new\s+java\.lang\.Object\[\]\s*\{([^}]*)\}\s*\)"
)
# References to defpackage classes (e.g. defpackage.vvh, defpackage.aafs)
DEFPACKAGE_REF_RE = re.compile(r"\bdefpackage\.([a-z][a-z0-9]*)\b")


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


def _extract_proto_class_from_text(
    path: Path, text: str
) -> dict[str, object] | None:
    """Extract metadata from protobuf-lite classes (extend zyt, implement aaaa)."""
    if not EXTENDS_ZYT_RE.search(text):
        return None

    class_name = path.stem  # e.g. "vvh" from "vvh.java"
    deprecated = bool(DEPRECATED_RE.search(text))

    # Extract field declarations (type + name)
    fields: list[dict[str, str]] = []
    for m in FIELD_DECL_RE.finditer(text):
        field_type = m.group(1)
        field_name = m.group(2)
        # Normalize defpackage types to just the class name
        if field_type.startswith("defpackage."):
            field_type = field_type.replace("defpackage.", "")
        elif field_type == "java.lang.String":
            field_type = "String"
        fields.append({"type": field_type, "name": field_name})

    # Extract aaaj descriptor string and field name array
    descriptor = ""
    field_names: list[str] = []
    aaaj_match = AAAJ_RE.search(text)
    if aaaj_match:
        descriptor = aaaj_match.group(1)
        names_raw = aaaj_match.group(2)
        field_names = [
            n.strip().strip('"') for n in names_raw.split(",") if n.strip().strip('"')
        ]

    # Extract sub-message type references from field declarations
    sub_message_refs: list[str] = []
    for f in fields:
        ft = f["type"]
        if ft not in ("int", "long", "boolean", "float", "double", "byte", "String",
                       "byte[]") and not ft.startswith("zzb") and not ft.startswith("zzf"):
            # Likely a sub-message reference (another proto class)
            sub_message_refs.append(ft)

    return {
        "file": str(path),
        "class_name": class_name,
        "deprecated": deprecated,
        "field_count": len(fields),
        "field_names": json.dumps(field_names),
        "field_types": json.dumps([f["type"] for f in fields]),
        "field_decls": json.dumps(fields),
        "sub_message_refs": json.dumps(sub_message_refs),
        "descriptor": descriptor,
    }


def _extract_class_references_from_text(
    path: Path, text: str, root: Path
) -> list[dict[str, object]]:
    """Find references to defpackage classes from all files."""
    rel = path.relative_to(root).as_posix()
    source_class = path.stem  # For defpackage files, this is the class name
    is_defpackage = "defpackage/" in rel

    rows: list[dict[str, object]] = []
    seen: set[str] = set()  # Dedupe per file
    for line_no, line in enumerate(text.splitlines(), 1):
        for m in DEFPACKAGE_REF_RE.finditer(line):
            target_class = m.group(1)
            # Skip self-references in defpackage files
            if is_defpackage and target_class == source_class:
                continue
            if target_class not in seen:
                seen.add(target_class)
                if is_defpackage:
                    source_pkg = f"defpackage.{source_class}"
                else:
                    source_pkg = rel.replace("sources/", "").replace("/", ".").rsplit(".", 1)[0]
                rows.append({
                    "file": str(path),
                    "line": line_no,
                    "source_package": source_pkg,
                    "target_class": target_class,
                })
    return rows


def extract_signals(root: Path, scope: str = "all") -> dict[str, list[dict[str, object]]]:
    uuids: list[dict[str, object]] = []
    constants: list[dict[str, object]] = []
    proto_accesses: list[dict[str, object]] = []
    proto_writes: list[dict[str, object]] = []
    call_edges: list[dict[str, object]] = []
    enum_maps: list[dict[str, object]] = []
    switch_maps: list[dict[str, object]] = []
    proto_classes: list[dict[str, object]] = []
    class_references: list[dict[str, object]] = []

    for path in _iter_text_files(root, scope):
        try:
            text = path.read_text(errors="ignore")
        except OSError:
            continue
        enum_maps.extend(_extract_enum_maps_from_text(path, text))
        switch_maps.extend(_extract_switch_maps_from_text(path, text))

        # Proto class metadata (defpackage proto-lite classes)
        proto_meta = _extract_proto_class_from_text(path, text)
        if proto_meta is not None:
            proto_classes.append(proto_meta)

        # Cross-references from named gearhead files to defpackage classes
        class_references.extend(
            _extract_class_references_from_text(path, text, root)
        )

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
        "proto_classes": sorted(
            proto_classes, key=lambda row: (row["class_name"],)
        ),
        "class_references": sorted(
            class_references,
            key=lambda row: (row["target_class"], row["source_package"], row["file"]),
        ),
    }
