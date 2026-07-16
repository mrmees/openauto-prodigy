# Bench-Findings Batch — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

Status: COMPLETED 2026-07-15 — Phase B 6-task SDD + gate closed; Phase A Stage A (probe GO) deployed; ALL bench rows PASS
**Design (read it first):** `docs/plans/2026-07-15-bench-findings-batch-design.md`
— twice Codex-reviewed (rounds 1+2 adjudicated; header records dispositions).
**Grounded against:** `dev` at `3aa21a2`.
**Codex plan review (gpt-5.6-sol, 2026-07-15):** verdict REWORK — 5 P1 /
5 P2 / 0 P3, ALL verified and accepted (zero dismissals) and incorporated:
QSignalSpy include + inline-slot/QSKIP conventions (Task 5), Stage B build +
ownership files (Task 2B), QPluginLoader complete-type include +
PreventUnloadHint clearing (Task 7), brace-aware stale-ref sweep + archival
restaleness guard (Task 8 / Ship Ceremony), rebuild-before-green + cd fixes,
IPC-level persistence test added (Task 4), test-class placement + exactly-one
pluginFailed assertions (Task 7), DoR out-of-scope lines on all dispatched
tasks. Probe methodology, QML patterns, and fixture setup validated by the
review with no findings.

**Goal:** Fix the HFP-SCO/EQ-tap routing collision (live regression), stop
deploys kicking the phone off BT, make input-device and master-volume settings
survive restarts, validate dynamic plugin binaries at load time, and clean
stale roadmap/architecture references.

**Architecture:** Item 4 is empirically gated: a live probe decides whether a
WirePlumber-rule discriminator (Stage A) or app-side retargeting (Stage B)
fixes the SCO hijack. Items 2–3 consolidate persistence on existing canonical
YAML keys with one debounced, shutdown-flushed writer per value. Item 5 makes
`PluginLoader` return an owning record so mismatched binaries can be unloaded
before `initialize()`.

**Tech stack:** Qt 6.8 system packages, PipeWire 1.4 / WirePlumber 0.5,
BlueZ D-Bus, yaml-cpp, QtTest.

## Global Constraints

- Build in `~/builds/openauto-prodigy` (ext4), NEVER in-repo. Suite:
  `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest --output-on-failure`
- ctest does NOT compile `main.cpp` — before claiming green:
  `cmake --build . --target openauto-prodigy -j$(nproc)`
- `IAudioService`/`IEqualizerService`/`IHostContext` virtual signatures MUST
  NOT change (plugin ABI). No `HOST_API_VERSION` bump is expected; if you
  believe you need one, STOP and escalate.
- Lock order everywhere: PW loop lock FIRST, then `mutex_`
  (`AudioService.cpp` ABBA rule).
- Never set `node.dont-fallback` on any bluez retarget rule.
- The IPC JSON field names (`input_device`, `master_volume`) are frozen API
  surface — YAML keys behind them may change, field names may not.
- Workers read root `AGENTS.md` + the nested AGENTS.md nearest their files
  (`src/AGENTS.md`, `qml/AGENTS.md`) before editing. Commit per task; nobody
  pushes mid-execution (Codex gate at the end).
- Docs never state exact test counts.
- Line numbers below are grounded at `64608d0`; pair them with the named
  symbols when the tree has drifted.

## Phase Structure

- **Phase A (bench-gated, HIGHEST priority — live call-audio regression):**
  Task 1 (probe, needs Matthew + a phone), then EXACTLY ONE of Task 2A / 2B.
- **Phase B (headless, no bench needed):** Tasks 3–8, executable in order
  while Phase A waits for bench time. Task 6 depends on Tasks 4 and 5.

---

### Task 1: Live discriminator probe — what can WirePlumber monitor rules see on A2DP vs SCO nodes?

**Tier:** main (bench-interactive; Matthew + any phone at the Pi)

**Files:**
- Create (Pi only, temporary): `/etc/wireplumber/wireplumber.conf.d/99-oap-probe.conf`
- No repo changes. Findings recorded in this plan file + session handoff.

**Interfaces:**
- Produces: a **Stage A GO / NO-GO verdict** and the recorded live properties
  (node.name, api.bluez5.profile values) of BOTH node types. Task 2A consumes
  the verified match property; Task 2B runs only on NO-GO.

**Why a probe:** `media.class` was bench-proven ABSENT at monitor-rule
evaluation (commit `9077e17`) even though it appears in `pw-dump` later.
Registry visibility (what `ScoNodeMonitor` sees) proves nothing about
rule-eval visibility. The probe plants marker properties via rules and checks
which markers actually landed.

- [ ] **Step 1: Write the probe conf to the Pi**

```bash
ssh matt@192.168.1.149 'sudo tee /etc/wireplumber/wireplumber.conf.d/99-oap-probe.conf' <<'EOF'
# TEMPORARY probe: which properties are visible at monitor-rule evaluation?
monitor.bluez.rules = [
  { matches = [ { api.bluez5.profile = "a2dp-source" } ]
    actions = { update-props = { openauto.probe.a2dp = true } } }
  { matches = [ { api.bluez5.profile = "headset-audio-gateway" } ]
    actions = { update-props = { openauto.probe.hfp = true } } }
  { matches = [ { node.name = "~bluez_input.*" } ]
    actions = { update-props = { openauto.probe.name = true } } }
]
EOF
```

- [ ] **Step 2: Restart wireplumber and re-create the A2DP node**

```bash
ssh matt@192.168.1.149 'systemctl --user restart wireplumber'
```

Then Matthew: toggle BT off/on on the phone (or pause/unpause long enough for
a transport cycle) and start music playing. The node must be CREATED after
the restart for the monitor rules to have evaluated.

- [ ] **Step 3: Inspect the A2DP node's markers**

```bash
ssh matt@192.168.1.149 'pw-dump | jq -r ".[] | select(.info.props[\"node.name\"] // \"\" | startswith(\"bluez_input\")) | .info.props | {name: .[\"node.name\"], class: .[\"media.class\"], profile: .[\"api.bluez5.profile\"], probeA2dp: .[\"openauto.probe.a2dp\"], probeHfp: .[\"openauto.probe.hfp\"], probeName: .[\"openauto.probe.name\"], target: .[\"target.object\"]}"'
```

Expected: `probeName: "true"` (control — proves probe conf loaded). Record
whether `probeA2dp` is set and the exact `profile` string.

- [ ] **Step 4: Place a call and inspect the SCO node's markers**

Matthew places (or receives) a call. While it is ACTIVE, run the same command
as Step 3. Record: SCO node's `node.name`, `profile`, and which probe markers
are present. Also note `target` — with the current shipped rule the SCO node
will show `target.object = "openauto-bt-eq-in"`, confirming the hijack live.

- [ ] **Step 5: Remove the probe and restore**

```bash
ssh matt@192.168.1.149 'sudo rm /etc/wireplumber/wireplumber.conf.d/99-oap-probe.conf && systemctl --user restart wireplumber'
```

- [x] **Step 6: Record the verdict — STAGE A GO (2026-07-15 bench)**

