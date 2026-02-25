"""Load APK class matches from auto-detection + manual overrides."""

from __future__ import annotations

import json
from pathlib import Path

TOOLS_DIR = Path(__file__).parent
APK_PROTOS_JSON = TOOLS_DIR / "proto_decode_output" / "apk_protos.json"
MANUAL_OVERRIDES = TOOLS_DIR / "proto_matches.json"


def load_matches() -> dict:
    """Load matches, with manual overrides taking precedence."""
    matches = {}

    if APK_PROTOS_JSON.exists():
        data = json.loads(APK_PROTOS_JSON.read_text(encoding="utf-8"))
        for item in data.get("matches", []):
            key = item.get("our_message") or item.get("our_proto", "")
            if not key:
                continue
            matches[key] = {
                "apk_class": item["apk_class"],
                "score": item["score"],
                "source": "auto",
                "our_proto_file": item.get("our_proto", ""),
                "diffs": item.get("diffs", []),
            }

    if MANUAL_OVERRIDES.exists():
        overrides = json.loads(MANUAL_OVERRIDES.read_text(encoding="utf-8"))
        for msg_name, info in overrides.items():
            matches[msg_name] = {
                "apk_class": info["apk_class"],
                "score": info.get("score", 1.0),
                "source": "manual",
                "note": info.get("note", ""),
                "diffs": [],
            }

    return matches
