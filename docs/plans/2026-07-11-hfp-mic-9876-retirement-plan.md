# HFP Mic + Live-Check Prep + 9876 Retirement Stage-1 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

Status: ACTIVE
Design doc: `docs/plans/2026-07-11-hfp-mic-9876-retirement-design.md` (read its §7 executor guidance first)
Grounded on: `02472f8`

**Goal:** Everything executable before the bench session — mic-fix artifacts (CVSD drop-in + patched-mSBC build), dead-slot D-Bus fixes, ApiInboundState parity + consumer migration, bench runbook, companion handoff prompt — ending with a Pi deploy that is bench-ready.

**Architecture:** Two independent workstreams. HFP: repair silently-dead BlueZ ObjectManager subscriptions (registered `QMap<QString,QVariantMap>` metatype, real slots, sender filtering) and prep two codec interventions for one bench sitting. 9876: bring the API-v1 inbound path to semantic parity with the legacy companion listener, re-point all QML/IPC consumers at it under a NEW context name (`CompanionState`), leaving the legacy `CompanionService` object alive for its own settings UI until teardown (B2 — planned post-bench, NOT in this plan).

**Tech Stack:** Qt 6.8 (QtDBus, QML), PipeWire 1.4 (Pi), Docker arm64 (deb rebuild), ctest.

## Global Constraints

- Tier tags per root `AGENTS.md` (`opus`/`sonnet`/`main`); workers read root + nearest nested `AGENTS.md` before editing.
- **Worktree build dir:** this branch lives in a worktree; configure its own ext4 build dir first:
  `cmake -S /mnt/e/claude/personal/openautopro/openauto-prodigy/.claude/worktrees/hfp-mic-9876-retirement -B ~/builds/oap-hfp-9876`
  Then per task: `cd ~/builds/oap-hfp-9876 && cmake --build . -j$(nproc) && ctest --output-on-failure`. **ctest does not compile the app target** — before claiming green: `cmake --build . --target openauto-prodigy -j$(nproc)`.
- HF role only (0x111e); no ofono; `proto/api/` frozen (NO proto changes in this phase — coverage verified complete).
- QML ships inside the binary — UI changes reach the Pi only via `./cross-build.sh` + binary rsync (`qml/AGENTS.md`).
- One task = one commit. Nobody pushes mid-execution.
- Do not edit anything under `docs/archive/`.

## File Map

| File | Task | Responsibility |
|---|---|---|
| `src/core/services/PhoneStateService.{hpp,cpp}` | 1 | HFP device monitoring — fixed InterfacesAdded + `adoptBluezDevice` seam |
| `tests/test_phone_state_service.cpp` | 1 | new device-adoption cases |
| `src/plugins/bt_audio/BtAudioPlugin.{hpp,cpp}` | 2 | slots + retype + sender filtering + connect logging |
| `tests/test_bt_audio_plugin.cpp` (new), `tests/CMakeLists.txt` | 2 | new test target |
| `config/50-prodigy-hfp-cvsd.conf` (new) | 3 | A1a CVSD drop-in (not installer-wired yet) |
| `tools/pipewire-msbc/{README.md,build.sh,0001-*.patch}` (new) | 4 | A1b patched arm64 deb build |
| `src/core/api/ApiInboundState.{hpp,cpp}` | 5 | parity: GPS extras, staleness, connected, clears |
| `src/core/api/ApiRequestHandlers.{hpp,cpp}` | 5 | forward new GPS fields; per-report owner tracking + disconnect clears |
| `tests/test_api_request_handlers.cpp` | 5 | forwarding + clearing + staleness cases |
| `src/main.cpp` | 6 | expose `CompanionState` context property |
| `qml/widgets/{BatteryWidget,WeatherWidget,CompanionStatusWidget}.qml` | 6 | read `CompanionState`; proxy row fix |
| `qml/applications/settings/CompanionSettings.qml` | 7 | Status section → `CompanionState`; legacy controls stay |
| `src/core/services/IpcServer.{hpp,cpp}` | 8 | `companion_status` reads inbound state |
| `docs/plans/2026-07-11-hfp-bench-runbook.md` (new) | 9 | bench instrument |
| `../companion-9876-migration-prompt.md` (outside repo) | 10 | companion-repo handoff |
| `docs/roadmap-current.md`, `docs/session-handoffs.md` | 11 | promote phase; handoff entry; deploy |

