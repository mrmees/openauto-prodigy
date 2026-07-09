# Theme/Wallpaper Upload Endpoint — Pre-Design Context Notes

**Status:** RESEARCH NOTES ONLY — brainstorm interrupted at the first clarifying question (session context limit). No design decided. Next session: resume `superpowers:brainstorming` from these notes (questions → approaches → design doc → writing-plans).
**Date:** 2026-07-07. Explored by subagent with file:line evidence; spot-verify line numbers before relying on them (develop @ fb05cec).

## What this feature is

Replace the legacy port-9876 theme/wallpaper transfer (`CompanionListenerService`) with a web-config HTTP upload endpoint. Locked prior decisions (2026-07-05 gap review, Matthew):
- Multipart upload → validate → apply via existing IPC (`docs/wishlist.md` promoted item).
- Web-config is the canonical HTTP channel; blobs do NOT ride the External API (256 KiB frame cap; `docs/roadmap-current.md:55`).
- Companion runs dual-stack (9876 for theme transfer only) until this ships; 9876 retirement gates on it. Companion will build its client only after this endpoint's contract is delivered (`openauto-companion/docs/plans/api-v1-migration.md:112-122,287-290`).

## Key exploration findings

1. **The apply path already exists**: `ThemeService::importCompanionTheme(name, seed, dayColors, nightColors, wallpaperJpeg)` (`ThemeService.cpp:505-590`) writes the theme dir under `~/.openauto/themes/<slug>/` (theme.yaml v2.0.0 + wallpaper.jpg), rescans, auto-switches via `setTheme(slug)` (persists `display.theme`). The endpoint is transport plumbing in front of this.
2. **Legacy wire (being replaced)**: HMAC-session TCP 9876, newline JSON; theme = `{name, seed, light:{...}, dark:{...}}` (camelCase M3 roles → hyphenated via `applyReceivedTheme`, `CompanionListenerService.cpp:616-632`); wallpaper = base64 64KB chunks (`theme_data` msgs), **5MB cap** (`.cpp:561-565`). KNOWN BUG not to repeat: `theme_ack` is `accepted:true` unconditionally even when import fails (`.cpp:644-651`) — new endpoint must return real success/failure.
3. **Web-config**: Flask (`web-config/server.py`), talks newline-JSON over unix socket `/tmp/openauto-prodigy.sock` to `IpcServer.cpp` (flat if/else dispatch, `IpcServer.cpp:115-152`). No upload route exists; **NO AUTH anywhere; binds 0.0.0.0:8080** (`server.py:165-171`). `handleSetTheme` (`IpcServer.cpp:234-279`) is good precedent: path-safe id regex `^[A-Za-z0-9._-]{1,64}$`, writes theme.yaml, live-applies. IPC unix socket is `QLocalServer::WorldAccessOption` (world-writable).
4. **Companion client**: `ThemeTransfer.kt` (64KB chunks) + `ThemeBuilderScreen.kt` — themes come from REAL user photos (photo picker → androidx Palette seeds → Material Color Utilities full M3 light+dark scheme). Endpoint must accept arbitrary user JPEGs, not curated palettes.
5. **Wallpaper config model**: `display.wallpaper_override` (`""` theme default / `"none"` / `file://` path); user wallpapers scanned from `~/.openauto/wallpapers/*.{jpg,jpeg,png,webp}`; `wallpaperSource` Q_PROPERTY is cache-busted with mtime.

## Open questions for Matthew (resume here — ONE at a time)

**Q1 — Auth posture (asked, NOT answered; session ended):** web-config is fully unauthenticated incl. `set_config`, so:
  (a) match web-config (none) + wishlist a proper all-routes auth pass separately — RECOMMENDED (theme upload adds nothing set_config can't already do worse);
  (b) shared-secret header on just this endpoint (security theater while set_config is open; drags pairing state into Flask);
  (c) full web-config auth pass now (3× scope).

**Q2 — Scope:** companion-only endpoint, or also a browser-facing upload UI (themes.html: upload wallpaper to `~/.openauto/wallpapers/`, maybe install theme zip)? Companion-only is the promoted item; browser UI is a natural cheap add-on — ask.

**Q3 — Blob-over-IPC transport (design detail, likely controller-decidable):** "apply via existing IPC" ≠ base64ing 5MB into one JSON line. Cleaner: Flask writes wallpaper to a temp file (same host), passes the PATH over IPC; Qt side validates (size cap, JPEG magic bytes, canonical path) and hands bytes to `importCompanionTheme`. Check IpcServer's line-reader buffer behavior before considering inline base64 at all.

**Q4 — Contract deliverable:** the design doc must include the exact HTTP contract (endpoint, multipart part names, field schema, size caps, response codes/body) as a handoff artifact for the companion maintainer (they're explicitly waiting on it).

## Constraints to carry into the design

- Real success/failure in the response (fix the ack-lies bug); map ThemeService failures to 4xx/5xx.
- Reuse the 5MB wallpaper cap (or justify a change); enforce in Flask (reject before buffering unbounded) AND on the Qt side.
- Theme color keys arrive camelCase from Material Color Utilities — the hyphenated conversion (single 42-token vocabulary) must live on the head-unit side, same as legacy.
- Slug/id validation per `handleSetTheme` precedent; no path traversal via theme name.
- Dual-stack transition: do NOT touch `CompanionListenerService` in this work item (retirement is a separate gated step).
