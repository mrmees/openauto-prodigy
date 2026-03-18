# Retrospective

## Milestone: v0.6.4 — Widget Work

**Shipped:** 2026-03-17
**Phases:** 3 (19-21) | **Plans:** 6 | ~5 hours

### What Was Built
- Per-instance widget configuration: ConfigSchemaField types (Enum, Bool, IntRange), schema validation, gear icon config sheet, YAML persistence with remap survival
- Four utility widgets: theme cycle (tap-to-advance), battery (companion data), companion status (1x1→expanded), AA focus toggle (projection state control)
- Clock widget restructured with 3 visual styles (digital/analog/minimal) via Loader Component switching, per-instance style selection
- WeatherService: Open-Meteo HTTP fetch, coordinate-rounded per-location cache, subscriber-aware refresh timer (shortest interval wins), safe cache eviction protecting subscribed entries
- WeatherWidget: 3 responsive breakpoints (1x1 temp → 2x1 icon+temp → 3x3+ extended), GPS lifecycle via CompanionService.gpsStale

### What Worked
- Parallel wave execution: both Phase 21 plans ran simultaneously (~7min each vs sequential ~14min)
- Code review (Codex) caught the refresh interval resubscription bug — changing interval in config sheet wouldn't take effect until page hide/show cycle. One-line fix.
- Phase ordering (config infrastructure → simple widgets → config-dependent widgets) meant Phase 20 widgets were functional without waiting for Phase 19
- 14 new weather service unit tests caught cache eviction edge cases during implementation — subscriber-protected entries were being evicted by auto-cleanup in getWeatherData()

### What Was Inefficient
- SUMMARY.md files had no `one_liner` frontmatter field — milestone completion had to derive accomplishments from commit messages and plan objectives
- Phase 21 weather plan needed 5 rounds of checker revision (subscriber lifecycle, safe eviction, gpsStale, test seams, interval tracking) before execution — the plan was ambitious for what is effectively a data-fetching service
- CompanionStatusWidget GPS row uses `lat !== 0` check instead of gpsStale — inconsistent with WeatherWidget's correct approach. Should have been caught during plan review, not integration check

### Patterns Established
- ConfigSchemaField types (Enum, Bool, IntRange) as the widget config vocabulary — extensible without changing the sheet renderer
- Subscriber-aware service refresh: service tracks per-location subscriber intervals, uses shortest, skips zero-subscriber locations
- Weather cache eviction protects subscribed entries unconditionally — stale data for active widgets is better than no data
- Loader Component switching for widget style variants — shared root properties, clean style isolation

### Key Lessons
1. Config sheet resubscription is easy to miss — any service with per-instance config that affects a subscription must resubscribe on config change, not just on visibility/location changes
2. Plan checker iterations are expensive but worthwhile — the 5 rounds on weather prevented implementation-time rework on subscriber lifecycle and cache eviction
3. Parallel plan execution within a wave is a real time saver — two independent plans completing in ~7min vs ~14min sequential
4. GPS availability detection should use a single canonical signal (gpsStale) project-wide — multiple widgets checking lat/lon !== 0 vs gpsStale creates inconsistency

### Cost Observations
- Model mix: 100% opus for executors, sonnet for verifier + integration checker
- Sessions: 1
- Notable: Entire milestone (3 phases, 6 plans) in a single session, ~5 hours wall clock

---

## Milestone: v0.6.3 — Proxy Routing Fix

**Shipped:** 2026-03-16
**Phases:** 4 (15-18) | **Plans:** 9 | **Tasks:** 16 | 1 day

### What Was Built
- Root daemon with restricted IPC socket (0660, openauto group, SO_PEERCRED peer credential validation)
- Idempotent iptables routing with flush/recreate model, owner-based + destination-based self-exemption via dedicated redsocks user
- Truthful ACTIVE/FAILED/DEGRADED status with verified pipeline health (TCP connect + iptables + upstream check)
- Startup self-heal cleans all stale proxy state before accepting IPC connections
- Both installers upgraded with consistent migration (group creation, stale rule cleanup, service template rendering)
- End-to-end hardware validation: transparent proxy routing proven on Pi with companion SOCKS bridge (origin IP changed from Pi to phone cellular)

