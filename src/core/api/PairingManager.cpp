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

} // namespace

PairingManager::PairingManager(PairedClientStore* store, QObject* parent)
    : QObject(parent)
    , store_(store)
    , timer_(new QTimer(this)) {
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, &PairingManager::cancelWindow);
}

void PairingManager::startWindow(int timeoutSeconds) {
    int pin = QRandomGenerator::system()->bounded(100000, 999999);
    pin_ = QString::number(pin);
    salt_ = randomBytes(16);
    windowOpen_ = true;

    timer_->stop();
    timer_->start(timeoutSeconds * 1000);

    emit windowChanged();
}

void PairingManager::cancelWindow() {
    timer_->stop();
    windowOpen_ = false;
    pin_.clear();
    salt_.clear();
    emit windowChanged();
}

bool PairingManager::windowOpen() const {
    return windowOpen_;
}

QString PairingManager::currentPin() const {
    return pin_;
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

    QByteArray secret = deriveSecret(pin_, salt_);
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

    store_->upsert(client);
    if (!store_->save()) {
        // Persistence failed: don't hand out a "durable" credential that
        // vanishes on restart. Undo the in-memory upsert and leave the
        // pairing window OPEN, same as a wrong-proof attempt — the user can
        // just retry the PIN.
        store_->remove(clientId);
        qWarning() << "API: pairing succeeded but persisting client failed; rejecting"
                   << clientId;
        return std::nullopt;
    }
    cancelWindow();

    return clientId;
}

} // namespace oap::api
