# Theme/Wallpaper Upload Endpoint Implementation Plan

Status: COMPLETED 2026-07-07

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `POST /api/theme/install` web-config endpoint that accepts a companion-app theme (manifest JSON + optional wallpaper JPEG), validates it, and applies it via the existing `ThemeService::importCompanionTheme()` — moving companion theme transfer off legacy port 9876.

**Architecture:** Flask receives the multipart upload, writes the wallpaper to a temp file, and passes the file *path* (not the bytes) plus the theme JSON over the existing unix-socket IPC as an `install_theme` command. A new, pure `parseThemeInstall()` module does authoritative validation (name, color schemes with camelCase→hyphen conversion, wallpaper size/JPEG-magic/canonical-path); a thin `IpcServer::handleInstallTheme` handler wires it to `importCompanionTheme()`. All heavy logic lives in a unit-tested pure function; the handler and Flask route are thin glue.

**Tech Stack:** C++/Qt 6 (QtCore, QtGui `QColor`), Flask (Python 3), QTest, Python `unittest` + Flask test client.

**Approved design:** `docs/superpowers/specs/2026-07-07-theme-upload-design.md` — read §4 (HTTP contract), §6 (IPC command), §7 (ThemeService), §8 (validation matrix) before starting.

## Global Constraints

- **Branch:** all work directly on `develop` (project single-branch workflow). Commit atomically per task; the orchestrator pushes after each task's review.
- **Build dir:** `~/builds/openauto-prodigy` (ext4 + ccache) — NOT in-tree `build/`. Build: `cmake --build ~/builds/openauto-prodigy -j$(nproc)`. Test: `ctest --test-dir ~/builds/openauto-prodigy --output-on-failure`.
- **Baseline:** 107 tests green at develop @ 81cfd8e. Each C++ task adds tests; the suite count only grows.
- **Frozen — do NOT touch:** `src/core/services/CompanionListenerService.cpp` (legacy 9876, dual-stack), `proto/api/`, `libs/prodigy-oaa-protocol/`.
- **No Pi deploy** in this plan (Matthew hardware-gates). Runtime end-to-end lands via a later deploy.
- **Wallpaper cap:** 5 MiB = `5 * 1024 * 1024` bytes. **JPEG magic:** first three bytes `FF D8 FF`. **Name:** 1–64 chars. **Temp upload dir:** `/tmp/oap-theme-upload` (mode 0700).
- **Commit trailer:** end every commit message with `Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>`.

## File Structure

- **Create** `src/core/services/ThemeInstallRequest.hpp` — structs + `parseThemeInstall()` declaration (no Q_OBJECT; free functions).
- **Create** `src/core/services/ThemeInstallRequest.cpp` — validation/conversion implementation.
- **Modify** `src/core/services/ThemeService.hpp` / `.cpp` — extract `static QString slugify(const QString&)`; `importCompanionTheme` calls it (behavior-preserving).
- **Modify** `src/core/services/IpcServer.hpp` / `.cpp` — `install_theme` dispatch + `handleInstallTheme`.
- **Modify** `src/CMakeLists.txt` — add `core/services/ThemeInstallRequest.cpp` to the `openauto-core` sources.
- **Modify** `web-config/server.py` — `POST /api/theme/install` route, `MAX_CONTENT_LENGTH`, 413 handler.
- **Create** `tests/test_theme_install_request.cpp` — pure-logic unit tests (validation matrix + slugify).
- **Create** `tests/test_ipc_install_theme.cpp` — socket round-trip test proving dispatch + framing.
- **Create** `web-config/test_server.py` — Flask route tests (status mapping, 413, 503, temp-file cleanup).
- **Modify** `tests/CMakeLists.txt` — register the two new C++ tests.

---

### Task 1: Pure validation layer + `slugify` extraction

**Files:**
- Create: `src/core/services/ThemeInstallRequest.hpp`
- Create: `src/core/services/ThemeInstallRequest.cpp`
- Modify: `src/core/services/ThemeService.hpp` (add `static QString slugify(const QString&)` near line 137, before `importCompanionTheme`)
- Modify: `src/core/services/ThemeService.cpp` (extract slugify from `importCompanionTheme`, lines 511-517)
- Modify: `src/CMakeLists.txt` (add source near line 47)
- Test: `tests/test_theme_install_request.cpp` (create)
- Modify: `tests/CMakeLists.txt` (register test near the `test_theme_service` block, ~line 47)

