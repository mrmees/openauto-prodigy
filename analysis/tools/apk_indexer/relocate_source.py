from __future__ import annotations

from pathlib import Path
import shutil


def relocate_source(src: Path, analysis_root: Path, version_name: str, version_code: str) -> Path:
    base = analysis_root / f"android_auto_{version_name}_{version_code}"
    target = base / "apk-source"
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        return base
    shutil.copytree(src, target)
    return base
