#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <optional>

#include "core/api/ApiAuth.hpp"

class QTimer;

namespace oap::api {

// Manages a single active PIN-pairing window: generates a fresh PIN + salt
// per window, verifies a client's HMAC proof against it, and persists newly
// paired clients to the PairedClientStore on success.
class PairingManager : public QObject {
    Q_OBJECT
public:
    explicit PairingManager(PairedClientStore* store, QObject* parent = nullptr);

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

private:
    PairedClientStore* store_;
    QTimer* timer_;
    bool windowOpen_ = false;
    QString pin_;
    QByteArray salt_;
};

} // namespace oap::api