**Interfaces:**
- Produces:
  - `oap::ThemeInstallRequest { QString name; QString seed; QMap<QString,QColor> dayColors; QMap<QString,QColor> nightColors; QByteArray wallpaperJpeg; }`
  - `oap::ThemeInstallParseResult { bool ok; QString error; ThemeInstallRequest request; }`
  - `oap::ThemeInstallParseResult oap::parseThemeInstall(const QVariantMap& data, const QString& allowedWallpaperDir, qint64 maxWallpaperBytes = 5*1024*1024)`
  - `static QString oap::ThemeService::slugify(const QString& name)`

- [ ] **Step 1: Write the header**

Create `src/core/services/ThemeInstallRequest.hpp`:
```cpp
#pragma once

#include <QByteArray>
#include <QColor>
#include <QMap>
#include <QString>
#include <QVariantMap>

namespace oap {

/// Validated, normalized inputs for a theme install (ready for ThemeService::importCompanionTheme).
struct ThemeInstallRequest {
    QString name;
    QString seed;
    QMap<QString, QColor> dayColors;    // from manifest "light", keys converted camelCase -> hyphenated
    QMap<QString, QColor> nightColors;  // from manifest "dark"
    QByteArray wallpaperJpeg;           // empty when no wallpaper supplied
};

struct ThemeInstallParseResult {
    bool ok = false;
    QString error;               // human-readable reason when !ok
    ThemeInstallRequest request; // populated when ok
};

/// Validate + normalize an `install_theme` IPC payload.
///
/// `data` keys: "name" (1-64 chars), "seed" (optional), "light"/"dark"
/// (non-empty maps of camelCase-role -> hex color), "wallpaper_path" (optional).
/// When "wallpaper_path" is present it must canonicalize strictly under
/// `allowedWallpaperDir`, be a regular file, decode to <= maxWallpaperBytes,
/// and start with the JPEG magic bytes FF D8 FF.
ThemeInstallParseResult parseThemeInstall(const QVariantMap& data,
                                          const QString& allowedWallpaperDir,
                                          qint64 maxWallpaperBytes = 5 * 1024 * 1024);

} // namespace oap
```

- [ ] **Step 2: Write the failing tests**

