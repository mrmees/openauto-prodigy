# APK Indexer (Local Context)

This directory contains a standalone APK preprocessing/indexing toolchain used for Android Auto reverse-engineering.

If a future session starts from here, this file is the bootstrap.

## Purpose

Given a decompiled Android Auto source tree, the tool:
- Copies source into a versioned analysis folder
- Extracts protocol-relevant static signals
- Writes SQLite + JSON indexes
- Generates a Markdown summary report

## Entrypoints

- Script: `analysis/tools/apk_indexer/run_indexer.py`
- Make target: `analysis/tools/apk_indexer/Makefile`

Run from repo root:

```bash
python3 analysis/tools/apk_indexer/run_indexer.py \
  --source <decompiled-apk-root> \
  --analysis-root analysis \
  --scope all
```

Or:

```bash
make -C analysis/tools/apk_indexer index \
  SOURCE=<decompiled-apk-root> \
  ANALYSIS_ROOT=analysis \
  SCOPE=all
```

## Repo-Specific Example

Current local decompile source:

```bash
/home/matt/claude/personal/openautopro/firmware/android-auto-apk/decompiled
```

Typical run:

```bash
python3 analysis/tools/apk_indexer/run_indexer.py \
  --source /home/matt/claude/personal/openautopro/firmware/android-auto-apk/decompiled \
  --analysis-root analysis \
  --scope all
```

## Scope Modes

- `all`:
  - Indexes the full decompiled source tree.
  - Best for enum/switch recovery and obfuscated protocol tracing.
- `projection`:
  - Only indexes files under `sources/com/google/android/projection/`.
  - Faster, less noise.
  - Expected downside: obfuscated `defpackage` artifacts (many enums/switches) may be missing.

## Version Resolution

Output folder version comes from:
1. `apktool.yml` (`versionName`, `versionCode`) if present
2. `manifest.json` in source root or parent (`version_name`, `version_code`)
3. fallback: `unknown_unknown`

## Output Layout

```text
analysis/android_auto_<versionName>_<versionCode>/
  apk-source/
  apk-index/
    sqlite/apk_index.db
    json/
      uuids.json
      constants.json
      proto_accesses.json
      proto_writes.json
      enum_maps.json
      switch_maps.json
      call_edges.json
    reports/summary.md
```

## SQLite Tables

- `uuids(file,line,value)`
- `constants(file,line,value)`
- `proto_accesses(file,line,accessor)` (set/get/has/clear style)
- `proto_writes(file,line,target,op,value)` (obfuscated builder-style writes, `|=` and `=`)
- `enum_maps(file,line,enum_class,int_value,enum_name)` (switch-based enum mapping)
- `switch_maps(file,line,switch_expr,case_value,target)` (case-to-call mapping)
- `call_edges(file,line,target)`

## Useful Queries

Top UUIDs:

```sql
select value, count(*) n
from uuids
group by value
order by n desc
limit 20;
```

Obfuscated proto writes:

```sql
select target, op, count(*) n
from proto_writes
group by target, op
order by n desc
limit 30;
```

Enum mapping (codec-style):

```sql
select enum_class, int_value, enum_name
from enum_maps
where enum_class = 'vyn'
order by int_value;
```

Switch dispatch hotspots:

```sql
select switch_expr, case_value, target, count(*) n
from switch_maps
group by switch_expr, case_value, target
order by n desc
limit 30;
```

## Verification

Run test suite:

```bash
PYTHONPATH=. pytest analysis/tools/apk_indexer/tests -v
```

Show CLI help:

```bash
python3 analysis/tools/apk_indexer/run_indexer.py --help
```