RESULT: `api.bluez5.profile` IS visible at monitor-rule evaluation.
A2DP node `bluez_input.D4_5B_51_B3_66_15.2` (aptX): probeA2dp=true,
probeName=true, probeHfp=absent. SCO downlink `bluez_input.94_45_60_27_A2_9A.0`
(headset-audio-gateway): probeHfp=true, probeName=true, probeA2dp=absent —
AND carried `target.object = openauto-bt-eq-in` under the shipped rule (the
hijack observed LIVE mid-call; caller's voice mixed into the tap with music).
SCO uplink `bluez_output.94_45_60_27_A2_9A.1` untouched (name pattern miss).
Bonus finding: with HFP torn down (post-wireplumber-restart), the Pixel-in-AA
routes call downlink over the AA link and uses the PHONE mic for uplink —
gearhead degrades gracefully around our unimplemented AVInput (wishlist
intel). Steps 1-5 all executed as written (jq absent on Pi — python3 used).

---

### Task 2A: Stage A — positive A2DP discriminator in the WirePlumber rule (ONLY on Task 1 GO)

**Tier:** main (includes Pi deploy ops)

**Files:**
- Modify: `config/50-openauto-bt-eq.conf`
- Modify: `docs/architecture.md:47-56` (§ "BT A2DP EQ tap")

**Interfaces:**
- Consumes: Task 1's verified property (expected
  `api.bluez5.profile = "a2dp-source"` — substitute the recorded value if it
  differs).
- Produces: SCO nodes never retargeted; A2DP nodes still retargeted.

- [ ] **Step 1: Tighten the rule match**

