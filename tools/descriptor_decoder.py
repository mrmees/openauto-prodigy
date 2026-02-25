#!/usr/bin/env python3
"""
Decode protobuf-lite descriptor strings from decompiled APK Java classes.

Replicates the logic in aaah.java (the schema compiler) to extract:
- Wire field numbers
- Field types (singular/repeated/packed/oneof)
- Object[] array mapping (which Java field each wire field maps to)
- Message class references and enum verifiers

Usage:
    python3 descriptor_decoder.py --class wbw vys wcz
    python3 descriptor_decoder.py --relevant           # all AA-related classes
    python3 descriptor_decoder.py --all                # every proto class in APK
    python3 descriptor_decoder.py --json --class wbw   # JSON output
"""

import re
import sys
import os
import json
from pathlib import Path

# APK decompiled source directory
APK_DIR = "/home/matt/claude/personal/openautopro/firmware/android-auto-apk/decompiled/sources/defpackage"

# Protobuf-lite base type names (0-17)
BASE_TYPE_NAMES = {
    0: "double", 1: "float", 2: "int64", 3: "uint64",
    4: "int32", 5: "fixed64", 6: "fixed32", 7: "bool",
    8: "string", 9: "message", 10: "bytes", 11: "uint32",
    12: "enum", 13: "sfixed32", 14: "sfixed64",
    15: "sint32", 16: "sint64", 17: "group",
}

# Protobuf wire types for each base type
WIRE_TYPES = {
    0: 1, 1: 5, 2: 0, 3: 0, 4: 0, 5: 1, 6: 5, 7: 0,
    8: 2, 9: 2, 10: 2, 11: 0, 12: 0, 13: 5, 14: 1,
    15: 0, 16: 0, 17: 2,
}


# Packed type codes skip non-packable types (string, message, bytes, group)
PACKED_TO_BASE = {
    35: 0, 36: 1, 37: 2, 38: 3, 39: 4, 40: 5, 41: 6, 42: 7,
    43: 11, 44: 12, 45: 13, 46: 14, 47: 15, 48: 16,
}


def get_cardinality(type_code):
    """Return (base_type, cardinality) from the raw type code (i88).

    From protobuf-java-lite FieldType enum:
      0-17:  singular (optional/required)
      18-34: repeated (LIST) — base = type_code - 18
      35-48: repeated packed (LIST_PACKED) — lookup table (skips non-packable)
      49:    MAP
      50:    repeated GROUP
      51+:   ONEOF — base = type_code - 51
    """
    if type_code >= 51:
        return type_code - 51, "oneof"
    elif type_code == 50:
        return 17, "repeated"  # group list → base type GROUP(17)
    elif type_code == 49:
        return -1, "map"
    elif type_code >= 35:
        base = PACKED_TO_BASE.get(type_code, type_code - 35)
        return base, "packed"
    elif type_code >= 18:
        return type_code - 18, "repeated"
    else:
        return type_code, "singular"


def decode_java_string_literal(raw):
    """Decode Java string escape sequences into a list of Unicode codepoints.

    The input is the raw text between double quotes as it appears in the .java file.
    Java escapes (\\u####, \\t, \\b, \\n, \\f, \\r, \\\\, \\") are decoded.
    Actual Unicode characters (like ᔄ U+1504) are passed through as-is.
    """
    codepoints = []
    i = 0
    while i < len(raw):
        if raw[i] == '\\' and i + 1 < len(raw):
            c = raw[i + 1]
            if c == 'u' and i + 5 < len(raw):
                codepoints.append(int(raw[i+2:i+6], 16))
                i += 6
            elif c == 'b': codepoints.append(8); i += 2
            elif c == 't': codepoints.append(9); i += 2
            elif c == 'n': codepoints.append(10); i += 2
            elif c == 'f': codepoints.append(12); i += 2
            elif c == 'r': codepoints.append(13); i += 2
            elif c == '"': codepoints.append(34); i += 2
            elif c == "'": codepoints.append(39); i += 2
            elif c == '\\': codepoints.append(92); i += 2
            else: codepoints.append(ord(c)); i += 2
        else:
            codepoints.append(ord(raw[i]))
            i += 1
    return codepoints


