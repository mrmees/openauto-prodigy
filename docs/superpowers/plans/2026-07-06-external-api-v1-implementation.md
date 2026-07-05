# External API v1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the External API v1 server — protobuf over TCP (9810) + WebSocket (9811) with PIN-pairing auth, snapshot-on-subscribe status streams, action/notification/phone requests, and companion inbound reports — per `docs/superpowers/specs/2026-07-06-external-api-v1-design.md`.

**Architecture:** `ApiServer` (Qt main thread) owns two listeners feeding one session layer (`ApiSession` per connection over an `IApiTransport` abstraction). Five `TopicPublisher`s bind core services, serialize once per change, fan out to subscribed sessions with a hard per-client byte cap. Pure serializer functions hold every normalization table. A new static lib `prodigy-api-proto` carries generated code from `proto/api/`.

**Tech Stack:** Qt 6.8 (Network, WebSockets — net-new), protobuf (proto3, protoc ≥3.15; Trixie ships 3.21), yaml-cpp, QtTest.

## Global Constraints

Copied from the design doc (§17 invariants) — every task implicitly includes these:

- **All API code runs on the Qt main thread.** No worker threads for handlers, ever.
- **Serializers are the only normalization site.** Raw service ints/strings never reach the wire.
- **Backpressure:** check `bytesToWrite()` before every send; if it exceeds the cap, disconnect the session. Never buffer-and-hope.
- **One session teardown path.** Client-registered actions must not survive their session.
- **Proto is additive-only after freeze.** Do not edit `proto/api/*.proto` in this plan except where a task says so explicitly (none do). If the proto seems wrong, STOP and record in `docs/session-handoffs.md`.
- **Never touch `libs/prodigy-oaa-protocol/`** (read-only reference is fine).
- **Phone bridge calls `IPhoneStateService` only** — no PipeWire/BlueZ includes anywhere under `src/core/api/`.
- Ports/defaults: TCP 9810, WS 9811, `api.max_queue_bytes` 1 MiB, frame cap 256 KiB, handshake timeout 5000 ms, pairing window 120 s.
- Namespace for new C++ code: `oap::api`. Generated proto namespace: `prodigy::api::v1` (alias locally as `namespace pb = prodigy::api::v1;`).
- Test conventions: QtTest, one file per target `tests/test_<subject>.cpp`, class `Test<Subject>`, registered via `oap_add_test(...)` in `tests/CMakeLists.txt`, `QTEST_MAIN` + `#include "<file>.moc"`. Loopback tests use ports 19900+ (19876–19893 are taken by companion tests).
- Verification commands (run from repo root unless noted): local build `cd build && cmake .. && make -j$(nproc)`; tests `cd build && ctest --output-on-failure`; cross-build `./cross-build.sh` (run once in Task 2 and once in Task 16 — it is slow).
- Grounding: design doc pinned at commit `a294898`. If `git log --oneline -1 -- src/core/services/` shows service changes newer than 2026-07-06, re-verify the mapping tables in design §8 before Tasks 6–7.

---

### Task 1: Proto codegen library `prodigy-api-proto`

**Files:**
- Create: `proto/CMakeLists.txt`
- Modify: `CMakeLists.txt` (root — add subdirectory after the oaa lib at line ~31)
- Modify: `tests/CMakeLists.txt` (register test)
- Test: `tests/test_api_proto_roundtrip.cpp`

**Interfaces:**
- Consumes: the ten committed files in `proto/api/` (do not edit them).
- Produces: static lib target **`prodigy-api-proto`**; headers included as `#include "api/api.pb.h"` etc.; C++ namespace `prodigy::api::v1`.

- [ ] **Step 1: Write `proto/CMakeLists.txt`**

Mechanism mirrors `libs/prodigy-oaa-protocol/CMakeLists.txt:8-49` (custom command per file; `protobuf_generate_cpp` mangles subdirectory layouts — `docs/session-handoffs.md:550`):

```cmake
# External API v1 protobuf library (package prodigy.api.v1)
# Contract files live in proto/api/ and are ADDITIVE-ONLY after freeze.
find_package(Protobuf REQUIRED)

set(API_PROTO_IMPORT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(API_PROTO_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

file(GLOB API_PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/api/*.proto)

set(API_PROTO_SRCS "")
set(API_PROTO_HDRS "")
foreach(PROTO_FILE ${API_PROTO_FILES})
    file(RELATIVE_PATH PROTO_REL ${API_PROTO_IMPORT_ROOT} ${PROTO_FILE})
    string(REPLACE ".proto" ".pb.cc" PROTO_SRC "${API_PROTO_OUTPUT_DIR}/${PROTO_REL}")
    string(REPLACE ".proto" ".pb.h"  PROTO_HDR "${API_PROTO_OUTPUT_DIR}/${PROTO_REL}")
    get_filename_component(PROTO_OUT_DIR ${PROTO_SRC} DIRECTORY)
    add_custom_command(
        OUTPUT ${PROTO_SRC} ${PROTO_HDR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_OUT_DIR}
        COMMAND protobuf::protoc
            --proto_path=${API_PROTO_IMPORT_ROOT}
            --cpp_out=${API_PROTO_OUTPUT_DIR}
            ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating API protobuf: ${PROTO_REL}")
    list(APPEND API_PROTO_SRCS ${PROTO_SRC})
    list(APPEND API_PROTO_HDRS ${PROTO_HDR})
endforeach()

add_library(prodigy-api-proto STATIC ${API_PROTO_SRCS})
target_include_directories(prodigy-api-proto PUBLIC ${API_PROTO_OUTPUT_DIR})
target_link_libraries(prodigy-api-proto PUBLIC protobuf::libprotobuf)
# Generated protobuf code triggers -Wall noise; keep it quiet like the oaa lib.
set_target_properties(prodigy-api-proto PROPERTIES CXX_STANDARD 17)
```

- [ ] **Step 2: Register subdirectory in root `CMakeLists.txt`**

After line 31 (`add_subdirectory(libs/prodigy-oaa-protocol)`), add:

```cmake
# External API v1 protobuf contract (prodigy-private, NOT the AA submodule)
add_subdirectory(proto)
```

- [ ] **Step 3: Write the round-trip test**

`tests/test_api_proto_roundtrip.cpp`:

```cpp
#include <QtTest>
#include "api/api.pb.h"

namespace pb = prodigy::api::v1;

class TestApiProtoRoundtrip : public QObject {
    Q_OBJECT
private slots:
    void testEnvelopeRoundtrip();
    void testOneofExclusivity();
};

void TestApiProtoRoundtrip::testEnvelopeRoundtrip() {
    pb::ApiMessage msg;
    msg.set_request_id(42);
    auto* hello = msg.mutable_client_hello();
    hello->set_requested_api_version_major(1);
    hello->set_client_name("test");
    hello->set_client_kind(pb::CLIENT_KIND_DIAGNOSTIC);

    std::string bytes;
    QVERIFY(msg.SerializeToString(&bytes));

    pb::ApiMessage parsed;
    QVERIFY(parsed.ParseFromString(bytes));
    QCOMPARE(parsed.request_id(), (quint64)42);
    QCOMPARE(parsed.payload_case(), pb::ApiMessage::kClientHello);
    QCOMPARE(QString::fromStdString(parsed.client_hello().client_name()), QString("test"));
}

void TestApiProtoRoundtrip::testOneofExclusivity() {
    pb::ApiMessage msg;
    msg.mutable_client_hello();
    msg.mutable_server_hello();  // replaces client_hello
    QCOMPARE(msg.payload_case(), pb::ApiMessage::kServerHello);
    QVERIFY(!msg.has_client_hello());
}

QTEST_MAIN(TestApiProtoRoundtrip)
#include "test_api_proto_roundtrip.moc"
```

Register in `tests/CMakeLists.txt` (after the last `oap_add_test` block):

```cmake
oap_add_test(test_api_proto_roundtrip
    SOURCES test_api_proto_roundtrip.cpp
    EXTRA_LIBS prodigy-api-proto)
```

- [ ] **Step 4: Build and run**

```bash
cd build && cmake .. && make -j$(nproc) test_api_proto_roundtrip
ctest -R test_api_proto_roundtrip --output-on-failure
```
Expected: generation messages "Generating API protobuf: api/…", then PASS (2 tests).

- [ ] **Step 5: Commit**

```bash
git add proto/CMakeLists.txt CMakeLists.txt tests/
git commit -m "build: generate prodigy-api-proto static lib from proto/api"
```

---

### Task 2: Qt6::WebSockets dependency plumbing

Qt WebSockets is net-new to this repo (verified: zero references). Wire it everywhere the build touches, BEFORE any code uses it.

**Files:**
- Modify: `CMakeLists.txt` line 11 (find_package components)
- Modify: `src/CMakeLists.txt` (~line 93, openauto-core link libs)
- Modify: `docs/development.md` line 20-21 (package list)
- Modify: `install.sh` line ~798-799 (package list)
- Modify: `docker/Dockerfile.cross-pi4` (~line 21-25, arm64 dev packages)

**Interfaces:**
- Produces: `Qt6::WebSockets` linked PUBLIC on `openauto-core` (tests inherit it transitively).

- [ ] **Step 1: Root CMake** — change line 11 to:

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Multimedia Network WebSockets DBus)
```

- [ ] **Step 2: Link on openauto-core** — in `src/CMakeLists.txt`, in the `target_link_libraries(openauto-core PUBLIC ...)` block (line ~87-102), add `Qt6::WebSockets` on its own line directly after `Qt6::Network`.

- [ ] **Step 3: Host packages** — `docs/development.md` line 20-21: add `qt6-websockets-dev` to the apt line that has `qt6-connectivity-dev qt6-multimedia-dev`. Same addition in `install.sh` (~line 799). Install locally now:

```bash
sudo apt install -y qt6-websockets-dev
```

- [ ] **Step 4: Cross image** — `docker/Dockerfile.cross-pi4`: add `qt6-websockets-dev:arm64 \` after line 25 (`qt6-connectivity-dev:arm64 \`). Then force an image rebuild:

```bash
docker image rm openauto-cross-pi4
./cross-build.sh
```
Expected: image rebuilds (one-time, slow), cross-build completes to `build-pi/src/openauto-prodigy`. If apt can't find the arm64 package, run `docker build --no-cache -t openauto-cross-pi4 -f docker/Dockerfile.cross-pi4 docker` and check the Dockerfile's dpkg arch setup lines above line 18.

- [ ] **Step 5: Local reconfigure proves the find_package**

```bash
cd build && cmake .. && make -j$(nproc)
```
Expected: configures clean, full build still green.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt src/CMakeLists.txt docs/development.md install.sh docker/Dockerfile.cross-pi4
git commit -m "build: add Qt6 WebSockets dependency for External API"
```

