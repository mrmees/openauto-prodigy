#include "CompanionListenerService.hpp"
#include "ThemeService.hpp"
#include "../Logging.hpp"
#include <QJsonDocument>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QDateTime>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QBuffer>
#include <QUuid>
#include <qrcodegen.hpp>

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

    listenPort_ = port;
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

void CompanionListenerService::loadOrGenerateVehicleId()
{
    QString dir = QDir::homePath() + "/.openauto";
    QDir().mkpath(dir);
    QString path = dir + "/vehicle.id";

    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        vehicleId_ = QString::fromUtf8(file.readAll().trimmed());
        if (!vehicleId_.isEmpty()) {
            qCInfo(lcCore) << "Companion: loaded vehicle_id" << vehicleId_;
            return;
        }
    }

    // Generate new UUID v4 (strip braces)
    vehicleId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);

    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(vehicleId_.toUtf8());
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
    qCInfo(lcCore) << "Companion: generated new vehicle_id" << vehicleId_;
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

    // Build QR payload with connection info
    QString payload = QString("openauto://pair?pin=%1&ssid=%2&host=10.0.0.1&port=%3")
        .arg(pinStr, wifiSsid_)
        .arg(listenPort_);
    if (!vehicleId_.isEmpty())
        payload += "&vehicle_id=" + vehicleId_;

    QByteArray pngData = generateQrPng(payload);
    qrCodeDataUri_ = "data:image/png;base64," + QString::fromLatin1(pngData.toBase64());
    emit qrCodeChanged();

    qCInfo(lcCore) << "Companion: pairing PIN generated, secret saved to" << path;
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
QString CompanionListenerService::qrCodeDataUri() const { return qrCodeDataUri_; }

QByteArray CompanionListenerService::generateQrPng(const QString& payload)
{
    using namespace qrcodegen;
    QrCode qr = QrCode::encodeText(payload.toUtf8().constData(), QrCode::Ecc::MEDIUM);

    int size = qr.getSize();
    int border = 2;
    int total = size + border * 2;
    int scale = 8;  // pixels per module — crisp at 200x200 display

    QImage image(total * scale, total * scale, QImage::Format_Grayscale8);
    image.fill(255);  // white

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (qr.getModule(x, y)) {
                int px = (x + border) * scale;
                int py = (y + border) * scale;
                for (int dy = 0; dy < scale; dy++)
                    for (int dx = 0; dx < scale; dx++)
                        image.setPixel(px + dx, py + dy, qRgb(0, 0, 0));
            }
        }
    }

    QByteArray pngData;
    QBuffer buffer(&pngData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return pngData;
}

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
    if (!vehicleId_.isEmpty())
        challenge["vehicle_id"] = vehicleId_;

    QByteArray challengeJson = QJsonDocument(challenge).toJson(QJsonDocument::Compact) + "\n";
    qCInfo(lcCore) << "Companion: sending challenge, nonce=" << currentNonce_.left(16) << "...";
    client_->write(challengeJson);
    client_->flush();
}

bool CompanionListenerService::validateHello(const QJsonObject& msg)
{
    if (msg["type"].toString() != "hello") return false;
    if (sharedSecret_.isEmpty()) {
        qCWarning(lcCore) << "Companion: auth failed — no shared secret set";
        return false;
    }

    QString token = msg["token"].toString();
    QByteArray expected = computeHmac(
        sharedSecret_.toUtf8(), currentNonce_);
    QString expectedHex = QString::fromLatin1(expected.toHex());

    qCInfo(lcCore) << "Companion auth:"
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
                if (!vehicleId_.isEmpty())
                    ack["vehicle_id"] = vehicleId_;

                QJsonObject display;
                display["width"] = displayWidth_;
                display["height"] = displayHeight_;
                ack["display"] = display;

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
                qCWarning(lcCore) << "Companion: status msg but no session key";
                continue;
            }
            if (!verifyMac(msg, line)) {
                qCWarning(lcCore) << "Companion: status msg MAC failed";
                continue;
            }
            qCInfo(lcCore) << "Companion: valid status message received";
            handleStatus(msg);
        } else if (type == "theme") {
            if (sessionKey_.isEmpty()) {
                qCWarning(lcCore) << "Companion: theme msg but no session key";
                continue;
            }
            if (!verifyMac(msg, line)) {
                qCWarning(lcCore) << "Companion: theme msg MAC failed";
                continue;
            }
            handleThemeMessage(msg);
        } else if (type == "theme_data") {
            if (sessionKey_.isEmpty()) {
                qCWarning(lcCore) << "Companion: theme_data msg but no session key";
                continue;
            }
            if (!verifyMac(msg, line)) {
                qCWarning(lcCore) << "Companion: theme_data msg MAC failed";
                continue;
            }
            handleThemeDataChunk(msg);
        }
    }
}

