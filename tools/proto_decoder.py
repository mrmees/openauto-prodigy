#!/usr/bin/env python3
"""
Protobuf-Lite Descriptor Decoder for Android Auto APK

Decodes obfuscated protobuf-lite message and enum classes from a decompiled
Android Auto APK. Extracts field numbers, types, and enum values by parsing
the RawMessageInfo descriptor string format used by Google's protobuf-java-lite.

Usage:
    python3 proto_decoder.py <apk_source_dir> [--output-dir <dir>] [--our-protos <dir>]
"""

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Optional


# ── Protobuf field type constants ──

FIELD_TYPES = {
    0: "double", 1: "float", 2: "int64", 3: "uint64", 4: "int32",
    5: "fixed64", 6: "fixed32", 7: "bool", 8: "string", 9: "message",
    10: "bytes", 11: "uint32", 12: "enum", 13: "sfixed32", 14: "sfixed64",
    15: "sint32", 16: "sint64", 17: "group",
}
REPEATED_OFFSET = 18  # types 18-35 are repeated versions of 0-17
MAP_OFFSET = 36       # types 36-50 are map entry versions
ONEOF_OFFSET = 51     # types 51+ are oneof variants

HAS_HAS_BIT = 0x1000
IS_REQUIRED = 0x100
IS_UTF8 = 0x200
NEEDS_CHECK_INIT = 0x400
IS_MAP_ENTRY = 0x800


# ── Data classes ──

@dataclass
class EnumValue:
    name: str
    number: int

@dataclass
class EnumDef:
    java_class: str
    values: list  # list of EnumValue

@dataclass
class FieldDef:
    number: int
    name: str           # Java field name (e.g. "c", "d")
    proto_type: str     # "int32", "enum", "message", etc.
    repeated: bool
    required: bool
    has_has_bit: bool
    has_bits_index: int
    enum_verifier: str  # obfuscated verifier ref (e.g. "vvs.u")
    enum_class: str     # resolved enum class (e.g. "vyn")
    message_class: str  # resolved message class (e.g. "vvp")
    oneof_index: int    # -1 if not oneof

@dataclass
class MessageDef:
    java_class: str
    field_count: int
    min_field: int
    max_field: int
    fields: list        # list of FieldDef
    java_field_types: dict  # java field name → declared type class
    parse_error: str = ""

@dataclass
class VerifierEntry:
    field_name: str     # static field name (e.g. "u")
    case_number: int
    enum_class: str     # resolved enum class

@dataclass
class VerifierFactory:
    java_class: str
    entries: list       # list of VerifierEntry


# ── Descriptor string parser ──

def read_varint(chars, pos):
    """Read a variable-length integer from the descriptor character array."""
    if pos >= len(chars):
        raise ValueError(f"read_varint: pos {pos} past end of {len(chars)} chars")
    c = chars[pos]
    pos += 1
    if c < 0xD800:
        return c, pos
    value = c & 0x1FFF
    shift = 13
    while pos < len(chars):
        c = chars[pos]
        pos += 1
        if c < 0xD800:
            return value | (c << shift), pos
        value |= (c & 0x1FFF) << shift
        shift += 13
    raise ValueError("Unterminated varint in descriptor string")


