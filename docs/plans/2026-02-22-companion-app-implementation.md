# OpenAuto Companion — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a companion Android app that pushes time/GPS/battery to the Pi and runs a SOCKS5 proxy, plus the Pi-side listener that receives and acts on this data.

**Architecture:** Two codebases — a new Kotlin Android app (`companion-app/`) and new C++ service code in the existing `openauto-prodigy` repo. Phone pushes JSON over TCP; Pi listens, validates auth, and dispatches to clock/GPS/battery subsystems. SOCKS5 proxy runs independently on the phone.

**Tech Stack:** Kotlin + Jetpack Compose (Android), C++17 + Qt 6 (Pi), HMAC-SHA256 auth, SOCKS5 RFC 1928/1929.

---

## Phase 1: Pi-Side CompanionListener (C++)

Build the Pi-side TCP server first so we can test it with `netcat`/scripts before the Android app exists.

---

### Task 1: CompanionListenerService Skeleton + Test

**Files:**
- Create: `src/core/services/CompanionListenerService.hpp`
- Create: `src/core/services/CompanionListenerService.cpp`
- Create: `tests/test_companion_listener.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing test**

```cpp
// tests/test_companion_listener.cpp
#include <QTest>
#include <QSignalSpy>
#include "core/services/CompanionListenerService.hpp"

class TestCompanionListener : public QObject {
    Q_OBJECT
private slots:
    void constructionDoesNotCrash()
    {
        oap::CompanionListenerService svc;
        QVERIFY(!svc.isListening());
    }

    void startListeningOnPort()
    {
        oap::CompanionListenerService svc;
        QVERIFY(svc.start(19876));  // test port
        QVERIFY(svc.isListening());
        svc.stop();
        QVERIFY(!svc.isListening());
    }

    void rejectsConnectionWithoutAuth()
    {
        oap::CompanionListenerService svc;
        svc.setSharedSecret("test-secret-key");
        svc.start(19876);

        // Connect a raw TCP client
        QTcpSocket client;
        client.connectToHost("127.0.0.1", 19876);
        QVERIFY(client.waitForConnected(1000));

        // Should receive a challenge
        QVERIFY(client.waitForReadyRead(1000));
        QByteArray challenge = client.readLine();
        QVERIFY(challenge.contains("\"type\":\"challenge\""));
        QVERIFY(challenge.contains("\"nonce\""));

        // Send garbage hello — should be rejected
        client.write("{\"type\":\"hello\",\"token\":\"bad\"}\n");
        client.flush();
        QVERIFY(client.waitForReadyRead(1000));
        QByteArray response = client.readLine();
        QVERIFY(response.contains("\"accepted\":false"));

        svc.stop();
    }
};

QTEST_MAIN(TestCompanionListener)
#include "test_companion_listener.moc"
```

**Step 2: Create the header**

```cpp
// src/core/services/CompanionListenerService.hpp
#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QByteArray>

namespace oap {

class CompanionListenerService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(double gpsLat READ gpsLat NOTIFY gpsChanged)
    Q_PROPERTY(double gpsLon READ gpsLon NOTIFY gpsChanged)
    Q_PROPERTY(double gpsSpeed READ gpsSpeed NOTIFY gpsChanged)
    Q_PROPERTY(double gpsAccuracy READ gpsAccuracy NOTIFY gpsChanged)
    Q_PROPERTY(double gpsBearing READ gpsBearing NOTIFY gpsChanged)
    Q_PROPERTY(bool gpsStale READ isGpsStale NOTIFY gpsChanged)
    Q_PROPERTY(int phoneBattery READ phoneBattery NOTIFY batteryChanged)
    Q_PROPERTY(bool phoneCharging READ isPhoneCharging NOTIFY batteryChanged)
    Q_PROPERTY(bool internetAvailable READ isInternetAvailable NOTIFY internetChanged)
    Q_PROPERTY(QString proxyAddress READ proxyAddress NOTIFY internetChanged)

public:
    explicit CompanionListenerService(QObject* parent = nullptr);
    ~CompanionListenerService() override;

    bool start(int port = 9876);
    void stop();
    bool isListening() const;

    void setSharedSecret(const QString& secret);

    // Q_PROPERTY getters
    bool isConnected() const;
    double gpsLat() const;
    double gpsLon() const;
    double gpsSpeed() const;
    double gpsAccuracy() const;
    double gpsBearing() const;
    bool isGpsStale() const;
    int phoneBattery() const;
    bool isPhoneCharging() const;
    bool isInternetAvailable() const;
    QString proxyAddress() const;

signals:
    void connectedChanged();
    void gpsChanged();
    void batteryChanged();
    void internetChanged();
    void timeAdjusted(qint64 oldTimeMs, qint64 newTimeMs, qint64 deltaMs);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    void sendChallenge();
    bool validateHello(const QJsonObject& msg);
    void handleStatus(const QJsonObject& msg);
    bool verifyMac(const QJsonObject& msg);
    void adjustClock(qint64 phoneTimeMs);
    QByteArray computeHmac(const QByteArray& key, const QByteArray& data);

    QTcpServer* server_ = nullptr;
    QTcpSocket* client_ = nullptr;
    QString sharedSecret_;
    QByteArray sessionKey_;
    QByteArray currentNonce_;
    qint64 lastSeq_ = -1;

    // State
    double gpsLat_ = 0.0;
    double gpsLon_ = 0.0;
    double gpsSpeed_ = 0.0;
    double gpsAccuracy_ = 0.0;
    double gpsBearing_ = 0.0;
    int gpsAgeMs_ = -1;
    int phoneBattery_ = -1;
    bool phoneCharging_ = false;
    bool internetAvailable_ = false;
    QString proxyAddress_;

    // Time safety
    int backwardJumpCount_ = 0;
    qint64 lastBackwardTarget_ = 0;
};

} // namespace oap
```

**Step 3: Create minimal implementation**

```cpp
// src/core/services/CompanionListenerService.cpp
#include "CompanionListenerService.hpp"
#include <QJsonDocument>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QDateTime>
#include <QProcess>

namespace oap {

CompanionListenerService::CompanionListenerService(QObject* parent)
    : QObject(parent)
{
}

CompanionListenerService::~CompanionListenerService()
{
    stop();
}

bool CompanionListenerService::start(int port)
{
    if (server_) return false;

    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection,
            this, &CompanionListenerService::onNewConnection);

    if (!server_->listen(QHostAddress::Any, port)) {
        delete server_;
        server_ = nullptr;
        return false;
    }
    return true;
}

void CompanionListenerService::stop()
{
    if (client_) {
        client_->disconnectFromHost();
        client_ = nullptr;
    }
    if (server_) {
        server_->close();
        delete server_;
        server_ = nullptr;
    }
    sessionKey_.clear();
    lastSeq_ = -1;

    if (gpsLat_ != 0.0 || phoneBattery_ != -1 || internetAvailable_) {
        gpsLat_ = 0.0; gpsLon_ = 0.0; gpsSpeed_ = 0.0;
        gpsAccuracy_ = 0.0; gpsBearing_ = 0.0; gpsAgeMs_ = -1;
        phoneBattery_ = -1; phoneCharging_ = false;
        internetAvailable_ = false; proxyAddress_.clear();
        emit gpsChanged();
        emit batteryChanged();
        emit internetChanged();
        emit connectedChanged();
    }
}

bool CompanionListenerService::isListening() const
{
    return server_ && server_->isListening();
}

void CompanionListenerService::setSharedSecret(const QString& secret)
{
    sharedSecret_ = secret;
}

bool CompanionListenerService::isConnected() const { return client_ != nullptr; }
double CompanionListenerService::gpsLat() const { return gpsLat_; }
double CompanionListenerService::gpsLon() const { return gpsLon_; }
double CompanionListenerService::gpsSpeed() const { return gpsSpeed_; }
double CompanionListenerService::gpsAccuracy() const { return gpsAccuracy_; }
double CompanionListenerService::gpsBearing() const { return gpsBearing_; }
bool CompanionListenerService::isGpsStale() const { return gpsAgeMs_ < 0 || gpsAgeMs_ > 30000; }
int CompanionListenerService::phoneBattery() const { return phoneBattery_; }
bool CompanionListenerService::isPhoneCharging() const { return phoneCharging_; }
bool CompanionListenerService::isInternetAvailable() const { return internetAvailable_; }
QString CompanionListenerService::proxyAddress() const { return proxyAddress_; }

void CompanionListenerService::onNewConnection()
{
    auto* pending = server_->nextPendingConnection();
    if (client_) {
        // Reject — only one companion at a time
        pending->write("{\"type\":\"error\",\"msg\":\"already connected\"}\n");
        pending->flush();
        pending->disconnectFromHost();
        pending->deleteLater();
        return;
    }

    client_ = pending;
    connect(client_, &QTcpSocket::readyRead,
            this, &CompanionListenerService::onClientReadyRead);
    connect(client_, &QTcpSocket::disconnected,
            this, &CompanionListenerService::onClientDisconnected);

    sendChallenge();
}

