#include <oaa/Messenger/Cryptor.hpp>

#include <algorithm>
#include <limits>
#include <memory>

namespace oaa {

namespace {

constexpr int TLS_RECORD_HEADER_SIZE = 5;

bool containsOnlyCompleteTlsRecords(const QByteArray& ciphertext)
{
    int offset = 0;
    while (offset < ciphertext.size()) {
        if (ciphertext.size() - offset < TLS_RECORD_HEADER_SIZE)
            return false;

        const auto* header = reinterpret_cast<const unsigned char*>(
            ciphertext.constData() + offset);
        const int recordLength = (static_cast<int>(header[3]) << 8)
            | static_cast<int>(header[4]);
        const int remaining = ciphertext.size() - offset - TLS_RECORD_HEADER_SIZE;
        if (recordLength > remaining)
            return false;

        offset += TLS_RECORD_HEADER_SIZE + recordLength;
    }
    return offset == ciphertext.size();
}

} // namespace

Cryptor::~Cryptor()
{
    deinit();
}

bool Cryptor::init(Role role)
{
    return init(role,
                QByteArray(s_certificate.data(),
                           static_cast<int>(s_certificate.size())),
                QByteArray(s_privateKey.data(),
                           static_cast<int>(s_privateKey.size())));
}

bool Cryptor::init(Role role, const QByteArray& certificatePem,
                   const QByteArray& privateKeyPem)
{
    deinit();
    m_lastError.clear();

    using CtxPtr = std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)>;
    using SslPtr = std::unique_ptr<SSL, decltype(&SSL_free)>;
    using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;
    using CertPtr = std::unique_ptr<X509, decltype(&X509_free)>;
    using KeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;

    const SSL_METHOD* method = (role == Role::Client)
        ? TLS_client_method()
        : TLS_server_method();

    ERR_clear_error();
    CtxPtr ctx(SSL_CTX_new(method), SSL_CTX_free);
    if (!ctx)
        return failInitialization(QStringLiteral("SSL_CTX_new failed"));

    ERR_clear_error();
    BioPtr certBio(BIO_new_mem_buf(certificatePem.constData(), certificatePem.size()),
                   BIO_free);
    if (!certBio)
        return failInitialization(QStringLiteral("certificate BIO creation failed"));
    CertPtr cert(PEM_read_bio_X509(certBio.get(), nullptr, nullptr, nullptr),
                 X509_free);
    if (!cert)
        return failInitialization(QStringLiteral("certificate PEM parse failed"));
    ERR_clear_error();
    if (SSL_CTX_use_certificate(ctx.get(), cert.get()) != 1)
        return failInitialization(QStringLiteral("SSL_CTX_use_certificate failed"));

    ERR_clear_error();
    BioPtr keyBio(BIO_new_mem_buf(privateKeyPem.constData(), privateKeyPem.size()),
                  BIO_free);
    if (!keyBio)
        return failInitialization(QStringLiteral("private-key BIO creation failed"));
    KeyPtr key(PEM_read_bio_PrivateKey(keyBio.get(), nullptr, nullptr, nullptr),
               EVP_PKEY_free);
    if (!key)
        return failInitialization(QStringLiteral("private-key PEM parse failed"));
    ERR_clear_error();
    if (SSL_CTX_use_PrivateKey(ctx.get(), key.get()) != 1)
        return failInitialization(QStringLiteral("SSL_CTX_use_PrivateKey failed"));
    ERR_clear_error();
    if (SSL_CTX_check_private_key(ctx.get()) != 1)
        return failInitialization(QStringLiteral("certificate/private-key mismatch"));

    SSL_CTX_set_verify(ctx.get(), SSL_VERIFY_NONE, nullptr);

    ERR_clear_error();
    SslPtr ssl(SSL_new(ctx.get()), SSL_free);
    if (!ssl)
        return failInitialization(QStringLiteral("SSL_new failed"));

    ERR_clear_error();
    BioPtr readBio(BIO_new(BIO_s_mem()), BIO_free);
    if (!readBio)
        return failInitialization(QStringLiteral("read BIO creation failed"));
    BioPtr writeBio(BIO_new(BIO_s_mem()), BIO_free);
    if (!writeBio)
        return failInitialization(QStringLiteral("write BIO creation failed"));
    BIO_set_mem_eof_return(readBio.get(), -1);
    BIO_set_mem_eof_return(writeBio.get(), -1);
    BIO_set_write_buf_size(readBio.get(), BIO_BUFFER_SIZE);
    BIO_set_write_buf_size(writeBio.get(), BIO_BUFFER_SIZE);

    // SSL takes ownership of BIOs
    SSL_set_bio(ssl.get(), readBio.get(), writeBio.get());
    m_readBio = readBio.release();
    m_writeBio = writeBio.release();