def decode_descriptor(desc_str, objects):
    """
    Decode a RawMessageInfo descriptor string + Object[] array into field definitions.

    Returns (fields, header_info, error_string).
    """
    chars = [ord(c) for c in desc_str]
    pos = 0

    # Read header (10 values for proto2)
    try:
        flags, pos = read_varint(chars, pos)
        field_count, pos = read_varint(chars, pos)
        if field_count == 0:
            return [], {"field_count": 0}, ""
        oneof_count, pos = read_varint(chars, pos)
        has_bits_count, pos = read_varint(chars, pos)
        min_field, pos = read_varint(chars, pos)
        max_field, pos = read_varint(chars, pos)
        num_entries, pos = read_varint(chars, pos)
        map_field_count, pos = read_varint(chars, pos)
        repeat_field_count, pos = read_varint(chars, pos)
        check_init_count, pos = read_varint(chars, pos)
    except (ValueError, IndexError) as e:
        return [], {}, f"Header parse error: {e}"

    header = {
        "flags": flags,
        "field_count": field_count,
        "oneof_count": oneof_count,
        "has_bits_count": has_bits_count,
        "min_field": min_field,
        "max_field": max_field,
        "num_entries": num_entries,
        "map_field_count": map_field_count,
        "repeat_field_count": repeat_field_count,
        "check_init_count": check_init_count,
    }

    # Object array position tracking
    obj_pos = 0

    # Skip oneof case field names (oneofCount * 2 entries: field name + field number)
    # Actually in proto2: oneof entries consume 2 objects each (case field + number)
    obj_pos += oneof_count * 2

    # Skip hasBits field names
    obj_pos += has_bits_count

    fields = []

    try:
        for _ in range(num_entries):
            field_number, pos = read_varint(chars, pos)
            type_with_extra, pos = read_varint(chars, pos)

            raw_type = type_with_extra & 0xFF
            has_has_bit = (type_with_extra & HAS_HAS_BIT) != 0
            is_required = (type_with_extra & IS_REQUIRED) != 0

            # Determine base type and repetition
            repeated = False
            is_map = False
            is_oneof = False
            oneof_index = -1

            if raw_type >= ONEOF_OFFSET:
                is_oneof = True
                base_type = raw_type - ONEOF_OFFSET
            elif raw_type >= MAP_OFFSET:
                is_map = True
                base_type = raw_type - MAP_OFFSET
            elif raw_type >= REPEATED_OFFSET:
                repeated = True
                base_type = raw_type - REPEATED_OFFSET
            else:
                base_type = raw_type

            type_name = FIELD_TYPES.get(base_type, f"unknown_{base_type}")

            # Read hasBitsIndex if applicable
            has_bits_index = -1
            if has_has_bit and base_type <= 17:
                has_bits_index, pos = read_varint(chars, pos)

            # Consume Object[] entries based on field type
            field_name = ""
            enum_verifier = ""
            enum_class = ""
            message_class = ""

            if obj_pos < len(objects):
                field_name = objects[obj_pos] if isinstance(objects[obj_pos], str) else ""
                obj_pos += 1

            # Enum fields consume a verifier
            if base_type == 12:  # ENUM
                if obj_pos < len(objects):
                    verifier_ref = objects[obj_pos]
                    enum_verifier = verifier_ref if isinstance(verifier_ref, str) else ""
                    obj_pos += 1

            # Repeated message/group fields consume a class reference
            if repeated and base_type in (9, 17):  # MESSAGE or GROUP
                if obj_pos < len(objects):
                    class_ref = objects[obj_pos]
                    message_class = class_ref if isinstance(class_ref, str) else ""
                    obj_pos += 1

            # Map fields consume a default entry
            if is_map:
                if obj_pos < len(objects):
                    obj_pos += 1  # skip map default entry

            # Oneof fields: read oneof index
            if is_oneof:
                # In oneof, additional varint for oneof index
                # (simplified — may need refinement)
                pass

            fields.append(FieldDef(
                number=field_number,
                name=field_name,
                proto_type=type_name,
                repeated=repeated,
                required=is_required,
                has_has_bit=has_has_bit,
                has_bits_index=has_bits_index,
                enum_verifier=enum_verifier,
                enum_class=enum_class,
                message_class=message_class,
                oneof_index=oneof_index,
            ))
    except (ValueError, IndexError) as e:
        return fields, header, f"Entry parse error at field {len(fields)}: {e}"

    return fields, header, ""


# ── Java source parsers ──

# Regex for enum classes
RE_ENUM_CLASS = re.compile(
    r'public\s+enum\s+(\w+)\s+implements\s+defpackage\.zyx'
)
RE_ENUM_VALUE = re.compile(
    r'(\w+)\((\d+)\)'
)