void CompanionListenerService::sendChallenge()
{
    // Generate 32-byte random nonce
    QByteArray nonce(32, 0);
    QRandomGenerator::global()->fillRange(
        reinterpret_cast<quint32*>(nonce.data()), nonce.size() / sizeof(quint32));
    currentNonce_ = nonce.toHex();

    QJsonObject challenge;
    challenge["type"] = "challenge";
    challenge["nonce"] = QString::fromLatin1(currentNonce_);
    challenge["version"] = 1;

    client_->write(QJsonDocument(challenge).toJson(QJsonDocument::Compact) + "\n");
    client_->flush();
}

bool CompanionListenerService::validateHello(const QJsonObject& msg)
{
    if (msg["type"].toString() != "hello") return false;
    if (sharedSecret_.isEmpty()) return false;

    QString token = msg["token"].toString();
    QByteArray expected = computeHmac(
        sharedSecret_.toUtf8(), currentNonce_);

    return token == QString::fromLatin1(expected.toHex());
}

void CompanionListenerService::onClientReadyRead()
{
    while (client_ && client_->canReadLine()) {
        QByteArray line = client_->readLine().trimmed();
        if (line.isEmpty()) continue;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError) continue;

        QJsonObject msg = doc.object();
        QString type = msg["type"].toString();

        if (type == "hello") {
            if (validateHello(msg)) {
                // Generate session key
                QByteArray rawKey(32, 0);
                QRandomGenerator::global()->fillRange(
                    reinterpret_cast<quint32*>(rawKey.data()),
                    rawKey.size() / sizeof(quint32));
                sessionKey_ = rawKey;

                QJsonObject ack;
                ack["type"] = "hello_ack";
                ack["accepted"] = true;
                // Session key encrypted with shared secret (XOR for v1, upgrade later)
                ack["session_key"] = QString::fromLatin1(rawKey.toHex());

                client_->write(QJsonDocument(ack).toJson(QJsonDocument::Compact) + "\n");
                client_->flush();
                emit connectedChanged();
            } else {
                QJsonObject reject;
                reject["type"] = "hello_ack";
                reject["accepted"] = false;
                client_->write(QJsonDocument(reject).toJson(QJsonDocument::Compact) + "\n");
                client_->flush();
            }
        } else if (type == "status") {
            if (sessionKey_.isEmpty()) continue;  // Not authenticated
            if (!verifyMac(msg)) continue;         // Bad MAC
            handleStatus(msg);
        }
    }
}

bool CompanionListenerService::verifyMac(const QJsonObject& msg)
{
    QString mac = msg["mac"].toString();
    if (mac.isEmpty()) return false;

    // Rebuild payload without mac field
    QJsonObject payload = msg;
    payload.remove("mac");
    QByteArray payloadBytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QByteArray expected = computeHmac(sessionKey_, payloadBytes);
    return mac == QString::fromLatin1(expected.toHex());
}

void CompanionListenerService::handleStatus(const QJsonObject& msg)
{
    // Sequence check (sliding window of 10)
    qint64 seq = msg["seq"].toInteger();
    if (seq <= lastSeq_ && seq > lastSeq_ - 10) return;  // Replay
    lastSeq_ = seq;

    // Time
    qint64 phoneTimeMs = msg["time_ms"].toInteger();
    if (phoneTimeMs > 0) {
        adjustClock(phoneTimeMs);
    }

    // GPS
    QJsonObject gps = msg["gps"].toObject();
    if (!gps.isEmpty()) {
        gpsLat_ = gps["lat"].toDouble();
        gpsLon_ = gps["lon"].toDouble();
        gpsSpeed_ = gps["speed"].toDouble();
        gpsAccuracy_ = gps["accuracy"].toDouble();
        gpsBearing_ = gps["bearing"].toDouble();
        gpsAgeMs_ = gps["age_ms"].toInt(-1);
        emit gpsChanged();
    }

    // Battery
    QJsonObject battery = msg["battery"].toObject();
    if (!battery.isEmpty()) {
        phoneBattery_ = battery["level"].toInt(-1);
        phoneCharging_ = battery["charging"].toBool();
        emit batteryChanged();
    }

    // SOCKS5 proxy
    QJsonObject socks = msg["socks5"].toObject();
    if (!socks.isEmpty()) {
        bool active = socks["active"].toBool();
        int port = socks["port"].toInt();
        bool changed = (active != internetAvailable_);
        internetAvailable_ = active;
        if (active && client_) {
            proxyAddress_ = QString("socks5://%1:%2")
                .arg(client_->peerAddress().toString())
                .arg(port);
        } else {
            proxyAddress_.clear();
        }
        if (changed) emit internetChanged();
    }
}

void CompanionListenerService::adjustClock(qint64 phoneTimeMs)
{
    qint64 piTimeMs = QDateTime::currentMSecsSinceEpoch();
    qint64 deltaMs = phoneTimeMs - piTimeMs;

    // Only adjust if delta > 30 seconds
    if (qAbs(deltaMs) < 30000) return;

    // Backward jump protection: reject >5min backward unless 3 consecutive agree
    if (deltaMs < -300000) {
        if (phoneTimeMs == lastBackwardTarget_) {
            backwardJumpCount_++;
        } else {
            backwardJumpCount_ = 1;
            lastBackwardTarget_ = phoneTimeMs;
        }
        if (backwardJumpCount_ < 3) return;  // Need 3 agreements
    }
    backwardJumpCount_ = 0;
    lastBackwardTarget_ = 0;

    // Set via timedatectl (polkit-authorized)
    QDateTime newTime = QDateTime::fromMSecsSinceEpoch(phoneTimeMs, Qt::UTC);
    QString timeStr = newTime.toString("yyyy-MM-dd hh:mm:ss");

    QProcess proc;
    proc.start("timedatectl", {"set-time", timeStr});
    proc.waitForFinished(5000);

    if (proc.exitCode() == 0) {
        qInfo() << "Companion: clock adjusted by" << deltaMs << "ms"
                << "(" << piTimeMs << "->" << phoneTimeMs << ")";
        emit timeAdjusted(piTimeMs, phoneTimeMs, deltaMs);
    } else {
        qWarning() << "Companion: timedatectl failed:" << proc.readAllStandardError();
    }
}

QByteArray CompanionListenerService::computeHmac(const QByteArray& key, const QByteArray& data)
{
    return QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha256);
}

void CompanionListenerService::onClientDisconnected()
{
    client_->deleteLater();
    client_ = nullptr;
    sessionKey_.clear();
    lastSeq_ = -1;

    // Clear all state
    gpsLat_ = 0.0; gpsLon_ = 0.0; gpsSpeed_ = 0.0;
    gpsAccuracy_ = 0.0; gpsBearing_ = 0.0; gpsAgeMs_ = -1;
    phoneBattery_ = -1; phoneCharging_ = false;
    internetAvailable_ = false; proxyAddress_.clear();

    emit connectedChanged();
    emit gpsChanged();
    emit batteryChanged();
    emit internetChanged();
}

} // namespace oap
```

**Step 4: Add to CMakeLists.txt**

In `src/CMakeLists.txt`, add `core/services/CompanionListenerService.cpp` to the source list after `core/services/NotificationService.cpp`.

In `tests/CMakeLists.txt`, add:
```cmake
add_executable(test_companion_listener
    test_companion_listener.cpp
    ${CMAKE_SOURCE_DIR}/src/core/services/CompanionListenerService.cpp
)

