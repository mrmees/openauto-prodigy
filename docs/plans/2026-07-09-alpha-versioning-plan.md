# ALPHA-YY-MM-DD-NN Versioning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

Status: ACTIVE
Date: 2026-07-09
Design: `docs/plans/2026-07-09-alpha-versioning-design.md`
Grounded: 06810a6 (dev)

**Goal:** Every user-visible version surface reports a single git-derived
`ALPHA-YY-MM-DD-NN` string; milestone git tags are the only version record.

**Architecture:** Top-level CMake derives `OAP_VERSION` at configure time via
`git describe --tags --match "ALPHA-*" --dirty` (fallback
`ALPHA-untagged-<shorthash>`); `src/CMakeLists.txt` exports it as a PUBLIC
compile definition on `openauto-core` alongside the existing `OAP_GIT_HASH`.
Six surfaces consume it; the config-sourced `identity.sw_version` key is
deleted end-to-end; `scripts/tag-alpha.sh` mints milestone tags.

**Tech Stack:** CMake 3.22+, Qt 6.8 (QtTest), bash, yaml-cpp config layer.

## Global Constraints

- Repo: `/mnt/e/claude/personal/openautopro/openauto-prodigy`. Build ONLY in
  `~/builds/openauto-prodigy` (ext4). NEVER create or use an in-repo `build/`
  dir — the repo sits on a 9p mount and object churn there is painfully slow.
- `ctest` does NOT compile the app target. Before claiming green, also run
  `cmake --build ~/builds/openauto-prodigy --target openauto-prodigy`.
- One task = one commit. Nobody pushes mid-execution. Commit messages end
  with `Co-Authored-By:` trailer per session convention.
- TDD: run the task's targeted test red→green first, then the FULL suite
  (`ctest --output-on-failure`) before committing.
- Version format is literal and exact: prefix `ALPHA`, date part
  `date +%y-%m-%d` (e.g. `26-07-09`), NN two digits zero-padded, joined by
  hyphens: `ALPHA-26-07-09-01`.
- Until the first tag exists (Task 6), every build reports the fallback
  `ALPHA-untagged-<shorthash>` — that IS the expected value in Tasks 1–5.
- Workers read root `AGENTS.md` and the nested `AGENTS.md` nearest their
  files (`src/AGENTS.md`, `src/core/aa/AGENTS.md`, `qml/AGENTS.md`) before
  editing. Scope is bounded to the files named in each task
  (wishlist-then-promote for anything else).

---

### Task 1: CMake `OAP_VERSION` derivation + well-formedness test

**Tier:** opus

**Files:**
- Modify: `CMakeLists.txt` (comment above line 2; new block after the
  `OAP_GIT_HASH` block ending line 46)
- Modify: `src/CMakeLists.txt:517-520`
- Create: `tests/test_version.cpp`
- Modify: `tests/CMakeLists.txt` (one line, after line 27
  `oap_add_test(test_config_key_coverage ...)`)

**Interfaces:**
- Consumes: existing `OAP_GIT_HASH` CMake variable (top-level
  `CMakeLists.txt:37-46`, value `unknown` when git fails).
- Produces: `OAP_VERSION` — a C string-literal macro (e.g.
  `"ALPHA-untagged-06810a6"`), PUBLIC compile definition on `openauto-core`,
  visible to every target linking it (app target and all tests included).
  Tasks 2–3 rely on exactly the spelling `OAP_VERSION`.

- [ ] **Step 1: Write the failing test**

Create `tests/test_version.cpp`:

```cpp
// Verifies the OAP_VERSION compile definition is present and well-formed.
// Beta transition: the prefix assertion below changes with the scheme
// (see AGENTS.md § Versioning).
#include <QtTest>

class TestVersion : public QObject
{
    Q_OBJECT
private slots:
    void testVersionDefineWellFormed();
};

void TestVersion::testVersionDefineWellFormed()
{
    const QString v = QStringLiteral(OAP_VERSION);
    QVERIFY(!v.isEmpty());
    // Accepts: ALPHA-YY-MM-DD-NN, ALPHA-YY-MM-DD-NN-<n>-g<hash>[-dirty],
    //          ALPHA-untagged-<hash>
    QVERIFY2(v.startsWith(QStringLiteral("ALPHA-")),
             qPrintable(QStringLiteral("unexpected version: ") + v));
}

QTEST_APPLESS_MAIN(TestVersion)
#include "test_version.moc"
```