---

### Task 3: `ApiFramer` — TCP length-prefix codec (pure class, TDD)

**Files:**
- Create: `src/core/api/ApiFramer.hpp`, `src/core/api/ApiFramer.cpp`
- Modify: `src/CMakeLists.txt` (add to openauto-core SOURCES block, alphabetical near `core/services/...` entries; also `target_link_libraries` gains `prodigy-api-proto` PUBLIC — do it in this task, all later API code needs it)
- Test: `tests/test_api_framing.cpp`

**Interfaces:**
- Produces:
```cpp
namespace oap::api {
class ApiFramer {
public:
    explicit ApiFramer(quint32 maxFrameBytes = 262144);
    QList<QByteArray> feed(const QByteArray& chunk); // complete payloads extracted
    bool violated() const;                            // latched on len==0 or len>max
    static QByteArray encode(const QByteArray& payload); // 4-byte BE len + payload
};
}
```
Header: `#include <QByteArray>`, `#include <QList>`, `#include <QtGlobal>`. Members: `QByteArray buffer_; quint32 maxFrameBytes_; bool violated_ = false;`.

- [ ] **Step 1: Write the failing test** — `tests/test_api_framing.cpp`:

```cpp
#include <QtTest>
#include "core/api/ApiFramer.hpp"

using oap::api::ApiFramer;

static QByteArray lenPrefix(quint32 n) {
    QByteArray b(4, 0);
    b[0] = (n >> 24) & 0xFF; b[1] = (n >> 16) & 0xFF;
    b[2] = (n >> 8) & 0xFF;  b[3] = n & 0xFF;
    return b;
}

class TestApiFraming : public QObject {
    Q_OBJECT
private slots:
    void testEncodeProducesPrefix();
    void testSingleCompleteFrame();
    void testPartialThenRest();
    void testTwoFramesCoalesced();
    void testByteAtATime();
    void testOversizedLengthViolates();
    void testZeroLengthViolates();
};

void TestApiFraming::testEncodeProducesPrefix() {
    QByteArray f = ApiFramer::encode("abc");
    QCOMPARE(f.size(), 7);
    QCOMPARE(f.left(4), lenPrefix(3));
    QCOMPARE(f.mid(4), QByteArray("abc"));
}

void TestApiFraming::testSingleCompleteFrame() {
    ApiFramer fr;
    auto out = fr.feed(ApiFramer::encode("hello"));
    QCOMPARE(out.size(), 1);
    QCOMPARE(out[0], QByteArray("hello"));
    QVERIFY(!fr.violated());
}

void TestApiFraming::testPartialThenRest() {
    ApiFramer fr;
    QByteArray f = ApiFramer::encode("hello");
    QVERIFY(fr.feed(f.left(6)).isEmpty());
    auto out = fr.feed(f.mid(6));
    QCOMPARE(out.size(), 1);
    QCOMPARE(out[0], QByteArray("hello"));
}

void TestApiFraming::testTwoFramesCoalesced() {
    ApiFramer fr;
    auto out = fr.feed(ApiFramer::encode("a") + ApiFramer::encode("bb"));
    QCOMPARE(out.size(), 2);
    QCOMPARE(out[0], QByteArray("a"));
    QCOMPARE(out[1], QByteArray("bb"));
}

void TestApiFraming::testByteAtATime() {
    ApiFramer fr;
    QByteArray f = ApiFramer::encode("xyz");
    QList<QByteArray> all;
    for (char c : f) all += fr.feed(QByteArray(1, c));
    QCOMPARE(all.size(), 1);
    QCOMPARE(all[0], QByteArray("xyz"));
}

void TestApiFraming::testOversizedLengthViolates() {
    ApiFramer fr(16);
    QVERIFY(fr.feed(lenPrefix(17)).isEmpty());
    QVERIFY(fr.violated());
}

void TestApiFraming::testZeroLengthViolates() {
    ApiFramer fr;
    QVERIFY(fr.feed(lenPrefix(0)).isEmpty());
    QVERIFY(fr.violated());
}

QTEST_MAIN(TestApiFraming)
#include "test_api_framing.moc"
```

Register: `oap_add_test(test_api_framing SOURCES test_api_framing.cpp)` in `tests/CMakeLists.txt`.

- [ ] **Step 2: Run to verify failure** — `cd build && cmake .. && make test_api_framing 2>&1 | tail -5` — Expected: compile error, `ApiFramer.hpp` not found.

- [ ] **Step 3: Implement** — `src/core/api/ApiFramer.cpp`:

```cpp
#include "core/api/ApiFramer.hpp"

namespace oap::api {

ApiFramer::ApiFramer(quint32 maxFrameBytes) : maxFrameBytes_(maxFrameBytes) {}

QByteArray ApiFramer::encode(const QByteArray& payload) {
    const quint32 n = static_cast<quint32>(payload.size());
    QByteArray out;
    out.reserve(4 + payload.size());
    out.append(char((n >> 24) & 0xFF));
    out.append(char((n >> 16) & 0xFF));
    out.append(char((n >> 8) & 0xFF));
    out.append(char(n & 0xFF));
    out.append(payload);
    return out;
}

QList<QByteArray> ApiFramer::feed(const QByteArray& chunk) {
    QList<QByteArray> frames;
    if (violated_) return frames;
    buffer_.append(chunk);
    while (buffer_.size() >= 4) {
        const auto* d = reinterpret_cast<const unsigned char*>(buffer_.constData());
        const quint32 len = (quint32(d[0]) << 24) | (quint32(d[1]) << 16)
                          | (quint32(d[2]) << 8) | quint32(d[3]);
        if (len == 0 || len > maxFrameBytes_) {
            violated_ = true;
            buffer_.clear();
            return frames;
        }
        if (quint32(buffer_.size()) < 4 + len) break;
        frames.append(buffer_.mid(4, len));
        buffer_.remove(0, 4 + int(len));
    }
    return frames;
}

bool ApiFramer::violated() const { return violated_; }

} // namespace oap::api
```

Add `core/api/ApiFramer.cpp` to the openauto-core SOURCES list in `src/CMakeLists.txt`, and add `prodigy-api-proto` to its `target_link_libraries` PUBLIC block.

- [ ] **Step 4: Run to verify pass** — `make -j$(nproc) test_api_framing && ctest -R test_api_framing --output-on-failure` — Expected: 7 PASS.

- [ ] **Step 5: Commit** — `git add src/core/api src/CMakeLists.txt tests/ && git commit -m "feat(api): TCP length-prefix framer"`

---

### Task 4: Auth primitives + `PairedClientStore` (TDD)

**Files:**
- Create: `src/core/api/ApiAuth.hpp`, `src/core/api/ApiAuth.cpp`
- Modify: `src/CMakeLists.txt` (add `core/api/ApiAuth.cpp` to SOURCES)
- Test: `tests/test_api_auth.cpp`

