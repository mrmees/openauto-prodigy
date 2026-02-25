#!/usr/bin/env python3
"""
AA Proto Graph Walker

Builds a complete AA protocol reference by:
1. Seeding from known APK class ↔ proto name mappings (29 unique matches)
2. BFS graph walk following all sub-message and enum references
3. Scanning gearhead package Java files for additional proto roots
4. Classifying by channel, inferring names, producing reference docs

Input: tools/proto_decode_output/apk_protos.json (from proto_decoder.py)
Output:
  - tools/aa_proto_graph.json (machine-readable knowledge base)
  - docs/aa-protocol-reference.md (human-readable reference)
"""

import json
import os
import sys
import re
from collections import defaultdict, deque
from pathlib import Path

# ── Configuration ──────────────────────────────────────────────────────────

SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
PROTO_JSON = SCRIPT_DIR / "proto_decode_output" / "apk_protos.json"
OUTPUT_JSON = SCRIPT_DIR / "aa_proto_graph.json"
OUTPUT_MD = PROJECT_ROOT / "docs" / "aa-protocol-reference.md"

# Decompiled APK source location
APK_SOURCE = PROJECT_ROOT / "analysis-projection" / \
    "android_auto_16.1.660414-release_161660414" / "apk-source" / "sources"
DEFPACKAGE = APK_SOURCE / "defpackage"
GEARHEAD = APK_SOURCE / "com" / "google" / "android" / "gearhead"

# ── Seed Set: Known APK Class → Our Proto Name ────────────────────────────
# Only from unique matches (29) — no ambiguous shared matches

SEED_MATCHES = {
    # Control channel
    "aahi": {"name": "ServiceDiscoveryRequest", "channel": "control"},
    "wby": {"name": "ServiceDiscoveryResponse", "channel": "control"},
    "wbw": {"name": "ChannelDescriptor", "channel": "control"},
    "aacd": {"name": "HeadUnitInfo", "channel": "control"},
    "aajk": {"name": "ConnectionConfiguration", "channel": "control"},
    "wcg": {"name": "BindingRequest", "channel": "control"},
    "wcv": {"name": "VendorExtensionChannel", "channel": "control"},

    # Video channel
    "vys": {"name": "AVChannel", "channel": "video"},
    "wcz": {"name": "VideoConfig", "channel": "video"},
    "wdb": {"name": "VideoFocusIndication", "channel": "video"},
    "vxb": {"name": "AVChannelSetupResponse", "channel": "video"},
    "vyx": {"name": "AVInputOpenRequest", "channel": "video"},

    # Sensor channel
    "war": {"name": "SensorStartRequest", "channel": "sensor"},
    "wbo": {"name": "SensorEventIndication", "channel": "sensor"},
    "xss": {"name": "Diagnostics", "channel": "sensor"},
    "ahdz": {"name": "GPSLocation", "channel": "sensor"},

    # Input channel
    "vxx": {"name": "InputEventIndication", "channel": "input"},
    "wcj": {"name": "TouchEvent", "channel": "input"},
    "vyi": {"name": "ButtonEvent", "channel": "input"},
    "zpl": {"name": "InputChannel", "channel": "input"},

    # Bluetooth channel
    "kay": {"name": "BluetoothPairingRequest", "channel": "bluetooth"},
    "xgq": {"name": "BluetoothPairingResponse", "channel": "bluetooth"},

    # WiFi channel
    "wan": {"name": "WifiSecurityResponse", "channel": "wifi"},

    # Navigation channel
    "ahdp": {"name": "NavigationChannel", "channel": "navigation"},
    "xnb": {"name": "NavigationDistance", "channel": "navigation"},
    "utj": {"name": "NavigationStep", "channel": "navigation"},

    # Media status channel
    "nmi": {"name": "MediaPlaybackMetadata", "channel": "media"},
    "wad": {"name": "PhoneStatus", "channel": "phone"},

    # Shared/multi-channel
    "vxh": {"name": "Door", "channel": "sensor"},
    "vyk": {"name": "Light", "channel": "sensor"},
}

