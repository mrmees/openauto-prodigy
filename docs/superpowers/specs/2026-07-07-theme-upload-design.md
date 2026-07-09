# Theme/Wallpaper Upload Endpoint — Design

**Date:** 2026-07-07
**Status:** APPROVED (brainstorm complete; Matthew approved architecture, endpoint name, require-both-schemes, 5 MiB cap). Ready for implementation planning.
**Supersedes exploration:** `docs/superpowers/specs/2026-07-07-theme-upload-context-notes.md`
**Baseline:** develop @ f66a387. Line numbers verified against this tree; spot-check before relying on them.

## 1. Goal & Context

Move companion **theme + wallpaper transfer** off the legacy HMAC-session TCP port **9876** (`CompanionListenerService`) onto a **web-config HTTP upload endpoint** that validates the payload and applies it via the *existing* `ThemeService::importCompanionTheme()`. This endpoint is the deliverable the companion maintainer is blocked on; shipping it gates the retirement of port 9876.

**Locked prior decisions** (2026-07-05 gap review + this brainstorm):
- Web-config is the canonical HTTP channel for blobs; theme/wallpaper data does **not** ride the External API (256 KiB frame cap — `docs/roadmap-current.md`).
- Multipart upload → validate → apply via existing IPC.
- **Auth (Q1):** none, matching the rest of web-config. Web-config already binds `0.0.0.0:8080` with **zero authentication** on any route, including `set_config`, which can change any setting. This endpoint adds no capability `set_config` does not already expose unauthenticated, so per-endpoint auth would be theater. A proper **all-routes web-config auth pass** is filed separately on the wishlist.
- **Scope (Q2):** companion-only endpoint now. A browser-facing upload UI in `themes.html` is a natural fast-follow (same endpoint) and is out of scope here.
- **Blob transport (Q3):** temp-file handoff — Flask writes the wallpaper to a temp file and passes the **path** over IPC (not base64 inline).

## 2. Non-Goals

- No auth on this endpoint (see §10; all-routes auth is a separate wishlist item).
- No browser upload UI (fast-follow).
- **No changes to `CompanionListenerService`** — legacy 9876 stays dual-stack until its separately-gated retirement. The camelCase→hyphen color conversion is therefore *duplicated* rather than shared (dedup happens at 9876 retirement).
- No theme *editing* UI changes — the existing `set_theme` path (`/api/theme` POST) is untouched.

## 3. Architecture & Data Flow

```
Companion  ──HTTP POST multipart──▶  Flask  POST /api/theme/install  (web-config/server.py)
                                        │  (1) cheap early reject: MAX_CONTENT_LENGTH → 413; wallpaper part must be image/jpeg;
                                        │      manifest must be parseable JSON with non-empty name
                                        │  (2) write wallpaper bytes → temp file  /tmp/oap-theme-upload/<rand>.jpg  (0700 dir)
                                        ▼
                        IPC  {"command":"install_theme","data":{name, seed, light{}, dark{}, wallpaper_path}}
                                        │   (tiny JSON line over the unix socket — no blob on the wire; 5s IPC timeout is ample)
                                        ▼
                        IpcServer::handleInstallTheme(data)      (src/core/services/IpcServer.cpp — AUTHORITATIVE validation)
                                        │  name len 1–64 · light & dark both non-empty & every value a valid QColor
                                        │  wallpaper_path (if present): canonical path under /tmp/oap-theme-upload/ + is-regular-file;
                                        │      read bytes; ≤ 5 MiB; JPEG magic FF D8 FF
                                        │  camelCase→hyphen convert  (light→day map, dark→night map)
                                        ▼
                        ThemeService::importCompanionTheme(name, seed, dayColors, nightColors, wallpaperJpeg) → bool
                                        │   (writes ~/.openauto/themes/<slug>/{theme.yaml,wallpaper.jpg}, rescans, auto-applies)
                                        ▼
                        IPC response  {"ok":true,"slug":"<slug>"}  |  {"ok":false,"error":"<reason>"}
                                        ▼
Flask maps ok/error → HTTP status + JSON body;   finally: os.unlink(temp file)  (Qt already read it — synchronous IPC)
```