Create `tests/test_theme_install_request.cpp`:
```cpp
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "core/services/ThemeInstallRequest.hpp"
#include "core/services/ThemeService.hpp"

using namespace oap;

// A minimal valid light/dark scheme for tests.
static QVariantMap validScheme() { return QVariantMap{{"primary", "#112233"}, {"onPrimary", "#ffffff"}}; }

class TestThemeInstallRequest : public QObject {
    Q_OBJECT

    QString writeJpeg(const QString& dir, const QByteArray& bytes) {
        const QString p = dir + "/wp.jpg";
        QFile f(p); f.open(QIODevice::WriteOnly); f.write(bytes); f.close();
        return p;
    }
    static QByteArray jpeg(int extra = 16) { return QByteArray("\xff\xd8\xff", 3) + QByteArray(extra, '\0'); }

private slots:
    void happyColorOnly() {
        QVariantMap d{{"name", "Sunset Vibes"}, {"seed", "#ff8a65"},
                      {"light", validScheme()}, {"dark", validScheme()}};
        auto r = parseThemeInstall(d, "/tmp/oap-theme-upload");
        QVERIFY(r.ok);
        QCOMPARE(r.request.name, QString("Sunset Vibes"));
        QVERIFY(r.request.wallpaperJpeg.isEmpty());
    }

    void camelToHyphenKeys() {
        QVariantMap d{{"name", "X"},
                      {"light", QVariantMap{{"onPrimary", "#010101"}, {"surfaceContainerHigh", "#020202"}}},
                      {"dark", validScheme()}};
        auto r = parseThemeInstall(d, "/tmp/oap-theme-upload");
        QVERIFY(r.ok);
        QVERIFY(r.request.dayColors.contains("on-primary"));
        QVERIFY(r.request.dayColors.contains("surface-container-high"));
        QVERIFY(!r.request.dayColors.contains("onPrimary"));
    }

    void rejectsEmptyName() {
        QVariantMap d{{"name", "   "}, {"light", validScheme()}, {"dark", validScheme()}};
        QVERIFY(!parseThemeInstall(d, "/tmp/oap-theme-upload").ok);
    }

    void rejectsLongName() {
        QVariantMap d{{"name", QString(65, 'a')}, {"light", validScheme()}, {"dark", validScheme()}};
        QVERIFY(!parseThemeInstall(d, "/tmp/oap-theme-upload").ok);
    }

    void rejectsEmptyLight() {
        QVariantMap d{{"name", "X"}, {"light", QVariantMap{}}, {"dark", validScheme()}};
        QVERIFY(!parseThemeInstall(d, "/tmp/oap-theme-upload").ok);
    }

    void rejectsEmptyDark() {
        QVariantMap d{{"name", "X"}, {"light", validScheme()}, {"dark", QVariantMap{}}};
        QVERIFY(!parseThemeInstall(d, "/tmp/oap-theme-upload").ok);
    }

    void rejectsInvalidColor() {
        QVariantMap d{{"name", "X"}, {"light", QVariantMap{{"primary", "not-a-color"}}}, {"dark", validScheme()}};
        QVERIFY(!parseThemeInstall(d, "/tmp/oap-theme-upload").ok);
    }

    void wallpaperHappy() {
        QTemporaryDir dir;
        const QString p = writeJpeg(dir.path(), jpeg());
        QVariantMap d{{"name", "X"}, {"light", validScheme()}, {"dark", validScheme()}, {"wallpaper_path", p}};
        auto r = parseThemeInstall(d, dir.path());
        QVERIFY(r.ok);
        QCOMPARE(r.request.wallpaperJpeg.size(), jpeg().size());
    }

    void wallpaperTooLarge() {
        QTemporaryDir dir;
        const QString p = writeJpeg(dir.path(), jpeg(64));
        QVariantMap d{{"name", "X"}, {"light", validScheme()}, {"dark", validScheme()}, {"wallpaper_path", p}};
        auto r = parseThemeInstall(d, dir.path(), /*maxWallpaperBytes*/ 8);  // 3+64 > 8
        QVERIFY(!r.ok);
    }

    void wallpaperBadMagic() {
        QTemporaryDir dir;
        const QString p = writeJpeg(dir.path(), QByteArray("\x89PNG", 4));
        QVariantMap d{{"name", "X"}, {"light", validScheme()}, {"dark", validScheme()}, {"wallpaper_path", p}};
        QVERIFY(!parseThemeInstall(d, dir.path()).ok);
    }

    void wallpaperPathOutsideAllowedDir() {
        QTemporaryDir allowed, elsewhere;
        const QString p = writeJpeg(elsewhere.path(), jpeg());
        QVariantMap d{{"name", "X"}, {"light", validScheme()}, {"dark", validScheme()}, {"wallpaper_path", p}};
        QVERIFY(!parseThemeInstall(d, allowed.path()).ok);   // path is under a different dir
    }

    void wallpaperPathIsDirectory() {
        QTemporaryDir dir;
        QVariantMap d{{"name", "X"}, {"light", validScheme()}, {"dark", validScheme()}, {"wallpaper_path", dir.path()}};
        QVERIFY(!parseThemeInstall(d, dir.path()).ok);       // not a regular file
    }

    // --- slugify (extracted from importCompanionTheme; behavior-preserving) ---
    void slugifyBasic()      { QCOMPARE(ThemeService::slugify("Sunset Vibes"), QString("sunset-vibes")); }
    void slugifyPunctuation(){ QCOMPARE(ThemeService::slugify("  Hello!! World  "), QString("hello-world")); }
    void slugifyEmpty()      { QCOMPARE(ThemeService::slugify(""), QString("companion-theme")); }
    void slugifyAllPunct()   { QCOMPARE(ThemeService::slugify("---"), QString("companion-theme")); }

    // Refactor guard: importCompanionTheme's created dir must match slugify(name).
    void importUsesSlugify() {
        QTemporaryDir themes;
        ThemeService svc;
        svc.scanThemeDirectories({themes.path()});
        QMap<QString, QColor> day{{"primary", QColor("#112233")}};
        QMap<QString, QColor> night{{"primary", QColor("#445566")}};
        QVERIFY(svc.importCompanionTheme("Sunset Vibes", "#ff8a65", day, night, QByteArray()));
        QVERIFY(QFile::exists(themes.path() + "/sunset-vibes/theme.yaml"));
    }
};

QTEST_MAIN(TestThemeInstallRequest)
#include "test_theme_install_request.moc"
```

- [ ] **Step 3: Register source + test in CMake, then build to see it fail**