# Channel descriptions for documentation
# Known field names from our proto files — maps message_name -> {field_num: field_name}
# Auto-populated at startup from libs/open-androidauto/proto/*.proto
OUR_FIELD_NAMES = {}  # Populated by load_our_field_names()

# Map from APK class to our proto message name (reverse of SEED_MATCHES)
APK_TO_OUR_NAME = {k: v["name"] for k, v in SEED_MATCHES.items() if v.get("name")}

# Unresolved enum references — APK classes that aren't enums but were referenced as enum types.
# Maps APK class → description of what enum it actually represents.
# Sub-message name mappings — depth-1 APK classes matched to our proto types by structure
SUB_MESSAGE_NAMES = {
    # Sensor data types
    "vyl": "GPSLocation",
    "vxa": "Compass",
    "wcd": "Speed",
    "wbn": "RPM",
    "vzx": "Odometer",
    "vxn": "FuelLevel",
    "wab": "ParkingBrake",
    "vxp": "Gear",
    "vxf": "Diagnostics",
    "vzw": "NightMode",
    "vxk": "Environment",
    # Input types
    "wci": "TouchLocation",
    "vvi": "ButtonEvent",
    "vyj": "RelativeInputEvent",
    "wbm": "AbsoluteInputEvent",
    "vvh": "KeyEvent",               # child of ButtonEvent — keycode + action
    "wbl": "TouchCoordinate",         # child of AbsoluteInputEvent — x, y, pressure
    "zli": "TouchSensitivity",        # child of InputChannelConfig — sensitivity values
    # Audio/Video
    "vvp": "AudioConfig",
    "vyt": "AVInputChannel",
    "wcm": "AdditionalVideoConfig",   # child of VideoConfig — extended codec/resolution params
    # WiFi
    "waw": "WifiSecurity",
    "wbb": "WifiDirectConfig",
    "wam": "AccessPointType",
    # Navigation
    "vzr": "NavigationChannelConfig",
    "utk": "NavigationStepData",
    "ahdl": "NavigationImageOptions",
    "xmw": "NavigationDistanceUnit",
    "vzq": "NavigationImageDimensions",  # child of NavigationChannelConfig — image width/height
    "xnd": "DistanceLabel",           # child of NavigationDistanceDisplay — value + unit strings
    "xng": "NavigationDistanceDisplay",  # child of NavigationDistance — distance + labels
    # Control
    "zyd": "PingConfiguration",
    "wdh": "NotificationChannel",
    "way": "RadioChannel",
    "wax": "RadioChannelConfig",      # child of RadioChannel [DEPRECATED] — station config
    "wbe": "RadioStationData",        # child of RadioChannelConfig [DEPRECATED] — station data
    # ServiceDiscoveryRequest
    "aahd": "SessionInfo",
    "aagr": "PhoneCapabilities",
    "aafs": "DeviceInfo",             # child of PhoneCapabilities — phone brand/model/device strings
    "aafu": "ConnectionConfig",       # child of PhoneCapabilities — connection params + ping config
    "aaft": "PingConfigPair",         # child of ConnectionConfig — request/response ping timeouts
    "aagh": "CapabilityFlag",         # child of PhoneCapabilities — named bool flag
    "aags": "CapabilityPair",         # child of PhoneCapabilities — two bool flags
    "aagv": "CapabilityEntry",        # child of PhoneCapabilities — detailed capability descriptor
    # Input channel config
    "zpn": "InputChannelConfig",
    # Channel descriptor sub-configs (ChannelDescriptor field-specific configs)
    "vwc": "BluetoothChannelConfig",  # ChannelDescriptor field #6 — BT channel config
    "vya": "InputChannelDescriptor",  # ChannelDescriptor field #4 — input channel config
    "wbu": "SensorChannelConfig",     # ChannelDescriptor field #2 — sensor channel config
    # Gearhead SDK (not AA protocol, but discovered via graph walk)
    "nll": "AssistantFeatureFlags",   # ClientRegistrationConfig — 14 bool feature flags
    "nlq": "VersionFeatureFlags",     # SupportedVersionInfo — 13 bool feature flags
}

