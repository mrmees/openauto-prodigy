#pragma once

#include <oaa/Version.hpp>

#include <QByteArray>
#include <QString>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>

#include <string>

namespace oaa {

class Cryptor {
public:
    enum class Role { Client, Server };
    enum class HandshakeResult { Complete, WantIo, Failed };
    struct DataResult {
        enum class Status { Complete, Failed };

        Status status = Status::Failed;
        QByteArray data;
        QString error;

        bool isComplete() const { return status == Status::Complete; }
    };

    Cryptor() = default;
    ~Cryptor();

    Cryptor(const Cryptor&) = delete;
    Cryptor& operator=(const Cryptor&) = delete;

    bool init(Role role);
    bool init(Role role, const QByteArray& certificatePem,
              const QByteArray& privateKeyPem);
    void deinit();

    HandshakeResult doHandshake();
    QString lastHandshakeError() const;
    QString lastError() const;
    DataResult readHandshakeBuffer();
    bool writeHandshakeBuffer(const QByteArray& data);

    DataResult encrypt(const QByteArray& plaintext);
    DataResult decrypt(const QByteArray& ciphertext, int frameLength);

    bool isActive() const;

private:
    SSL_CTX* m_ctx = nullptr;
    SSL* m_ssl = nullptr;
    BIO* m_readBio = nullptr;  // incoming data (we write to it, SSL reads from it)
    BIO* m_writeBio = nullptr; // outgoing data (SSL writes to it, we read from it)
    bool m_active = false;
    QString m_lastError;

    DataResult readWriteBio(const QString& context);
    DataResult failData(const QString& context, int sslError = -1);
    bool failInitialization(const QString& context, int sslError = -1);
    static QString buildError(const QString& context, int sslError = -1);

    static const std::string s_certificate;
    static const std::string s_privateKey;
};

} // namespace oaa