In `src/CMakeLists.txt`, add the source next to `core/services/ThemeService.cpp` (~line 47):
```cmake
    core/services/ThemeInstallRequest.cpp
```
In `tests/CMakeLists.txt`, after the `test_theme_service` block (~line 47), add:
```cmake
oap_add_test(test_theme_install_request SOURCES test_theme_install_request.cpp)
```
Run: `cmake --build ~/builds/openauto-prodigy -j$(nproc)`
Expected: **FAIL** — linker error `undefined reference to oap::parseThemeInstall(...)` and `oap::ThemeService::slugify(...)` (implementations don't exist yet). (You must re-run CMake configure first if the build dir doesn't pick up the new files: `cmake -S . -B ~/builds/openauto-prodigy` — reuses cache.)

- [ ] **Step 4: Implement `parseThemeInstall`**

Create `src/core/services/ThemeInstallRequest.cpp`:
```cpp
#include "core/services/ThemeInstallRequest.hpp"

#include <QFile>
#include <QFileInfo>

namespace oap {

// Companion sends camelCase M3 role names; theme.yaml uses hyphenated keys.
// Insert a hyphen before each uppercase letter and lowercase it (matches the
// legacy CompanionListenerService::applyReceivedTheme conversion exactly).
static QString camelToHyphen(const QString& in) {
    QString out;
    for (QChar ch : in) {
        if (ch.isUpper()) { out += '-'; out += ch.toLower(); }
        else              { out += ch; }
    }
    return out;
}

static bool parseColorMap(const QVariantMap& in, QMap<QString, QColor>& out, QString& err) {
    if (in.isEmpty()) { err = QStringLiteral("empty color scheme"); return false; }
    for (auto it = in.constBegin(); it != in.constEnd(); ++it) {
        const QColor c(it.value().toString());
        if (!c.isValid()) { err = QStringLiteral("invalid color for '%1'").arg(it.key()); return false; }
        out.insert(camelToHyphen(it.key()), c);
    }
    return true;
}

ThemeInstallParseResult parseThemeInstall(const QVariantMap& data,
                                          const QString& allowedWallpaperDir,
                                          qint64 maxWallpaperBytes) {
    ThemeInstallParseResult r;
    ThemeInstallRequest& req = r.request;

    req.name = data.value(QStringLiteral("name")).toString().trimmed();
    if (req.name.isEmpty() || req.name.size() > 64) {
        r.error = QStringLiteral("name must be 1-64 characters");
        return r;
    }
    req.seed = data.value(QStringLiteral("seed")).toString();

    QString err;
    if (!parseColorMap(data.value(QStringLiteral("light")).toMap(), req.dayColors, err)) {
        r.error = QStringLiteral("light: ") + err;
        return r;
    }
    if (!parseColorMap(data.value(QStringLiteral("dark")).toMap(), req.nightColors, err)) {
        r.error = QStringLiteral("dark: ") + err;
        return r;
    }

    const QString wp = data.value(QStringLiteral("wallpaper_path")).toString();
    if (!wp.isEmpty()) {
        const QFileInfo fi(wp);
        const QString canon = fi.canonicalFilePath();
        const QString allowedCanon = QFileInfo(allowedWallpaperDir).canonicalFilePath();
        // Strict containment: must be a regular file whose canonical path lives
        // directly under the canonical allowed dir (trailing '/' blocks sibling-prefix escapes).
        if (canon.isEmpty() || allowedCanon.isEmpty()
            || !canon.startsWith(allowedCanon + QLatin1Char('/'))
            || !fi.isFile()) {
            r.error = QStringLiteral("wallpaper path is invalid");
            return r;
        }
        QFile f(canon);
        if (!f.open(QIODevice::ReadOnly)) {
            r.error = QStringLiteral("cannot read wallpaper");
            return r;
        }
        const QByteArray bytes = f.readAll();
        f.close();
        if (bytes.size() > maxWallpaperBytes) {
            r.error = QStringLiteral("wallpaper too large");
            return r;
        }
        if (bytes.size() < 3
            || static_cast<unsigned char>(bytes[0]) != 0xFF
            || static_cast<unsigned char>(bytes[1]) != 0xD8
            || static_cast<unsigned char>(bytes[2]) != 0xFF) {
            r.error = QStringLiteral("wallpaper is not a JPEG");
            return r;
        }
        req.wallpaperJpeg = bytes;
    }

    r.ok = true;
    return r;
}

} // namespace oap
```

- [ ] **Step 5: Extract `slugify` on ThemeService**