### What Worked
- Phase ordering (privilege → routing → status → hardware) prevented debugging cascading failures — each phase built on verified foundations
- Hardware checkpoints (Phase 16-01 and Phase 18) caught real deployment issues: service needed restart after code deploy, IPv6-mapped addresses from Qt, redsocks config permissions
- Code review (Codex) caught 6 findings on the Phase 18 plan alone: negative baseline had no companion evidence, stale logcat buffer, ADB location mismatch, grep -oP on Trixie, DNS resolution mismatch for tcpdump, wildcard artifact matching
- Clean revalidation after bugfixes produced honest PASS — no manual intervention in the final run

### What Was Inefficient
- Three bugs discovered during hardware validation that should have been caught by unit tests: eth0 skip_interfaces bypass, redsocks config 0600 permissions, IPv6-mapped address prefix. All were in the IPC-to-daemon boundary layer that had no integration-level test coverage.
- Phase 18 initial validation required manual redsocks/iptables setup mid-session — the validation log shows manual intervention, making the first PASS conditional. Had to rerun on clean HEAD to get an honest artifact.
- The `skip_interfaces` default was fixed in proxy_manager.py but not in the IPC validator (openauto_system.py) — two places defining the same default is a latent sync bug
- Qt `peerAddress()` returning IPv6-mapped addresses (`::ffff:x.x.x.x`) was a surprise — no documentation or prior experience flagged this

### Patterns Established
- Flush/recreate model for iptables (never check-and-reuse) — deterministic state on every enable
- TCP connect as listener truth gate (not PID check) — only proves the service is actually accepting connections
- `_enable_locked`/`_disable_locked` pattern avoids self-deadlock on enable→disable rollback
- Four-point proof chain for hardware validation: loopback redirect, outbound bypass, companion receipt, request success

### Key Lessons
1. Two places defining the same default (`["lo", "eth0"]` in both proxy_manager.py and openauto_system.py) is a guaranteed desync bug. Use a single constant.
2. Qt dual-stack sockets return IPv6-mapped addresses — any code writing IP addresses to config files needs to handle `::ffff:` prefix stripping.
3. Hardware validation that requires mid-session bugfixes produces dishonest artifacts — always rerun on clean HEAD for the canonical evidence.
4. `eth0` in skip_interfaces seems harmless until you realize the default route uses eth0 — then it exempts ALL internet traffic from redirect. Network exemption semantics must be thought through against actual routing tables.

---

## Milestone: v0.6.2 — Theme Expression & Wallpaper Scaling

**Shipped:** 2026-03-16
**Phases:** 5 (13, 13.1, 13.2, 14, 14.1) | **Plans:** 6 | 1 day

### What Was Built
- Wallpaper texture memory capped to display dimensions, cover-fit with clip, transition stability
- Companion reconnect hardening (always-replace stale clients, socket cleanup, inactivity timeout)
- Theme persistence (setTheme writes to config, wallpaper picker gated behind toggle)
- M3 color audit: solid primaryContainer fills, surface tint centralization, warning/onWarning tokens, night comfort guardrail (HSL sat clamp 0.55)
- State matrix document as single source of truth for control→token assignments
- 9 companion-created M3 themes promoted to bundled defaults, Prodigy as first-install identity
- FullScreenPicker GPU fix (removed per-delegate MultiEffect shadows)

### What Worked
- Code review loop (Claude + Codex) caught 3 real issues in phase 14: false M3 pairing (0.3 opacity overlays), inaccurate state matrix doc, imprecise switch documentation
- Phase 14.1 context discussion correctly identified this as a promotion/migration phase, not a theme exploration phase — reviewer caught the framing issue
- Inserting phase 14.1 for theme migration was clean — decimal numbering, context capture, plan, execute in one session
- Night comfort guardrail (HSL saturation clamp) is simpler and more effective than HCT chroma math — Qt-native, no external dependencies

### What Was Inefficient
- SCP'd prodigy theme from Pi had stale color values — had to re-pull after companion app resent. Should have verified theme YAML freshness before committing
- User theme `~/.openauto/themes/default/` on Pi was masking the bundled default — not caught until Pi verification. Plan should have identified all potential user theme overrides upfront
- FullScreenPicker freeze with 9+ themes was a surprise GPU limitation — MultiEffect shadows on every ListView delegate overwhelmed Pi 4 GPU. Should test picker with realistic item counts during development
- Three rounds of plan revision for 14.1 (verification gap, Pi deployment, assertion accuracy) — reviewer found real issues each time but the initial plan was too optimistic about verification

### Patterns Established
- Night comfort guardrail via HSL saturation clamp inside activeColor() — applies to tint blends automatically
- State matrix document (`docs/state-matrix.md`) as canonical reference for control→token assignments
- Companion-created themes as bundled defaults — M3 color math from the companion app, not hand-crafted YAML
- FullScreenPicker delegates use flat backgrounds (no MultiEffect shadows) for GPU safety at scale

