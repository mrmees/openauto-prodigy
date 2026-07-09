# JS/Web-Widget Runtime Implementation Plan (Phase C2)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Web widgets — HTML/JS packages under `~/.openauto/webwidgets/<id>/` served over a `prodigy://` scheme into a sandboxed `WebEngineView` grid widget, themed and API-connected through an injected `prodigy` shim that is a WebSocket client of External API v1.

**Architecture:** A `QWebEngineUrlSchemeHandler` (thin shell) delegates to a pure, headless-testable `WebWidgetContentResolver`; a `WebWidgetScanner` maps `widget.yaml` manifests to standard `WidgetDescriptor`s (kind `WebWidget` — **already exists**, added by multi-dashboards T3); `WebWidgetHost.qml` lazily instantiates a locked-down `WebEngineView` with four injected user scripts (templated bootstrap, protobuf runtime, generated proto module, static `prodigy.js`). All C++/QML WebEngine surfaces sit behind an optional `HAS_WEBENGINE` build gate mirroring `HAS_BLUETOOTH`.

**Tech Stack:** Qt 6.8 WebEngineQuick (optional component), yaml-cpp, protobufjs (Node needed only at proto-regen time, never in the CMake build).

**Design doc:** `docs/superpowers/specs/2026-07-06-js-runtime-design.md` (grounding for every decision; D1–D8 are locked). Grounded against `develop` @ `68a1992` (API v1 + v1.1 shipped; `api_version_minor` is now **1**).

## Global Constraints

- Build in `~/builds/openauto-prodigy` ONLY (ext4 + ccache); never create an in-tree `build/`. Cross-build via `sg docker -c "./cross-build.sh"`. Full suite currently 104 tests, green.
- **Rail R3:** the `prodigy` shim's public surface is exactly the design §6 list (`ready, context, apiUrl, subscribe, dispatch, notify, request, on`) — nothing more. No QWebChannel. Nothing in `window` beyond `prodigy` + `__prodigyBootstrap`.
- **No proto changes.** `proto/api/` is read-only for this plan (frozen additive; nothing here needs a field). `libs/prodigy-oaa-protocol/` untouched.
- **Do NOT re-add `WebWidget`** to `DashboardContributionKind` — it exists (`WidgetTypes.hpp:24-28`, index 2, order frozen). The picker filter already admits it (`WidgetRegistry.cpp:29-41`).
- CSS custom properties are exactly `--prodigy-<token>` with the API's hyphenated token names (42 tokens, e.g. `on-primary`, `surface-container-high`) — one vocabulary end to end; no camelCase variants.
- One shared origin: `prodigy://widgets/...` (D2). `QWebEngineUrlScheme::registerScheme` MUST run before `QGuiApplication` construction.
- Non-WebEngine builds must stay green: all new C++ logic except the scheme-handler shell compiles unconditionally; WebEngine-touching code is `#ifdef HAS_WEBENGINE` / `if(TARGET Qt6::WebEngineQuick)` gated.
- Localhost API sessions are trusted (no auth after `ClientHello` — `ApiSession.cpp:59-62,102-123`); WS frames are one bare serialized `ApiMessage` per binary message (no length prefix); `request_id` correlates request/response, `0` = stream event.
- **Plan-level deviation from design §6.4 (recorded):** the desktop dev-auth branch (client_id/secret via localStorage for paired LAN development) is DEFERRED — it needs the AuthRequired/AuthResponse HMAC vocabulary and desktop pairing UX; do it alongside the companion's WS auth client so both share one reference implementation. `prodigy.apiUrl` remains overridable, so nothing in this plan blocks it.
- Commits pre-approved on `develop`; push asks Matthew (standing rule: pushes to develop pre-approved during execution).

## File Structure

```
src/core/widget/WebWidgetManifest.{hpp,cpp}      # widget.yaml struct + parser (unconditional)
src/core/widget/WebWidgetScanner.{hpp,cpp}       # dir scan -> WidgetRegistry (unconditional)
src/core/webwidget/WebWidgetContentResolver.{hpp,cpp}  # id+path -> file, content types (unconditional)
src/core/webwidget/WebWidgetSchemeHandler.{hpp,cpp}    # QWebEngineUrlSchemeHandler shell (HAS_WEBENGINE)
qml/widgets/WebWidgetHost.qml                    # lazy WebEngineView host (loads only when scanner registered something)
resources/web/prodigy.js                         # static shim (hand-written)
resources/web/prodigy-proto.js                   # GENERATED protobufjs static module (committed)
resources/web/protobuf.min.js                    # protobufjs minimal runtime (committed, vendored)
tools/gen-proto-js.sh                            # regen script (Node; only on proto change)
examples/webwidgets/hello-theme/{widget.yaml,index.html}  # reference widget package
tests/test_web_widget_manifest.cpp
tests/test_web_widget_resolver.cpp
tests/test_web_widget_scanner.cpp
```

---

### Task 1: Optional WebEngine build gate + early init in main()

**Files:**
- Modify: `CMakeLists.txt:12` (OPTIONAL_COMPONENTS line)
- Modify: `src/CMakeLists.txt` (new gate block next to the HAS_BLUETOOTH block at ~:504-522)
- Modify: `src/main.cpp:151-153` (top of `main()`)
- Modify: `install.sh` (PACKAGES array, ~:797-801), `install-prebuilt.sh` (runtime package list)
- Modify: `docker/` cross-build image definition (find it — `cross-build.sh` references it) to add `qt6-webengine-dev`
- Check/Modify: `tests/test_install_list_prebuilt.py` / `tests/test_prebuilt_release_package.py` if they assert package lists

**Interfaces:**
- Produces: `HAS_WEBENGINE` compile definition (PUBLIC on `openauto-core`) that every later task's gated code uses; `prodigy` scheme registered + `QtWebEngineQuick::initialize()` called before `QGuiApplication`.

- [ ] **Step 1: Add the optional component**

`CMakeLists.txt` line 12 becomes:
```cmake
find_package(Qt6 OPTIONAL_COMPONENTS Bluetooth WebEngineQuick)
```

- [ ] **Step 2: Add the gate block** in `src/CMakeLists.txt`, directly after the `HAS_BLUETOOTH` block (~line 522):

```cmake
# Web widget runtime (JS/HTML widgets) — optional; needs qt6-webengine-dev
# to build, qml6-module-qtwebengine at runtime. Non-WebEngine builds keep
# the scanner/resolver logic (headless-testable) but never register widgets.
if(TARGET Qt6::WebEngineQuick)
    target_link_libraries(openauto-core PUBLIC Qt6::WebEngineQuick)
    target_compile_definitions(openauto-core PUBLIC HAS_WEBENGINE)
    message(STATUS "Qt6::WebEngineQuick found — web widget runtime enabled")
else()
    message(STATUS "Qt6::WebEngineQuick NOT found — web widget runtime disabled")
endif()
```
(No `target_sources` yet — the handler .cpp arrives in Task 3 and appends itself to this block.)

- [ ] **Step 3: Early init in main()** — `src/main.cpp`. Add includes near the top of the include block:

```cpp
#ifdef HAS_WEBENGINE
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#include <QWebEngineUrlScheme>
#endif
```

and make these the FIRST statements of `main()` (before `QGuiApplication app(argc, argv);` at :153):

```cpp
#ifdef HAS_WEBENGINE
    // Chromium requires custom schemes registered before the app object
    // exists (design §3/§9); initialize() must also precede QGuiApplication.
    {
        QWebEngineUrlScheme scheme("prodigy");
        scheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
        scheme.setFlags(QWebEngineUrlScheme::SecureScheme
                        | QWebEngineUrlScheme::LocalAccessAllowed);
        QWebEngineUrlScheme::registerScheme(scheme);
    }
    QtWebEngineQuick::initialize();
#endif
```

- [ ] **Step 4: Installer + cross-image packages**
  - `install.sh` PACKAGES (Qt block, :797-801): add `qt6-webengine-dev qml6-module-qtwebengine`.
  - `install-prebuilt.sh`: add `qml6-module-qtwebengine` (pulls `libqt6webenginequick6`) to its runtime package list.
  - Locate the cross-build Docker image definition (`grep -l webengine docker/* ; cat cross-build.sh` to find the Dockerfile) and add `qt6-webengine-dev` to its apt list so the Pi binary actually gets the feature. **Rebuild the image** per whatever `cross-build.sh` documents.
  - Run `grep -rn "websockets\|qt6-" tests/test_install_list_prebuilt.py tests/test_prebuilt_release_package.py` — if either asserts the package list, update the expectation to include the new packages.