Register it in `tests/CMakeLists.txt` directly below line 27
(`oap_add_test(test_config_key_coverage SOURCES test_config_key_coverage.cpp)`):

```cmake
oap_add_test(test_version SOURCES test_version.cpp)
```

- [ ] **Step 2: Run it to verify it fails**

```bash
cmake -S /mnt/e/claude/personal/openautopro/openauto-prodigy -B ~/builds/openauto-prodigy
cmake --build ~/builds/openauto-prodigy --target test_version -j"$(nproc)"
```

Expected: compile FAILURE — `error: 'OAP_VERSION' was not declared in this scope`.

- [ ] **Step 3: Add the CMake derivation**

In top-level `CMakeLists.txt`, add this comment directly ABOVE line 2's
`project(...)`:

```cmake
# NOTE: the numeric VERSION below is CMake-internal only. The displayed /
# reported app version is git-derived — see the OAP_VERSION block below.
```

Then insert AFTER the `endif()` that closes the `OAP_GIT_HASH` fallback
(line 46, `set(OAP_GIT_HASH "unknown")` block), BEFORE `add_subdirectory(src)`:

```cmake
# App version identity: ALPHA-YY-MM-DD-NN milestone tags, minted by
# scripts/tag-alpha.sh (see AGENTS.md § Versioning). Captured at CONFIGURE
# time — after tagging, reconfigure + rebuild or the binary keeps the old
# string. No ALPHA tag yet (or git absent) -> ALPHA-untagged-<shorthash>.
execute_process(
    COMMAND git describe --tags --match "ALPHA-*" --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE OAP_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT OAP_VERSION)
    set(OAP_VERSION "ALPHA-untagged-${OAP_GIT_HASH}")
endif()
message(STATUS "App version: ${OAP_VERSION}")
```

In `src/CMakeLists.txt`, replace lines 517–520:

```cmake
# Git hash for ServiceDiscoveryResponse sw_build field
target_compile_definitions(openauto-core PUBLIC
    OAP_GIT_HASH="${OAP_GIT_HASH}"
)
```

with:

```cmake
# Version identity for every user-visible surface: Qt applicationVersion
# (--version, QML Qt.application.version), IPC status, External API
# ServerHello, AA ServiceDiscovery sw_build.
target_compile_definitions(openauto-core PUBLIC
    OAP_GIT_HASH="${OAP_GIT_HASH}"
    OAP_VERSION="${OAP_VERSION}"
)
```

(The old comment was false — `OAP_GIT_HASH` never fed `sw_build`; Task 2
makes `OAP_VERSION` actually do so.)

- [ ] **Step 4: Run test to verify it passes**

```bash
cmake -S /mnt/e/claude/personal/openautopro/openauto-prodigy -B ~/builds/openauto-prodigy
cmake --build ~/builds/openauto-prodigy --target test_version -j"$(nproc)"
cd ~/builds/openauto-prodigy && ctest -R test_version --output-on-failure
```

Expected: configure log contains `App version: ALPHA-untagged-06810a6`
(hash = current HEAD, changes as commits land); test PASSES.

- [ ] **Step 5: Full suite + app target**

```bash
cmake --build ~/builds/openauto-prodigy -j"$(nproc)"
cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j"$(nproc)"
cd ~/builds/openauto-prodigy && ctest --output-on-failure
```

Expected: all green.

- [ ] **Step 6: Commit**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git add CMakeLists.txt src/CMakeLists.txt tests/test_version.cpp tests/CMakeLists.txt
git commit -m "feat: derive OAP_VERSION from ALPHA milestone tags at configure time"
```

---

### Task 2: Point all six version surfaces at `OAP_VERSION`

**Tier:** opus

**Files:**
- Modify: `src/main.cpp:209`
- Modify: `src/core/services/IpcServer.cpp:332`
- Modify: `src/core/api/ApiServer.cpp:116-117`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.cpp:74`
- Modify: `qml/applications/settings/SystemSettings.qml:114-119`

**Interfaces:**
- Consumes: `OAP_VERSION` and `OAP_GIT_HASH` compile definitions (Task 1).
- Produces: `QCoreApplication::applicationVersion()` == `OAP_VERSION` (QML
  reads it as `Qt.application.version`); API `appVersion_` format
  `"<OAP_VERSION> (<OAP_GIT_HASH>)"`. Task 3 relies on ApiServer no longer
  reading `identity.sw_version`.

