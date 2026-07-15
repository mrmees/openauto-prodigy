# B2 Teardown Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

Status: COMPLETED 2026-07-14
Design: `docs/plans/2026-07-14-b2-teardown-design.md` (Matthew-approved 2026-07-14)
Grounded on: `27b4392` (dev)

**Goal:** Delete the legacy `CompanionListenerService` (port 9876) and everything that dies with it; rehome the polkit rule; drop the custom-AP installer prompt; add reporting-session liveness expiry; land two riders (udisks polkit in prebuilt installer, `gps_stale` in IPC).

**Architecture:** Teardown-first on `dev` — Task 1 deletes the service and gets the tree green, Tasks 2–4 work in the clean tree. The only new logic is liveness expiry inside `ApiRequestHandlers`, reusing the existing ownership model (`reportingSessions_`, per-type owners, `recomputeOwnerPresence()`). One Codex gate over the full range at the end.

**Tech Stack:** C++17/Qt6 (QTimer/QElapsedTimer), QtTest, bash installers, CMake.

## Global Constraints

- Build ONLY in `~/builds/openauto-prodigy` (ext4). NEVER create an in-repo `build/` on `/mnt/e` (9p IO penalty).
- `ctest` NEVER compiles `main.cpp`. Any task touching `main.cpp` (Task 1) MUST run the explicit app-target build: `cmake --build ~/builds/openauto-prodigy -j$(nproc) --target openauto-prodigy`.
- Expected suite size after Task 1: **122 tests** (123 minus `test_companion_listener`). All must pass at every task boundary.
- Commit per task on `dev`. NOBODY pushes mid-execution (commit/push race rule); push only after the Codex gate passes.
- Workers read root `AGENTS.md` + the nested `AGENTS.md` nearest their files (`src/AGENTS.md` for Tasks 1/3).
- Error-string and JSON-key stability: IPC `companion_status` key names and the `"Companion service not available"` error string are consumed by the web-config panel — do not rename them.
- Line numbers below are from commit `27b4392`. If a hunk doesn't match, re-grep before editing; do not guess.

---

### Task 1: Core deletion + `companion_status` rewrite (+ `gps_stale` rider)

**Tier:** opus

**Files:**
- Delete: `src/core/services/CompanionListenerService.hpp`, `src/core/services/CompanionListenerService.cpp`, `tests/test_companion_listener.cpp`
- Modify: `src/CMakeLists.txt:62`, `tests/CMakeLists.txt:88`, `src/main.cpp` (:43, :403–445, :950–951, :956–963, :1148–1149, ~:1208–1220 comments), `src/core/plugin/IHostContext.hpp` (:16, :37), `src/core/plugin/HostContext.hpp` (:17, :33, :52), `src/core/services/IpcServer.hpp` (:14, :46–47, setter decl, :78), `src/core/services/IpcServer.cpp` (:6, :86–89, :406–442), `tests/test_plugin_manager.cpp:43`, `tests/test_plugin_model.cpp:42`
- Test: `tests/test_ipc_install_theme.cpp` (extend `companionStatusPrefersInboundStateWhenSet`)

**Interfaces:**
- Consumes: existing `ApiInboundState` getters (`connected()`, `gpsLat()`, `gpsLon()`, `gpsSpeedMps()`, `gpsStale()`, `phoneBattery()`, `phoneCharging()`, `internetAvailable()`, `proxyAddress()`).
- Produces: `IpcServer::handleCompanionStatus()` emits inbound-only JSON with a new `gps_stale` bool key; `IHostContext` no longer declares `companionListenerService()` (Task 3/4 rely on a tree with zero `CompanionListenerService` code references).

- [ ] **Step 1: Write the failing rider test**

In `tests/test_ipc_install_theme.cpp`, inside `companionStatusPrefersInboundStateWhenSet()` (starts :81), after the existing assertions on the `companion_status` response object (locate the local `QJsonObject` the round-trip returns — likely named `resp` or similar; use its actual name), add:

```cpp
        QVERIFY2(resp.contains("gps_stale"),
                 "companion_status must expose gps_stale (wishlist 2026-07-13)");
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) --target test_ipc_install_theme && ctest -R test_ipc_install_theme --output-on-failure
```
Expected: FAIL on the new `gps_stale` assertion.

- [ ] **Step 3: Delete the service files and CMake entries**

```bash
git rm src/core/services/CompanionListenerService.hpp src/core/services/CompanionListenerService.cpp tests/test_companion_listener.cpp
```

In `src/CMakeLists.txt` remove the line:
```cmake
    core/services/CompanionListenerService.cpp
```
In `tests/CMakeLists.txt` remove the line:
```cmake
oap_add_test(test_companion_listener SOURCES test_companion_listener.cpp)
```

- [ ] **Step 4: Remove main.cpp wiring**

