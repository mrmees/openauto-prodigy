# Android Auto APK Preprocessing Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a reproducible APK preprocessing/indexing pipeline that auto-detects Android Auto version metadata and writes versioned source + SQLite/JSON index artifacts.

**Architecture:** A Python-based pipeline script orchestrates metadata discovery, source relocation, extraction, indexing, and report generation. Fast text extraction (`rg`) feeds deterministic SQLite + JSON outputs. The plan keeps v1 lightweight and verifiable on hobby hardware.

**Tech Stack:** Python 3, SQLite, ripgrep, bash, markdown reports

---

### Task 1: Scaffold Analysis Tooling Directories

**Files:**
- Create: `analysis/tools/apk_indexer/`
- Create: `analysis/tools/apk_indexer/__init__.py`
- Create: `analysis/tools/apk_indexer/README.md`

**Step 1: Create directories**

```bash
mkdir -p analysis/tools/apk_indexer
```

**Step 2: Create package marker and readme**

```bash
touch analysis/tools/apk_indexer/__init__.py
cat > analysis/tools/apk_indexer/README.md <<'MD'
APK indexer tooling for Android Auto reverse-engineering.
MD
```

**Step 3: Commit**

```bash
git add analysis/tools/apk_indexer
git commit -m "chore: scaffold apk indexer tooling directories"
```

### Task 2: Implement Version Metadata Resolver

**Files:**
- Create: `analysis/tools/apk_indexer/resolve_version.py`
- Test: `analysis/tools/apk_indexer/tests/test_resolve_version.py`

**Step 1: Write failing test**

```python
# analysis/tools/apk_indexer/tests/test_resolve_version.py
from analysis.tools.apk_indexer.resolve_version import resolve_version

def test_resolve_from_apktool_yml(tmp_path):
    p = tmp_path / "apktool.yml"
    p.write_text("versionName: 16.1\nversionCode: '1248424'\n")
    name, code = resolve_version(tmp_path)
    assert name == "16.1"
    assert code == "1248424"
```

**Step 2: Run test to verify it fails**

Run: `pytest analysis/tools/apk_indexer/tests/test_resolve_version.py -v`
Expected: FAIL (module/function missing)

**Step 3: Write minimal implementation**

```python
# analysis/tools/apk_indexer/resolve_version.py
from pathlib import Path
import re

def resolve_version(root: Path):
    yml = root / "apktool.yml"
    if yml.exists():
        txt = yml.read_text(errors="ignore")
        vn = re.search(r"versionName:\s*['\"]?([^'\"\n]+)", txt)
        vc = re.search(r"versionCode:\s*['\"]?([^'\"\n]+)", txt)
        if vn and vc:
            return vn.group(1).strip(), vc.group(1).strip()
    return "unknown", "unknown"
```

**Step 4: Run test to verify pass**

Run: `pytest analysis/tools/apk_indexer/tests/test_resolve_version.py -v`
Expected: PASS

**Step 5: Commit**

```bash
git add analysis/tools/apk_indexer/resolve_version.py analysis/tools/apk_indexer/tests/test_resolve_version.py
git commit -m "feat: add apk version resolver with tests"
```

### Task 3: Implement Source Relocation Into Versioned Folder

**Files:**
- Create: `analysis/tools/apk_indexer/relocate_source.py`
- Test: `analysis/tools/apk_indexer/tests/test_relocate_source.py`

**Step 1: Write failing test**

```python
from pathlib import Path
from analysis.tools.apk_indexer.relocate_source import relocate_source

def test_relocate_source_creates_versioned_apk_source(tmp_path):
    src = tmp_path / "decompiled"
    src.mkdir()
    (src / "a.txt").write_text("x")
    dest = relocate_source(src, tmp_path / "analysis", "16.1", "1248424")
    assert (dest / "apk-source" / "a.txt").exists()
```

**Step 2: Run test to verify fail**

Run: `pytest analysis/tools/apk_indexer/tests/test_relocate_source.py -v`
Expected: FAIL

**Step 3: Implement minimal relocation**

```python
# relocate_source.py
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
```

**Step 4: Run test to verify pass**

Run: `pytest analysis/tools/apk_indexer/tests/test_relocate_source.py -v`
Expected: PASS

**Step 5: Commit**

```bash
git add analysis/tools/apk_indexer/relocate_source.py analysis/tools/apk_indexer/tests/test_relocate_source.py
git commit -m "feat: add versioned apk-source relocation utility"
```

### Task 4: Implement Extractors (strings/uuids/constants/proto access/call edges)

**Files:**
- Create: `analysis/tools/apk_indexer/extract.py`
- Test: `analysis/tools/apk_indexer/tests/test_extract.py`

**Step 1: Write failing tests for each extractor category**

```python
def test_extract_uuid(): ...
def test_extract_constant_hex(): ...
def test_extract_proto_access_setter(): ...
def test_extract_call_edge_like_invocation(): ...
```

**Step 2: Run tests to verify fail**

Run: `pytest analysis/tools/apk_indexer/tests/test_extract.py -v`
Expected: FAIL

**Step 3: Implement regex-driven extractors with deterministic sorting**

```python
# extract.py
# - walk files
# - collect line-oriented matches
# - return sorted dict lists
```

**Step 4: Run tests to verify pass**

Run: `pytest analysis/tools/apk_indexer/tests/test_extract.py -v`
Expected: PASS

**Step 5: Commit**