**Temp-file lifecycle:** Flask owns it end-to-end — creates it, passes the path, then deletes it in a `finally` block **after** the synchronous `ipc_request()` returns (Qt has finished reading the bytes by the time it responds, so there is no read-before-delete race). Qt never deletes it. If Flask crashes between write and unlink, a stale temp file leaks into `/tmp/oap-theme-upload/`; acceptable (bounded dir, cleaned on reboot; a future janitor is a wishlist nicety, not a v1 need).

## 4. HTTP Contract  *(companion-maintainer handoff artifact)*

### `POST /api/theme/install`
`Content-Type: multipart/form-data`

| Part        | Kind                    | Required | Constraints |
|-------------|-------------------------|----------|-------------|
| `manifest`  | text form field (JSON)  | yes      | UTF-8 JSON string, schema below |
| `wallpaper` | file part               | no       | `Content-Type: image/jpeg`; ≤ **5 MiB** (5 × 1024 × 1024 bytes). Omit for a color-only theme. |

> **Companion-maintainer note:** send `manifest` as a plain multipart form field (no `filename` on that part). If the part carries a filename, Werkzeug/Flask routes it to `request.files` instead of `request.form` and the endpoint returns **400 "missing manifest"**. Only the `wallpaper` part carries a filename.

**`manifest` JSON schema:**
```jsonc
{
  "name": "Sunset Vibes",        // REQUIRED — display name, 1–64 chars. Slugified host-side for the dir/id.
  "seed": "#FF8A65",             // optional — source seed color, stored for future regeneration; not required to apply.
  "light": {                      // REQUIRED, non-empty — Material 3 light scheme
    "primary": "#FF8A65",
    "onPrimary": "#3A0B00",
    "surfaceContainerHigh": "#2B2320"
    // … full M3 role set as emitted by Material Color Utilities
  },
  "dark": {                       // REQUIRED, non-empty — Material 3 dark scheme
    "primary": "#FFB59D",
    "onPrimary": "#5A1B00"
    // …
  }
}
```
- **Color role keys are camelCase** (`onPrimary`, `surfaceContainerHigh`) exactly as Material Color Utilities / androidx `Palette` emit them — the head unit converts them to the hyphenated `theme.yaml` vocabulary (`onPrimary`→`on-primary`) by inserting a hyphen before each uppercase letter and lowercasing it. **The companion sends precisely what it sends over legacy 9876 today** (same `{name, seed, light{}, dark{}}` shape — see `CompanionListenerService::applyReceivedTheme`, `src/core/services/CompanionListenerService.cpp:609-635`).
- Color **values** are hex strings: `#RRGGBB` or `#AARRGGBB` (anything `QColor(QString)` accepts). Each must parse to a valid color, or the request is rejected (improvement over legacy, which stored invalid colors silently).
- `light` and `dark` are **both required and non-empty** (a theme with no colors is rejected). The full 42-role M3 set is *not* enforced — whatever valid roles are present are stored (matches legacy leniency; missing roles fall back at load time).

### Responses (JSON body)

| Status | Body | Meaning |
|--------|------|---------|
| `200 OK` | `{"installed": true, "slug": "sunset-vibes", "applied": true}` | Theme written to `~/.openauto/themes/<slug>/` and auto-applied (it becomes the active theme). |
| `400 Bad Request` | `{"installed": false, "error": "<reason>"}` | Malformed/missing manifest, empty name, name > 64 chars, empty `light`/`dark`, invalid color value, non-JPEG wallpaper, or bad JPEG magic bytes. |
| `413 Payload Too Large` | `{"installed": false, "error": "payload too large"}` | Request body / wallpaper exceeds the cap (Flask `MAX_CONTENT_LENGTH`). |
| `500 Internal Server Error` | `{"installed": false, "error": "<reason>"}` | `importCompanionTheme()` returned false (e.g. could not write theme files). |
| `503 Service Unavailable` | `{"installed": false, "error": "Qt app not running"}` | IPC socket missing/unreachable — matches existing web-config graceful degradation. |

