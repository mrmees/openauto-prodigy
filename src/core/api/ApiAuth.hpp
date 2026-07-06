#pragma once

#include <QString>
#include <QByteArray>
#include <QList>
#include <optional>

namespace oap::api {

// Crypto primitives
QByteArray deriveSecret(const QString& pin, const QByteArray& salt);
QByteArray hmacProof(const QByteArray& secret, const QByteArray& nonce);
bool constantTimeEquals(const QByteArray& a, const QByteArray& b);

// Paired client data
struct PairedClient {
    QString clientId;
    QByteArray secret;    // 32 raw bytes
    QString name;
    int kind = 0;         // pb::ClientKind numeric value
    QString pairedAtIso;  // ISO8601
};

// Persisted store for paired clients
class PairedClientStore {
public:
    explicit PairedClientStore(const QString& yamlPath);
    bool load();                       // missing file = ok, empty store
    bool save();                       // writes file with 0600 perms; false on open failure or short write
    std::optional<PairedClient> find(const QString& clientId) const;
    void upsert(const PairedClient& c);
    bool remove(const QString& clientId);
    QList<PairedClient> all() const;

private:
    QString path_;
    QList<PairedClient> clients_;
};

} // namespace oap::api