def extract_from_java(filepath):
    """Extract descriptor string, Object[], and class info from a Java source file."""
    with open(filepath, 'r') as f:
        content = f.read()

    # Extract class name
    class_match = re.search(r'public final class (\w+)', content)
    class_name = class_match.group(1) if class_match else Path(filepath).stem

    # Must have aaaj constructor (proto message class marker)
    if 'aaaj' not in content:
        return None

    # Extract descriptor string - find the text between quotes in aaaj constructor
    m = re.search(r'new defpackage\.aaaj\([^,]+,\s*"', content)
    if not m:
        return None

    # Walk from the opening quote to find the closing quote (handle escaped quotes)
    start = m.end()
    i = start
    while i < len(content):
        if content[i] == '\\' and i + 1 < len(content):
            i += 2  # skip escape sequence
        elif content[i] == '"':
            break
        else:
            i += 1
    raw_desc = content[start:i]
    codepoints = decode_java_string_literal(raw_desc)

    # Extract Object[] items
    obj_match = re.search(r'new java\.lang\.Object\[\]\{([^}]+)\}', content)
    obj_items = []
    if obj_match:
        raw_obj = obj_match.group(1)
        for m2 in re.finditer(r'"([^"]+)"|defpackage\.(\w+)\.class|defpackage\.(\w+)\.(\w+)', raw_obj):
            if m2.group(1):
                obj_items.append({"type": "string", "value": m2.group(1)})
            elif m2.group(2):
                obj_items.append({"type": "class", "value": m2.group(2)})
            elif m2.group(3):
                obj_items.append({"type": "field_ref", "class": m2.group(3), "field": m2.group(4)})

    # Extract Java field type declarations for cross-reference
    java_fields = {}
    for m3 in re.finditer(r'(?:public|private)\s+(?:final\s+)?(\S+)\s+(\w+)\s*[;=]', content):
        field_type = m3.group(1)
        field_name = m3.group(2)
        # Only single-letter fields (obfuscated protobuf fields)
        if len(field_name) == 1 and field_name.isalpha():
            java_fields[field_name] = field_type

    return {
        "class_name": class_name,
        "codepoints": codepoints,
        "object_array": obj_items,
        "java_fields": java_fields,
    }