- [ ] **Step 1: Apply the five edits**

`src/main.cpp:209` — replace:

```cpp
    app.setApplicationVersion("0.1.0");
```

with:

```cpp
    app.setApplicationVersion(QStringLiteral(OAP_VERSION));
```

`src/core/services/IpcServer.cpp:332` — replace:

```cpp
    obj["version"] = QStringLiteral("0.1.0");
```

with:

```cpp
    obj["version"] = QStringLiteral(OAP_VERSION);
```

`src/core/api/ApiServer.cpp:116-117` — replace:

```cpp
    const QString sw = cfgStr("identity.sw_version", QString());
    appVersion_ = sw + QStringLiteral(" (" OAP_GIT_HASH ")");
```

with:

```cpp
    appVersion_ = QStringLiteral(OAP_VERSION " (" OAP_GIT_HASH ")");
```

(Both macros are string literals; preprocessor concatenation inside
`QStringLiteral` is intentional.)

`src/core/aa/ServiceDiscoveryBuilder.cpp:74` — replace:

```cpp
    config.swBuild = "1";
```

with:

```cpp
    config.swBuild = OAP_VERSION;
```

(`swBuild` is a `QString`; the surrounding Crankshaft-NG identity fields —
manufacturer, model, year, vehicle id — are the phone's match keys and MUST
NOT change. `sw_build` is not a match key; it only shows up in phone-side
logs.)

`qml/applications/settings/SystemSettings.qml:114-119` — replace:

```qml
        SettingsRow { rowIndex: 0
            ReadOnlyField {
                label: "Version"
                configPath: "identity.sw_version"
            }
        }
```

with:

```qml
        SettingsRow { rowIndex: 0
            ReadOnlyField {
                label: "Version"
                value: Qt.application.version
            }
        }
```

(`ReadOnlyField.value` takes precedence over `configPath` lookup in
`Component.onCompleted`; `Qt.application.version` reflects
`setApplicationVersion`, i.e. `OAP_VERSION`.)

- [ ] **Step 2: Build app target + full suite**

```bash
cmake --build ~/builds/openauto-prodigy -j"$(nproc)"
cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j"$(nproc)"
cd ~/builds/openauto-prodigy && ctest --output-on-failure
```

Expected: all green (no test asserts the old strings — verified at plan
time).

- [ ] **Step 3: Runtime smoke — `--version`**

```bash
QT_QPA_PLATFORM=offscreen ~/builds/openauto-prodigy/src/openauto-prodigy --version
```

Expected output: `OpenAuto Prodigy ALPHA-untagged-<shorthash>` (Qt prints
`applicationName applicationVersion`). If the binary exits non-zero or prints
`0.1.0`, the task has failed — do not proceed.

- [ ] **Step 4: Commit**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git add src/main.cpp src/core/services/IpcServer.cpp src/core/api/ApiServer.cpp src/core/aa/ServiceDiscoveryBuilder.cpp qml/applications/settings/SystemSettings.qml
git commit -m "feat: report OAP_VERSION on all version surfaces"
```

---

### Task 3: Remove `identity.sw_version` end-to-end

**Tier:** sonnet

**Files:**
- Modify: `src/core/YamlConfig.cpp` (line 77 default; lines 503–511 accessors)
- Modify: `src/core/YamlConfig.hpp:68-69`
- Modify: `tests/test_yaml_config.cpp:150,164`
- Modify: `tests/data/test_config.yaml:37`
- Modify: `tests/test_config_key_coverage.cpp:64`

**Interfaces:**
- Consumes: Task 2 already removed the last production readers (ApiServer,
  SystemSettings.qml). `swVersion()`/`setSwVersion()` have NO production
  callers — tests only (verified at plan time).
- Produces: `identity.sw_version` no longer exists in defaults; because
  `setValueByPath()` validates against the defaults schema, the key can no
  longer be set via config APIs. Leftover `sw_version:` entries in existing
  user configs are ignored (no migration function needed).

- [ ] **Step 1: Remove the code (red)**

`src/core/YamlConfig.cpp:77` — delete the line:

```cpp
    root_["identity"]["sw_version"] = "0.3.0";
```

`src/core/YamlConfig.cpp:503-511` — delete both accessors:

```cpp
QString YamlConfig::swVersion() const
{
    return QString::fromStdString(root_["identity"]["sw_version"].as<std::string>("0.3.0"));
}

