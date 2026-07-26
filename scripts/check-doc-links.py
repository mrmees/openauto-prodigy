#!/usr/bin/env python3
"""Check that relative markdown links in live docs resolve. Archive dirs are exempt."""
import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
SKIP_DIRS = {"docs/archive", "build", "build-pi", "libs/prodigy-oaa-protocol/proto",
             "reviews", ".git", ".superpowers", "node_modules"}
LINK = re.compile(r"\[[^\]]*\]\(([^)#\s]+)(?:#[^)]*)?\)")

def usage(stream=sys.stdout):
    print("Usage: check-doc-links.py [--scope tracked-live]", file=stream)


def live_md_files(tracked_only=False):
    if tracked_only:
        result = subprocess.run(
            ["git", "-C", str(REPO), "ls-files", "-z", "--", "*.md"],
            capture_output=True,
        )
        if result.returncode != 0:
            detail = result.stderr.decode(errors="replace").strip()
            print(f"ERROR: git ls-files failed: {detail}", file=sys.stderr)
            sys.exit(2)
        candidates = (
            REPO / entry.decode()
            for entry in result.stdout.split(b"\0")
            if entry
        )
    else:
        candidates = REPO.rglob("*.md")

    for p in candidates:
        if not p.is_file():
            continue
        rel = p.relative_to(REPO).as_posix()
        if not any(rel == d or rel.startswith(d + "/") for d in SKIP_DIRS):
            yield p

args = sys.argv[1:]
if args in (["-h"], ["--help"]):
    usage()
    sys.exit(0)
if args not in ([], ["--scope", "tracked-live"]):
    usage(sys.stderr)
    sys.exit(2)

bad = 0
for md in live_md_files(tracked_only=bool(args)):
    for target in LINK.findall(md.read_text(errors="replace")):
        if target.startswith(("http://", "https://", "mailto:")):
            continue
        resolved = (md.parent / target).resolve()
        if not resolved.exists():
            print(f"BROKEN: {md.relative_to(REPO)} -> {target}")
            bad += 1
print(f"{'FAIL' if bad else 'OK'}: {bad} broken links")
sys.exit(1 if bad else 0)