In `src/core/services/ThemeService.hpp`, add above the `importCompanionTheme` declaration (~line 135):
```cpp
    /// Convert a display name into a theme id: lowercase, collapse runs of
    /// non-alphanumerics to '-', trim leading/trailing '-'; empty -> "companion-theme".
    static QString slugify(const QString& name);
```
In `src/core/services/ThemeService.cpp`, add the definition (e.g. just above `importCompanionTheme`, before line 505):
```cpp
QString ThemeService::slugify(const QString& name)
{
    QString slug = name.toLower().trimmed();
    static const QRegularExpression nonAlnum("[^a-z0-9]+");
    slug.replace(nonAlnum, "-");
    while (slug.startsWith('-')) slug.remove(0, 1);
    while (slug.endsWith('-')) slug.chop(1);
    if (slug.isEmpty()) slug = "companion-theme";
    return slug;
}
```
Then replace the inline slug block inside `importCompanionTheme` (`ThemeService.cpp:511-517` — the `QString slug = name.toLower()...` through `if (slug.isEmpty()) slug = "companion-theme";`) with:
```cpp
    QString slug = slugify(name);
```
(`QRegularExpression` is already included in ThemeService.cpp — the moved `static` regex compiles once as before.)

- [ ] **Step 6: Build + run the new test**

Run: `cmake --build ~/builds/openauto-prodigy -j$(nproc)`
Expected: clean compile.
Run: `ctest --test-dir ~/builds/openauto-prodigy --output-on-failure -R test_theme_install_request`
Expected: **PASS** (all slots). Then run the full suite:
Run: `ctest --test-dir ~/builds/openauto-prodigy --output-on-failure`
Expected: **108/108** (107 baseline + 1 new test executable). No regressions.

- [ ] **Step 7: Commit**

```bash
git add src/core/services/ThemeInstallRequest.hpp src/core/services/ThemeInstallRequest.cpp \
        src/core/services/ThemeService.hpp src/core/services/ThemeService.cpp \
        src/CMakeLists.txt tests/test_theme_install_request.cpp tests/CMakeLists.txt
git commit -m "feat(theme): pure theme-install validation module + ThemeService::slugify extraction

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: IpcServer `install_theme` wiring + socket round-trip test

**Files:**
- Modify: `src/core/services/IpcServer.hpp` (declare `QByteArray handleInstallTheme(const QVariantMap&)` in the private handler group, ~line 49)
- Modify: `src/core/services/IpcServer.cpp` (dispatch entry ~line 133; new handler; include `ThemeInstallRequest.hpp`)
- Test: `tests/test_ipc_install_theme.cpp` (create)
- Modify: `tests/CMakeLists.txt` (register)

**Interfaces:**
- Consumes: `oap::parseThemeInstall(...)`, `oap::ThemeService::slugify(...)`, `oap::ThemeService::importCompanionTheme(...)` (Task 1).
- Produces: IPC command `install_theme` → response `{"ok":true,"slug":"<slug>"}` or `{"ok":false,"error":"<reason>"}`.

- [ ] **Step 1: Write the failing round-trip test**

Create `tests/test_ipc_install_theme.cpp`:
```cpp
#include <QTest>
#include <QTemporaryDir>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>

#include "core/services/IpcServer.hpp"
#include "core/services/ThemeService.hpp"

using namespace oap;

class TestIpcInstallTheme : public QObject {
    Q_OBJECT

    // Send one newline-framed request, return the parsed JSON response object.
    QJsonObject roundTrip(const QString& socketPath, const QJsonObject& request) {
        QLocalSocket sock;
        sock.connectToServer(socketPath);
        if (!sock.waitForConnected(2000)) return {};
        sock.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n");
        sock.flush();
        if (!sock.waitForReadyRead(2000)) return {};
        const QByteArray buf = sock.readAll();
        sock.disconnectFromServer();
        return QJsonDocument::fromJson(buf.trimmed()).object();
    }

private slots:
    void installsColorOnlyTheme() {
        QTemporaryDir themes, sockDir;
        ThemeService svc;
        svc.scanThemeDirectories({themes.path()});
        IpcServer server;
        server.setThemeService(&svc);
        const QString sockPath = sockDir.path() + "/ipc.sock";
        QVERIFY(server.start(sockPath));

        QJsonObject light{{"primary", "#112233"}, {"onPrimary", "#ffffff"}};
        QJsonObject dark{{"primary", "#445566"}};
        QJsonObject req{
            {"command", "install_theme"},
            {"data", QJsonObject{{"name", "Sunset Vibes"}, {"seed", "#ff8a65"},
                                 {"light", light}, {"dark", dark}}}};
        const QJsonObject resp = roundTrip(sockPath, req);
        QVERIFY(resp.value("ok").toBool());
        QCOMPARE(resp.value("slug").toString(), QString("sunset-vibes"));
        QVERIFY(QFile::exists(themes.path() + "/sunset-vibes/theme.yaml"));
    }