bool CompanionListenerService::verifyMac(const QJsonObject& msg, const QByteArray& rawLine)
{
    QString mac = msg["mac"].toString();
    if (mac.isEmpty()) {
        qCWarning(lcCore) << "Companion: MAC empty in status message";
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
        qCWarning(lcCore) << "Companion: could not strip mac from raw payload";
        return false;
    }

    QByteArray expected = computeHmac(sessionKey_, payloadBytes);
    QString expectedHex = QString::fromLatin1(expected.toHex());

    bool ok = (mac == expectedHex);
    if (!ok) {
        qCWarning(lcCore) << "Companion: MAC mismatch"
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
        int port = socks["port"].toInt(-1);
        QString host = client_ ? client_->peerAddress().toString() : QString();
        bool routeInfoChanged = (active && (host != requestedProxyHost_ || port != requestedProxyPort_));

        if (active && (port <= 0 || host.isEmpty())) {
            qCWarning(lcCore) << "Companion: invalid SOCKS5 status payload"
                       << "active=" << active << "port=" << port << "host=" << host;
            // Force disable routing if we can't apply it safely.
            if (internetAvailable_) {
                internetAvailable_ = false;
                proxyAddress_.clear();
                syncProxyRouteFromStatus(false, QString(), 0, QString());
                emit internetChanged();
            }
            return;
        }

        bool changed = (active != internetAvailable_) || routeInfoChanged;
        internetAvailable_ = active;
        if (active && !host.isEmpty()) {
            requestedProxyHost_ = host;
            requestedProxyPort_ = port;
            proxyAddress_ = QString("socks5://%1:%2").arg(host).arg(port);
            syncProxyRouteFromStatus(true, host, port, socks5Password());
        } else {
            proxyAddress_.clear();
            requestedProxyHost_.clear();
            requestedProxyPort_ = 0;
            syncProxyRouteFromStatus(false, QString(), 0, QString());
        }
        if (changed) {
            emit internetChanged();
        }
    }
}

void CompanionListenerService::syncProxyRouteFromStatus(
    bool active, const QString& host, int port, const QString& password)
{
    if (!systemClient_)
        return;

    if (active) {
        bool hostPortUnchanged = (proxyRouteApplied_
                                 && host == requestedProxyHost_
                                 && port == requestedProxyPort_);
        if (!hostPortUnchanged) {
            systemClient_->setProxyRoute(true, host, port, password);
        }
        proxyRouteApplied_ = true;
        requestedProxyHost_ = host;
        requestedProxyPort_ = port;
        return;
    }

    if (proxyRouteApplied_) {
        systemClient_->setProxyRoute(false);
    }
    proxyRouteApplied_ = false;
    requestedProxyHost_.clear();
    requestedProxyPort_ = 0;
}

void CompanionListenerService::syncProxyRoute()
{
    proxyRouteApplied_ = false;
    syncProxyRouteFromStatus(internetAvailable_, requestedProxyHost_, requestedProxyPort_,
                             socks5Password());
}

QString CompanionListenerService::socks5Password() const
{
    // SOCKS5 password = first 8 hex chars of shared secret (companion app convention)
    return sharedSecret_.left(8).toLower();
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
        qCInfo(lcCore) << "Companion: clock adjusted by" << deltaMs << "ms"
                << "(" << piTimeMs << "->" << phoneTimeMs << ")";
        emit timeAdjusted(piTimeMs, phoneTimeMs, deltaMs);
    } else {
        qCWarning(lcCore) << "Companion: timedatectl failed:" << proc.readAllStandardError();
    }
}

QByteArray CompanionListenerService::computeHmac(const QByteArray& key, const QByteArray& data)
{
    return QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha256);
}