# Regex for message classes
RE_MESSAGE_CLASS = re.compile(
    r'public\s+final\s+class\s+(\w+)\s+extends\s+defpackage\.zyt'
)

# Regex for verifier factory classes
RE_VERIFIER_CLASS = re.compile(
    r'(?:public\s+)?(?:final\s+)?class\s+(\w+)\s+implements\s+defpackage\.zyz'
)

# Regex for static verifier field declarations
RE_VERIFIER_FIELD = re.compile(
    r'static\s+final\s+defpackage\.zyz\s+(\w+)\s*=\s*new\s+defpackage\.(\w+)\((\d+)\)'
)

# Regex for verifier switch case → enum class
RE_VERIFIER_CASE = re.compile(
    r'case\s+(\d+):\s*\n\s*return\s+defpackage\.(\w+)\.(\w+)\(i2\)'
)
RE_VERIFIER_DEFAULT = re.compile(
    r'default:\s*\n\s*(?:if\s*\()?\s*defpackage\.(\w+)\.(\w+)\(i2\)'
)

# Regex for the aaaj constructor descriptor string
# Matches: new defpackage.aaaj(a, "...", new java.lang.Object[]{...})
RE_AAAJ = re.compile(
    r'new\s+defpackage\.aaaj\(a,\s*"([^"]*)"',
    re.DOTALL
)

# Regex for Object[] array contents
RE_OBJECT_ARRAY = re.compile(
    r'new\s+java\.lang\.Object\[\]\{([^}]*)\}',
    re.DOTALL
)

# Regex for Java field declarations (instance fields on message classes)
RE_JAVA_FIELD = re.compile(
    r'public\s+(?:defpackage\.)?(\w+)\s+(\w+)\s*;'
)
RE_JAVA_FIELD_PRIM = re.compile(
    r'public\s+(int|long|float|double|boolean|byte)\s+(\w+)'
)


def parse_enum_file(filepath):
    """Parse a Java enum class file, return EnumDef or None."""
    try:
        text = filepath.read_text(encoding='utf-8', errors='replace')
    except Exception:
        return None

    m = RE_ENUM_CLASS.search(text)
    if not m:
        return None

    class_name = m.group(1)

    # Find enum values — they appear as NAME(number) in the enum declaration
    values = []
    for vm in RE_ENUM_VALUE.finditer(text):
        name = vm.group(1)
        number = int(vm.group(2))
        # Skip constructor parameter names and method names
        if name[0].isupper() or name.startswith("MEDIA_") or name.startswith("STATUS_"):
            values.append(EnumValue(name=name, number=number))

    if not values:
        # Try alternative pattern: look in the b(int) method switch cases
        switch_pattern = re.compile(r'case\s+(\d+):\s*\n\s*return\s+(\w+);')
        for sm in switch_pattern.finditer(text):
            number = int(sm.group(1))
            name = sm.group(2)
            values.append(EnumValue(name=name, number=number))

    return EnumDef(java_class=class_name, values=values)


def parse_verifier_factory(filepath):
    """Parse a verifier factory class, return VerifierFactory or None."""
    try:
        text = filepath.read_text(encoding='utf-8', errors='replace')
    except Exception:
        return None

    m = RE_VERIFIER_CLASS.search(text)
    if not m:
        return None

    class_name = m.group(1)

    # Build field name → case number mapping
    field_to_case = {}
    for fm in RE_VERIFIER_FIELD.finditer(text):
        field_name = fm.group(1)
        case_num = int(fm.group(3))
        field_to_case[field_name] = case_num

    # Build case number → enum class mapping from switch
    case_to_enum = {}
    for cm in RE_VERIFIER_CASE.finditer(text):
        case_num = int(cm.group(1))
        enum_class = cm.group(2)
        case_to_enum[case_num] = enum_class

    # Check default case
    dm = RE_VERIFIER_DEFAULT.search(text)
    default_enum = dm.group(1) if dm else ""

    # Build entries
    entries = []
    for field_name, case_num in sorted(field_to_case.items(), key=lambda x: x[1]):
        enum_class = case_to_enum.get(case_num, default_enum if case_num == max(field_to_case.values()) else "")
        entries.append(VerifierEntry(
            field_name=field_name,
            case_number=case_num,
            enum_class=enum_class,
        ))

    return VerifierFactory(java_class=class_name, entries=entries)


