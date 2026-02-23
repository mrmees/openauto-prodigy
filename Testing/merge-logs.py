#!/usr/bin/env python3
"""Merge Pi protocol log and phone logcat into unified timeline."""

import sys
import re
from pathlib import Path


def parse_pi_log(path):
    """Parse TSV protocol log. Returns list of (seconds, line)."""
    entries = []
    with open(path) as f:
        for line in f:
            if line.startswith("TIME\t"):
                continue  # skip header
            parts = line.strip().split('\t', 1)
            if len(parts) >= 2:
                try:
                    ts = float(parts[0])
                    entries.append((ts, "PI", line.strip()))
                except ValueError:
                    continue
    return entries


def parse_phone_log(path):
    """Parse logcat. Returns list of (seconds_from_midnight, line)."""
    entries = []
    ts_re = re.compile(r'^(\d{2})-(\d{2})\s+(\d{2}):(\d{2}):(\d{2})\.(\d{3})')
    for line in open(path):
        m = ts_re.match(line)
        if m:
            h, mi, s, ms = int(m.group(3)), int(m.group(4)), int(m.group(5)), int(m.group(6))
            secs = h * 3600 + mi * 60 + s + ms / 1000.0
            entries.append((secs, "PHONE", line.strip()))
    return entries


def find_tcp_connect(entries, source):
    """Find TCP connect event for clock alignment."""
    for ts, src, line in entries:
        if src != source:
            continue
        if source == "PI" and "Phone->HU" in line and "VERSION_RESPONSE" in line:
            return ts
        if source == "PHONE" and "Socket connected" in line:
            return ts
    return None


def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <pi-protocol.log> <phone-logcat.log> [output.md]")
        sys.exit(1)

    pi_path, phone_path = sys.argv[1], sys.argv[2]
    out_path = sys.argv[3] if len(sys.argv) > 3 else None

    pi_entries = parse_pi_log(pi_path)
    phone_entries = parse_phone_log(phone_path)

    # Find alignment points
    pi_anchor = find_tcp_connect(pi_entries, "PI")
    phone_anchor = find_tcp_connect(phone_entries, "PHONE")

    if pi_anchor is None or phone_anchor is None:
        print("WARNING: Could not find TCP connect anchor in both logs.")
        print(f"  Pi anchor: {pi_anchor}, Phone anchor: {phone_anchor}")
        print("  Falling back to no alignment (raw timestamps).")
        offset = 0
    else:
        offset = pi_anchor - phone_anchor
        print(f"Clock offset: Pi leads phone by {offset:.3f}s")

    # Normalize phone timestamps to Pi time
    merged = []
    for ts, src, line in pi_entries:
        merged.append((ts, src, line))
    for ts, src, line in phone_entries:
        merged.append((ts + offset, src, line))

    merged.sort(key=lambda x: x[0])

    # Output
    out = open(out_path, 'w') if out_path else sys.stdout
    out.write("# Merged Protocol Timeline\n\n")
    out.write(f"Pi entries: {len(pi_entries)}, Phone entries: {len(phone_entries)}\n")
    out.write(f"Clock offset: {offset:.3f}s\n\n")
    out.write("```\n")
    for ts, src, line in merged:
        prefix = "PI   " if src == "PI" else "PHONE"
        out.write(f"{ts:10.3f}  [{prefix}]  {line}\n")
    out.write("```\n")
    if out_path:
        out.close()
        print(f"Written to {out_path}")


if __name__ == "__main__":
    main()
