"""Parse proto2 .proto files into structured field data."""

from __future__ import annotations

import re
from pathlib import Path

BASE_TYPES = {
    "double",
    "float",
    "int32",
    "int64",
    "uint32",
    "uint64",
    "sint32",
    "sint64",
    "fixed32",
    "fixed64",
    "sfixed32",
    "sfixed64",
    "bool",
    "string",
    "bytes",
}


def _strip_comments(content: str) -> str:
    content = re.sub(r"//.*", "", content)
    content = re.sub(r"/\*.*?\*/", "", content, flags=re.DOTALL)
    return content


def _extract_blocks(content: str, kind: str) -> list[tuple[str, str]]:
    blocks: list[tuple[str, str]] = []
    pos = 0
    marker = f"{kind} "
    while True:
        start = content.find(marker, pos)
        if start == -1:
            break

        name_match = re.match(rf"{kind}\s+(\w+)\s*\{{", content[start:])
        if not name_match:
            pos = start + len(marker)
            continue

        name = name_match.group(1)
        open_idx = start + name_match.end() - 1
        depth = 0
        end_idx = open_idx
        while end_idx < len(content):
            ch = content[end_idx]
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    break
            end_idx += 1

        if depth != 0:
            pos = start + len(marker)
            continue

        body = content[open_idx + 1 : end_idx]
        blocks.append((name, body))
        pos = end_idx + 1

    return blocks


def _extract_top_level_blocks(content: str, kind: str) -> list[tuple[str, str]]:
    """Extract only top-level blocks of a given kind (not nested in braces)."""
    blocks: list[tuple[str, str]] = []
    marker = f"{kind} "
    pos = 0
    depth = 0

    while pos < len(content):
        ch = content[pos]
        if ch == "{":
            depth += 1
            pos += 1
            continue
        if ch == "}":
            depth = max(0, depth - 1)
            pos += 1
            continue

        if depth == 0 and content.startswith(marker, pos):
            name_match = re.match(rf"{kind}\s+(\w+)\s*\{{", content[pos:])
            if not name_match:
                pos += len(marker)
                continue

            name = name_match.group(1)
            open_idx = pos + name_match.end() - 1
            block_depth = 0
            end_idx = open_idx
            while end_idx < len(content):
                block_ch = content[end_idx]
                if block_ch == "{":
                    block_depth += 1
                elif block_ch == "}":
                    block_depth -= 1
                    if block_depth == 0:
                        break
                end_idx += 1

            if block_depth != 0:
                pos += len(marker)
                continue

            body = content[open_idx + 1 : end_idx]
            blocks.append((name, body))
            pos = end_idx + 1
            continue

        pos += 1

    return blocks


def parse_proto_file(filepath: str) -> dict:
    """Parse a .proto file, returning package, imports, and messages."""
    content = _strip_comments(Path(filepath).read_text(encoding="utf-8"))
    result = {"package": "", "messages": {}, "imports": []}

    pkg = re.search(r"\bpackage\s+([\w.]+)\s*;", content)
    if pkg:
        result["package"] = pkg.group(1)

    result["imports"] = re.findall(r"\bimport\s+\"([^\"]+)\"\s*;", content)

    for msg_name, msg_body in _extract_blocks(content, "message"):
        fields = []
        enums = {}

        for enum_name, enum_body in _extract_blocks(msg_body, "enum"):
            values = {}
            for value_name, number in re.findall(r"\b(\w+)\s*=\s*(-?\d+)\s*;", enum_body):
                values[value_name] = int(number)
            enums[enum_name] = values

        field_pattern = re.compile(
            r"\b(required|optional|repeated)\s+([\w.]+)\s+(\w+)\s*=\s*(\d+)\s*(\[[^\]]*\])?\s*;"
        )

        for match in field_pattern.finditer(msg_body):
            cardinality = match.group(1)
            field_type = match.group(2)
            field_name = match.group(3)
            field_num = int(match.group(4))
            options = match.group(5) or ""

            if "packed" in options and "true" in options:
                cardinality = "packed"

            clean_type = field_type.split(".")[-1]
            message_type = None if clean_type in BASE_TYPES else clean_type

            fields.append(
                {
                    "number": field_num,
                    "name": field_name,
                    "type": clean_type if clean_type in BASE_TYPES else clean_type,
                    "cardinality": cardinality,
                    "message_type": message_type,
                }
            )

        oneof_field_pattern = re.compile(r"\b([\w.]+)\s+(\w+)\s*=\s*(\d+)\s*(\[[^\]]*\])?\s*;")
        for _, oneof_body in _extract_blocks(msg_body, "oneof"):
            for match in oneof_field_pattern.finditer(oneof_body):
                field_type = match.group(1)
                field_name = match.group(2)
                field_num = int(match.group(3))

                clean_type = field_type.split(".")[-1]
                message_type = None if clean_type in BASE_TYPES else clean_type
                fields.append(
                    {
                        "number": field_num,
                        "name": field_name,
                        "type": clean_type,
                        "cardinality": "oneof",
                        "message_type": message_type,
                    }
                )

        if not fields and enums:
            fields.append(
                {
                    "number": 1,
                    "name": "value",
                    "type": "int32",
                    "cardinality": "optional",
                    "message_type": None,
                }
            )

        result["messages"][msg_name] = {"fields": fields, "enums": enums}

    # Enum-only files are represented in APK as a synthetic wrapper message:
    # message <EnumName> { optional int32 value = 1; }
    for enum_name, enum_body in _extract_top_level_blocks(content, "enum"):
        if enum_name in result["messages"]:
            continue
        values = {}
        for value_name, number in re.findall(r"\b(\w+)\s*=\s*(-?\d+)\s*;", enum_body):
            values[value_name] = int(number)
        result["messages"][enum_name] = {
            "fields": [
                {
                    "number": 1,
                    "name": "value",
                    "type": "int32",
                    "cardinality": "optional",
                    "message_type": None,
                }
            ],
            "enums": {enum_name: values},
        }

    return result


def parse_all_protos(proto_dir: str) -> dict:
    """Parse all .proto files in a directory and flatten by message name."""
    results = {}
    for proto_file in sorted(Path(proto_dir).glob("*.proto")):
        parsed = parse_proto_file(str(proto_file))
        for msg_name, msg_data in parsed["messages"].items():
            results[msg_name] = {
                "file": proto_file.name,
                "fields": msg_data["fields"],
                "enums": msg_data["enums"],
                "package": parsed["package"],
            }
    return results