def parse_object_array(text):
    """Parse the Object[] array from the aaaj constructor call."""
    m = RE_OBJECT_ARRAY.search(text)
    if not m:
        return []

    raw = m.group(1)
    objects = []

    # Tokenize the array contents
    # Elements are: "string", defpackage.Foo.class, defpackage.Foo.bar
    for token in re.finditer(r'"(\w+)"|defpackage\.(\w+)\.class|defpackage\.(\w+)\.(\w+)', raw):
        if token.group(1) is not None:
            # String literal (field name)
            objects.append(token.group(1))
        elif token.group(2) is not None:
            # Class reference (e.g., defpackage.vvp.class)
            objects.append(f"{token.group(2)}.class")
        elif token.group(3) is not None:
            # Static field reference (e.g., defpackage.vvs.u)
            objects.append(f"{token.group(3)}.{token.group(4)}")

    return objects


def decode_java_string(s):
    """
    Decode a Java string literal from source, handling Unicode escapes
    and standard escape sequences.
    """
    # The string is already decoded by the Java decompiler into Unicode chars,
    # but some escapes remain as \uXXXX, \b, \t, \n, \r, etc.
    result = []
    i = 0
    while i < len(s):
        if s[i] == '\\' and i + 1 < len(s):
            next_c = s[i + 1]
            if next_c == 'u' and i + 5 < len(s):
                # \uXXXX escape
                hex_str = s[i+2:i+6]
                try:
                    result.append(chr(int(hex_str, 16)))
                    i += 6
                    continue
                except ValueError:
                    pass
            elif next_c == 'b':
                result.append('\b')
                i += 2
                continue
            elif next_c == 't':
                result.append('\t')
                i += 2
                continue
            elif next_c == 'n':
                result.append('\n')
                i += 2
                continue
            elif next_c == 'r':
                result.append('\r')
                i += 2
                continue
            elif next_c == '\\':
                result.append('\\')
                i += 2
                continue
            elif next_c == '"':
                result.append('"')
                i += 2
                continue
        result.append(s[i])
        i += 1
    return ''.join(result)


def parse_message_file(filepath):
    """Parse a Java message class file, return MessageDef or None."""
    try:
        text = filepath.read_text(encoding='utf-8', errors='replace')
    except Exception:
        return None

    m = RE_MESSAGE_CLASS.search(text)
    if not m:
        return None

    class_name = m.group(1)

    # Find the aaaj constructor call
    aaaj_match = RE_AAAJ.search(text)
    if not aaaj_match:
        return MessageDef(
            java_class=class_name, field_count=0, min_field=0, max_field=0,
            fields=[], java_field_types={},
            parse_error="No aaaj constructor found"
        )

    # Decode the descriptor string
    raw_desc = aaaj_match.group(1)
    desc_str = decode_java_string(raw_desc)

    # Parse Object[] array
    objects = parse_object_array(text)

    # Parse Java field declarations for type resolution
    java_field_types = {}
    for fm in RE_JAVA_FIELD.finditer(text):
        type_class = fm.group(1)
        field_name = fm.group(2)
        java_field_types[field_name] = type_class

    # Decode the descriptor
    fields, header, error = decode_descriptor(desc_str, objects)

    # Resolve message types for singular MESSAGE fields from Java declarations
    for f in fields:
        if f.proto_type == "message" and not f.repeated and not f.message_class:
            if f.name in java_field_types:
                f.message_class = java_field_types[f.name]

    return MessageDef(
        java_class=class_name,
        field_count=header.get("field_count", 0),
        min_field=header.get("min_field", 0),
        max_field=header.get("max_field", 0),
        fields=fields,
        java_field_types=java_field_types,
        parse_error=error,
    )


# ── Scanning ──