**Known regression window (accepted in design):** after Task 6 deploys, companion data widgets read the API path; they show "offline/--" until the companion app migrates (B1, Matthew-driven). The legacy service keeps running and its settings UI keeps working.

---

### Task 1: PhoneStateService — fixed InterfacesAdded + adoptBluezDevice seam

**Tier:** opus
**Files:** Modify `src/core/services/PhoneStateService.hpp` (~:78-92), `src/core/services/PhoneStateService.cpp` (:316-319, :343, :358-470); Test `tests/test_phone_state_service.cpp`
**Out of scope:** BtAudioPlugin (Task 2); PropertiesChanged/InterfacesRemoved connects (already correctly typed here).

**Interfaces — Produces:**
```cpp
// PhoneStateService.hpp, above the class:
/// ObjectManager InterfacesAdded carries a{sa{sv}}; QtDBus refuses to deliver
/// it to a QVariantMap slot (connect fails at runtime — bench 2026-07-10 root
/// cause, same family as UsbInterfaceMap). Registered in startDBusMonitoring().
using BluezInterfaceMap = QMap<QString, QVariantMap>;

public:
    /// Evaluate one BlueZ Device1 property map (from the initial scan or a
    /// live InterfacesAdded) and adopt the device if it is a connected HFP
    /// phone. Public because it IS the unit-test surface (same convention as
    /// the state-machine event slots above).
    void adoptBluezDevice(const QString& path, const QVariantMap& deviceProps);

private slots:
    void onInterfacesAdded(const QDBusObjectPath& path, const BluezInterfaceMap& interfaces);
```