def decode_descriptor(data):
    """Decode a protobuf-lite descriptor into field mappings.

    Replicates aaah.java's a(Class) method logic.

    Header format (all values are varints):
      [version_prefix] field_count charAt16 charAt17 charAt18
      charAt19 field_count2 i36 charAt20 charAt21

    Per-field format:
      wire_field_number  type_flags  [has_bits_value]

    has_bits_value is present only when (type_flags & 0xFF) <= 17
    (singular/optional fields). Repeated, packed, and oneof fields
    don't track has-bits.
    """
    codepoints = data["codepoints"]
    obj_arr = data["object_array"]
    class_name = data["class_name"]
    java_fields = data.get("java_fields", {})

    pos = 0
    version_prefix = codepoints[0] if codepoints else 0

    # Skip version/flags prefix.
    # aaah.java: charAt(0) is consumed. If >= 0xD800, read continuation chars.
    # Then i = next position. charAt15 = charAt(i) = field count.
    if codepoints[pos] >= 0xD800:
        pos += 1
        while pos < len(codepoints) and codepoints[pos] >= 0xD800:
            pos += 1
    else:
        pos += 1  # skip single version char

    # Read field count (charAt15)
    field_count = codepoints[pos]; pos += 1

    if field_count == 0:
        return {"class_name": class_name, "fields": [], "header": {"field_count": 0}}

    # Read 8 more header varints
    charAt16 = codepoints[pos]; pos += 1  # has-bits field count
    charAt17 = codepoints[pos]; pos += 1  # oneof field count
    charAt18 = codepoints[pos]; pos += 1  # check-init related
    charAt19 = codepoints[pos]; pos += 1  # map field count
    field_count2 = codepoints[pos]; pos += 1  # field count (redundant)
    i36 = codepoints[pos]; pos += 1
    charAt20 = codepoints[pos]; pos += 1
    charAt21 = codepoints[pos]; pos += 1

    header = {
        "field_count": field_count,
        "has_bits_count": charAt16,
        "oneof_count": charAt17,
        "check_init": charAt18,
        "map_fields": charAt19,
        "field_count2": field_count2,
        "i36": i36,
        "charAt20": charAt20,
        "charAt21": charAt21,
    }

    # Object[] index: skip has-bits metadata (2 per has-bits field) and oneof metadata
    obj_idx = charAt16 * 2 + charAt17

    fields = []

    for _ in range(field_count):
        if pos + 1 >= len(codepoints):
            fields.append({"error": f"Ran out of descriptor data at pos {pos}"})
            break

        wire_field = codepoints[pos]; pos += 1
        type_flags = codepoints[pos]; pos += 1

        i88 = type_flags & 0xFF
        base_type, cardinality = get_cardinality(i88)
        type_name = BASE_TYPE_NAMES.get(base_type, f"unknown({base_type})")

        field_info = {
            "wire_field": wire_field,
            "type": type_name,
            "cardinality": cardinality,
            "type_code": i88,
            "base_type": base_type,
            "type_flags_raw": type_flags,
        }

        # Flag bits in type_flags
        field_info["flags"] = {
            "bit8_required": bool(type_flags & 256),
            "bit10": bool(type_flags & 1024),
            "bit11_packed": bool(type_flags & 2048),
            "bit12_no_hasbits": bool(type_flags & 4096),
        }

        # Determine oneof style:
        # - Old-style (all versions): type_code >= 51, base = type_code - 51
        # - New-style (version >= 2): bit12 (4096) set on singular field (i88 <= 17)
        is_new_style_oneof = (
            version_prefix >= 2 and i88 < 51 and i88 <= 17 and bool(type_flags & 4096)
        )

        # Oneof fields — old-style (i88 >= 51)
        if i88 >= 51:
            # Oneof: read oneof discriminator varint
            oneof_idx = codepoints[pos]; pos += 1
            field_info["oneof_idx"] = oneof_idx

            # Object[] consumption for oneofs
            oneof_base = i88 - 51
            if oneof_base in (9, 17):  # message/group in oneof
                if obj_idx < len(obj_arr):
                    field_info["message_class"] = _obj_str(obj_arr[obj_idx])
                obj_idx += 1
            elif oneof_base == 12:  # enum in oneof
                if obj_idx < len(obj_arr):
                    field_info["enum_verifier"] = _obj_str(obj_arr[obj_idx])
                obj_idx += 1
        else:
            # Normal field (or new-style oneof): read field name from Object[]
            if obj_idx < len(obj_arr):
                entry = obj_arr[obj_idx]
                field_info["java_field"] = _obj_str(entry)
                # Cross-reference with Java field declarations
                if entry.get("type") == "string" and entry["value"] in java_fields:
                    field_info["java_type"] = java_fields[entry["value"]]
            field_info["obj_idx"] = obj_idx
            obj_idx += 1

            # Extra Object[] entries for certain types
            if i88 == 27:
                # Repeated message: +1 for class reference
                if obj_idx < len(obj_arr):
                    field_info["message_class"] = _obj_str(obj_arr[obj_idx])
                obj_idx += 1
            elif i88 == 49:
                # Map: +1 for map entry class
                if obj_idx < len(obj_arr):
                    field_info["map_class"] = _obj_str(obj_arr[obj_idx])
                obj_idx += 1
            elif i88 in (9, 17):
                # Singular message/group: class inferred from Java field type
                pass
            elif i88 in (12, 30, 44):
                # Enum variants (singular=12, repeated=30, packed=44): +1 for verifier
                # In proto2, always present. In proto3, only if 2048 flag set.
                if obj_idx < len(obj_arr):
                    field_info["enum_verifier"] = _obj_str(obj_arr[obj_idx])
                obj_idx += 1
            elif i88 == 50:
                # Group variant: +1 or +2 entries
                obj_idx += 1
                if type_flags & 2048:
                    obj_idx += 1

            # New-style oneof: read oneof_idx after Object[] consumption
            if is_new_style_oneof:
                if pos < len(codepoints):
                    oneof_idx = codepoints[pos]; pos += 1
                    field_info["oneof_idx"] = oneof_idx
                field_info["cardinality"] = "oneof"

            # Has-bits handling varies by descriptor version:
            # - Version >= 2: has_bits are NEVER inline (stored differently)
            # - Version 0 with has_bits_count == 0: no has_bits
            # - Version 1: has_bits ALWAYS present for singular fields (i88 <= 17)
            #   (bit12 has different meaning in v1; does NOT control has_bits)
            if not is_new_style_oneof:
                if version_prefix >= 2:
                    should_read_has_bits = False
                elif version_prefix == 0 and charAt16 == 0:
                    should_read_has_bits = False
                else:
                    should_read_has_bits = i88 <= 17

                if should_read_has_bits:
                    if pos < len(codepoints):
                        has_bits_val = codepoints[pos]; pos += 1
                        field_info["has_bits"] = {
                            "word": has_bits_val // 32,
                            "bit": has_bits_val % 32,
                            "raw": has_bits_val,
                        }

        fields.append(field_info)

    return {
        "class_name": class_name,
        "fields": fields,
        "header": header,
        "obj_array": [_obj_str(x) for x in obj_arr],
        "java_fields": java_fields,
    }


def _obj_str(entry):
    """Convert an Object[] entry to a readable string."""
    if isinstance(entry, dict):
        if entry["type"] == "string":
            return entry["value"]
        elif entry["type"] == "class":
            return f"{entry['value']}.class"
        elif entry["type"] == "field_ref":
            return f"{entry['class']}.{entry['field']}"
    return str(entry)