void YamlConfig::setSwVersion(const QString& v)
{
    root_["identity"]["sw_version"] = v.toStdString();
}
```

`src/core/YamlConfig.hpp:68-69` — delete both declarations:

```cpp
    QString swVersion() const;
    void setSwVersion(const QString& v);
```

- [ ] **Step 2: Run tests to verify they fail (proves coverage)**

```bash
cmake --build ~/builds/openauto-prodigy -j"$(nproc)" 2>&1 | tail -20
```

Expected: compile FAILURE in `test_yaml_config.cpp` —
`error: 'class oap::YamlConfig' has no member named 'swVersion'`.

- [ ] **Step 3: Update the tests (green)**

`tests/test_yaml_config.cpp:150` — in `testIdentityDefaults()`, delete:

```cpp
    QCOMPARE(config.swVersion(), QString("0.3.0"));
```

`tests/test_yaml_config.cpp:164` — in `testIdentityFromFile()`, delete:

```cpp
    QCOMPARE(config.swVersion(), QString("9.9.9"));
```

`tests/data/test_config.yaml:37` — delete the line:

```yaml
  sw_version: "9.9.9"
```

`tests/test_config_key_coverage.cpp:64` — delete the entry:

```cpp
        "identity.sw_version",
```

- [ ] **Step 4: Full suite + app target**

```bash
cmake --build ~/builds/openauto-prodigy -j"$(nproc)"
cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j"$(nproc)"
cd ~/builds/openauto-prodigy && ctest --output-on-failure
```

Expected: all green (including `test_yaml_config` and
`test_config_key_coverage`).

- [ ] **Step 5: Commit**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git add src/core/YamlConfig.cpp src/core/YamlConfig.hpp tests/test_yaml_config.cpp tests/data/test_config.yaml tests/test_config_key_coverage.cpp
git commit -m "refactor: remove identity.sw_version config key"
```

---

### Task 4: `scripts/tag-alpha.sh` milestone tag helper

**Tier:** sonnet

**Files:**
- Create: `scripts/tag-alpha.sh`

**Interfaces:**
- Consumes: nothing from other tasks (pure git).
- Produces: annotated tags named exactly `ALPHA-YY-MM-DD-NN`; the CMake
  `--match "ALPHA-*"` pattern from Task 1 picks them up. Task 6 runs this
  script to mint the first tag.

- [ ] **Step 1: Write the script**

Create `scripts/tag-alpha.sh`:

```bash
#!/usr/bin/env bash
# Mint the next ALPHA-YY-MM-DD-NN milestone tag (annotated, on HEAD).
# NN = today's build number: max existing NN for today + 1 (deleted tags are
# never reused). Milestone tags are created ONLY when Matthew declares one.
# Beta transition: change PREFIX here + the --match pattern in the top-level
# CMakeLists.txt + the prefix assert in tests/test_version.cpp (see
# AGENTS.md § Versioning).
set -euo pipefail

PREFIX="ALPHA"
TODAY="$(date +%y-%m-%d)"
LAST="$(git tag --list "${PREFIX}-${TODAY}-*" \
        | sed "s/^${PREFIX}-${TODAY}-//" | sort -n | tail -n1)"
# 10#: NN is zero-padded — force base-10 so 08/09 don't parse as octal.
NN="$(printf '%02d' $(( 10#${LAST:-0} + 1 )))"
TAG="${PREFIX}-${TODAY}-${NN}"

if ! git diff-index --quiet HEAD --; then
    echo "WARNING: working tree is dirty — a build from this tree reports -dirty." >&2
fi

git tag -a "$TAG" -m "Milestone build ${TAG}"
echo "Created tag: ${TAG}"
echo "Next steps:"
echo "  1. Push the tag (needs go-ahead): git push origin ${TAG}"
echo "  2. Reconfigure + rebuild — the version is captured at CONFIGURE time."
```

- [ ] **Step 2: Set the executable bit git-side**

