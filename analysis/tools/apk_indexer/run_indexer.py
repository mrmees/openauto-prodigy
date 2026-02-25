from __future__ import annotations

from pathlib import Path
import argparse

from analysis.tools.apk_indexer.config import IndexerConfig
from analysis.tools.apk_indexer.extract import extract_signals
from analysis.tools.apk_indexer.relocate_source import relocate_source
from analysis.tools.apk_indexer.resolve_version import resolve_version
from analysis.tools.apk_indexer.write_json import write_json_exports
from analysis.tools.apk_indexer.write_sqlite import write_sqlite


def run_indexer(source_root: Path, analysis_root: Path) -> Path:
    version_name, version_code = resolve_version(source_root)
    base = relocate_source(source_root, analysis_root, version_name, version_code)
    source = base / "apk-source"

    signals = extract_signals(source)

    sqlite_dir = base / "apk-index" / "sqlite"
    json_dir = base / "apk-index" / "json"
    reports_dir = base / "apk-index" / "reports"
    reports_dir.mkdir(parents=True, exist_ok=True)

    write_sqlite(sqlite_dir / "apk_index.db", signals)
    write_json_exports(json_dir, signals)
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
    args = parser.parse_args(argv)
    return IndexerConfig(source_root=args.source, analysis_root=args.analysis_root)


def main(argv: list[str] | None = None) -> int:
    cfg = parse_args(argv)
    run_indexer(cfg.source_root, cfg.analysis_root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