### Key Lessons
1. User theme override masking is a real deployment issue — bundled themes with the same ID as user themes are invisible. Cleanup must be part of the deployment verification plan.
2. Per-delegate GPU effects (MultiEffect, layer.enabled) don't scale on Pi 4 — 4 items worked fine, 9 froze. Test with realistic list sizes.
3. Theme YAML freshness matters — SCP captures a snapshot, but companion app palettes can change between SCP and deployment. Verify colors match expectations.
4. Code review as a workflow step continues to catch real issues — the M3 pairing fix (solid fills vs 0.3 opacity) was a genuine contrast/accessibility improvement.

---

## Milestone: v0.6.1 — Widget Framework & Layout Refinement

**Shipped:** 2026-03-15
**Phases:** 5 (09-12 + 10.1 insertion) | **Plans:** 10 | 2 days

### What Was Built
- DPI-based grid sizing with diagonal-proportional cellSide formula replacing fixed-pixel math
- Auto-snap threshold (50%, two-pass with cascade guard) recovers gutter waste — 7x3 to 7x4 on 1024x600
- Launcher dock replaced by singleton launcher widgets on protected reserved page
- Page dots moved to navbar flanking the clock, PageIndicator removed from HomeMenu
- Formalized widget contract: WidgetInstanceContext with live colSpan/rowSpan/isCurrentPage, WidgetContextFactory for QML-side creation, Binding-based injection
- All 6 widgets rewritten: span-based breakpoints, widgetContext providers, ActionRegistry egress, pressAndHold forwarding
- Widget developer guide (481 lines) + plugin-api.md refresh + 15 v0.6-v0.6.1 ADRs
- Test suite at 85/85 (fixed pre-existing distance unit bug during audit)

### What Worked
- Code review loop caught real issues before they shipped: cellWidth/cellHeight reactivity, hasMedia contract mismatch, edit-mode z-order conflict, WidgetPicker role off-by-1, stale ThemeService names, wrong distance unit enum
- Integration checker during audit found the WidgetPicker role bug that no unit test could catch (QML uses integer literals, C++ tests use enum names)
- Documentation review loop (5 rounds) eliminated stale API references, wrong plugin IID, missing service docs, and false ADR claims
- Distance unit enum fix (99b88b0) was a pre-existing bug from Phase 06 — the audit process surfaced it naturally
- Phase 10.1 insertion for grid spacing refinement was clean — decimal phase numbering worked well

### What Was Inefficient
- WidgetPicker role numbers should have used named constants or C++ bridge from the start — hardcoded integers in QML are a silent bug factory
- Documentation required 5 review rounds to get factually accurate — each round found 2-4 doc-to-code mismatches. Should have had the executor verify each claim against source during writing, not after
- Plugin-api.md was allowed to go stale across multiple milestones — updating it retroactively in Phase 12 was much harder than keeping it current incrementally
- Phase 06 VERIFICATION.md was never created, creating a gap that propagated to the audit

### Patterns Established
- WidgetContextFactory as dedicated class (not on WidgetGridModel) — keeps model pure data
- Context injection via Loader.onLoaded + Binding elements (not context properties) — typed, NOTIFY-capable
- Span-based breakpoints (colSpan >= N) replacing pixel thresholds — resolution-independent
- ActionRegistry.dispatch for widget command egress — decouples widgets from navigation internals
- widgetMouseArea z:10 in edit mode / z:-1 normal — above widget content, below edit controls
- promoteToBase on every mutation including opacity — all user edits survive remap
- Plugin-facing API docs scoped explicitly ("see headers for full surface") to prevent drift

### Key Lessons
1. Hardcoded enum integers in QML are silent bugs — the C++ model was correct but QML had wrong numbers for months. Use a bridge or named constants.
2. Documentation accuracy requires code-level verification during writing, not after — "verify against source" instructions in the plan prevented some errors but not all
3. Code review as a workflow step catches integration-level bugs that unit tests miss — the z-order conflict and WidgetPicker role bug were both invisible to the test suite
4. Pre-existing bugs surface naturally during milestone audits — the distance unit enum was broken since Phase 06 but nobody noticed until the audit ran the full suite with fresh eyes

---

## Milestone: v0.5.3 — Widget Grid & Content Widgets