def scan_directory(source_dir):
    """Scan all Java files in the directory and classify them."""
    source_path = Path(source_dir)
    defpackage = source_path / "defpackage"

    if not defpackage.exists():
        # Try source_dir directly
        defpackage = source_path

    enums = {}          # class_name → EnumDef
    verifiers = {}      # class_name → VerifierFactory
    messages = {}       # class_name → MessageDef

    java_files = list(defpackage.glob("*.java"))
    total = len(java_files)

    print(f"Scanning {total} Java files in {defpackage}...")

    errors = []

    for i, filepath in enumerate(java_files):
        if (i + 1) % 2000 == 0 or i + 1 == total:
            print(f"  [{i+1}/{total}] scanned...")

        try:
            text = filepath.read_text(encoding='utf-8', errors='replace')
        except Exception:
            continue

        # Quick classification before full parsing
        if 'implements defpackage.zyx' in text and 'public enum' in text:
            result = parse_enum_file(filepath)
            if result:
                enums[result.java_class] = result

        elif 'implements defpackage.zyz' in text and 'new defpackage.' in text:
            result = parse_verifier_factory(filepath)
            if result:
                verifiers[result.java_class] = result

        elif 'extends defpackage.zyt' in text:
            result = parse_message_file(filepath)
            if result:
                if result.parse_error:
                    errors.append((result.java_class, result.parse_error))
                messages[result.java_class] = result

    print(f"\nScan complete:")
    print(f"  Enums:      {len(enums)}")
    print(f"  Verifiers:  {len(verifiers)}")
    print(f"  Messages:   {len(messages)}")
    print(f"  Errors:     {len(errors)}")

    if errors:
        print(f"\nFirst 10 errors:")
        for cls, err in errors[:10]:
            print(f"  {cls}: {err}")

    return enums, verifiers, messages, errors


# ── Verifier resolution ──

def resolve_verifiers(messages, verifiers, enums):
    """
    Resolve enum verifier references in message fields.
    e.g., "vvs.u" → verifier factory "vvs", field "u" → case 20 → enum class "vyn"
    """
    resolved = 0
    unresolved = []

    for msg in messages.values():
        for f in msg.fields:
            if not f.enum_verifier:
                continue

            # Parse "factory.field" format
            parts = f.enum_verifier.split(".")
            if len(parts) != 2:
                unresolved.append((msg.java_class, f.number, f.enum_verifier))
                continue

            factory_class, field_name = parts
            factory = verifiers.get(factory_class)
            if not factory:
                unresolved.append((msg.java_class, f.number, f.enum_verifier))
                continue

            # Find the entry for this field
            entry = None
            for e in factory.entries:
                if e.field_name == field_name:
                    entry = e
                    break

            if entry and entry.enum_class:
                f.enum_class = entry.enum_class
                resolved += 1
            else:
                unresolved.append((msg.java_class, f.number, f.enum_verifier))

    # Also resolve .class references for repeated message fields
    for msg in messages.values():
        for f in msg.fields:
            if f.message_class and f.message_class.endswith(".class"):
                f.message_class = f.message_class[:-6]  # strip ".class"

    print(f"\nVerifier resolution:")
    print(f"  Resolved:   {resolved}")
    print(f"  Unresolved: {len(unresolved)}")
    if unresolved[:5]:
        for cls, fnum, ref in unresolved[:5]:
            print(f"    {cls} field {fnum}: {ref}")

    return unresolved


# ── Our proto parser (for structural matching) ──

@dataclass
class OurField:
    number: int
    name: str
    type: str
    repeated: bool
    required: bool

@dataclass
class OurProto:
    filename: str
    message_name: str
    package: str
    fields: list  # list of OurField

RE_PROTO_MSG = re.compile(r'message\s+(\w+)\s*\{')
RE_PROTO_PKG = re.compile(r'package\s+([\w.]+)\s*;')
RE_PROTO_FIELD = re.compile(
    r'(required|optional|repeated)\s+([\w.]+)\s+(\w+)\s*=\s*(\d+)'
)