Five hunks (all in `src/main.cpp`):

(a) Delete the include (:43):
```cpp
#include "core/services/CompanionListenerService.hpp"
```

(b) Delete the whole Companion Listener block (:403–445), from
```cpp
    // --- Companion Listener ---
    oap::CompanionListenerService* companionListener = nullptr;
```
through the closing brace of its `else` branch:
```cpp
    } else {
        qCInfo(lcCore) << "Companion: disabled in config";
    }
```

(c) Delete the IPC hookup (:950–951):
```cpp
    if (companionListener)
        ipcServer->setCompanionListenerService(companionListener);
```

(d) Delete the SystemServiceClient hookup (:956–963) — KEEP line :955 (`auto* systemClient = new oap::SystemServiceClient(&app);`), delete only:
```cpp
    if (companionListener)
        companionListener->setSystemServiceClient(systemClient);
    if (companionListener && systemClient) {
        QObject::connect(systemClient, &oap::SystemServiceClient::connectedChanged, systemClient, [=]() {
            if (systemClient->isConnected()) {
                companionListener->syncProxyRoute();
            }
        });
    }
```
Safety rationale (record in the commit message if the deletion feels scary): the API path re-applies the proxy route on EVERY accepted `ConnectivityReport` (`ApiInboundState::setConnectivity` emits `proxyRouteChanged` unconditionally → `main.cpp:1222` → `systemClient->setProxyRoute`), and the companion re-reports at ~1 Hz per contract — a daemon restart self-heals within ~1 s without this legacy resync hook.

(e) Delete the legacy context property (:1148–1149):
```cpp
    if (companionListener)
        engine.rootContext()->setContextProperty("CompanionService", companionListener);
```

Then rewrite the stale comment block above the `CompanionState` registration (~:1208–1220): keep the two functional lines
```cpp
    engine.rootContext()->setContextProperty("CompanionState", apiServer->inboundState());
    ipcServer->setInboundState(apiServer->inboundState());
```
exactly as they are, and replace the surrounding commentary (which references the deleted `CompanionService` property and `setCompanionListenerService()`) with:
```cpp
    // Companion phone reports (GPS / battery / connectivity) surfaced to QML
    // via the API v1 inbound cache (design §B0). Registered unconditionally —
    // widgets bind CompanionState whether or not the API server is running.
```
and, above the `setInboundState` line:
```cpp
    // IPC companion_status reads the same inbound state.
```

- [ ] **Step 5: Remove IHostContext/HostContext surface + mocks**

`src/core/plugin/IHostContext.hpp`: delete the forward declaration (:16) `class CompanionListenerService;` and the pure virtual (:37) `virtual CompanionListenerService* companionListenerService() = 0;`

`src/core/plugin/HostContext.hpp`: delete the setter (:17), the override (:33), and the member (:52):
```cpp
    void setCompanionListenerService(CompanionListenerService* svc) { companion_ = svc; }
    CompanionListenerService* companionListenerService() override { return companion_; }
    CompanionListenerService* companion_ = nullptr;
```

`tests/test_plugin_manager.cpp:43` and `tests/test_plugin_model.cpp:42`: delete the mock override line in each:
```cpp
    oap::CompanionListenerService* companionListenerService() override { return nullptr; }
```

- [ ] **Step 6: Rewrite IpcServer**

`src/core/services/IpcServer.hpp`: delete the forward decl (:14) `class CompanionListenerService;`, the setter declaration (`void setCompanionListenerService(CompanionListenerService* svc);`), and the member (:78) `CompanionListenerService* companion_ = nullptr;`. Replace the :46–47 comment with:
```cpp
    // API v1 inbound state — the only companion_status source since the B2
    // teardown (2026-07-14). Key names are stable for the web-config panel.
```

`src/core/services/IpcServer.cpp`: delete the include (:6) `#include "CompanionListenerService.hpp"` and the setter definition (:86–89). Replace `handleCompanionStatus()` (:406–442) in full with:
```cpp
QByteArray IpcServer::handleCompanionStatus()
{
    // Companion phone state over API v1 (ApiInboundState). Key names predate
    // v1 and stay stable for the web-config panel's consumers.
    if (!inbound_) return R"({"error":"Companion service not available"})";

    QJsonObject obj;
    obj["connected"] = inbound_->connected();
    obj["gps_lat"] = inbound_->gpsLat();
    obj["gps_lon"] = inbound_->gpsLon();
    obj["gps_speed"] = inbound_->gpsSpeedMps();
    obj["gps_stale"] = inbound_->gpsStale();
    obj["battery"] = inbound_->phoneBattery();
    obj["charging"] = inbound_->phoneCharging();
    obj["internet"] = inbound_->internetAvailable();
    obj["proxy"] = inbound_->proxyAddress();
    obj["source"] = "api";
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}
```
Note the deliberate drops: `vehicle_id` (zero consumers — verified in the design) and the legacy fallback body. `gps_stale` is the new rider key.