- [ ] **Step 5: Build + full suite + boot smoke**

```bash
cd ~/builds/openauto-prodigy && cmake . && make -j$(nproc)
# expect: "Qt6::WebEngineQuick found — web widget runtime enabled" in cmake output
ctest --output-on-failure          # expect: 104/104 (or current count) pass
QT_QPA_PLATFORM=offscreen timeout 15 ./src/openauto-prodigy --verbose ; echo "exit=$?"
# expect: boots to steady state, no WebEngine warnings about scheme registration
```

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt src/CMakeLists.txt src/main.cpp install.sh install-prebuilt.sh docker/ tests/
git commit -m "build(webwidget): optional Qt6 WebEngineQuick gate (HAS_WEBENGINE) + prodigy:// scheme registration + installer/cross-image packages"
```

---

### Task 2: WebWidgetManifest (widget.yaml parser)

**Files:**
- Create: `src/core/widget/WebWidgetManifest.hpp`, `src/core/widget/WebWidgetManifest.cpp`
- Modify: `src/CMakeLists.txt` (add `core/widget/WebWidgetManifest.cpp` to the unconditional `openauto-core` sources, next to the other `core/widget/` entries)
- Test: `tests/test_web_widget_manifest.cpp`; Modify `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `struct oap::WebWidgetManifest { QString id, name, entry, category, description, icon, dirPath; int minCols, minRows, maxCols, maxRows, defaultCols, defaultRows; static WebWidgetManifest fromFile(const QString&); bool isValid() const; }`
- Style reference: `src/core/plugin/PluginManifest.cpp:13-70` (LoadFile + `as<T>(default)` + one `YAML::Exception` catch returning `{}`).

- [ ] **Step 1: Write the failing tests** — `tests/test_web_widget_manifest.cpp`:

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include "core/widget/WebWidgetManifest.hpp"

class TestWebWidgetManifest : public QObject {
    Q_OBJECT

    QString writeManifest(QTemporaryDir& dir, const QByteArray& yaml) {
        const QString path = dir.filePath("widget.yaml");
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(yaml);
        f.close();
        return path;
    }

private slots:
    void testFullManifestParses() {
        QTemporaryDir dir;
        const auto path = writeManifest(dir,
            "id: com.example.speedo\n"
            "name: \"Speedometer\"\n"
            "entry: main.html\n"
            "category: navigation\n"
            "description: \"GPS speedometer\"\n"
            "icon: \"\\ue9e4\"\n"
            "size:\n"
            "  minCols: 2\n  minRows: 1\n  maxCols: 4\n  maxRows: 3\n"
            "  defaultCols: 2\n  defaultRows: 2\n");
        const auto m = oap::WebWidgetManifest::fromFile(path);
        QVERIFY(m.isValid());
        QCOMPARE(m.id, QStringLiteral("com.example.speedo"));
        QCOMPARE(m.name, QStringLiteral("Speedometer"));
        QCOMPARE(m.entry, QStringLiteral("main.html"));
        QCOMPARE(m.category, QStringLiteral("navigation"));
        QCOMPARE(m.minCols, 2); QCOMPARE(m.maxRows, 3);
        QCOMPARE(m.defaultCols, 2); QCOMPARE(m.defaultRows, 2);
        QCOMPARE(m.dirPath, dir.path());
    }
    void testDefaultsApplied() {
        QTemporaryDir dir;
        const auto m = oap::WebWidgetManifest::fromFile(
            writeManifest(dir, "id: a.b\nname: Minimal\n"));
        QVERIFY(m.isValid());
        QCOMPARE(m.entry, QStringLiteral("index.html"));
        QCOMPARE(m.category, QStringLiteral("status"));
        QCOMPARE(m.minCols, 1); QCOMPARE(m.maxCols, 6);
        QCOMPARE(m.maxRows, 4); QCOMPARE(m.defaultCols, 1);
    }
    void testMissingIdInvalid() {
        QTemporaryDir dir;
        QVERIFY(!oap::WebWidgetManifest::fromFile(
            writeManifest(dir, "name: NoId\n")).isValid());
    }
    void testUnsafeIdInvalid() {
        QTemporaryDir dir;
        // id becomes a prodigy:// URL segment — path chars are forbidden
        QVERIFY(!oap::WebWidgetManifest::fromFile(
            writeManifest(dir, "id: \"../escape\"\nname: Evil\n")).isValid());
        QVERIFY(!oap::WebWidgetManifest::fromFile(
            writeManifest(dir, "id: \"a/b\"\nname: Evil\n")).isValid());
    }
    void testUnsafeEntryInvalid() {
        QTemporaryDir dir;
        QVERIFY(!oap::WebWidgetManifest::fromFile(
            writeManifest(dir, "id: a.b\nname: X\nentry: \"../../etc/passwd\"\n")).isValid());
    }
    void testMalformedYamlInvalid() {
        QTemporaryDir dir;
        QVERIFY(!oap::WebWidgetManifest::fromFile(
            writeManifest(dir, "id: [unclosed\n\t: {{\n")).isValid());
    }
    void testMissingFileInvalid() {
        QVERIFY(!oap::WebWidgetManifest::fromFile(
            QStringLiteral("/nonexistent/widget.yaml")).isValid());
    }
};
QTEST_GUILESS_MAIN(TestWebWidgetManifest)
#include "test_web_widget_manifest.moc"
```

`tests/CMakeLists.txt`:
```cmake
oap_add_test(test_web_widget_manifest SOURCES test_web_widget_manifest.cpp)
```

- [ ] **Step 2: Run to verify failure**

```bash
cd ~/builds/openauto-prodigy && cmake . && make -j$(nproc) test_web_widget_manifest
```
Expected: FAIL to compile — `core/widget/WebWidgetManifest.hpp: No such file or directory`.

- [ ] **Step 3: Implement** — `src/core/widget/WebWidgetManifest.hpp`:

```cpp
#pragma once

#include <QString>

namespace oap {

// Parsed ~/.openauto/webwidgets/<dir>/widget.yaml (design §4). Mirrors the
// WidgetDescriptor fields a web package may set; defaults match
// WidgetDescriptor defaults.
struct WebWidgetManifest {
    QString id;           // required; becomes the prodigy:// URL segment
    QString name;         // required; picker display name
    QString entry = QStringLiteral("index.html");
    QString category = QStringLiteral("status");
    QString description;
    QString icon;         // Material codepoint, native-widget convention
    int minCols = 1;
    int minRows = 1;
    int maxCols = 6;
    int maxRows = 4;
    int defaultCols = 1;
    int defaultRows = 1;
    QString dirPath;      // package directory, set by fromFile()

    static WebWidgetManifest fromFile(const QString& filePath);
    bool isValid() const;
};

} // namespace oap
```

`src/core/widget/WebWidgetManifest.cpp`:

```cpp
#include "core/widget/WebWidgetManifest.hpp"

#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>
#include <yaml-cpp/yaml.h>

namespace oap {

bool WebWidgetManifest::isValid() const
{
    // id is used verbatim as a URL path segment and resolver key.
    static const QRegularExpression safeId(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));
    if (id.isEmpty() || name.isEmpty() || !safeId.match(id).hasMatch())
        return false;
    if (entry.isEmpty() || entry.startsWith(u'/') || entry.contains(QStringLiteral("..")))
        return false;
    if (minCols < 1 || minRows < 1 || maxCols < minCols || maxRows < minRows)
        return false;
    if (defaultCols < minCols || defaultCols > maxCols
        || defaultRows < minRows || defaultRows > maxRows)
        return false;
    return true;
}

WebWidgetManifest WebWidgetManifest::fromFile(const QString& filePath)
{
    WebWidgetManifest m;
    try {
        YAML::Node root = YAML::LoadFile(filePath.toStdString());

        m.id = QString::fromStdString(root["id"].as<std::string>(""));
        m.name = QString::fromStdString(root["name"].as<std::string>(""));
        m.entry = QString::fromStdString(root["entry"].as<std::string>("index.html"));
        m.category = QString::fromStdString(root["category"].as<std::string>("status"));
        m.description = QString::fromStdString(root["description"].as<std::string>(""));
        m.icon = QString::fromStdString(root["icon"].as<std::string>(""));

        if (root["size"]) {
            const auto& s = root["size"];
            m.minCols = s["minCols"].as<int>(m.minCols);
            m.minRows = s["minRows"].as<int>(m.minRows);
            m.maxCols = s["maxCols"].as<int>(m.maxCols);
            m.maxRows = s["maxRows"].as<int>(m.maxRows);
            m.defaultCols = s["defaultCols"].as<int>(m.defaultCols);
            m.defaultRows = s["defaultRows"].as<int>(m.defaultRows);
        }
        m.dirPath = QFileInfo(filePath).absolutePath();
    } catch (const YAML::Exception& e) {
        qWarning() << "WebWidgetManifest: failed to parse" << filePath << ":" << e.what();
        return {};
    }
    return m;
}

} // namespace oap
```

Add `core/widget/WebWidgetManifest.cpp` to `openauto-core` sources in `src/CMakeLists.txt` (adjacent to the other `core/widget/` files).

- [ ] **Step 4: Run tests to verify pass**

```bash
cd ~/builds/openauto-prodigy && cmake . && make -j$(nproc) test_web_widget_manifest \
  && ctest --output-on-failure -R web_widget_manifest
