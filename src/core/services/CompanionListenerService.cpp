#include "CompanionListenerService.hpp"
#include <QJsonDocument>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QDateTime>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>

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

QString CompanionListenerService::generatePairingPin()
{
    int pin = QRandomGenerator::global()->bounded(100000, 999999);
    QString pinStr = QString::number(pin);

    // Derive shared secret: SHA256(PIN + fixed salt)
    // Both Pi and phone use this same derivation so they arrive at the same secret.
    QByteArray material = pinStr.toUtf8() + QByteArray(":openauto-companion-v1");
    QByteArray secret = QCryptographicHash::hash(material, QCryptographicHash::Sha256).toHex();

    // Save to file
    QString dir = QDir::homePath() + "/.openauto";
    QDir().mkpath(dir);
    QString path = dir + "/companion.key";
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(secret);
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }

    setSharedSecret(QString::fromUtf8(secret));
    qInfo() << "Companion: pairing PIN generated, secret saved to" << path;
    return pinStr;
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
        reinterpret_cast<quint32*>(nonce.data()), nonce.size() / static_cast<int>(sizeof(quint32)));
    currentNonce_ = nonce.toHex();

    QJsonObject challenge;
    challenge["type"] = "challenge";
    challenge["nonce"] = QString::fromLatin1(currentNonce_);
    challenge["version"] = 1;

    QByteArray challengeJson = QJsonDocument(challenge).toJson(QJsonDocument::Compact) + "\n";
    qInfo() << "Companion: sending challenge, nonce=" << currentNonce_.left(16) << "...";
    client_->write(challengeJson);
    client_->flush();
}

bool CompanionListenerService::validateHello(const QJsonObject& msg)
{
    if (msg["type"].toString() != "hello") return false;
    if (sharedSecret_.isEmpty()) {
        qWarning() << "Companion: auth failed — no shared secret set";
        return false;
    }

    QString token = msg["token"].toString();
    QByteArray expected = computeHmac(
        sharedSecret_.toUtf8(), currentNonce_);
    QString expectedHex = QString::fromLatin1(expected.toHex());

    qInfo() << "Companion auth:"
            << "secret_len=" << sharedSecret_.length()
            << "secret_prefix=" << sharedSecret_.left(8)
            << "nonce=" << currentNonce_.left(16) << "..."
            << "received_token=" << token.left(16) << "..."
            << "expected_token=" << expectedHex.left(16) << "..."
            << "match=" << (token == expectedHex);

    return token == expectedHex;
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
                    rawKey.size() / static_cast<int>(sizeof(quint32)));
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
            if (sessionKey_.isEmpty()) {
                qWarning() << "Companion: status msg but no session key";
                continue;
            }
            if (!verifyMac(msg, line)) {
                qWarning() << "Companion: status msg MAC failed";
                continue;
            }
            qInfo() << "Companion: valid status message received";
            handleStatus(msg);
        }
    }
}

bool CompanionListenerService::verifyMac(const QJsonObject& msg, const QByteArray& rawLine)
{
    QString mac = msg["mac"].toString();
    if (mac.isEmpty()) {
        qWarning() << "Companion: MAC empty in status message";
        return false;
    }

    // Strip the "mac" field from the raw JSON bytes to get the payload
    // that the sender computed the MAC over.
    // Strategy: parse, remove mac, re-serialize — but use sender's key order
    // by stripping the mac k/v from the raw bytes instead.
    //
    // Find and remove ,"mac":"<hex>" from the raw line.
    // This preserves the sender's exact serialization (key order, number format).
    QByteArray macPattern = ",\"mac\":\"" + mac.toLatin1() + "\"";
    QByteArray payloadBytes = rawLine;
    int macPos = payloadBytes.indexOf(macPattern);
    if (macPos < 0) {
        // Try alternate position: "mac" might be first key
        macPattern = "\"mac\":\"" + mac.toLatin1() + "\",";
        macPos = payloadBytes.indexOf(macPattern);
    }
    if (macPos >= 0) {
        payloadBytes.remove(macPos, macPattern.size());
    } else {
        qWarning() << "Companion: could not strip mac from raw payload";
        return false;
    }

    QByteArray expected = computeHmac(sessionKey_, payloadBytes);
    QString expectedHex = QString::fromLatin1(expected.toHex());

    bool ok = (mac == expectedHex);
    if (!ok) {
        qWarning() << "Companion: MAC mismatch"
                    << "received=" << mac.left(16) << "..."
                    << "expected=" << expectedHex.left(16) << "..."
                    << "payload_len=" << payloadBytes.size()
                    << "payload_prefix=" << payloadBytes.left(80);
    }
    return ok;
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
    // Qt 6.4: Qt::UTC, Qt 6.5+: QTimeZone::UTC (suppress deprecation on 6.8)
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QDateTime newTime = QDateTime::fromMSecsSinceEpoch(phoneTimeMs, Qt::UTC);
    QT_WARNING_POP
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