    if (role == Role::Client) {
        SSL_set_connect_state(ssl.get());
    } else {
        SSL_set_accept_state(ssl.get());
    }

    m_ctx = ctx.release();
    m_ssl = ssl.release();
    m_active = false;
    m_lastError.clear();
    return true;
}

void Cryptor::deinit()
{
    if (m_ssl) {
        SSL_free(m_ssl); // also frees the BIOs
        m_ssl = nullptr;
        m_readBio = nullptr;
        m_writeBio = nullptr;
    }
    if (m_ctx) {
        SSL_CTX_free(m_ctx);
        m_ctx = nullptr;
    }
    m_active = false;
    m_lastError.clear();
}

Cryptor::HandshakeResult Cryptor::doHandshake()
{
    if (m_active)
        return HandshakeResult::Complete;
    if (!m_ssl) {
        m_lastError = QStringLiteral("TLS handshake is not initialized");
        return HandshakeResult::Failed;
    }

    // SSL_get_error() requires the current thread's error queue to be empty
    // before the I/O operation it classifies.
    ERR_clear_error();
    int ret = SSL_do_handshake(m_ssl);
    if (ret == 1) {
        m_active = true;
        m_lastError.clear();
        return HandshakeResult::Complete;
    }

    int err = SSL_get_error(m_ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        return HandshakeResult::WantIo;
    }

    m_lastError = buildError(QStringLiteral("SSL_do_handshake failed"), err);
    return HandshakeResult::Failed;
}

QString Cryptor::lastHandshakeError() const
{
    return m_lastError;
}

QString Cryptor::lastError() const
{
    return m_lastError;
}

Cryptor::DataResult Cryptor::readHandshakeBuffer()
{
    return readWriteBio(QStringLiteral("handshake output BIO read failed"));
}

bool Cryptor::writeHandshakeBuffer(const QByteArray& data)
{
    if (!m_readBio) {
        m_lastError = QStringLiteral("handshake input BIO is not initialized");
        return false;
    }
    if (data.isEmpty()) {
        m_lastError.clear();
        return true;
    }

    int written = 0;
    while (written < data.size()) {
        const int remaining = data.size() - written;
        ERR_clear_error();
        const int result = BIO_write(m_readBio, data.constData() + written, remaining);
        if (result <= 0) {
            m_lastError = buildError(QStringLiteral("handshake input BIO write failed"));
            return false;
        }
        written += result;
    }
    m_lastError.clear();
    return true;
}

Cryptor::DataResult Cryptor::encrypt(const QByteArray& plaintext)
{
    if (!m_active || !m_ssl)
        return failData(QStringLiteral("TLS encryption is not active"));
    if (plaintext.size() > FRAME_MAX_PAYLOAD)
        return failData(QStringLiteral("TLS plaintext exceeds one AA frame"));
    if (plaintext.isEmpty()) {
        m_lastError.clear();
        return {DataResult::Status::Complete, {}, {}};
    }

    int written = 0;
    while (written < plaintext.size()) {
        const int remaining = plaintext.size() - written;
        ERR_clear_error();
        const int result = SSL_write(m_ssl, plaintext.constData() + written, remaining);
        if (result <= 0) {
            const int error = SSL_get_error(m_ssl, result);
            return failData(QStringLiteral("SSL_write failed"), error);
        }
        written += result;
    }

    auto output = readWriteBio(QStringLiteral("encrypted output BIO read failed"));
    if (!output.isComplete())
        return output;
    if (output.data.isEmpty())
        return failData(QStringLiteral("SSL_write produced no encrypted bytes"));
    return output;
}

Cryptor::DataResult Cryptor::decrypt(const QByteArray& ciphertext, int frameLength)
{
    if (!m_active || !m_ssl)
        return failData(QStringLiteral("TLS decryption is not active"));
    if (!m_readBio)
        return failData(QStringLiteral("TLS input BIO is not initialized"));
    if (ciphertext.isEmpty())
        return failData(QStringLiteral("encrypted AA frame is empty"));
    if (frameLength != ciphertext.size())
        return failData(QStringLiteral("encrypted AA frame length mismatch"));
    if (!containsOnlyCompleteTlsRecords(ciphertext))
        return failData(QStringLiteral("incomplete TLS record in encrypted AA frame"));

    int written = 0;
    while (written < ciphertext.size()) {
        const int remaining = ciphertext.size() - written;
        ERR_clear_error();
        const int result = BIO_write(m_readBio, ciphertext.constData() + written,
                                     remaining);
        if (result <= 0)
            return failData(QStringLiteral("encrypted input BIO write failed"));
        written += result;
    }

    int estimatedSize = frameLength - TLS_OVERHEAD;
    if (estimatedSize <= 0) estimatedSize = 2048;

    QByteArray plaintext;
    plaintext.reserve(estimatedSize);

    char chunk[2048];
    while (true) {
        ERR_clear_error();
        const int read = SSL_read(m_ssl, chunk, sizeof(chunk));
        if (read > 0) {
            plaintext.append(chunk, read);
            continue;
        }

        const int error = SSL_get_error(m_ssl, read);
        if ((error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE)
            && !plaintext.isEmpty()) {
            break;
        }
        return failData(
            error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE
                ? QStringLiteral("TLS record produced no application data")
                : QStringLiteral("SSL_read failed"),
            error);
    }

    m_lastError.clear();
    return {DataResult::Status::Complete, std::move(plaintext), {}};
}