**Shipped:** 2026-03-13
**Phases:** 5 | **Plans:** 13 | 55 commits, 1 day

### What Was Built
- Cell-based WidgetGridModel replacing pane-based layout — occupancy tracking, collision detection, YAML v3 persistence
- Grid renderer with Repeater positioning, pixel-breakpoint widget revision (Clock, AA Status, Now Playing)
- NavigationDataBridge + ManeuverIconProvider for AA turn-by-turn widget with modern 0x8006/0x8007 messages
- MediaDataBridge with AA > BT source priority and unified NowPlayingWidget
- Full edit mode: drag-to-reposition, drag-to-resize with ghost rectangle, FAB add/X remove, safety exits
- SwipeView multi-page with lazy Loader instantiation, page management FABs, empty page auto-cleanup

### What Worked
- 5 phases + 13 plans executed in a single day — tight scoping and parallel-capable phases (06+07) kept velocity high
- Integration checker confirmed 42/42 requirements wired with 0 broken flows — cross-phase design was clean
- Pi verification during Phase 07 and 08 caught real bugs (drag overlay routing, persistence wipe on startup, dot tap confusion) before phase close
- AA 16.2 nav message path (0x8006+0x8007) discovered during Phase 06 Pi testing — old 0x8004 path dead on modern phones

### What Was Inefficient
- Phases 04 and 05 lost their directories — no VERIFICATION.md, no SUMMARY artifacts. Audit had to rely on git history and REQUIREMENTS.md checkboxes
- Phase 06 VERIFICATION.md never created, 11 NAV/MEDIA requirements never flipped from Pending — pure paperwork gap caught at audit
- No SUMMARY files had `requirements_completed` frontmatter — 3-source cross-reference was incomplete across the board
- Nav distance unit tests (miles/feet/yards) failing — pre-existing from Phase 06 but never fixed

### Patterns Established
- Drag overlay MouseArea at z:25 for consistent drag across interactive/static widgets
- Ghost rectangle for resize preview (not animating width/height — janky on Pi)
- SwipeView.interactive bound to !editMode for gesture conflict prevention
- Auto-save signal connected AFTER config load to prevent persistence wipe on startup
- Overlay elements (drag, picker, toast) stay at homeScreen scope outside SwipeView to avoid clipping

### Key Lessons
1. Phase directories must survive until milestone audit — premature cleanup creates verification gaps
2. REQUIREMENTS.md checkboxes should be flipped immediately when a phase completes, not deferred
3. A single-day milestone is possible when phases are well-scoped and dependencies are clean
4. Pi verification checkpoints are essential — 3 bugs in Phase 07-08 would have shipped without them

---

## Milestone: v0.5.0 — Protocol Compliance

**Shipped:** 2026-03-08
**Phases:** 4 | **Plans:** 8 | 31 commits, 3 days

### What Was Built
- Proto submodule updated to v1.0 (223 proto files) with unhandled message debug logging for full protocol observability
- NavigationTurnEvent parsing with road name, maneuver type, distance, icon, multi-step lane guidance
- First HU-to-phone outbound commands: VoiceSessionRequest (START/STOP) and media playback toggle
- BT channel auth data exchange (BluetoothAuthenticationData/Result) and InputBindingNotification haptic feedback
- Dead code cleanup after v1.2 proto verification retracted 4 requirements (NAV-02, AUD-01, AUD-02, MED-01)
- Protocol library renamed from open-androidauto to prodigy-oaa-protocol

### What Worked
- TDD approach caught serialization edge cases early (command builders, auth payload handling)
- Phase 4 cleanup was a natural fit — verifying against v1.2 protos exposed the misidentified messages cleanly
- Outbound command pattern (protobuf build → serialize → sendRequested) established in Phase 3 is reusable
- Small, focused plans (avg ~6 min each) kept execution tight
- Retraction process was clean — implemented, verified wrong, removed, documented

### What Was Inefficient
- Implemented 4 features (NAV-02, AUD-01, AUD-02, MED-01) that were later retracted after v1.2 proto verification — wasted effort on misidentified message IDs from v1.0
- Phase 4 was unplanned — added mid-milestone to clean up the retracted code, but this is exactly the kind of scope creep that makes milestones unpredictable
- No Pi UAT for voice session or BT auth — commands were wired and tested in unit tests but not verified on real hardware