Replace the `matches` block in `config/50-openauto-bt-eq.conf` (keep the
header comment's fallback warning and the `actions` block unchanged):

```
    matches = [
      {
        # Positive A2DP match — HFP SCO voice is ALSO a bluez_input.* node
        # (api.bluez5.profile = headset-audio-gateway) and must NOT be
        # retargeted into the tap (2026-07-15 post-merge P1). Property
        # availability at monitor-rule eval was live-probed 2026-07-15;
        # media.class is NOT available at that point (bench 2026-07-15).
        node.name = "~bluez_input.*"
        api.bluez5.profile = "a2dp-source"
      }
    ]
```

- [ ] **Step 2: Update `docs/architecture.md`**

In § "BT A2DP EQ tap", replace the sentence citing
`(~bluez_input.*, Stream/Output/Audio)` with the actual mechanism:

```
A WirePlumber rule (`config/50-openauto-bt-eq.conf`, installed to
`/etc/wireplumber/wireplumber.conf.d/`) retargets BlueZ A2DP input streams
(matched by `node.name = ~bluez_input.*` AND
`api.bluez5.profile = a2dp-source` — HFP SCO voice shares the name pattern
and is deliberately excluded) onto the app's capture node
`openauto-bt-eq-in`.
```

Also correct the tap-lifecycle claim in the same paragraph: the capture node
exists while `BtAudioTap` is up (brought up by `BtAudioPlugin`); the
"BT Audio" playback leg is what toggles with transport activity.

- [ ] **Step 3: Deploy to the Pi and verify both directions**

```bash
scp config/50-openauto-bt-eq.conf matt@192.168.1.149:/tmp/ && \
ssh matt@192.168.1.149 'sudo mv /tmp/50-openauto-bt-eq.conf /etc/wireplumber/wireplumber.conf.d/ && systemctl --user restart wireplumber'
```

Then (Matthew): cycle BT, play music → verify with the Step-3 jq from Task 1
that the A2DP node has `target: "openauto-bt-eq-in"`; place a call → verify
the SCO node has NO `target.object` and far-end voice is audible.

- [ ] **Step 4: Commit**

```bash
git add config/50-openauto-bt-eq.conf docs/architecture.md
git commit -m "fix(bt-eq): exclude HFP SCO from the A2DP tap retarget rule"
```

---

### Task 2B: Stage B — app-side A2DP retargeting (ONLY on Task 1 NO-GO) — **SKIPPED: Task 1 verdict was Stage A GO (2026-07-15)**

**Tier:** main — executed by the main (Fable) session directly, NOT
dispatched; it adds PipeWire-thread callbacks. This task is a bounded
contract; the main session expands it to full code only after Task 1 selects
it.

**Files:**
- Create: `src/plugins/bt_audio/BtEqRetargeter.{hpp,cpp}` (pattern:
  `src/core/audio/ScoNodeMonitor.{hpp,cpp}` — registry watch, props match,
  tracked map, `pw_thread_loop_lock` discipline)
- Modify: `src/CMakeLists.txt` — the source list is EXPLICIT (BT sources
  around `:83`); add `plugins/bt_audio/BtEqRetargeter.cpp` or it never
  compiles
- Modify: `src/plugins/bt_audio/BtAudioPlugin.hpp` (forward-declare + own the
  retargeter next to the existing tap state ~`:177`; explicit shutdown/reset
  in the capture-first teardown path) and `BtAudioPlugin.cpp` (wire it)
- Delete: `config/50-openauto-bt-eq.conf`
- Modify: `install.sh` (BT-EQ conf block ~`:1454-1460`),
  `install-prebuilt.sh` (BT-EQ conf block ~`:321-328`)
- Modify: `docs/architecture.md:47-56`
- Modify: `src/AGENTS.md` if new PW-thread rules emerge

**Out of scope:** no EQ/tap behavior changes beyond retargeting; no
`node.dont-fallback`; no HFP/SCO routing logic (exclusion only).

**Contract (from the design — all five bullets are acceptance criteria):**
1. Continuously watch registry node add/remove events (NOT a one-shot sweep);
   verify `api.bluez5.profile == "a2dp-source"` on each candidate node's
   registry props before touching it.
2. Bind the default `pw_metadata` object; retarget via
   `pw_metadata_set_property(metadata, node_id, "target.object", "Spa:String:JSON", "\"openauto-bt-eq-in\"")`;
   clear the key for tracked nodes when the tap goes down (fallback must
   remain WirePlumber-default routing — never `node.dont-fallback` semantics).
3. Sweep all existing tracked nodes when the A2DP transport becomes active
   AND retarget matching nodes that appear while active (covers the
   registry-global-after-active-edge race).
4. Deployment cleanup: both installers REPLACE their copy-if-present block
   with removal of the obsolete drop-in:

```bash
    # BT A2DP retargeting moved app-side (2026-07-15) — remove the obsolete
    # WirePlumber drop-in from earlier installs; SCO must not be retargeted.
    if [[ -f /etc/wireplumber/wireplumber.conf.d/50-openauto-bt-eq.conf ]]; then
        sudo rm /etc/wireplumber/wireplumber.conf.d/50-openauto-bt-eq.conf
        systemctl --user try-restart wireplumber 2>/dev/null || true
        ok "Obsolete BT-EQ WirePlumber rule removed (retargeting is app-side now)"
    fi
```

   Plus the live Pi: `sudo rm` the installed drop-in + wireplumber restart
   BEFORE the app deploy.
5. Acceptance: installed drop-in ABSENT on the Pi; music tap-routed from
   birth AND on app-restart-during-live-streaming (this task absorbs the
   wishlisted sweep item — flip that wishlist entry on ship); SCO direct;
   the full Task-1-style jq verification for both node types.

This task also swallows the wishlisted "tap sweep of pre-existing live
bluez_input nodes" — record that in the wishlist flip at ship time.

Final step (explicit): full build + suite + app target
(`cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest --output-on-failure && cmake --build . --target openauto-prodigy -j$(nproc)`),
then commit all files above in one commit:
`git commit -m "fix(bt-eq): app-side A2DP retargeting — SCO excluded, WirePlumber rule retired"`.

---

### Task 3: Remove the ExecStopPost `bluetoothctl disconnect` hook

**Tier:** sonnet

**Files:**
- Modify: `install.sh:1723` (inside the `openauto-prodigy.service` heredoc in
  the service-creation function)

**Interfaces:**
- Consumes: nothing. Produces: nothing other tasks rely on. The LIVE Pi unit
  edit happens at deploy time (Ship Ceremony step 3), not in this task.

**Out of scope:** no other unit-file changes (the `wf-panel-restore`
ExecStopPost stays); no `install-prebuilt.sh` changes (it never had the
hook); no Pi-side edits.

- [ ] **Step 1: Delete exactly this line from the heredoc**

```
ExecStopPost=-/bin/sh -c '[ "\$SERVICE_RESULT" = "success" ] && timeout 5 /usr/bin/bluetoothctl disconnect || true'
```

The adjacent `wf-panel-restore` ExecStopPost line STAYS. (Context: the hook's
2026-03-02 rationale — commit `843f347`, "dead audio sink" — is obsoleted by
the WirePlumber fallback; `install-prebuilt.sh` never had the hook.)

- [ ] **Step 2: Verify**

```bash
grep -n "bluetoothctl disconnect" install.sh
```

Expected: no output. Also `bash -n install.sh` → exit 0 (syntax intact).

- [ ] **Step 3: Commit**

```bash
git add install.sh
git commit -m "fix(install): stop kicking the phone off BT on every clean service stop"
```

---

### Task 4: Input-device persistence — consolidate on `audio.microphone.device`

**Tier:** opus

**Files:**
- Modify: `qml/applications/settings/AudioSettings.qml:71-95` (the
  `FullScreenPicker` with `id: inputPicker`)
- Modify: `src/core/services/IpcServer.cpp:390-391`
  (`handleSetAudioConfig`, the `input_device` persist branch)
- Modify: `docs/reference/settings-tree.md:72` (Input Device row)
- Test: `tests/test_yaml_config.cpp` (extend)
- Test: Create `tests/test_ipc_audio_config.cpp` + register in
  `tests/CMakeLists.txt`

**Out of scope:** NO AA microphone transport work (wishlisted separately); no
`inputDeviceChanged` signal / Q_PROPERTY on AudioService; no schema changes;
no migration of manually authored `audio.input_device` keys.

**Interfaces:**
- Consumes: `YamlConfig::setValueByPath` / `microphoneDevice()`,
  `AudioService::inputDevice()` (Q_INVOKABLE),
  `AudioInputDeviceModel.indexOfDevice(QString)` + `countChanged` (both exist,
  `src/ui/AudioDeviceModel.hpp`).
- Produces: the IPC persist block shape that Task 6 rewrites — Task 6's final
  block already includes this task's key change; execute 4 before 6.

- [ ] **Step 1: Write the failing test**

In `tests/test_yaml_config.cpp`, add to the test class (match the file's
existing slot style):

```cpp
void testMicrophoneDevicePathWrite()
{
    oap::YamlConfig cfg;
    // Canonical key: schema-valid, round-trips, feeds the startup getter.
    QVERIFY(cfg.setValueByPath("audio.microphone.device", "alsa_input.usb-mic"));
    QCOMPARE(cfg.valueByPath("audio.microphone.device").toString(),
             QString("alsa_input.usb-mic"));
    QCOMPARE(cfg.microphoneDevice(), QString("alsa_input.usb-mic"));
    // Regression pin for the 2026-07-15 root cause: the dead key the UI used
    // to write is NOT in the defaults schema and must stay rejected.
    QVERIFY(!cfg.setValueByPath("audio.input_device", "anything"));
}
```

- [ ] **Step 2: Run it — expect PASS already** (this pins existing behavior;
  it fails only if someone later adds `audio.input_device` to the schema)

```bash
cd ~/builds/openauto-prodigy && cmake --build . --target test_yaml_config -j$(nproc) && ctest -R test_yaml_config --output-on-failure
```

Expected: PASS. (This is a characterization test, not TDD red — the C++
behavior is already correct; the bug is the callers' key choice.)

- [ ] **Step 2b: Write the failing IPC persistence test**

Create `tests/test_ipc_audio_config.cpp` (same-thread `QLocalSocket`
round-trip pattern — copy the `roundTrip` helper verbatim from
`tests/test_ipc_install_theme.cpp:24-40`):

```cpp
#include <QTest>
#include <QTemporaryDir>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QElapsedTimer>

#include "core/services/IpcServer.hpp"
#include "core/services/AudioService.hpp"
#include "core/YamlConfig.hpp"

using namespace oap;

class TestIpcAudioConfig : public QObject {
    Q_OBJECT

    // Send one newline-framed request, return the parsed JSON response
    // object (same-thread QLocalSocket pattern — see test_ipc_install_theme).
    QJsonObject roundTrip(const QString& socketPath, const QJsonObject& request) {
        QLocalSocket sock;
        sock.connectToServer(socketPath);
        if (!sock.waitForConnected(2000)) return {};
        sock.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n");
        sock.flush();
        QElapsedTimer timer;
        timer.start();
        while (sock.bytesAvailable() == 0 && timer.elapsed() < 2000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            sock.waitForReadyRead(20);
        }
        const QByteArray buf = sock.readAll();
        sock.disconnectFromServer();
        return QJsonDocument::fromJson(buf.trimmed()).object();
    }

private slots:
    void setAudioConfigPersistsMicrophoneDevice() {
        QTemporaryDir dir;
        const QString yamlPath = dir.path() + "/config.yaml";
        YamlConfig cfg;
        AudioService audio;
        IpcServer server;
        server.setAudioService(&audio);
        server.setConfig(&cfg, yamlPath);
        const QString sockPath = dir.path() + "/ipc.sock";
        QVERIFY(server.start(sockPath));

        QJsonObject req{{"command", "set_audio_config"},
                        {"data", QJsonObject{{"input_device", "alsa_input.usb-mic"}}}};
        const QJsonObject resp = roundTrip(sockPath, req);
        QVERIFY(resp.value("ok").toBool());

        // Live service value applied (works without a PipeWire daemon)
        QCOMPARE(audio.inputDevice(), QString("alsa_input.usb-mic"));

        // Persisted on the CANONICAL key — reload from disk and check the
        // getter main.cpp uses at startup.
        YamlConfig reloaded;
        reloaded.load(yamlPath);
        QCOMPARE(reloaded.microphoneDevice(), QString("alsa_input.usb-mic"));
    }
};

QTEST_MAIN(TestIpcAudioConfig)
#include "test_ipc_audio_config.moc"
```

Register in `tests/CMakeLists.txt` next to `test_ipc_install_theme`:

```cmake
oap_add_test(test_ipc_audio_config SOURCES test_ipc_audio_config.cpp)
```

- [ ] **Step 2c: Run to verify it fails**

```bash
cd ~/builds/openauto-prodigy && cmake . && cmake --build . --target test_ipc_audio_config -j$(nproc) && ctest -R test_ipc_audio_config --output-on-failure
```

Expected: FAIL on the `reloaded.microphoneDevice()` compare — the current
IPC branch writes the schema-rejected `audio.input_device` key, so nothing
lands on disk and the getter returns the `"auto"` default.

- [ ] **Step 3: Fix the QML picker**

Replace the `FullScreenPicker { id: inputPicker ... }` block in
`AudioSettings.qml` with:

```qml
            FullScreenPicker {
                id: inputPicker
                flat: true
                label: "Input Device"
                model: typeof AudioInputDeviceModel !== "undefined" ? AudioInputDeviceModel : null
                textRole: "description"

                function syncSelection() {
                    if (typeof AudioInputDeviceModel === "undefined") return
                    var current = (typeof AudioService !== "undefined")
                        ? AudioService.inputDevice() : ""
                    if (!current)
                        current = ConfigService.value("audio.microphone.device")
                    if (!current) current = "auto"
                    var idx = AudioInputDeviceModel.indexOfDevice(current)
                    if (idx >= 0) currentIndex = idx
                }

                onActivated: function(index) {
                    if (typeof AudioInputDeviceModel === "undefined") return
                    var nodeName = AudioInputDeviceModel.data(
                        AudioInputDeviceModel.index(index, 0), Qt.UserRole + 1)
                    ConfigService.setValue("audio.microphone.device", nodeName)
                    ConfigService.save()
                    if (typeof AudioService !== "undefined")
                        AudioService.setInputDevice(nodeName)
                }

                Component.onCompleted: syncSelection()

                // Device enumeration resets the model after construction
                // (AudioDeviceModel::refresh) — re-apply the selection.
                Connections {
                    target: typeof AudioInputDeviceModel !== "undefined" ? AudioInputDeviceModel : null
                    function onCountChanged() { inputPicker.syncSelection() }
                }
            }
```

- [ ] **Step 4: Fix the IPC persist key**

`src/core/services/IpcServer.cpp` `handleSetAudioConfig`, change:

```cpp
        if (data.contains("input_device"))
            config_->setValueByPath("audio.input_device", data.value("input_device").toString());
```

to:

```cpp
        if (data.contains("input_device"))
            config_->setValueByPath("audio.microphone.device", data.value("input_device").toString());
```

(The JSON field name `input_device` is frozen; only the YAML key changes.)

- [ ] **Step 5: Fix the settings reference**

`docs/reference/settings-tree.md:72` — replace the row:

```
| Picker | Input Device | `audio.input_device` | PipeWire device list, restart required |
```

with:

```
| Picker | Input Device | `audio.microphone.device` | PipeWire device list; applies to capture streams created after the change |
```

- [ ] **Step 6: Build + test**

```bash
cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest -R "test_yaml_config|test_config_key_coverage|test_ipc_audio_config" --output-on-failure && cmake --build . --target openauto-prodigy -j$(nproc)
```

Expected: all PASS (the Step-2c red test goes green on the key fix), app
target builds (QML ships in the binary).

- [ ] **Step 7: Commit**

```bash
git add qml/applications/settings/AudioSettings.qml src/core/services/IpcServer.cpp docs/reference/settings-tree.md tests/test_yaml_config.cpp tests/test_ipc_audio_config.cpp tests/CMakeLists.txt
git commit -m "fix(audio): persist input-device selection on the canonical audio.microphone.device key"
```

---

### Task 5: AudioService rider — no emit under lock, emit only on change

**Tier:** opus

**Files:**
- Modify: `src/core/services/AudioService.cpp:488-511` (`setMasterVolume`)
- Test: `tests/test_audio_service.cpp` (extend)

**Interfaces:**
- Consumes: nothing new.
- Produces: `masterVolumeChanged` emitted exactly once per actual value
  change, never under `mutex_`. Task 6's debounce wiring relies on
  emit-only-on-change (boot-time re-apply of the same value must not
  schedule a save).

**Out of scope:** no `IAudioService` virtual-signature changes (plugin ABI);
no persistence wiring (that is Task 6); volume is still APPLIED to streams
even when unchanged (only the emit is gated).

- [ ] **Step 1: Write the failing test**

`tests/test_audio_service.cpp` includes only `<QTest>` — FIRST add:

```cpp
#include <QSignalSpy>
```

The file defines its slots INLINE in the class body (`private slots:` with
bodies) and uses `isAvailable()` + `QSKIP` as its PipeWire seam — follow both
conventions. Add these two slots inside the class:

```cpp
    void masterVolumeSignalOnlyOnChange()
    {
        oap::AudioService svc;
        QSignalSpy spy(&svc, &oap::AudioService::masterVolumeChanged);
        svc.setMasterVolume(50);
        QCOMPARE(spy.count(), 1);
        svc.setMasterVolume(50);           // same value — no signal
        QCOMPARE(spy.count(), 1);
        svc.setMasterVolume(200);          // clamps to 100 — one signal
        QCOMPARE(spy.count(), 2);
        svc.setMasterVolume(150);          // clamps to 100 again — no signal
        QCOMPARE(spy.count(), 2);
    }

    void masterVolumeGetterReentrantFromSignal()
    {
        // Regression: the no-PipeWire branch used to emit while holding
        // mutex_; a direct-connected slot reading masterVolume() deadlocked.
        // Only the !threadLoop_ branch had the bug — skip when a live
        // PipeWire daemon means that branch isn't the one under test
        // (file convention, and pre-fix this test would HANG there... on the
        // buggy branch; the skip keeps the red run bounded either way).
        oap::AudioService svc;
        if (svc.isAvailable())
            QSKIP("PipeWire daemon running — no-daemon branch not exercised");
        int seen = -1;
        QObject::connect(&svc, &oap::AudioService::masterVolumeChanged,
                         [&]() { seen = svc.masterVolume(); });
        svc.setMasterVolume(42);
        QCOMPARE(seen, 42);
    }
```

- [ ] **Step 2: Run to verify it fails (bounded — the deadlock case hangs)**

```bash
cd ~/builds/openauto-prodigy && cmake --build . --target test_audio_service -j$(nproc) && timeout 60 ctest -R test_audio_service --output-on-failure; echo "exit: $?"
```

Expected: `masterVolumeSignalOnlyOnChange` FAILS (current code emits
unconditionally — this fails in BOTH PipeWire environments).
`masterVolumeGetterReentrantFromSignal`: with no PipeWire daemon the run
deadlocks and `timeout` kills it (exit 124) — that IS the red confirmation;
with a daemon it SKIPs.

- [ ] **Step 3: Implement**

Replace `AudioService::setMasterVolume` with:

```cpp
void AudioService::setMasterVolume(int volume)
{
    const int clamped = qBound(0, volume, 100);
    bool changed = false;

    // Lock ordering: PW lock first, then mutex_ (same as destructor).
    // Emit AFTER all locks are released — QML slots read masterVolume(),
    // which takes mutex_ (non-recursive).
    if (!threadLoop_) {
        {
            QMutexLocker lock(&mutex_);
            changed = (masterVolume_ != clamped);
            masterVolume_ = clamped;
        }
        if (changed) emit masterVolumeChanged();
        return;
    }

    pw_thread_loop_lock(threadLoop_);
    {
        QMutexLocker lock(&mutex_);
        changed = (masterVolume_ != clamped);
        masterVolume_ = clamped;

        // Cubic curve for perceptual volume scaling
        float vol = cubicVolume(masterVolume_);

        for (auto* handle : streams_)
            applyVolumeToStream(handle, vol);
    }
    pw_thread_loop_unlock(threadLoop_);
    if (changed) emit masterVolumeChanged();
}
```

- [ ] **Step 4: Rebuild and run tests to verify they pass**

```bash
cd ~/builds/openauto-prodigy && cmake --build . --target test_audio_service -j$(nproc) && ctest -R test_audio_service --output-on-failure
```

Expected: PASS (both new tests, plus the existing suite for the file). No
`timeout` needed — the fix removes the hang.

- [ ] **Step 5: Commit**

```bash
git add src/core/services/AudioService.cpp tests/test_audio_service.cpp
git commit -m "fix(audio): emit masterVolumeChanged outside locks and only on change"
```

---

### Task 6: Master-volume persistence — one debounced writer with shutdown flush

**Tier:** opus  ·  **Depends on:** Task 4 (IPC block), Task 5 (emit-on-change)

**Files:**
- Modify: `src/main.cpp` (after the `setMasterVolume(yamlConfig->masterVolume())`
  initial-load line, `main.cpp:321`)
- Modify: `qml/applications/settings/AudioSettings.qml:23-32` (Master Volume
  slider)
- Modify: `qml/controls/SettingsSlider.qml` (expose `pressed`)
- Modify: `src/core/services/IpcServer.cpp` `handleSetAudioConfig`
- Test: none unit-testable (`main.cpp` wiring — ctest never compiles it);
  verification = app-target build + Ship Ceremony bench rows. The QML and IPC
  edits ride the same commit.

**Interfaces:**
- Consumes: `masterVolumeChanged` emit-only-on-change (Task 5);
  `YamlConfig::setMasterVolume(int)` + `save(path)` (returns bool);
  Task 4's IPC key change (this task rewrites the same block — final shape
  below INCLUDES Task 4's `audio.microphone.device` key).
- Produces: the single persist path for `audio.master_volume`.

**Out of scope:** no changes to `SettingsSlider`'s configPath behavior for
OTHER sliders (mic gain etc. keep using it); no mute-semantics change
(volume 0 persists — accepted); no `EqualizerService`-style service class
for volume (the main.cpp wiring IS the design).

- [ ] **Step 1: Wire the debounced persist in `main.cpp`**

Immediately AFTER `audioService->setMasterVolume(yamlConfig->masterVolume());`
(so the boot-time apply cannot schedule a save — and with Task 5's
emit-on-change it wouldn't anyway):

```cpp
    // Master-volume persistence: every runtime mutation (gesture, navbar,
    // mute toggle, IPC, settings slider) funnels through setMasterVolume;
    // flush debounced, mirror EqualizerService::kSaveDebounceMs. Timer-active
    // IS the dirty flag. Receiver-context connection keeps a future
    // worker-thread setMasterVolume caller off the timer's thread.
    {
        auto* volSaveTimer = new QTimer(&app);
        volSaveTimer->setSingleShot(true);
        volSaveTimer->setInterval(2000);
        auto flushVolume = [audioService, yc = yamlConfig.get(),
                            path = yamlPath, volSaveTimer]() {
            yc->setMasterVolume(audioService->masterVolume());
            if (!yc->save(path)) {
                qCWarning(lcCore) << "Master-volume flush failed; re-arming";
                volSaveTimer->start();
            }
        };
        QObject::connect(audioService, &oap::AudioService::masterVolumeChanged,
                         volSaveTimer, qOverload<>(&QTimer::start));
        QObject::connect(volSaveTimer, &QTimer::timeout,
                         volSaveTimer, flushVolume);
        QObject::connect(&app, &QGuiApplication::aboutToQuit, volSaveTimer,
                         [volSaveTimer, flushVolume]() {
                             if (volSaveTimer->isActive()) {
                                 volSaveTimer->stop();
                                 flushVolume();   // synchronous, last chance
                             }
                         });
    }
```

(Hard crash/power loss inside the 2 s window is the accepted debounce
tradeoff — per the design, documented, not defended.)

- [ ] **Step 2: Expose `pressed` on SettingsSlider**

In `qml/controls/SettingsSlider.qml`, next to the existing
`property alias value: slider.value`:

```qml
    readonly property alias pressed: slider.pressed
```

- [ ] **Step 3: Make the settings slider a pure view of the live value**

Replace the Master Volume `SettingsSlider` block in `AudioSettings.qml`
(REMOVING its `configPath` — `SettingsSlider`'s own debounced config write
was the second racing persist path):

```qml
            SettingsSlider {
                id: masterVolumeSlider
                label: "Master Volume"
                from: 0; to: 100; stepSize: 1
                onMoved: {
                    if (typeof AudioService !== "undefined")
                        AudioService.setMasterVolume(value)
                }
                Component.onCompleted: {
                    if (typeof AudioService !== "undefined")
                        value = AudioService.masterVolume
                }
                Connections {
                    target: typeof AudioService !== "undefined" ? AudioService : null
                    function onMasterVolumeChanged() {
                        if (!masterVolumeSlider.pressed)
                            masterVolumeSlider.value = AudioService.masterVolume
                    }
                }
            }
```

- [ ] **Step 4: Single-writer IPC block**

In `IpcServer.cpp` `handleSetAudioConfig`, replace the whole
`// Persist to config if available` block with (this is the FINAL shape —
it includes Task 4's key change):

```cpp
    // Persist device selections immediately. Master volume is deliberately
    // NOT written here — the centralized debounced path (main.cpp, driven by
    // masterVolumeChanged) is the single writer for audio.master_volume; an
    // unconditional save here would flush the stale in-memory value first.
    if (config_) {
        bool persisted = false;
        if (data.contains("output_device")) {
            config_->setValueByPath("audio.output_device",
                                    data.value("output_device").toString());
            persisted = true;
        }
        if (data.contains("input_device")) {
            config_->setValueByPath("audio.microphone.device",
                                    data.value("input_device").toString());
            persisted = true;
        }
        if (persisted)
            config_->save(configPath_);
    }
```

- [ ] **Step 5: Build everything**

```bash
cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest --output-on-failure && cmake --build . --target openauto-prodigy -j$(nproc)
```

Expected: suite green, app target builds.

- [ ] **Step 6: Commit**

```bash
git add src/main.cpp qml/applications/settings/AudioSettings.qml qml/controls/SettingsSlider.qml src/core/services/IpcServer.cpp
git commit -m "feat(audio): persist master volume — single debounced writer with shutdown flush"
```

---

### Task 7: Plugin ABI runtime validation — owning loader record + mismatch rejection

**Tier:** opus

**Files:**
- Modify: `src/core/plugin/PluginLoader.hpp`, `src/core/plugin/PluginLoader.cpp`
- Modify: `src/core/plugin/PluginManager.hpp` (PluginEntry),
  `src/core/plugin/PluginManager.cpp` (`discoverPlugins`, destructor)
- Create: `tests/fixtures/fixture_stale_plugin.cpp`,
  `tests/fixtures/fixture_imposter_plugin.cpp`,
  `tests/fixtures/fixture_valid_plugin.cpp`,
  `tests/data/fixture_stale.yaml`, `tests/data/fixture_imposter.yaml`,
  `tests/data/fixture_valid.yaml`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/test_plugin_manager.cpp` (extend; focused command
  `ctest -R test_plugin_manager --output-on-failure`)

**Interfaces:**
- Consumes: `IPlugin` (unchanged), `PluginDiscovery::HOST_API_VERSION` (== 2,
  `PluginDiscovery.hpp:17`), `OAP_PLUGIN_IID` (`IPlugin.hpp`), the loader
  naming rule `lib<last-id-segment>.so` (`PluginManager.cpp:58`).
- Produces: `PluginLoader::LoadResult { QPluginLoader* loader; IPlugin* plugin; }`
  — heap loader, ownership passes to `PluginEntry::loader`. `IPlugin`
  virtuals are UNTOUCHED (plugin ABI constraint).

**Out of scope:** NO `HOST_API_VERSION` bump and NO `IPlugin`/`IHostContext`
signature changes; no unload of ACCEPTED plugins at manager teardown (loader
objects deleted without unload — today's semantics); no manifest-schema
changes.

- [ ] **Step 1: Build the fixture plugins**

`tests/fixtures/fixture_stale_plugin.cpp` — lies about API version (binary
says 1, manifest will say 2):

```cpp
#include "core/plugin/IPlugin.hpp"
#include <QObject>

class FixtureStalePlugin : public QObject, public oap::IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OAP_PLUGIN_IID)
    Q_INTERFACES(oap::IPlugin)
public:
    QString id() const override { return "org.test.stale"; }
    QString name() const override { return "Stale Fixture"; }
    QString version() const override { return "1.0"; }
    int apiVersion() const override { return 1; }   // stale — host is 2
    bool initialize(oap::IHostContext*) override { return true; }
    void shutdown() override {}
    QUrl qmlComponent() const override { return {}; }
    QUrl iconSource() const override { return {}; }
    QStringList requiredServices() const override { return {}; }
};

#include "fixture_stale_plugin.moc"
```

`tests/fixtures/fixture_imposter_plugin.cpp` — right API version, wrong
identity (manifest will claim `org.test.other`):

```cpp
#include "core/plugin/IPlugin.hpp"
#include <QObject>

class FixtureImposterPlugin : public QObject, public oap::IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OAP_PLUGIN_IID)
    Q_INTERFACES(oap::IPlugin)
public:
    QString id() const override { return "org.test.imposter"; }
    QString name() const override { return "Imposter Fixture"; }
    QString version() const override { return "1.0"; }
    int apiVersion() const override { return 2; }
    bool initialize(oap::IHostContext*) override { return true; }
    void shutdown() override {}
    QUrl qmlComponent() const override { return {}; }
    QUrl iconSource() const override { return {}; }
    QStringList requiredServices() const override { return {}; }
};