`installed` reflects the **real** `importCompanionTheme()` boolean. This fixes the legacy defect where `theme_ack.accepted` was hard-coded `true` even when the import failed (`CompanionListenerService.cpp:638` computes `ok` but `:648` sends `accepted:true` regardless).

### Example (curl)
```bash
curl -X POST http://10.0.0.1:8080/api/theme/install \
  -F 'manifest={"name":"Sunset Vibes","seed":"#FF8A65","light":{...},"dark":{...}};type=application/json' \
  -F 'wallpaper=@/path/to/photo.jpg;type=image/jpeg'
# → 200 {"installed":true,"slug":"sunset-vibes","applied":true}
```

## 5. Flask Endpoint (`web-config/server.py`)

New route beside the existing `/api/theme` GET/POST (`server.py:110-120`):
```python
@app.route("/api/theme/install", methods=["POST"])
def api_install_theme():
    ...
```
Responsibilities:
1. **Size guard:** set `app.config["MAX_CONTENT_LENGTH"]` (module-level, ≈ 6 MiB = 5 MiB wallpaper + JSON headroom) so oversize bodies get a Flask-native `413` *before* buffering. (Confirm this is set once at app config, not per-route.)
2. **Parse `manifest`** from `request.form["manifest"]` as JSON; on failure → `400`. Require non-empty `name` → else `400`.
3. **Wallpaper (optional):** if `request.files` has `wallpaper`, require its mimetype be `image/jpeg` (→ `400` otherwise); create `/tmp/oap-theme-upload/` (mode 0700) if missing; write bytes to a temp file there (`tempfile.mkstemp(dir=..., suffix=".jpg")`).
4. **IPC:** `ipc_request("install_theme", {name, seed, light, dark, wallpaper_path})` — omit `wallpaper_path` when there is no wallpaper. Reuses the existing `ipc_request()` helper (`server.py:23-54`; sends `{"command","data"}\n`, reads newline-delimited response, 5 s timeout).
5. **Map response → HTTP:** `{"ok":true}` → `200`; `{"error":"Qt app not running (...)"}` (socket missing) → `503`; other `{"ok":false,"error":...}` → `500` for apply failure, `400` for validation-class errors. (Handler returns a machine-distinguishable `error` — see §6 — so Flask can pick 400 vs 500; simplest: handler returns an `code` field or Flask treats validation errors it can detect itself as 400 and any Qt `ok:false` as 500. **Decision:** Flask does all *format/type* validation itself (→400); any `ok:false` from Qt is treated as `500` except the socket-missing case (→503). Qt-side validation failures are defense-in-depth and should be unreachable when Flask validates first, so mapping them to 500 is acceptable.)
6. **Cleanup:** `finally: os.unlink(temp_path)` if a temp file was created.

## 6. IPC Command — `install_theme`

Add to the `IpcServer::handleRequest` dispatch (`src/core/services/IpcServer.cpp:126-151`), beside `set_theme`:
```cpp
if (command == QLatin1String("install_theme"))
    return handleInstallTheme(data);
```
`handleInstallTheme(const QVariantMap& data)` returns the established shape (`{"ok":true,"slug":...}` / `{"ok":false,"error":...}` — same convention as `handleSetTheme`, `IpcServer.cpp:234-279`). Responsibilities (authoritative validation — never trust Flask alone):
1. Guard `themeService_` present → else `{"ok":false,"error":"Theme service not available"}`.
2. `name` = `data["name"]`; reject if empty or length > 64.
3. `light`/`dark` = `data["light"].toMap()` / `data["dark"].toMap()`; reject if either empty. For each entry, convert the **camelCase key → hyphenated** (hyphen before each uppercase, lowercase — the exact `parseColorMap` transform at `CompanionListenerService.cpp:616-632`) and parse the value with `QColor`; reject if any value is an invalid QColor. Build `QMap<QString,QColor> dayColors` (from `light`) and `nightColors` (from `dark`).
4. `wallpaper_path` = `data["wallpaper_path"]` (optional). If present & non-empty:
   - Canonicalize (`QFileInfo::canonicalFilePath()`); reject if the canonical path does not sit under the real path of `/tmp/oap-theme-upload/`, or is not a regular file (path-injection / symlink defense — even though Flask generates the path).
   - Read bytes; reject if size > 5 MiB; reject if the first 3 bytes are not `FF D8 FF` (JPEG magic).
   - → `QByteArray wallpaperJpeg`. Else empty (`importCompanionTheme` skips the wallpaper write when empty — `ThemeService.cpp:532`).
