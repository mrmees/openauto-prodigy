from __future__ import annotations

from pathlib import Path
import json
import re


def resolve_version(root: Path) -> tuple[str, str]:
    yml = root / "apktool.yml"
    if yml.exists():
        txt = yml.read_text(errors="ignore")
        vn = re.search(r"versionName:\s*['\"]?([^'\"\n]+)", txt)
        vc = re.search(r"versionCode:\s*['\"]?([^'\"\n]+)", txt)
        if vn and vc:
            return vn.group(1).strip(), vc.group(1).strip()

    for manifest in (root / "manifest.json", root.parent / "manifest.json"):
        if manifest.exists():
            data = json.loads(manifest.read_text(errors="ignore"))
            version_name = data.get("version_name") or data.get("versionName")
            version_code = data.get("version_code") or data.get("versionCode")
            if version_name and version_code:
                return str(version_name).strip(), str(version_code).strip()
    return "unknown", "unknown"