- [ ] **Step 7: Build everything and run the suite**

```bash
cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) --target openauto-prodigy && cmake --build . -j$(nproc) && ctest --output-on-failure
```
Expected: app target links; **122/122 pass**, including the Step 1 assertion. If the compiler surfaces `CompanionListenerService` references this plan missed, fix them (delete the reference, matching this task's pattern) and note the deviation in your report.

- [ ] **Step 8: Reference sweep gate**

```bash
git grep -In "CompanionListenerService" -- src tests
```
Expected: comment-only hits in `src/core/QrPng.hpp`, `src/core/api/ApiInboundState.{hpp,cpp}`, `src/core/api/ApiRequestHandlers.{hpp,cpp}`, `src/core/services/ClockSyncService.hpp` (swept in Tasks 3/4). ZERO code hits.

- [ ] **Step 9: Commit**

```bash
git add -A && git commit -m "feat!: retire CompanionListenerService (B2 teardown) — port 9876 is gone

companion_status is API-inbound-only (+gps_stale rider, -vehicle_id);
legacy daemon-reconnect route resync is redundant (ConnectivityReport
re-applies at ~1 Hz). Design: docs/plans/2026-07-14-b2-teardown-design.md"
```

---

### Task 2: Polkit rename + installer cleanup (custom-AP prompt drop, udisks rider)

**Tier:** sonnet

**Files:**
- Rename: `config/companion-polkit.rules` → `config/clock-sync-polkit.rules` (content unchanged)
- Modify: `install.sh` (:1418–1423 YAML block, :1437–1441 polkit block), `install-prebuilt.sh` (:212–213 prompt, :296–305 YAML block, :311–314 polkit block, + new udisks block)

**Interfaces:**
- Consumes: nothing from other tasks (independent of Task 1).
- Produces: installed polkit filename **stays** `/etc/polkit-1/rules.d/50-openauto-time.rules` (upgrade overwrite in place) — Task 4's docs reference this.

- [ ] **Step 1: Rename the rule file**

```bash
git mv config/companion-polkit.rules config/clock-sync-polkit.rules
```

- [ ] **Step 2: Update install.sh**

(a) Replace the polkit block (:1437–1441):
```bash
    # Companion app polkit rule (allows timedatectl set-time/set-timezone without sudo)
    if [[ -f "$INSTALL_DIR/config/companion-polkit.rules" ]]; then
        sudo cp "$INSTALL_DIR/config/companion-polkit.rules" /etc/polkit-1/rules.d/50-openauto-time.rules
        ok "Companion polkit rule installed"
    fi
```
with:
```bash
    # Clock-sync polkit rule (allows timedatectl set-time/set-timezone/set-ntp
    # without sudo, for ClockSyncService's TimeReport clock stepping)
    if [[ -f "$INSTALL_DIR/config/clock-sync-polkit.rules" ]]; then
        sudo cp "$INSTALL_DIR/config/clock-sync-polkit.rules" /etc/polkit-1/rules.d/50-openauto-time.rules
        ok "Clock-sync polkit rule installed"
    fi
```

(b) In the default-config YAML heredoc (~:1418–1423), delete exactly these three lines (keep the heredoc's surrounding structure intact):
```yaml
companion:
  enabled: true
  port: 9876
```

- [ ] **Step 3: Update install-prebuilt.sh**

(a) Delete the custom-AP prompt (:212–213) — `AP_IP="10.0.0.1"` at :30 remains the sole (fixed) source:
```bash
        read -p "AP static IP [10.0.0.1]: " AP_IP
        AP_IP=${AP_IP:-10.0.0.1}
```

(b) Delete the same three-line `companion:` YAML block from its default-config heredoc (~:296–305 region).

(c) Apply the same polkit-block replacement as install.sh Step 2(a) to lines :311–314.

(d) Immediately after the BlueZ agent polkit block (ends ~:318), add the udisks block, verbatim from install.sh:1449–1454:
```bash
    # udisks2 polkit rule (allows the service user to mount/unmount and
    # power off USB media without a password prompt)
    if [[ -f "$INSTALL_DIR/config/udisks-polkit.rules" ]]; then
        sudo cp "$INSTALL_DIR/config/udisks-polkit.rules" /etc/polkit-1/rules.d/50-openauto-udisks.rules
        ok "udisks2 polkit rule installed"
    fi
```

- [ ] **Step 4: Verify**

```bash
bash -n install.sh && bash -n install-prebuilt.sh && \
git grep -c "companion-polkit" -- install.sh install-prebuilt.sh; \
git grep -n "AP static IP" install-prebuilt.sh; \
git grep -c "udisks-polkit" install-prebuilt.sh; \
git grep -n "port: 9876" -- install.sh install-prebuilt.sh
```
Expected: both `bash -n` silent; `companion-polkit` grep exits 1 (zero hits); "AP static IP" zero hits; `udisks-polkit` count = 2 (guard + cp); `port: 9876` zero hits. Then run the release-package test (packager stages installer + config files):
```bash
cd ~/builds/openauto-prodigy && ctest -R test_prebuilt_release_package --output-on-failure
```
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat: installers — clock-sync polkit rename, companion.* namespace removed, custom-AP prompt dropped, udisks rule in prebuilt

Installed filename stays 50-openauto-time.rules (upgrade overwrite).
AP is fixed at 10.0.0.1/24 by decision (Matthew 2026-07-14, design §2)."
```

---

### Task 3: Liveness expiry in ApiRequestHandlers (TDD)

**Tier:** opus

**Files:**
- Modify: `src/core/api/ApiRequestHandlers.hpp`, `src/core/api/ApiRequestHandlers.cpp`
- Test: `tests/test_api_request_handlers.cpp`

**Interfaces:**
- Consumes: `ApiInboundState::clearGps()/clearBattery()/setConnectivity()/setOwnerPresent()` (unchanged); `ApiSession` pointers as hash keys (existing pattern).
- Produces (test seams other steps use): `void setLivenessThresholdMs(int ms)`, `void setLivenessNowFnForTest(std::function<qint64()> fn)`, `bool livenessTimerActiveForTest() const`, `void expireStaleReportingSessions()`. Private helpers: `clearReportingState(ApiSession*)`, `noteReportAccepted(ApiSession*)`, `updateLivenessTimer()`, `qint64 livenessNowMs() const`.

- [ ] **Step 1: Write the failing tests**

In `tests/test_api_request_handlers.cpp`, add to the anonymous namespace (after `clientHello()`):
```cpp
QByteArray batteryReport(int percent, bool charging) {
    pb::ApiMessage m;
    auto* r = m.mutable_battery_report();
    r->set_percent(percent);
    r->set_charging(charging);
    return serialize(m);
}

QByteArray gpsReport(double lat, double lon) {
    pb::ApiMessage m;
    auto* r = m.mutable_gps_report();
    r->set_latitude(lat);
    r->set_longitude(lon);
    return serialize(m);
}

QByteArray connectivityReport(bool internet, bool socks, quint16 port) {
    pb::ApiMessage m;
    auto* r = m.mutable_connectivity_report();
    r->set_internet_available(internet);
    r->set_socks5_active(socks);
    r->set_socks5_port(port);
    return serialize(m);
}
```
(If equivalent helpers already exist for the GPS/connectivity tests, reuse those instead of duplicating.)

Declare in the test class and implement these seven slots:

```cpp
void TestApiRequestHandlers::testLivenessExpiryClearsReportingRole() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});
    qint64 fakeNow = 0;
    handler.setLivenessNowFnForTest([&] { return fakeNow; });

    auto* transport = new FakeTransport();
    ApiSessionDeps deps; deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());
    transport->injectMessage(batteryReport(80, true));
    transport->injectMessage(gpsReport(45.0, -93.0));
    QVERIFY(inbound.connected());
    QCOMPARE(inbound.phoneBattery(), 80);
    QVERIFY(inbound.gpsValid());

    fakeNow = 30001;   // strictly past the 30 s default threshold
    handler.expireStaleReportingSessions();
    QVERIFY(!inbound.connected());
    QCOMPARE(inbound.phoneBattery(), -1);
    QVERIFY(!inbound.gpsValid());
}