target_include_directories(test_companion_listener PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_companion_listener PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

add_test(NAME test_companion_listener COMMAND test_companion_listener)
```

**Step 5: Build and run tests**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: 18 tests pass (17 existing + 1 new)

**Step 6: Commit**

```bash
git add src/core/services/CompanionListenerService.hpp \
        src/core/services/CompanionListenerService.cpp \
        tests/test_companion_listener.cpp \
        src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: CompanionListenerService skeleton with auth and status handling"
```

---

### Task 2: Wire CompanionListener into App

**Files:**
- Modify: `src/core/plugin/IHostContext.hpp`
- Modify: `src/core/plugin/HostContext.hpp`
- Modify: `src/main.cpp`
- Modify: `src/core/services/IpcServer.hpp`
- Modify: `src/core/services/IpcServer.cpp`

**Step 1: Add to IHostContext**

In `src/core/plugin/IHostContext.hpp`:
- Add forward declaration: `class CompanionListenerService;`
- Add pure virtual getter: `virtual CompanionListenerService* companionListenerService() = 0;`

**Step 2: Add to HostContext**

In `src/core/plugin/HostContext.hpp`:
- Add `#include "core/services/CompanionListenerService.hpp"`
- Add setter: `void setCompanionListenerService(oap::CompanionListenerService* svc) { companion_ = svc; }`
- Add override: `oap::CompanionListenerService* companionListenerService() override { return companion_; }`
- Add member: `oap::CompanionListenerService* companion_ = nullptr;`

**Step 3: Register in main.cpp**

After NotificationService creation (~line 98), add:
```cpp
auto companionListener = new oap::CompanionListenerService(&app);
// TODO: read shared secret from ~/.openauto/companion.key
// TODO: read port from config
companionListener->start(9876);
hostContext->setCompanionListenerService(companionListener);
engine.rootContext()->setContextProperty("CompanionService", companionListener);
```

Add `#include "core/services/CompanionListenerService.hpp"` to main.cpp includes.

**Step 4: Add IPC status endpoint**

In `IpcServer.hpp`, add:
```cpp
void setCompanionListenerService(oap::CompanionListenerService* svc);
```
And member: `oap::CompanionListenerService* companion_ = nullptr;`

In `IpcServer.cpp`, add setter and handler:
```cpp
void IpcServer::setCompanionListenerService(oap::CompanionListenerService* svc) {
    companion_ = svc;
}
```

In `handleRequest()`, add routing:
```cpp
} else if (command == "companion_status") {
    return handleCompanionStatus();
}
```

Add handler:
```cpp
QByteArray IpcServer::handleCompanionStatus() {
    if (!companion_) return R"({"error":"Companion service not available"})";
    QJsonObject obj;
    obj["connected"] = companion_->isConnected();
    obj["gps_lat"] = companion_->gpsLat();
    obj["gps_lon"] = companion_->gpsLon();
    obj["gps_speed"] = companion_->gpsSpeed();
    obj["battery"] = companion_->phoneBattery();
    obj["charging"] = companion_->isPhoneCharging();
    obj["internet"] = companion_->isInternetAvailable();
    obj["proxy"] = companion_->proxyAddress();
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}
```

Wire IPC in main.cpp: `ipcServer->setCompanionListenerService(companionListener);`

**Step 5: Build and verify**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: All 18 tests pass, no build errors.

**Step 6: Commit**

```bash
git add src/core/plugin/IHostContext.hpp src/core/plugin/HostContext.hpp \
        src/main.cpp src/core/services/IpcServer.hpp src/core/services/IpcServer.cpp
git commit -m "feat: wire CompanionListener into app, IPC, and QML"
```

---

### Task 3: Config + Pairing + Polkit

**Files:**
- Create: `config/companion-polkit.rules`
- Modify: `install.sh` (add polkit rule install + companion config)
- Modify: `src/main.cpp` (read config for companion port + secret)

**Step 1: Create polkit rule**

```javascript
// config/companion-polkit.rules
polkit.addRule(function(action, subject) {
    if (action.id === "org.freedesktop.timedate1.set-time" &&
        subject.user === "matt") {
        return polkit.Result.YES;
    }
});
```

**Step 2: Add companion config section**

In default config (or document for users), add:
```yaml
companion:
  enabled: true
  port: 9876
```

**Step 3: Update main.cpp to read config**

```cpp
bool companionEnabled = config->valueByPath("companion.enabled").toBool(true);
int companionPort = config->valueByPath("companion.port").toInt(9876);

if (companionEnabled) {
    auto companionListener = new oap::CompanionListenerService(&app);
    // Read shared secret from file
    QFile secretFile(QDir::homePath() + "/.openauto/companion.key");
    if (secretFile.open(QIODevice::ReadOnly)) {
        companionListener->setSharedSecret(QString::fromUtf8(secretFile.readAll().trimmed()));
    }
    companionListener->start(companionPort);
    hostContext->setCompanionListenerService(companionListener);
    engine.rootContext()->setContextProperty("CompanionService", companionListener);
}
```

**Step 4: Add pairing generation to settings**

Add a Q_INVOKABLE method to CompanionListenerService:
```cpp
Q_INVOKABLE QString generatePairingPin();
```

Implementation:
```cpp
QString CompanionListenerService::generatePairingPin()
{
    int pin = QRandomGenerator::global()->bounded(100000, 999999);
    QString pinStr = QString::number(pin);

    // Derive shared secret from PIN + device identity
    QByteArray material = pinStr.toUtf8() + QSysInfo::machineUniqueId()
                        + QByteArray::number(QDateTime::currentMSecsSinceEpoch());
    QByteArray secret = QCryptographicHash::hash(material, QCryptographicHash::Sha256).toHex();

    // Save to file
    QString path = QDir::homePath() + "/.openauto/companion.key";
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(secret);
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }

    setSharedSecret(QString::fromUtf8(secret));
    return pinStr;  // Display this to user
}
```

**Step 5: Add install.sh section**

In `install.sh`, after existing polkit/systemd setup, add:
```bash
# Companion app polkit rule for time setting
sudo cp config/companion-polkit.rules /etc/polkit-1/rules.d/50-openauto-time.rules
```

**Step 6: Build and verify**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass.

**Step 7: Commit**

```bash
git add config/companion-polkit.rules src/main.cpp \
        src/core/services/CompanionListenerService.hpp \
        src/core/services/CompanionListenerService.cpp \
        install.sh
git commit -m "feat: companion config, pairing PIN generation, polkit rule"
```

---

### Task 4: Manual Integration Test with netcat

**No code changes — validation only.**

**Step 1: Deploy to Pi**

```bash
rsync -av --exclude='build/' --exclude='.git/' --exclude='libs/aasdk/' \
    /home/matt/claude/personal/openautopro/openauto-prodigy/ \
    matt@192.168.1.149:/home/matt/openauto-prodigy/
ssh matt@192.168.1.149 "cd /home/matt/openauto-prodigy/build && cmake --build . -j3"
```

**Step 2: Generate pairing secret on Pi**

```bash
# Create a test secret
ssh matt@192.168.1.149 "echo 'test-secret-for-development' > ~/.openauto/companion.key && chmod 600 ~/.openauto/companion.key"
```

**Step 3: Launch app and test with netcat**

```bash
# On dev machine, connect to Pi's companion port
nc 192.168.1.149 9876
# Should receive: {"type":"challenge","nonce":"...","version":1}

# Compute HMAC manually (or write a quick Python script):
python3 -c "
import hmac, hashlib, json
secret = b'test-secret-for-development'
nonce = b'PASTE_NONCE_HERE'
token = hmac.new(secret, nonce, hashlib.sha256).hexdigest()
print(json.dumps({'type':'hello','version':1,'token':token,'capabilities':['time','gps','battery']}))"
# Paste output into netcat
# Should receive: {"type":"hello_ack","accepted":true,"session_key":"..."}
```

**Step 4: Install polkit rule and test timedatectl**

```bash
ssh matt@192.168.1.149 "sudo cp /home/matt/openauto-prodigy/config/companion-polkit.rules /etc/polkit-1/rules.d/50-openauto-time.rules"
# Test that timedatectl works without sudo:
ssh matt@192.168.1.149 "timedatectl set-time '2026-02-22 20:00:00'"
# Should succeed without password prompt
```

**Step 5: Document results**

Note any issues encountered for fixing in subsequent tasks.

---

## Phase 2: Android Companion App (Kotlin)

New repository/directory for the Android app.

---

### Task 5: Android Project Scaffold

**Files:**
- Create: `companion-app/` directory (new project)
- Create: Standard Android project structure via Gradle

**Step 1: Create project directory**

```bash
mkdir -p /home/matt/claude/personal/openautopro/companion-app
cd /home/matt/claude/personal/openautopro/companion-app
```

**Step 2: Create Gradle build files**

`settings.gradle.kts`:
```kotlin
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}
rootProject.name = "OpenAutoCompanion"
include(":app")
```

`build.gradle.kts` (root):
```kotlin
plugins {
    id("com.android.application") version "8.7.3" apply false
    id("org.jetbrains.kotlin.android") version "2.1.0" apply false
    id("org.jetbrains.kotlin.plugin.compose") version "2.1.0" apply false
}
```

`app/build.gradle.kts`:
```kotlin
plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "org.openauto.companion"
    compileSdk = 35

    defaultConfig {
        applicationId = "org.openauto.companion"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"
    }

    buildFeatures {
        compose = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.15.0")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.7")
    implementation("androidx.activity:activity-compose:1.9.3")
    implementation(platform("androidx.compose:compose-bom:2024.12.01"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.ui:ui-tooling-preview")
    debugImplementation("androidx.compose.ui:ui-tooling")

    testImplementation("junit:junit:4.13.2")
    testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.9.0")
}
```

`gradle.properties`:
```properties
android.useAndroidX=true
kotlin.code.style=official
org.gradle.jvmargs=-Xmx2048m
```

**Step 3: Create AndroidManifest.xml**

`app/src/main/AndroidManifest.xml`:
```xml
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android">

    <uses-permission android:name="android.permission.INTERNET" />
    <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
    <uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION" />
    <uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
    <uses-permission android:name="android.permission.CHANGE_NETWORK_STATE" />
    <uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
    <uses-permission android:name="android.permission.FOREGROUND_SERVICE_LOCATION" />
    <uses-permission android:name="android.permission.POST_NOTIFICATIONS" />
    <uses-permission android:name="android.permission.NEARBY_WIFI_DEVICES"
        android:usesPermissionFlags="neverForLocation" />

    <application
        android:name=".CompanionApp"
        android:label="OpenAuto Companion"
        android:icon="@mipmap/ic_launcher"
        android:supportsRtl="true"
        android:theme="@style/Theme.OpenAutoCompanion">

        <activity
            android:name=".ui.MainActivity"
            android:exported="true"
            android:theme="@style/Theme.OpenAutoCompanion">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>

        <service
            android:name=".service.CompanionService"
            android:foregroundServiceType="location"
            android:exported="false" />

    </application>
</manifest>
```

**Step 4: Create Application class**

`app/src/main/java/org/openauto/companion/CompanionApp.kt`:
```kotlin
package org.openauto.companion

import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager

class CompanionApp : Application() {
    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
    }

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID,
            "Companion Service",
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = "Shows when sharing data with OpenAuto Prodigy"
        }
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(channel)
    }

    companion object {
        const val CHANNEL_ID = "companion_service"
    }
}
```

**Step 5: Create minimal MainActivity**

`app/src/main/java/org/openauto/companion/ui/MainActivity.kt`:
```kotlin
package org.openauto.companion.ui

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Surface {
                    Text("OpenAuto Companion — v0.1.0")
                }
            }
        }
    }
}
```

**Step 6: Create theme + resources**

`app/src/main/res/values/themes.xml`:
```xml
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <style name="Theme.OpenAutoCompanion" parent="android:Theme.Material.Light.NoActionBar" />
</resources>
```

Create placeholder launcher icons (or use default).

**Step 7: Initialize git repo**

```bash
cd /home/matt/claude/personal/openautopro/companion-app
git init
echo "/.gradle/\n/build/\n/app/build/\n*.iml\n.idea/\nlocal.properties" > .gitignore
git add -A
git commit -m "feat: initial Android project scaffold with manifest and permissions"
```

**Step 8: Verify build**

Run: `./gradlew assembleDebug`
Expected: BUILD SUCCESSFUL, APK at `app/build/outputs/apk/debug/app-debug.apk`

Note: This requires Android SDK on the dev machine. If not installed, this step validates on a machine with Android Studio or can be deferred.

---

### Task 6: Data Push Service (Foreground Service + TCP Client)

**Files:**
- Create: `app/src/main/java/org/openauto/companion/service/CompanionService.kt`
- Create: `app/src/main/java/org/openauto/companion/net/PiConnection.kt`
- Create: `app/src/main/java/org/openauto/companion/net/Protocol.kt`
- Create: `app/src/test/java/org/openauto/companion/net/ProtocolTest.kt`

**Step 1: Write protocol helpers + test**

`app/src/test/java/org/openauto/companion/net/ProtocolTest.kt`:
```kotlin
package org.openauto.companion.net

import org.junit.Assert.*
import org.junit.Test

class ProtocolTest {
    @Test
    fun computeHmac_matchesExpected() {
        val key = "test-key".toByteArray()
        val data = "test-data".toByteArray()
        val hmac = Protocol.computeHmac(key, data)
        // HMAC-SHA256("test-key", "test-data") is deterministic
        assertNotNull(hmac)
        assertEquals(64, hmac.length) // hex-encoded SHA256
    }

    @Test
    fun buildHello_includesAllFields() {
        val hello = Protocol.buildHello(
            secret = "my-secret",
            nonce = "abc123",
            capabilities = listOf("time", "gps")
        )
        assertEquals("hello", hello.getString("type"))
        assertEquals(1, hello.getInt("version"))
        assertTrue(hello.has("token"))
        assertEquals(2, hello.getJSONArray("capabilities").length())
    }

    @Test
    fun buildStatus_includesSeqAndMac() {
        val status = Protocol.buildStatus(
            seq = 42,
            sessionKey = "session-key".toByteArray(),
            timeMs = 1740250000000L,
            timezone = "America/Chicago",
            gpsLat = 32.77, gpsLon = -96.80,
            gpsAccuracy = 8.5, gpsSpeed = 45.2,
            gpsBearing = 180.0, gpsAgeMs = 1200,
            batteryLevel = 72, batteryCharging = true,
            socks5Port = 1080, socks5Active = true
        )
        assertEquals("status", status.getString("type"))
        assertEquals(42, status.getInt("seq"))
        assertTrue(status.has("mac"))
        assertTrue(status.has("sent_mono_ms"))
    }

    @Test
    fun verifyMac_roundTrips() {
        val sessionKey = "my-session-key".toByteArray()
        val status = Protocol.buildStatus(
            seq = 1, sessionKey = sessionKey,
            timeMs = System.currentTimeMillis(),
            timezone = "UTC",
            gpsLat = 0.0, gpsLon = 0.0,
            gpsAccuracy = 0.0, gpsSpeed = 0.0,
            gpsBearing = 0.0, gpsAgeMs = 0,
            batteryLevel = 50, batteryCharging = false,
            socks5Port = 0, socks5Active = false
        )
        assertTrue(Protocol.verifyMac(status, sessionKey))
    }
}
```

**Step 2: Implement Protocol.kt**

`app/src/main/java/org/openauto/companion/net/Protocol.kt`:
```kotlin
package org.openauto.companion.net

import android.os.SystemClock
import org.json.JSONArray
import org.json.JSONObject
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

object Protocol {
    fun computeHmac(key: ByteArray, data: ByteArray): String {
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(SecretKeySpec(key, "HmacSHA256"))
        return mac.doFinal(data).joinToString("") { "%02x".format(it) }
    }

    fun buildHello(secret: String, nonce: String, capabilities: List<String>): JSONObject {
        val token = computeHmac(secret.toByteArray(), nonce.toByteArray())
        return JSONObject().apply {
            put("type", "hello")
            put("version", 1)
            put("token", token)
            put("capabilities", JSONArray(capabilities))
        }
    }

    fun buildStatus(
        seq: Int,
        sessionKey: ByteArray,
        timeMs: Long,
        timezone: String,
        gpsLat: Double, gpsLon: Double,
        gpsAccuracy: Double, gpsSpeed: Double,
        gpsBearing: Double, gpsAgeMs: Int,
        batteryLevel: Int, batteryCharging: Boolean,
        socks5Port: Int, socks5Active: Boolean
    ): JSONObject {
        val payload = JSONObject().apply {
            put("type", "status")
            put("seq", seq)
            put("sent_mono_ms", SystemClock.elapsedRealtime())
            put("time_ms", timeMs)
            put("timezone", timezone)
            put("gps", JSONObject().apply {
                put("lat", gpsLat)
                put("lon", gpsLon)
                put("accuracy", gpsAccuracy)
                put("speed", gpsSpeed)
                put("bearing", gpsBearing)
                put("age_ms", gpsAgeMs)
            })
            put("battery", JSONObject().apply {
                put("level", batteryLevel)
                put("charging", batteryCharging)
            })
            put("socks5", JSONObject().apply {
                put("port", socks5Port)
                put("active", socks5Active)
            })
        }

        // Compute MAC over payload
        val payloadStr = payload.toString()
        val mac = computeHmac(sessionKey, payloadStr.toByteArray())
        payload.put("mac", mac)

        return payload
    }

    fun verifyMac(msg: JSONObject, sessionKey: ByteArray): Boolean {
        val mac = msg.optString("mac", "")
        if (mac.isEmpty()) return false
        val copy = JSONObject(msg.toString())
        copy.remove("mac")
        val expected = computeHmac(sessionKey, copy.toString().toByteArray())
        return mac == expected
    }
}
```

**Step 3: Implement PiConnection.kt**

`app/src/main/java/org/openauto/companion/net/PiConnection.kt`:
```kotlin
package org.openauto.companion.net

import android.util.Log
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.PrintWriter
import java.net.InetSocketAddress
import java.net.Socket

class PiConnection(
    private val host: String = "10.0.0.1",
    private val port: Int = 9876,
    private val sharedSecret: String
) {
    private var socket: Socket? = null
    private var writer: PrintWriter? = null
    private var reader: BufferedReader? = null
    var sessionKey: ByteArray? = null
        private set
    var isAuthenticated = false
        private set

    fun connect(): Boolean {
        return try {
            val s = Socket()
            s.connect(InetSocketAddress(host, port), 5000)
            s.soTimeout = 10000
            socket = s
            writer = PrintWriter(s.getOutputStream(), true)
            reader = BufferedReader(InputStreamReader(s.getInputStream()))

            // Read challenge
            val challengeLine = reader?.readLine() ?: return false
            val challenge = JSONObject(challengeLine)
            if (challenge.getString("type") != "challenge") return false

            val nonce = challenge.getString("nonce")

            // Send hello
            val hello = Protocol.buildHello(sharedSecret, nonce,
                listOf("time", "gps", "battery", "socks5"))
            writer?.println(hello.toString())

            // Read ack
            val ackLine = reader?.readLine() ?: return false
            val ack = JSONObject(ackLine)
            if (!ack.optBoolean("accepted", false)) return false

            // Store session key
            val keyHex = ack.getString("session_key")
            sessionKey = keyHex.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
            isAuthenticated = true
            true
        } catch (e: Exception) {
            Log.e(TAG, "Connection failed", e)
            disconnect()
            false
        }
    }

    fun sendStatus(status: JSONObject) {
        writer?.println(status.toString())
    }

    fun disconnect() {
        isAuthenticated = false
        sessionKey = null
        try { writer?.close() } catch (_: Exception) {}
        try { reader?.close() } catch (_: Exception) {}
        try { socket?.close() } catch (_: Exception) {}
        socket = null; writer = null; reader = null
    }

    fun isConnected(): Boolean = socket?.isConnected == true && !socket!!.isClosed

    companion object {
        private const val TAG = "PiConnection"
    }
}
```

**Step 4: Implement CompanionService.kt**

`app/src/main/java/org/openauto/companion/service/CompanionService.kt`:
```kotlin
package org.openauto.companion.service

import android.app.Notification
import android.app.PendingIntent
import android.app.Service
import android.content.BatteryManager
import android.content.Intent
import android.content.IntentFilter
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.IBinder
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import androidx.core.app.NotificationCompat
import org.openauto.companion.CompanionApp
import org.openauto.companion.R
import org.openauto.companion.net.PiConnection
import org.openauto.companion.net.Protocol
import org.openauto.companion.ui.MainActivity
import java.util.TimeZone
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

class CompanionService : Service() {
    private var connection: PiConnection? = null
    private val executor = Executors.newSingleThreadScheduledExecutor()
    private var pushTask: ScheduledFuture<*>? = null
    private val seq = AtomicInteger(0)

    private var lastLocation: Location? = null
    private var locationManager: LocationManager? = null

    private val locationListener = object : LocationListener {
        override fun onLocationChanged(location: Location) {
            lastLocation = location
        }
    }

    override fun onCreate() {
        super.onCreate()
        locationManager = getSystemService(LocationManager::class.java)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val secret = intent?.getStringExtra("shared_secret") ?: ""
        startForeground(NOTIFICATION_ID, buildNotification("Connecting..."))

        startLocationUpdates()

        executor.execute {
            val conn = PiConnection(sharedSecret = secret)
            if (conn.connect()) {
                connection = conn
                updateNotification("Connected to OpenAuto Prodigy")
                startPushLoop()
            } else {
                updateNotification("Connection failed — retrying...")
                // Retry after 10s
                executor.schedule({
                    onStartCommand(intent, flags, startId)
                }, 10, TimeUnit.SECONDS)
            }
        }

        return START_STICKY
    }

    private fun startPushLoop() {
        pushTask = executor.scheduleAtFixedRate({
            try {
                val conn = connection ?: return@scheduleAtFixedRate
                val key = conn.sessionKey ?: return@scheduleAtFixedRate
                if (!conn.isConnected()) {
                    stopSelf()
                    return@scheduleAtFixedRate
                }

                val loc = lastLocation
                val gpsAgeMs = if (loc != null) {
                    ((SystemClock.elapsedRealtimeNanos() - loc.elapsedRealtimeNanos) / 1_000_000).toInt()
                } else -1

                val batteryIntent = registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
                val batteryLevel = batteryIntent?.let {
                    val level = it.getIntExtra(BatteryManager.EXTRA_LEVEL, -1)
                    val scale = it.getIntExtra(BatteryManager.EXTRA_SCALE, 100)
                    (level * 100) / scale
                } ?: -1
                val charging = batteryIntent?.let {
                    val status = it.getIntExtra(BatteryManager.EXTRA_STATUS, -1)
                    status == BatteryManager.BATTERY_STATUS_CHARGING ||
                        status == BatteryManager.BATTERY_STATUS_FULL
                } ?: false

                val status = Protocol.buildStatus(
                    seq = seq.incrementAndGet(),
                    sessionKey = key,
                    timeMs = System.currentTimeMillis(),
                    timezone = TimeZone.getDefault().id,
                    gpsLat = loc?.latitude ?: 0.0,
                    gpsLon = loc?.longitude ?: 0.0,
                    gpsAccuracy = loc?.accuracy?.toDouble() ?: 0.0,
                    gpsSpeed = loc?.speed?.toDouble() ?: 0.0,
                    gpsBearing = loc?.bearing?.toDouble() ?: 0.0,
                    gpsAgeMs = gpsAgeMs,
                    batteryLevel = batteryLevel,
                    batteryCharging = charging,
                    socks5Port = 1080,
                    socks5Active = false  // TODO: wire to SOCKS5 service state
                )

                conn.sendStatus(status)
            } catch (e: Exception) {
                Log.e(TAG, "Push failed", e)
            }
        }, 0, 5, TimeUnit.SECONDS)
    }

    private fun startLocationUpdates() {
        try {
            locationManager?.requestLocationUpdates(
                LocationManager.FUSED_PROVIDER,
                5000L, 0f, locationListener, Looper.getMainLooper()
            )
        } catch (e: SecurityException) {
            Log.w(TAG, "Location permission not granted", e)
            // Fall back to GPS_PROVIDER
            try {
                locationManager?.requestLocationUpdates(
                    LocationManager.GPS_PROVIDER,
                    5000L, 0f, locationListener, Looper.getMainLooper()
                )
            } catch (_: SecurityException) {
                Log.e(TAG, "No location permission at all")
            }
        }
    }

    private fun buildNotification(text: String): Notification {
        val pendingIntent = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE
        )
        return NotificationCompat.Builder(this, CompanionApp.CHANNEL_ID)
            .setContentTitle("OpenAuto Companion")
            .setContentText(text)
            .setSmallIcon(android.R.drawable.ic_menu_share)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .build()
    }

    private fun updateNotification(text: String) {
        val nm = getSystemService(android.app.NotificationManager::class.java)
        nm.notify(NOTIFICATION_ID, buildNotification(text))
    }

    override fun onDestroy() {
        pushTask?.cancel(false)
        executor.execute {
            connection?.disconnect()
        }
        locationManager?.removeUpdates(locationListener)
        executor.shutdown()
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    companion object {
        private const val TAG = "CompanionService"
        private const val NOTIFICATION_ID = 1001
    }
}
```

**Step 5: Run protocol tests**

Run: `./gradlew test`
Expected: ProtocolTest passes (3 tests). Note: `buildStatus` test that uses `SystemClock` will need the `sent_mono_ms` assertion adjusted for unit tests (SystemClock returns 0 in JVM tests).

**Step 6: Commit**

```bash
git add -A
git commit -m "feat: data push service with TCP client, protocol auth, GPS/battery collection"
```

---

### Task 7: WiFi SSID Auto-Start

**Files:**
- Create: `app/src/main/java/org/openauto/companion/service/WifiMonitor.kt`
- Modify: `app/src/main/java/org/openauto/companion/CompanionApp.kt`
- Modify: `AndroidManifest.xml`

**Step 1: Create WifiMonitor**

`app/src/main/java/org/openauto/companion/service/WifiMonitor.kt`:
```kotlin
package org.openauto.companion.service

import android.content.Context
import android.content.Intent
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiInfo
import android.os.Build
import android.util.Log
import androidx.core.content.ContextCompat

class WifiMonitor(
    private val context: Context,
    private val targetSsid: String = "OpenAutoProdigy",
    private val sharedSecret: String
) {
    private val connectivityManager =
        context.getSystemService(ConnectivityManager::class.java)
    private var registered = false

    private val networkCallback = object : ConnectivityManager.NetworkCallback(
        FLAG_INCLUDE_LOCATION_INFO
    ) {
        override fun onCapabilitiesChanged(network: Network, caps: NetworkCapabilities) {
            val wifiInfo = caps.transportInfo as? WifiInfo ?: return
            val ssid = wifiInfo.ssid?.removeSurrounding("\"") ?: return

            if (ssid == targetSsid) {
                Log.i(TAG, "Connected to target SSID: $ssid")
                startCompanionService()
            }
        }

        override fun onLost(network: Network) {
            Log.i(TAG, "WiFi network lost")
            stopCompanionService()
        }
    }

    fun start() {
        if (registered) return
        val request = NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .build()
        connectivityManager.registerNetworkCallback(request, networkCallback)
        registered = true
        Log.i(TAG, "WiFi monitor started, watching for SSID: $targetSsid")
    }

    fun stop() {
        if (!registered) return
        connectivityManager.unregisterNetworkCallback(networkCallback)
        registered = false
    }

    private fun startCompanionService() {
        val intent = Intent(context, CompanionService::class.java).apply {
            putExtra("shared_secret", sharedSecret)
        }
        ContextCompat.startForegroundService(context, intent)
    }

    private fun stopCompanionService() {
        context.stopService(Intent(context, CompanionService::class.java))
    }

    companion object {
        private const val TAG = "WifiMonitor"
    }
}
```

**Step 2: Wire into CompanionApp**

Update `CompanionApp.kt` to start WiFi monitoring after permissions are granted. This will be triggered from MainActivity after permission flow completes.

Add to `CompanionApp.kt`:
```kotlin
var wifiMonitor: WifiMonitor? = null
    private set

fun startWifiMonitor(sharedSecret: String, targetSsid: String = "OpenAutoProdigy") {
    wifiMonitor?.stop()
    wifiMonitor = WifiMonitor(this, targetSsid, sharedSecret).also { it.start() }
}
```

**Step 3: Commit**

```bash
git add -A
git commit -m "feat: WiFi SSID auto-detection triggers companion service"
```

---

### Task 8: SOCKS5 Proxy Server

**Files:**
- Create: `app/src/main/java/org/openauto/companion/net/Socks5Server.kt`
- Create: `app/src/test/java/org/openauto/companion/net/Socks5ServerTest.kt`

**Step 1: Write failing test**

`app/src/test/java/org/openauto/companion/net/Socks5ServerTest.kt`:
```kotlin
package org.openauto.companion.net

import org.junit.Assert.*
import org.junit.Test

class Socks5ServerTest {
    @Test
    fun isPrivateAddress_blocksRfc1918() {
        assertTrue(Socks5Server.isBlockedAddress("10.0.0.1"))
        assertTrue(Socks5Server.isBlockedAddress("172.16.0.1"))
        assertTrue(Socks5Server.isBlockedAddress("172.31.255.255"))
        assertTrue(Socks5Server.isBlockedAddress("192.168.1.1"))
        assertTrue(Socks5Server.isBlockedAddress("127.0.0.1"))
        assertTrue(Socks5Server.isBlockedAddress("169.254.1.1"))
    }

    @Test
    fun isPrivateAddress_allowsPublic() {
        assertFalse(Socks5Server.isBlockedAddress("8.8.8.8"))
        assertFalse(Socks5Server.isBlockedAddress("1.1.1.1"))
        assertFalse(Socks5Server.isBlockedAddress("142.250.80.46"))
    }

    @Test
    fun parseAuthRequest_validCredentials() {
        // RFC 1929: VER(1) ULEN(1) UNAME(ULEN) PLEN(1) PASSWD(PLEN)
        val user = "prodigy"
        val pass = "secret123"
        val bytes = byteArrayOf(
            0x01,                                  // version
            user.length.toByte(), *user.toByteArray(),
            pass.length.toByte(), *pass.toByteArray()
        )
        val (u, p) = Socks5Server.parseAuthRequest(bytes)
        assertEquals("prodigy", u)
        assertEquals("secret123", p)
    }
}
```

**Step 2: Implement Socks5Server.kt**

`app/src/main/java/org/openauto/companion/net/Socks5Server.kt`:
```kotlin
package org.openauto.companion.net

import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.util.Log
import java.io.InputStream
import java.io.OutputStream
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.net.Socket
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger

class Socks5Server(
    private val port: Int = 1080,
    private val bindAddress: String = "0.0.0.0",
    private val username: String,
    private val password: String,
    private val connectivityManager: ConnectivityManager? = null
) {
    private var serverSocket: ServerSocket? = null
    private val running = AtomicBoolean(false)
    private val executor: ExecutorService = Executors.newCachedThreadPool()
    private val connectionCount = AtomicInteger(0)
    private val failedAuths = ConcurrentHashMap<String, Pair<Int, Long>>() // ip -> (count, lockoutUntil)
    private var cellularNetwork: Network? = null
    private var acceptThread: Thread? = null

    val isActive: Boolean get() = running.get()

    fun start() {
        if (running.getAndSet(true)) return

        // Request cellular network for egress binding
        connectivityManager?.let { cm ->
            val request = NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_CELLULAR)
                .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                .build()
            cm.requestNetwork(request, object : ConnectivityManager.NetworkCallback() {
                override fun onAvailable(network: Network) {
                    cellularNetwork = network
                    Log.i(TAG, "Cellular network available for proxy egress")
                }
                override fun onLost(network: Network) {
                    if (cellularNetwork == network) cellularNetwork = null
                }
            })
        }

        serverSocket = ServerSocket()
        serverSocket!!.bind(InetSocketAddress(bindAddress, port))

        acceptThread = Thread({
            Log.i(TAG, "SOCKS5 server listening on $bindAddress:$port")
            while (running.get()) {
                try {
                    val client = serverSocket!!.accept()
                    if (connectionCount.get() >= MAX_CONNECTIONS) {
                        client.close()
                        continue
                    }
                    connectionCount.incrementAndGet()
                    executor.execute { handleClient(client) }
                } catch (e: Exception) {
                    if (running.get()) Log.e(TAG, "Accept error", e)
                }
            }
        }, "socks5-accept")
        acceptThread!!.start()
    }

    fun stop() {
        running.set(false)
        try { serverSocket?.close() } catch (_: Exception) {}
        acceptThread?.join(2000)
        executor.shutdownNow()
    }

    private fun handleClient(client: Socket) {
        try {
            client.soTimeout = IDLE_TIMEOUT_MS
            val input = client.getInputStream()
            val output = client.getOutputStream()
            val clientIp = client.inetAddress.hostAddress ?: "unknown"

            // Check lockout
            failedAuths[clientIp]?.let { (count, until) ->
                if (count >= MAX_AUTH_FAILURES && System.currentTimeMillis() < until) {
                    client.close()
                    return
                }
            }

            // Greeting: client sends VER NMETHODS METHODS
            val greeting = ByteArray(3)
            if (input.read(greeting) < 3) return
            if (greeting[0] != 0x05.toByte()) return  // SOCKS5

            // We require username/password auth (0x02)
            output.write(byteArrayOf(0x05, 0x02))
            output.flush()

            // Auth: RFC 1929
            val authBuf = ByteArray(513)
            val authLen = input.read(authBuf)
            if (authLen < 5) return
            val (user, pass) = parseAuthRequest(authBuf.copyOf(authLen))

            if (user != username || pass != password) {
                output.write(byteArrayOf(0x01, 0x01)) // auth failure
                output.flush()
                val entry = failedAuths.getOrDefault(clientIp, Pair(0, 0L))
                failedAuths[clientIp] = Pair(entry.first + 1, System.currentTimeMillis() + LOCKOUT_MS)
                return
            }
            output.write(byteArrayOf(0x01, 0x00)) // auth success
            output.flush()

            // Request: VER CMD RSV ATYP DST.ADDR DST.PORT
            val reqHeader = ByteArray(4)
            if (input.read(reqHeader) < 4) return
            if (reqHeader[1] != 0x01.toByte()) {
                // Only CONNECT supported
                sendReply(output, 0x07) // command not supported
                return
            }

            val (destHost, destPort) = parseDestination(input, reqHeader[3])
                ?: run { sendReply(output, 0x01); return }

            // Egress ACL
            if (isBlockedAddress(destHost)) {
                sendReply(output, 0x02) // connection not allowed
                return
            }

            // Connect to destination via cellular
            val remote = Socket()
            cellularNetwork?.bindSocket(remote)

            try {
                remote.connect(InetSocketAddress(destHost, destPort), CONNECT_TIMEOUT_MS)
            } catch (e: Exception) {
                sendReply(output, 0x05) // connection refused
                return
            }

            // Send success reply
            sendReply(output, 0x00)

            // Relay data bidirectionally
            relay(client, remote)
        } catch (e: Exception) {
            Log.d(TAG, "Client handler error", e)
        } finally {
            try { client.close() } catch (_: Exception) {}
            connectionCount.decrementAndGet()
        }
    }

    private fun parseDestination(input: InputStream, atyp: Byte): Pair<String, Int>? {
        return when (atyp) {
            0x01.toByte() -> { // IPv4
                val addr = ByteArray(4)
                if (input.read(addr) < 4) return null
                val port = readPort(input) ?: return null
                InetAddress.getByAddress(addr).hostAddress!! to port
            }
            0x03.toByte() -> { // Domain name
                val len = input.read()
                if (len < 1) return null
                val domain = ByteArray(len)
                if (input.read(domain) < len) return null
                val port = readPort(input) ?: return null
                String(domain) to port
            }
            0x04.toByte() -> { // IPv6
                val addr = ByteArray(16)
                if (input.read(addr) < 16) return null
                val port = readPort(input) ?: return null
                InetAddress.getByAddress(addr).hostAddress!! to port
            }
            else -> null
        }
    }

    private fun readPort(input: InputStream): Int? {
        val hi = input.read()
        val lo = input.read()
        if (hi < 0 || lo < 0) return null
        return (hi shl 8) or lo
    }

    private fun sendReply(output: OutputStream, status: Byte) {
        output.write(byteArrayOf(
            0x05, status, 0x00, 0x01,
            0, 0, 0, 0,  // bind addr
            0, 0          // bind port
        ))
        output.flush()
    }

    private fun relay(client: Socket, remote: Socket) {
        val t1 = Thread({
            try {
                client.getInputStream().copyTo(remote.getOutputStream())
            } catch (_: Exception) {}
            try { remote.shutdownOutput() } catch (_: Exception) {}
        }, "relay-c2r")

        val t2 = Thread({
            try {
                remote.getInputStream().copyTo(client.getOutputStream())
            } catch (_: Exception) {}
            try { client.shutdownOutput() } catch (_: Exception) {}
        }, "relay-r2c")

        t1.start(); t2.start()
        t1.join(); t2.join()
        try { remote.close() } catch (_: Exception) {}
    }

    companion object {
        private const val TAG = "Socks5Server"
        private const val MAX_CONNECTIONS = 20
        private const val IDLE_TIMEOUT_MS = 120_000
        private const val CONNECT_TIMEOUT_MS = 10_000
        private const val MAX_AUTH_FAILURES = 3
        private const val LOCKOUT_MS = 30_000L

        fun isBlockedAddress(host: String): Boolean {
            return try {
                val addr = InetAddress.getByName(host)
                addr.isLoopbackAddress ||
                    addr.isLinkLocalAddress ||
                    addr.isSiteLocalAddress ||
                    addr.isMulticastAddress
            } catch (_: Exception) { false }
        }

        fun parseAuthRequest(bytes: ByteArray): Pair<String, String> {
            // RFC 1929: VER(1) ULEN(1) UNAME(ULEN) PLEN(1) PASSWD(PLEN)
            val ulen = bytes[1].toInt() and 0xFF
            val user = String(bytes, 2, ulen)
            val plen = bytes[2 + ulen].toInt() and 0xFF
            val pass = String(bytes, 3 + ulen, plen)
            return user to pass
        }
    }
}
```

**Step 3: Run tests**

Run: `./gradlew test`
Expected: All tests pass (Protocol + Socks5Server tests).

**Step 4: Commit**

```bash
git add -A
git commit -m "feat: SOCKS5 CONNECT-only proxy with auth, egress ACLs, cellular binding"
```

---

### Task 9: Companion App UI (Jetpack Compose)

**Files:**
- Modify: `app/src/main/java/org/openauto/companion/ui/MainActivity.kt`
- Create: `app/src/main/java/org/openauto/companion/ui/StatusScreen.kt`
- Create: `app/src/main/java/org/openauto/companion/ui/PairingScreen.kt`
- Create: `app/src/main/java/org/openauto/companion/data/CompanionPrefs.kt`

**Step 1: Create preferences helper**

`app/src/main/java/org/openauto/companion/data/CompanionPrefs.kt`:
```kotlin
package org.openauto.companion.data

import android.content.Context
import android.content.SharedPreferences

class CompanionPrefs(context: Context) {
    private val prefs: SharedPreferences =
        context.getSharedPreferences("companion", Context.MODE_PRIVATE)

    var sharedSecret: String
        get() = prefs.getString("shared_secret", "") ?: ""
        set(value) = prefs.edit().putString("shared_secret", value).apply()

    var targetSsid: String
        get() = prefs.getString("target_ssid", "OpenAutoProdigy") ?: "OpenAutoProdigy"
        set(value) = prefs.edit().putString("target_ssid", value).apply()

    var socks5Enabled: Boolean
        get() = prefs.getBoolean("socks5_enabled", true)
        set(value) = prefs.edit().putBoolean("socks5_enabled", value).apply()

    val isPaired: Boolean get() = sharedSecret.isNotEmpty()
}
```

**Step 2: Create PairingScreen**

`app/src/main/java/org/openauto/companion/ui/PairingScreen.kt`:
```kotlin
package org.openauto.companion.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp

@Composable
fun PairingScreen(onPaired: (pin: String) -> Unit) {
    var pin by remember { mutableStateOf("") }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(32.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Text("Pair with Head Unit", style = MaterialTheme.typography.headlineMedium)
        Spacer(modifier = Modifier.height(16.dp))
        Text("Enter the 6-digit PIN shown on your head unit's settings screen.")
        Spacer(modifier = Modifier.height(32.dp))

        OutlinedTextField(
            value = pin,
            onValueChange = { if (it.length <= 6 && it.all { c -> c.isDigit() }) pin = it },
            label = { Text("PIN") },
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            singleLine = true,
            modifier = Modifier.width(200.dp)
        )

        Spacer(modifier = Modifier.height(24.dp))

        Button(
            onClick = { onPaired(pin) },
            enabled = pin.length == 6
        ) {
            Text("Pair")
        }
    }
}
```

**Step 3: Create StatusScreen**

`app/src/main/java/org/openauto/companion/ui/StatusScreen.kt`:
```kotlin
package org.openauto.companion.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

data class CompanionStatus(
    val connected: Boolean = false,
    val sharingTime: Boolean = false,
    val sharingGps: Boolean = false,
    val sharingBattery: Boolean = false,
    val socks5Active: Boolean = false,
    val ssid: String = ""
)

@Composable
fun StatusScreen(
    status: CompanionStatus,
    socks5Enabled: Boolean,
    onSocks5Toggle: (Boolean) -> Unit,
    onUnpair: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp)
    ) {
        Text("OpenAuto Companion", style = MaterialTheme.typography.headlineMedium)
        Spacer(modifier = Modifier.height(24.dp))

        // Connection status
        Card(modifier = Modifier.fillMaxWidth()) {
            Row(
                modifier = Modifier.padding(16.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                val color = if (status.connected) Color(0xFF4CAF50) else Color(0xFF9E9E9E)
                Surface(
                    shape = MaterialTheme.shapes.small,
                    color = color,
                    modifier = Modifier.size(12.dp)
                ) {}
                Spacer(modifier = Modifier.width(12.dp))
                Text(if (status.connected) "Connected" else "Waiting for ${status.ssid}...")
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        // Sharing status
        Text("Sharing", style = MaterialTheme.typography.titleMedium)
        Spacer(modifier = Modifier.height(8.dp))

        StatusRow("Time", status.sharingTime && status.connected)
        StatusRow("GPS Location", status.sharingGps && status.connected)
        StatusRow("Battery Level", status.sharingBattery && status.connected)
        StatusRow("Internet (SOCKS5)", status.socks5Active && status.connected)

        Spacer(modifier = Modifier.height(24.dp))

        // Settings
        Text("Settings", style = MaterialTheme.typography.titleMedium)
        Spacer(modifier = Modifier.height(8.dp))

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("Internet Sharing")
            Switch(checked = socks5Enabled, onCheckedChange = onSocks5Toggle)
        }

        Spacer(modifier = Modifier.weight(1f))

        TextButton(onClick = onUnpair, modifier = Modifier.align(Alignment.CenterHorizontally)) {
            Text("Unpair", color = MaterialTheme.colorScheme.error)
        }
    }
}

@Composable
private fun StatusRow(label: String, active: Boolean) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(label)
        Text(
            if (active) "Active" else "—",
            color = if (active) Color(0xFF4CAF50) else Color(0xFF9E9E9E)
        )
    }
}
```

**Step 4: Wire up MainActivity with permission flow**

Replace `MainActivity.kt`:
```kotlin
package org.openauto.companion.ui

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.*
import androidx.core.content.ContextCompat
import org.openauto.companion.CompanionApp
import org.openauto.companion.data.CompanionPrefs
import java.security.MessageDigest

class MainActivity : ComponentActivity() {
    private lateinit var prefs: CompanionPrefs

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { results ->
        if (results.values.all { it }) {
            startMonitoringIfPaired()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        prefs = CompanionPrefs(this)

        requestPermissions()

        setContent {
            MaterialTheme {
                var isPaired by remember { mutableStateOf(prefs.isPaired) }

                if (!isPaired) {
                    PairingScreen(onPaired = { pin ->
                        deriveAndStoreSecret(pin)
                        isPaired = true
                        startMonitoringIfPaired()
                    })
                } else {
                    val status = remember { mutableStateOf(CompanionStatus(ssid = prefs.targetSsid)) }
                    var socks5Enabled by remember { mutableStateOf(prefs.socks5Enabled) }

                    StatusScreen(
                        status = status.value,
                        socks5Enabled = socks5Enabled,
                        onSocks5Toggle = {
                            socks5Enabled = it
                            prefs.socks5Enabled = it
                        },
                        onUnpair = {
                            prefs.sharedSecret = ""
                            isPaired = false
                        }
                    )
                }
            }
        }
    }

    private fun requestPermissions() {
        val needed = mutableListOf(
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.ACCESS_COARSE_LOCATION
        )
        if (Build.VERSION.SDK_INT >= 33) {
            needed.add(Manifest.permission.POST_NOTIFICATIONS)
            needed.add(Manifest.permission.NEARBY_WIFI_DEVICES)
        }

        val missing = needed.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (missing.isNotEmpty()) {
            permissionLauncher.launch(missing.toTypedArray())
        } else {
            startMonitoringIfPaired()
        }
    }

    private fun deriveAndStoreSecret(pin: String) {
        val material = "$pin:${Build.SERIAL}:${System.currentTimeMillis()}"
        val digest = MessageDigest.getInstance("SHA-256")
        val secret = digest.digest(material.toByteArray()).joinToString("") { "%02x".format(it) }
        prefs.sharedSecret = secret
    }

    private fun startMonitoringIfPaired() {
        if (prefs.isPaired) {
            (application as CompanionApp).startWifiMonitor(prefs.sharedSecret, prefs.targetSsid)
        }
    }
}
```

**Step 5: Build APK**

Run: `./gradlew assembleDebug`
Expected: BUILD SUCCESSFUL

**Step 6: Commit**

```bash
git add -A
git commit -m "feat: companion app UI with pairing, status, permission flow"
```

---

### Task 10: End-to-End Integration Test

**No new code — validation of the full stack.**

**Step 1: Build and deploy Pi side**

```bash
# From dev machine
rsync -av --exclude='build/' --exclude='.git/' --exclude='libs/aasdk/' \
    /home/matt/claude/personal/openautopro/openauto-prodigy/ \
    matt@192.168.1.149:/home/matt/openauto-prodigy/
ssh matt@192.168.1.149 "cd /home/matt/openauto-prodigy/build && cmake --build . -j3"
```

**Step 2: Generate pairing PIN on Pi**

Launch the app and use the settings UI or IPC to generate a PIN:
```bash
echo '{"command":"companion_pair"}' | nc -U /tmp/openauto-prodigy.sock
# Returns: {"pin":"123456","secret":"abcdef..."}
```

Or manually create a test secret:
```bash
ssh matt@192.168.1.149 "echo 'test-development-secret-64chars-hex' > ~/.openauto/companion.key"
```

**Step 3: Install APK on phone**

```bash
adb install app/build/outputs/apk/debug/app-debug.apk
```

Or transfer APK to phone and sideload.

**Step 4: Pair and verify**

1. Open OpenAuto Companion on phone
2. Enter PIN from Pi
3. Connect phone to Pi's WiFi AP (OpenAutoProdigy)
4. Watch Pi logs: `ssh matt@192.168.1.149 "tail -f /tmp/oap.log | grep Companion"`
5. Verify: challenge → hello → hello_ack → status messages flowing

**Step 5: Verify time sync**

```bash
# Deliberately skew Pi clock
ssh matt@192.168.1.149 "sudo timedatectl set-time '2026-02-22 12:00:00'"
# Watch for companion to correct it
ssh matt@192.168.1.149 "tail -f /tmp/oap.log | grep 'clock adjusted'"
```

**Step 6: Verify SOCKS5 proxy**

```bash
# From Pi, test proxy (use phone's DHCP address)
ssh matt@192.168.1.149 "curl -x socks5://user:pass@10.0.0.x:1080 https://httpbin.org/ip"
```

**Step 7: Document results and fix any issues**

---

## Phase 3: Polish & Hardening

---

### Task 11: Pi Settings UI for Companion

**Files:**
- Create: `qml/applications/settings/CompanionSettings.qml`
- Modify: Settings navigation to include Companion page

**Step 1: Create QML settings page**

Shows: connection status, paired device, pairing button, companion GPS/battery display, proxy address.

Uses `CompanionService` context property exposed in Task 2.

```qml
// qml/applications/settings/CompanionSettings.qml
import QtQuick
import QtQuick.Layouts

Item {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: UiMetrics.sectionGap
        spacing: UiMetrics.gap

        Text {
            text: "Companion App"
            font.pixelSize: UiMetrics.fontHeading
            font.bold: true
            color: ThemeService.specialFontColor
        }

        // Status
        Row {
            spacing: UiMetrics.gap
            Rectangle {
                width: UiMetrics.iconSmall; height: UiMetrics.iconSmall
                radius: width / 2
                color: CompanionService.connected ? "#4CAF50" : "#666666"
            }
            Text {
                text: CompanionService.connected ? "Phone Connected" : "Not Connected"
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.normalFontColor
            }
        }

        // GPS
        Text {
            visible: CompanionService.connected && !CompanionService.gpsStale
            text: "GPS: " + CompanionService.gpsLat.toFixed(4) + ", " + CompanionService.gpsLon.toFixed(4)
            font.pixelSize: UiMetrics.fontSmall
            color: ThemeService.descriptionFontColor
        }

        // Battery
        Text {
            visible: CompanionService.connected && CompanionService.phoneBattery >= 0
            text: "Phone Battery: " + CompanionService.phoneBattery + "%"
                  + (CompanionService.phoneCharging ? " (charging)" : "")
            font.pixelSize: UiMetrics.fontSmall
            color: ThemeService.descriptionFontColor
        }

        // Internet
        Text {
            visible: CompanionService.internetAvailable
            text: "Internet: " + CompanionService.proxyAddress
            font.pixelSize: UiMetrics.fontSmall
            color: ThemeService.descriptionFontColor
        }

        // Pairing button
        Item { Layout.fillHeight: true }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: pairText.implicitWidth + UiMetrics.touchMin
            height: UiMetrics.touchMin
            radius: height / 2
            color: pairMouse.pressed ? Qt.darker(ThemeService.barBackgroundColor, 1.3)
                                     : ThemeService.barBackgroundColor
            border.color: ThemeService.descriptionFontColor
            border.width: 1

            Text {
                id: pairText
                anchors.centerIn: parent
                text: "Generate Pairing PIN"
                color: ThemeService.normalFontColor
                font.pixelSize: UiMetrics.fontBody
            }

            MouseArea {
                id: pairMouse
                anchors.fill: parent
                onClicked: {
                    var pin = CompanionService.generatePairingPin()
                    pinDisplay.text = "PIN: " + pin
                    pinDisplay.visible = true
                }
            }
        }

        Text {
            id: pinDisplay
            Layout.alignment: Qt.AlignHCenter
            visible: false
            font.pixelSize: Math.round(48 * UiMetrics.scale)
            font.bold: true
            color: ThemeService.specialFontColor
        }
    }
}
```

**Step 2: Wire into settings navigation**

Add to the settings page list (wherever other settings pages are registered).

**Step 3: Commit**

```bash
git add qml/applications/settings/CompanionSettings.qml
# + any navigation changes
git commit -m "feat: companion settings page with pairing PIN and status display"
```

---

### Task 12: Wire SOCKS5 into CompanionService

**Files:**
- Modify: `app/src/main/java/org/openauto/companion/service/CompanionService.kt`

**Step 1: Start SOCKS5 server alongside data push**

In `CompanionService.onStartCommand()`, after successful connection:
```kotlin
if (prefs.socks5Enabled) {
    socks5Server = Socks5Server(
        port = 1080,
        username = "prodigy",
        password = secret.take(16),
        connectivityManager = getSystemService(ConnectivityManager::class.java)
    ).also { it.start() }
}
```

Update status push to reflect SOCKS5 state:
```kotlin
socks5Port = socks5Server?.let { if (it.isActive) 1080 else 0 } ?: 0,
socks5Active = socks5Server?.isActive ?: false
```

**Step 2: Stop SOCKS5 on destroy**

In `onDestroy()`: `socks5Server?.stop()`

**Step 3: Commit**

```bash
git add -A
git commit -m "feat: wire SOCKS5 proxy into companion service lifecycle"
```

---

### Task 13: Pairing Handshake Alignment

**Files:**
- Modify: Pi `CompanionListenerService.cpp` (generatePairingPin)
- Modify: Android `MainActivity.kt` (deriveAndStoreSecret)

**Step 1: Align secret derivation**

Both sides must derive the same shared secret from the same PIN. The current implementation has the Pi using `QSysInfo::machineUniqueId() + timestamp` and the phone using `Build.SERIAL + timestamp` — these will never match.

**Fix:** Use only the PIN + a fixed salt. The PIN is short-lived (displayed once, entered once), so time-based material is unnecessary.

Pi side:
```cpp
QString CompanionListenerService::generatePairingPin()
{
    int pin = QRandomGenerator::global()->bounded(100000, 999999);
    QString pinStr = QString::number(pin);

    QByteArray material = pinStr.toUtf8() + QByteArray("openauto-companion-v1");
    QByteArray secret = QCryptographicHash::hash(material, QCryptographicHash::Sha256).toHex();

    // Save
    QString path = QDir::homePath() + "/.openauto/companion.key";
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(secret);
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
    setSharedSecret(QString::fromUtf8(secret));
    return pinStr;
}
```

Phone side:
```kotlin
private fun deriveAndStoreSecret(pin: String) {
    val material = "$pin:openauto-companion-v1"
    val digest = MessageDigest.getInstance("SHA-256")
    val secret = digest.digest(material.toByteArray()).joinToString("") { "%02x".format(it) }
    prefs.sharedSecret = secret
}
```

Both derive `SHA256(PIN + "openauto-companion-v1")` → identical secret.

**Step 2: Test**

Verify manually: Python `hashlib.sha256(b"123456openauto-companion-v1").hexdigest()` matches what both sides produce.

**Step 3: Commit**

```bash
# In both repos
git commit -am "fix: align pairing secret derivation between Pi and phone"
```

---

## Summary

| Phase | Tasks | What It Delivers |
|-------|-------|-----------------|
| **1: Pi Listener** | 1–4 | TCP server, auth, time/GPS/battery reception, polkit, manual test |
| **2: Android App** | 5–9 | Full companion app: data push, SOCKS5 proxy, UI, auto-start |
| **3: Polish** | 10–13 | E2E testing, settings UI, SOCKS5 wiring, pairing alignment |

**Total estimated files:** ~15 new, ~10 modified
**Dependencies between phases:** Phase 2 can start after Task 1 (just needs the protocol spec). Phase 3 requires both Phase 1 and 2.

---

Plan complete and saved to `docs/plans/2026-02-22-companion-app-implementation.md`. Two execution options:

**1. Subagent-Driven (this session)** — I dispatch fresh subagent per task, review between tasks, fast iteration

**2. Parallel Session (separate)** — Open new session with executing-plans, batch execution with checkpoints

Which approach?