The repo lives on a 9p mount where `chmod` fails — set the bit in the index:

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git update-index --add --chmod=+x scripts/tag-alpha.sh
```

Canonical invocation stays `bash scripts/tag-alpha.sh` regardless.

- [ ] **Step 3: Test in a scratch repo (never tag the real repo here)**

```bash
T="$(mktemp -d)" && cd "$T" && git init -q
git -c user.email=t@t -c user.name=t commit -q --allow-empty -m init
REPO=/mnt/e/claude/personal/openautopro/openauto-prodigy
bash "$REPO/scripts/tag-alpha.sh"          # expect: Created tag: ALPHA-<today>-01
bash "$REPO/scripts/tag-alpha.sh"          # expect: Created tag: ALPHA-<today>-02
git tag -a "ALPHA-$(date +%y-%m-%d)-08" -m x
bash "$REPO/scripts/tag-alpha.sh"          # expect: ALPHA-<today>-09 (octal guard)
git tag -d "ALPHA-$(date +%y-%m-%d)-01" >/dev/null
bash "$REPO/scripts/tag-alpha.sh"          # expect: ALPHA-<today>-10 (max+1, no reuse)
git tag --list "ALPHA-*"
cd / && rm -rf "$T"
```

Expected final list: `-02`, `-08`, `-09`, `-10` for today's date (and no
`-01`). Any deviation = script bug; fix before committing.

- [ ] **Step 4: Verify the real repo is untouched, then commit**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git tag --list "ALPHA-*"        # expect: empty
git add scripts/tag-alpha.sh
git commit -m "feat: add tag-alpha.sh milestone tag helper"
```

---

### Task 5: Documentation — AGENTS.md convention + schema cleanup

**Tier:** sonnet

**Files:**
- Modify: `AGENTS.md` (new section between `## Docs Conventions` and
  `## Scope Note`, i.e. after line 155)
- Modify: `docs/reference/config-schema.md` (lines 70, 142; Migration Policy
  section ~line 321)
- Modify: `tools/package-prebuilt-release.sh:9` (example comment only)

**Interfaces:**
- Consumes: names/paths exactly as created in Tasks 1–4 (`OAP_VERSION`,
  `scripts/tag-alpha.sh`, `tests/test_version.cpp`).
- Produces: AGENTS.md § Versioning — referenced by the Task 1 CMake comment
  and the Task 4 script header.

- [ ] **Step 1: Add the AGENTS.md section**

Insert between `## Docs Conventions` and `## Scope Note`:

```markdown
## Versioning

- Alpha scheme: **`ALPHA-YY-MM-DD-NN`** annotated git tags (date from
  `date +%y-%m-%d`, NN = build number of the day, two digits). Tags are
  created ONLY when Matthew declares a milestone — never per deploy or per
  build. Mint the next one with `bash scripts/tag-alpha.sh`.
- The binary derives its version at CMake **configure time**
  (`git describe --tags --match "ALPHA-*" --dirty` → `OAP_VERSION` compile
  definition on `openauto-core`). After tagging, reconfigure + rebuild or
  the binary keeps the previous string. Untagged builds report
  `ALPHA-<tag>-<n>-g<hash>` / `ALPHA-untagged-<hash>`.
- Every user-visible surface reads `OAP_VERSION` (Qt applicationVersion and
  `--version`, QML `Qt.application.version`, IPC status, External API
  ServerHello, AA ServiceDiscovery `sw_build`). Never hardcode a version
  string. `identity.sw_version` was removed 2026-07-09.
- Beta transition checklist (all four together): `PREFIX` in
  `scripts/tag-alpha.sh`; the `--match` pattern in top-level
  `CMakeLists.txt`; the prefix assertion in `tests/test_version.cpp`; this
  section.
```

- [ ] **Step 2: Clean config-schema.md**

Delete line 70 from the example block:

```yaml
  sw_version: "0.3.0"                 # app version
```

Delete the table row at line 142:

```markdown
| `identity.sw_version` | string | `"0.3.0"` | App software version |
```

In `## Migration Policy`, after the **Schema validation** paragraph, add:

```markdown
**Removed keys:**
- 2026-07-09: `identity.sw_version` — the app version is compiled in
  (`OAP_VERSION`, git-derived; see `AGENTS.md` § Versioning). A leftover
  `sw_version:` entry in an existing `config.yaml` is ignored.
```

- [ ] **Step 3: Fix the packager example**

`tools/package-prebuilt-release.sh:9` — replace:

```bash
#     --version-tag v0.1.0
```

with:

```bash
#     --version-tag ALPHA-26-07-09-01
```