```
Expected: PASS (7 slots).

- [ ] **Step 5: Full suite, then commit**

```bash
ctest --output-on-failure   # all green (previous count + 1 binary)
git add src/core/widget/WebWidgetManifest.* src/CMakeLists.txt tests/
git commit -m "feat(webwidget): widget.yaml manifest parser with URL-safe id/entry validation"
```

---

### Task 3: WebWidgetContentResolver + prodigy:// scheme handler

**Files:**
- Create: `src/core/webwidget/WebWidgetContentResolver.hpp`, `.cpp` (unconditional sources)
- Create: `src/core/webwidget/WebWidgetSchemeHandler.hpp`, `.cpp` (added ONLY inside the `if(TARGET Qt6::WebEngineQuick)` block from Task 1)
- Test: `tests/test_web_widget_resolver.cpp`; Modify `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `oap::WebWidgetContentResolver` — `void registerPackage(const QString& id, const QString& dirPath)`, `QString resolve(const QString& id, const QString& relativePath) const` (empty on unknown/escape/missing), `static QByteArray contentTypeFor(const QString& filePath)`.
- Produces: `oap::WebWidgetSchemeHandler(WebWidgetContentResolver*, QObject*)` (HAS_WEBENGINE only). Task 4 wires both in main.cpp.

- [ ] **Step 1: Write the failing tests** — `tests/test_web_widget_resolver.cpp`:

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include "core/webwidget/WebWidgetContentResolver.hpp"

class TestWebWidgetResolver : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    oap::WebWidgetContentResolver resolver_;

    void touch(const QString& rel, const QByteArray& content = "x") {
        QFileInfo fi(dir_.filePath(rel));
        QDir().mkpath(fi.absolutePath());
        QFile f(fi.absoluteFilePath());
        f.open(QIODevice::WriteOnly);
        f.write(content);
    }

private slots:
    void initTestCase() {
        touch("pkg/index.html", "<html/>");
        touch("pkg/assets/app.js");
        touch("outside.txt", "secret");
        resolver_.registerPackage("com.test.pkg", dir_.filePath("pkg"));
    }
    void testResolvesEntryFile() {
        const auto p = resolver_.resolve("com.test.pkg", "index.html");
        QVERIFY(!p.isEmpty());
        QVERIFY(p.endsWith(QStringLiteral("pkg/index.html")));
    }
    void testResolvesNestedAsset() {
        QVERIFY(!resolver_.resolve("com.test.pkg", "assets/app.js").isEmpty());
    }
    void testUnknownIdEmpty() {
        QVERIFY(resolver_.resolve("nope", "index.html").isEmpty());
    }
    void testMissingFileEmpty() {
        QVERIFY(resolver_.resolve("com.test.pkg", "missing.html").isEmpty());
    }
    void testTraversalBlocked() {
        QVERIFY(resolver_.resolve("com.test.pkg", "../outside.txt").isEmpty());
        QVERIFY(resolver_.resolve("com.test.pkg", "assets/../../outside.txt").isEmpty());
    }
    void testSymlinkEscapeBlocked() {
        QFile::link(dir_.filePath("outside.txt"), dir_.filePath("pkg/sneaky.txt"));
        QVERIFY(resolver_.resolve("com.test.pkg", "sneaky.txt").isEmpty());
    }
    void testContentTypes() {
        using R = oap::WebWidgetContentResolver;
        QCOMPARE(R::contentTypeFor("a/index.html"), QByteArray("text/html"));
        QCOMPARE(R::contentTypeFor("a/x.js"), QByteArray("application/javascript"));
        QCOMPARE(R::contentTypeFor("a/x.css"), QByteArray("text/css"));
        QCOMPARE(R::contentTypeFor("a/x.svg"), QByteArray("image/svg+xml"));
        QCOMPARE(R::contentTypeFor("a/x.png"), QByteArray("image/png"));
        QCOMPARE(R::contentTypeFor("a/x.jpg"), QByteArray("image/jpeg"));
        QCOMPARE(R::contentTypeFor("a/x.woff2"), QByteArray("font/woff2"));
        QCOMPARE(R::contentTypeFor("a/x.json"), QByteArray("application/json"));
        QCOMPARE(R::contentTypeFor("a/x.bin"), QByteArray("application/octet-stream"));
    }
};
QTEST_GUILESS_MAIN(TestWebWidgetResolver)
#include "test_web_widget_resolver.moc"
```

`tests/CMakeLists.txt`: `oap_add_test(test_web_widget_resolver SOURCES test_web_widget_resolver.cpp)`

- [ ] **Step 2: Run to verify failure** — compile error, header missing (as Task 2 Step 2).

- [ ] **Step 3: Implement the resolver** — `src/core/webwidget/WebWidgetContentResolver.hpp`:

```cpp
#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

namespace oap {

// Maps prodigy://widgets/<id>/<path> to files inside registered package
// directories. Pure logic (no WebEngine types) so it tests headless;
// WebWidgetSchemeHandler is the thin WebEngine shell around it (design §3).
class WebWidgetContentResolver {
public:
    void registerPackage(const QString& id, const QString& dirPath);

    // Absolute canonical file path, or empty for unknown id, missing file,
    // or any path escaping the package dir (traversal / symlink).
    QString resolve(const QString& id, const QString& relativePath) const;

    static QByteArray contentTypeFor(const QString& filePath);

private:
    QHash<QString, QString> packages_;  // id -> canonical package dir
};

} // namespace oap
```

`src/core/webwidget/WebWidgetContentResolver.cpp`:

```cpp
#include "core/webwidget/WebWidgetContentResolver.hpp"

#include <QFileInfo>

namespace oap {

void WebWidgetContentResolver::registerPackage(const QString& id, const QString& dirPath)
{
    const QString canonical = QFileInfo(dirPath).canonicalFilePath();
    if (!canonical.isEmpty())
        packages_.insert(id, canonical);
}

QString WebWidgetContentResolver::resolve(const QString& id,
                                          const QString& relativePath) const
{
    const QString base = packages_.value(id);
    if (base.isEmpty())
        return {};
    // canonicalFilePath() resolves ".." and symlinks; empty if missing.
    const QString file = QFileInfo(base + u'/' + relativePath).canonicalFilePath();
    if (file.isEmpty() || !file.startsWith(base + u'/'))
        return {};
    return file;
}

QByteArray WebWidgetContentResolver::contentTypeFor(const QString& filePath)
{
    const QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == u"html" || ext == u"htm") return QByteArrayLiteral("text/html");
    if (ext == u"js")    return QByteArrayLiteral("application/javascript");
    if (ext == u"css")   return QByteArrayLiteral("text/css");
    if (ext == u"svg")   return QByteArrayLiteral("image/svg+xml");
    if (ext == u"png")   return QByteArrayLiteral("image/png");
    if (ext == u"jpg" || ext == u"jpeg") return QByteArrayLiteral("image/jpeg");
    if (ext == u"woff2") return QByteArrayLiteral("font/woff2");
    if (ext == u"json")  return QByteArrayLiteral("application/json");
    return QByteArrayLiteral("application/octet-stream");
}

} // namespace oap
```

Add `core/webwidget/WebWidgetContentResolver.cpp` to the unconditional `openauto-core` sources.

- [ ] **Step 4: Run resolver tests to verify pass**

```bash
cd ~/builds/openauto-prodigy && cmake . && make -j$(nproc) test_web_widget_resolver \
  && ctest --output-on-failure -R web_widget_resolver
```
Expected: PASS (8 slots).

- [ ] **Step 5: Implement the scheme handler shell** (no unit test — verified on Pi in Task 9; keep it thin). `src/core/webwidget/WebWidgetSchemeHandler.hpp`:

```cpp
#pragma once

#include <QWebEngineUrlSchemeHandler>

#include "core/webwidget/WebWidgetContentResolver.hpp"

