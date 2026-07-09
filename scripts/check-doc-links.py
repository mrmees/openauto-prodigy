#!/usr/bin/env python3
"""Check that relative markdown links in live docs resolve. Archive dirs are exempt."""
import re, sys, pathlib

REPO = pathlib.Path(__file__).resolve().parent.parent
SKIP_DIRS = {"docs/archive", "build", "build-pi", "libs/prodigy-oaa-protocol/proto",
             "reviews", ".git", ".superpowers", "node_modules"}
LINK = re.compile(r"\[[^\]]*\]\(([^)#\s]+)(?:#[^)]*)?\)")

def live_md_files():
    for p in REPO.rglob("*.md"):
        rel = p.relative_to(REPO).as_posix()
        if not any(rel == d or rel.startswith(d + "/") for d in SKIP_DIRS):
            yield p

bad = 0
for md in live_md_files():
    for target in LINK.findall(md.read_text(errors="replace")):
        if target.startswith(("http://", "https://", "mailto:")):
            continue
        resolved = (md.parent / target).resolve()
        if not resolved.exists():
            print(f"BROKEN: {md.relative_to(REPO)} -> {target}")
            bad += 1
print(f"{'FAIL' if bad else 'OK'}: {bad} broken links")
sys.exit(1 if bad else 0)