UNRESOLVED_ENUM_NOTES = {
    "a": "AudioStreamType (TELEPHONY=0, SYSTEM_AUDIO=1, MEDIA=3, GUIDANCE=5) — utility class, not proto enum",
    "vee": "Not a proto enum — UI utility class (View animations, scale types)",
    "viz": "VideoFPS (NONE=0, _60=1, _30=2) — builder/helper class, not proto enum",
}

# ChannelDescriptor field→channel type mapping (field number → channel name)
CHANNEL_DESCRIPTOR_FIELDS = {
    1: ("channel_id", ""),
    2: ("sensor_channel", "sensor"),
    3: ("av_channel", "video"),
    4: ("input_channel", "input"),
    5: ("av_input_channel", "avinput"),
    6: ("bluetooth_channel", "bluetooth"),
    7: ("radio_channel", ""),
    8: ("navigation_channel", "navigation"),
    9: ("media_info_channel", "media"),
    10: ("phone_status_channel", "phone"),
    11: ("unknown_channel_11", ""),
    12: ("vendor_extension_channel", ""),
    13: ("notification_channel", ""),
    14: ("wifi_channel", "wifi"),
    15: ("unknown_channel_15", ""),
}

CHANNEL_INFO = {
    "control": {
        "title": "Control Channel (0)",
        "description": "Session management, service discovery, auth, ping, shutdown, audio/nav focus",
    },
    "video": {
        "title": "Video Channel (3)",
        "description": "Video streaming config, codec negotiation, focus management, AV setup",
    },
    "audio_media": {
        "title": "Audio — Media Channel (4)",
        "description": "Music/media audio stream",
    },
    "audio_speech": {
        "title": "Audio — Speech Channel (5)",
        "description": "Voice assistant / navigation audio",
    },
    "audio_system": {
        "title": "Audio — System Channel (6)",
        "description": "System sounds (notifications, alerts)",
    },
    "input": {
        "title": "Input Channel (1)",
        "description": "Touch events, button events, relative/absolute input",
    },
    "sensor": {
        "title": "Sensor Channel (2)",
        "description": "GPS, compass, speed, RPM, night mode, driving status, diagnostics",
    },
    "bluetooth": {
        "title": "Bluetooth Channel (8)",
        "description": "BT pairing requests/responses",
    },
    "wifi": {
        "title": "WiFi Channel (14)",
        "description": "WiFi credential exchange for wireless AA",
    },
    "navigation": {
        "title": "Navigation Channel (9)",
        "description": "Turn-by-turn steps, distance/ETA, navigation state",
    },
    "media": {
        "title": "Media Status Channel (10)",
        "description": "Playback state, track metadata, album art",
    },
    "phone": {
        "title": "Phone Status Channel (11)",
        "description": "Call state, signal strength, battery",
    },
    "avinput": {
        "title": "AV Input Channel (7)",
        "description": "Microphone input for voice assistant",
    },
    "unknown": {
        "title": "Uncategorized",
        "description": "Protos found via gearhead scan or transitive reference, channel unknown",
    },
}

# ── Data Loading ──────────────────────────────────────────────────────────

