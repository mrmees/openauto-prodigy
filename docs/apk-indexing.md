# APK Indexing

This project includes a lightweight preprocessing/indexing pipeline for decompiled Android Auto APK sources.

## Run

```bash
python3 analysis/tools/apk_indexer/run_indexer.py \
  --source <path-to-decompiled-apk-root> \
  --analysis-root analysis
```

Protocol-focused (reduced-noise) scope:

```bash
python3 analysis/tools/apk_indexer/run_indexer.py \
  --source <path-to-decompiled-apk-root> \
  --analysis-root analysis \
  --scope projection
```

Or via make:

```bash
make -C analysis/tools/apk_indexer index SOURCE=<path-to-decompiled-apk-root> ANALYSIS_ROOT=analysis
```

## Output Layout

The tool auto-detects `versionName` + `versionCode` from `apktool.yml` and writes:

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
      call_edges.json
    reports/summary.md
```

## SQLite Queries

Top UUIDs:

```sql
select value, count(*) as n
from uuids
group by value
order by n desc
limit 20;
```

Top proto accessors:

```sql
select accessor, count(*) as n
from proto_accesses
group by accessor
order by n desc
limit 20;
```

Top call targets:

```sql
select target, count(*) as n
from call_edges
group by target
order by n desc
limit 20;
```
