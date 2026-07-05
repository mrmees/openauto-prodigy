# Fable Work Program — Paid-Alternative Parity Deep Design Sprint

**Date:** 2026-07-05
**Status:** DRAFT — awaiting Matthew's approval. Nothing below executes until approved.
**Grounded against:** local main = origin/main at `e13b591` (verified current 2026-07-05).

## 1. Goal

Matthew loses access to Claude Fable 5 in ~2 days. Spend that time on the design work with the highest reasoning leverage — the one-shot architectural decisions that are expensive to get wrong and hard for smaller models to hold in context — producing self-contained specs and implementation plans that lower-power agents (Sonnet-class) execute afterward.

**Leverage ranking that drives this program:**

| Item | Reasoning leverage | Why |
|---|---|---|
| Unified extensibility architecture (API + JS bridge + dashboards + overlays) | **Highest** | Four interlocking features; schema/API surface is one-shot; integration mistakes compound |
| External API v1 schema | **Highest** | Original protobuf design; v1 mistakes propagate forever |
| HFP call audio | **High** | Existing spike doc has a role confusion (see §5.D) and hand-waves call control; wrong stack choice costs weeks |
| WebEngine spike + JS runtime design | **High** | Empirical gate; running it live grounds the design in measured numbers |
| Dashboards/overlays design | **Medium-high** | Architectural generalization of existing widget grid |
| 0x8012 wire verification protocol | **Medium** | Good protocol reasoning, but blocked on live phone experiments |
| Media player, equalizer completion, logging audit | **Low** | Commodity work; light plans suffice |

## 2. Deliverable Format (applies to every item)

- **Design doc** per item: `docs/superpowers/specs/YYYY-MM-DD-<topic>-design.md`.
- **Implementation plan** per item: superpowers `writing-plans` format — bite-sized tasks, exact file paths, code-level guidance, verification commands (local build + ctest + cross-build), executable by a fresh lower-power session with zero archaeology.
- Every design doc ends with an **Executor Guidance** section: invariants, pitfalls (CLAUDE.md gotchas + new ones discovered during design), test strategy, and definition of done. This compensates for the executor's weaker inference-time judgment.
- Each doc pins the commit it was grounded on, so a future executor knows to re-verify if the substrate moved.

## 3. Approaches Considered

1. **Keystone-first (chosen):** one unified extensibility-architecture design first; per-item specs in dependency order beneath it; live spikes interleaved so designs branch on data, not guesses. Protects against four Sonnet sessions later producing four features that don't compose.
2. **Item-by-item:** each roadmap item designed independently. Faster per item, but the API/JS-bridge/dashboard/overlay quartet share contracts — independent design risks integration Frankenstein.
3. **Plans-only sprint:** churn out implementation plans for everything from current knowledge. Maximum coverage, minimum reasoning value — that's work a Sonnet could do, so it wastes exactly the resource that's scarce.

## 4. Non-Goals

- No production feature implementation during the sprint (plans are the product). Exceptions: the spike harness (throwaway) and committing the v1 `.proto` contract (cheap now, locks the contract while the designer is present).
- No wire/config compatibility with any paid alternative (standing decision, licensing).
- No new roadmap items — this program covers only already-promoted items plus stretch light-plans.

## 5. The Program

### Phase A — Unified Extensibility Architecture (keystone)

One design doc fixing the layering:

```
existing substrate (EventBus, ActionRegistry, NotificationService,
                    IpcServer, provider interfaces, widget models)
        ↓
External API v1 (original protobuf over TCP + WebSocket)
        ↓
prodigy JS bridge (WebEngine, gated on spike)
        ↓
consumers: dashboards (incl. web widgets), overlay framework,
           companion app, third-party clients
```

Decisions locked here: transport framing; schema conventions and versioning; auth/security model — **updated by the companion decision:** localhost-only is no longer viable since the companion phone app is an API client; working default is bind localhost + AP interface (10.0.0.1) with PIN-pairing auth for non-localhost clients, LAN (192.168.x) exposure opt-in (Decision 4); threading model (where the API server lives relative to ASIO/Qt threads); event fan-out from EventBus to API subscribers; naming conventions. Grounded in a code-level inventory of the substrate (scout dispatched 2026-07-05).

**CompanionListenerService relationship (Codex review raised it; Matthew decided 2026-07-05): REPLACE.** The companion app is not developed enough to preserve — it will be rewritten against the new External API, which becomes the single canonical external surface. Consequences for the design: `CompanionListenerService` (QTcpServer port 9876, single-client, HMAC auth from PIN-derived shared secret, QR pairing) retires after migration — an executor-phase task, not a sprint task; its data (GPS, battery, charging, internet/proxy status) becomes API status streams/inbound reports in the v1 schema; its PIN → SHA256 shared-secret → HMAC pairing flow remains **candidate prior art** for the API's auth model (one auth story, one port); no wire back-compat obligation exists.