def parse_our_protos(proto_dir):
    """Parse our .proto files for structural matching."""
    protos = {}
    proto_path = Path(proto_dir)

    for filepath in proto_path.glob("*.proto"):
        try:
            text = filepath.read_text()
        except Exception:
            continue

        pkg_match = RE_PROTO_PKG.search(text)
        package = pkg_match.group(1) if pkg_match else ""

        msg_match = RE_PROTO_MSG.search(text)
        if not msg_match:
            continue

        message_name = msg_match.group(1)

        fields = []
        for fm in RE_PROTO_FIELD.finditer(text):
            modifier = fm.group(1)
            field_type = fm.group(2)
            field_name = fm.group(3)
            field_number = int(fm.group(4))

            # Normalize type
            field_type = field_type.split(".")[-1]  # strip package prefix
            if field_type == "Enum":
                field_type = "enum"

            fields.append(OurField(
                number=field_number,
                name=field_name,
                type=field_type,
                repeated=(modifier == "repeated"),
                required=(modifier == "required"),
            ))

        protos[filepath.stem] = OurProto(
            filename=filepath.name,
            message_name=message_name,
            package=package,
            fields=fields,
        )

    return protos


# ── Structural matching ──

def match_protos(our_protos, apk_messages, apk_enums):
    """
    Match APK messages to our protos by structural fingerprint.
    Returns list of (our_proto_name, apk_class, match_score, diffs).
    """
    matches = []

    for our_name, our_proto in our_protos.items():
        if not our_proto.fields:
            continue

        our_fingerprint = {f.number: f for f in our_proto.fields}
        best_match = None
        best_score = 0

        for apk_class, apk_msg in apk_messages.items():
            if not apk_msg.fields:
                continue

            apk_fingerprint = {f.number: f for f in apk_msg.fields}

            # Count matching field numbers with compatible types
            matching = 0
            total = len(our_fingerprint)

            for fnum, our_field in our_fingerprint.items():
                if fnum in apk_fingerprint:
                    apk_field = apk_fingerprint[fnum]
                    # Check type compatibility
                    our_type = our_field.type.lower()
                    apk_type = apk_field.proto_type.lower()

                    # Normalize for comparison
                    type_compatible = (
                        our_type == apk_type or
                        (our_type == "enum" and apk_type == "enum") or
                        (our_type in ("int32", "uint32", "sint32") and apk_type in ("int32", "uint32", "sint32")) or
                        (our_type in ("int64", "uint64", "sint64") and apk_type in ("int64", "uint64", "sint64")) or
                        # Our proto types like "AudioConfig" match apk "message"
                        (apk_type == "message" and our_type not in FIELD_TYPES.values()) or
                        (our_type == "bool" and apk_type == "bool") or
                        (our_type == "string" and apk_type == "string") or
                        (our_type == "bytes" and apk_type == "bytes")
                    )

                    rep_match = our_field.repeated == apk_field.repeated

                    if type_compatible and rep_match:
                        matching += 1

            if total > 0:
                score = matching / total
            else:
                score = 0

            if score > best_score:
                best_score = score
                best_match = apk_class

        if best_match and best_score >= 0.5:
            # Generate diffs
            apk_msg = apk_messages[best_match]
            apk_fields = {f.number: f for f in apk_msg.fields}
            our_fields = {f.number: f for f in our_proto.fields}

            diffs = []
            # Fields in APK but not ours (new fields)
            for fnum, af in sorted(apk_fields.items()):
                if fnum not in our_fields:
                    diffs.append(f"+ field {fnum}: {af.proto_type} {af.name}"
                                 + (f" (enum: {af.enum_class})" if af.enum_class else "")
                                 + (f" (msg: {af.message_class})" if af.message_class else ""))

            # Fields in ours but not APK (removed fields)
            for fnum, of in sorted(our_fields.items()):
                if fnum not in apk_fields:
                    diffs.append(f"- field {fnum}: {of.type} {of.name}")

            matches.append({
                "our_proto": our_name,
                "our_message": our_proto.message_name,
                "apk_class": best_match,
                "score": round(best_score, 3),
                "our_field_count": len(our_proto.fields),
                "apk_field_count": len(apk_msg.fields),
                "diffs": diffs,
            })

    # Sort by score descending
    matches.sort(key=lambda x: -x["score"])
    return matches


