from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class IndexerConfig:
    source_root: Path
    analysis_root: Path
    scope: str = "all"