void TestApiRequestHandlers::testLivenessExpiryTearsProxyRoute() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});
    qint64 fakeNow = 0;
    handler.setLivenessNowFnForTest([&] { return fakeNow; });

    auto* transport = new FakeTransport();
    ApiSessionDeps deps; deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());
    transport->injectMessage(connectivityReport(true, true, 1080));
    QVERIFY(inbound.proxyActive());

    QSignalSpy routeSpy(&inbound, &ApiInboundState::proxyRouteChanged);
    fakeNow = 30001;
    handler.expireStaleReportingSessions();
    QVERIFY(!inbound.proxyActive());
    QVERIFY(routeSpy.count() >= 1);
    QCOMPARE(routeSpy.last().at(0).toBool(), false);   // route torn down
}

void TestApiRequestHandlers::testLivenessExpirySparesNonReportingRoles() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});
    qint64 fakeNow = 0;
    handler.setLivenessNowFnForTest([&] { return fakeNow; });

    auto* transport = new FakeTransport();
    ApiSessionDeps deps; deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    pb::ApiMessage reg;
    reg.set_request_id(5);
    reg.mutable_register_actions_request()->add_actions()->set_id("testapp.hello");
    transport->injectMessage(serialize(reg));
    transport->injectMessage(batteryReport(50, false));

    fakeNow = 30001;
    handler.expireStaleReportingSessions();
    QVERIFY(!inbound.connected());                       // reporting role expired...
    QCOMPARE(session.state(), ApiSession::State::Ready);  // ...but the session lives
    QVERIFY(actions.contains("testapp.hello"));            // ...and keeps its action
}

