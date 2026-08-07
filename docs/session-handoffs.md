# Session Handoffs

Newest entries first. Older entries are in
[`docs/archive/session-handoffs/`](archive/session-handoffs/).

---

## 2026-08-06 — Generic configurable web widgets completed

**What changed:** implemented explicit-opt-in, full-screen widget configuration
with draft/Save/Cancel semantics; typed enum, boolean, integer-range, and
collection fields; deterministic widget-owned collection discovery; canonical
path confinement and read-only `__data__` delivery; persisted per-instance
configuration; and complete `prodigy.config`/`configchange` snapshots. Added a
domain-neutral configurable web-widget package plus Sample and Alternate
profiles. The configuration page now reuses Prodigy's established full-screen
picker and button styling.

**Why:** one separately installed widget runtime must be able to select among
independently copied profiles without making Prodigy understand Gauge Studio,
OBD/CAN, or any future backend's domain. The two example profiles also prove
that a saved selection reaches a running web widget without restarting.

**Status:** hardware accepted at code anchor `b30c262`. The Pi passed
configure-on-add, Cancel without removal, live Sample/Alternate switching,
resize compatibility, persistence across restart, missing saved-item display
without fallback, and empty-collection behavior with disabled Save and working
Cancel. Both profiles were restored afterward, the service is active, and the
selected Alternate profile renders normally. The completed design and Prodigy
plan are archived; the Gauge Studio handoff remains ACTIVE. Nothing has been
pushed in this session.

**Verification:** the native build, explicit `openauto-prodigy` app target,
full offscreen CTest, document-link check, `git diff --check`, ARM
`./cross-build.sh`, Pi deployment, service restart, warning-level journal
check, and touchscreen/recovery checklist passed. One early CTest invocation
without `QT_QPA_PLATFORM=offscreen` hit the expected headless XCB startup
failure; the required offscreen invocation passed.

**Review:** the user message `go` authorized implementation and the review-gate
reset from immutable feature base `efea94113cf7decd67748facc4e81f9dec79f3be`.
Fable pass 1 reported BLOCKER=0, MAJOR=0, MINOR=6; two findings were confirmed
and remediated, two were dismissed as intentional supported behavior, and two
were deferred. Pass 2 reviewed only the remediation commit and reported
BLOCKER=0, MAJOR=0, MINOR=2; both were deferred cosmetic/test-maintenance
leads. Total adjudication: confirmed/remediated 2, dismissed 2, deferred 4.
Hardware then exposed a supported-production blocker that persistence handled
but the live web update missed; direct fresh-property serialization fixed it
and the user accepted the result. No third review was run because the bounded
two-pass review budget was exhausted.

**Next 1–3 steps:** use
`docs/plans/2026-08-06-gauge-studio-profile-handoff.md` in the Gauge Studio
repository; replace the generic example with the single Gauge runtime and
exported profiles; remove any extra Configurable Example widgets from the Pi
dashboard when no longer useful for testing.