- [ ] **Step 1: failing tests** — append to `tests/test_phone_state_service.cpp` (follow the file's existing fixture style; the service under test is constructed without a bus — these paths touch no live D-Bus):

```cpp
void TestPhoneStateService::adoptBluezDevice_connectedHfpPhone_adopts()
{
    oap::PhoneStateService svc;
    QSignalSpy spy(&svc, &oap::PhoneStateService::connectionChanged);
    QVariantMap props{
        {QStringLiteral("Connected"), true},
        {QStringLiteral("Alias"), QStringLiteral("Pixel 8")},
        {QStringLiteral("UUIDs"), QStringList{QStringLiteral("0000111e-0000-1000-8000-00805f9b34fb")}},
    };
    svc.adoptBluezDevice(QStringLiteral("/org/bluez/hci0/dev_AA_BB"), props);
    QCOMPARE(spy.count(), 1);
    QVERIFY(svc.phoneConnected());
    QCOMPARE(svc.deviceName(), QStringLiteral("Pixel 8"));
}

void TestPhoneStateService::adoptBluezDevice_nonHfp_ignored()
{
    oap::PhoneStateService svc;
    QSignalSpy spy(&svc, &oap::PhoneStateService::connectionChanged);
    QVariantMap props{
        {QStringLiteral("Connected"), true},
        {QStringLiteral("UUIDs"), QStringList{QStringLiteral("0000110b-0000-1000-8000-00805f9b34fb")}}, // A2DP sink only
    };
    svc.adoptBluezDevice(QStringLiteral("/org/bluez/hci0/dev_CC_DD"), props);
    QCOMPARE(spy.count(), 0);
    QVERIFY(!svc.phoneConnected());
}

void TestPhoneStateService::interfacesAdded_payloadConsumed_noBusReadback()
{
    oap::PhoneStateService svc;
    QSignalSpy spy(&svc, &oap::PhoneStateService::connectionChanged);
    oap::BluezInterfaceMap ifaces;
    ifaces.insert(QStringLiteral("org.bluez.Device1"), QVariantMap{
        {QStringLiteral("Connected"), true},
        {QStringLiteral("Alias"), QStringLiteral("Pixel 8")},
        {QStringLiteral("UUIDs"), QStringList{QStringLiteral("0000111e-0000-1000-8000-00805f9b34fb")}},
    });
    // private slot → invoke through the meta-object (also proves the
    // signature is invokable with the registered type)
    QMetaObject::invokeMethod(&svc, "onInterfacesAdded", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(QStringLiteral("/org/bluez/hci0/dev_AA_BB"))),
        Q_ARG(oap::BluezInterfaceMap, ifaces));
    QCOMPARE(spy.count(), 1);
}
```
(If `phoneConnected()`/`deviceName()` accessors are not public on the service, use the existing test accessor pattern in this file — do not add new public state just for tests.)

- [ ] **Step 2:** `ctest --test-dir ~/builds/oap-hfp-9876 -R phone_state --output-on-failure` → new cases FAIL (no `adoptBluezDevice`, slot signature mismatch).
- [ ] **Step 3: implement**
  - Declare `BluezInterfaceMap` + register in `startDBusMonitoring()`: `qDBusRegisterMetaType<BluezInterfaceMap>();` (before the connects) and `qRegisterMetaType<oap::BluezInterfaceMap>("oap::BluezInterfaceMap");` if invokeMethod needs it.
  - Retype the `InterfacesAdded` connect/disconnect pair (`:319`, `:343`) to `SLOT(onInterfacesAdded(QDBusObjectPath,BluezInterfaceMap))`.
  - New `onInterfacesAdded`: look up `org.bluez.Device1` in the payload; absent → return; present → `adoptBluezDevice(path.path(), it.value())`. **No `QDBusInterface` read-back** (delete the current body's live query).
  - `adoptBluezDevice`: the Connected/UUIDs(0000111e/0000111f prefix)/Alias logic currently duplicated in `scanExistingDevices()` (:397-416) and old `onInterfacesAdded` (:424-460) — single-source it here; both callers delegate.
  - Log every `bus.connect(...)` result in `startDBusMonitoring()`:
    `qCInfo(lcPhone) << "PhoneStateService D-Bus subscriptions: InterfacesAdded=" << okAdded << "InterfacesRemoved=" << okRemoved << "PropertiesChanged=" << okProps;` (reuse the file's existing logging category).
- [ ] **Step 4:** targeted ctest green, then full `ctest` + app target build.
- [ ] **Step 5:** commit `fix: PhoneStateService hot-plug — registered map slot + adoptBluezDevice seam`.

**Acceptance:** three new tests pass; no `QDBusInterface` constructed in `onInterfacesAdded`; subscription-result log line exists.

---

### Task 2: BtAudioPlugin — real slots, retype, sender filtering, connect logging

**Tier:** opus
**Files:** Modify `src/plugins/bt_audio/BtAudioPlugin.hpp` (:109-116, members :136-137), `src/plugins/bt_audio/BtAudioPlugin.cpp` (:83-138, :312-323); Create `tests/test_bt_audio_plugin.cpp`; Modify `tests/CMakeLists.txt` (copy an existing plugin-test target block, e.g. the phone-state one).
**Out of scope:** PhoneStateService (Task 1); any behavioral change to transport/player state logic beyond filtering.

**Interfaces — Produces:**
```cpp
// BtAudioPlugin.hpp
using BtInterfaceMap = QMap<QString, QVariantMap>;   // same rationale as Task 1

private slots:   // were plain private methods — all three connects failed at startup
    void onInterfacesAdded(const QDBusObjectPath& path, const BtInterfaceMap& interfaces);
    void onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces);
    // sender path arrives in the trailing QDBusMessage (UsbMediaWatcher
    // onDrivePropertiesChanged pattern); filter against transportPath_/playerPath_
    void onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                             const QStringList& invalidated, const QDBusMessage& message);
```

- [ ] **Step 1: failing tests** — `tests/test_bt_audio_plugin.cpp` (new; QTest scaffold copied from `tests/test_phone_state_service.cpp`'s harness style):
  1. `propertiesChanged_wrongSenderPath_ignored` — set the plugin's tracked player path via its adoption flow or test seam, then `QMetaObject::invokeMethod` `onPropertiesChanged` with `interface="org.bluez.MediaPlayer1"`, a `Track` change, and a `QDBusMessage` whose path is a DIFFERENT object → `metadataChanged` signal NOT emitted.
  2. `propertiesChanged_matchingSender_applied` — same but message path == tracked player path → `metadataChanged` emitted.
  3. `interfacesAdded_slotInvokableWithRegisteredType` — invoke `onInterfacesAdded` via meta-object with a `BtInterfaceMap` payload containing `org.bluez.MediaTransport1` → no crash, transport adopted (assert via the plugin's connection-state signal).
  (Constructing a `QDBusMessage` with a set path: `QDBusMessage::createSignal(path, iface, "PropertiesChanged")` — its `path()` round-trips without a bus.)
- [ ] **Step 2:** build the new target; run `ctest -R bt_audio` → FAIL (methods not slots / wrong signature).
- [ ] **Step 3: implement** — move the three declarations under `private slots:`; retype `InterfacesAdded` (register `BtInterfaceMap` in `startDBusMonitoring()`); add the `QDBusMessage` parameter and guards at the top of `onPropertiesChanged`:
```cpp
const QString sender = message.path();
if (interface == QLatin1String("org.bluez.MediaTransport1") && sender != transportPath_) return;
if (interface == QLatin1String("org.bluez.MediaPlayer1") && sender != playerPath_) return;
```
  Update BOTH connect and disconnect signature strings (`:83-106`, `:119-133`) — a mismatched disconnect is a silent leak. Log all three connect results (Task 1 pattern).
- [ ] **Step 4:** targeted green → full `ctest` + app target.
- [ ] **Step 5:** commit `fix: BtAudioPlugin D-Bus handlers — real slots, typed map, sender filtering`.

**Acceptance:** new test target in ctest; three cases pass; startup subscription log line; disconnects mirror connects.

---

### Task 3: A1a CVSD drop-in asset

**Tier:** sonnet
**Files:** Create `config/50-prodigy-hfp-cvsd.conf`
**Out of scope:** installer wiring (post-bench decision), architecture.md (documented only if A1a ships as the fix).

- [ ] **Step 1:** create the file exactly:
```
# Prodigy HFP codec pin — forces CVSD by removing the shared mSBC/LC3-SWB
# feature bit (PipeWire gates both wideband codecs on SPA_BT_FEATURE_MSBC;
# no LC3-SWB-only toggle exists — design doc 2026-07-11 §A1).
# Deployed to /etc/wireplumber/wireplumber.conf.d/ ONLY while pinning CVSD.
monitor.bluez.properties = {
  bluez5.enable-msbc = false
}
```
- [ ] **Step 2:** commit `feat: CVSD WirePlumber drop-in for HFP mic bench (A1a)`.

**Acceptance:** file exists with that content; nothing references it from installers yet.

---

### Task 4: A1b patched-mSBC PipeWire arm64 build

**Tier:** opus
**Files:** Create `tools/pipewire-msbc/build.sh`, `tools/pipewire-msbc/0001-hfp-disable-lc3-swb.patch`, `tools/pipewire-msbc/README.md`
**Out of scope:** installing on the Pi (bench-day step, runbook covers it); upstreaming.

- [ ] **Step 1: the patch** — semantic change in `spa/plugins/bluez5/backend-native.c`, `device_supports_codec()` (1.4 branch ~:715): the `HFP_AUDIO_CODEC_LC3_SWB` case becomes an unconditional `return false;` (regenerate hunk offsets against the exact `apt-get source` tree inside the container):
```c
 	case HFP_AUDIO_CODEC_LC3_SWB:
-#ifdef HAVE_LC3
-		/* LC3-SWB has same transport requirements as msbc. ... */
-		alt1_ok = false;
-		alt6_ok = msbc_alt6_ok;
-		break;
-#else
-		return false;
-#endif
+		/* prodigy: LC3-SWB uplink is silent at the far end (bench
+		 * 2026-07-05/07-11); drop it from +BAC so the AG selects mSBC.
+		 * Remove when upstream fixes SWB uplink encode. */
+		return false;
```
- [ ] **Step 2: build.sh** — arm64 container build (binfmt already proven by `cross-build.sh`); versioned output:
```bash
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p out
docker run --rm --platform linux/arm64 -v "$PWD":/w -w /build debian:trixie bash -euxc '
  sed -i "s/^Types: deb$/Types: deb deb-src/" /etc/apt/sources.list.d/debian.sources
  apt-get update
  apt-get install -y --no-install-recommends build-essential devscripts quilt
  apt-get build-dep -y pipewire
  apt-get source pipewire
  cd pipewire-*/
  quilt import /w/0001-hfp-disable-lc3-swb.patch && quilt push
  dch --local +prodigy "Disable HFP LC3-SWB advertisement (silent uplink); force mSBC."
  dpkg-buildpackage -b -uc -us
  cp ../libspa-0.2-bluez5_*.deb /w/out/
'
echo "Staged: $(ls out/)"
```
- [ ] **Step 3: README.md** — one page: why (design §A1b), how to rebuild when Debian bumps pipewire (rerun script), Pi install/uninstall:
  `sudo apt install ./libspa-0.2-bluez5_*+prodigy*.deb && sudo apt-mark hold libspa-0.2-bluez5` / revert: `sudo apt-mark unhold libspa-0.2-bluez5 && sudo apt install --reinstall libspa-0.2-bluez5`. Note: only `libspa-0.2-bluez5` is installed; version must match the Pi's `pipewire` version (both from trixie).
- [ ] **Step 4: verify** — run `./build.sh` to completion; `ls tools/pipewire-msbc/out/` shows `libspa-0.2-bluez5_1.4.*+prodigy*_arm64.deb`. Then `rsync` the deb to `matt@192.168.1.149:~/pipewire-msbc/` (staged, NOT installed).
- [ ] **Step 5:** commit `feat: patched-mSBC PipeWire build kit (A1b) — LC3-SWB dropped from +BAC` (commit `out/` is gitignored; add `tools/pipewire-msbc/out/` to `.gitignore` in this commit).

**Acceptance:** script produces the deb non-interactively; deb staged on the Pi; README covers rebuild + revert.

---

### Task 5: ApiInboundState parity (B0a)

**Tier:** opus
**Files:** Modify `src/core/api/ApiInboundState.{hpp,cpp}`, `src/core/api/ApiRequestHandlers.{hpp,cpp}` (:125-148 sessionClosed, :334-360 handleReport); Test `tests/test_api_request_handlers.cpp`
**Out of scope:** QML/IPC consumers (Tasks 6-8); proto files (frozen; fields already exist).

**Interfaces — Produces (consumed by Tasks 6-8):**
```cpp
// ApiInboundState — full parity Q_PROPERTY surface (legacy names, design §B0):
Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)        // any live report owner
Q_PROPERTY(bool gpsValid READ gpsValid NOTIFY gpsChanged)
Q_PROPERTY(bool gpsStale READ gpsStale NOTIFY gpsChanged)
Q_PROPERTY(double gpsLat READ gpsLat NOTIFY gpsChanged)
Q_PROPERTY(double gpsLon READ gpsLon NOTIFY gpsChanged)
Q_PROPERTY(double gpsSpeed READ gpsSpeedMps NOTIFY gpsChanged)           // legacy alias
Q_PROPERTY(double gpsSpeedMps READ gpsSpeedMps NOTIFY gpsChanged)
Q_PROPERTY(double gpsAccuracy READ gpsAccuracy NOTIFY gpsChanged)
Q_PROPERTY(double gpsBearing READ gpsBearing NOTIFY gpsChanged)
Q_PROPERTY(int phoneBattery READ phoneBattery NOTIFY batteryChanged)
Q_PROPERTY(bool phoneCharging READ phoneCharging NOTIFY batteryChanged)
Q_PROPERTY(bool internetAvailable READ internetAvailable NOTIFY internetChanged)
Q_PROPERTY(bool proxyActive READ proxyActive NOTIFY internetChanged)     // NEW (fixes dead proxyStatus read)
Q_PROPERTY(QString proxyAddress READ proxyAddress NOTIFY internetChanged)

void setGps(double lat, double lon, double speedMps,
            double bearingDeg, double accuracyM, quint32 ageMs);   // widened
void setBattery(int percent, bool charging);                       // unchanged
void clearGps();       // owner disconnected → gpsValid=false, gpsChanged
void clearBattery();   // owner disconnected → phoneBattery=-1, batteryChanged
void setOwnerPresent(bool present);  // drives `connected` (handlers call it)
```
Staleness: store `QElapsedTimer` restarted on each accepted fix minus `age_ms`; `gpsStale()` = no fix yet, or effective age > 30 000 ms; a 5 s repeating `QTimer` re-emits `gpsChanged` while a fix is held so QML bindings flip without a new report (stop the timer on clearGps).

- [ ] **Step 1: failing tests** in `tests/test_api_request_handlers.cpp` (follow its existing handler-fixture style — it already constructs `ApiRequestHandlers` with fake deps): GpsReport with bearing/accuracy/age forwards all six args; GpsReport marks owner → `sessionClosed(owner)` clears (`gpsValid()==false`, `phoneBattery()==-1`, `connected()==false`); non-owner session close clears nothing; fresh fix → `gpsStale()==false`, then simulate expiry (setter/test seam: allow injecting the stale threshold, e.g. `setStaleThresholdMs(50)` + `QTRY_VERIFY(state.gpsStale())`).
- [ ] **Step 2:** `ctest -R api_request_handlers` → FAIL.
- [ ] **Step 3: implement** — extend `ApiInboundState` per the interface block (keep the existing `setConnectivity` contract untouched — SOCKS route plumbing depends on it); in `ApiRequestHandlers`: add `gpsOwner_`/`batteryOwner_` (same pattern as `connectivityOwner_` :144-148), set on each accepted report along with `deps_.inbound->setOwnerPresent(true)`; in `sessionClosed()` clear each owned report type and recompute owner-presence (false when no owner of any type remains — connectivity owner counts); `handleReport` kGpsReport forwards `r.bearing_deg(), r.accuracy_m(), r.age_ms()` after the existing lat/lon validation.
- [ ] **Step 4:** targeted green → full `ctest` + app target.
- [ ] **Step 5:** commit `feat: ApiInboundState parity — GPS extras, staleness, owner-tracked clears (B0a)`.

**Acceptance:** all Step-1 cases green; existing connectivity-owner test still green; no proto diff.

---

### Task 6: QML consumer migration (B0b)

**Tier:** opus
**Files:** Modify `src/main.cpp` (context properties, after ApiServer creation ~:1285-1294 — `CompanionState` must be set once `apiServer->inboundState()` exists), `qml/widgets/BatteryWidget.qml` (:13-14), `qml/widgets/WeatherWidget.qml` (:26-35), `qml/widgets/CompanionStatusWidget.qml` (:13-18)
**Out of scope:** CompanionSettings.qml (Task 7); removing the legacy `CompanionService` exposure (B2).

- [ ] **Step 1:** `src/main.cpp` — alongside the other context properties, unconditionally:
```cpp
engine.rootContext()->setContextProperty("CompanionState", apiServer->inboundState());
```
(If the engine loads QML before the ApiServer exists in current ordering, set the property before `engine.load...` by hoisting ApiServer construction — verify actual order in file; the QML null-guards make a late set safe only if it is set before first paint of these widgets. State what you found in the commit message.)
- [ ] **Step 2:** widget edits — mechanical swap `CompanionService` → `CompanionState` in the null-safe readonly property lines of the three widgets, plus in `CompanionStatusWidget.qml` replace the dead read
  `property string proxy: CompanionService ? CompanionService.proxyStatus : ""` (a property that never existed — row was permanently "Proxy Off") with
  `readonly property bool proxyOn: CompanionState ? CompanionState.proxyActive : false` and update the two `proxy === "active"` conditions to `proxyOn`.
- [ ] **Step 3:** build app target + full ctest (QML lint/structure tests run in suite); manual smoke on desktop build: widgets render with "--"/offline placeholders (no companion connected on WSL).
- [ ] **Step 4:** commit `feat: companion widgets read CompanionState (API v1 inbound) — B0b`.

**Acceptance:** zero `CompanionService` references left in the three widget files; app builds; suite green.

---

### Task 7: CompanionSettings — status via API path, legacy controls intact (B0c)

**Tier:** opus
**Files:** Modify `qml/applications/settings/CompanionSettings.qml`
**Out of scope:** removing the `companion.enabled` toggle or legacy pairing UI (that IS the B2 diff); ApiSettings.qml (API pairing already lives there).

- [ ] **Step 1:** in the Status section only, swap data bindings from `CompanionService` to `CompanionState` (properties per Task 5's surface). Legacy-only bindings (`qrCodeDataUri`, pairing controls, the `companion.enabled` toggle) KEEP reading `CompanionService` — they control the still-running legacy listener; add one comment: `// Legacy pairing/enable rows: removed with CompanionListenerService at 9876 retirement (design 2026-07-11 §B2).`
- [ ] **Step 2:** `tests/test_settings_menu_structure.cpp` must stay green (`ctest -R settings_menu`); app target builds.
- [ ] **Step 3:** commit `feat: CompanionSettings status reads CompanionState; legacy controls annotated (B0c)`.

**Acceptance:** status rows bind to `CompanionState`; legacy rows untouched and functional; suite green.

---

### Task 8: IPC companion_status reads inbound state (B0d)

**Tier:** sonnet
**Files:** Modify `src/core/services/IpcServer.hpp` (add `setInboundState(oap::api::ApiInboundState*)` + member), `src/core/services/IpcServer.cpp` (:400-415), `src/main.cpp` (wire it next to `setCompanionListenerService` ~:1025)
**Out of scope:** removing `setCompanionListenerService` (B2); other IPC handlers.

- [ ] **Step 1:** failing test — extend the IPC test that covers status handlers (pattern: `tests/test_ipc_install_theme.cpp` fixture): after `setInboundState(&state)` and `state.setBattery(55,true)`, `handleCompanionStatus()` JSON contains `"battery":55,"charging":true,"source":"api"`.
- [ ] **Step 2:** implement — `handleCompanionStatus()` prefers `inbound_` when set: connected/gps_lat/gps_lon/gps_speed/battery/charging/internet/proxy from `ApiInboundState` getters + `"source":"api"`; keep `vehicle_id` from `companion_` while it exists; fall back to the legacy body when `inbound_` is null.
- [ ] **Step 3:** targeted → full green; commit `feat: IPC companion_status sourced from ApiInboundState (B0d)`.

**Acceptance:** JSON keys unchanged plus `source`; web-config panel consumers unaffected (same key names).

---

### Task 9: Bench runbook (A3)

**Tier:** main (Fable session authors; encodes bench judgment)
**Files:** Create `docs/plans/2026-07-11-hfp-bench-runbook.md`

- [ ] Content mirrors design §A1 decision tree + §A3 order + §5 observables, fully self-contained for bench execution: substrate recording commands, override checks, A1a deploy/restart/codec-read commands, per-attempt validity gate + minimal L6 controls (`pw-link -l`, capture-level, mute checks), A1b install/hold/revert commands, L3/L4/L5/L6 rows with the exact `busctl` invocations from the archived D2 §11, companion cutover checklist (`companion.enabled: false`, restart, `ss -ltnp | grep 9876` empty, per-payload observables, companion log v1-only), inline RESULT placeholders per row.
- [ ] Commit `docs: HFP + cutover bench runbook`.

**Acceptance:** every row has a command and an expected observation; no row requires this conversation's context to execute.

---

### Task 10: Companion handoff prompt (B1)

**Tier:** main
**Files:** Create `/mnt/e/claude/personal/openautopro/companion-9876-migration-prompt.md` (outside this repo — sibling of `companion-api-v1.1-handoff-prompt.md`)

- [ ] Content per design §B1: sender inventory instructions; the definitive proto coverage table (GPS incl. bearing/accuracy/age → `GpsReport`, battery+charging → `BatteryReport`, internet/SOCKS5 → `ConnectivityReport`, time/timezone → `TimeReport`); v1 transport swap on the existing tested foundation; mutually-exclusive transports during cutover (no dual proxy/time publishers); flag-off-or-delete legacy client; validation checklist ending at the head-unit bench cutover (runbook §6) + companion-log v1-only check; hard rails (no proto edits — gaps come back as questions; head-unit contract frozen).
- [ ] No repo commit (file is outside); note its path in the Task 11 handoff entry.

**Acceptance:** prompt is self-contained for a fresh companion-repo session; includes the "flag back, never edit proto" rail.

---

### Task 11: Roadmap, deploy, handoff (integration)

**Tier:** main
**Files:** Modify `docs/roadmap-current.md`, `docs/session-handoffs.md`

- [ ] **Step 1:** roadmap — add to **Now**: this phase (mic bench pending, cutover pending companion); trim the superseded "Later" 9876 wording to point at the design doc.
- [ ] **Step 2:** full local verification: `cmake --build ~/builds/oap-hfp-9876 -j$(nproc) && ctest --test-dir ~/builds/oap-hfp-9876 --output-on-failure && cmake --build ~/builds/oap-hfp-9876 --target openauto-prodigy -j$(nproc)`.
- [ ] **Step 3:** `./cross-build.sh` (from the worktree) → rsync binary to Pi → restart service → journal check: the two positive subscription log lines from Tasks 1-2 present; no `Could not connect` for these services.
- [ ] **Step 4:** handoff entry (what/why/status/next: bench session; verification results verbatim). Commit `docs: stage-1 complete — bench-ready; runbook + companion prompt authored`.
- [ ] **Step 5:** review gate per AGENTS.md (`bash scripts/codex-review.sh <base>`) — adjudicate, then STOP for Matthew (push + bench scheduling are his calls).

**Acceptance:** Pi runs the new binary with clean journal; all docs updated; gate adjudicated; nothing pushed.

---

## Execution notes

- Waves: Tasks 1,2,3,4,5 parallel-safe (disjoint files); 6,7,8 after 5; 9,10,11 by the main session (11 last).
- Escalation ladder per AGENTS.md: two worker attempts → Codex (`gpt-5.6-sol`) with write access → Fable.
- B2 teardown is deliberately absent — planned as its own short plan after the bench cutover passes (design §4 gate).
