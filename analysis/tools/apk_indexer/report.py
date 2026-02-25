from __future__ import annotations

from collections import Counter
from pathlib import Path


def _top_rows(values: list[str], limit: int = 10) -> list[tuple[str, int]]:
    return Counter(values).most_common(limit)


def _render_section(title: str, rows: list[tuple[str, int]]) -> str:
    lines = [f"## {title}", ""]
    if not rows:
        lines.append("- (none)")
    else:
        for value, count in rows:
            lines.append(f"- `{value}`: {count}")
    lines.append("")
    return "\n".join(lines)


def write_summary_report(
    report_path: Path, signals: dict[str, list[dict[str, object]]]
) -> Path:
    report_path.parent.mkdir(parents=True, exist_ok=True)

    sections = [
        _render_section(
            "Top UUIDs",
            _top_rows([str(row["value"]) for row in signals.get("uuids", [])]),
        ),
        _render_section(
            "Top Constants",
            _top_rows([str(row["value"]) for row in signals.get("constants", [])]),
        ),
        _render_section(
            "Top Proto Accessors",
            _top_rows([str(row["accessor"]) for row in signals.get("proto_accesses", [])]),
        ),
        _render_section(
            "Top Proto Writes",
            _top_rows(
                [
                    f"{row['target']} {row['op']}"
                    for row in signals.get("proto_writes", [])
                ]
            ),
        ),
        _render_section(
            "Top Call Targets",
            _top_rows([str(row["target"]) for row in signals.get("call_edges", [])]),
        ),
    ]

    body = "# APK Index Summary\n\n" + "".join(sections)
    report_path.write_text(body)
    return report_path