namespace oap {

// Serves prodigy://widgets/<id>/<path> from scanned package directories.
// All decisions live in WebWidgetContentResolver; this class only speaks
// WebEngine (design §3).
class WebWidgetSchemeHandler : public QWebEngineUrlSchemeHandler {
    Q_OBJECT
public:
    explicit WebWidgetSchemeHandler(WebWidgetContentResolver* resolver,
                                    QObject* parent = nullptr);
    void requestStarted(QWebEngineUrlRequestJob* job) override;

private:
    WebWidgetContentResolver* resolver_;
};

} // namespace oap
```

`src/core/webwidget/WebWidgetSchemeHandler.cpp`:

```cpp
#include "core/webwidget/WebWidgetSchemeHandler.hpp"

#include <QFile>
#include <QUrl>
#include <QWebEngineUrlRequestJob>

namespace oap {

WebWidgetSchemeHandler::WebWidgetSchemeHandler(WebWidgetContentResolver* resolver,
                                               QObject* parent)
    : QWebEngineUrlSchemeHandler(parent), resolver_(resolver) {}

void WebWidgetSchemeHandler::requestStarted(QWebEngineUrlRequestJob* job)
{
    const QUrl url = job->requestUrl();
    if (url.host() != QLatin1String("widgets")) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    const QString path = url.path();                 // "/<id>/<rel...>"
    const int slash = path.indexOf(u'/', 1);
    if (slash < 0) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    const QString id = path.mid(1, slash - 1);
    const QString rel = path.mid(slash + 1);
    const QString file = resolver_->resolve(id, rel);
    if (file.isEmpty()) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    auto* f = new QFile(file, job);                  // job owns the device
    if (!f->open(QIODevice::ReadOnly)) {
        job->fail(QWebEngineUrlRequestJob::RequestFailed);
        return;
    }
    job->reply(WebWidgetContentResolver::contentTypeFor(file), f);
}

} // namespace oap
```

In `src/CMakeLists.txt`, inside the Task 1 gate block, add:
```cmake
    target_sources(openauto-core PRIVATE
        core/webwidget/WebWidgetSchemeHandler.cpp
    )
```

- [ ] **Step 6: Full build + suite, commit**

```bash
cd ~/builds/openauto-prodigy && cmake . && make -j$(nproc) && ctest --output-on-failure
git add src/core/webwidget/ src/CMakeLists.txt tests/
git commit -m "feat(webwidget): prodigy:// content resolver (canonical-path jail, content types) + scheme handler shell"
```

---

### Task 4: WebWidgetScanner + main.cpp wiring

**Files:**
- Create: `src/core/widget/WebWidgetScanner.hpp`, `.cpp` (unconditional)
- Modify: `src/main.cpp` (~line 788, after native widget registrations at 621-788, before the plugin descriptor loop at 791)
- Test: `tests/test_web_widget_scanner.cpp`; Modify `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `WebWidgetManifest::fromFile/isValid` (Task 2), `WebWidgetContentResolver::registerPackage` (Task 3), `WidgetRegistry::registerWidget` (returns false on duplicate id, first-wins — `WidgetRegistry.cpp:7-12`).
- Produces: `static int oap::WebWidgetScanner::scan(const QString& rootDir, WidgetRegistry& registry, WebWidgetContentResolver* resolver)` — returns count registered. Descriptors carry `qmlComponent = qrc:/OpenAutoProdigy/WebWidgetHost.qml`, `contributionKind = WebWidget`, `defaultConfig = {"url": "prodigy://widgets/<id>/<entry>"}` — Task 8's host reads `effectiveConfig.url`.

- [ ] **Step 1: Write the failing tests** — `tests/test_web_widget_scanner.cpp`:

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include "core/widget/WebWidgetScanner.hpp"
#include "core/widget/WidgetRegistry.hpp"
#include "core/webwidget/WebWidgetContentResolver.hpp"

class TestWebWidgetScanner : public QObject {
    Q_OBJECT

    void writePackage(const QString& root, const QString& dirName, const QByteArray& yaml) {
        QDir().mkpath(root + u'/' + dirName);
        QFile f(root + u'/' + dirName + QStringLiteral("/widget.yaml"));
        f.open(QIODevice::WriteOnly);
        f.write(yaml);
    }

private slots:
    void testScanRegistersValidSkipsBad() {
        QTemporaryDir dir;
        writePackage(dir.path(), "speedo",
            "id: com.test.speedo\nname: Speedo\nentry: main.html\n"
            "size: {defaultCols: 2, defaultRows: 2}\n");
        writePackage(dir.path(), "clockish",
            "id: com.test.clockish\nname: Clockish\n");
        writePackage(dir.path(), "broken", "name: NoId\n");          // invalid
        QDir().mkpath(dir.path() + QStringLiteral("/nomanifest"));    // no widget.yaml

        oap::WidgetRegistry registry;
        oap::WebWidgetContentResolver resolver;
        const int n = oap::WebWidgetScanner::scan(dir.path(), registry, &resolver);
        QCOMPARE(n, 2);

        const auto d = registry.descriptor(QStringLiteral("com.test.speedo"));
        QVERIFY(d.has_value());
        QCOMPARE(d->displayName, QStringLiteral("Speedo"));
        QCOMPARE(d->contributionKind, oap::DashboardContributionKind::WebWidget);
        QCOMPARE(d->qmlComponent,
                 QUrl(QStringLiteral("qrc:/OpenAutoProdigy/WebWidgetHost.qml")));
        QCOMPARE(d->defaultConfig.value(QStringLiteral("url")).toString(),
                 QStringLiteral("prodigy://widgets/com.test.speedo/main.html"));
        QCOMPARE(d->defaultCols, 2);
        QVERIFY(!registry.descriptor(QStringLiteral("com.test.clockish"))->displayName.isEmpty());
        // resolver learned the package dir
        QVERIFY(resolver.resolve(QStringLiteral("com.test.speedo"),
                                 QStringLiteral("widget.yaml")).endsWith(
                                 QStringLiteral("speedo/widget.yaml")));
    }
    void testDuplicateIdSkippedFirstWins() {
        QTemporaryDir dir;
        writePackage(dir.path(), "a", "id: com.test.dup\nname: First\n");
        oap::WidgetRegistry registry;
        oap::WidgetDescriptor native;
        native.id = QStringLiteral("com.test.dup");
        native.displayName = QStringLiteral("Native");
        registry.registerWidget(native);
        QCOMPARE(oap::WebWidgetScanner::scan(dir.path(), registry, nullptr), 0);
        QCOMPARE(registry.descriptor(QStringLiteral("com.test.dup"))->displayName,
                 QStringLiteral("Native"));
    }
    void testMissingRootDirNoop() {
        oap::WidgetRegistry registry;
        QCOMPARE(oap::WebWidgetScanner::scan(QStringLiteral("/nonexistent-dir-xyz"),
                                             registry, nullptr), 0);
    }
};
QTEST_GUILESS_MAIN(TestWebWidgetScanner)
#include "test_web_widget_scanner.moc"
```

`tests/CMakeLists.txt`: `oap_add_test(test_web_widget_scanner SOURCES test_web_widget_scanner.cpp)`

- [ ] **Step 2: Run to verify failure** — compile error, `WebWidgetScanner.hpp` missing.

- [ ] **Step 3: Implement** — `src/core/widget/WebWidgetScanner.hpp`:

```cpp
#pragma once

#include <QString>

namespace oap {

class WidgetRegistry;
class WebWidgetContentResolver;

// Startup scan of ~/.openauto/webwidgets/ (design §4): each subdirectory
// with a valid widget.yaml becomes a WebWidget WidgetDescriptor hosted by
// WebWidgetHost.qml. Bad packages are logged and skipped — a broken widget
// must never take the launcher down. Scan-once; no hot reload in v1.
class WebWidgetScanner {
public:
    // Returns the number of widgets registered. resolver may be null
    // (tests, non-WebEngine builds) — package dirs are then not served.
    static int scan(const QString& rootDir, WidgetRegistry& registry,
                    WebWidgetContentResolver* resolver);
};

} // namespace oap
```

`src/core/widget/WebWidgetScanner.cpp`:

```cpp
#include "core/widget/WebWidgetScanner.hpp"

#include <QDebug>
#include <QDir>

#include "core/webwidget/WebWidgetContentResolver.hpp"
#include "core/widget/WebWidgetManifest.hpp"
#include "core/widget/WidgetRegistry.hpp"

