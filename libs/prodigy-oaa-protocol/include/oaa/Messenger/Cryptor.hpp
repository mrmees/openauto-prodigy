#pragma once

#include <oaa/Version.hpp>

#include <QByteArray>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>

#include <string>

namespace oaa {

class Cryptor {
public:
    enum class Role { Client, Server };

    Cryptor() = default;
    ~Cryptor();

    Cryptor(const Cryptor&) = delete;
    Cryptor& operator=(const Cryptor&) = delete;

    void init(Role role);
    void deinit();

    bool doHandshake();
    QByteArray readHandshakeBuffer();
    void writeHandshakeBuffer(const QByteArray& data);

    QByteArray encrypt(const QByteArray& plaintext);
    QByteArray decrypt(const QByteArray& ciphertext, int frameLength);

    bool isActive() const;

private:
    SSL_CTX* m_ctx = nullptr;
    SSL* m_ssl = nullptr;
    BIO* m_readBio = nullptr;  // incoming data (we write to it, SSL reads from it)
    BIO* m_writeBio = nullptr; // outgoing data (SSL writes to it, we read from it)
    X509* m_cert = nullptr;
    EVP_PKEY* m_key = nullptr;
    bool m_active = false;

    static const std::string s_certificate;
    static const std::string s_privateKey;
};

} // namespace oaa
