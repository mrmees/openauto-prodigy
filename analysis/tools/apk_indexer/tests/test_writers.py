import json
import sqlite3

from analysis.tools.apk_indexer.write_json import write_json_exports
from analysis.tools.apk_indexer.write_sqlite import write_sqlite


def _sample_signals():
    return {
        "uuids": [{"file": "A.java", "line": 1, "value": "4de17a00-52cb-11e6-bdf4-0800200c9a66"}],
        "constants": [{"file": "B.java", "line": 2, "value": "0x1A2B"}],
        "proto_accesses": [{"file": "C.java", "line": 3, "accessor": "setChannelId"}],
        "call_edges": [{"file": "D.java", "line": 4, "target": "transport.sendMessage"}],
        "proto_writes": [{"file": "E.java", "line": 5, "target": "xhqVar.b", "op": "|=", "value": "16"}],
    }


def test_sqlite_schema_created(tmp_path):
    db = tmp_path / "apk_index.db"
    write_sqlite(db, _sample_signals())

    with sqlite3.connect(db) as conn:
        tables = {
            row[0]
            for row in conn.execute(
                "select name from sqlite_master where type='table'"
            ).fetchall()
        }
        assert "uuids" in tables
        assert "constants" in tables
        assert "proto_accesses" in tables
        assert "call_edges" in tables
        assert "proto_writes" in tables


def test_json_exports_created(tmp_path):
    out_dir = tmp_path / "json"
    write_json_exports(out_dir, _sample_signals())

    uuids = json.loads((out_dir / "uuids.json").read_text())
    assert uuids[0]["value"] == "4de17a00-52cb-11e6-bdf4-0800200c9a66"