# ── Output generators ──

def proto_type_str(field, apk_enums, apk_messages):
    """Generate a proto type string for a field."""
    if field.proto_type == "enum":
        if field.enum_class and field.enum_class in apk_enums:
            return field.enum_class
        return f"int32 /* enum {field.enum_class or '?'} */"
    elif field.proto_type == "message":
        return field.message_class or "bytes /* message ? */"
    else:
        return field.proto_type


def generate_proto_file(msg, apk_enums, apk_messages):
    """Generate a .proto file content from a decoded message."""
    lines = [
        f'// Decoded from APK class: {msg.java_class}',
        f'// Field count: {msg.field_count}, range: {msg.min_field}-{msg.max_field}',
        '',
        'syntax = "proto2";',
        '',
        f'message {msg.java_class} {{',
    ]

    for f in sorted(msg.fields, key=lambda x: x.number):
        modifier = "repeated" if f.repeated else ("required" if f.required else "optional")
        type_str = proto_type_str(f, apk_enums, apk_messages)
        comment_parts = []
        if f.enum_class:
            comment_parts.append(f"enum: {f.enum_class}")
        if f.message_class:
            comment_parts.append(f"msg: {f.message_class}")
        comment = f"  // {', '.join(comment_parts)}" if comment_parts else ""
        lines.append(f'    {modifier} {type_str} {f.name} = {f.number};{comment}')

    lines.append('}')
    if msg.parse_error:
        lines.append(f'// PARSE ERROR: {msg.parse_error}')

    return '\n'.join(lines)


