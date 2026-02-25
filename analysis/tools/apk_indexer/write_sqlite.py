from __future__ import annotations

from pathlib import Path
import sqlite3


def _create_schema(conn: sqlite3.Connection) -> None:
    conn.executescript(
        """
        CREATE TABLE IF NOT EXISTS uuids (
            file TEXT NOT NULL,
            line INTEGER NOT NULL,
            value TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS constants (
            file TEXT NOT NULL,
            line INTEGER NOT NULL,
            value TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS proto_accesses (
            file TEXT NOT NULL,
            line INTEGER NOT NULL,
            accessor TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS call_edges (
            file TEXT NOT NULL,
            line INTEGER NOT NULL,
            target TEXT NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_uuids_value ON uuids(value);
        CREATE INDEX IF NOT EXISTS idx_constants_value ON constants(value);
        CREATE INDEX IF NOT EXISTS idx_proto_accessor ON proto_accesses(accessor);
        CREATE INDEX IF NOT EXISTS idx_call_target ON call_edges(target);
        """
    )


def write_sqlite(db_path: Path, signals: dict[str, list[dict[str, object]]]) -> Path:
    db_path.parent.mkdir(parents=True, exist_ok=True)
    with sqlite3.connect(db_path) as conn:
        _create_schema(conn)
        conn.execute("DELETE FROM uuids")
        conn.execute("DELETE FROM constants")
        conn.execute("DELETE FROM proto_accesses")
        conn.execute("DELETE FROM call_edges")

        conn.executemany(
            "INSERT INTO uuids(file, line, value) VALUES (?, ?, ?)",
            [(row["file"], row["line"], row["value"]) for row in signals.get("uuids", [])],
        )
        conn.executemany(
            "INSERT INTO constants(file, line, value) VALUES (?, ?, ?)",
            [(row["file"], row["line"], row["value"]) for row in signals.get("constants", [])],
        )
        conn.executemany(
            "INSERT INTO proto_accesses(file, line, accessor) VALUES (?, ?, ?)",
            [
                (row["file"], row["line"], row["accessor"])
                for row in signals.get("proto_accesses", [])
            ],
        )
        conn.executemany(
            "INSERT INTO call_edges(file, line, target) VALUES (?, ?, ?)",
            [
                (row["file"], row["line"], row["target"])
                for row in signals.get("call_edges", [])
            ],
        )
    return db_path