def format_result(result, verbose=False):
    """Format a decode result for human reading."""
    lines = []
    hdr = result["header"]
    lines.append(f"\n{'='*70}")
    lines.append(f"Class: {result['class_name']}  ({hdr['field_count']} fields)")
    if verbose:
        lines.append(f"Object[]: {result.get('obj_array', [])}")
        lines.append(f"Java fields: {result.get('java_fields', {})}")
    lines.append(f"{'='*70}")

    for f in result["fields"]:
        if "error" in f:
            lines.append(f"  ERROR: {f['error']}")
            continue

        wire = f["wire_field"]
        card = f["cardinality"]
        tname = f["type"]

        prefix = ""
        if card == "repeated": prefix = "repeated "
        elif card == "packed": prefix = "packed "
        elif card == "oneof": prefix = "oneof "

        java = f.get("java_field", "?")
        java_type = f.get("java_type", "")

        extra = ""
        if "message_class" in f:
            extra = f"  [{f['message_class']}]"
        elif "enum_verifier" in f:
            extra = f"  [{f['enum_verifier']}]"
        elif "map_class" in f:
            extra = f"  [{f['map_class']}]"

        has_str = ""
        if "has_bits" in f:
            h = f["has_bits"]
            has_str = f"  (has_bit {h['word']}:{h['bit']})"

        type_hint = ""
        if java_type and java_type not in ("int", "long", "boolean", "float", "double", "byte"):
            type_hint = f"  java:{java_type}"

        lines.append(f"  wire={wire:>3}  {prefix}{tname:<16} field={java}{extra}{has_str}{type_hint}")

    return "\n".join(lines)


def decode_class(class_name):
    """Decode a specific APK class by name."""
    filepath = os.path.join(APK_DIR, f"{class_name}.java")
    if not os.path.exists(filepath):
        print(f"File not found: {filepath}", file=sys.stderr)
        return None

    data = extract_from_java(filepath)
    if not data:
        print(f"No aaaj descriptor found in {class_name}.java", file=sys.stderr)
        return None

    result = decode_descriptor(data)
    return result


# ---- Classes relevant to Android Auto protocol ----

# Top-level: ChannelDescriptor and its sub-channel types
RELEVANT_CLASSES = [
    # Top-level channel descriptor
    "wbw",   # ChannelDescriptor

    # Sub-channel message types (from wbw Object[])
    "wbu",   # SensorChannel
    "vys",   # AVChannel
    "vya",   # InputChannel
    "vyt",   # AVInputChannel
    "vwc",   # BluetoothChannel
    "way",   # WiFiProjection (new in v16.1)
    "vzr",   # NavigationChannel
    "wcv",   # MediaInfoChannel
    "wdh",   # WiFiChannel
    "vwo",   # CarControl
    "vyr",   # PhoneStatusChannel
    "wah",   # NotificationChannel
    "vym",   # MediaBrowser
    "vxq",   # AudioFocus (new in v16.1)
    "vwt",   # Radio?
    "vwd",   # VendorExtension?

    # Nested message types
    "wbt",   # SensorType entry (in SensorChannel)
    "vvp",   # AudioConfig (in AVChannel)
    "wcz",   # VideoConfig (in AVChannel)
    "vxz",   # TouchScreenConfig (in InputChannel)
    "vxy",   # TouchConfig variant?
    "vyv",   # NavigationImageOptions (in NavigationChannel)
]


def main():
    verbose = "--verbose" in sys.argv or "-v" in sys.argv
    json_output = "--json" in sys.argv
    all_args = sys.argv[1:]

    if len(all_args) == 0:
        print("Usage: descriptor_decoder.py [--verbose] [--json] --class <name> [name2 ...]")
        print("       descriptor_decoder.py --relevant     # AA-related classes")
        print("       descriptor_decoder.py --all          # every proto class")
        sys.exit(1)

    if "--relevant" in all_args:
        classes = RELEVANT_CLASSES
    elif "--all" in all_args:
        classes = []
        for f in sorted(Path(APK_DIR).glob("*.java")):
            data = extract_from_java(str(f))
            if data:
                classes.append(f.stem)
    elif "--class" in all_args:
        idx = all_args.index("--class")
        classes = [a for a in all_args[idx+1:] if not a.startswith("-")]
    else:
        classes = [a for a in all_args if not a.startswith("-")]

    results = []
    for cls in classes:
        result = decode_class(cls)
        if result:
            results.append(result)
            if not json_output:
                print(format_result(result, verbose))

    if json_output:
        # Clean up for JSON output
        for r in results:
            for f in r["fields"]:
                # Remove internal tracking fields
                for k in ["type_flags_raw", "obj_idx", "type_code", "base_type"]:
                    f.pop(k, None)
        print(json.dumps(results, indent=2))

    if not json_output and len(results) > 1:
        print(f"\n{'='*70}")
        print(f"Decoded {len(results)} classes")


if __name__ == "__main__":
    main()