#include "fixture_imposter_plugin.moc"
```

`tests/fixtures/fixture_valid_plugin.cpp` — everything matches (proves the
gate passes good plugins):

```cpp
#include "core/plugin/IPlugin.hpp"
#include <QObject>

class FixtureValidPlugin : public QObject, public oap::IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OAP_PLUGIN_IID)
    Q_INTERFACES(oap::IPlugin)
public:
    QString id() const override { return "org.test.valid"; }
    QString name() const override { return "Valid Fixture"; }
    QString version() const override { return "1.0"; }
    int apiVersion() const override { return 2; }
    bool initialize(oap::IHostContext*) override { return true; }
    void shutdown() override {}
    QUrl qmlComponent() const override { return {}; }
    QUrl iconSource() const override { return {}; }
    QStringList requiredServices() const override { return {}; }
};

#include "fixture_valid_plugin.moc"
```

Manifests (`api_version: 2` so ALL pass discovery — the runtime gate is what
each case exercises). `tests/data/fixture_stale.yaml`:

```yaml
id: org.test.stale
name: "Stale Fixture"
version: "1.0"
api_version: 2
type: full
```

`tests/data/fixture_imposter.yaml` (id deliberately ≠ binary id; the `.so`
basename must match THIS id per the loader naming rule):

```yaml
id: org.test.other
name: "Imposter Fixture"
version: "1.0"
api_version: 2
type: full
```

`tests/data/fixture_valid.yaml`:

```yaml
id: org.test.valid
name: "Valid Fixture"
version: "1.0"
api_version: 2
type: full
```

- [ ] **Step 2: CMake — fixture targets + test defines**

In `tests/CMakeLists.txt`, after the `oap_add_test` helper:

```cmake
# --- Fixture plugins: real .so files for dynamic-load validation tests ---
# Layout matches PluginManager's naming rule: <dir>/plugin.yaml +
# lib<last-id-segment>.so
function(oap_add_fixture_plugin TARGET SRC OUT_NAME DIRNAME MANIFEST)
    add_library(${TARGET} MODULE fixtures/${SRC})
    target_link_libraries(${TARGET} PRIVATE openauto-core)
    set_target_properties(${TARGET} PROPERTIES
        OUTPUT_NAME ${OUT_NAME}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fixture_plugins/${DIRNAME})
    configure_file(data/${MANIFEST}
        ${CMAKE_CURRENT_BINARY_DIR}/fixture_plugins/${DIRNAME}/plugin.yaml COPYONLY)