bool Cryptor::isActive() const
{
    return m_active;
}

Cryptor::DataResult Cryptor::readWriteBio(const QString& context)
{
    if (!m_writeBio)
        return failData(context + QStringLiteral(": BIO is not initialized"));

    const long pending = BIO_ctrl_pending(m_writeBio);
    if (pending <= 0) {
        m_lastError.clear();
        return {DataResult::Status::Complete, {}, {}};
    }
    if (pending > std::numeric_limits<int>::max())
        return failData(context + QStringLiteral(": pending data is too large"));

    QByteArray output;
    output.resize(static_cast<int>(pending));
    int offset = 0;
    while (offset < output.size()) {
        ERR_clear_error();
        const int read = BIO_read(m_writeBio, output.data() + offset,
                                  output.size() - offset);
        if (read <= 0)
            return failData(context);
        offset += read;
    }
    m_lastError.clear();
    return {DataResult::Status::Complete, std::move(output), {}};
}

Cryptor::DataResult Cryptor::failData(const QString& context, int sslError)
{
    m_lastError = buildError(context, sslError);
    return {DataResult::Status::Failed, {}, m_lastError};
}

bool Cryptor::failInitialization(const QString& context, int sslError)
{
    const QString error = buildError(context, sslError);
    deinit();
    m_lastError = error;
    return false;
}

QString Cryptor::buildError(const QString& context, int sslError)
{
    QString error = context;
    if (sslError >= 0)
        error += QStringLiteral(" (SSL_get_error=%1)").arg(sslError);

    int count = 0;
    while (count < 4 && error.size() < 1024) {
        const unsigned long code = ERR_get_error();
        if (code == 0)
            break;
        char errorText[256] = {};
        ERR_error_string_n(code, errorText, sizeof(errorText));
        error += QStringLiteral(": ") + QString::fromLatin1(errorText);
        ++count;
    }
    return error.left(1024);
}

const std::string Cryptor::s_certificate = "-----BEGIN CERTIFICATE-----\n\
MIIDKjCCAhICARswDQYJKoZIhvcNAQELBQAwWzELMAkGA1UEBhMCVVMxEzARBgNV\n\
BAgMCkNhbGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxHzAdBgNVBAoM\n\
Fkdvb2dsZSBBdXRvbW90aXZlIExpbmswJhcRMTQwNzA0MDAwMDAwLTA3MDAXETQ1\n\
MDQyOTE0MjgzOC0wNzAwMFMxCzAJBgNVBAYTAkpQMQ4wDAYDVQQIDAVUb2t5bzER\n\
MA8GA1UEBwwISGFjaGlvamkxFDASBgNVBAoMC0pWQyBLZW53b29kMQswCQYDVQQL\n\
DAIwMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAM911mNnUfx+WJtx\n\
uk06GO7kXRW/gXUVNQBkbAFZmVdVNvLoEQNthi2X8WCOwX6n6oMPxU2MGJnvicP3\n\
6kBqfHhfQ2Fvqlf7YjjhgBHh0lqKShVPxIvdatBjVQ76aym5H3GpkigLGkmeyiVo\n\
VO8oc3cJ1bO96wFRmk7kJbYcEjQyakODPDu4QgWUTwp1Z8Dn41ARMG5OFh6otITL\n\
XBzj9REkUPkxfS03dBXGr5/LIqvSsnxib1hJ47xnYJXROUsBy3e6T+fYZEEzZa7y\n\
7tFioHIQ8G/TziPmvFzmQpaWMGiYfoIgX8WoR3GD1diYW+wBaZTW+4SFUZJmRKgq\n\
TbMNFkMCAwEAATANBgkqhkiG9w0BAQsFAAOCAQEAsGdH5VFn78WsBElMXaMziqFC\n\
zmilkvr85/QpGCIztI0FdF6xyMBJk/gYs2thwvF+tCCpXoO8mjgJuvJZlwr6fHzK\n\
Ox5hNUb06AeMtsUzUfFjSZXKrSR+XmclVd+Z6/ie33VhGePOPTKYmJ/PPfTT9wvT\n\
93qswcxhA+oX5yqLbU3uDPF1ZnJaEeD/YN45K/4eEA4/0SDXaWW14OScdS2LV0Bc\n\
YmsbkPVNYZn37FlY7e2Z4FUphh0A7yME2Eh/e57QxWrJ1wubdzGnX8mrABc67ADU\n\
U5r9tlTRqMs7FGOk6QS2Cxp4pqeVQsrPts4OEwyPUyb3LfFNo3+sP111D9zEow==\n\
-----END CERTIFICATE-----\n";