5. Compute `slug = ThemeService::slugify(name)` (see §7) for the response.
6. `bool ok = themeService_->importCompanionTheme(name, seed, dayColors, nightColors, wallpaperJpeg);`
7. Return `{"ok":ok,"slug":slug}` (include `error` when `!ok`).

**Duplicated conversion:** the camelCase→hyphen lambda is copied from `CompanionListenerService` (frozen). Noted for dedup at 9876 retirement.

**Threading:** `handleInstallTheme` runs synchronously on the Qt **main thread** (IpcServer is a `QLocalServer` whose read callbacks fire on the main thread). It reads the ≤5 MiB wallpaper, writes `theme.yaml` + `wallpaper.jpg`, rescans, and applies — a brief (<~1 s) UI stall while it runs. This is acceptable and matches the legacy path (`applyReceivedTheme` also ran on the main thread). No worker-thread offload in v1; if theme installs ever grow slow enough to jank the UI noticeably, that's a future optimization, not a v1 requirement.

## 7. ThemeService — minimal touch

`importCompanionTheme(name, seed, dayColors, nightColors, wallpaperJpeg)` (`ThemeService.cpp:505-590`) already does everything: slugifies the name, writes `~/.openauto/themes/<slug>/{theme.yaml (v2.0.0),wallpaper.jpg}`, rescans, and auto-switches (`setTheme(slug)` persists `display.theme`). **It needs no changes to the apply logic.**

One small, behavior-preserving refactor so the IPC handler can report the canonical slug without duplicating the algorithm: **extract the existing inline slugify** (`ThemeService.cpp:511-517` — lowercase, `[^a-z0-9]+`→`-`, trim hyphens, empty→`companion-theme`) into `static QString ThemeService::slugify(const QString& name)`, and have `importCompanionTheme` call it. The IPC handler calls the same static for its response `slug`. This touches **only ThemeService** (its other caller, `CompanionListenerService`, uses `importCompanionTheme`, not the slug helper — so the frozen file is unaffected).

## 8. Validation Matrix (two-sided; Qt authoritative)

| Check | Flask (fail-fast) | Qt (authoritative) |
|-------|-------------------|--------------------|
| Body/wallpaper size cap | `MAX_CONTENT_LENGTH` → 413 | re-checks bytes ≤ 5 MiB |
| Wallpaper is JPEG | mimetype `image/jpeg` | magic bytes `FF D8 FF` |
| Manifest is JSON w/ name | parse + non-empty name | name len 1–64 |
| `light`/`dark` present | — | both non-empty |
| Color values valid | — | each parses to valid `QColor` |
| Temp path safety | Flask generates the path | canonical path under `/tmp/oap-theme-upload/` + regular file |
| Name path traversal | — | inherent: `slugify` collapses `[^a-z0-9]+`→`-`, so no `/` or `..` survives |

## 9. Error Handling

- Every failure returns a real status + JSON `error`; **no silent success**. `installed`/`ok` is the true `importCompanionTheme` result.
- IPC unreachable (Qt app down) → Flask returns `503` (reuses the existing `ipc_request` "Qt app not running" error string, `server.py:48`).
- Qt-side validation failures (unreachable if Flask validated) map to `500`; they exist as defense-in-depth, not as the primary gate.

