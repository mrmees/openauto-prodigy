#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <functional>
#include <optional>

#include "core/api/ApiAuth.hpp"

class QTimer;

namespace oap::api {

// Manages a single active secure-code pairing window: generates a fresh
// 120-bit Base32 code + salt
// per window, verifies a client's HMAC proof against it, and persists newly
// paired clients to the PairedClientStore on success.
class PairingManager : public QObject {
    Q_OBJECT
public:
    using CodeGenerator = std::function<QString()>;

    explicit PairingManager(PairedClientStore* store, QObject* parent = nullptr);

    bool startWindow(int timeoutSeconds);   // new code+salt each call; restarts timer
    void cancelWindow();
    bool windowOpen() const;
    QString currentCode() const;            // canonical; "" when closed
    QString displayCode() const;            // grouped XXXX-...; "" when closed
    QByteArray currentSalt() const;         // 16 random bytes, per-window
    QByteArray makeNonce() const;           // 32 random bytes, caller keeps it

    void setCodeGeneratorForTest(CodeGenerator generator) {
        codeGenerator_ = std::move(generator);
    }
    static bool isValidCode(const QString& code);
    static QString formatCode(const QString& code);

    // Returns new client id on success (persists to store, closes window).
    std::optional<QString> completePairing(const QByteArray& nonce, const QByteArray& proof,
                                            const QString& clientName, int clientKind);

signals:
    void windowChanged();

private:
    PairedClientStore* store_;
    QTimer* timer_;
    bool windowOpen_ = false;
    QString code_;
    QByteArray salt_;
    CodeGenerator codeGenerator_;
};

} // namespace oap::api