    void rejectsInvalidPayload() {
        QTemporaryDir themes, sockDir;
        ThemeService svc;
        svc.scanThemeDirectories({themes.path()});
        IpcServer server;
        server.setThemeService(&svc);
        const QString sockPath = sockDir.path() + "/ipc.sock";
        QVERIFY(server.start(sockPath));

        QJsonObject req{
            {"command", "install_theme"},
            {"data", QJsonObject{{"name", ""}, {"light", QJsonObject{{"primary", "#111111"}}},
                                 {"dark", QJsonObject{{"primary", "#222222"}}}}}};
        const QJsonObject resp = roundTrip(sockPath, req);
        QVERIFY(!resp.value("ok").toBool());
        QVERIFY(!resp.value("error").toString().isEmpty());
    }
};

QTEST_MAIN(TestIpcInstallTheme)
#include "test_ipc_install_theme.moc"
```
Register in `tests/CMakeLists.txt` (near the other theme test):
```cmake
oap_add_test(test_ipc_install_theme SOURCES test_ipc_install_theme.cpp)
```
Run: `cmake -S . -B ~/builds/openauto-prodigy && cmake --build ~/builds/openauto-prodigy -j$(nproc) && ctest --test-dir ~/builds/openauto-prodigy -R test_ipc_install_theme --output-on-failure`
Expected: **FAIL** — the server returns `{"error":"Unknown command"}` (no `install_theme` dispatch yet), so `ok` is false and `slug` is absent → `installsColorOnlyTheme` fails.

- [ ] **Step 2: Declare the handler**

In `src/core/services/IpcServer.hpp`, add to the private handler group (after `handleSetTheme`, ~line 49):
```cpp
    QByteArray handleInstallTheme(const QVariantMap& data);
```

- [ ] **Step 3: Add dispatch + implement the handler**

In `src/core/services/IpcServer.cpp`, add the include near the top with the other core includes:
```cpp
#include "core/services/ThemeInstallRequest.hpp"
```
Add the dispatch line in `handleRequest`, right after the `set_theme` entry (~line 133):
```cpp
    if (command == QLatin1String("install_theme"))
        return handleInstallTheme(data);
```
Add the handler (place it after `handleSetTheme`, ~line 279). Note `QJsonObject`/`QJsonDocument` are already included/used in this file (`handleGetConfig`):
```cpp
QByteArray IpcServer::handleInstallTheme(const QVariantMap& data)
{
    if (!themeService_)
        return R"({"ok":false,"error":"Theme service not available"})";

    // Temp dir Flask writes the wallpaper into; the path in `data` must resolve here.
    const QString uploadDir = QStringLiteral("/tmp/oap-theme-upload");
    const ThemeInstallParseResult res = parseThemeInstall(data, uploadDir);
    if (!res.ok) {
        const QJsonObject o{{"ok", false}, {"error", res.error}};
        return QJsonDocument(o).toJson(QJsonDocument::Compact);
    }

    const ThemeInstallRequest& req = res.request;
    const bool ok = themeService_->importCompanionTheme(
        req.name, req.seed, req.dayColors, req.nightColors, req.wallpaperJpeg);

    QJsonObject o;
    o["ok"] = ok;
    if (ok)
        o["slug"] = ThemeService::slugify(req.name);
    else
        o["error"] = QStringLiteral("theme import failed");
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}
```

- [ ] **Step 4: Build + run**

Run: `cmake --build ~/builds/openauto-prodigy -j$(nproc)`
Expected: clean compile.
Run: `ctest --test-dir ~/builds/openauto-prodigy -R test_ipc_install_theme --output-on-failure`
Expected: **PASS** (both slots).
Run: `ctest --test-dir ~/builds/openauto-prodigy --output-on-failure`
Expected: **109/109** (108 + 1 new test). No regressions.

- [ ] **Step 5: Commit**

```bash
git add src/core/services/IpcServer.hpp src/core/services/IpcServer.cpp \
        tests/test_ipc_install_theme.cpp tests/CMakeLists.txt