void CompanionListenerService::handleThemeMessage(const QJsonObject& msg)
{
    QJsonObject theme = msg["theme"].toObject();
    if (theme.isEmpty()) {
        qCWarning(lcCore) << "Companion: theme message missing 'theme' object";
        return;
    }

    QJsonObject wallpaper = msg["wallpaper"].toObject();
    int chunks = wallpaper["chunks"].toInt(0);
    int size = wallpaper["size"].toInt(0);

    // Safety cap: reject wallpaper > 5MB
    if (size > 5 * 1024 * 1024) {
        qCWarning(lcCore) << "Companion: wallpaper too large (" << size << "bytes), rejecting theme";
        return;
    }

    pendingThemeJson_ = theme;
    expectedWallpaperChunks_ = chunks;
    receivedWallpaperChunks_ = 0;
    pendingWallpaperData_.clear();
    pendingWallpaperData_.reserve(size);

    qCInfo(lcCore) << "Companion: theme" << theme["name"].toString()
                   << "wallpaper:" << size << "bytes in" << chunks << "chunks";

    // No wallpaper — apply theme immediately
    if (chunks == 0) {
        applyReceivedTheme();
    }
}

void CompanionListenerService::handleThemeDataChunk(const QJsonObject& msg)
{
    if (expectedWallpaperChunks_ <= 0) {
        qCWarning(lcCore) << "Companion: theme_data received but no pending theme";
        return;
    }

    int index = msg["chunk"].toInt(-1);
    QString b64 = msg["data"].toString();

    if (index < 0 || b64.isEmpty()) {
        qCWarning(lcCore) << "Companion: invalid theme_data chunk (index=" << index << ")";
        return;
    }

    QByteArray decoded = QByteArray::fromBase64(b64.toLatin1());
    pendingWallpaperData_.append(decoded);
    receivedWallpaperChunks_++;

    qCDebug(lcCore) << "Companion: theme_data chunk" << receivedWallpaperChunks_
                    << "/" << expectedWallpaperChunks_ << "(" << decoded.size() << "bytes)";

    if (receivedWallpaperChunks_ >= expectedWallpaperChunks_) {
        applyReceivedTheme();
    }
}

void CompanionListenerService::applyReceivedTheme()
{
    QString name = pendingThemeJson_["name"].toString();
    QString seed = pendingThemeJson_["seed"].toString();

    // Parse light/dark JSON objects into QMap<QString, QColor>
    // Companion sends camelCase keys — convert to hyphenated
    auto parseColorMap = [](const QJsonObject& obj) -> QMap<QString, QColor> {
        QMap<QString, QColor> map;
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            // Convert camelCase to hyphenated: insert hyphen before uppercase, lowercase all
            QString key;
            for (QChar ch : it.key()) {
                if (ch.isUpper()) {
                    key += '-';
                    key += ch.toLower();
                } else {
                    key += ch;
                }
            }
            map.insert(key, QColor(it.value().toString()));
        }
        return map;
    };

    QMap<QString, QColor> dayColors = parseColorMap(pendingThemeJson_["light"].toObject());
    QMap<QString, QColor> nightColors = parseColorMap(pendingThemeJson_["dark"].toObject());

    if (themeService_) {
        bool ok = themeService_->importCompanionTheme(name, seed, dayColors, nightColors, pendingWallpaperData_);
        qCInfo(lcCore) << "Companion: theme import" << name << (ok ? "succeeded" : "failed");
    } else {
        qCWarning(lcCore) << "Companion: no ThemeService set, cannot import theme";
    }

    // Send theme_ack
    if (client_) {
        QJsonObject ack;
        ack["type"] = "theme_ack";
        ack["accepted"] = true;
        client_->write(QJsonDocument(ack).toJson(QJsonDocument::Compact) + "\n");
        client_->flush();
    }

    // Clear pending state
    pendingWallpaperData_.clear();
    pendingThemeJson_ = QJsonObject();
    expectedWallpaperChunks_ = 0;
    receivedWallpaperChunks_ = 0;
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
    requestedProxyHost_.clear();
    requestedProxyPort_ = 0;
    proxyRouteApplied_ = false;
    pendingWallpaperData_.clear();
    pendingThemeJson_ = QJsonObject();
    expectedWallpaperChunks_ = 0;
    receivedWallpaperChunks_ = 0;
    if (systemClient_)
        systemClient_->setProxyRoute(false);

    emit connectedChanged();
    emit gpsChanged();
    emit batteryChanged();
    emit internetChanged();
}

} // namespace oap