namespace oap {

int WebWidgetScanner::scan(const QString& rootDir, WidgetRegistry& registry,
                           WebWidgetContentResolver* resolver)
{
    QDir root(rootDir);
    if (!root.exists())
        return 0;

    int count = 0;
    const auto dirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& dirName : dirs) {
        const QString manifestPath = root.filePath(dirName + QStringLiteral("/widget.yaml"));
        if (!QFile::exists(manifestPath))
            continue;
        const WebWidgetManifest m = WebWidgetManifest::fromFile(manifestPath);
        if (!m.isValid()) {
            qWarning() << "WebWidgetScanner: skipping invalid package" << manifestPath;
            continue;
        }

        WidgetDescriptor d;
        d.id = m.id;
        d.displayName = m.name;
        d.iconName = m.icon;
        d.category = m.category;
        d.description = m.description;
        d.qmlComponent = QUrl(QStringLiteral("qrc:/OpenAutoProdigy/WebWidgetHost.qml"));
        d.contributionKind = DashboardContributionKind::WebWidget;
        d.defaultConfig = QVariantMap{
            {QStringLiteral("url"),
             QStringLiteral("prodigy://widgets/%1/%2").arg(m.id, m.entry)},
        };
        d.minCols = m.minCols;
        d.minRows = m.minRows;
        d.maxCols = m.maxCols;
        d.maxRows = m.maxRows;
        d.defaultCols = m.defaultCols;
        d.defaultRows = m.defaultRows;

        if (!registry.registerWidget(d)) {
            qWarning() << "WebWidgetScanner: duplicate widget id" << m.id
                       << "— keeping the earlier registration";
            continue;
        }
        if (resolver)
            resolver->registerPackage(m.id, m.dirPath);
        ++count;
    }
    return count;
}

} // namespace oap
```

Add `core/widget/WebWidgetScanner.cpp` to unconditional `openauto-core` sources.

- [ ] **Step 4: Run scanner tests → pass; then wire main.cpp.** In `src/main.cpp`, right after the native widget registrations and BEFORE the plugin descriptor loop (`main.cpp:791`), add:

```cpp
#ifdef HAS_WEBENGINE
    // Web widget runtime: serve scanned packages over prodigy:// and
    // register them as grid widgets (design 2026-07-06-js-runtime §3-§4).
    auto* webWidgetResolver = new oap::WebWidgetContentResolver();
    auto* webWidgetSchemeHandler =
        new oap::WebWidgetSchemeHandler(webWidgetResolver, &app);
    QQuickWebEngineProfile::defaultProfile()->installUrlSchemeHandler(
        "prodigy", webWidgetSchemeHandler);
    const int webWidgetCount = oap::WebWidgetScanner::scan(
        QDir::homePath() + QStringLiteral("/.openauto/webwidgets"),
        *widgetRegistry, webWidgetResolver);
    if (webWidgetCount > 0)
        qInfo() << "Registered" << webWidgetCount << "web widget(s)";
#endif
```

with includes (top of main.cpp, near the gated includes from Task 1):

```cpp
#ifdef HAS_WEBENGINE
#include <QtWebEngineQuick/QQuickWebEngineProfile>
#include "core/webwidget/WebWidgetContentResolver.hpp"
#include "core/webwidget/WebWidgetSchemeHandler.hpp"
#include "core/widget/WebWidgetScanner.hpp"
#endif
```

Note: `WebWidgetContentResolver` is not a QObject — the single instance intentionally lives for the app lifetime (the profile handler references it).

- [ ] **Step 5: Full build + suite + offscreen boot smoke** (as Task 1 Step 5 — the scan runs with an empty/missing `~/.openauto/webwidgets`, must be a silent no-op). Commit:

```bash
git add src/core/widget/WebWidgetScanner.* src/main.cpp src/CMakeLists.txt tests/
git commit -m "feat(webwidget): package scanner -> WidgetRegistry + prodigy:// profile wiring in main"
```

---

### Task 5: ThemeService::themeTokenMap() — one token vocabulary

**Files:**
- Modify: `src/core/services/ThemeService.hpp`, `.cpp` (move the 42-token list here)
- Modify: `src/core/api/ApiSerializers.cpp` (consume the map; delete its local `kThemeTokens`)
- Test: extend `tests/test_api_serializers.cpp`

**Interfaces:**
- Produces: `Q_INVOKABLE QVariantMap ThemeService::themeTokenMap() const` — 42 entries, hyphenated token name → `"#rrggbb"` string. Task 8's QML bootstrap calls it; the API serializer consumes it (single source of the vocabulary).

- [ ] **Step 1: Write the failing test** — add to `tests/test_api_serializers.cpp`:

```cpp
void testThemeTokenMapMatchesSerializer() {
    oap::ThemeService theme;
    theme.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));
    const QVariantMap map = theme.themeTokenMap();
    QCOMPARE(map.size(), 42);
    QVERIFY(map.contains(QStringLiteral("on-primary")));
    QVERIFY(map.contains(QStringLiteral("surface-container-high")));

    const auto status = oap::api::serial::buildSystemStatus(
        theme, QStringLiteral("1.0.0 (test)"), nullptr, nullptr);
    QCOMPARE(int(status.theme_tokens_size()), map.size());
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        const auto tok = status.theme_tokens().find(it.key().toStdString());
        QVERIFY2(tok != status.theme_tokens().end(), qPrintable(it.key()));
        QCOMPARE(QString::fromStdString(tok->second), it.value().toString());
    }
}
```
(Match the existing fixture style at `test_api_serializers.cpp:157-215`; `buildSystemStatus` now takes the DisplayInfo pointer added by API v1.1 — pass `nullptr`.)

- [ ] **Step 2: Run to verify failure** — `themeTokenMap` doesn't exist: compile error.

- [ ] **Step 3: Implement.** Move the `kThemeTokens[]` array (42 names, currently `src/core/api/ApiSerializers.cpp:13-27`) VERBATIM into `ThemeService.cpp` (file-local). Add to `ThemeService.hpp` (public slots/invokables section):

```cpp
    // The API/web-runtime theme vocabulary: hyphenated token -> "#rrggbb".
    // Single source for SystemStatus.theme_tokens and the web bootstrap's
    // CSS custom properties (--prodigy-<token>).
    Q_INVOKABLE QVariantMap themeTokenMap() const;
```

Implementation — reuse EXACTLY the accessor the serializer's token loop uses today (read `ApiSerializers.cpp`'s loop before writing this; keep its accessor, move the logic verbatim):

```cpp
QVariantMap ThemeService::themeTokenMap() const
{
    QVariantMap map;
    for (const char* name : kThemeTokens) {
        const QString key = QString::fromLatin1(name);
        map.insert(key, color(key).name());   // keep the serializer's accessor
    }
    return map;
}
```

Then rewrite the serializer's token loop in `buildSystemStatus` to consume it:

```cpp
    const QVariantMap tokens = theme.themeTokenMap();
    for (auto it = tokens.constBegin(); it != tokens.constEnd(); ++it)
        (*status.mutable_theme_tokens())[it.key().toStdString()] =
            it.value().toString().toStdString();
```
and delete `kThemeTokens` from `ApiSerializers.cpp`.

- [ ] **Step 4: Run to verify pass**

```bash
cd ~/builds/openauto-prodigy && make -j$(nproc) test_api_serializers \
  && ctest --output-on-failure -R api_serializers
```
Expected: PASS including the pre-existing `testSystemThemeTokensAndVersion` (42 tokens, hex format) — proves the move changed nothing on the wire.

- [ ] **Step 5: Full suite, commit**

```bash
ctest --output-on-failure
git add src/core/services/ThemeService.* src/core/api/ApiSerializers.cpp tests/test_api_serializers.cpp
git commit -m "refactor(theme): themeTokenMap() as the single token vocabulary (API serializer + web bootstrap consume it)"
```

---

### Task 6: Protobuf-JS toolchain (generated module, committed)

**Files:**
- Create: `tools/gen-proto-js.sh` (executable)
- Create (generated/vendored, committed): `resources/web/prodigy-proto.js`, `resources/web/protobuf.min.js`
- Modify: `resources/resources.qrc` (add the two files + reserve `web/prodigy.js` for Task 7 — add its entry in Task 7)

**Interfaces:**
- Produces: browser globals — `protobuf` (minimal runtime) and `protobuf.roots["prodigy-api"].prodigy.api.v1` (generated types: `ApiMessage`, `ClientKind`, `Topic`, ...). Task 7's `prodigy.js` consumes exactly these names.

- [ ] **Step 1: Write the script** — `tools/gen-proto-js.sh`:

```bash
#!/usr/bin/env bash
# Regenerates resources/web/prodigy-proto.js (+ vendored protobuf.min.js)
# from proto/api/*.proto. Requires Node.js — needed ONLY when the frozen
# additive proto contract gains fields; never part of the CMake build
# (design 2026-07-06-js-runtime D7).
set -euo pipefail
cd "$(dirname "$0")/.."

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

