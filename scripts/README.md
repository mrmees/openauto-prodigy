# scripts/

Development helper scripts. All are safe to run — none mutate the repo or remote state.

- **`codex-review.sh`** — pre-push Codex review gate (see AGENTS.md § Review gate). Reviews `@{upstream}..HEAD` in a read-only sandbox; pass an explicit base ref to override the range (`bash scripts/codex-review.sh <base-ref>`). Findings land as structured P1/P2/P3 verdicts in `reviews/` (gitignored). Exit codes: `0` ok/empty findings, `1` usage error, `2` codex CLI not installed, `4` codex run failed.
- **`validate-resolutions.sh`** — launches the app at target resolutions for visual testing. Interactive Xvfb+VNC per resolution by default; `--screenshot` for automated captures; `--native` for a local display; `-r WxH` for a single resolution.
- **`check-doc-links.py`** — verifies relative markdown links in live docs resolve (archive dirs exempt). Run from anywhere: `python3 scripts/check-doc-links.py`; exits non-zero on broken links.
