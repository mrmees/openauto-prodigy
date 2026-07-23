#include "core/api/PairingManager.hpp"

#include <QRandomGenerator>
#include <QUuid>
#include <QDateTime>
#include <QTimer>
#include <QDebug>

namespace oap::api {

namespace {

QByteArray randomBytes(int count) {
    QByteArray bytes(count, '\0');
    quint32* data = reinterpret_cast<quint32*>(bytes.data());
    int wholeWords = count / static_cast<int>(sizeof(quint32));
    QRandomGenerator::system()->fillRange(data, wholeWords);

    int consumed = wholeWords * static_cast<int>(sizeof(quint32));
    for (int i = consumed; i < count; ++i) {
        bytes[i] = static_cast<char>(QRandomGenerator::system()->bounded(256));
    }
    return bytes;
}

QString randomPairingCode() {
    static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    QString result;
    result.reserve(24);
    for (int i = 0; i < 24; ++i)
        result.append(QLatin1Char(kAlphabet[QRandomGenerator::system()->bounded(32)]));
    return result;
}

} // namespace

PairingManager::PairingManager(PairedClientStore* store, QObject* parent)
    : QObject(parent)
    , store_(store)
    , timer_(new QTimer(this))
    , codeGenerator_(randomPairingCode) {
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, &PairingManager::cancelWindow);
}

bool PairingManager::startWindow(int timeoutSeconds) {
    if (!store_ || !store_->loadedSuccessfully()) {
        qWarning() << "API: pairing unavailable because client store did not load";
        return false;
    }

    const QString generated = codeGenerator_ ? codeGenerator_() : QString();
    if (!isValidCode(generated)) {
        qWarning() << "API: secure pairing code generator returned invalid data";
        return false;
    }
    code_ = generated;
    salt_ = randomBytes(16);
    windowOpen_ = true;

    timer_->stop();
    timer_->start(timeoutSeconds * 1000);

    emit windowChanged();
    return true;
}

void PairingManager::cancelWindow() {
    timer_->stop();
    windowOpen_ = false;
    code_.clear();
    salt_.clear();
    emit windowChanged();
}

bool PairingManager::windowOpen() const {
    return windowOpen_;
}

QString PairingManager::currentCode() const {
    return code_;
}

QString PairingManager::displayCode() const {
    return formatCode(code_);
}

bool PairingManager::isValidCode(const QString& code) {
    if (code.size() != 24)
        return false;
    for (const QChar c : code) {
        const ushort u = c.unicode();
        if (!((u >= 'A' && u <= 'Z') || (u >= '2' && u <= '7')))
            return false;
    }
    return true;
}

QString PairingManager::formatCode(const QString& code) {
    if (code.isEmpty())
        return QString();
    QString grouped;
    grouped.reserve(code.size() + 5);
    for (int i = 0; i < code.size(); ++i) {
        if (i > 0 && i % 4 == 0)
            grouped.append(QLatin1Char('-'));
        grouped.append(code.at(i));
    }
    return grouped;
}

QByteArray PairingManager::currentSalt() const {
    return salt_;
}

QByteArray PairingManager::makeNonce() const {
    return randomBytes(32);
}

std::optional<QString> PairingManager::completePairing(const QByteArray& nonce, const QByteArray& proof,
                                                         const QString& clientName, int clientKind) {
    if (!windowOpen_) {
        return std::nullopt;
    }

    QByteArray secret = deriveSecret(code_, salt_);
    QByteArray expected = hmacProof(secret, nonce);
    if (!constantTimeEquals(expected, proof)) {
        return std::nullopt;
    }

    QString clientId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    PairedClient client;
    client.clientId = clientId;
    client.secret = secret;
    client.name = clientName;
    client.kind = clientKind;
    client.pairedAtIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate) + "Z";
    client.credentialGeneration = kSecureCodeCredentialGeneration;

    store_->upsert(client);
    if (!store_->save()) {
        // Persistence failed: don't hand out a "durable" credential that
        // vanishes on restart. Undo the in-memory upsert and leave the
        // pairing window OPEN, same as a wrong-proof attempt — the user can
        // just retry the code.
        store_->remove(clientId);
        qWarning() << "API: pairing succeeded but persisting client failed; rejecting"
                   << clientId;
        return std::nullopt;
    }
    cancelWindow();

    return clientId;
}

} // namespace oap::api