npm install --prefix "$WORK" --no-save --silent protobufjs@7 protobufjs-cli@1

"$WORK/node_modules/.bin/pbjs" \
    -t static-module -w closure -r prodigy-api \
    --no-service --no-delimited \
    -o resources/web/prodigy-proto.js \
    proto/api/*.proto

cp "$WORK/node_modules/protobufjs/dist/minimal/protobuf.min.js" \
    resources/web/protobuf.min.js

echo "Regenerated resources/web/prodigy-proto.js and protobuf.min.js"
echo "Root: protobuf.roots['prodigy-api'].prodigy.api.v1"
```
`chmod +x tools/gen-proto-js.sh`.

- [ ] **Step 2: Ensure Node exists, run it**

```bash
command -v node || sudo apt-get install -y nodejs npm
./tools/gen-proto-js.sh
grep -c "prodigy.api.v1" resources/web/prodigy-proto.js   # expect: >= 1
node -e "global.window=global; require('./resources/web/protobuf.min.js'); require('./resources/web/prodigy-proto.js'); const pb=protobuf.roots['prodigy-api'].prodigy.api.v1; const b=pb.ApiMessage.encode(pb.ApiMessage.create({requestId:7, clientHello:{requestedApiVersionMajor:1, clientKind:pb.ClientKind.CLIENT_KIND_WEB_WIDGET, clientName:'t'}})).finish(); const m=pb.ApiMessage.decode(b); console.log('roundtrip ok', m.requestId.toString(), m.clientHello.clientName);"
```
Expected: `roundtrip ok 7 t`. (If the closure wrapper's global handling differs, adjust the smoke invocation — the committed artifacts are what matters; record any wrapper quirk in the report for Task 7's shim.)

- [ ] **Step 3: Add to qrc** — in `resources/resources.qrc`, under its existing top-level prefix (inspect the file; icons resolve as `:/icons/...`, so the prefix is `/`), add:

```xml
<file>web/protobuf.min.js</file>
<file>web/prodigy-proto.js</file>
```
Resulting URLs: `qrc:/web/protobuf.min.js`, `qrc:/web/prodigy-proto.js`. If the qrc uses a different prefix layout, keep the final URLs recorded and use them consistently in Task 7/8.

- [ ] **Step 4: Build (resource compile) + full suite, commit**

```bash
cd ~/builds/openauto-prodigy && cmake . && make -j$(nproc) && ctest --output-on-failure
git add tools/gen-proto-js.sh resources/web/ resources/resources.qrc
git commit -m "feat(webwidget): protobufjs toolchain — gen script + committed static module and runtime"
```

---

### Task 7: prodigy.js shim

**Files:**
- Create: `resources/web/prodigy.js`
- Modify: `resources/resources.qrc` (add `<file>web/prodigy.js</file>`)

**Interfaces:**
- Consumes: `window.__prodigyBootstrap` (Task 8 injects: `{apiUrl, context:{instanceId,widgetId,colSpan,rowSpan,kind}, themeTokens}`), globals from Task 6.
- Produces: `window.prodigy` with EXACTLY the design §6 surface. Wire behavior: `ClientHello` → `ServerHello` (localhost trusted, no auth), auto-subscribe `TOPIC_SYSTEM`, re-apply theme tokens on every `SystemStatus`, request/response by `request_id`, reconnect 1s→30s capped backoff with automatic re-subscribe.

- [ ] **Step 1: Write the file** — `resources/web/prodigy.js`:

```js
/* prodigy.js — the bootstrap shim injected into every web widget (design
 * 2026-07-06-js-runtime §6). This is the ONLY privileged surface a widget
 * gets; everything rides the public External API over WebSocket (rail R3).
 * Injection order (WebWidgetHost.qml): bootstrap -> protobuf.min.js ->
 * prodigy-proto.js -> this file.
 */
(function () {
    'use strict';
    if (window.prodigy) return;

    var boot = window.__prodigyBootstrap || {};
    var root = (window.protobuf && protobuf.roots && protobuf.roots['prodigy-api']) || null;
    var pb = root && root.prodigy && root.prodigy.api ? root.prodigy.api.v1 : null;

    // ---- theme tokens -> CSS custom properties (--prodigy-<token>) ------
    function applyTokens(tokens) {
        if (!tokens) return;
        var el = document.documentElement;
        Object.keys(tokens).forEach(function (k) {
            el.style.setProperty('--prodigy-' + k, tokens[k]);
        });
    }
    applyTokens(boot.themeTokens);   // first paint is already themed (D6)

    // ---- events ----------------------------------------------------------
    var listeners = {};
    function emit(name, arg) {
        (listeners[name] || []).forEach(function (cb) {
            try { cb(arg); } catch (e) { console.error('prodigy: listener error', e); }
        });
    }

    var TOPIC = { media: 1, navigation: 2, projection: 3, phone: 4, system: 5 };
    var STATUS_FIELD = {
        mediaStatus: 'media', navigationStatus: 'navigation',
        projectionStatus: 'projection', phoneStatus: 'phone', systemStatus: 'system'
    };

    var ws = null;
    var nextRequestId = 1;
    var pending = {};                  // request_id -> {resolve, reject}
    var subs = {};                     // topic name -> [callback]
    var backoffMs = 1000;
    var readyResolve;
    var readyPromise = new Promise(function (res) { readyResolve = res; });

    function encode(fields) {
        return pb.ApiMessage.encode(pb.ApiMessage.create(fields)).finish();
    }
    function reqId(msg) {
        return msg.requestId && msg.requestId.toNumber
            ? msg.requestId.toNumber() : Number(msg.requestId || 0);
    }

    function request(fields) {
        return readyPromise.then(function () {
            return new Promise(function (resolve, reject) {
                var id = nextRequestId++;
                fields.requestId = id;
                pending[id] = { resolve: resolve, reject: reject };
                try { ws.send(encode(fields)); }
                catch (e) { delete pending[id]; reject(e); }
            });
        });
    }

    function activeTopics() {
        var t = [TOPIC.system];        // theme updates always flow
        Object.keys(subs).forEach(function (name) {
            if (subs[name].length && TOPIC[name] !== TOPIC.system)
                t.push(TOPIC[name]);
        });
        return t;
    }

    function handleStream(msg) {
        Object.keys(STATUS_FIELD).forEach(function (field) {
            if (!msg[field]) return;
            var topic = STATUS_FIELD[field];
            if (topic === 'system' && msg[field].themeTokens) {
                applyTokens(msg[field].themeTokens);
                emit('themechange', msg[field].themeTokens);
            }
            (subs[topic] || []).forEach(function (cb) {
                try { cb(msg[field]); } catch (e) { console.error('prodigy: subscriber error', e); }
            });
        });
    }

    function onFrame(ev) {
        var msg;
        try { msg = pb.ApiMessage.decode(new Uint8Array(ev.data)); }
        catch (e) { console.error('prodigy: undecodable frame', e); return; }

        if (msg.serverHello) {         // (re)connected
            backoffMs = 1000;
            ws.send(encode({ requestId: nextRequestId++,
                             subscribeRequest: { topics: activeTopics() } }));
            readyResolve();
            return;
        }
        var id = reqId(msg);
        if (id && pending[id]) {
            var p = pending[id];
            delete pending[id];
            if (msg.error)
                p.reject(new Error('api error ' + msg.error.code +
                                   (msg.error.message ? ': ' + msg.error.message : '')));
            else
                p.resolve(msg);
            return;
        }
        handleStream(msg);             // request_id 0 = stream event
    }

    function connect() {
        ws = new WebSocket(boot.apiUrl);
        ws.binaryType = 'arraybuffer';
        ws.onopen = function () {
            ws.send(encode({
                requestId: nextRequestId++,
                clientHello: {
                    requestedApiVersionMajor: 1,
                    requestedApiVersionMinor: 1,
                    clientName: (boot.context && boot.context.widgetId) || 'web-widget',
                    clientKind: pb.ClientKind.CLIENT_KIND_WEB_WIDGET
                }
            }));
        };
        ws.onmessage = onFrame;
        ws.onclose = function () {
            Object.keys(pending).forEach(function (id) {
                pending[id].reject(new Error('prodigy: connection closed'));
                delete pending[id];
            });
            setTimeout(connect, backoffMs);
            backoffMs = Math.min(backoffMs * 2, 30000);   // capped backoff (design §6)
        };
        ws.onerror = function () { /* onclose fires next */ };
    }

    window.prodigy = {
        get ready() { return readyPromise; },
        context: boot.context || {},
        apiUrl: boot.apiUrl,

        subscribe: function (topic, cb) {
            if (!(topic in TOPIC)) throw new Error('prodigy: unknown topic ' + topic);
            (subs[topic] = subs[topic] || []).push(cb);
            readyPromise.then(function () {
                try { ws.send(encode({ requestId: nextRequestId++,
                                       subscribeRequest: { topics: activeTopics() } })); }
                catch (e) { /* re-subscribe happens on reconnect */ }
            });
            return function unsubscribe() {
                var arr = subs[topic] || [];
                var i = arr.indexOf(cb);
                if (i >= 0) arr.splice(i, 1);
            };
        },

        dispatch: function (actionId, payload) {
            var req = { id: String(actionId) };
            if (payload !== undefined) req.payloadJson = JSON.stringify(payload);
            return request({ dispatchActionRequest: req }).then(function (msg) {
                return !!(msg.dispatchActionResponse && msg.dispatchActionResponse.dispatched);
            });
        },

        notify: function (message, opts) {
            opts = opts || {};
            var req = { kind: 1 /* TOAST */, message: String(message),
                        ttlMs: opts.ttlMs || 0 };
            if (opts.priority !== undefined) req.priority = opts.priority;
            return request({ postNotificationRequest: req }).then(function (msg) {
                return msg.postNotificationResponse
                    ? msg.postNotificationResponse.notificationId : '';
            });
        },

        request: request,              // low-level escape hatch (design §6)

        on: function (name, cb) {
            (listeners[name] = listeners[name] || []).push(cb);
        },

        // host-internal: WebWidgetHost pushes span changes (design §6 bootstrap)
        _updateContext: function (ctx) {
            window.prodigy.context = ctx;
            emit('contextchange', ctx);
        }
    };

    if (pb) connect();
    else console.error('prodigy: proto module missing — API bridge disabled');
})();
```

- [ ] **Step 2: Syntax-check + Node smoke against decode/encode**

```bash
node --check resources/web/prodigy.js    # expect: silence (syntax OK)
```
(Full behavior is exercised in Task 9's loopback/dev-page check and the Pi checklist — the shim needs a real WebSocket + DOM. Do not build a Node test harness; design §9's test strategy applies.)

- [ ] **Step 3: qrc + build + commit**

Add `<file>web/prodigy.js</file>` to `resources/resources.qrc`.

```bash
cd ~/builds/openauto-prodigy && make -j$(nproc) && ctest --output-on-failure
git add resources/web/prodigy.js resources/resources.qrc
git commit -m "feat(webwidget): prodigy.js shim — themed CSS vars, WS API client with correlation + reconnect"
```

---

### Task 8: WebWidgetHost.qml

**Files:**
- Create: `qml/widgets/WebWidgetHost.qml`
- Modify: `src/CMakeLists.txt` (`qt_add_qml_module` `QML_FILES` list, next to the widget block at ~:450-461: add `../qml/widgets/WebWidgetHost.qml`)

**Interfaces:**
- Consumes: `widgetContext` (`WidgetInstanceContext` — `instanceId, widgetId, colSpan, rowSpan, isCurrentPage, effectiveConfig` Q_PROPERTYs), `ConfigService.value("api.ws_port")` (root context property, Q_INVOKABLE), `ThemeService.themeTokenMap()` (Task 5 — VERIFY the QML context-property name: `grep -n "setContextProperty" src/main.cpp | grep -i theme`; use that exact name), qrc script URLs from Tasks 6/7.
- Produces: the descriptor-referenced host at `qrc:/OpenAutoProdigy/WebWidgetHost.qml` (Task 4's scanner URL).

- [ ] **Step 1: Write the component** — `qml/widgets/WebWidgetHost.qml`:

```qml
import QtQuick
import QtWebEngine

