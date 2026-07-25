# scripts/

Development helper scripts. None mutate tracked files or remote state unless
their description explicitly says so. Review artifacts and state are written
only under gitignored `reviews/`.

- **`review-gate.sh`** — author-aware pre-push review gate (see AGENTS.md § One bounded review gate). It routes Codex work to Opus (Fable for `--major`) and Claude work to Codex, captures immutable SHAs, pins `high` effort, and enforces one initial plus one remediation pass. Reviews have no wall-clock autokill. State and structured verdicts land in `reviews/`. Exit codes distinguish usage, missing runtimes, reviewer failure, third-pass refusal, duplicate HEAD, and changed history.
- **`codex-review.sh`** — compatibility wrapper for explicitly requested Codex reviews. New workflows should use `review-gate.sh --author ...` so reviewer independence is preserved.
- **`tests/test_review_gate.sh`** — isolated fake-reviewer test for routing, immutable ranges, duplicate rejection, pass-two delta scope, and hard pass-three refusal.
- **`validate-resolutions.sh`** — launches the app at target resolutions for visual testing. Interactive Xvfb+VNC per resolution by default; `--screenshot` for automated captures; `--native` for a local display; `-r WxH` for a single resolution. Xvfb modes refuse an occupied `:99` display without signaling its owner; cleanup is limited to children launched by the current invocation.
- **`check-doc-links.py`** — verifies relative markdown links in live docs resolve (archive dirs exempt). Run from anywhere: `python3 scripts/check-doc-links.py`; exits non-zero on broken links.
