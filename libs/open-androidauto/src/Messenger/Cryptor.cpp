#include <oaa/Messenger/Cryptor.hpp>

namespace oaa {

Cryptor::~Cryptor()
{
    deinit();
}

void Cryptor::init(Role role)
{
    deinit();

    const SSL_METHOD* method = (role == Role::Client)
        ? TLS_client_method()
        : TLS_server_method();

    m_ctx = SSL_CTX_new(method);

    // Load certificate from PEM string
    BIO* certBio = BIO_new_mem_buf(s_certificate.data(), static_cast<int>(s_certificate.size()));
    m_cert = PEM_read_bio_X509(certBio, nullptr, nullptr, nullptr);
    BIO_free(certBio);
    SSL_CTX_use_certificate(m_ctx, m_cert);

    // Load private key from PEM string
    BIO* keyBio = BIO_new_mem_buf(s_privateKey.data(), static_cast<int>(s_privateKey.size()));
    m_key = PEM_read_bio_PrivateKey(keyBio, nullptr, nullptr, nullptr);
    BIO_free(keyBio);
    SSL_CTX_use_PrivateKey(m_ctx, m_key);

    SSL_CTX_set_verify(m_ctx, SSL_VERIFY_NONE, nullptr);

    m_ssl = SSL_new(m_ctx);

    // Create memory BIO pair
    m_readBio = BIO_new(BIO_s_mem());
    m_writeBio = BIO_new(BIO_s_mem());
    BIO_set_mem_eof_return(m_readBio, -1);
    BIO_set_mem_eof_return(m_writeBio, -1);
    BIO_set_write_buf_size(m_readBio, BIO_BUFFER_SIZE);
    BIO_set_write_buf_size(m_writeBio, BIO_BUFFER_SIZE);

    // SSL takes ownership of BIOs
    SSL_set_bio(m_ssl, m_readBio, m_writeBio);

    if (role == Role::Client) {
        SSL_set_connect_state(m_ssl);
    } else {
        SSL_set_accept_state(m_ssl);
    }

    m_active = false;
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
    if (m_cert) {
        X509_free(m_cert);
        m_cert = nullptr;
    }
    if (m_key) {
        EVP_PKEY_free(m_key);
        m_key = nullptr;
    }
    m_active = false;
}

bool Cryptor::doHandshake()
{
    if (m_active) return true;

    int ret = SSL_do_handshake(m_ssl);
    if (ret == 1) {
        m_active = true;
        return true;
    }

    int err = SSL_get_error(m_ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        return false;
    }

    return false;
}

QByteArray Cryptor::readHandshakeBuffer()
{
    int pending = BIO_ctrl_pending(m_writeBio);
    if (pending <= 0) return {};

    QByteArray result(pending, Qt::Uninitialized);
    int read = BIO_read(m_writeBio, result.data(), pending);
    if (read <= 0) return {};

    result.resize(read);
    return result;
}

void Cryptor::writeHandshakeBuffer(const QByteArray& data)
{
    BIO_write(m_readBio, data.constData(), data.size());
}

QByteArray Cryptor::encrypt(const QByteArray& plaintext)
{
    SSL_write(m_ssl, plaintext.constData(), plaintext.size());

    int pending = BIO_ctrl_pending(m_writeBio);
    if (pending <= 0) return {};

    QByteArray result(pending, Qt::Uninitialized);
    int read = BIO_read(m_writeBio, result.data(), pending);
    if (read <= 0) return {};

    result.resize(read);
    return result;
}

QByteArray Cryptor::decrypt(const QByteArray& ciphertext, int frameLength)
{
    BIO_write(m_readBio, ciphertext.constData(), ciphertext.size());

    int estimatedSize = frameLength - TLS_OVERHEAD;
    if (estimatedSize <= 0) estimatedSize = 2048;

    QByteArray result;
    result.reserve(estimatedSize);

    char chunk[2048];
    while (true) {
        int read = SSL_read(m_ssl, chunk, sizeof(chunk));
        if (read <= 0) break;
        result.append(chunk, read);
    }

    return result;
}

bool Cryptor::isActive() const
{
    return m_active;
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
