# WebEngine Spike Results — Pi 4 Go/No-Go (Phase C, part 1)

Status: COMPLETED 2026-07-05

**Date:** 2026-07-05
**Verdict: GO** — for the 8GB unit outright, and extrapolated GO for 4GB boards.
**Hardware:** Pi 4 **8GB** (the Miata unit; note: docs previously assumed 4GB worst case), RPi OS Trixie, labwc, 1024×600. Qt 6.8.2, `qml6-module-qtwebengine` 6.8.2+dfsg-4 (installed this session), `qml-qt6` runner.
**Method:** standalone QML harness (`Window` + N `WebEngineView`s) loading a local HTML widget with a 60fps canvas gauge animation + live clock — deliberately heavier than realistic idle widgets. Memory = summed **PSS** (`/proc/*/smaps_rollup`) across the harness process tree including all `QtWebEngineProcess` children. Harness + measure script preserved on the Pi at `~/spike/` and reproducible from this doc.

## Thresholds (defined before measurement) and results

| # | Threshold | Result | Verdict |
|---|---|---|---|
| T1 | 1 view total footprint ≤ 350 MB PSS | **229 MB** (6 procs) | PASS |
| T2 | Marginal per extra view ≤ 150 MB | **+15 MB** (2nd), **+14 MB** (3rd) | PASS |
| T3 | AA + 2 views leaves ≥ 4.5 GB avail (8GB unit) | **7.17 GB** avail | PASS |
| T4 | No visible AA stutter; prodigy CPU rise < 20 pts | Smooth (Matthew, visual); prodigy 25% → 25% (top, instantaneous) | PASS |
| T5 | Load start → loadFinished < 3 s (local content) | **815 ms** (first view; 777–873 ms across runs) | PASS |
| T6 | Renderer SIGKILL: harness survives, view reloadable | `onRenderProcessTerminated` fired, auto-reload recovered ~4 s, compositor + memory unaffected | PASS |

CPU context for T4: the two animated views cost ~115% of the Pi's 400% CPU budget (qml6 ~42% + 2 renderers ~25% each) **at worst-case 60fps animation**; prodigy's AA decode held steady at 25%.

## Key findings beyond the numbers

1. **Same-origin views share one renderer process** — 3 views = 8 processes but only +29 MB over 1 view. Prodigy widgets served from one local origin will share; even hostile per-origin isolation would add ~60–100 MB/origin, still within budget.
2. **4GB extrapolation:** prodigy (~250 MB) + system (~550 MB) + 3 web widgets (~260 MB) ≈ 1.1 GB — leaves ~2.5 GB on a 4GB board. GO there too; revisit only if widget count balloons.
3. **Crash recovery pattern for WebWidgetHost:** handle `onRenderProcessTerminated` with a debounced `reload()` — proven under SIGKILL. This becomes the standard recovery block in `WebWidgetHost.qml`.
4. **First-load latency (~800 ms)** argues for lazy view instantiation per dashboard page (don't pre-warm all pages' web widgets), consistent with existing HomeMenu loader behavior.

## Gotchas recorded for executors

- The QML runner binary on Trixie is **`qml6`** (package `qml-qt6`); `/usr/lib/qt6/bin/qml` also works. Wayland env needed over SSH: `XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-0 QT_QPA_PLATFORM=wayland`.
- **`pkill -f` inside an ssh one-liner kills the ssh session itself** if the pattern matches the remote shell's own command line (which contains your whole command). Use `pkill -x <binary>`. This cost two silent exit-255 rounds this session. Candidate for CLAUDE.md gotchas.
- `ps -o pcpu` is lifetime-average CPU, useless for in-session load questions — use `top -bn1`.
- A bare QML `Window` on labwc has no close affordance and sits above the AA surface; any long-lived harness needs a kill path (`pkill -x qml6`).

## Consequence

The HTML/JS runtime (Phase C part 2: `prodigy` JS bridge + `WebWidgetHost` + packaging) proceeds unconditionally on the architecture fixed in `2026-07-05-extensibility-architecture-design.md` §5 (web widgets are WebSocket API clients; no QWebChannel). The fallback design (native-QML-only widgets) is moot and will not be written.
