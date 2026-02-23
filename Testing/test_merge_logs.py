#!/usr/bin/env python3
"""Tests for merge-logs.py"""

import importlib.util
import tempfile
import os
from pathlib import Path

# Load merge-logs.py as a module (hyphen in filename)
spec = importlib.util.spec_from_file_location("merge_logs", Path(__file__).parent / "merge-logs.py")
merge_logs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(merge_logs)


def _write_tmp(content):
    f = tempfile.NamedTemporaryFile(mode="w", suffix=".log", delete=False)
    f.write(content)
    f.close()
    return f.name


PI_LOG = """\
TIME\tDIR\tCHANNEL\tMESSAGE\tSIZE\tPAYLOAD_PREVIEW
0.076\tHU->Phone\tch0/CONTROL\tVERSION_REQUEST\t6\t00 01 00 07
0.189\tPhone->HU\tch0/CONTROL\tVERSION_RESPONSE\t8\t00 01 00 07 00 00
0.300\tHU->Phone\tch0/CONTROL\tSSL_HANDSHAKE\t1535\t16 03 01
"""

PHONE_LOG = """\
02-23 01:51:05.000 19618 30152 I CAR.BT: BT connected
02-23 01:51:08.076 19618 32194 I CAR.SETUP.WIFI: Socket connected to 10.0.0.1:5288
02-23 01:51:08.200 19618 32194 I CAR.SETUP.WIFI: Handshake complete
"""


def test_parse_pi_log():
    path = _write_tmp(PI_LOG)
    try:
        entries = merge_logs.parse_pi_log(path)
        assert len(entries) == 3
        assert entries[0][0] == 0.076
        assert "VERSION_REQUEST" in entries[0][2]
    finally:
        os.unlink(path)


def test_parse_phone_log():
    path = _write_tmp(PHONE_LOG)
    try:
        entries = merge_logs.parse_phone_log(path)
        assert len(entries) == 3
        # 01:51:05.000 = 1*3600 + 51*60 + 5 = 6665.0
        assert abs(entries[0][0] - 6665.0) < 0.001
    finally:
        os.unlink(path)


def test_find_pi_anchor():
    path = _write_tmp(PI_LOG)
    try:
        entries = merge_logs.parse_pi_log(path)
        # Wrap in expected format (ts, source, line)
        tagged = [(ts, "PI", line) for ts, _, line in entries]
        anchor = merge_logs.find_tcp_connect(tagged, "PI")
        assert anchor == 0.076
    finally:
        os.unlink(path)


def test_find_phone_anchor():
    path = _write_tmp(PHONE_LOG)
    try:
        entries = merge_logs.parse_phone_log(path)
        anchor = merge_logs.find_tcp_connect(entries, "PHONE")
        # 01:51:08.076 = 6668.076
        assert abs(anchor - 6668.076) < 0.001
    finally:
        os.unlink(path)


def test_offset_calculation():
    """Pi VERSION_REQUEST at 0.076s, phone Socket connected at 6668.076s.
    Offset should be 0.076 - 6668.076 = -6668.0"""
    pi_path = _write_tmp(PI_LOG)
    phone_path = _write_tmp(PHONE_LOG)
    try:
        pi_entries = merge_logs.parse_pi_log(pi_path)
        phone_entries = merge_logs.parse_phone_log(phone_path)
        pi_anchor = merge_logs.find_tcp_connect(
            [(ts, "PI", line) for ts, _, line in pi_entries], "PI"
        )
        phone_anchor = merge_logs.find_tcp_connect(phone_entries, "PHONE")
        offset = pi_anchor - phone_anchor
        assert abs(offset - (-6668.0)) < 0.001
    finally:
        os.unlink(pi_path)
        os.unlink(phone_path)


def test_no_anchor_graceful():
    """Missing anchor events should not crash."""
    pi_path = _write_tmp("TIME\tDIR\n0.1\tHU->Phone\tch0/CONTROL\tSSL_HANDSHAKE\t10\taa bb\n")
    phone_path = _write_tmp("02-23 01:00:00.000 1 1 I TAG: no anchor here\n")
    try:
        pi_entries = merge_logs.parse_pi_log(pi_path)
        phone_entries = merge_logs.parse_phone_log(phone_path)
        pi_anchor = merge_logs.find_tcp_connect(
            [(ts, "PI", line) for ts, _, line in pi_entries], "PI"
        )
        phone_anchor = merge_logs.find_tcp_connect(phone_entries, "PHONE")
        assert pi_anchor is None
        assert phone_anchor is None
    finally:
        os.unlink(pi_path)
        os.unlink(phone_path)


def test_full_merge_output():
    """End-to-end: merge produces sorted markdown output."""
    pi_path = _write_tmp(PI_LOG)
    phone_path = _write_tmp(PHONE_LOG)
    out_path = tempfile.mktemp(suffix=".md")
    try:
        # Simulate main() by calling the pieces
        import sys
        old_argv = sys.argv
        sys.argv = ["merge-logs.py", pi_path, phone_path, out_path]
        merge_logs.main()
        sys.argv = old_argv

        content = Path(out_path).read_text()
        assert "Merged Protocol Timeline" in content
        assert "Pi entries: 3" in content
        assert "Phone entries: 3" in content
        assert "PI   " in content
        assert "PHONE" in content
        assert "```" in content
    finally:
        os.unlink(pi_path)
        os.unlink(phone_path)
        if os.path.exists(out_path):
            os.unlink(out_path)