endfunction()

oap_add_fixture_plugin(fixture_stale_plugin    fixture_stale_plugin.cpp    stale    stale    fixture_stale.yaml)
oap_add_fixture_plugin(fixture_imposter_plugin fixture_imposter_plugin.cpp other    imposter fixture_imposter.yaml)
oap_add_fixture_plugin(fixture_valid_plugin    fixture_valid_plugin.cpp    valid    valid    fixture_valid.yaml)
```

And change the existing `test_plugin_manager` registration to:

```cmake
oap_add_test(test_plugin_manager SOURCES test_plugin_manager.cpp
    DEFS FIXTURE_PLUGINS_DIR="${CMAKE_CURRENT_BINARY_DIR}/fixture_plugins")
add_dependencies(test_plugin_manager
    fixture_stale_plugin fixture_imposter_plugin fixture_valid_plugin)
```

- [ ] **Step 3: Write the failing tests**

`tests/test_plugin_manager.cpp` declares slots in the class and DEFINES them
later, before `QTEST_MAIN` (`tests/test_plugin_manager.cpp:52` area) — follow
that split or the functions won't be test slots. Also add
`#include <QSignalSpy>` if the file lacks it. Declarations, inside the test
class's `private slots:`:

```cpp
    void testDynamicLoadRejectsStaleBinary();
    void testDynamicLoadRejectsIdMismatch();
    void testDynamicLoadAcceptsValidBinary();
```