// Hosts one web widget package (design 2026-07-06-js-runtime §5).
// Lazy: the WebEngineView instantiates on first page visibility and stays
// alive afterwards (D4). Crash recovery per D5. Locked-down settings +
// same-origin-only navigation (§5, §7).
Item {
    id: hostRoot

    property QtObject widgetContext: null
    readonly property var effectiveCfg: widgetContext ? widgetContext.effectiveConfig : ({})
    readonly property string widgetUrl: effectiveCfg && effectiveCfg.url ? effectiveCfg.url : ""
    property bool everVisible: false
    property int retryCount: 0

    function maybeActivate() {
        if (widgetContext && widgetContext.isCurrentPage)
            everVisible = true
    }
    Component.onCompleted: maybeActivate()
    onWidgetContextChanged: maybeActivate()
    Connections {
        target: hostRoot.widgetContext
        function onIsCurrentPageChanged() { hostRoot.maybeActivate() }
        function onColSpanChanged() { hostRoot.pushContext() }
        function onRowSpanChanged() { hostRoot.pushContext() }
    }

    function contextObject() {
        return {
            instanceId: widgetContext ? widgetContext.instanceId : "",
            widgetId: widgetContext ? widgetContext.widgetId : "",
            colSpan: widgetContext ? widgetContext.colSpan : 1,
            rowSpan: widgetContext ? widgetContext.rowSpan : 1,
            kind: "widget"
        }
    }
    function bootstrapSource() {
        var boot = {
            apiUrl: "ws://127.0.0.1:" + ConfigService.value("api.ws_port"),
            context: contextObject(),
            themeTokens: ThemeService.themeTokenMap()
        }
        return "window.__prodigyBootstrap = " + JSON.stringify(boot) + ";"
    }
    function pushContext() {
        if (viewLoader.item)
            viewLoader.item.runJavaScript(
                "window.prodigy && prodigy._updateContext("
                + JSON.stringify(contextObject()) + ")")
    }

    Loader {
        id: viewLoader
        anchors.fill: parent
        active: hostRoot.everVisible && hostRoot.widgetUrl !== ""
        sourceComponent: WebEngineView {
            backgroundColor: "transparent"
            settings.javascriptCanOpenWindows: false
            settings.localContentCanAccessFileUrls: false
            settings.localContentCanAccessRemoteUrls: true   // https subresources OK (§5)

            Component.onCompleted: {
                var bs = WebEngine.script()
                bs.name = "prodigy-bootstrap"
                bs.injectionPoint = WebEngineScript.DocumentCreation
                bs.worldId = WebEngineScript.MainWorld
                bs.sourceCode = hostRoot.bootstrapSource()

                var rt = WebEngine.script()
                rt.name = "protobuf-runtime"
                rt.injectionPoint = WebEngineScript.DocumentCreation
                rt.worldId = WebEngineScript.MainWorld
                rt.sourceUrl = "qrc:/web/protobuf.min.js"

                var gen = WebEngine.script()
                gen.name = "prodigy-proto"
                gen.injectionPoint = WebEngineScript.DocumentCreation
                gen.worldId = WebEngineScript.MainWorld
                gen.sourceUrl = "qrc:/web/prodigy-proto.js"

                var shim = WebEngine.script()
                shim.name = "prodigy-shim"
                shim.injectionPoint = WebEngineScript.DocumentCreation
                shim.worldId = WebEngineScript.MainWorld
                shim.sourceUrl = "qrc:/web/prodigy.js"

                userScripts.collection = [bs, rt, gen, shim]
                url = hostRoot.widgetUrl
            }

            onRenderProcessTerminated: function (terminationStatus, exitCode) {
                if (hostRoot.retryCount >= 3) {        // D5: 3 attempts then error card
                    errorCard.visible = true
                    return
                }
                hostRoot.retryCount += 1
                reloadTimer.interval = 1000 * Math.pow(2, hostRoot.retryCount) // 2s/4s/8s
                reloadTimer.start()
            }
            onLoadingChanged: function (loadingInfo) {
                if (loadingInfo.status === WebEngineView.LoadSucceededStatus) {
                    hostRoot.retryCount = 0
                    errorCard.visible = false
                }
            }
            onNavigationRequested: function (request) {
                // Same-origin top-level navigation only (§5).
                if (request.url.toString().indexOf("prodigy://widgets/") !== 0)
                    request.action = WebEngineNavigationRequest.IgnoreRequest
            }
        }
    }

    Timer {
        id: reloadTimer
        repeat: false
        onTriggered: if (viewLoader.item) viewLoader.item.reload()
    }

    Rectangle {
        id: errorCard
        anchors.fill: parent
        visible: false
        radius: 8
        color: ThemeService.surfaceContainerHigh   // match sibling widget styling —
                                                   // verify property names against an
                                                   // existing widget QML before building
        Column {
            anchors.centerIn: parent
            spacing: 8
            Text {
                text: hostRoot.widgetContext ? hostRoot.widgetContext.widgetId : "Web widget"
                color: ThemeService.onSurface
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: qsTr("Failed to load — tap to retry")
                color: ThemeService.onSurfaceVariant
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                hostRoot.retryCount = 0
                errorCard.visible = false
                if (viewLoader.item) viewLoader.item.reload()
            }
        }
    }
}
```

Execution notes (verify, don't guess):
- The `WebEngine.script()` factory + `userScripts.collection` assignment is the Qt 6 QML API — confirm against the installed Qt 6.8 docs (`/usr/lib/x86_64-linux-gnu/qt6/qml/QtWebEngine/plugins.qmltypes`, grep `script`/`collection`). If the collection property differs, use `userScripts.insert([bs, rt, gen, shim])`.
- `ThemeService`/`ConfigService` context-property names and the theme color property names (`surfaceContainerHigh` etc.): copy from an existing sibling widget QML (e.g. `qml/widgets/BatteryWidget.qml` or any themed card) — match, don't invent.
- qrc URLs must match Task 6/7's recorded URLs.

- [ ] **Step 2: Register in the QML module** — `src/CMakeLists.txt` `QML_FILES` (widget block ~:450-461): add `../qml/widgets/WebWidgetHost.qml`.

- [ ] **Step 3: Build + full suite + offscreen boot smoke** (host not instantiated without packages — smoke proves no QML-module regression):

```bash
cd ~/builds/openauto-prodigy && cmake . && make -j$(nproc) && ctest --output-on-failure
QT_QPA_PLATFORM=offscreen timeout 15 ./src/openauto-prodigy --verbose ; echo "exit=$?"
```

- [ ] **Step 4: Commit**

```bash
git add qml/widgets/WebWidgetHost.qml src/CMakeLists.txt
git commit -m "feat(webwidget): WebWidgetHost.qml — lazy locked-down WebEngineView with shim injection + crash recovery"
```

---

### Task 9: Example widget, WSL live check, docs, Pi checklist

**Files:**
- Create: `examples/webwidgets/hello-theme/widget.yaml`, `examples/webwidgets/hello-theme/index.html`
- Modify: `docs/development.md` (WebEngine dev packages note), `docs/INDEX.md` if it indexes plans/specs
- Append: handoff entry to `docs/session-handoffs.md` (per executor handbook §6)

**Interfaces:**
- Consumes: everything. This is the integration gate.

- [ ] **Step 1: Example package** — `examples/webwidgets/hello-theme/widget.yaml`:

```yaml
id: org.openauto.example.hello-theme
name: "Hello Theme"
entry: index.html
category: status
description: "Reference web widget: themed card, live media title, action dispatch"
size:
  defaultCols: 2
  defaultRows: 2