def load_our_field_names():
    """Load field names from our .proto files."""
    import glob
    proto_dir = PROJECT_ROOT / "libs" / "open-androidauto" / "proto"
    for proto_file in sorted(glob.glob(str(proto_dir / "*.proto"))):
        with open(proto_file) as f:
            content = f.read()
        msg_match = re.search(r'message\s+(\w+)', content)
        if not msg_match:
            continue
        msg_name = msg_match.group(1)
        fields = {}
        for m in re.finditer(r'(?:optional|required|repeated)\s+\S+\s+(\w+)\s*=\s*(\d+)', content):
            fname, fnum = m.group(1), int(m.group(2))
            fields[fnum] = fname
        if fields:
            OUR_FIELD_NAMES[msg_name] = fields


def infer_field_name(apk_class, field_num, field):
    """Try to infer a meaningful field name."""
    # If this APK class maps to one of our protos (seed or sub-message), use our field name
    our_name = APK_TO_OUR_NAME.get(apk_class, "") or SUB_MESSAGE_NAMES.get(apk_class, "")
    if our_name and our_name in OUR_FIELD_NAMES:
        our_fields = OUR_FIELD_NAMES[our_name]
        if field_num in our_fields:
            return our_fields[field_num]

    # ChannelDescriptor special case — we know what each field is
    if apk_class == "wbw" and field_num in CHANNEL_DESCRIPTOR_FIELDS:
        return CHANNEL_DESCRIPTOR_FIELDS[field_num][0]

    # ServiceDiscoveryResponse — we know many fields
    if apk_class == "wby":
        sdr_fields = {
            1: "channels", 2: "head_unit_name", 3: "car_model", 4: "car_year",
            5: "car_serial", 6: "left_hand_drive_vehicle", 7: "headunit_manufacturer",
            8: "headunit_model", 9: "sw_build", 10: "sw_version",
            11: "can_play_native_media_during_vr", 13: "session_configuration",
            14: "display_name", 15: "probe_for_support",
        }
        if field_num in sdr_fields:
            return sdr_fields[field_num]

    # ServiceDiscoveryRequest — from live capture
    if apk_class == "aahi":
        sdr_fields = {
            1: "phone_icon_small", 2: "phone_icon_medium", 3: "phone_icon_large",
            4: "device_name", 5: "device_brand",
        }
        if field_num in sdr_fields:
            return sdr_fields[field_num]

    # HeadUnitInfo — we know the fields
    if apk_class == "aacd":
        hui_fields = {
            1: "make", 2: "model", 3: "year", 4: "vehicle_id",
            5: "driver_position", 6: "headunit_make", 7: "headunit_model",
            8: "sw_build",
        }
        if field_num in hui_fields:
            return hui_fields[field_num]

    # Fall back to obfuscated name
    return field.get("name", "")


def load_proto_data():
    """Load the decoded proto data from apk_protos.json."""
    with open(PROTO_JSON) as f:
        data = json.load(f)
    return data["messages"], data["enums"], data.get("matches", [])


# ── Phase 1: Graph Walk ──────────────────────────────────────────────────

def graph_walk(messages, enums):
    """BFS from seed set, collecting all reachable protos and enums."""

    # Track discovered protos and enums
    discovered_messages = {}  # class_name -> {channel, depth, referenced_by, ...}
    discovered_enums = {}     # class_name -> {channel, used_by, ...}
    queue = deque()

    # Seed the queue
    for apk_class, info in SEED_MATCHES.items():
        if apk_class in messages:
            discovered_messages[apk_class] = {
                "our_name": info["name"],
                "channels": {info["channel"]},
                "depth": 0,
                "referenced_by": set(),
                "seed": True,
            }
            queue.append((apk_class, info["channel"], 0))
        else:
            print(f"  WARNING: seed class '{apk_class}' not found in decoded messages")

    print(f"Phase 1: Graph walk from {len(queue)} seed protos")

    # BFS
    while queue:
        class_name, channel, depth = queue.popleft()
        msg = messages.get(class_name)
        if not msg:
            continue

        for field in msg["fields"]:
            # Follow message references
            msg_class = field.get("message_class", "")
            if msg_class and msg_class in messages:
                if msg_class not in discovered_messages:
                    inferred_name = SEED_MATCHES.get(msg_class, {}).get("name", "") or \
                                    SUB_MESSAGE_NAMES.get(msg_class, "")
                    discovered_messages[msg_class] = {
                        "our_name": inferred_name,
                        "channels": {channel},
                        "depth": depth + 1,
                        "referenced_by": {class_name},
                        "seed": False,
                    }
                    queue.append((msg_class, channel, depth + 1))
                else:
                    discovered_messages[msg_class]["channels"].add(channel)
                    discovered_messages[msg_class]["referenced_by"].add(class_name)

            # Collect enum references
            enum_class = field.get("enum_class", "")
            if enum_class and enum_class in enums:
                if enum_class not in discovered_enums:
                    discovered_enums[enum_class] = {
                        "channels": {channel},
                        "used_by": {class_name},
                    }
                else:
                    discovered_enums[enum_class]["channels"].add(channel)
                    discovered_enums[enum_class]["used_by"].add(class_name)

    print(f"  Found {len(discovered_messages)} messages, {len(discovered_enums)} enums")
    return discovered_messages, discovered_enums