### Phase B — External API v1 Spec + Implementation Plan

- Complete original `.proto` package: status streams (media/nav/projection/phone), action dispatch + registration, notification/toast display. v1 scope only — grow by demand.
- Server design: transport choice (Qt WebSockets vs ASIO), connection lifecycle, subscription model, backpressure.
- Integration points into existing services; config schema (`api.*` keys); test plan with a loopback client.
- **The actual `.proto` files get written and committed during the sprint** (Decision 5) — the contract is the highest-value artifact to get right while Fable is present.
- **Proto-freeze gate (Codex review, 2026-07-05):** no proto commit until (a) Phase A layering decisions are made (companion relationship already decided: replace — see §5.A), and (b) the HFP call-control path decision (Phase D1, pulled ahead — see §5.D) is settled. Design principles that de-risk the phone stream regardless: phone *status* is modeled on HFP-standard indicator semantics (call/callsetup/callheld → incoming/dialing/alerting/active/held/waiting), **not** on the current UI mock's enum; and v1 includes capability discovery (clients must already handle "no phone connected," so "dial/DTMF unavailable" is the same mechanism, and the action surface can grow as Phase D lands without breaking the contract).

### Phase C — WebEngine Spike (live) + JS Runtime Design

- Live on the Pi: install `qml6-module-qtwebengine` (Decision 2 — reversible via apt), deploy a minimal WebEngineView QML harness. Go/no-go thresholds defined **before** measuring, across these categories (expanded per Codex review): idle RSS; per-view RSS at 1/2/3 concurrent web views; RSS + CPU with an active AA session decoding video; web-view startup latency; AA frame stability while a web view is active; touch/input latency; and renderer-process crash/reload behavior under memory pressure (WebEngine's renderer dying on a 4GB Pi is a realistic failure mode — recovery UX is part of the verdict).
- Then the JS runtime design doc: process/lifecycle model, `prodigy` JS object surface (theme tokens, input events, API access — mapped onto Phase B's API rather than a parallel channel), security sandboxing, packaging of web widgets/apps.
- If the spike fails: documented fallback (native-QML-only widgets; HTML path shelved with reasoning recorded).

### Phase D — HFP Call Audio Design + Plan

The 2026-02-18 spike doc needs correction before anyone builds on it:
- **Role confusion:** it claims the head unit needs the HFP AG profile (0x111f). A car head unit is the **HF (hands-free) role, 0x111e**; the phone is the AG. This inverts which side does what.
- **Call control hand-wave:** "send AT commands via BlueZ D-Bus" — BlueZ core exposes no AT-command D-Bus API. Real options: PipeWire's native HFP backend capabilities, implementing the RFCOMM AT channel ourselves, or reconsidering ofono. This is the central design decision.

Split in two (Codex review — the call-control decision gates the proto freeze):

- **D1 — Call-control path decision (pulled ahead of the proto commit, session 2):** role/UUID correction; choose the call-control path — PipeWire native HFP backend capabilities vs. implementing the RFCOMM AT channel ourselves vs. reconsidering ofono. Paper analysis of what each path actually exposes for dial/answer/hangup/DTMF/hold, plus the quick live inspection if Matthew + phone are available. Output: a decision record the API's phone action/capability surface is built on. Scout finding reinforces this: PhonePlugin's dial/answer/hangup/DTMF are UI-only mocks today, so there is no existing behavior to preserve.
- **D2 — Call audio design + plan (session 3):** SCO audio routing through AudioService's 3-stream model; coexistence with AA's phone stream (who owns call audio when AA is up vs down — the roadmap rationale); codec expectations (mSBC preferred, CVSD fallback, LC3-SWB awareness); the full **live-inspection checklist** (phone paired to Pi, inspect what PipeWire/BlueZ actually expose) — needs Matthew + phone present (Decision 3); ideally runs during the sprint, otherwise ships as a self-contained checklist an executor runs later.

### Phase E — Dashboards + Overlay Framework Design + Plan

- Multiple named dashboards on top of WidgetGridModel/WidgetPickerModel; widget size options (paid alternatives: 2 widths × 3 heights); web-view widgets as first-class citizens (post-spike).
- Generalized overlay system: position/size/visibility driven by ActionRegistry actions and the API; migration story for the purpose-built overlays (incoming call, pairing, gesture).
- Depends on Phase A contracts; web-widget parts depend on Phase C verdict.

### Phase F — Stretch: Light Plans for Commodity Items

~30 minutes each, plan-only: media player plugin (Qt Multimedia + MediaStatusService integration); equalizer completion (audit current plugin state first); 0x8012 `UpdateHuUiConfigRequest` wire-verification experiment protocol; key-event navigation map notes (steering-wheel buttons via GPIO/HID).

### Cross-cutting — Executor Handbook

`docs/superpowers/plans/` gains a short handbook: how to pick up each plan, the verification workflow (build → ctest → cross-build → Pi deploy), which docs are canonical, standing guardrails (open-android-auto submodule is hands-off, wishlist-then-promote governance). Written once, referenced by every plan.

## 6. Sequencing Across Remaining Sessions

| Session | Work |
|---|---|
| 1 (today) | Approve program → Phase A (incl. companion-relationship decision) → start Phase B design |
| 2 | Phase D1 (HFP call-control path decision) → finish B, proto committed behind the freeze gate → Phase C spike live on Pi → JS runtime design |
| 3 | Phase D2 (call audio design; live HFP inspection if phone available) → Phase E |
| 4 (if it exists) | Phase F + executor handbook + final handoff/memory updates |

Cutting from the bottom loses the least; the ordering is the priority list.

**On approval, add a one-line note to `docs/roadmap-current.md`** (Codex review): the sprint produces *designs* ahead of the roadmap's execution order; the "Now" items (HFP, EQ) keep their delivery priority — HFP execution in fact gets unblocked by Phase D. This prevents future agents reading the roadmap and this spec as conflicting.

## 7. Decision Points for Matthew

1. **Approve program and ordering?** Anything to cut or add?
2. **OK to `apt install qml6-module-qtwebengine` on the Pi** for the spike? (Reversible; needed for live measurement.)
3. **Can you be around with your phone for ~15 min** during Phase D for the HFP live inspection (pair phone to Pi, observe PipeWire/BlueZ)? If not, it ships as an executor checklist.
4. **API exposure default:** ~~localhost-only~~ superseded by the companion-replace decision — the companion phone app is an API client, so the API must be reachable beyond localhost. Recommendation: bind localhost + AP interface (10.0.0.1) with PIN-pairing auth (companion-style) for non-localhost clients; LAN (192.168.x) exposure opt-in.
5. **Commit the v1 `.proto` during the sprint** (my recommendation — locks the contract while the designer is present), or leave schema authoring to executors?

## 8. Substrate Scout Findings (2026-07-05, code-level inventory)

A read-only inventory of the extensibility substrate was run before Phase A. Load-bearing corrections to the parity spec's assumptions:

1. **PhonePlugin call control is a UI mock.** `dial()` is a `TODO` (`PhonePlugin.cpp:346`), `answer()`/`hangup()` only flip local state, `sendDTMF()` is empty. No HFP telephony, no SCO handling anywhere in the codebase. The header comment repeats the spike doc's AG-role claim. Phase D therefore designs call control **from scratch**, not as a completion task.
2. **The equalizer is functional, not "completeness unverified."** `EqualizerService` runs 3 engines (media/nav/phone) with presets + persistence, and `AudioService.cpp:154-158` actually applies EQ on the PipeWire RT thread. Phase F's EQ item shrinks to: verify web-config/advanced-profile parity against the roadmap outcome statement.
3. **NotificationService renders toasts only.** `kind == "incoming_call"` and `"status_icon"` have no UI path (`NotificationArea.qml:33`), and `priority` is stored but never used. The Phase E overlay framework is the natural home for non-toast kinds — this goes into Phase A's layering decisions.
4. **IpcServer has no uniform response envelope** (raw objects, `{"error":…}`, `{"ok":true}` mixed) and no widget/notification/action commands. The External API (Phase B) must not inherit this shape; whether the web-config IPC later migrates onto the API is a Phase A decision.
5. Mechanisms confirmed solid: EventBus (thread-safe pub/sub, delivers on main thread), ActionRegistry (main-thread dispatch; all built-ins registered in `main.cpp`), widget stack (registry + grid model with per-instance config schemas, v3 YAML persistence). One open TODO: `ConfigService.cpp:31` — no live plugin-config change signal yet.

## 9. Risks

| Risk | Mitigation |
|---|---|
| Program too big for the time | Ordered by leverage; bottom-up cuts lose least |
| Designs go stale before execution | Docs pin their grounding commit; executor handbook mandates substrate re-verification |
| Spike hardware surprises invalidate JS runtime design | Spike runs early and live; fallback design documented |
| Substrate docs overstate completeness (like the archived extensibility plan's stale header, in reverse) | Phase A starts from a fresh code-level inventory, not doc claims |
| Matthew unavailable during sprint windows | Every phase's output is self-contained; decision points have stated defaults |