- [ ] **Step 4: Link check + commit**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
python3 scripts/check-doc-links.py
git add AGENTS.md docs/reference/config-schema.md tools/package-prebuilt-release.sh
git commit -m "docs: record ALPHA versioning convention; drop sw_version from schema"
```

Expected: link checker exits 0.

---

### Task 6: Final verification, Codex gate, landing + first tag

**Tier:** main (Fable session — gate adjudication and Pi deploy are
judgment-heavy; do not dispatch)

**Files:**
- Modify: `docs/session-handoffs.md` (append handoff entry)
- Move: `docs/plans/2026-07-09-alpha-versioning-plan.md` and
  `...-design.md` → `docs/archive/plans/` (status flipped, same commit)

**Interfaces:**
- Consumes: everything from Tasks 1–5.
- Produces: the landing commit tagged `ALPHA-<today>-01`; deployed Pi binary
  reporting it.

- [ ] **Step 1: Full local verification**

```bash
cmake -S /mnt/e/claude/personal/openautopro/openauto-prodigy -B ~/builds/openauto-prodigy
cmake --build ~/builds/openauto-prodigy -j"$(nproc)"
cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j"$(nproc)"
cd ~/builds/openauto-prodigy && ctest --output-on-failure
QT_QPA_PLATFORM=offscreen ~/builds/openauto-prodigy/src/openauto-prodigy --version
```

Expected: all green; version prints `ALPHA-untagged-<shorthash>` (tag not
minted yet).

- [ ] **Step 2: Codex review gate (pre-push, per AGENTS.md)**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
bash scripts/codex-review.sh
```

Adjudicate EVERY finding (confirmed → fix; dismissed → stated reason; no
silent drops). Substantial fixes → one gate re-run on the new range. Exit
2/4 → degrade to Fable-only review and note it in the handoff.

- [ ] **Step 3: Handoff + archive (landing commit)**

Append to `docs/session-handoffs.md` (match existing entry format): what
changed (versioning scheme + sw_version removal), why, gate adjudication
counts, verification results, next steps (push + deploy). Flip both plan
files' `Status:` to `COMPLETED 2026-07-09` and `git mv` them to
`docs/archive/plans/` in the same commit:

```bash
git mv docs/plans/2026-07-09-alpha-versioning-design.md docs/archive/plans/
git mv docs/plans/2026-07-09-alpha-versioning-plan.md docs/archive/plans/
git add docs/session-handoffs.md
git commit -m "docs: land ALPHA versioning; handoff + archive plan"
```

- [ ] **Step 4: Mint milestone tag #1 (approved 2026-07-09: "tag it when it lands")**

```bash
bash scripts/tag-alpha.sh
```

Expected: `Created tag: ALPHA-<today>-01` on the landing commit.

- [ ] **Step 5: Rebuild and confirm the tag is picked up**

```bash
cmake -S /mnt/e/claude/personal/openautopro/openauto-prodigy -B ~/builds/openauto-prodigy
cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j"$(nproc)"
QT_QPA_PLATFORM=offscreen ~/builds/openauto-prodigy/src/openauto-prodigy --version
```

Expected: `OpenAuto Prodigy ALPHA-<today>-01` — exactly the tag, no suffix.

- [ ] **Step 6: Cross-build + stamp check**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
./cross-build.sh
strings build-pi/src/openauto-prodigy | grep -m1 "ALPHA-"
```

Expected: the tag string embedded in the ARM binary. (Docker may need
`sg docker -c './cross-build.sh'`.)

- [ ] **Step 7: Deploy to Pi + live verify**

```bash
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.149:~/openauto-prodigy/build/src/
ssh matt@192.168.1.149 'sudo systemctl restart openauto-prodigy.service'
ssh matt@192.168.1.149 'strings ~/openauto-prodigy/build/src/openauto-prodigy | grep -m1 ALPHA-'
```

Expected: service restarts clean; deployed binary reports the tag.
Hardware-dependent: an AA phone-connect check confirming ServiceDiscovery
still completes with the new `sw_build`. If no phone is at hand, record it
as a pending checklist item in the handoff — say so plainly.

- [ ] **Step 8: Push — ONLY with Matthew's explicit go-ahead**

```bash
git push origin dev
git push origin "ALPHA-<today>-01"
```

---

## Execution notes

- Tasks 1 → 2 → 3 are strictly ordered (each consumes the previous).
  Tasks 4 and 5 depend only on Task 1's naming and can run after Task 3 in
  sequence; Task 5 references the script header, so run Task 4 before 5.
- Workers report synthesized results only: files changed (one line each),
  test command + pass/fail counts, deviations. Raw logs stay out of the main
  session.
- Escalation per AGENTS.md ladder: two focused attempts, then Codex with a
  prompt file, then Fable takes over.