# ── Phase 2: Gearhead Scan ───────────────────────────────────────────────

def gearhead_scan(messages, discovered_messages, discovered_enums):
    """Scan gearhead package for references to proto classes."""

    if not GEARHEAD.exists():
        print("Phase 2: Gearhead directory not found, skipping")
        return 0

    # Build set of all known proto class names
    proto_classes = set(messages.keys())

    # Scan all Java files in gearhead
    new_roots = set()
    java_files = list(GEARHEAD.rglob("*.java"))
    print(f"Phase 2: Scanning {len(java_files)} gearhead Java files")

    for java_file in java_files:
        try:
            content = java_file.read_text(errors="replace")
        except Exception:
            continue

        # Look for references to defpackage classes
        # Pattern: defpackage.XXX or just XXX if imported
        for match in re.finditer(r'(?:defpackage\.)?([a-z]{2,5})\b', content):
            class_name = match.group(1)
            if class_name in proto_classes and class_name not in discovered_messages:
                new_roots.add(class_name)

    # BFS from new roots
    if new_roots:
        print(f"  Found {len(new_roots)} new proto references in gearhead")
        queue = deque()
        for class_name in new_roots:
            discovered_messages[class_name] = {
                "our_name": SUB_MESSAGE_NAMES.get(class_name, ""),
                "channels": {"gearhead"},
                "depth": 0,
                "referenced_by": set(),
                "seed": False,
                "gearhead_root": True,
            }
            queue.append((class_name, "gearhead", 0))

        while queue:
            class_name, channel, depth = queue.popleft()
            msg = messages.get(class_name)
            if not msg:
                continue

            for field in msg["fields"]:
                msg_class = field.get("message_class", "")
                if msg_class and msg_class in messages:
                    if msg_class not in discovered_messages:
                        discovered_messages[msg_class] = {
                            "our_name": SUB_MESSAGE_NAMES.get(msg_class, ""),
                            "channels": {channel},
                            "depth": depth + 1,
                            "referenced_by": {class_name},
                            "seed": False,
                        }
                        queue.append((msg_class, channel, depth + 1))
                    else:
                        discovered_messages[msg_class]["channels"].add(channel)
                        discovered_messages[msg_class]["referenced_by"].add(class_name)

                enum_class = field.get("enum_class", "")
                if enum_class and enum_class in enums_global:
                    if enum_class not in discovered_enums:
                        discovered_enums[enum_class] = {
                            "channels": {channel},
                            "used_by": {class_name},
                        }
                    else:
                        discovered_enums[enum_class]["channels"].add(channel)
                        discovered_enums[enum_class]["used_by"].add(class_name)

    print(f"  Total after gearhead scan: {len(discovered_messages)} messages, {len(discovered_enums)} enums")
    return len(new_roots)


