from __future__ import annotations

from pathlib import Path
import argparse
import sys

if __package__ in (None, ""):
    sys.path.append(str(Path(__file__).resolve().parents[3]))

from analysis.tools.apk_indexer.config import IndexerConfig
from analysis.tools.apk_indexer.extract import extract_signals
from analysis.tools.apk_indexer.report import write_summary_report
from analysis.tools.apk_indexer.relocate_source import relocate_source
from analysis.tools.apk_indexer.resolve_version import resolve_version
from analysis.tools.apk_indexer.write_json import write_json_exports
from analysis.tools.apk_indexer.write_sqlite import write_sqlite


def run_indexer(source_root: Path, analysis_root: Path, scope: str = "all") -> Path:
    version_name, version_code = resolve_version(source_root)
    base = relocate_source(source_root, analysis_root, version_name, version_code)
    source = base / "apk-source"

    signals = extract_signals(source, scope=scope)

    sqlite_dir = base / "apk-index" / "sqlite"
    json_dir = base / "apk-index" / "json"
    reports_dir = base / "apk-index" / "reports"
    reports_dir.mkdir(parents=True, exist_ok=True)

    write_sqlite(sqlite_dir / "apk_index.db", signals)
    write_json_exports(json_dir, signals)
    write_summary_report(reports_dir / "summary.md", signals)
    return base


def parse_args(argv: list[str] | None = None) -> IndexerConfig:
    parser = argparse.ArgumentParser(
        description="Build SQLite/JSON indexes from decompiled Android Auto APK sources."
    )
    parser.add_argument("--source", required=True, type=Path, help="Path to decompiled APK root")
    parser.add_argument(
        "--analysis-root",
        required=True,
        type=Path,
        help="Root directory where versioned analysis outputs will be written",
    )
    parser.add_argument(
        "--scope",
        choices=("all", "projection"),
        default="all",
        help="Optional filter scope for extracted files",
    )
    args = parser.parse_args(argv)
    return IndexerConfig(
        source_root=args.source, analysis_root=args.analysis_root, scope=args.scope
    )


def main(argv: list[str] | None = None) -> int:
    cfg = parse_args(argv)
    run_indexer(cfg.source_root, cfg.analysis_root, scope=cfg.scope)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