void TestApiRequestHandlers::testLivenessRevivalOnNextReport() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});
    qint64 fakeNow = 0;
    handler.setLivenessNowFnForTest([&] { return fakeNow; });

    auto* transport = new FakeTransport();
    ApiSessionDeps deps; deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());
    transport->injectMessage(batteryReport(80, true));

    fakeNow = 30001;
    handler.expireStaleReportingSessions();
    QVERIFY(!inbound.connected());

    transport->injectMessage(batteryReport(75, true));   // wedged phone woke up
    QVERIFY(inbound.connected());
    QCOMPARE(inbound.phoneBattery(), 75);
}

void TestApiRequestHandlers::testLivenessPerSessionIndependence() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});
    qint64 fakeNow = 0;
    handler.setLivenessNowFnForTest([&] { return fakeNow; });

    auto* transportA = new FakeTransport();
    ApiSessionDeps depsA; depsA.requests = &handler;
    ApiSession sessionA(transportA, depsA);
    transportA->injectMessage(clientHello());
    transportA->injectMessage(batteryReport(80, true));

    auto* transportB = new FakeTransport();
    ApiSessionDeps depsB; depsB.requests = &handler;
    ApiSession sessionB(transportB, depsB);
    fakeNow = 20000;
    transportB->injectMessage(clientHello());
    transportB->injectMessage(gpsReport(45.0, -93.0));

    fakeNow = 31000;   // A is 31 s stale, B only 11 s
    handler.expireStaleReportingSessions();
    QCOMPARE(inbound.phoneBattery(), -1);   // A's battery cleared
    QVERIFY(inbound.gpsValid());            // B's GPS intact
    QVERIFY(inbound.connected());           // presence survives via B
}

void TestApiRequestHandlers::testLivenessBoundaryNotExpiredAtThreshold() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});
    qint64 fakeNow = 0;
    handler.setLivenessNowFnForTest([&] { return fakeNow; });

    auto* transport = new FakeTransport();
    ApiSessionDeps deps; deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());
    transport->injectMessage(batteryReport(80, true));

    fakeNow = 30000;   // exactly the threshold: NOT expired (strict >)
    handler.expireStaleReportingSessions();
    QVERIFY(inbound.connected());

    fakeNow = 30001;
    handler.expireStaleReportingSessions();
    QVERIFY(!inbound.connected());
}

void TestApiRequestHandlers::testLivenessTimerArmsAndDisarms() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    QVERIFY(!handler.livenessTimerActiveForTest());   // idle until a report

    auto* transport = new FakeTransport();
    ApiSessionDeps deps; deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());
    transport->injectMessage(batteryReport(80, true));
    QVERIFY(handler.livenessTimerActiveForTest());

    transport->close();   // session teardown -> reporting set empties
    QVERIFY(!handler.livenessTimerActiveForTest());
}
```
Notes for the implementer: `actions.contains(...)` — if `ActionRegistry` has no `contains()`, assert via the pattern `testDisconnectUnregisters` uses (list round-trip or `dispatchAction` result). `transport->close()` drives `sessionClosed` synchronously via the FakeTransport `closed()` signal — mirror how `testReportOwnerCloseClearsState` triggers disconnect.

- [ ] **Step 2: Run tests to verify they fail**

```bash
cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) --target test_api_request_handlers 2>&1 | tail -20
```
Expected: COMPILE FAILURE — `setLivenessNowFnForTest` etc. don't exist yet. That is the failing state for API-shape TDD; proceed.

- [ ] **Step 3: Implement**

`src/core/api/ApiRequestHandlers.hpp` — add includes `#include <QTimer>`, `#include <QElapsedTimer>`, `#include <functional>`. After `void sessionClosed(ApiSession* session) override;` add:
```cpp
    // Liveness expiry (B2 design §5): a reporting session whose last accepted
    // report is older than the threshold loses its reporting role — presence,
    // per-type report ownership, cached inbound state, proxy route. Its
    // registered actions, notifications, and the socket itself are untouched;
    // a later accepted report re-registers it exactly like a first report.
    // The sweep runs on a coarse timer only while reporting sessions exist.
    // Threshold = 30 s (~30 missed beats at the companion's ~1 Hz contract
    // cadence, symmetric with the GPS staleness window).
    void setLivenessThresholdMs(int ms) { livenessThresholdMs_ = ms; }
    void setLivenessNowFnForTest(std::function<qint64()> fn) { livenessNow_ = std::move(fn); }
    bool livenessTimerActiveForTest() const { return livenessTimer_.isActive(); }
    void expireStaleReportingSessions();
```
In the private section add:
```cpp
    // Strip the session's reporting role: presence-set membership, per-type
    // report ownership + the cached state each owned (GPS, battery, proxy
    // route). Shared by sessionClosed() and liveness expiry. Never touches
    // actions/notifications/the socket.
    void clearReportingState(ApiSession* session);
    // Accepted-report bookkeeping: presence set + liveness stamp + timer arm.
    void noteReportAccepted(ApiSession* session);
    void updateLivenessTimer();
    qint64 livenessNowMs() const;
```
and the members (next to `reportingSessions_`):
```cpp
    QHash<ApiSession*, qint64> lastReportMs_;   // monotonic ms of last accepted report
    int livenessThresholdMs_ = 30000;
    QTimer livenessTimer_;
    QElapsedTimer livenessClock_;
    std::function<qint64()> livenessNow_;       // test seam; default = livenessClock_
```
While here, update the `reportingSessions_`/owner comments (:89–98) that say "removed on close" / "Legacy CompanionListenerService parity (clearClientSession() …)" to reflect the two removal paths and drop the legacy reference, e.g. "…a session is removed on close or when its reporting role expires (liveness)" and "cleared — with the cached state — by clearReportingState() when the owning session closes or expires."