### Patterns Established
- `[AA:unhandled]` prefix for library-level unregistered channel logging (distinct from lcAA category)
- Outbound command pattern: build protobuf → serialize → channel sendRequested signal
- BT auth signals emit raw QByteArray (no proto schema exists — raw bytes are correct)
- Message ID 0x8004 reused across channels (BT/Input/Nav) — channel-scoped, no collision
- Voice session on ControlChannel (ch0, 0x0011), not InputChannel

### Key Lessons
1. Proto verification against a newer schema should happen BEFORE implementation, not after — would have avoided 4 retracted features
2. Library rename is a mechanical but high-touch operation — 125 files changed, every build file and reference updated. Worth doing early in a project's life
3. WiFi BSSID vs SSID bug was subtle and only discoverable by reading the SDP spec carefully — protocol-level bugs need spec-first verification
4. Retraction is a valid and healthy outcome — better to remove wrong code than leave it in "just in case"

---

## Milestone: v0.4.5 — Navbar Rework

**Shipped:** 2026-03-05
**Phases:** 4 | **Plans:** 11 | 51 commits, 2 days

### What Was Built
- Zone-based evdev touch routing (TouchRouter + EvdevCoordBridge) replacing hardcoded sidebar dispatch
- 3-control navbar with tap/short-hold/long-hold gesture state machine, 4-edge positioning, LHD/RHD awareness
- Navbar-aware AA viewport margins with correct touch routing split between navbar and phone
- GestureOverlay evdev touch fix — per-control hit zones with priority preemption under EVIOCGRAB
- Content-space touch coordinate mapping via shared computeContentDimensions()
- Dead UI cleanup — TopBar, NavStrip, Clock, BottomBar, sidebar overlay all removed

### What Worked
- Bottom-up phase ordering (touch routing → navbar → AA integration → cleanup) prevented rework — each phase built cleanly on the last
- TDD on TouchRouter and NavbarController caught edge cases early (finger stickiness, gesture timing)
- Phase 3 gap closures (plans 03-06) were fast and targeted — each fixed a specific Pi-discovered issue
- Zone priority system (50/200/210) elegantly solved the navbar vs overlay vs AA touch routing hierarchy
- Cross-compilation Docker toolchain (restored in Phase 2) enabled faster Pi iteration

### What Was Inefficient
- Phase 3 grew from 2 planned plans to 6 — AA integration required more gap closure than anticipated
- Phase 2 missing VERIFICATION.md — skipped verifier, relied on Phase 4 UAT to confirm
- EDGE-02 (dynamic edge reflow) partial — content margins don't recompute live, requires restart
- Hardcoded BAR_THICK (56) in ServiceDiscoveryBuilder duplicates NavbarController constant — not DRY

### Patterns Established
- Zone priority hierarchy: navbar (50) < gesture overlay consume (200) < per-control (210)
- `onMoved` instead of `onValueChanged` for QML slider sync (prevents feedback loops)
- QTimer::singleShot(16ms) for post-layout QML geometry queries via mapToScene()
- EvdevCoordBridge created in plugin, shared with controllers — single bridge per AA session
- Zone deregistration wired to `visibleChanged` signal (not manual dismiss calls)

### Key Lessons
1. AA integration always reveals more issues than expected — budget 2-3x the planned plans for real-device testing
2. Zone-based touch routing is the right abstraction — extensible, testable, and solved the gesture overlay bug that had been lingering
3. Evdev-to-service bridging via QMetaObject::invokeMethod(Qt::QueuedConnection) is the reliable pattern for cross-thread touch handling
4. computeContentDimensions() as a static shared method prevents the most insidious bug class — coordinate space mismatches between touch and video

---

## Milestone: v0.4.4 — Scalable UI

**Shipped:** 2026-03-03
**Phases:** 5 | **Plans:** 10 | 183 commits, 2 days

### What Was Built
- YAML config override system (scale, fontScale, individual tokens) with precedence chain
- Unclamped dual-axis UiMetrics: min(h,v) for layout safety, geometric mean for font readability
- Full QML tokenization — 20+ files, zero hardcoded pixel values (grep-verified)
- Container-derived grid layouts (LauncherMenu GridView, settings grid, EQ min guard)
- Runtime auto-detection via DisplayInfo bridge, --geometry CLI flag for headless validation
- Layout-derived sidebar touch zones matching QML at any resolution
- Dead display config removal (width/height/orientation settings purged)

### What Worked
- Phases 1-2 done pre-GSD but verified clean by milestone audit — audit process validated retroactive work
- Wave-based parallel plan execution in Phase 3 (3 plans, 3 QML file groups) was highly efficient
- TDD on sidebar touch zones (05-02) caught the actual bug immediately
- --geometry flag proved invaluable for testing without hardware swaps
- Tech debt cleanup as a discrete step before milestone completion kept scope focused