Definitions, with the other definitions BEFORE `QTEST_MAIN` (helper first —
the design's contract is EXACTLY ONE `pluginFailed` per rejected plugin):

```cpp
static int failuresFor(const QSignalSpy& spy, const QString& id)
{
    int n = 0;
    for (const auto& args : spy)
        if (args.at(0).toString() == id) ++n;
    return n;
}

void TestPluginManager::testDynamicLoadRejectsStaleBinary()
{
    oap::PluginManager mgr;
    QSignalSpy failedSpy(&mgr, &oap::PluginManager::pluginFailed);
    mgr.discoverPlugins(QStringLiteral(FIXTURE_PLUGINS_DIR));

    // stale: manifest v2 passes discovery, binary reports apiVersion 1.
    QVERIFY(mgr.plugin("org.test.stale") == nullptr);
    QCOMPARE(failuresFor(failedSpy, "org.test.stale"), 1);
}

void TestPluginManager::testDynamicLoadRejectsIdMismatch()
{
    oap::PluginManager mgr;
    QSignalSpy failedSpy(&mgr, &oap::PluginManager::pluginFailed);
    mgr.discoverPlugins(QStringLiteral(FIXTURE_PLUGINS_DIR));

    // imposter: manifest id org.test.other, binary id org.test.imposter.
    QVERIFY(mgr.plugin("org.test.other") == nullptr);
    QVERIFY(mgr.plugin("org.test.imposter") == nullptr);
    QCOMPARE(failuresFor(failedSpy, "org.test.other"), 1);
}

void TestPluginManager::testDynamicLoadAcceptsValidBinary()
{
    oap::PluginManager mgr;
    QSignalSpy failedSpy(&mgr, &oap::PluginManager::pluginFailed);
    mgr.discoverPlugins(QStringLiteral(FIXTURE_PLUGINS_DIR));
    QVERIFY(mgr.plugin("org.test.valid") != nullptr);
    QCOMPARE(failuresFor(failedSpy, "org.test.valid"), 0);
    MockHostContext ctx;
    mgr.initializeAll(&ctx);
    QCOMPARE(mgr.plugin("org.test.valid")->apiVersion(), 2);
}
```

(If the test class has a different name than `TestPluginManager`, use the
file's actual class name.)

- [ ] **Step 4: Run to verify the reject cases fail**

```bash
cd ~/builds/openauto-prodigy && cmake . && cmake --build . --target test_plugin_manager fixture_stale_plugin fixture_imposter_plugin fixture_valid_plugin -j$(nproc) && ctest -R test_plugin_manager --output-on-failure
```

Expected: `testDynamicLoadRejectsStaleBinary` and
`testDynamicLoadRejectsIdMismatch` FAIL (today the stale/imposter plugins
load and register); `testDynamicLoadAcceptsValidBinary` PASSES.

- [ ] **Step 5: Implement the owning record + validation**

`PluginLoader.hpp`:

```cpp
#pragma once

#include <QString>

class QPluginLoader;

namespace oap {

class IPlugin;

/// Thin wrapper around QPluginLoader for loading dynamic plugin .so files.
class PluginLoader {
public:
    /// Result of a dynamic load. `loader` is heap-allocated and OWNED BY THE
    /// CALLER (PluginManager stores it in the PluginEntry); it is the handle
    /// for unload-on-rejection. Both members null on failure.
    struct LoadResult {
        QPluginLoader* loader = nullptr;
        IPlugin* plugin = nullptr;
        bool ok() const { return loader && plugin; }
    };

    static LoadResult load(const QString& soPath);
};

} // namespace oap
```

`PluginLoader.cpp`:

```cpp
#include "PluginLoader.hpp"
#include "IPlugin.hpp"
#include <QLibrary>
#include <QPluginLoader>
#include "../Logging.hpp"

namespace oap {

PluginLoader::LoadResult PluginLoader::load(const QString& soPath)
{
    auto* loader = new QPluginLoader(soPath);
    // Qt 6 sets QLibrary::PreventUnloadHint by DEFAULT on plugin loads —
    // clear it BEFORE instance() so a rejected binary can actually be
    // unloaded from the address space (the whole point of the ABI gate).
    loader->setLoadHints(loader->loadHints() & ~QLibrary::PreventUnloadHint);
    QObject* instance = loader->instance();
    if (!instance) {
        qCCritical(lcPlugin) << "Failed to load plugin: " << soPath
                             << " — " << loader->errorString();
        delete loader;
        return {};
    }

    auto* plugin = qobject_cast<IPlugin*>(instance);
    if (!plugin) {
        qCCritical(lcPlugin) << "Loaded object from " << soPath
                             << " does not implement IPlugin";
        if (!loader->unload())
            qCWarning(lcPlugin) << "unload failed for" << soPath;
        delete loader;
        return {};
    }

    return {loader, plugin};
}

} // namespace oap
```

`PluginManager.hpp` — extend the entry (plugin ABI untouched; this is
manager-internal):

```cpp
    struct PluginEntry {
        IPlugin* plugin = nullptr;
        QPluginLoader* loader = nullptr;  // dynamic plugins only; owned
        PluginManifest manifest;
        bool isStatic = false;
        bool initialized = false;
    };
```

(add `class QPluginLoader;` forward declaration above the namespace.)

`PluginManager.cpp` — FIRST add the include (the header only
forward-declares `QPluginLoader`; `unload()`/`delete` need the complete
type — do NOT rely on transitive includes):

```cpp
#include <QPluginLoader>
```

Then in `discoverPlugins`, replace the load block:

```cpp
        // Try to load the .so
        QString soPath = manifest.dirPath + "/lib" + manifest.id.split('.').last() + ".so";
        auto result = PluginLoader::load(soPath);
        if (!result.ok()) {
            emit pluginFailed(manifest.id, "Failed to load shared library");
            continue;
        }

        // Runtime ABI gate: the MANIFEST passed discovery, but the BINARY is
        // what dispatches against IHostContext's vtable. Trust the binary.
        QString mismatch;
        if (result.plugin->apiVersion() != PluginDiscovery::HOST_API_VERSION) {
            mismatch = QStringLiteral("binary API v%1 != host v%2")
                           .arg(result.plugin->apiVersion())
                           .arg(PluginDiscovery::HOST_API_VERSION);
        } else if (result.plugin->id() != manifest.id) {
            mismatch = QStringLiteral("binary id '%1' != manifest id '%2'")
                           .arg(result.plugin->id(), manifest.id);
        }
        if (!mismatch.isEmpty()) {
            qCCritical(lcPlugin) << "Rejecting plugin " << manifest.id
                                 << ": " << mismatch;
            if (!result.loader->unload())
                qCWarning(lcPlugin) << "unload failed for rejected plugin "
                                    << manifest.id;
            delete result.loader;
            emit pluginFailed(manifest.id, mismatch);
            continue;
        }

        PluginEntry entry;
        entry.plugin = result.plugin;
        entry.loader = result.loader;
        entry.manifest = manifest;
        entry.isStatic = false;
```

And in the destructor (after `shutdownAll()`), release the loader objects
WITHOUT unloading (unloading live plugins at teardown risks destroying code
still referenced; QPluginLoader's destructor does not unload):

```cpp
PluginManager::~PluginManager()
{
    shutdownAll();
    for (auto& entry : entries_)
        delete entry.loader;   // does NOT unload; loader object cleanup only
}
```

(`#include <QPluginLoader>` and `#include "PluginDiscovery.hpp"` are already
reachable — verify includes compile.)

- [ ] **Step 6: Run tests to verify all pass**

```bash
cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest -R "test_plugin_manager|test_plugin_discovery|test_plugin_model" --output-on-failure
```

Expected: PASS (all three reject/accept cases + no regressions in the
neighboring plugin tests).

- [ ] **Step 7: Commit**

```bash
git add src/core/plugin/PluginLoader.hpp src/core/plugin/PluginLoader.cpp src/core/plugin/PluginManager.hpp src/core/plugin/PluginManager.cpp tests/fixtures/ tests/data/fixture_*.yaml tests/CMakeLists.txt tests/test_plugin_manager.cpp
git commit -m "fix(plugin): validate dynamic plugin binary API version and ID before initialize"
```

---

### Task 8: Roadmap + stale-reference sweep

**Tier:** sonnet

**Files:**
- Modify: `docs/roadmap-current.md`

**Interfaces:** none — docs only.

**Out of scope:** no edits under `docs/archive/` (history, never "fixed");
no wishlist changes (those flip at ship); no INDEX.md restructuring beyond
what stale refs require.

- [ ] **Step 1: Move shipped BT-EQ work out of "Now"**

In `docs/roadmap-current.md`: move the "BT A2DP through the equalizer + EQ
hygiene" block (`:58` area) from "Now" into "Done" with a
`SHIPPED 2026-07-15 (ALPHA-26-07-15-01)` note, repointing its design/plan
refs to `docs/archive/plans/2026-07-14-bt-a2dp-eq-{design,plan}.md`.

- [ ] **Step 2: Add this batch under "Now"**

```markdown
- Bench-findings batch — **PROMOTED 2026-07-15** (items 1-3 by Matthew from
  the BT-EQ bench findings; items 4-6 from the Codex post-merge review of
  PR #20, folded in same day). Design:
  `docs/plans/2026-07-15-bench-findings-batch-design.md` (twice
  Codex-reviewed), plan: `docs/plans/2026-07-15-bench-findings-batch-plan.md`.
  Headline: HFP-SCO/EQ-tap routing collision (live call-audio regression),
  ExecStopPost phone-kick removal, input-device + master-volume persistence,
  plugin ABI runtime validation, doc-reference hygiene.
```

- [ ] **Step 3: Sweep every `docs/plans/` reference in the file**

The file contains brace-expanded shorthand
(e.g. `...-{design,plan}.md` at `docs/roadmap-current.md:70`) — FIRST rewrite
any braced reference into explicit full paths, THEN validate every match on
every line (no `head -1`):

```bash
grep -oE "docs/plans/[A-Za-z0-9._{},-]+\.md" docs/roadmap-current.md | sort -u | while read -r f; do
  case "$f" in
    *"{"*) echo "BRACED (expand by hand first): $f" ;;
    *) [ ! -f "$f" ] && echo "STALE: $f" ;;
  esac
done
```

For every STALE hit, repoint to the file's actual location (usually
`docs/archive/plans/<same name>`); verify each replacement target exists
(`ls` it). Re-run the loop until it prints nothing.

- [ ] **Step 4: Commit**

```bash
git add docs/roadmap-current.md
git commit -m "docs(roadmap): BT-EQ shipped, bench-findings batch promoted, stale plan refs repointed"
```

---

## Ship Ceremony (after all tasks; main session, NOT a dispatched task)

1. **Full verification:** suite + app target (Global Constraints commands).
2. **Codex gate:** `bash scripts/codex-review.sh` on `@{upstream}..HEAD`;
   adjudicate EVERY finding (fix or dismiss-with-reason, recorded in the
   handoff); substantial fixes → one gate re-run. No push without go.
3. **Deploy to Pi** (order matters):
   - `./cross-build.sh` → rsync binary per AGENTS.md.
   - Live unit edit (Task 3's counterpart):
     `ssh matt@192.168.1.149 'sudo sed -i "/bluetoothctl disconnect/d" /etc/systemd/system/openauto-prodigy.service && sudo systemctl daemon-reload'`
   - Stage A: conf already deployed in Task 2A. Stage B: remove the drop-in
     per Task 2B contract point 4.
   - Restart order: `bluetooth` → `pipewire wireplumber` → `openauto-prodigy`.
     (This deploy restart may still kick the phone — the NEW unit takes
     effect for the NEXT stop.)
4. **Bench rows** (design § acceptance, all must pass):
   - **Calls (Matthew + phone):** from active A2DP playback → incoming AND
     outgoing call → far-end voice audible + direct-routed, rocker/VGS
     volume works, uplink works → hang up → music resumes THROUGH the tap
     (audible EQ + HU volume) with no app restart.
   - **ExecStopPost:** clean `systemctl restart openauto-prodigy` during BT
     streaming → phone STAYS connected, music continues via fallback.
   - **Input device:** select mic → `audio: microphone: device:` in
     config.yaml → app restart → picker + `AudioService.inputDevice()` match.
     (NO AA-assistant row — impossible until the wishlisted AVInput work.)
   - **Master volume:** gesture change → >2 s → config.yaml updated → restart
     → restored. Gesture change → clean restart WITHIN 2 s → new value
     survives (aboutToQuit flush). Rapid drag → ONE write (watch mtime).
   - **Mute-persist semantics:** mute (long-hold) → restart → boots at 0,
     silent, fades in on raise — accepted behavior, just confirm no crash.
   - Opportunistic: remove the stale one-way S25 bond from the HU.
5. **Docs:** wishlist flips (ExecStopPost, input-device, master-volume,
   + sweep-item absorption if Stage B ran), this plan + design →
   COMPLETED + `docs/archive/plans/` (same commit), session-handoffs entry
   (adjudication counts included). **In that same archival commit:** move
   Task 8's "Bench-findings batch" roadmap entry from "Now" to "Done" and
   repoint its design+plan refs to `docs/archive/plans/` — otherwise the
   archival itself recreates the exact staleness Task 8 fixed.
6. **Push on Matthew's go. Tag ONLY on Matthew's declaration**
   (`bash scripts/tag-alpha.sh` → reconfigure → cross-build → package →
   `gh release create --prerelease`).