# ── Phase 3: Build Knowledge Base ────────────────────────────────────────

def build_knowledge_base(messages, enums, discovered_messages, discovered_enums):
    """Build the structured JSON knowledge base."""

    kb = {
        "metadata": {
            "apk_version": "16.1.660414-release",
            "total_apk_protos": len(messages),
            "total_apk_enums": len(enums),
            "aa_relevant_messages": len(discovered_messages),
            "aa_relevant_enums": len(discovered_enums),
            "seed_count": sum(1 for v in discovered_messages.values() if v.get("seed")),
        },
        "messages": {},
        "enums": {},
        "channels": defaultdict(lambda: {"messages": [], "enums": []}),
    }

    # Build message entries
    for class_name, info in sorted(discovered_messages.items()):
        msg = messages.get(class_name, {})
        fields = []
        artifact_count = 0
        for f in msg.get("fields", []):
            # Skip likely decoder artifacts (field numbers >= 50)
            if f["number"] >= 50:
                artifact_count += 1
                continue
            inferred = infer_field_name(class_name, f["number"], f)
            field_entry = {
                "number": f["number"],
                "name": inferred if inferred != f.get("name", "") else f.get("name", ""),
                "obfuscated_name": f.get("name", ""),
                "type": f["proto_type"],
                "repeated": f.get("repeated", False),
                "required": f.get("required", False),
            }
            if f.get("message_class"):
                field_entry["message_class"] = f["message_class"]
                # Add our name if we know it
                ref_info = discovered_messages.get(f["message_class"], {})
                if ref_info.get("our_name"):
                    field_entry["message_name"] = ref_info["our_name"]
            if f.get("enum_class"):
                field_entry["enum_class"] = f["enum_class"]
            if f.get("enum_verifier"):
                field_entry["enum_verifier"] = f["enum_verifier"]
            fields.append(field_entry)

        channels = sorted(info["channels"])
        primary_channel = channels[0] if channels else "unknown"

        entry = {
            "apk_class": class_name,
            "our_name": info.get("our_name", ""),
            "channels": channels,
            "depth": info["depth"],
            "seed": info.get("seed", False),
            "gearhead_root": info.get("gearhead_root", False),
            "referenced_by": sorted(info.get("referenced_by", set())),
            "field_count": msg.get("field_count", 0),
            "fields": fields,
        }

        # Build references list (what this message points to)
        references = []
        for f in msg.get("fields", []):
            if f.get("message_class") and f["message_class"] in discovered_messages:
                references.append(f["message_class"])
        entry["references"] = sorted(set(references))

        kb["messages"][class_name] = entry

        # Add to channel index
        for ch in channels:
            kb["channels"][ch]["messages"].append(class_name)

    # Build enum entries
    for class_name, info in sorted(discovered_enums.items()):
        enum_data = enums.get(class_name, {})
        values = []
        for v in enum_data.get("values", []):
            values.append({"name": v["name"], "number": v["number"]})

        channels = sorted(info["channels"])
        entry = {
            "apk_class": class_name,
            "channels": channels,
            "used_by": sorted(info.get("used_by", set())),
            "values": values,
        }

        kb["enums"][class_name] = entry

        for ch in channels:
            kb["channels"][ch]["enums"].append(class_name)

    # Convert defaultdict to regular dict
    kb["channels"] = dict(kb["channels"])

    return kb


# ── Phase 4: Generate Markdown Reference ─────────────────────────────────