def write_outputs(output_dir, enums, verifiers, messages, matches, our_protos):
    """Write all output files."""
    out = Path(output_dir)
    out.mkdir(parents=True, exist_ok=True)

    # JSON dump of everything
    json_data = {
        "summary": {
            "total_enums": len(enums),
            "total_verifiers": len(verifiers),
            "total_messages": len(messages),
            "total_matches": len(matches),
            "messages_with_errors": sum(1 for m in messages.values() if m.parse_error),
        },
        "matches": matches,
        "enums": {
            k: {"values": [asdict(v) for v in e.values]}
            for k, e in sorted(enums.items())
        },
        "messages": {
            k: {
                "field_count": m.field_count,
                "min_field": m.min_field,
                "max_field": m.max_field,
                "fields": [asdict(f) for f in m.fields],
                "parse_error": m.parse_error,
            }
            for k, m in sorted(messages.items())
        },
    }

    json_path = out / "apk_protos.json"
    with open(json_path, 'w') as f:
        json.dump(json_data, f, indent=2)
    print(f"\nWrote {json_path}")

    # Matched protos with diffs
    matched_dir = out / "matched"
    matched_dir.mkdir(exist_ok=True)

    for match in matches:
        apk_class = match["apk_class"]
        our_name = match["our_proto"]
        msg = messages[apk_class]

        proto_content = generate_proto_file(msg, enums, messages)

        # Add diff info
        if match["diffs"]:
            proto_content += "\n\n// === DIFF vs our proto ===\n"
            for d in match["diffs"]:
                proto_content += f"// {d}\n"

        filepath = matched_dir / f"{our_name}__{apk_class}.proto"
        with open(filepath, 'w') as f:
            f.write(proto_content)

    print(f"Wrote {len(matches)} matched proto files to {matched_dir}")

    # Unmatched messages
    unmatched_dir = out / "unmatched"
    unmatched_dir.mkdir(exist_ok=True)

    matched_classes = {m["apk_class"] for m in matches}
    unmatched = {k: v for k, v in messages.items() if k not in matched_classes and v.fields}

    for class_name, msg in sorted(unmatched.items()):
        proto_content = generate_proto_file(msg, enums, messages)
        filepath = unmatched_dir / f"{class_name}.proto"
        with open(filepath, 'w') as f:
            f.write(proto_content)

    print(f"Wrote {len(unmatched)} unmatched proto files to {unmatched_dir}")

    # Enums
    enum_dir = out / "enums"
    enum_dir.mkdir(exist_ok=True)

    for class_name, enum in sorted(enums.items()):
        lines = [
            f'// Decoded from APK class: {class_name}',
            '',
            'syntax = "proto2";',
            '',
            f'enum {class_name} {{',
        ]
        for v in sorted(enum.values, key=lambda x: x.number):
            lines.append(f'    {v.name} = {v.number};')
        lines.append('}')

        filepath = enum_dir / f"{class_name}.proto"
        with open(filepath, 'w') as f:
            f.write('\n'.join(lines))

    print(f"Wrote {len(enums)} enum files to {enum_dir}")

    # Summary report
    report_path = out / "summary.md"
    with open(report_path, 'w') as f:
        f.write("# APK Proto Decoder Results\n\n")
        f.write(f"- **Enums decoded:** {len(enums)}\n")
        f.write(f"- **Verifier factories:** {len(verifiers)}\n")
        f.write(f"- **Messages decoded:** {len(messages)}\n")
        f.write(f"- **Messages with errors:** {sum(1 for m in messages.values() if m.parse_error)}\n")
        f.write(f"- **Matched to our protos:** {len(matches)}\n")
        f.write(f"- **Unmatched (with fields):** {len(unmatched)}\n\n")

        if matches:
            f.write("## Matched Protos\n\n")
            f.write("| Our Proto | APK Class | Score | Our Fields | APK Fields | Diffs |\n")
            f.write("|-----------|-----------|-------|------------|------------|-------|\n")
            for m in matches:
                diff_count = len(m["diffs"])
                f.write(f"| {m['our_proto']} | {m['apk_class']} | {m['score']} | "
                        f"{m['our_field_count']} | {m['apk_field_count']} | {diff_count} |\n")

        if matches:
            f.write("\n## Diffs\n\n")
            for m in matches:
                if m["diffs"]:
                    f.write(f"### {m['our_proto']} → {m['apk_class']}\n\n")
                    for d in m["diffs"]:
                        f.write(f"- `{d}`\n")
                    f.write("\n")

    print(f"Wrote {report_path}")


# ── Main ──

def main():
    parser = argparse.ArgumentParser(description="Decode protobuf-lite descriptors from APK source")
    parser.add_argument("apk_source", help="Path to decompiled APK sources directory")
    parser.add_argument("--output-dir", "-o", default="tools/proto_decode_output",
                        help="Output directory (default: tools/proto_decode_output)")
    parser.add_argument("--our-protos", "-p", default=None,
                        help="Path to our .proto files for structural matching")
    args = parser.parse_args()

    # Phase 1: Scan
    enums, verifiers, messages, errors = scan_directory(args.apk_source)

    # Phase 2: Resolve verifier references
    unresolved = resolve_verifiers(messages, verifiers, enums)

    # Phase 3: Match against our protos (if provided)
    matches = []
    our_protos = {}
    if args.our_protos:
        our_protos = parse_our_protos(args.our_protos)
        print(f"\nParsed {len(our_protos)} of our proto files")
        matches = match_protos(our_protos, messages, enums)
        print(f"Found {len(matches)} matches (score >= 0.5)")

    # Phase 4: Output
    write_outputs(args.output_dir, enums, verifiers, messages, matches, our_protos)

    # Print error rate
    total_msgs = len(messages)
    error_msgs = sum(1 for m in messages.values() if m.parse_error)
    if total_msgs > 0:
        error_rate = error_msgs / total_msgs * 100
        print(f"\nParse error rate: {error_msgs}/{total_msgs} ({error_rate:.1f}%)")
        if error_rate > 5:
            print("⚠  Error rate exceeds 5% — consider Approach B (Java runtime decoder)")

    return 0 if error_msgs == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
