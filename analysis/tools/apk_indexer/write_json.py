from __future__ import annotations

from pathlib import Path
import json


def write_json_exports(
    output_dir: Path, signals: dict[str, list[dict[str, object]]]
) -> list[Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    written: list[Path] = []
    for key, rows in signals.items():
        dest = output_dir / f"{key}.json"
        dest.write_text(json.dumps(rows, indent=2, sort_keys=True) + "\n")
        written.append(dest)
    return written
