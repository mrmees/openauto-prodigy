# Retrospective

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