const std::string Cryptor::s_privateKey = "-----BEGIN RSA PRIVATE KEY-----\n\
MIIEowIBAAKCAQEAz3XWY2dR/H5Ym3G6TToY7uRdFb+BdRU1AGRsAVmZV1U28ugR\n\
A22GLZfxYI7Bfqfqgw/FTYwYme+Jw/fqQGp8eF9DYW+qV/tiOOGAEeHSWopKFU/E\n\
i91q0GNVDvprKbkfcamSKAsaSZ7KJWhU7yhzdwnVs73rAVGaTuQlthwSNDJqQ4M8\n\
O7hCBZRPCnVnwOfjUBEwbk4WHqi0hMtcHOP1ESRQ+TF9LTd0Fcavn8siq9KyfGJv\n\
WEnjvGdgldE5SwHLd7pP59hkQTNlrvLu0WKgchDwb9POI+a8XOZClpYwaJh+giBf\n\
xahHcYPV2Jhb7AFplNb7hIVRkmZEqCpNsw0WQwIDAQABAoIBAB2u7ZLheKCY71Km\n\
bhKYqnKb6BmxgfNfqmq4858p07/kKG2O+Mg1xooFgHrhUhwuKGbCPee/kNGNrXeF\n\
pFW9JrwOXVS2pnfaNw6ObUWhuvhLaxgrhqLAdoUEgWoYOHcKzs3zhj8Gf6di+edq\n\
SyTA8+xnUtVZ6iMRKvP4vtCUqaIgBnXdmQbGINP+/4Qhb5R7XzMt/xPe6uMyAIyC\n\
y5Fm9HnvekaepaeFEf3bh4NV1iN/R8px6cFc6ELYxIZc/4Xbm91WGqSdB0iSriaZ\n\
TjgrmaFjSO40tkCaxI9N6DGzJpmpnMn07ifhl2VjnGOYwtyuh6MKEnyLqTrTg9x0\n\
i3mMwskCgYEA9IyljPRerXxHUAJt+cKOayuXyNt80q9PIcGbyRNvn7qIY6tr5ut+\n\
ZbaFgfgHdSJ/4nICRq02HpeDJ8oj9BmhTAhcX6c1irH5ICjRlt40qbPwemIcpybt\n\
mb+DoNYbI8O4dUNGH9IPfGK8dRpOok2m+ftfk94GmykWbZF5CnOKIp8CgYEA2Syc\n\
5xlKB5Qk2ZkwXIzxbzozSfunHhWWdg4lAbyInwa6Y5GB35UNdNWI8TAKZsN2fKvX\n\
RFgCjbPreUbREJaM3oZ92o5X4nFxgjvAE1tyRqcPVbdKbYZgtcqqJX06sW/g3r/3\n\
RH0XPj2SgJIHew9sMzjGWDViMHXLmntI8rVA7d0CgYBOr36JFwvrqERN0ypNpbMr\n\
epBRGYZVSAEfLGuSzEUrUNqXr019tKIr2gmlIwhLQTmCxApFcXArcbbKs7jTzvde\n\
PoZyZJvOr6soFNozP/YT8Ijc5/quMdFbmgqhUqLS5CPS3z2N+YnwDNj0mO1aPcAP\n\
STmcm2DmxdaolJksqrZ0owKBgQCD0KJDWoQmaXKcaHCEHEAGhMrQot/iULQMX7Vy\n\
gl5iN5E2EgFEFZIfUeRWkBQgH49xSFPWdZzHKWdJKwSGDvrdrcABwdfx520/4MhK\n\
d3y7CXczTZbtN1zHuoTfUE0pmYBhcx7AATT0YCblxrynosrHpDQvIefBBh5YW3AB\n\
cKZCOQKBgEM/ixzI/OVSZ0Py2g+XV8+uGQyC5XjQ6cxkVTX3Gs0ZXbemgUOnX8co\n\
eCXS4VrhEf4/HYMWP7GB5MFUOEVtlLiLM05ruUL7CrphdfgayDXVcTPfk75lLhmu\n\
KAwp3tIHPoJOQiKNQ3/qks5km/9dujUGU2ARiU3qmxLMdgegFz8e\n\
-----END RSA PRIVATE KEY-----\n";

} // namespace oaa
