from __future__ import annotations

from pathlib import Path
import re


def resolve_version(root: Path) -> tuple[str, str]:
    yml = root / "apktool.yml"
    if yml.exists():
        txt = yml.read_text(errors="ignore")
        vn = re.search(r"versionName:\s*['\"]?([^'\"\n]+)", txt)
        vc = re.search(r"versionCode:\s*['\"]?([^'\"\n]+)", txt)
        if vn and vc:
            return vn.group(1).strip(), vc.group(1).strip()
    return "unknown", "unknown"
