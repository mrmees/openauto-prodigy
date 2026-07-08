# Widevine Enablement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire the RPi OS Widevine CDM into QtWebEngine so DRM (EME) content can play, guarantee the CDM package at install time, prove it end-to-end on the Pi with a probe tool, and codify the native/web boundary rule in docs.

**Architecture:** A small pure helper (`oap::` free functions) resolves the CDM path and builds the `QTWEBENGINE_CHROMIUM_FLAGS` value; `main.cpp` applies it via `qputenv` before `QtWebEngineQuick::initialize()`, respecting operator overrides. A standalone `eme-probe` tool (WebEngineView + qrc-served test page) verifies CDM loading and codec support on the Pi. Spec: `docs/superpowers/specs/2026-07-07-web-surface-strategy-design.md` (Slice 1).

**Tech Stack:** Qt 6.8 (WebEngineQuick), QTest, CMake, bash (install.sh).

## Global Constraints

- Widevine is an **enhancement, not a dependency**: every change must be a no-op on systems without the CDM or without WebEngine (`HAS_WEBENGINE` builds fine absent).
- Desktop Chromium is **left untouched** (spec Decision 2 — Matthew's call 2026-07-07). No purge, no apt-mark of chromium packages.
- Respect operator overrides: if `QTWEBENGINE_CHROMIUM_FLAGS` already mentions `widevine-path`, do not modify it.
- Repo conventions: `namespace oap`, QTest classes with `QTEST_GUILESS_MAIN` + `#include "<file>.moc"`, tests registered via `oap_add_test` in `tests/CMakeLists.txt`, all work committed on `develop`.
- Do not touch `libs/prodigy-oaa-protocol/` (submodule, hands-off).

---

### Task 1: WidevineCdm helper (pure functions, TDD)

**Files:**
- Create: `src/core/WidevineCdm.hpp`
- Create: `src/core/WidevineCdm.cpp`
- Create: `tests/test_widevine_cdm.cpp`
- Modify: `src/CMakeLists.txt` (sources list near line 4, `core/YamlConfig.cpp`)
- Modify: `tests/CMakeLists.txt` (Core tests block, after `test_hostapd_config` line 23)

**Interfaces:**
- Consumes: nothing (Qt6 Core only).
- Produces (used by Tasks 2 and 4):
  - `QStringList oap::widevineCdmCandidates()` — known CDM locations, priority order
  - `QString oap::resolveWidevineCdmPath(const QStringList& candidates)` — first existing path or empty
  - `QByteArray oap::appendWidevineFlag(const QByteArray& existingFlags, const QString& cdmPath)` — new flags value; unchanged if `cdmPath` empty or flags already contain `widevine-path`

- [ ] **Step 1: Write the failing test**

Create `tests/test_widevine_cdm.cpp`:

```cpp
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "core/WidevineCdm.hpp"

class TestWidevineCdm : public QObject {
    Q_OBJECT

private slots:
    void resolveReturnsFirstExistingCandidate()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString missing = dir.filePath("nope/libwidevinecdm.so");
        const QString present = dir.filePath("libwidevinecdm.so");
        QFile f(present);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
        f.close();

        QCOMPARE(oap::resolveWidevineCdmPath({missing, present}), present);
    }

    void resolveReturnsEmptyWhenNothingExists()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QCOMPARE(oap::resolveWidevineCdmPath({dir.filePath("a.so"), dir.filePath("b.so")}),
                 QString());
    }

    void candidatesListRpiPathsInPriorityOrder()
    {
        const QStringList c = oap::widevineCdmCandidates();
        QCOMPARE(c.size(), 2);
        QCOMPARE(c.at(0),
                 QString("/opt/WidevineCdm/gmp-widevinecdm/latest/libwidevinecdm.so"));
        QCOMPARE(c.at(1),
                 QString("/opt/WidevineCdm/_platform_specific/linux_arm64/libwidevinecdm.so"));
    }

    void appendOnEmptyFlagsProducesBareSwitch()
    {
        QCOMPARE(oap::appendWidevineFlag(QByteArray(), "/opt/cdm.so"),
                 QByteArray("--widevine-path=/opt/cdm.so"));
    }

    void appendOnExistingFlagsSeparatesWithSpace()
    {
        QCOMPARE(oap::appendWidevineFlag("--disable-gpu", "/opt/cdm.so"),
                 QByteArray("--disable-gpu --widevine-path=/opt/cdm.so"));
    }

    void appendWithEmptyCdmPathIsUnchanged()
    {
        QCOMPARE(oap::appendWidevineFlag("--disable-gpu", QString()),
                 QByteArray("--disable-gpu"));
    }

    void appendRespectsOperatorOverride()
    {
        const QByteArray flags = "--widevine-path=/custom/cdm.so";
        QCOMPARE(oap::appendWidevineFlag(flags, "/opt/other.so"), flags);
    }
};

QTEST_GUILESS_MAIN(TestWidevineCdm)

#include "test_widevine_cdm.moc"
```

Register it in `tests/CMakeLists.txt` after the `test_hostapd_config` line:

```cmake
oap_add_test(test_widevine_cdm SOURCES test_widevine_cdm.cpp)
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake -S . -B build >/dev/null && cmake --build build --target test_widevine_cdm -j$(nproc)`
Expected: FAIL to compile — `core/WidevineCdm.hpp: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

Create `src/core/WidevineCdm.hpp`:

```cpp
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace oap {

// Known system Widevine CDM locations (RPi OS libwidevinecdm0 layouts),
// priority order. Spec: 2026-07-07-web-surface-strategy §Slice 1.2.
QStringList widevineCdmCandidates();

// First candidate that exists on disk, or an empty string.
QString resolveWidevineCdmPath(const QStringList& candidates);

// Value QTWEBENGINE_CHROMIUM_FLAGS should take so Chromium loads the CDM.
// Returns existingFlags unchanged when cdmPath is empty or the flags
// already mention widevine-path (operator override wins).
QByteArray appendWidevineFlag(const QByteArray& existingFlags, const QString& cdmPath);

} // namespace oap
```

Create `src/core/WidevineCdm.cpp`:

```cpp
#include "WidevineCdm.hpp"

#include <QFileInfo>

namespace oap {

QStringList widevineCdmCandidates()
{
    return {
        QStringLiteral("/opt/WidevineCdm/gmp-widevinecdm/latest/libwidevinecdm.so"),
        QStringLiteral("/opt/WidevineCdm/_platform_specific/linux_arm64/libwidevinecdm.so"),
    };
}

QString resolveWidevineCdmPath(const QStringList& candidates)
{
    for (const QString& path : candidates) {
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}

QByteArray appendWidevineFlag(const QByteArray& existingFlags, const QString& cdmPath)
{
    if (cdmPath.isEmpty() || existingFlags.contains("widevine-path"))
        return existingFlags;

    QByteArray flag = "--widevine-path=" + cdmPath.toUtf8();
    if (existingFlags.isEmpty())
        return flag;
    return existingFlags + ' ' + flag;
}

} // namespace oap
```

Add to the `openauto-core` sources list in `src/CMakeLists.txt` (alongside `core/YamlConfig.cpp` near line 4):

```cmake
    core/WidevineCdm.cpp
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_widevine_cdm -j$(nproc) && ctest --test-dir build -R test_widevine_cdm --output-on-failure`
Expected: `100% tests passed, 0 tests failed out of 1`

- [ ] **Step 5: Commit**

```bash
git add src/core/WidevineCdm.hpp src/core/WidevineCdm.cpp src/CMakeLists.txt \
        tests/test_widevine_cdm.cpp tests/CMakeLists.txt
git commit -m "feat(core): widevine CDM path resolution + chromium flag helper"
```

---

### Task 2: Wire the CDM into app startup

**Files:**
- Modify: `src/main.cpp` (the `#ifdef HAS_WEBENGINE` block at lines 161–177)

**Interfaces:**
- Consumes: `oap::widevineCdmCandidates()`, `oap::resolveWidevineCdmPath()`, `oap::appendWidevineFlag()` from Task 1.
- Produces: nothing consumed by later tasks (behavioral change only).

- [ ] **Step 1: Add the include**

In `src/main.cpp`, with the other core includes near the top of the file, add:

```cpp
#include "core/WidevineCdm.hpp"
```

- [ ] **Step 2: Apply the flag before WebEngine init**

In the existing `#ifdef HAS_WEBENGINE` block in `main()`, insert between the scheme-registration brace and `QtWebEngineQuick::initialize();` (currently line 176):

```cpp
    // Widevine CDM auto-wiring (spec 2026-07-07-web-surface-strategy §Slice 1):
    // point Chromium at the system CDM so DRM (EME) content can play. Must
    // happen before initialize(); an operator-supplied widevine-path in
    // QTWEBENGINE_CHROMIUM_FLAGS wins.
    {
        const QString cdm = oap::resolveWidevineCdmPath(oap::widevineCdmCandidates());
        const QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
        const QByteArray updated = oap::appendWidevineFlag(flags, cdm);
        if (updated != flags) {
            qputenv("QTWEBENGINE_CHROMIUM_FLAGS", updated);
            qCInfo(lcCore) << "Widevine CDM wired:" << cdm;
        } else if (cdm.isEmpty()) {
            qCInfo(lcCore) << "No Widevine CDM found — DRM content unavailable";
        } else {
            qCInfo(lcCore) << "Widevine flags preset by environment — leaving untouched";
        }
    }
```

- [ ] **Step 3: Build and run the full suite**

Run: `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure`
Expected: build succeeds; all tests pass (89 with the new one from Task 1).

- [ ] **Step 4: Smoke-run locally**

Run: `./build/src/openauto-prodigy --geometry 1024x600 2>&1 | grep -i widevine` (Ctrl-C / kill after the line appears; WSL has no `/opt/WidevineCdm`)
Expected: `No Widevine CDM found — DRM content unavailable`

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp
git commit -m "feat(app): auto-wire widevine CDM into QTWEBENGINE_CHROMIUM_FLAGS at startup"
```

---

### Task 3: Installer guarantees the CDM package

**Files:**
- Modify: `install.sh` (`install_dependencies()`, after the `run_with_spinner "Installing ..."` line ~853)

**Interfaces:**
- Consumes: installer helpers `info()`, `warn()`, `run_with_spinner()` (already defined in `install.sh`).
- Produces: nothing consumed by later tasks.

- [ ] **Step 1: Add the best-effort install block**

In `install_dependencies()`, immediately after the existing
`run_with_spinner "Installing ${#PACKAGES[@]} packages" sudo apt-get install -y -q "${PACKAGES[@]}"`
line and before `update_step 1 done`, insert:

```bash
    # Widevine CDM — enables DRM (EME) playback in the web runtime (spec
    # 2026-07-07-web-surface-strategy §Slice 1.1). Present in RPi OS repos,
    # absent on plain Debian: best-effort, never a hard dependency. Desktop
    # Chromium (if installed) is deliberately left untouched.
    if apt-cache show libwidevinecdm0 >/dev/null 2>&1; then
        run_with_spinner "Installing Widevine CDM (libwidevinecdm0)" \
            sudo apt-get install -y -q libwidevinecdm0
    else
        warn "libwidevinecdm0 not found in APT repos — DRM (Widevine) web content will not play"
    fi
```

- [ ] **Step 2: Syntax-check the script**

Run: `bash -n install.sh && echo OK`
Expected: `OK`

- [ ] **Step 3: Commit**

```bash
git add install.sh
git commit -m "feat(install): ensure libwidevinecdm0 (best-effort) for DRM web content"
```

---

### Task 4: eme-probe verification tool

**Files:**
- Create: `tools/eme-probe/CMakeLists.txt`
- Create: `tools/eme-probe/main.cpp`
- Create: `tools/eme-probe/probe.qml`
- Create: `tools/eme-probe/probe.html`
- Create: `tools/eme-probe/probe.qrc`
- Modify: root `CMakeLists.txt` (after `add_subdirectory(src)`, line 48)

**Interfaces:**
- Consumes: `oap::widevineCdmCandidates()`, `oap::resolveWidevineCdmPath()`, `oap::appendWidevineFlag()` (compiled in directly — the probe does NOT link openauto-core).
- Produces: `eme-probe` binary (EXCLUDE_FROM_ALL; built on demand), used by Task 5.

- [ ] **Step 1: Create the CMake target**

`tools/eme-probe/CMakeLists.txt`:

```cmake
# EME/Widevine probe — manual verification tool for DRM playback
# (spec 2026-07-07-web-surface-strategy §Slice 1.3). Not part of the
# default build: cmake --build . --target eme-probe
if(TARGET Qt6::WebEngineQuick)
    add_executable(eme-probe EXCLUDE_FROM_ALL
        main.cpp
        probe.qrc
        ${CMAKE_SOURCE_DIR}/src/core/WidevineCdm.cpp
    )
    target_include_directories(eme-probe PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(eme-probe PRIVATE Qt6::Gui Qt6::Qml Qt6::WebEngineQuick)
endif()
```

In the root `CMakeLists.txt`, directly after `add_subdirectory(src)`:

```cmake
add_subdirectory(tools/eme-probe)
```

- [ ] **Step 2: Write the probe entry point**

`tools/eme-probe/main.cpp`:

```cpp
// EME/Widevine probe: loads a qrc-served test page (secure context) that
// reports codec support and com.widevine.alpha availability to the console.
// Pass a URL argument for interactive testing (Shaka demo, Spotify, ...).
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include "core/WidevineCdm.hpp"

int main(int argc, char *argv[])
{
    const QString cdm = oap::resolveWidevineCdmPath(oap::widevineCdmCandidates());
    const QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", oap::appendWidevineFlag(flags, cdm));
    qInfo() << "eme-probe: widevine cdm =" << (cdm.isEmpty() ? QStringLiteral("NOT FOUND") : cdm);

    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);

    const QString target = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                    : QStringLiteral("qrc:/probe.html");

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("targetUrl"), target}});
    engine.load(QUrl(QStringLiteral("qrc:/probe.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
```

- [ ] **Step 3: Write the QML host and test page**

`tools/eme-probe/probe.qml`:

```qml
import QtQuick
import QtWebEngine

Window {
    id: win
    required property string targetUrl
    width: 1024
    height: 600
    visible: true
    title: "EME probe"

    WebEngineView {
        anchors.fill: parent
        url: win.targetUrl
        onJavaScriptConsoleMessage: function (level, message, lineNumber, sourceId) {
            console.log("[page]", message)
        }
    }
}
```

`tools/eme-probe/probe.html`:

```html
<!doctype html>
<html>
<body style="background:#111;color:#eee;font-family:sans-serif">
<h2>Prodigy EME probe</h2>
<pre id="out"></pre>
<script>
function log(m) {
    document.getElementById('out').textContent += m + '\n';
    console.log(m);
}
log('EME-PROBE: secureContext=' + window.isSecureContext);
const v = document.createElement('video');
log('EME-PROBE: h264=' + (v.canPlayType('video/mp4; codecs="avc1.42E01E"') || 'no'));
log('EME-PROBE: aac=' + (v.canPlayType('audio/mp4; codecs="mp4a.40.2"') || 'no'));
log('EME-PROBE: vp9=' + (v.canPlayType('video/webm; codecs="vp9"') || 'no'));

// Two attempts: mp4 needs proprietary codecs; webm isolates "CDM loads at
// all" from "proprietary codecs missing".
const attempts = [
    ['mp4', {
        initDataTypes: ['cenc'],
        audioCapabilities: [{contentType: 'audio/mp4; codecs="mp4a.40.2"'}],
        videoCapabilities: [{contentType: 'video/mp4; codecs="avc1.42E01E"'}]
    }],
    ['webm', {
        initDataTypes: ['cenc'],
        audioCapabilities: [{contentType: 'audio/webm; codecs="opus"'}],
        videoCapabilities: [{contentType: 'video/webm; codecs="vp9"'}]
    }]
];
for (const [label, config] of attempts) {
    navigator.requestMediaKeySystemAccess('com.widevine.alpha', [config]).then(
        () => log('EME-PROBE: WIDEVINE SUPPORTED (' + label + ')'),
        (e) => log('EME-PROBE: WIDEVINE UNAVAILABLE (' + label + ') — ' + e.name + ': ' + e.message)
    );
}
</script>
</body>
</html>
```

`tools/eme-probe/probe.qrc`:

```xml
<RCC>
    <qresource prefix="/">
        <file>probe.qml</file>
        <file>probe.html</file>
    </qresource>
</RCC>
```

- [ ] **Step 4: Build it locally**

Run: `cmake -S . -B build >/dev/null && cmake --build build --target eme-probe -j$(nproc)`
Expected: `eme-probe` builds without errors (dev box has qt6-webengine-dev). Also confirm the default build still skips it: `cmake --build build -j$(nproc) 2>&1 | grep -c eme-probe` → `0`.

- [ ] **Step 5: Commit**

```bash
git add tools/eme-probe/ CMakeLists.txt
git commit -m "feat(tools): eme-probe — widevine/codec verification harness"
```

---

### Task 5: Pi deployment + on-device verification

**Files:** none (deployment/verification only; Pi at `matt@192.168.1.149`).

**Interfaces:**
- Consumes: everything from Tasks 1–4, deployed.
- Produces: verified facts recorded in Task 6's roadmap entry (CDM loads: yes/no; h264/aac: yes/no).

- [ ] **Step 1: Push and cross-build**

```bash
git push
./cross-build.sh
```
Expected: push succeeds; cross-build completes in ~4–6 min producing `build-pi/src/openauto-prodigy`.

- [ ] **Step 2: Deploy binary + sources, restart**

```bash
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.149:~/openauto-prodigy/build/src/
ssh matt@192.168.1.149 'cd ~/openauto-prodigy && git pull && sudo systemctl restart openauto-prodigy.service'
```
Expected: rsync transfers one binary; pull fast-forwards; service restarts.

- [ ] **Step 3: Confirm app-side wiring in the journal**

Run: `ssh matt@192.168.1.149 'journalctl -u openauto-prodigy.service -b --since "2 min ago" | grep -i widevine'`
Expected: `Widevine CDM wired: "/opt/WidevineCdm/gmp-widevinecdm/latest/libwidevinecdm.so"`

- [ ] **Step 4: Build the probe natively on the Pi**

Run: `ssh matt@192.168.1.149 'cd ~/openauto-prodigy/build && cmake .. >/dev/null && cmake --build . --target eme-probe -j3'`
Expected: `eme-probe` builds (a few minutes; single small target).

- [ ] **Step 5: Run the probe headed on the Pi**

```bash
ssh matt@192.168.1.149 'cd ~/openauto-prodigy/build && \
  timeout 30 env WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 QT_QPA_PLATFORM=wayland \
  ./tools/eme-probe/eme-probe 2>&1 | grep -E "eme-probe:|EME-PROBE"'
```
Expected output (this is the acceptance gate for the whole slice):
```
eme-probe: widevine cdm = "/opt/WidevineCdm/gmp-widevinecdm/latest/libwidevinecdm.so"
[page] EME-PROBE: secureContext=true
[page] EME-PROBE: h264=probably   (or "maybe"; "no" means proprietary codecs absent — record it)
[page] EME-PROBE: aac=probably    (same)
[page] EME-PROBE: vp9=probably
[page] EME-PROBE: WIDEVINE SUPPORTED (mp4)
[page] EME-PROBE: WIDEVINE SUPPORTED (webm)
```
`WIDEVINE UNAVAILABLE` on both attempts = the CDM didn't load (version/ABI mismatch): STOP, capture full probe stderr (drop the grep), and investigate before Task 6 — the spec's Decision 3 depends on this result.

- [ ] **Step 6: Widget-runtime regression check**

Run: `ssh matt@192.168.1.149 'ps -eo comm,args | grep -c "QtWebEngineProc.*renderer"; journalctl -u openauto-prodigy.service -b --since "10 min ago" | grep -ciE "webwidget.*(error|fail)" || true'`
Expected: renderer count ≥ 1 (dashboard web widget alive); zero webwidget errors.

- [ ] **Step 7: Manual real-service check (Matthew, ~10 min, non-blocking)**

With Matthew at the bench, run the probe against real services:
```bash
ssh matt@192.168.1.149 'cd ~/openauto-prodigy/build && \
  env WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 QT_QPA_PLATFORM=wayland \
  ./tools/eme-probe/eme-probe https://shaka-player-demo.appspot.com/'
```
In the Shaka demo pick a Widevine asset (e.g. "Angel One (multicodec, multilingual, Widevine)") and confirm video+audio plays. Optionally repeat with `https://open.spotify.com` (login on the touchscreen, play a track). Record results in the roadmap entry. This step can trail the rest of the slice — do not block Task 6 on it; mark the roadmap entry "pending bench check" if it hasn't happened.

---

### Task 6: Documentation deliverables

**Files:**
- Modify: `docs/design-philosophy.md` (Core Principles, after `### 7. Installable by Normal Humans`, before `## What We Don't Do` at line 75)
- Modify: `docs/wishlist.md` (Candidate Ideas section)
- Modify: `docs/roadmap-current.md` (Done + Later sections)

**Interfaces:**
- Consumes: verification results from Task 5 (steps 3, 5, 6).
- Produces: nothing (docs only).

- [ ] **Step 1: Codify the native/web rule in design-philosophy.md**

Insert after the `### 7. Installable by Normal Humans` section body:

```markdown
### 8. Native Core, Web Extensions

Native QML for anything core, driving-relevant, or latency-sensitive (projection,
phone, BT audio, settings, launcher, media playback). The web runtime
(QtWebEngine) is the extension surface: glanceable dashboard content, optional
add-ons, and third-party/community work that shouldn't require touching C++.

Two consequences:

- **WebEngine stays optional.** The app must build and run fully without
  qt6-webengine installed (`HAS_WEBENGINE`). No core function may depend on a
  browser stack existing.
- **No core plugin migrates to web.** Renderer processes can crash and reload
  (the widget host has retry machinery for exactly that); a weather tile
  tolerates this, an incoming-call overlay does not.

Decided 2026-07-07 after the web-widget ship — full rationale in
`docs/superpowers/specs/2026-07-07-web-surface-strategy-design.md`.
```

- [ ] **Step 2: Add the WebAppHost arc to the wishlist**

Add under `## Candidate Ideas` in `docs/wishlist.md`:

```markdown
- **WebAppHost — fullscreen streaming web apps (Spotify / YouTube / parked video)** —
  Sibling of the widget runtime with inverted policy: manifest-driven app packages
  under `~/.openauto/webapps/` surfacing as launcher tiles, persistent per-app
  WebEngine profiles (log in once), external navigation + fullscreen allowed,
  Widevine via the slice-1 CDM wiring, TV user-agents where they help (YouTube
  leanback = touch-friendly + code-pairing login). Open questions for its design
  arc: keyboard for non-code logins (Qt VirtualKeyboard vs. pair-from-phone),
  audio-focus policy for background playback under the AA view, one-app-alive
  resource cap, librespot as a Spotify Connect complement. Scoped in
  `docs/superpowers/specs/2026-07-07-web-surface-strategy-design.md` (Decision 3);
  queued behind the media player arc.
```

- [ ] **Step 3: Update the roadmap**

In `docs/roadmap-current.md`:

Add to `## Done (recent)`:

```markdown
- Widevine enablement (web-surface strategy Slice 1) — CDM auto-wiring in main.cpp
  (`--widevine-path` from `/opt/WidevineCdm`, env-override respected), best-effort
  `libwidevinecdm0` in install.sh, `tools/eme-probe` verification harness. Verified
  on Pi 2026-07-07: <record Task 5 results: CDM loaded yes/no, h264/aac/vp9, EME
  key-system access>. Desktop Chromium deliberately left installed (user's browser).
  Spec: `docs/superpowers/specs/2026-07-07-web-surface-strategy-design.md`. COMPLETE.
```

(Replace the `<record Task 5 results: ...>` placeholder with the actual probe output
from Task 5 steps 3/5/6 — do not commit the angle-bracket text. If step 7's bench
check hasn't happened, append "Real-service bench check (Shaka/Spotify) pending
~10 min with Matthew.")

Add to `## Later`, after the "Companion app work" bullet:

```markdown
- Streaming web apps (WebAppHost) — fullscreen Spotify/YouTube/parked-video surface
  riding the slice-1 Widevine wiring. Scoped (Decision 3 of the web-surface spec);
  wishlist entry has the open questions. Queued behind the media player arc.
```

- [ ] **Step 4: Commit**

```bash
git add docs/design-philosophy.md docs/wishlist.md docs/roadmap-current.md
git commit -m "docs: native-core/web-extensions principle + WebAppHost wishlist + slice-1 roadmap record"
```