def generate_markdown(kb, enums_data):
    """Generate human-readable Markdown reference organized by channel."""

    lines = []
    lines.append("# Android Auto Protocol Reference")
    lines.append("")
    lines.append(f"*Auto-generated from AA APK v{kb['metadata']['apk_version']}*")
    lines.append(f"*{kb['metadata']['aa_relevant_messages']} messages, "
                 f"{kb['metadata']['aa_relevant_enums']} enums "
                 f"(of {kb['metadata']['total_apk_protos']} total in APK)*")
    lines.append("")

    # Table of contents
    lines.append("## Table of Contents")
    lines.append("")
    channel_order = [
        "control", "video", "audio_media", "audio_speech", "audio_system",
        "input", "sensor", "bluetooth", "wifi", "navigation", "media",
        "phone", "avinput", "gearhead", "unknown",
    ]
    for ch in channel_order:
        if ch in kb["channels"]:
            info = CHANNEL_INFO.get(ch, {"title": ch.title()})
            msg_count = len(kb["channels"][ch]["messages"])
            enum_count = len(kb["channels"][ch]["enums"])
            lines.append(f"- [{info['title']}](#{ch}) — {msg_count} messages, {enum_count} enums")
    lines.append("")

    # Each channel
    for ch in channel_order:
        if ch not in kb["channels"]:
            continue

        info = CHANNEL_INFO.get(ch, {"title": ch.title(), "description": ""})
        ch_data = kb["channels"][ch]

        lines.append(f"---")
        lines.append(f"")
        lines.append(f"## {info['title']}")
        lines.append(f"")
        if info.get("description"):
            lines.append(f"{info['description']}")
            lines.append(f"")

        # Messages sorted by depth (roots first), then name
        msg_classes = sorted(ch_data["messages"],
                           key=lambda c: (kb["messages"][c]["depth"], c))

        for class_name in msg_classes:
            msg = kb["messages"][class_name]
            our_name = msg["our_name"]
            title = f"{our_name} (`{class_name}`)" if our_name else f"`{class_name}`"

            depth_str = f"depth {msg['depth']}"
            if msg["seed"]:
                depth_str = "**seed**"
            elif msg.get("gearhead_root"):
                depth_str = "**gearhead root**"

            lines.append(f"### {title}")
            lines.append(f"")

            # Show referenced_by
            if msg["referenced_by"]:
                refs = []
                for ref in msg["referenced_by"]:
                    ref_name = kb["messages"].get(ref, {}).get("our_name", "")
                    refs.append(f"`{ref}`" + (f" ({ref_name})" if ref_name else ""))
                lines.append(f"*Referenced by: {', '.join(refs)} | {depth_str}*")
            else:
                lines.append(f"*{depth_str}*")
            lines.append(f"")

            # Fields table (filter out likely decoder artifacts: field# >= 50)
            real_fields = [f for f in msg["fields"] if f["number"] < 50]
            artifact_fields = [f for f in msg["fields"] if f["number"] >= 50]
            if real_fields:
                lines.append(f"| # | Name | Type | Notes |")
                lines.append(f"|---|------|------|-------|")
                for f in sorted(real_fields, key=lambda x: x["number"]):
                    num = f["number"]
                    name = f["name"]
                    ftype = f["type"]
                    if f.get("repeated"):
                        ftype = f"repeated {ftype}"
                    if f.get("required"):
                        ftype = f"**{ftype}** (required)"

                    notes = []
                    if f.get("message_class"):
                        msg_name = f.get("message_name", "")
                        if msg_name:
                            notes.append(f"→ {msg_name} (`{f['message_class']}`)")
                        else:
                            notes.append(f"→ `{f['message_class']}`")
                    if f.get("enum_class"):
                        # Look up enum values
                        enum_entry = kb["enums"].get(f["enum_class"], {})
                        enum_vals = enum_entry.get("values", [])
                        if enum_vals:
                            val_strs = [f"{v['name']}={v['number']}" for v in enum_vals[:5]]
                            if len(enum_vals) > 5:
                                val_strs.append(f"...+{len(enum_vals)-5}")
                            notes.append(f"enum `{f['enum_class']}`: {', '.join(val_strs)}")
                        elif f["enum_class"] in UNRESOLVED_ENUM_NOTES:
                            notes.append(f"*{UNRESOLVED_ENUM_NOTES[f['enum_class']]}*")
                        else:
                            notes.append(f"enum `{f['enum_class']}`")

                    lines.append(f"| {num} | {name} | {ftype} | {' | '.join(notes) if notes else ''} |")
                if artifact_fields:
                    lines.append(f"*{len(artifact_fields)} high-numbered fields omitted (likely decoder artifacts)*")
                lines.append(f"")
            else:
                lines.append(f"*(empty message)*")
                lines.append(f"")

        # Enums for this channel
        enum_classes = sorted(ch_data.get("enums", []))
        if enum_classes:
            lines.append(f"### Enums ({ch})")
            lines.append(f"")
            for enum_class in enum_classes:
                enum_entry = kb["enums"].get(enum_class, {})
                values = enum_entry.get("values", [])
                used_by = enum_entry.get("used_by", [])

                lines.append(f"**`{enum_class}`**")
                if used_by:
                    lines.append(f"*Used by: {', '.join(f'`{u}`' for u in used_by)}*")
                lines.append(f"")
                if values:
                    for v in values:
                        lines.append(f"- `{v['name']}` = {v['number']}")
                    lines.append(f"")

    return "\n".join(lines)