```bash
git add analysis/tools/apk_indexer/extract.py analysis/tools/apk_indexer/tests/test_extract.py
git commit -m "feat: add v1 static extractors for protocol-relevant signals"
```

### Task 5: Implement SQLite Writer + JSON Exporter

**Files:**
- Create: `analysis/tools/apk_indexer/write_sqlite.py`
- Create: `analysis/tools/apk_indexer/write_json.py`
- Test: `analysis/tools/apk_indexer/tests/test_writers.py`

**Step 1: Write failing tests for DB schema + JSON files**

```python
def test_sqlite_schema_created(tmp_path): ...
def test_json_exports_created(tmp_path): ...
```

**Step 2: Run tests to verify fail**

Run: `pytest analysis/tools/apk_indexer/tests/test_writers.py -v`
Expected: FAIL

**Step 3: Implement writers**

```python
# write_sqlite.py
# create tables + insert rows + indexes
# write_json.py
# dump canonical sorted JSON files
```

**Step 4: Run tests to verify pass**

Run: `pytest analysis/tools/apk_indexer/tests/test_writers.py -v`
Expected: PASS

**Step 5: Commit**

```bash
git add analysis/tools/apk_indexer/write_sqlite.py analysis/tools/apk_indexer/write_json.py analysis/tools/apk_indexer/tests/test_writers.py
git commit -m "feat: add sqlite and json index writers"
```

### Task 6: Implement End-to-End Orchestrator Script

**Files:**
- Create: `analysis/tools/apk_indexer/run_indexer.py`
- Create: `analysis/tools/apk_indexer/config.py`
- Test: `analysis/tools/apk_indexer/tests/test_run_indexer.py`

**Step 1: Write failing integration-style test**

```python
def test_run_indexer_builds_versioned_outputs(tmp_path):
    # create fake decompile root + apktool.yml
    # run main()
    # assert sqlite/json/report paths exist
```

**Step 2: Run test to verify fail**

Run: `pytest analysis/tools/apk_indexer/tests/test_run_indexer.py -v`
Expected: FAIL

**Step 3: Implement orchestrator**

```python
# run_indexer.py
# 1) detect source root
# 2) resolve version
# 3) relocate source
# 4) extract signals
# 5) write sqlite/json
# 6) emit summary report
```

**Step 4: Run test to verify pass**

Run: `pytest analysis/tools/apk_indexer/tests/test_run_indexer.py -v`
Expected: PASS

**Step 5: Commit**

```bash
git add analysis/tools/apk_indexer/run_indexer.py analysis/tools/apk_indexer/config.py analysis/tools/apk_indexer/tests/test_run_indexer.py
git commit -m "feat: add end-to-end apk indexing pipeline"
```

### Task 7: Add Summary Report Generator

**Files:**
- Create: `analysis/tools/apk_indexer/report.py`
- Test: `analysis/tools/apk_indexer/tests/test_report.py`

**Step 1: Write failing test for report markdown content**

```python
def test_summary_report_contains_top_sections(tmp_path): ...
```

**Step 2: Run test to verify fail**

Run: `pytest analysis/tools/apk_indexer/tests/test_report.py -v`
Expected: FAIL

**Step 3: Implement report generator**

```python
# report.py
# writes reports/summary.md with top UUIDs/constants/proto hotspots
```

**Step 4: Run test to verify pass**

Run: `pytest analysis/tools/apk_indexer/tests/test_report.py -v`
Expected: PASS

**Step 5: Commit**

```bash
git add analysis/tools/apk_indexer/report.py analysis/tools/apk_indexer/tests/test_report.py
git commit -m "feat: add apk index summary report generation"
```

### Task 8: Wire Convenience Entrypoints + Documentation

**Files:**
- Create: `analysis/tools/apk_indexer/Makefile`
- Modify: `docs/development.md`
- Create: `docs/apk-indexing.md`

**Step 1: Add simple invocation targets**

```make
index:
	python3 analysis/tools/apk_indexer/run_indexer.py
```

**Step 2: Document inputs/outputs and query examples**

```markdown
# docs/apk-indexing.md
- where source is discovered from
- output paths
- sqlite query examples for UUID/message/proto lookups
```

**Step 3: Verify docs + command path**

Run:
```bash
python3 analysis/tools/apk_indexer/run_indexer.py --help
```
Expected: usage/help printed.

**Step 4: Commit**

```bash
git add analysis/tools/apk_indexer/Makefile docs/development.md docs/apk-indexing.md
git commit -m "docs: add apk indexing usage and developer workflow"
```

### Final Verification

**Step 1: Run full test set for indexer tools**

Run:
```bash
pytest analysis/tools/apk_indexer/tests -v
```
Expected: PASS.

**Step 2: Run indexer against real decompiled source**

Run:
```bash
python3 analysis/tools/apk_indexer/run_indexer.py
```
Expected:
- versioned folder created under `analysis/android_auto_<versionName>_<versionCode>/`
- `apk-source/`, `apk-index/sqlite/apk_index.db`, `apk-index/json/*.json`, `apk-index/reports/summary.md`

**Step 3: Sanity-query SQLite**

Run:
```bash
sqlite3 analysis/android_auto_*/apk-index/sqlite/apk_index.db "select value,count(*) from uuids group by value order by count(*) desc limit 10;"
```
Expected: known AA UUIDs visible.

**Step 4: Confirm deterministic rerun behavior**

Run indexer twice, verify no duplicate schema corruption and stable outputs.