`src/core/api/ApiRequestHandlers.cpp`:

Constructor becomes:
```cpp
ApiRequestHandlers::ApiRequestHandlers(Deps deps, QObject* parent)
    : QObject(parent), deps_(deps) {
    livenessClock_.start();
    livenessTimer_.setInterval(5000);
    connect(&livenessTimer_, &QTimer::timeout,
            this, &ApiRequestHandlers::expireStaleReportingSessions);
}
```

Replace the report-clearing tail of `sessionClosed()` (:141–160 — everything from the `// Clear every report type…` comment through `recomputeOwnerPresence();`) with:
```cpp
    // Strip the reporting role (presence + owned report state + route).
    clearReportingState(session);
```

New functions (place after `recomputeOwnerPresence()`):
```cpp
void ApiRequestHandlers::clearReportingState(ApiSession* session) {
    // A non-owner must never touch a report type it doesn't own.
    if (session == gpsOwner_) {
        gpsOwner_ = nullptr;
        if (deps_.inbound) deps_.inbound->clearGps();
    }
    if (session == batteryOwner_) {
        batteryOwner_ = nullptr;
        if (deps_.inbound) deps_.inbound->clearBattery();
    }
    if (session == connectivityOwner_) {
        connectivityOwner_ = nullptr;
        if (deps_.inbound)
            deps_.inbound->setConnectivity(QString(), false, 0, QString());
    }
    reportingSessions_.remove(session);
    lastReportMs_.remove(session);
    recomputeOwnerPresence();
    updateLivenessTimer();
}

void ApiRequestHandlers::noteReportAccepted(ApiSession* session) {
    reportingSessions_.insert(session);
    lastReportMs_.insert(session, livenessNowMs());
    recomputeOwnerPresence();
    updateLivenessTimer();
}

void ApiRequestHandlers::updateLivenessTimer() {
    if (reportingSessions_.isEmpty())
        livenessTimer_.stop();
    else if (!livenessTimer_.isActive())   // never restart: a restart on every
        livenessTimer_.start();            // 1 Hz report would starve the tick
}

qint64 ApiRequestHandlers::livenessNowMs() const {
    return livenessNow_ ? livenessNow_() : livenessClock_.elapsed();
}

void ApiRequestHandlers::expireStaleReportingSessions() {
    const qint64 now = livenessNowMs();
    const QList<ApiSession*> sessions = lastReportMs_.keys();   // snapshot: clearReportingState mutates
    for (ApiSession* s : sessions) {
        const qint64 age = now - lastReportMs_.value(s);
        if (age > livenessThresholdMs_) {
            qInfo() << "API: reporting session expired after" << age
                    << "ms without an accepted report";
            clearReportingState(s);
        }
    }
}
```

In `handleReport()`, replace all four occurrences of the pair
```cpp
            reportingSessions_.insert(session);
            recomputeOwnerPresence();
```
with
```cpp
            noteReportAccepted(session);
```
(GPS :382–383, battery :395–396, connectivity :424–425, time :437–438 — the per-type owner assignments directly above each stay untouched.)

- [ ] **Step 4: Run tests to verify they pass**

```bash
cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) --target test_api_request_handlers && ctest -R test_api_request_handlers --output-on-failure
```
Expected: PASS, all existing + 7 new methods.

- [ ] **Step 5: Full suite + app target**

```bash
cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) --target openauto-prodigy && cmake --build . -j$(nproc) && ctest --output-on-failure
```
Expected: 122/122.

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "feat: reporting-session liveness expiry — silently-vanished phone drops connected within ~35 s