### What Was Inefficient
- Phases 1-2 lacked SUMMARY.md files (no disk directories) — audit had to verify from code
- ROADMAP.md checkboxes drifted from actual progress — needed manual sync pass
- Phase 3 token mapping decisions (which hardcoded value → which token) required many small judgment calls that could've been pre-decided in a mapping table

### Patterns Established
- `_tok()` pattern for overridable tokens with auto-derived fallback
- Inline `Math.round(N * UiMetrics.scale)` for one-off dimensions not worth a dedicated token
- Container-derived grid sizing (width from parent, column count computed) over fixed column layouts
- `--geometry WxH` flag for resolution testing without compositor cooperation

### Key Lessons
1. Token mapping deserves a pre-planning step — deciding "14px → fontTiny" before touching files avoids per-file deliberation
2. GSD phase tracking should start from Phase 1, not Phase 3 — retroactive audit is harder than incremental tracking
3. Dual-axis scale (min for layout, geomean for fonts) is genuinely better than single-axis — fonts need to grow in both dimensions

---

## Milestone: v0.4.3 — Interface Polish & Settings Reorganization

**Shipped:** 2026-03-03
**Phases:** 4 | **Plans:** 8 | **Tasks:** 18 | 74 commits

### What Was Built
- Automotive-minimal control restyling with press feedback across all interactive controls
- 6-category settings tile grid replacing flat ListView
- EQ dual-access (NavStrip shortcut + Audio settings path with deep navigation)
- Shell polish (NavStrip, TopBar, launcher, modals, BT forget)
- Day/night smooth color animation on all major surfaces
- Tech debt cleanup (highlightFontColor, divider hacks, dead code)

### What Worked
- Visual foundation first — settings pages inherited polish automatically (zero rework)
- Phase 4 gap closure: audit identified 5 items, single plan closed all of them
- Purely QML-side work with minimal C++ kept risk low
- Descoping at Pi verification (tile subtitles, WiFi AP) improved the product

### What Was Inefficient
- Phase 2 verification flagged descoped items as failures before requirements were updated
- SUMMARY frontmatter inconsistencies made automated extraction harder

### Patterns Established
- Descope at Pi verification and update requirements immediately
- Gap closure via audit → plan → execute cycle
- Signal relay pattern for cross-component deep navigation

### Key Lessons
1. Restyle shared controls before creating new pages — avoids double-work
2. Descoping is a valid outcome, not a failure
3. QML undefined property bindings are silent — need verification to catch

---

## Milestone: v0.4 — Logging & Theming

**Shipped:** 2026-03-01
**Phases:** 2 | **Plans:** 9 | **Duration:** ~96 min execution

### What Was Built
- Categorized logging infrastructure (6 subsystems, quiet defaults, verbose via CLI/config/web panel)
- Full log call migration — zero raw qDebug/qInfo/qWarning calls remain
- Multi-theme color palette system with user override paths
- 3-tier brightness control (sysfs > ddcutil > software overlay)
- Wallpaper system with per-theme defaults, user override, settings picker
- Gesture overlay brightness control and wallpaper shell integration

### What Worked
- Gap-closure plans were fast and targeted (~1-3 min each)
- UAT-driven gap discovery caught real issues (config defaults gate, wallpaper refresh)
- Static library build optimization eliminated redundant test compilations
- Phase 2 averaged 7 min/plan vs Phase 1's 19 min — velocity improved with familiarity

### What Was Inefficient
- Original milestone scope (v1.0 with 4+ phases) was too ambitious — rescoped to v0.4 after completing only 2 phases
- Context gathering for Phase 3 (equalizer) was done without user input — had to be discarded and redone
- Phase 1 Plan 3 (UAT gap closure) took disproportionate time due to log severity triage complexity

### Patterns Established
- Config defaults gate: any new config key must be registered in `initDefaults()` before `setValueByPath()` will accept writes
- Q_INVOKABLE required for QML → C++ method calls via context properties
- Wallpaper override independent of theme (setTheme sets default, setWallpaper overrides)
- Software dimming as fallback when hardware backlight unavailable

### Key Lessons
- Scope milestones to what's actually planned in the roadmap, not aspirational feature lists
- Context gathering must involve the user — AI assumptions about feature design are not substitute for real requirements
- Gap-closure plans are inherently faster than initial implementation plans — budget accordingly

---
