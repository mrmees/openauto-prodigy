# Session Handoffs

Newest entries first.

---

## 2026-07-24 — PR #35 pre-merge review

**What changed:** nothing — the review confirmed the branch as-is.

**Review adjudication:** zero findings. Verified in depth: the navbar/popup
generation plumbing (every begin synchronously unregisters old zones, evdev
callbacks capture their generation and the marshaled slot rejects mismatches,
so retired geometry cannot fire actions); the deferred plugin-view retirement
ordering (plugin hook deactivates synchronously, the child QQmlContext
outlives the dying view, `~PluginModel` drains retirements while its child
view host is still alive, and drained IDs make leftover queued callbacks
no-ops); widget-grid invariants (every mutator applies the pending remap
first, baselines commit only on user mutations with all dashboards forced to
unified grid dimensions before persisting); and the per-zone/per-slot popup
press state replacing a lambda-static shared across all zones and fingers.
Observations without action: one event-loop turn of old/new plugin view
overlap on switch (new renders on top; retained context keeps the dying
view's handlers valid by design).

**Verification:** full local build, explicit `openauto-prodigy` target, and
`ctest --output-on-failure` (all tests) passed on the branch. Physical
multi-touch, popup switching, and exact navbar-boundary touches remain
unvalidated on hardware — worth a deliberate poke on the next live drive.

**Next 1-3 steps:** (1) merge PR #35; (2) at the next milestone tag, deploy
PR #32-#35 together and physically exercise navbar edges and popup switching.

---

## 2026-07-24 — UI input and widget-grid lifecycle remediation COMPLETE

**What changed:** navbar evdev claims now come from rendered QML geometry, with
generation-safe replacement, accepted touch-slot ownership, and popup state
isolated by generation, zone, and slot. Widget remaps keep retained placements
and reserved pages reachable, apply pending dimension changes before edits,
and persist page and dimension baselines across restarts. Plugin deactivation
now precedes replacement activation while the outgoing QML view and child
context survive the current dispatch and retire in order afterward. A dedicated
owner replaces window/current-screen DPI connections. The bounded implementation
and review fixes are recorded in commits `6f28137` through `5feaa5a`.

**Why:** fixed geometry, shared or stale gesture state, unreachable widget
placements, stale remap baselines, synchronous QML destruction, late plugin
deactivation, and accumulating screen connections could make shell input,
dashboard state, or plugin transitions disagree with their rendered and
persisted owners. The remediation establishes one explicit owner at each
boundary without changing Android Auto protocol/input mapping, frozen API or
YAML fields, audio, HFP, or Bluetooth behavior.

**Status:** COMPLETE on `agent/ui-input-widget-grid-lifecycle-remediation`,
based on merged PR #34. The reviewed aarch64 binary through `5feaa5a` was
retained behind rollback snapshot
`/var/backups/openauto-prodigy/20260724T032627Z` and deployed as
`ALPHA-26-07-15-02-154-g5feaa5a`. One application process (PID `332025`) owns
responsive IPC. A native dashboard screenshot and live dashboard transition
were verified through the trusted localhost API before Android Auto re-entered
wireless H.265 projection. The exact original configuration was restored
(`57415b...`); hostapd PID `46989` and Bluetooth PID `672` remained
unchanged with zero restarts. Physical multi-touch, popup switching, and navbar
boundary rows were unavailable remotely and were not inferred.

**Review gate:** the initial repository pass returned three findings and the
required rerun returned three more. All six were confirmed and fixed; none were
dismissed or silently dropped. The fixes cover stale navbar geometry events and
generation rollback, widget baseline persistence, and ordered plugin view,
deactivation, and context retirement. No third run was performed under the
one-rerun policy.

**Verification:** focused navbar, widget-grid/remap, plugin-model/view-host, and
display tests passed. `cmake --build . -j$(nproc)`, the explicit
`openauto-prodigy` target, and the full `ctest --output-on-failure` passed in
`~/builds/openauto-prodigy`. Documentation-link validation and
`git diff --check origin/main..HEAD` passed. `./cross-build.sh` produced the
deployed aarch64 binary. Live validation covered shell/dashboard rendering and
transition, one responsive process and IPC owner, wireless H.265 re-entry,
exact configuration restoration, and unchanged hostapd/Bluetooth lifetimes.

**Next 1-3 steps:** (1) publish this completed branch as a draft PR targeting
`main`; (2) review and merge it as an independent consolidated wave; (3)
revalidate the next audit wave before activating another bounded design and
plan.

---