**Interfaces:**
- Produces (all in `namespace oap::api`):
```cpp
QByteArray deriveSecret(const QString& pin, const QByteArray& salt); // SHA256(pin utf8 || salt), 32 raw bytes
QByteArray hmacProof(const QByteArray& secret, const QByteArray& nonce); // HMAC-SHA256, raw bytes
bool constantTimeEquals(const QByteArray& a, const QByteArray& b);

struct PairedClient {
    QString clientId;
    QByteArray secret;    // 32 raw bytes
    QString name;
    int kind = 0;         // pb::ClientKind numeric value
    QString pairedAtIso;  // ISO8601
};

class PairedClientStore {
public:
    explicit PairedClientStore(const QString& yamlPath);
    bool load();                       // missing file = ok, empty store
    void save();                       // writes file with 0600 perms
    std::optional<PairedClient> find(const QString& clientId) const;
    void upsert(const PairedClient& c);
    bool remove(const QString& clientId);
    QList<PairedClient> all() const;
private:
    QString path_;
    QList<PairedClient> clients_;
};
```
Implementation notes: `deriveSecret` = `QCryptographicHash::hash(pin.toUtf8() + salt, QCryptographicHash::Sha256)` (raw, NOT hex — differs deliberately from companion's hex-encoded secret). `hmacProof` = `QMessageAuthenticationCode::hash(nonce, secret, QCryptographicHash::Sha256)`. `constantTimeEquals`: length check then single-pass `unsigned char diff |= a[i]^b[i]` loop (do NOT use `==`). YAML layout of the store file:

```yaml
clients:
  - id: "uuid"
    secret_hex: "64 hex chars"
    name: "Matt's Pixel"
    kind: 1
    paired_at: "2026-07-06T12:00:00Z"
```

Persist with yaml-cpp (already a dependency; see `src/core/YamlConfig.cpp` includes for the pattern). Set permissions after write: `QFile::setPermissions(path_, QFileDevice::ReadOwner | QFileDevice::WriteOwner)` — same as `CompanionListenerService.cpp:120-127`.

- [ ] **Step 1: Write the failing test** — `tests/test_api_auth.cpp`:

```cpp
#include <QtTest>
#include "core/api/ApiAuth.hpp"

using namespace oap::api;

class TestApiAuth : public QObject {
    Q_OBJECT
private slots:
    void testDeriveSecretDeterministic();
    void testDeriveSecretSaltMatters();
    void testHmacProofVerifies();
    void testConstantTimeEquals();
    void testStoreRoundTrip();
    void testStorePermissions();
    void testUpsertReplaces();
};

void TestApiAuth::testDeriveSecretDeterministic() {
    QByteArray salt("0123456789abcdef");
    QByteArray s1 = deriveSecret("123456", salt);
    QByteArray s2 = deriveSecret("123456", salt);
    QCOMPARE(s1, s2);
    QCOMPARE(s1.size(), 32);
}

void TestApiAuth::testDeriveSecretSaltMatters() {
    QVERIFY(deriveSecret("123456", "saltA") != deriveSecret("123456", "saltB"));
    QVERIFY(deriveSecret("123456", "saltA") != deriveSecret("654321", "saltA"));
}

void TestApiAuth::testHmacProofVerifies() {
    QByteArray secret = deriveSecret("123456", "salt");
    QByteArray nonce(32, 'n');
    QByteArray proof = hmacProof(secret, nonce);
    QCOMPARE(proof.size(), 32);
    QVERIFY(constantTimeEquals(proof, hmacProof(secret, nonce)));
    QVERIFY(!constantTimeEquals(proof, hmacProof(secret, QByteArray(32, 'x'))));
}

void TestApiAuth::testConstantTimeEquals() {
    QVERIFY(constantTimeEquals("abc", "abc"));
    QVERIFY(!constantTimeEquals("abc", "abd"));
    QVERIFY(!constantTimeEquals("abc", "abcd"));
    QVERIFY(constantTimeEquals("", ""));
}

void TestApiAuth::testStoreRoundTrip() {
    QString path = "/tmp/oap_test_api_clients.yaml";
    QFile::remove(path);
    PairedClientStore store(path);
    QVERIFY(store.load());  // missing file is fine
    QVERIFY(store.all().isEmpty());

    PairedClient c{"id-1", QByteArray(32, 's'), "TestPhone", 1, "2026-07-06T00:00:00Z"};
    store.upsert(c);
    store.save();

    PairedClientStore store2(path);
    QVERIFY(store2.load());
    auto found = store2.find("id-1");
    QVERIFY(found.has_value());
    QCOMPARE(found->secret, QByteArray(32, 's'));
    QCOMPARE(found->name, QString("TestPhone"));
    QVERIFY(!store2.find("nope").has_value());
}

void TestApiAuth::testStorePermissions() {
    QString path = "/tmp/oap_test_api_clients_perm.yaml";
    QFile::remove(path);
    PairedClientStore store(path);
    store.upsert({"id", QByteArray(32, 'k'), "n", 0, ""});
    store.save();
    auto perms = QFile(path).permissions();
    QVERIFY(!(perms & QFileDevice::ReadGroup));
    QVERIFY(!(perms & QFileDevice::ReadOther));
}

void TestApiAuth::testUpsertReplaces() {
    PairedClientStore store("/tmp/oap_test_api_clients2.yaml");
    store.upsert({"id-1", QByteArray(32, 'a'), "A", 0, ""});
    store.upsert({"id-1", QByteArray(32, 'b'), "B", 0, ""});
    QCOMPARE(store.all().size(), 1);
    QCOMPARE(store.find("id-1")->name, QString("B"));
}

QTEST_MAIN(TestApiAuth)
#include "test_api_auth.moc"
```

Register: `oap_add_test(test_api_auth SOURCES test_api_auth.cpp)`.

- [ ] **Step 2: Verify failure** — `cd build && cmake .. && make test_api_auth 2>&1 | tail -3` — Expected: `ApiAuth.hpp` not found.

- [ ] **Step 3: Implement `ApiAuth.cpp`** — headers `<QCryptographicHash>`, `<QMessageAuthenticationCode>`, `<QFile>`, `<QSaveFile>`, `<yaml-cpp/yaml.h>`, `<fstream>`. The three free functions are one-liners per the interface notes above plus:

```cpp
bool constantTimeEquals(const QByteArray& a, const QByteArray& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    return diff == 0;
}
```

Store `load()`: if `!QFile::exists(path_)` return true; parse with `YAML::LoadFile(path_.toStdString())`, iterate `doc["clients"]`, `secret = QByteArray::fromHex(...)`. `save()`: build `YAML::Node`, emit to string, write via QFile, then `setPermissions`. Wrap yaml-cpp parse in try/catch returning false.

- [ ] **Step 4: Verify pass** — `make -j$(nproc) test_api_auth && ctest -R test_api_auth --output-on-failure` — Expected: 7 PASS.

- [ ] **Step 5: Commit** — `git add -A src/core/api tests && git commit -m "feat(api): auth primitives and paired-client store"`

---

### Task 5: `PairingManager` (TDD)

**Files:**
- Create: `src/core/api/PairingManager.hpp`, `src/core/api/PairingManager.cpp`
- Modify: `src/CMakeLists.txt` (SOURCES)
- Test: `tests/test_api_pairing.cpp`

**Interfaces:**
- Consumes: Task 4 (`deriveSecret`, `hmacProof`, `constantTimeEquals`, `PairedClientStore`).
- Produces (`namespace oap::api`):
```cpp
class PairingManager : public QObject {
    Q_OBJECT
public:
    PairingManager(PairedClientStore* store, QObject* parent = nullptr);
    void startWindow(int timeoutSeconds);   // new PIN+salt each call; restarts timer
    void cancelWindow();
    bool windowOpen() const;
    QString currentPin() const;             // "" when closed; 6 digits when open
    QByteArray currentSalt() const;         // 16 random bytes, per-window
    QByteArray makeNonce() const;           // 32 random bytes, caller keeps it
    // Returns new client id on success (persists to store, closes window).
    std::optional<QString> completePairing(const QByteArray& nonce, const QByteArray& proof,
                                           const QString& clientName, int clientKind);
signals:
    void windowChanged();
};
```
Implementation: PIN via `QRandomGenerator::global()->bounded(100000, 999999)` (companion pattern, `CompanionListenerService.cpp:111`); salt/nonce via `QRandomGenerator::global()->generate()` filled loop or `QRandomGenerator::system()->fillRange`. `completePairing`: if window closed return nullopt; compute `expected = hmacProof(deriveSecret(pin_, salt_), nonce)`; `constantTimeEquals(expected, proof)` else nullopt; client id = `QUuid::createUuid().toString(QUuid::WithoutBraces)`; upsert + `store_->save()`; `cancelWindow()`; return id. Window timer: `QTimer` single-shot calling `cancelWindow()`.

- [ ] **Step 1: Write the failing test** — `tests/test_api_pairing.cpp` with cases: `testWindowLifecycle` (closed → open with 6-digit pin → cancel closes, signal count via `QSignalSpy`), `testCompletePairingHappyPath` (derive proof exactly as a client would from `currentPin()`/`currentSalt()`, expect id returned, store contains it, window closed after), `testWrongPinRejected` (proof from wrong pin → nullopt, window STAYS open — one bad guess must not close the window), `testClosedWindowRejects` (nullopt when never opened), `testWindowExpiry` (startWindow(0) — actually use 1s and `QTest::qWait(1100)` — window closes itself). Register `oap_add_test(test_api_pairing SOURCES test_api_pairing.cpp)`.

Test code skeleton (fill the five slots following this pattern):

```cpp
#include <QtTest>
#include "core/api/PairingManager.hpp"
#include "core/api/ApiAuth.hpp"

using namespace oap::api;

class TestApiPairing : public QObject {
    Q_OBJECT
private slots:
    void testWindowLifecycle();
    void testCompletePairingHappyPath();
    void testWrongPinRejected();
    void testClosedWindowRejects();
    void testWindowExpiry();
};

void TestApiPairing::testCompletePairingHappyPath() {
    PairedClientStore store("/tmp/oap_test_pairing_store.yaml");
    QFile::remove("/tmp/oap_test_pairing_store.yaml");
    PairingManager mgr(&store);
    mgr.startWindow(60);
    QByteArray nonce = mgr.makeNonce();
    QByteArray secret = deriveSecret(mgr.currentPin(), mgr.currentSalt());
    auto id = mgr.completePairing(nonce, hmacProof(secret, nonce), "TestPhone", 3);
    QVERIFY(id.has_value());
    QVERIFY(store.find(*id).has_value());
    QCOMPARE(store.find(*id)->secret, secret);
    QVERIFY(!mgr.windowOpen());
}
```

- [ ] **Step 2: Verify failure** — compile error, header missing.
- [ ] **Step 3: Implement** per interface notes. `PairingManager.cpp` needs `#include <QTimer>` (CLAUDE.md gotcha — forward declaration is not enough).
- [ ] **Step 4: Verify pass** — `ctest -R test_api_pairing --output-on-failure` — Expected: 5 PASS.
- [ ] **Step 5: Commit** — `git commit -am "feat(api): PIN pairing manager with windowed challenge/response"`

---

### Task 6: Serializers I — media, projection, system (TDD)

**Files:**
- Create: `src/core/api/ApiSerializers.hpp`, `src/core/api/ApiSerializers.cpp`
- Modify: `src/CMakeLists.txt` (SOURCES); also move the `OAP_GIT_HASH="${OAP_GIT_HASH}"` compile definition from the executable (`src/CMakeLists.txt:460`) so openauto-core has it too: add to the existing `target_compile_definitions(openauto-core ...)` block (or create one) — keep the executable's copy, duplication is harmless.
- Test: `tests/test_api_serializers.cpp`

**Interfaces:**
- Consumes: `prodigy-api-proto` headers; `oap::IMediaStatusProvider`, `oap::IProjectionStatusProvider`, `oap::ThemeService`, `oap::BluetoothManager`.
- Produces (`namespace oap::api::serial`, all pure, header `ApiSerializers.hpp`):
```cpp
prodigy::api::v1::MediaStatus buildMediaStatus(const oap::IMediaStatusProvider& p);
prodigy::api::v1::ProjectionStatus buildProjectionStatus(const oap::IProjectionStatusProvider& p);
prodigy::api::v1::SystemStatus buildSystemStatus(oap::ThemeService& theme,
                                                 const QString& appVersion,
                                                 oap::BluetoothManager* bt /*nullable*/);
// Task 7 adds:
prodigy::api::v1::PhoneStatus buildPhoneStatus(const oap::IPhoneStateService& p,
                                               qint64 activeCallStartedAtMs);
prodigy::api::v1::NavigationStatus buildNavigationStatus(const oap::INavigationProvider& p);
```

**Normalization tables (normative — design doc §8; copy exactly):**

Media source: `""`→`MEDIA_SOURCE_NONE`, `"Bluetooth"`→`MEDIA_SOURCE_BLUETOOTH`, `"AndroidAuto"`→`MEDIA_SOURCE_ANDROID_AUTO`, anything else→`MEDIA_SOURCE_UNSPECIFIED`.

Playback state is **source-dependent** (the same int means different things per source — this is the trap):

| source | raw int | pb::PlaybackState |
|---|---|---|
| Bluetooth (`BtAudioPlugin.hpp:50-53`) | 0 | `PLAYBACK_STATE_STOPPED` |
| Bluetooth | 1 | `PLAYBACK_STATE_PLAYING` |
| Bluetooth | 2 | `PLAYBACK_STATE_PAUSED` |
| AndroidAuto (`MediaStatusChannelHandler.hpp:20-23`) | 1 | `PLAYBACK_STATE_STOPPED` |
| AndroidAuto | 2 | `PLAYBACK_STATE_PLAYING` |
| AndroidAuto | 3 | `PLAYBACK_STATE_PAUSED` |
| any | other | `PLAYBACK_STATE_UNSPECIFIED` |
| source NONE | any | `PLAYBACK_STATE_UNSPECIFIED` |

Projection (explicit switch on `p.projectionState()`, never static_cast): 0→`PROJECTION_STATE_DISCONNECTED`, 1→`_WAITING_FOR_DEVICE`, 2→`_CONNECTING`, 3→`_PROJECTING`, 4→`_BACKGROUNDED`, default→`_UNSPECIFIED`.

System: `night_mode = theme.realNightMode()`; `theme_id = theme.currentThemeId()`; `app_version = appVersion` (composed by caller as `config identity.sw_version + " (" + OAP_GIT_HASH + ")"`); `bluetooth.connected/device_name` from `bt->connectedDeviceName()` (connected = !name.isEmpty()) or false/"" when `bt == nullptr`. `theme_tokens`: iterate this exact 45-name list, value `theme.color(name).name()` (QColor `#rrggbb`):

```cpp
static const char* kThemeTokens[] = {
    "primary","on-primary","primary-container","on-primary-container",
    "secondary","on-secondary","secondary-container","on-secondary-container",
    "tertiary","on-tertiary","tertiary-container","on-tertiary-container",
    "error","on-error","error-container","on-error-container",
    "background","on-background","surface","on-surface",
    "surface-variant","on-surface-variant","surface-dim","surface-bright",
    "surface-container-lowest","surface-container-low","surface-container",
    "surface-container-high","surface-container-highest",
    "outline","outline-variant",
    "inverse-surface","inverse-on-surface","inverse-primary",
    "scrim","shadow",
    "success","on-success","surface-tint-high","surface-tint-highest",
    "warning","on-warning",
};
```
(42 entries — the ThemeService getter surface at `ThemeService.hpp:34-91`; `IThemeService::color(name)` resolves hyphenated names via `activeColor`.)

- [ ] **Step 1: Write the failing tests** — `tests/test_api_serializers.cpp`. Drive REAL services: `oap::MediaStatusService media; media.setBtConnected(true); media.updateBtMetadata("T","A","Al"); media.updateBtPlaybackState(1);` then assert `buildMediaStatus(media)` fields (`PLAYBACK_STATE_PLAYING`, `MEDIA_SOURCE_BLUETOOTH`). Add the AA-trap case: `media.setAaConnected(true); media.updateAaPlaybackState(1);` → expect `PLAYBACK_STATE_STOPPED` (AA 1 = Stopped!). Projection: `ProjectionStatusProvider` wraps a plain `QObject src; src.setProperty("connectionState", 3); src.setProperty("statusMessage", "ok");` → `PROJECTION_STATE_PROJECTING`. System: `oap::ThemeService theme;` → assert `theme_tokens_size() == 42`, every value matches `^#[0-9a-f]{6}$` (QRegularExpression), `night_mode` matches `theme.realNightMode()`, and with `bt=nullptr` bluetooth is `connected=false`. Register `oap_add_test(test_api_serializers SOURCES test_api_serializers.cpp)`.
- [ ] **Step 2: Verify failure** — header missing.
- [ ] **Step 3: Implement** the three builders per the tables. Include what you use: `"core/services/MediaStatusService.hpp"` etc. in the test; the serializer header includes only the interface headers + pb headers.
- [ ] **Step 4: Verify pass** — `ctest -R test_api_serializers --output-on-failure`.
- [ ] **Step 5: Commit** — `git commit -am "feat(api): media/projection/system serializers with normalization tables"`

---

### Task 7: Serializers II — phone, navigation (TDD)

**Files:**
- Modify: `src/core/api/ApiSerializers.hpp/.cpp` (add the two builders), `tests/test_api_serializers.cpp` (add cases)

**Interfaces:** produces the two functions declared in Task 6's block.

**Normalization tables (normative):**

Call state (`ICallStateProvider::CallState`, `ICallStateProvider.hpp:14` — only 3 values exist today): `Idle(0)` → `calls` empty; `Ringing(1)` → one `Call{state=CALL_STATE_INCOMING}`; `Active(2)` → one `Call{state=CALL_STATE_ACTIVE, started_at_unix_ms=activeCallStartedAtMs}`. `line_identification = p.callerNumber()`, `display_name = p.callerName()`. When D2's TelephonyClient widens the provider enum, this switch grows — that is the designed seam.

`hfp_connected = p.phoneConnected()`, `device_name = p.deviceName()` (plain getters, NOT Q_PROPERTYs — call them directly). Capabilities in v1: all six flags false (the provider is a UI mock until Phase D2; `can_hold_swap`/`can_multiparty` stay false permanently in v1 per the design doc §8.4).

Navigation — maneuver mapping from the raw AA code (`INavigationProvider::maneuverType()`; values are the oaa `ManeuverTypeEnum.proto` list, read-only verified 2026-07-06). Table (raw → `ManeuverType`, `TurnSide`):

| raw | pb ManeuverType | pb TurnSide |
|---|---|---|
| 0 | `MANEUVER_TYPE_UNSPECIFIED` | unspecified |
| 1 | `MANEUVER_TYPE_DEPART` | unspecified |
| 2 | `MANEUVER_TYPE_NAME_CHANGE` | unspecified |
| 3 | `MANEUVER_TYPE_KEEP` | LEFT |
| 4 | `MANEUVER_TYPE_KEEP` | RIGHT |
| 5 | `MANEUVER_TYPE_SLIGHT_TURN` | LEFT |
| 6 | `MANEUVER_TYPE_SLIGHT_TURN` | RIGHT |
| 7 | `MANEUVER_TYPE_TURN` | LEFT |
| 8 | `MANEUVER_TYPE_TURN` | RIGHT |
| 9 | `MANEUVER_TYPE_SHARP_TURN` | LEFT |
| 10 | `MANEUVER_TYPE_SHARP_TURN` | RIGHT |
| 11 | `MANEUVER_TYPE_U_TURN` | LEFT |
| 12 | `MANEUVER_TYPE_U_TURN` | RIGHT |
| 13–20 | `MANEUVER_TYPE_ON_RAMP` | odd raw = LEFT, even raw = RIGHT |
| 21–24 | `MANEUVER_TYPE_OFF_RAMP` | odd raw = LEFT, even raw = RIGHT |
| 25 | `MANEUVER_TYPE_FORK` | LEFT |
| 26 | `MANEUVER_TYPE_FORK` | RIGHT |
| 27 | `MANEUVER_TYPE_MERGE` | LEFT |
| 28 | `MANEUVER_TYPE_MERGE` | RIGHT |
| 29 | `MANEUVER_TYPE_MERGE` | unspecified |
| 32–35 | `MANEUVER_TYPE_ROUNDABOUT_ENTER_AND_EXIT` | unspecified |
| 36 | `MANEUVER_TYPE_STRAIGHT` | unspecified |
| 37, 38 | `MANEUVER_TYPE_FERRY` | unspecified |
| 39, 40 | `MANEUVER_TYPE_DESTINATION` | unspecified (40 = straight) |
| 41 | `MANEUVER_TYPE_DESTINATION` | LEFT |
| 42 | `MANEUVER_TYPE_DESTINATION` | RIGHT |
| 43, 45 | `MANEUVER_TYPE_ROUNDABOUT_ENTER` | unspecified |
| 44, 46 | `MANEUVER_TYPE_ROUNDABOUT_EXIT` | unspecified |
| 47, 49 | `MANEUVER_TYPE_FERRY` | LEFT |
| 48, 50 | `MANEUVER_TYPE_FERRY` | RIGHT |
| anything else | `MANEUVER_TYPE_OTHER` | unspecified |

Implement as one `switch`. Ignore `INavigationProvider::turnDirection()` for the side — the side is encoded in the maneuver code itself (turnDirection is redundant AA data; using one source avoids disagreement). `nav_active`, `road_name`, `formatted_distance` pass through. `distance_meters`: leave 0 in this task — populated by Task 13's interface promotion (the proto field exists; serializer gains one line there).

- [ ] **Step 1: Add failing tests** — phone: build a `PhoneStateService` (constructor takes parent only; do NOT call `startDBusMonitoring()`), drive with `setIncomingCall("+15551234567", "Alice")` → expect one call, `CALL_STATE_INCOMING`, `line_identification == "+15551234567"`; then `answer()` → `CALL_STATE_ACTIVE` with the passed `activeCallStartedAtMs` echoed; then `hangup()` → empty calls. All capability flags false. Navigation: `INavigationProvider` is abstract — write a 15-line `FakeNavProvider` in the test implementing the getters from member fields; spot-check raws 7 (TURN/LEFT), 14 (ON_RAMP/RIGHT), 36 (STRAIGHT), 39 (DESTINATION), 51 (OTHER), 33 (ROUNDABOUT_ENTER_AND_EXIT).
- [ ] **Step 2: Verify failure** — undefined symbols.
- [ ] **Step 3: Implement** both builders.
- [ ] **Step 4: Verify pass** — `ctest -R test_api_serializers --output-on-failure`.
- [ ] **Step 5: Commit** — `git commit -am "feat(api): phone/navigation serializers per HFP semantics and maneuver table"`

---

### Task 8: Transports — `IApiTransport`, TCP, WebSocket (TDD)

**Files:**
- Create: `src/core/api/ApiTransport.hpp`, `src/core/api/ApiTransport.cpp`
- Modify: `src/CMakeLists.txt` (SOURCES)
- Test: `tests/test_api_transports.cpp`

**Interfaces:**
- Consumes: `ApiFramer` (Task 3).
- Produces (`namespace oap::api`):
```cpp
class IApiTransport : public QObject {
    Q_OBJECT
public:
    explicit IApiTransport(QObject* parent = nullptr) : QObject(parent) {}
    virtual void sendMessage(const QByteArray& serialized) = 0;
    virtual qint64 bytesToWrite() const = 0;
    virtual void close() = 0;
    virtual QHostAddress peerAddress() const = 0;
signals:
    void messageReceived(const QByteArray& serialized);
    void closed();
};

class TcpApiTransport : public IApiTransport {
    Q_OBJECT
public:
    // Takes ownership (reparents the socket).
    TcpApiTransport(QTcpSocket* socket, quint32 maxFrameBytes, QObject* parent = nullptr);
    // sendMessage frames with ApiFramer::encode; readyRead feeds framer_;
    // framer violation -> close(). closed() relayed from disconnected().
};

class WsApiTransport : public IApiTransport {
    Q_OBJECT
public:
    WsApiTransport(QWebSocket* socket, quint32 maxFrameBytes, QObject* parent = nullptr);
    // binaryMessageReceived -> size check vs maxFrameBytes (violation -> close)
    //   -> messageReceived. textMessageReceived is a violation -> close().
    // sendMessage -> sendBinaryMessage.
};
```
`IApiTransport` needs a `.cpp` TU for MOC — put the two concrete implementations and nothing else in `ApiTransport.cpp`; the interface's virtual destructor keeps MOC happy in the same TU (CLAUDE.md gotcha: Q_OBJECT header-only classes need a .cpp listed in CMakeLists).

- [ ] **Step 1: Write the failing test** — `tests/test_api_transports.cpp`. Pattern: `libs/prodigy-oaa-protocol/tests/test_tcp_transport.cpp` (ephemeral-port loopback). Cases:
  - `testTcpRoundTrip`: `QTcpServer` on `QHostAddress::LocalHost` port 0; client `QTcpSocket` connects; wrap the server-side `nextPendingConnection()` in `TcpApiTransport`; client writes `ApiFramer::encode("payload")` raw; `QSignalSpy(transport, &IApiTransport::messageReceived)` → one message == "payload". Then `transport->sendMessage("reply")` → client reads 4+5 bytes with the correct prefix.
  - `testTcpSplitDelivery`: client writes the frame in two halves with `QTest::qWait(50)` between → still exactly one messageReceived.
  - `testTcpFramingViolationCloses`: client writes a 4-byte zero length prefix → `closed()` spy fires.
  - `testWsRoundTrip`: `QWebSocketServer(QStringLiteral("t"), QWebSocketServer::NonSecureMode)` listen port 0; `QWebSocket` client `open(QUrl(QString("ws://127.0.0.1:%1").arg(port)))`; wrap server's `nextPendingConnection()` in `WsApiTransport`; client `sendBinaryMessage("payload")` → messageReceived; server `sendMessage` → client `binaryMessageReceived`.
  - `testWsTextFrameCloses`: client `sendTextMessage("nope")` → closed() fires.
  Pump with `QTRY_COMPARE(spy.count(), 1)` (QtTest's event-loop-aware retry macro) rather than manual processEvents loops. Register `oap_add_test(test_api_transports SOURCES test_api_transports.cpp)` and add `set_tests_properties(test_api_transports PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")` next to the existing socket-test property lines (`tests/CMakeLists.txt:170-175`).
- [ ] **Step 2: Verify failure** — header missing.
- [ ] **Step 3: Implement.** Includes: `<QTcpSocket>`, `<QWebSocket>`, `<QHostAddress>`. TCP `bytesToWrite()` = `socket_->bytesToWrite()`; WS = `socket_->bytesToWrite()` (exists on QWebSocket since Qt 5.12).
- [ ] **Step 4: Verify pass** — `ctest -R test_api_transports --output-on-failure` — Expected: 5 PASS.
- [ ] **Step 5: Commit** — `git commit -am "feat(api): TCP and WebSocket transports behind IApiTransport"`

---

### Task 9: Topic publishers (coalescing fan-in)

**Files:**
- Create: `src/core/api/ApiPublishers.hpp`, `src/core/api/ApiPublishers.cpp`
- Modify: `src/CMakeLists.txt` (SOURCES)
- Test: `tests/test_api_publishers.cpp`

**Interfaces:**
- Consumes: serializers (Tasks 6–7), provider interfaces.
- Produces (`namespace oap::api`):
```cpp
class TopicPublisher : public QObject {
    Q_OBJECT
public:
    TopicPublisher(prodigy::api::v1::Topic topic, QObject* parent = nullptr);
    prodigy::api::v1::Topic topic() const { return topic_; }
    QByteArray snapshotBytes();  // buildEnvelope() serialized now (request_id 0)
signals:
    void statusReady(prodigy::api::v1::Topic topic, const QByteArray& envelopeBytes);
protected:
    void scheduleEmit();  // 0-ms single-shot QTimer; collapses same-turn bursts
    virtual prodigy::api::v1::ApiMessage buildEnvelope() = 0;
private:
    prodigy::api::v1::Topic topic_;
    QTimer coalesce_;
};

class MediaPublisher : public TopicPublisher {
    Q_OBJECT
public:
    explicit MediaPublisher(oap::IMediaStatusProvider* p, QObject* parent = nullptr);
    // ctor: connect(p, &IMediaStatusProvider::mediaStatusChanged, this, [this]{ scheduleEmit(); });
protected:
    prodigy::api::v1::ApiMessage buildEnvelope() override; // wraps buildMediaStatus(*p_)
};
// NavigationPublisher: connects navActiveChanged + turnDataChanged + distanceChanged
// ProjectionPublisher: projectionStateChanged + statusMessageChanged
// SystemPublisher(ThemeService*, QString appVersion, BluetoothManager* /*nullable*/):
//   modeChanged + colorsChanged + currentThemeIdChanged (+ BluetoothManager's
//   connectedDeviceNameChanged signal if non-null — check exact NOTIFY name in
//   BluetoothManager.hpp:30 before wiring)
// PhonePublisher(IPhoneStateService*): callStateChanged + connectionChanged.
//   NOT callDurationChanged (design §8.4). Holds startedAtMs_: on transition
//   to Active (track previous callState int) set startedAtMs_ =
//   QDateTime::currentMSecsSinceEpoch() - p->callDuration()*1000; on Idle reset to 0.
//   buildEnvelope() calls buildPhoneStatus(*p_, startedAtMs_).
```
`buildEnvelope()` in each: `pb::ApiMessage m; m.set_request_id(0); *m.mutable_media_status() = serial::buildMediaStatus(*p_); return m;` — the envelope setter differs per class (`mutable_navigation_status`, etc.).

- [ ] **Step 1: Write the failing test** — `tests/test_api_publishers.cpp`:
  - `testSnapshotBytesParses`: `MediaStatusService` + `MediaPublisher`; parse `snapshotBytes()` as `pb::ApiMessage`, expect `payload_case() == kMediaStatus`, `request_id == 0`.
  - `testCoalescingCollapsesBursts`: `QSignalSpy spy(&pub, &TopicPublisher::statusReady);` call `media.updateBtMetadata("a","b","c")` then `media.updateBtPlaybackState(1)` back-to-back (two mediaStatusChanged emissions, same event-loop turn) → `QTest::qWait(20)` → `QCOMPARE(spy.count(), 1)`.
  - `testSeparateTurnsSeparateEmits`: mutate, `QTest::qWait(20)`, mutate again, `QTest::qWait(20)` → spy.count() == 2.
  - `testPhoneStartedAtSynthesis`: `PhoneStateService` + `PhonePublisher`; `setIncomingCall(...)`; `answer()`; wait; parse last emitted envelope → `calls(0).started_at_unix_ms()` within ±5s of now. `hangup()`; wait; → empty calls.
  Register with offscreen env like Task 8.
- [ ] **Step 2: Verify failure.**
- [ ] **Step 3: Implement.** `#include <QTimer>` (gotcha). `scheduleEmit()`: `if (!coalesce_.isActive()) coalesce_.start(0);` with `coalesce_` single-shot connected to serialize+emit.
- [ ] **Step 4: Verify pass** — `ctest -R test_api_publishers --output-on-failure`.
- [ ] **Step 5: Commit** — `git commit -am "feat(api): coalescing topic publishers for five status domains"`

---

### Task 10: `ApiSession` — handshake state machine + backpressure

**Files:**
- Create: `src/core/api/ApiSession.hpp`, `src/core/api/ApiSession.cpp`
- Modify: `src/CMakeLists.txt` (SOURCES)
- Test: `tests/test_api_session.cpp`

**Interfaces:**
- Consumes: `IApiTransport` (Task 8), `PairingManager`/`PairedClientStore`/auth primitives (Tasks 4–5).
- Produces (`namespace oap::api`):
```cpp
// Everything the session needs injected; ApiServer fills this once.
struct ApiSessionDeps {
    PairingManager* pairing = nullptr;
    PairedClientStore* store = nullptr;
    // Topics currently servable + static capabilities snapshot builder.
    std::function<prodigy::api::v1::Capabilities()> capabilities;
    // Snapshot bytes for one topic, or empty if unavailable.
    std::function<QByteArray(prodigy::api::v1::Topic)> snapshotFor;
    // Non-handshake, non-subscribe requests (Task 11 handler). May be null in tests.
    class IApiRequestSink* requests = nullptr;
    QString serverName;
    QString appVersion;
    qint64 maxQueueBytes = 1048576;
    int handshakeTimeoutMs = 5000;
};

// Interface Task 11 implements; session forwards routed requests here.
class IApiRequestSink {
public:
    virtual ~IApiRequestSink() = default;
    // ctx.requestId echoes into the response the sink sends via session->sendMessage.
    virtual void handleRequest(class ApiSession* session, quint64 requestId,
                               const prodigy::api::v1::ApiMessage& msg) = 0;
    virtual void sessionClosed(class ApiSession* session) = 0;
};

class ApiSession : public QObject {
    Q_OBJECT
public:
    enum class State { ExpectHello, AuthPending, PairingPending, Ready, Closed };
    ApiSession(IApiTransport* transport /*ownership*/, ApiSessionDeps deps, QObject* parent = nullptr);
    State state() const;
    QString clientId() const;        // "" for localhost-trusted sessions
    QString clientName() const;
    bool subscribedTo(prodigy::api::v1::Topic t) const;
    void deliver(const QByteArray& envelopeBytes);           // enforces queue cap
    void sendMessage(quint64 requestId, prodigy::api::v1::ApiMessage msg); // sets id, serializes, cap-checked send
    void closeWithError(quint64 requestId, prodigy::api::v1::ErrorCode code, const QString& text);
    void setPeerTrustOverrideForTest(std::optional<bool> trusted);
signals:
    void becameReady();
    void terminated();   // emitted exactly once, from the single teardown path
};
```
State machine behavior (design doc §4 — implement exactly):
- ctor: start single-shot handshake timer (`handshakeTimeoutMs`); connect transport `messageReceived`/`closed`.
- Trust decision: `peerTrustOverride_.value_or(transport_->peerAddress().isLoopback())`.
- `ExpectHello` + `client_hello`: version != 1 → `closeWithError(id, UNSUPPORTED_VERSION, ...)`. Trusted → send ServerHello (sets `api_version_major(1)`, `api_version_minor(0)`, serverName, appVersion, `session_id` = new UUID, capabilities from deps), state=Ready, stop timer, emit becameReady. Untrusted + `auth.client_id` known in store → send `AuthRequired{nonce}` (remember nonce), state=AuthPending. Untrusted + `auth.pairing_request` + window open → send `PairingChallenge{nonce, salt}` (nonce from `pairing->makeNonce()`, salt = `pairing->currentSalt()`), state=PairingPending. Anything else → `AuthReject` + teardown.
- `AuthPending` + `auth_response`: look up client, `constantTimeEquals(hmacProof(client.secret, nonce_), proof)` → ServerHello/Ready, else AuthReject + teardown.
- `PairingPending` + `pairing_response`: `pairing->completePairing(nonce_, proof, helloName_, helloKind_)` → ServerHello with `granted_client_id`, Ready; else AuthReject + teardown.
- Any message type illegal for the current state → `closeWithError(0, INVALID_REQUEST, ...)`.
- `Ready` routing inside the session itself: `subscribe_request` (build SubscribeResponse: accepted iff `deps.snapshotFor(topic)` non-empty; then send snapshots for accepted topics), `unsubscribe_request` → Ack, `get_capabilities_request` → CapabilitiesResponse, `ping` → Pong. EVERYTHING else → `deps.requests->handleRequest(this, id, msg)` (if null sink: `closeWithError(id, INTERNAL, "no handler")`).
- `deliver()`/`sendMessage()`: before writing, if `transport_->bytesToWrite() + bytes.size() > deps.maxQueueBytes` → teardown (slow consumer). `deliver` also gates on `state==Ready && subscribedTo(topic)` — session stores subscriptions as `QSet<int>`; the caller (ApiServer) checks `subscribedTo` too, belt and braces.
- Teardown (single path): idempotent guard bool; state=Closed; notify `deps.requests->sessionClosed(this)`; `transport_->close()`; emit `terminated()`.

- [ ] **Step 1: Write the failing test** — `tests/test_api_session.cpp`. Write a `FakeTransport : IApiTransport` in-file: records `sent` QList<QByteArray>, settable `peer`, settable `fakeBytesToWrite`; `injectMessage(bytes)` emits messageReceived. Helper `pb::ApiMessage lastSent()` parses `sent.last()`. Cases:
  - `testHelloTrustedGoesReady`: loopback peer; inject ClientHello(major=1) → lastSent is ServerHello with non-empty session_id, `state()==Ready`.
  - `testBadVersionRejected`: ClientHello(major=2) → lastSent is Error UNSUPPORTED_VERSION, terminated() spy fired.
  - `testFirstMessageMustBeHello`: inject Ping first → Error INVALID_REQUEST + terminated.
  - `testHandshakeTimeout`: deps.handshakeTimeoutMs=50; construct; `QTest::qWait(120)` → terminated.
  - `testRemoteAuthHappyPath`: trust override false; seed store with client (known secret); ClientHello with client_id → lastSent AuthRequired with 32-byte nonce; inject AuthResponse with `hmacProof(secret, nonce)` → ServerHello, Ready.
  - `testRemoteAuthBadProof`: as above with wrong proof → AuthReject + terminated.
  - `testPairingFlow`: trust override false; open window on PairingManager; ClientHello(pairing_request) → PairingChallenge; compute proof from `currentPin()`+salt → ServerHello has granted_client_id, store now contains it.
  - `testSubscribeSnapshotAndAck`: Ready session; deps.snapshotFor returns canned bytes for TOPIC_MEDIA, empty else; inject Subscribe([MEDIA, PHONE]) → SubscribeResponse marks MEDIA accepted, PHONE rejected; next sent message == canned snapshot; `subscribedTo(MEDIA)` true.
  - `testQueueCapDisconnects`: Ready; `fakeBytesToWrite = deps.maxQueueBytes`; `deliver(bytes)` → terminated.
  - `testPingPong`: Ready; inject Ping(id=7) → Pong with request_id 7.
  Register with offscreen env.
- [ ] **Step 2: Verify failure.**
- [ ] **Step 3: Implement `ApiSession.cpp`.** Parse with `pb::ApiMessage m; if (!m.ParseFromArray(bytes.constData(), bytes.size())) { closeWithError(0, INVALid...) }` — note INVALID_REQUEST + teardown on parse failure. Switch on `m.payload_case()`.
- [ ] **Step 4: Verify pass** — `ctest -R test_api_session --output-on-failure` — Expected: 10 PASS.
- [ ] **Step 5: Commit** — `git commit -am "feat(api): session state machine with auth, subscribe, backpressure"`

---

### Task 11: Request bridges — actions, notifications, phone, companion ingest

**Files:**
- Create: `src/core/api/ApiRequestHandlers.hpp`, `src/core/api/ApiRequestHandlers.cpp`
- Create: `src/core/api/ApiInboundState.hpp`, `src/core/api/ApiInboundState.cpp`
- Modify: `src/CMakeLists.txt` (SOURCES ×2)
- Test: `tests/test_api_request_handlers.cpp`

**Interfaces:**
- Consumes: `IApiRequestSink`/`ApiSession` (Task 10), `oap::ActionRegistry`, `oap::INotificationService`, `oap::IPhoneStateService`.
- Produces (`namespace oap::api`):
```cpp
class ApiInboundState : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool gpsValid READ gpsValid NOTIFY gpsChanged)
    Q_PROPERTY(double gpsLat READ gpsLat NOTIFY gpsChanged)
    Q_PROPERTY(double gpsLon READ gpsLon NOTIFY gpsChanged)
    Q_PROPERTY(double gpsSpeedMps READ gpsSpeedMps NOTIFY gpsChanged)
    Q_PROPERTY(int phoneBattery READ phoneBattery NOTIFY batteryChanged)
    Q_PROPERTY(bool phoneCharging READ phoneCharging NOTIFY batteryChanged)
    Q_PROPERTY(bool internetAvailable READ internetAvailable NOTIFY internetChanged)
    Q_PROPERTY(QString proxyAddress READ proxyAddress NOTIFY internetChanged)
public:
    // setters used by the handler: setGps(...), setBattery(...),
    // setConnectivity(peerHost, active, port, password)  // password from
    //   ConnectivityReport.socks5_password ("" when unset)
    // proxyAddress composes "socks5://<peerHost>:<port>" like CompanionListenerService.cpp:427
signals:
    void gpsChanged(); void batteryChanged(); void internetChanged();
    void timeReported(qint64 unixMs);
    void proxyRouteChanged(bool active, QString host, quint16 port, QString password);
};

class ApiRequestHandlers : public QObject, public IApiRequestSink {
    Q_OBJECT
public:
    struct Deps {
        oap::ActionRegistry* actions = nullptr;
        oap::INotificationService* notifications = nullptr;
        oap::IPhoneStateService* phone = nullptr;
        ApiInboundState* inbound = nullptr;
    };
    explicit ApiRequestHandlers(Deps deps, QObject* parent = nullptr);
    void handleRequest(ApiSession* session, quint64 requestId,
                       const prodigy::api::v1::ApiMessage& msg) override;
    void sessionClosed(ApiSession* session) override;
};
```
Behavior (design doc §9–§10 — implement exactly):
- **list_actions**: for each id in `actions->registeredActions()` emit `ActionInfo{id, label: clientLabels_.value(id), client_owned: clientOwners_.contains(id)}`.
- **dispatch_action**: payload_json → QVariant via `QJsonDocument::fromJson` (`.object()`/`.array()` → toVariant; bare scalars: wrap input as `[value]` then take first — or use `QJsonValue` parse trick: `QJsonDocument::fromJson("[" + json + "]")`). `dispatched = actions->dispatch(id, payload)`.
- **register_actions**: per spec — reject when `actions->registeredActions().contains(id)` ("duplicate id") or id starts with any of `{"app.","aa.","navbar.","theme.","media.","phone.","system.","overlay.","api."}` ("reserved prefix"). Accept: `actions->registerAction(id, [this, id](const QVariant& payload){ forwardInvocation(id, payload); })`; record `clientOwners_[id] = session`, `clientLabels_[id] = label`. `forwardInvocation`: serialize payload back to JSON (`QJsonDocument(QJsonArray{QJsonValue::fromVariant(payload)})` → strip brackets, or store raw string alongside) and `owner->sendMessage(0, envelope with ActionInvokedEvent)`.
- **unregister_actions**: only ids where `clientOwners_[id] == session`: `actions->unregisterAction(id)`, erase maps. Respond Ack.
- **sessionClosed**: same loop over every id owned by that session (the auto-unregister invariant), plus drop its notification ownership set.
- **post_notification**: build QVariantMap `{{"kind","toast"},{"message",...},{"sourcePluginId","api:"+ (session->clientId().isEmpty() ? "localhost" : session->clientId())},{"priority", req.has_priority() ? qBound(0u, req.priority(), 100u) : 50},{"ttlMs", ttl}}` → `notifications->post(map)`; record id in `notificationOwners_[session]`; respond PostNotificationResponse. `priority` is proto3 `optional` (Codex fix) — an explicit 0 must reach the service as 0; only an ABSENT field becomes 50.
- **dismiss_notification**: if id in `notificationOwners_[session]` → `notifications->dismiss(id)` + Ack; else Error NOT_FOUND.
- **dial/answer/hangup/send_dtmf**: capability flags and command results MUST NOT contradict (Codex review 2026-07-06 — the proto is the client contract, phone.proto file header). v1 capabilities are all-false (Task 7), so ALL FOUR commands respond `PhoneCommandResponse{UNAVAILABLE}` unconditionally. Do NOT wire the mock provider's `answer()`/`hangup()` — they only flip local UI state, which is not answering a real call. Keep the logic in one `phoneCommand()` helper so D2 swaps in real `TelephonyClient` calls and truthful flags in one place.
- **gps/battery/connectivity/time reports**: update `inbound` setters (connectivity uses `session` transport peer host); NO response ever. Malformed/out-of-range → log qWarning and drop.
- Anything else → `session->closeWithError(requestId, INVALID_REQUEST, "unroutable payload")`.

- [ ] **Step 1: Write the failing test.** Reuse the `FakeTransport` pattern (copy the ~30-line fake into this file; tests may not share headers). Build real `ActionRegistry`, `NotificationService`, `PhoneStateService`, `ApiInboundState`, session in Ready state (loopback trust) with the handler as sink. Cases: `testListContainsRegistered`, `testDispatchUnknownFalse`, `testRegisterReservedPrefixRejected` (try `"media.hack"`), `testRegisterDuplicateRejected`, `testClientActionRoundTrip` (register `"testapp.ping"`, dispatch it via `ActionRegistry::dispatch` directly → session receives ActionInvokedEvent), `testDisconnectUnregisters` (teardown session → `registeredActions()` no longer contains it), `testNotificationOwnership` (post from session A, dismiss from B → NOT_FOUND; from A → gone), `testNotificationPriorityZeroHonored` (post with explicit `priority=0` → service map carries 0; post with priority ABSENT → 50), `testAllPhoneCommandsUnavailable` (inject Dial, Answer, Hangup, SendDtmf — each → `PHONE_COMMAND_RESULT_UNAVAILABLE`, even after `setIncomingCall(...)`: capabilities are false, the contract wins over the mock), `testGpsReportUpdatesState` (+ spy on gpsChanged), `testConnectivityEmitsProxyRoute` (report WITH `socks5_password` → signal carries it; without → empty QString), `testTimeReportSignal`.
- [ ] **Step 2: Verify failure.**
- [ ] **Step 3: Implement both classes.**
- [ ] **Step 4: Verify pass** — `ctest -R test_api_request_handlers --output-on-failure`.
- [ ] **Step 5: Commit** — `git commit -am "feat(api): request bridges for actions, notifications, phone, companion ingest"`

---

### Task 12: `ApiServer` — listeners, peer policy, composition

**Files:**
- Create: `src/core/api/ApiServer.hpp`, `src/core/api/ApiServer.cpp`
- Modify: `src/CMakeLists.txt` (SOURCES)
- Test: `tests/test_api_server.cpp`

**Interfaces:**
- Consumes: everything from Tasks 3–11.
- Produces (`namespace oap::api`):
```cpp
struct ApiServiceRefs {           // filled in main.cpp (Task 13)
    oap::IMediaStatusProvider* media = nullptr;
    oap::INavigationProvider* navigation = nullptr;      // nullable
    oap::IProjectionStatusProvider* projection = nullptr; // nullable
    oap::IPhoneStateService* phone = nullptr;
    oap::ThemeService* theme = nullptr;
    oap::INotificationService* notifications = nullptr;
    oap::ActionRegistry* actions = nullptr;
    oap::IConfigService* config = nullptr;
    oap::BluetoothManager* bluetooth = nullptr;          // nullable
};

class ApiServer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool pairingActive READ pairingActive NOTIFY pairingChanged)
    Q_PROPERTY(QString pairingPin READ pairingPin NOTIFY pairingChanged)
public:
    ApiServer(ApiServiceRefs refs, QObject* parent = nullptr);
    bool start();                       // reads api.* config; returns false if both listens fail
    void stop();
    quint16 tcpPort() const;            // actual bound ports (for tests: config 0 = ephemeral)
    quint16 wsPort() const;
    ApiInboundState* inboundState();    // for main.cpp wiring
    Q_INVOKABLE void startPairing();    // opens window; also registered as action api.pairing.start
    Q_INVOKABLE void cancelPairing();
    bool pairingActive() const;
    QString pairingPin() const;
    int sessionCount() const;           // tests
signals:
    void pairingChanged();
};
```
Implementation:
- `start()`: read config (`api.enabled` default true — if false return false without listening; `api.tcp_port` 9810, `api.ws_port` 9811, `api.expose_lan` false, `api.max_queue_bytes` 1048576, `api.pairing_timeout_s` 120, `api.handshake_timeout_ms` 5000) via `refs_.config->value("api.tcp_port")` with `.isValid() ? ... : default` fallbacks (companion pattern, `main.cpp:310-313`). Listen both servers on `QHostAddress::Any`. `QWebSocketServer` in `NonSecureMode`, name `"prodigy-api"`.
- Peer policy on `newConnection`/`newWebSocketConnection`: accept iff `addr.isLoopback() || inApSubnet(addr) || exposeLan_` where `inApSubnet` = IPv4 in `10.0.0.0/24` (`addr.isInSubnet(QHostAddress("10.0.0.0"), 24)` — normalize v4-mapped-v6 with `QHostAddress(addr.toIPv4Address())` when `addr.protocol()!=IPv4` and `toIPv4Address` yields nonzero). Rejected: `socket->abort()` / `ws->close()`, no protocol messages.
- Accepted: wrap in transport (frame cap 262144), build `ApiSessionDeps` (capabilities lambda: `supported_topics` = the topics whose refs are non-null; `phone` caps all-false in v1; snapshotFor lambda: map Topic → matching publisher's `snapshotBytes()`, empty QByteArray for null providers; requests = the handler; serverName from config `identity.head_unit_name` fallback "OpenAuto Prodigy"; appVersion = `identity.sw_version` + " (" OAP_GIT_HASH ")"), create session, connect `becameReady`, track in `sessions_`.
- Publisher fan-out: create the five publishers (skip null providers); connect each `statusReady(topic, bytes)` to a lambda: `for (auto* s : sessions_) if (s->state()==Ready && s->subscribedTo(topic)) s->deliver(bytes);`.
- `terminated()` → remove from `sessions_`, `deleteLater`.
- ctor registers action: `refs_.actions->registerAction("api.pairing.start", [this](const QVariant&){ startPairing(); });` (and `api.pairing.cancel`). `startPairing()`: `pairing_->startWindow(pairingTimeoutS_)`; relay `PairingManager::windowChanged` → `pairingChanged`.
- Store path: `QDir::homePath() + "/.openauto/api_clients.yaml"`; make it a ctor default arg (`ApiServer(refs, parent)` + `setStorePathForTest(path)`) so tests don't touch the real home file.

- [ ] **Step 1: Write the failing test** — `tests/test_api_server.cpp`. Use a real `YamlConfig`+`ConfigService` with `setValue("api.tcp_port", 0)`, `setValue("api.ws_port", 0)` (ephemeral). Build minimal refs: `MediaStatusService`, `ActionRegistry`, `NotificationService`, `ThemeService`, `PhoneStateService`, `ConfigService`; navigation/projection/bluetooth null. Cases:
  - `testStartsAndBindsEphemeral`: `start()` true; `tcpPort() != 0`; `wsPort() != 0`.
  - `testTcpEndToEndHelloSubscribe`: raw `QTcpSocket` to `127.0.0.1:tcpPort()`; send framed ClientHello(major 1) → framed ServerHello back (parse: capabilities contain TOPIC_MEDIA but NOT TOPIC_NAVIGATION); send Subscribe(MEDIA) → SubscribeResponse + MediaStatus snapshot; then `media.updateBtMetadata(...)` → a delta arrives.
  - `testWsEndToEnd`: same happy path over `QWebSocket`.
  - `testDisabledDoesNotListen`: config `api.enabled=false` → `start()` false, `sessionCount()==0`.
  - `testPairingActionRegistered`: `actions.registeredActions().contains("api.pairing.start")`; dispatch it → `pairingActive()` true, `pairingPin()` six digits.
  Helper `sendFramed(QTcpSocket&, const pb::ApiMessage&)` / `readFramed` in-file. Offscreen env property. Ports: ephemeral only — no fixed 199xx needed here.
- [ ] **Step 2: Verify failure.**
- [ ] **Step 3: Implement `ApiServer.cpp`.**
- [ ] **Step 4: Verify pass** — `ctest -R test_api_server --output-on-failure`.
- [ ] **Step 5: Commit** — `git commit -am "feat(api): ApiServer composition, listeners, peer policy, pairing action"`

---

### Task 13: Config defaults + main.cpp wiring + `media.*` actions + `distanceMeters` promotion

**Files:**
- Modify: `src/core/YamlConfig.cpp` (initDefaults, after the `navbar` block ~line 170)
- Modify: `src/core/services/INavigationProvider.hpp` (add `distanceMeters`)
- Modify: `src/core/aa/NavigationDataBridge.hpp` (add `override` to its existing `distanceMeters()`)
- Modify: `src/core/api/ApiSerializers.cpp` (populate `distance_meters` — one line)
- Modify: `src/main.cpp` (instantiate + wire; register media actions)
- Test: extend `tests/test_config_service.cpp` (one case) and `tests/test_api_serializers.cpp` (distance case)

**Interfaces:**
- Consumes: `ApiServer`, `ApiServiceRefs` (Task 12).
- Produces: running API in the app; `INavigationProvider::distanceMeters()` pure virtual.

- [ ] **Step 1: YamlConfig defaults** — in `initDefaults()` add:

```cpp
// External API v1
root_["api"]["enabled"] = true;
root_["api"]["tcp_port"] = 9810;
root_["api"]["ws_port"] = 9811;
root_["api"]["expose_lan"] = false;
root_["api"]["max_queue_bytes"] = 1048576;
root_["api"]["pairing_timeout_s"] = 120;
root_["api"]["handshake_timeout_ms"] = 5000;
```

Add to `tests/test_config_service.cpp`: `QCOMPARE(svc.value("api.tcp_port").toInt(), 9810);` in `testReadTopLevelValues`.

- [ ] **Step 2: Promote `distanceMeters`** — in `INavigationProvider.hpp` add `Q_PROPERTY(int distanceMeters READ distanceMeters NOTIFY distanceChanged)` beside `formattedDistance` and `virtual int distanceMeters() const = 0;` in the pure-virtual block. In `NavigationDataBridge.hpp:36` append `override` to its `distanceMeters()`. Remove the now-duplicate `Q_PROPERTY(int distanceMeters ...)` at `NavigationDataBridge.hpp:21` (the base declares it). Serializer: `out.set_distance_meters(p.distanceMeters());`. Add serializer test case with `FakeNavProvider` distance 500.

- [ ] **Step 3: media.* actions in main.cpp** — directly after the `theme.toggle` registration (`main.cpp:776-779`), using the same lambda style as the surrounding registrations:

```cpp
actionRegistry->registerAction("media.playPause", [mediaStatusService](const QVariant&) {
    mediaStatusService->playPause();
});
actionRegistry->registerAction("media.next", [mediaStatusService](const QVariant&) {
    mediaStatusService->next();
});
actionRegistry->registerAction("media.previous", [mediaStatusService](const QVariant&) {
    mediaStatusService->previous();
});
```
NOTE: `mediaStatusService` is created at `main.cpp:400` — the action block at ~763 runs later in main(), so the pointer exists; verify capture compiles (it is a raw pointer local).

- [ ] **Step 4: Instantiate ApiServer in main.cpp** — after the QML context property block (~line 940, so every ref exists), add:

```cpp
// External API v1 — the single external integration surface (design doc
// docs/superpowers/specs/2026-07-06-external-api-v1-design.md)
oap::api::ApiServiceRefs apiRefs;
apiRefs.media = mediaStatusService;
apiRefs.navigation = navBridge;              // may be nullptr (no orchestrator)
apiRefs.projection = projectionProvider;     // may be nullptr
apiRefs.phone = phoneStateService;
apiRefs.theme = themeService;
apiRefs.notifications = notificationService;
apiRefs.actions = actionRegistry;
apiRefs.config = configService;
apiRefs.bluetooth = bluetoothManager;        // may be nullptr on non-BT builds
auto* apiServer = new oap::api::ApiServer(apiRefs, &app);
if (!apiServer->start())
    qWarning() << "[main] External API disabled or failed to start";
engine.rootContext()->setContextProperty("ApiService", apiServer);
QObject::connect(apiServer->inboundState(), &oap::api::ApiInboundState::proxyRouteChanged,
                 &app, [systemClient](bool active, const QString& host, quint16 port,
                                      const QString& password) {
    if (systemClient) systemClient->setProxyRoute(active, host, port, password);
});
```
Match the ACTUAL local variable names at the cited lines (scout inventory: services created `main.cpp:270-477`; QML props `889-940`) — e.g. the nav bridge local may be named differently; read the surrounding 20 lines first and adapt names, NOT structure. `setProxyRoute`'s real signature is in `SystemServiceClient.hpp:10-59` — check the password argument's exact type/position. The password comes in-band from `ConnectivityReport.socks5_password` (Codex fix — the legacy secret-derivation at `CompanionListenerService.cpp:496-500` dies with the single global companion secret; the rewritten companion sends its proxy password explicitly). If `systemClient` does not exist at this point in main.cpp, connect nothing and log — companion retirement (when this matters) is a later phase.

- [ ] **Step 5: Build + full test pass** — `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure` — Expected: all green including the two extended tests.
- [ ] **Step 6: Run the app locally (WSL)** — `./build/src/openauto-prodigy` (offscreen is fine: `QT_QPA_PLATFORM=offscreen`), then from another shell verify the listener: `python3 -c "import socket; s=socket.create_connection(('127.0.0.1',9810),2); print('tcp ok')"`. Expected: `tcp ok`, app log shows no API errors. Ctrl-C the app.
- [ ] **Step 7: Commit** — `git commit -am "feat(api): wire ApiServer into main, api.* config defaults, media actions"`

---

### Task 14: Settings QML — API pairing page

**Files:**
- Create: `qml/applications/settings/ApiSettings.qml`
- Modify: `qml/applications/settings/SettingsMenu.qml` (four insertion points — pattern-match the "companion" entries at lines 192, 244, 257, 268)
- Modify: `src/CMakeLists.txt` qt_add_qml_module QML_FILES list (find the block listing `applications/settings/CompanionSettings.qml` and add the new file beside it)

**Interfaces:**
- Consumes: `ApiService` context property (Task 13): `pairingActive`, `pairingPin`, `startPairing()`, `cancelPairing()`.

- [ ] **Step 1: Write `ApiSettings.qml`** — pattern-match `CompanionSettings.qml` (guard property + Flickable + SettingsRow/SectionHeader). Content:

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    readonly property bool hasService: typeof ApiService !== "undefined"

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: UiMetrics.settingsPageInset
        anchors.rightMargin: UiMetrics.settingsPageInset
        anchors.topMargin: UiMetrics.marginPage
        spacing: 0

        SettingsRow { rowIndex: 0
            SettingsToggle {
                label: "External API Enabled"
                configPath: "api.enabled"
                restartRequired: true
            }
        }
        SettingsRow { rowIndex: 1
            SettingsToggle {
                label: "Allow LAN Clients"
                configPath: "api.expose_lan"
                restartRequired: true
            }
        }

        SectionHeader { text: "Remote Client Pairing" }

        SettingsRow { rowIndex: 0
            RowLayout {
                anchors.fill: parent
                spacing: UiMetrics.gap

                Text {
                    text: {
                        if (!root.hasService) return "API service unavailable"
                        return ApiService.pairingActive
                               ? "PIN: " + ApiService.pairingPin
                               : "No pairing in progress"
                    }
                    font.pixelSize: UiMetrics.fontBody
                    color: root.hasService && ApiService.pairingActive
                           ? ThemeService.primary : ThemeService.onSurface
                    Layout.fillWidth: true
                }

                Rectangle {
                    width: pairLabel.implicitWidth + UiMetrics.gap * 2
                    height: UiMetrics.touchMin
                    radius: height / 2
                    color: pairArea.pressed
                           ? Qt.darker(ThemeService.surfaceContainerLow, 1.3)
                           : ThemeService.surfaceContainerLow
                    border.color: ThemeService.onSurfaceVariant
                    border.width: 1
                    opacity: root.hasService ? 1.0 : 0.4

                    Text {
                        id: pairLabel
                        anchors.centerIn: parent
                        text: root.hasService && ApiService.pairingActive
                              ? "Cancel Pairing" : "Start Pairing"
                        font.pixelSize: UiMetrics.fontSmall
                        color: ThemeService.onSurface
                    }
                    SettingsHoldArea {
                        id: pairArea
                        anchors.fill: parent
                        enabled: root.hasService
                        onShortClicked: ApiService.pairingActive
                                        ? ApiService.cancelPairing()
                                        : ApiService.startPairing()
                    }
                }
            }
        }
    }
}
```
Check `CompanionSettings.qml`'s actual `SettingsHoldArea` signal name (`onShortClicked` assumed from its pairing button at line ~74) and `SettingsToggle`'s exact property names before copying — mirror whatever the companion page uses.

- [ ] **Step 2: Register in `SettingsMenu.qml`** — add beside each "companion" occurrence: `ListElement { name: "External API"; icon: ""; pageId: "api" }` (any sensible icon glyph from the set used nearby), `Component { id: apiPage; ApiSettings {} }`, `"api": "External API",` and `"api": apiPage,` in the two maps.
- [ ] **Step 3: Verify** — build + run app (offscreen won't show UI; on a desktop WSLg session just launch and click Settings → External API; minimum bar: `qml6 -I qml qml/applications/settings/ApiSettings.qml` parses without errors, or app starts with no QML warnings mentioning ApiSettings).
- [ ] **Step 4: Commit** — `git add qml src/CMakeLists.txt && git commit -m "feat(api): settings page for API enable/LAN/pairing"`

---

### Task 15: Loopback integration suite

**Files:**
- Test: `tests/test_api_loopback.cpp` (register with offscreen env)

**Interfaces:** consumes the full stack via `ApiServer` on ephemeral ports (Task 12's test helpers — copy `sendFramed`/`readFramed` in-file).

This is the design doc §15 mandatory list, end to end over real sockets. One fixture builds: YamlConfig(+ConfigService, ephemeral ports), MediaStatusService, PhoneStateService, ThemeService, ActionRegistry, NotificationService, ApiServer with test store path (`/tmp/oap_test_loopback_clients.yaml`, removed in init()).

- [ ] **Step 1: Write the six mandatory cases + three extras**
  1. `testHandshakeTcpAndWs` — ClientHello→ServerHello on both transports.
  2. `testAuthRejectPaths` — server-side seam: get the session via… sessions are internal; instead run the remote-path unit coverage in Task 10 (already done) and here verify the SERVER's localhost-trust: hello without auth succeeds from 127.0.0.1. (Record in the test comment: remote-path integration is covered by session unit tests; loopback cannot present a remote peer.)
  3. `testSnapshotOnSubscribe` — set BT metadata BEFORE subscribing; subscribe MEDIA → snapshot carries the pre-set title; subscribe NAVIGATION (null provider) → rejected in SubscribeResponse.
  4. `testDeltaDelivery` — two clients subscribed; mutate media; both get the delta; a third connected-but-unsubscribed client gets nothing (assert on a 200 ms quiet read).
  5. `testSlowConsumerDisconnect` — client A subscribes then STOPS reading (do not call read; keep socket open); flood: `for (int i=0;i<20000;++i) media.updateBtMetadata(QString::number(i),"a","b")` with `QCoreApplication::processEvents()` every 100 iterations; expect A's socket to hit disconnected/readChannelFinished within 10 s while client B (draining) keeps receiving. This validates the `bytesToWrite` cap end-to-end.
  6. `testClientActionLifecycle` — register `"looptest.act"` over the socket; `ActionRegistry::dispatch("looptest.act", 5)` server-side → ActionInvokedEvent arrives with payload_json "5"; drop the socket; `QTRY_VERIFY(!registry.registeredActions().contains("looptest.act"))`; dispatch again → returns false.
  7. `testNotificationOwnershipE2E` — A posts, B dismiss → Error NOT_FOUND; A dismiss → Ack.
  8. `testPhoneUnavailableE2E` — DialRequest AND AnswerCallRequest → PhoneCommandResponse UNAVAILABLE (capabilities all-false in v1; contract and behavior must agree).
  9. `testInboundReportsE2E` — GpsReport + TimeReport over the wire → ApiInboundState props updated, timeReported spy fired, no response frames arrive (200 ms quiet read).
- [ ] **Step 2: Run** — `ctest -R test_api_loopback --output-on-failure` — Expected: 9 PASS. If testSlowConsumerDisconnect is flaky on queue timing, raise the flood count — do NOT raise the byte cap.
- [ ] **Step 3: Full suite** — `ctest --output-on-failure` — everything green.
- [ ] **Step 4: Commit** — `git commit -am "test(api): loopback integration suite covering design §15 mandatory cases"`

---

### Task 16: Cross-build, docs, handoff

**Files:**
- Modify: `docs/development.md` (architecture note), `CLAUDE.md` (Key Files table + repository layout), `docs/session-handoffs.md` (entry), `docs/INDEX.md` (if it indexes specs/plans)

- [ ] **Step 1: Cross-build** — `./cross-build.sh` — Expected: clean aarch64 build. Fix only build breaks (usually a missing arm64 package — see Task 2 Step 4).
- [ ] **Step 2: CLAUDE.md** — Repository Layout: add `proto/api/` line ("External API v1 protobuf contract (prodigy-private; additive-only)"); Key Files: add `src/core/api/ApiServer.cpp` ("External API v1 server — sessions, pairing, publishers"). Do not restate design; one line each.
- [ ] **Step 3: development.md** — under the architecture/services docs section add: External API v1 — design `docs/superpowers/specs/2026-07-06-external-api-v1-design.md`, ports 9810/9811, config `api.*`.
- [ ] **Step 4: session-handoffs.md** — entry: what shipped, test counts, any deviations from the design doc (there must be none without a recorded reason).
- [ ] **Step 5: Full verification** — `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure && cd .. && ./cross-build.sh` — all green.
- [ ] **Step 6: Commit** — `git commit -am "docs: External API v1 implementation handoff"`

---

## Self-review notes (author, 2026-07-06)

- Spec coverage: design §3.1/3.2 (Tasks 1–2, 8, 12), §4 (Task 10), §5 (Tasks 4–5, 12, 14), §6 (Tasks 9, 10, 15), §7 (Tasks 10, 12), §8 tables (Tasks 6–7 — values verified against `BtAudioPlugin.hpp:50-53`, `MediaStatusChannelHandler.hpp:20-23`, oaa `ManeuverTypeEnum.proto`), §9 (Task 11), §10 (Task 11), §12 (Tasks 12–13), §15 (Tasks 3–12 unit + Task 15 loopback), §2 fence deltas (Task 13 media actions; Task 11 TimeReport ingest).
- Known deliberate gap: remote-peer auth cannot be integration-tested over loopback; covered at session level with the trust override seam (Task 10) — recorded inside Task 15 case 2.
- Codex review 2026-07-06 fixes folded in: notification `priority` is proto3 `optional` (explicit 0 honored, absent → 50); ALL phone commands return UNAVAILABLE in v1 so capability flags and results never contradict (the earlier answer/hangup special case is DELETED — do not resurrect it); `ConnectivityReport.socks5_password` flows in-band through `proxyRouteChanged` to `setProxyRoute`.
- Type consistency check: `IApiTransport` signatures match between Tasks 8/10/11; `ApiSessionDeps.snapshotFor` empty-QByteArray convention consistent in Tasks 10/12; `pb` alias used throughout.