30 s report-age threshold per owning session, 5 s sweep armed only while
reporting sessions exist; expiry strips ONLY the reporting role (actions,
notifications, socket untouched) and reuses the sessionClosed clear path
(extracted clearReportingState). Closes the PR #19 deferred gate finding."
```

---

### Task 4: Docs, comments, and wishlist sweep

**Tier:** sonnet

**Files:**
- Modify: `src/core/QrPng.hpp`, `src/core/api/ApiInboundState.hpp`, `src/core/api/ApiInboundState.cpp`, `src/core/services/ClockSyncService.hpp`, `qml/widgets/WeatherWidget.qml`, `qml/applications/settings/ApiSettings.qml`, `tests/test_settings_menu_structure.cpp`, `tests/test_ipc_install_theme.cpp`, `docs/architecture.md`, `docs/reference/plugin-api.md`, `docs/reference/settings-tree.md`, `docs/roadmap-current.md`, `docs/wishlist.md`
- Move: `docs/plans/2026-07-11-hfp-mic-9876-retirement-design.md`, `docs/plans/2026-07-11-hfp-mic-9876-retirement-plan.md`, `docs/plans/2026-07-11-hfp-bench-runbook.md` → `docs/archive/plans/` (Status → COMPLETED)

**Interfaces:** none consumed/produced — text only. Line refs are pre-Task-1..3; re-grep each target string.

- [ ] **Step 1: Code/QML/test comment updates**

(a) `src/core/QrPng.hpp` — replace
```cpp
// base64 PNG data URI ready for a QML Image source. Shared by the API
// pairing surface; the legacy CompanionListenerService keeps its private
// copy until the 9876 retirement (B2) deletes it.
```
with
```cpp
// base64 PNG data URI ready for a QML Image source. Used by the API
// pairing surface (ApiServer::pairingQrDataUri).
```

(b) `src/core/api/ApiInboundState.hpp:8–9` — replace
```cpp
// Q_PROPERTY surface is consumed by QML widgets (companion status) and mirrors
// the legacy CompanionListenerService names it replaces (design §B0). The
```
with
```cpp
// Q_PROPERTY surface is consumed by QML widgets (companion status); the names
// are kept from the retired legacy listener for QML/IPC stability (§B0). The
```

(c) `src/core/api/ApiInboundState.cpp:82–83` — replace
```cpp
        // Compose the head unit's SOCKS5 route: the proxy host is the phone's
        // (this connection's) peer address (CompanionListenerService.cpp:448).
```
with
```cpp
        // Compose the head unit's SOCKS5 route: the proxy host is the phone's
        // (this connection's) peer address.
```

(d) `src/core/services/ClockSyncService.hpp:11–12` — replace
```cpp
// that previously lived twice: CompanionListenerService::adjustClock (legacy
// 9876 path, retired at B2) and a static mirror in main.cpp.
```
with
```cpp
// that previously lived twice (the legacy companion listener and a static
// mirror in main.cpp; both retired in the B2 teardown, 2026-07-14).
```

(e) `qml/widgets/WeatherWidget.qml:29` — in the comment, replace `CompanionListenerService` with `CompanionState`.

(f) `qml/applications/settings/ApiSettings.qml:5–9` — replace the header comment block with:
```qml
// The merged "Companion" settings page (design 2026-07-14): API v1 pairing,
// live phone status (CompanionState = ApiInboundState), and the API toggles.
// The legacy CompanionSettings page died in the merge; the legacy listener
// itself was deleted in the B2 teardown (2026-07-14).
```

(g) `tests/test_settings_menu_structure.cpp` — update the assertion message
`"The legacy companion.enabled toggle stays dead until B2 retires the namespace"` to
`"The retired companion.enabled toggle must not resurface (B2 teardown 2026-07-14)"`. The assertion itself stays.

(h) `tests/test_ipc_install_theme.cpp:23` — drop the stale parenthetical: replace `(same pattern as test_companion_listener.cpp).` with `(same-thread QLocalSocket pattern).`

- [ ] **Step 2: Docs updates**

(a) `docs/architecture.md:18` — remove `` `CompanionListenerService`, `` from the shared-host-services list.

(b) `docs/reference/plugin-api.md` — delete the entire `### CompanionListenerService` section (heading :298 through its closing `**Note:** This is a concrete class…` line, inclusive).

(c) `docs/reference/settings-tree.md` (§Companion, ~:145–150) — two edits in the same paragraph. Replace the sentence `The `companion.*` config keys still exist without UI until the B2 teardown retires them.` with:
```
The `companion.*` config namespace was retired in the B2 teardown
(2026-07-14). Stale `companion:` blocks in existing user configs are ignored
harmlessly; `~/.openauto/companion.key` and `~/.openauto/vehicle.id` are
orphaned legacy files (never auto-deleted).
```
And in the sentence above it, replace `the legacy 9876 pairing controls` with `the legacy pairing controls` (reference docs describe current state; the dated history lives in the handoff log).