## 10. Security Posture

- **No authentication**, matching every other web-config route (Q1). The endpoint's worst case (writing a theme dir + switching the active theme) is strictly less powerful than the already-unauthenticated `set_config`. Filed: **wishlist — all-routes web-config auth pass** (the honest fix, done once across the panel).
- **Path safety:** the wallpaper path is Flask-generated (not client-controlled), and Qt still validates canonical-under-temp-prefix + regular-file. Theme name cannot traverse (slugify). Theme id regex precedent: `handleSetTheme` uses `^[A-Za-z0-9._-]{1,64}$` (`IpcServer.cpp:240`) — our path safety comes from slugify instead, which is stricter (no `.`).
- **DoS bound:** 5 MiB cap enforced before unbounded buffering (Flask `MAX_CONTENT_LENGTH`) and again in Qt.

## 11. Testing

**C++ unit tests** (new `tests/test_ipc_install_theme.cpp` or extend an IpcServer test, wired into CMake):
- Happy path: valid manifest + JPEG → `ok:true`, slug correct, theme dir + wallpaper written, theme applied.
- Color-only (no `wallpaper_path`) → `ok:true`, no wallpaper file.
- Oversize wallpaper (> 5 MiB temp file) → `ok:false`.
- Non-JPEG magic bytes → `ok:false`.
- Empty `name` / name > 64 → `ok:false`.
- Empty `light` or `dark` → `ok:false`.
- Invalid color value → `ok:false`.
- `wallpaper_path` outside `/tmp/oap-theme-upload/` (e.g. `/etc/hostname`) → `ok:false` (path-injection rejected).
- camelCase→hyphen conversion correctness (`onPrimary`→`on-primary`, `surfaceContainerHigh`→`surface-container-high`) in the written `theme.yaml`.
- `slugify` static: unit-test a few names incl. the empty→`companion-theme` fallback, and assert `importCompanionTheme` still produces the same dir as before the refactor.

**Flask route test** (against a mock unix socket, per existing web-config test approach):
- Multipart parse + IPC command shape (`install_theme`, data fields).
- 413 on oversize body.
- 503 when the socket is absent.
- Status-code mapping (200/400/500).
- Temp file is unlinked in the `finally` (assert it's gone after the call, on both success and IPC-error paths).

## 12. Wishlist / Follow-ups (to file with the design commit)

- **All-routes web-config auth pass** — web-config is fully unauthenticated (binds `0.0.0.0:8080`; `set_config`, `install_theme`, all of it). Do a single proper auth pass across every route rather than per-endpoint theater. (From this design's Q1.)
- **Browser-facing theme/wallpaper upload UI** in `themes.html` — fast-follow hitting the same `/api/theme/install` endpoint (drag-drop, preview, progress). (Q2.)
- **Dedup camelCase→hyphen conversion + slugify sharing** at 9876 retirement — collapse the `CompanionListenerService` copy into the shared path once the legacy service is deleted.
- **(nice-to-have) `/tmp/oap-theme-upload/` janitor** — reboot clears it; a periodic sweep or per-request stale-file cleanup would tidy leaked temp files from a crashed Flask mid-request.

## 13. File-Touch Summary

**Changed:**
- `web-config/server.py` — new `POST /api/theme/install` route + `MAX_CONTENT_LENGTH` config.
- `src/core/services/IpcServer.cpp` / `.hpp` — `install_theme` dispatch + `handleInstallTheme`.
- `src/core/services/ThemeService.cpp` / `.hpp` — extract `static QString slugify(const QString&)`; `importCompanionTheme` calls it (behavior-preserving).
- `tests/` + `tests/CMakeLists.txt` — new IPC install-theme tests (+ slugify test).
- `docs/wishlist.md` — the four follow-ups above.

**Frozen (do NOT touch):**
- `src/core/services/CompanionListenerService.cpp` — legacy 9876 dual-stack until separately-gated retirement.
- `proto/api/`, `libs/prodigy-oaa-protocol/`.