```

`examples/webwidgets/hello-theme/index.html`:

```html
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
  body {
    margin: 0; padding: 12px;
    background: transparent;
    color: var(--prodigy-on-surface, #eee);
    font-family: sans-serif;
  }
  .card {
    background: var(--prodigy-surface-container-high, #333);
    border: 1px solid var(--prodigy-outline-variant, #555);
    border-radius: 10px; padding: 12px;
  }
  h3 { margin: 0 0 6px 0; color: var(--prodigy-primary, #8cf); }
  button {
    background: var(--prodigy-primary-container, #246);
    color: var(--prodigy-on-primary-container, #fff);
    border: none; border-radius: 6px; padding: 8px 12px; margin-top: 8px;
  }
</style>
</head>
<body>
<div class="card">
  <h3>Hello Theme</h3>
  <div id="status">connecting…</div>
  <div id="track"></div>
  <button id="pp">Play / Pause</button>
</div>
<script>
  prodigy.ready.then(function () {
    document.getElementById('status').textContent =
      'connected as ' + prodigy.context.widgetId;
  });
  prodigy.subscribe('media', function (m) {
    document.getElementById('track').textContent =
      (m.title || '(no title)') + (m.artist ? ' — ' + m.artist : '');
  });
  prodigy.on('themechange', function () {
    // CSS vars update automatically; hook shown for completeness
  });
  document.getElementById('pp').addEventListener('click', function () {
    prodigy.dispatch('media.playPause');
  });
</script>
</body>
</html>
```
(Field names inside the `media` status object are protobufjs camelCase of `media.proto` — verify `title`/`artist` against `proto/api/media.proto` and adjust.)

- [ ] **Step 2: WSL live check (real WebEngine, real API server)** — the dev box has WebEngine + a display path via WSLg, and the API listens on loopback:

```bash
mkdir -p ~/.openauto/webwidgets
cp -r examples/webwidgets/hello-theme ~/.openauto/webwidgets/
cd ~/builds/openauto-prodigy && ./src/openauto-prodigy --verbose 2>&1 | tee /tmp/webwidget-live.log
# In the UI: add "Hello Theme" from the widget picker; verify:
#   - card renders themed (CSS vars applied pre-connect)
#   - status flips to "connected as org.openauto.example.hello-theme"
#   - day/night toggle re-colors the card live (themechange path)
# Then: rm -r ~/.openauto/webwidgets/hello-theme (leave dev box clean)
```
Log check: `grep -E "Registered 1 web widget|prodigy" /tmp/webwidget-live.log`. If WSLg display is unavailable, record that this check moves to the Pi checklist — do not fake it.

- [ ] **Step 3: Docs**
  - `docs/development.md`: add `qt6-webengine-dev` + `qml6-module-qtwebengine` to the dev dependency list with one line: "optional — web widget runtime; builds fine without".
  - Append the handoff entry (what shipped, deviations, the §6.4 dev-auth deferral, Pi checklist below).

- [ ] **Step 4: Pi manual checklist** (record in the handoff; run on next Pi session):

```
1. Deploy: sg docker -c ./cross-build.sh   # config log MUST print "WebEngineQuick found"
   rsync + restart per CLAUDE.md; git pull on Pi for QML.
2. scp -r examples/webwidgets/hello-theme matt@192.168.1.149:~/.openauto/webwidgets/
3. Restart service; journal shows "Registered 1 web widget(s)".
4. Add Hello Theme from picker → themed card, "connected as ...", media title
   updates during BT playback; Play/Pause button works (action dispatch).
5. Crash recovery: ssh matt@192.168.1.149 'pkill -x QtWebEngineProcess'
   → card auto-reloads within ~4s (D5), launcher unaffected.
6. Day/night flip in settings → card re-colors within one frame.
7. Resize the widget (edit mode) → prodigy.context spans update
   (verify via a temporary on-page span readout or remote devtools).
8. Memory: ps/smem PSS for QtWebEngineProcess ≈ spike budget (≤350 MB total).
```

- [ ] **Step 5: Final gate + commit**

```bash
cd ~/builds/openauto-prodigy && cmake . && make -j$(nproc) && ctest --output-on-failure
sg docker -c "./cross-build.sh"   # from the repo root; expect WebEngineQuick found
git add examples/ docs/
git commit -m "feat(webwidget): hello-theme reference package + docs + Pi checklist"
```

---

## Self-Review (author, 2026-07-06)

1. **Spec coverage:** D1 scheme (T1/T3) ✓, D2 single origin (T3 resolver + host nav policy) ✓, D3 packaging/scanner (T2/T4) ✓, D4 lazy host (T8) ✓, D5 crash recovery (T8) ✓, D6 two-script shim — implemented as bootstrap + 3 static scripts (runtime/generated/shim), a faithful decomposition of "bootstrap + prodigy.js" given D7's committed protobufjs artifacts ✓, D7 toolchain (T6) ✓, D8 widgets-only (kind:"widget" hardcoded in host) ✓. §6.4 dev-auth branch consciously DEFERRED (header + Global Constraints). §5 fullscreen-deny: WebEngineView denies fullscreen by default when no handler accepts the request — no `onFullScreenRequested` accept exists in the host; nothing to add.
2. **Placeholder scan:** all "verify, don't guess" notes point at exact files/greps with a stated fallback — no TBDs.
3. **Type consistency:** `WebWidgetManifest` fields (T2) match scanner usage (T4); resolver signature (T3) matches handler + scanner + main wiring; `themeTokenMap()` (T5) matches host bootstrap (T8); qrc URLs (T6/T7) match host script URLs (T8); `prodigy._updateContext` (T7) matches host `pushContext()` (T8).