# ── Main ─────────────────────────────────────────────────────────────────

def main():
    print("AA Proto Graph Walker")
    print("=" * 60)

    # Load data
    print("Loading proto data...")
    messages, enums, matches = load_proto_data()
    print(f"  Loaded {len(messages)} messages, {len(enums)} enums")

    print("Loading our field names...")
    load_our_field_names()
    print(f"  Loaded names for {len(OUR_FIELD_NAMES)} proto messages")

    # Make enums globally accessible for gearhead scan
    global enums_global
    enums_global = enums

    # Phase 1: Graph walk
    discovered_messages, discovered_enums = graph_walk(messages, enums)

    # Phase 2: Gearhead scan
    new_roots = gearhead_scan(messages, discovered_messages, discovered_enums)

    # Phase 3: Build knowledge base
    print("Phase 3: Building knowledge base")
    kb = build_knowledge_base(messages, enums, discovered_messages, discovered_enums)

    # Stats
    print(f"\nResults:")
    print(f"  AA-relevant messages: {kb['metadata']['aa_relevant_messages']}")
    print(f"  AA-relevant enums: {kb['metadata']['aa_relevant_enums']}")
    print(f"  Channels: {len(kb['channels'])}")
    for ch, ch_data in sorted(kb["channels"].items()):
        print(f"    {ch}: {len(ch_data['messages'])} messages, {len(ch_data['enums'])} enums")

    # Save JSON
    print(f"\nSaving JSON to {OUTPUT_JSON}")
    # Convert sets to lists for JSON serialization (should already be done but safety check)
    with open(OUTPUT_JSON, "w") as f:
        json.dump(kb, f, indent=2, default=list)

    # Phase 4: Generate Markdown
    print(f"Generating Markdown to {OUTPUT_MD}")
    md = generate_markdown(kb, enums)
    OUTPUT_MD.parent.mkdir(parents=True, exist_ok=True)
    with open(OUTPUT_MD, "w") as f:
        f.write(md)

    # Summary of most interesting findings
    print(f"\nTop discoveries (deepest new sub-messages):")
    unnamed = [(k, v) for k, v in kb["messages"].items() if not v["our_name"] and v["depth"] > 0]
    unnamed.sort(key=lambda x: (-len(x[1]["referenced_by"]), x[1]["depth"]))
    for class_name, info in unnamed[:20]:
        refs = ", ".join(info["referenced_by"][:3])
        channels = ", ".join(info["channels"])
        print(f"  {class_name} (depth {info['depth']}, {info['field_count']} fields, "
              f"channels: {channels}, refs: {refs})")

    print(f"\nDone! See {OUTPUT_MD} for the full reference.")


if __name__ == "__main__":
    main()
