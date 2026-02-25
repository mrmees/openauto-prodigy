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
        CREATE TABLE IF NOT EXISTS proto_writes (
            file TEXT NOT NULL,
            line INTEGER NOT NULL,
            target TEXT NOT NULL,
            op TEXT NOT NULL,
            value TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS enum_maps (
            file TEXT NOT NULL,
            line INTEGER NOT NULL,
            enum_class TEXT NOT NULL,
            int_value INTEGER NOT NULL,
            enum_name TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS switch_maps (
            file TEXT NOT NULL,
            line INTEGER NOT NULL,
            switch_expr TEXT NOT NULL,
            case_value TEXT NOT NULL,
            target TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS call_edges (
            file TEXT NOT NULL,
            line INTEGER NOT NULL,
            target TEXT NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_uuids_value ON uuids(value);
        CREATE INDEX IF NOT EXISTS idx_constants_value ON constants(value);
        CREATE INDEX IF NOT EXISTS idx_proto_accessor ON proto_accesses(accessor);
        CREATE INDEX IF NOT EXISTS idx_proto_writes_target ON proto_writes(target);
        CREATE INDEX IF NOT EXISTS idx_enum_maps_class ON enum_maps(enum_class);
        CREATE INDEX IF NOT EXISTS idx_switch_maps_expr ON switch_maps(switch_expr);
        CREATE INDEX IF NOT EXISTS idx_call_target ON call_edges(target);
        CREATE TABLE IF NOT EXISTS proto_classes (
            file TEXT NOT NULL,
            class_name TEXT NOT NULL,
            deprecated INTEGER NOT NULL DEFAULT 0,
            field_count INTEGER NOT NULL,
            field_names TEXT NOT NULL,
            field_types TEXT NOT NULL,
            field_decls TEXT NOT NULL,
            sub_message_refs TEXT NOT NULL,
            descriptor TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS class_references (
            file TEXT NOT NULL,
            line INTEGER NOT NULL,
            source_package TEXT NOT NULL,
            target_class TEXT NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_proto_classes_name ON proto_classes(class_name);
        CREATE INDEX IF NOT EXISTS idx_class_refs_target ON class_references(target_class);
        CREATE INDEX IF NOT EXISTS idx_class_refs_source ON class_references(source_package);
        """
    )


def write_sqlite(db_path: Path, signals: dict[str, list[dict[str, object]]]) -> Path:
    db_path.parent.mkdir(parents=True, exist_ok=True)
    with sqlite3.connect(db_path) as conn:
        _create_schema(conn)
        conn.execute("DELETE FROM uuids")
        conn.execute("DELETE FROM constants")
        conn.execute("DELETE FROM proto_accesses")
        conn.execute("DELETE FROM proto_writes")
        conn.execute("DELETE FROM enum_maps")
        conn.execute("DELETE FROM switch_maps")
        conn.execute("DELETE FROM call_edges")
        conn.execute("DELETE FROM proto_classes")
        conn.execute("DELETE FROM class_references")

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
            "INSERT INTO proto_writes(file, line, target, op, value) VALUES (?, ?, ?, ?, ?)",
            [
                (row["file"], row["line"], row["target"], row["op"], row["value"])
                for row in signals.get("proto_writes", [])
            ],
        )
        conn.executemany(
            "INSERT INTO enum_maps(file, line, enum_class, int_value, enum_name) VALUES (?, ?, ?, ?, ?)",
            [
                (
                    row["file"],
                    row["line"],
                    row["enum_class"],
                    row["int_value"],
                    row["enum_name"],
                )
                for row in signals.get("enum_maps", [])
            ],
        )
        conn.executemany(
            "INSERT INTO switch_maps(file, line, switch_expr, case_value, target) VALUES (?, ?, ?, ?, ?)",
            [
                (
                    row["file"],
                    row["line"],
                    row["switch_expr"],
                    row["case_value"],
                    row["target"],
                )
                for row in signals.get("switch_maps", [])
            ],
        )
        conn.executemany(
            "INSERT INTO call_edges(file, line, target) VALUES (?, ?, ?)",
            [
                (row["file"], row["line"], row["target"])
                for row in signals.get("call_edges", [])
            ],
        )
        conn.executemany(
            "INSERT INTO proto_classes(file, class_name, deprecated, field_count, "
            "field_names, field_types, field_decls, sub_message_refs, descriptor) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
            [
                (
                    row["file"],
                    row["class_name"],
                    1 if row["deprecated"] else 0,
                    row["field_count"],
                    row["field_names"],
                    row["field_types"],
                    row["field_decls"],
                    row["sub_message_refs"],
                    row["descriptor"],
                )
                for row in signals.get("proto_classes", [])
            ],
        )
        conn.executemany(
            "INSERT INTO class_references(file, line, source_package, target_class) "
            "VALUES (?, ?, ?, ?)",
            [
                (row["file"], row["line"], row["source_package"], row["target_class"])
                for row in signals.get("class_references", [])
            ],
        )
    return db_path