git commit -m "feat(ipc): install_theme command — validate + apply companion theme via importCompanionTheme

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Flask `POST /api/theme/install` route + tests

**Files:**
- Modify: `web-config/server.py` (add `MAX_CONTENT_LENGTH` config, 413 handler, and the route beside `/api/theme` POST at ~line 115)
- Test: `web-config/test_server.py` (create)

**Interfaces:**
- Consumes: the `install_theme` IPC command (Task 2) via the existing `ipc_request()` helper (`server.py:23-54`).
- Produces: `POST /api/theme/install` per design §4 (multipart `manifest` + optional `wallpaper`; 200/400/413/500/503 JSON responses).

- [ ] **Step 1: Write the failing Flask tests**

Create `web-config/test_server.py`:
```python
import io
import json
import os
import unittest
from unittest import mock

import server


def multipart(manifest, wallpaper=None):
    data = {"manifest": json.dumps(manifest)}
    if wallpaper is not None:
        data["wallpaper"] = (io.BytesIO(wallpaper), "wp.jpg", "image/jpeg")
    return data


VALID = {"name": "Sunset Vibes", "light": {"primary": "#111111"}, "dark": {"primary": "#222222"}}


class InstallThemeTest(unittest.TestCase):
    def setUp(self):
        server.app.config["TESTING"] = True
        self.client = server.app.test_client()

    @mock.patch("server.ipc_request")
    def test_happy_path(self, ipc):
        ipc.return_value = {"ok": True, "slug": "sunset-vibes"}
        r = self.client.post("/api/theme/install", data=multipart(VALID),
                             content_type="multipart/form-data")
        self.assertEqual(r.status_code, 200)
        self.assertEqual(r.get_json(), {"installed": True, "slug": "sunset-vibes", "applied": True})
        self.assertEqual(ipc.call_args[0][0], "install_theme")

    @mock.patch("server.ipc_request")
    def test_missing_name_is_400(self, ipc):
        r = self.client.post("/api/theme/install",
                             data=multipart({"light": {"primary": "#111"}, "dark": {"primary": "#222"}}),
                             content_type="multipart/form-data")
        self.assertEqual(r.status_code, 400)
        ipc.assert_not_called()

    @mock.patch("server.ipc_request")
    def test_app_down_is_503(self, ipc):
        ipc.return_value = {"error": "Qt app not running (IPC socket not found)"}
        r = self.client.post("/api/theme/install", data=multipart(VALID),
                             content_type="multipart/form-data")
        self.assertEqual(r.status_code, 503)

    @mock.patch("server.ipc_request")
    def test_import_failure_is_500(self, ipc):
        ipc.return_value = {"ok": False, "error": "theme import failed"}
        r = self.client.post("/api/theme/install", data=multipart(VALID),
                             content_type="multipart/form-data")
        self.assertEqual(r.status_code, 500)

    @mock.patch("server.ipc_request")
    def test_temp_file_created_and_cleaned(self, ipc):
        captured = {}

        def fake(cmd, data=None):
            captured["path"] = data.get("wallpaper_path")
            captured["existed_during_call"] = os.path.exists(data.get("wallpaper_path", ""))
            return {"ok": True, "slug": "x"}

        ipc.side_effect = fake
        r = self.client.post("/api/theme/install",
                             data=multipart(VALID, wallpaper=b"\xff\xd8\xff" + b"\x00" * 16),
                             content_type="multipart/form-data")
        self.assertEqual(r.status_code, 200)
        self.assertTrue(captured["existed_during_call"])          # present while Qt reads it
        self.assertFalse(os.path.exists(captured["path"]))         # unlinked in finally

    def test_oversize_is_413(self):
        big = b"\xff\xd8\xff" + b"\x00" * (7 * 1024 * 1024)        # > MAX_CONTENT_LENGTH
        r = self.client.post("/api/theme/install", data=multipart(VALID, wallpaper=big),
                             content_type="multipart/form-data")
        self.assertEqual(r.status_code, 413)


if __name__ == "__main__":
    unittest.main()
```
Run: `cd web-config && python3 -m unittest test_server -v`
Expected: **FAIL** — the route does not exist yet (404s → assertions fail). (If `flask` is not importable, `pip install flask` first — it's in `web-config/requirements.txt`.)

- [ ] **Step 2: Implement the route**

In `web-config/server.py`, add `import tempfile` to the imports (top of file, near `import os`). Right after `app = Flask(__name__)` (~line 17) add:
```python
app.config["MAX_CONTENT_LENGTH"] = 6 * 1024 * 1024  # 5 MiB wallpaper + JSON headroom

UPLOAD_TMP_DIR = "/tmp/oap-theme-upload"


@app.errorhandler(413)
def _too_large(_e):
    return jsonify({"installed": False, "error": "payload too large"}), 413
```
Add the route beside `/api/theme` POST (~after line 120):
```python
@app.route("/api/theme/install", methods=["POST"])
def api_install_theme():
    """Companion theme+wallpaper upload → validate → apply via IPC install_theme."""
    raw = request.form.get("manifest")
    if not raw:
        return jsonify({"installed": False, "error": "missing manifest"}), 400
    try:
        manifest = json.loads(raw)
    except (ValueError, TypeError):
        return jsonify({"installed": False, "error": "manifest is not valid JSON"}), 400
    name = (manifest.get("name") or "").strip()
    if not name:
        return jsonify({"installed": False, "error": "manifest.name is required"}), 400

    data = {
        "name": name,
        "seed": manifest.get("seed", ""),
        "light": manifest.get("light", {}),
        "dark": manifest.get("dark", {}),
    }

    temp_path = None
    wp = request.files.get("wallpaper")
    if wp is not None and wp.filename:
        if (wp.mimetype or "") != "image/jpeg":
            return jsonify({"installed": False, "error": "wallpaper must be image/jpeg"}), 400
        os.makedirs(UPLOAD_TMP_DIR, mode=0o700, exist_ok=True)
        fd, temp_path = tempfile.mkstemp(dir=UPLOAD_TMP_DIR, suffix=".jpg")
        os.close(fd)
        wp.save(temp_path)
        data["wallpaper_path"] = temp_path

    try:
        resp = ipc_request("install_theme", data)
        if resp.get("ok"):
            return jsonify({"installed": True, "slug": resp.get("slug", ""), "applied": True}), 200
        err = resp.get("error", "unknown error")
        if "not running" in err or "not found" in err or "not accepting" in err or "timed out" in err:
            return jsonify({"installed": False, "error": "Qt app not running"}), 503
        return jsonify({"installed": False, "error": err}), 500
    finally:
        if temp_path and os.path.exists(temp_path):
            os.unlink(temp_path)
```

- [ ] **Step 3: Run the Flask tests**

Run: `cd web-config && python3 -m unittest test_server -v`
Expected: **PASS** — all six tests. (`test_temp_file_created_and_cleaned` proves the temp file exists during the IPC call and is unlinked afterward; `test_oversize_is_413` proves the size guard fires before the route body.)

- [ ] **Step 4: Confirm the C++ suite is untouched**

Run: `ctest --test-dir ~/builds/openauto-prodigy --output-on-failure`
Expected: **109/109** (this task changed no C++).

- [ ] **Step 5: Commit**

```bash
git add web-config/server.py web-config/test_server.py
git commit -m "feat(web-config): POST /api/theme/install — companion theme upload endpoint

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Self-Review (author checklist, done)

- **Spec coverage:** §4 HTTP contract → Task 3 (route, manifest schema, response codes). §6 IPC command → Task 2. §7 ThemeService slugify + reuse importCompanionTheme → Task 1. §8 validation matrix (name, colors, camelCase, magic, size, path) → Task 1 `parseThemeInstall` + its test matrix. §5 Flask temp-file lifecycle → Task 3 route + cleanup test. §11 testing → Tasks 1–3 tests. §9 error/ack-lies fix → real `ok` boolean returned in Task 2, mapped to status in Task 3. §12 wishlist follow-ups → already committed (911d325), not in this plan.
- **Placeholder scan:** none — every step has concrete code/commands.
- **Type consistency:** `parseThemeInstall`/`ThemeInstallParseResult`/`ThemeInstallRequest`/`ThemeService::slugify` signatures identical across Task 1 (define) and Task 2 (consume). IPC command string `install_theme` and response fields `ok`/`slug`/`error` consistent across Tasks 2 and 3.
- **Note on frozen conversion:** `camelToHyphen` in Task 1 is a deliberate copy of the legacy `CompanionListenerService` lambda (frozen file) — dedup is a wishlist item at 9876 retirement, not this plan.