(d) `docs/roadmap-current.md:77` — replace the `Remaining (this repo): **retire `CompanionListenerService` + port 9876**…` bullet with:
```
  - **DONE (2026-07-14):** `CompanionListenerService` + port 9876 retired (B2 teardown, `docs/plans/2026-07-14-b2-teardown-design.md`). Reporting-session liveness expiry shipped with it — a silently-vanished phone drops `connected` within ~35 s.
```

- [ ] **Step 3: Wishlist ledger**

(a) `:53` (theme-upload) — replace `Blocks legacy-9876 retirement; see roadmap companion-migration item.` with `The 9876 retirement it blocked landed 2026-07-14 (B2).`

(b) `:73` (daemon proxy-route auto-teardown) — the item STAYS; remove the clause `interaction with legacy `CompanionListenerService::proxyRouteApplied_` (won't re-apply after a behind-its-back disable) and` (leaving the flap-risk concern), and append: `App-side note: B2's liveness expiry (2026-07-14) already tears the route when the owning session goes silent >30 s; the daemon-side auto-disable remains as defense in depth below the app.`

(c) `:74` (RNG hygiene) — replace the item body after the bold title with: `CLOSED 2026-07-14 — the service was deleted in the B2 teardown.`

(d) `:91` (camelCase→hyphen dedup) — replace the item body after the bold title with: `CLOSED 2026-07-14 — legacy copy deleted (B2); the ThemeInstallRequest copy is the single shared implementation.`

(e) `:143` (custom AP) — append: `**Decision (Matthew, 2026-07-14):** prompt dropped from install-prebuilt.sh (B2); 10.0.0.1/24 is the enforced invariant. Revisit only if a real custom-subnet need appears.`

(f) PR #19 section, liveness item (:149) — replace the item body after the bold title with: `SHIPPED 2026-07-14 in the B2 teardown — 30 s report-age expiry per owning session, 5 s sweep, reporting-role-only (actions/notifications/socket untouched).`

- [ ] **Step 4: Archive the completed 2026-07-11 phase docs**

For each of `2026-07-11-hfp-mic-9876-retirement-design.md`, `2026-07-11-hfp-mic-9876-retirement-plan.md`, `2026-07-11-hfp-bench-runbook.md` in `docs/plans/`: edit the `Status:` header to `COMPLETED 2026-07-14` FIRST, then `git mv` to `docs/archive/plans/` (edit-then-mv in separate operations — `git mv` does not stage prior edits; `git add` the moved path explicitly). Fix any now-broken relative links the checker reports.

- [ ] **Step 5: Verify**

```bash
python3 scripts/check-doc-links.py && \
{ git grep -In -e "9876" -e "CompanionListener" -- src qml tests scripts config install.sh install-prebuilt.sh && echo LEAK || echo CODE-CLEAN; } && \
git grep -lI -e "9876" -e "CompanionListener" -- docs ':!docs/archive'
```
Expected: `OK: 0 broken links`; `CODE-CLEAN` (zero hits anywhere in code, QML, tests, scripts, config, installers); and the docs file-list must contain ONLY `docs/session-handoffs.md`, `docs/wishlist.md`, `docs/roadmap-current.md`, and the B2 pair `docs/plans/2026-07-14-b2-teardown-{design,plan}.md` — these carry dated ledger history by convention. Any other docs file in the list gets swept. Then full suite:
```bash
cd ~/builds/openauto-prodigy && ctest --output-on-failure
```
Expected: 122/122 (comment edits in tests must still compile).

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "docs: B2 teardown sweep — comments, reference docs, wishlist ledger, phase-doc archival"
```

---

## Post-task wrap-up (main session, after all four tasks)

1. **Codex gate** over the full range: `bash scripts/codex-review.sh` (per AGENTS.md §Tiered Execution Workflow; adjudicate findings, fix confirmed ones, one re-run max for substantial fixes).
2. **Cross-build + Pi deploy + journal check** (pre-flight lines healthy, no new warnings). On-Pi eyeball: Companion settings page, companion widgets over API v1, `nc -zv <pi> 9876` → connection refused.
3. **Handoff entry** in `docs/session-handoffs.md` (pre-push, per the settings-merge gate precedent) + flip this plan and the design doc to `COMPLETED` and `git mv` both to `docs/archive/plans/` in the same commit.
4. **Push `dev`** after the gate passes (no parallel work during push — commit/push race rule).
5. **Bench note (not a merge gate):** next bench visit, airplane-mode the phone mid-session and watch `connected` drop within ~35 s (liveness live check).
