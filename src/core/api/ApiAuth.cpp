#include "core/api/ApiAuth.hpp"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QFile>
#include <QSaveFile>
#include <QDebug>
#include <QSet>
#include <yaml-cpp/yaml.h>

#include <algorithm>

namespace oap::api {

QByteArray deriveSecret(const QString& pairingCode, const QByteArray& salt) {
    return QCryptographicHash::hash(pairingCode.toUtf8() + salt, QCryptographicHash::Sha256);
}

QByteArray hmacProof(const QByteArray& secret, const QByteArray& nonce) {
    return QMessageAuthenticationCode::hash(nonce, secret, QCryptographicHash::Sha256);
}

bool constantTimeEquals(const QByteArray& a, const QByteArray& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    return diff == 0;
}

namespace {

bool isValidSecretHex(const std::string& value) {
    if (value.size() != 64)
        return false;

    return std::all_of(value.cbegin(), value.cend(), [](char c) {
        return (c >= '0' && c <= '9')
            || (c >= 'a' && c <= 'f')
            || (c >= 'A' && c <= 'F');
    });
}

bool isKnownCredentialGeneration(int generation) {
    return generation == kLegacyCredentialGeneration
        || generation == kSecureCodeCredentialGeneration;
}

} // namespace

// PairedClientStore implementation

PairedClientStore::PairedClientStore(const QString& yamlPath)
    : path_(yamlPath) {
}

bool PairedClientStore::load() {
    if (!QFile::exists(path_)) {
        clients_.clear();
        loadedSuccessfully_ = true;
        return true;  // missing file is OK
    }

    try {
        YAML::Node doc = YAML::LoadFile(path_.toStdString());
        if (!doc || !doc.IsMap()) {
            loadedSuccessfully_ = false;
            return false;
        }

        const YAML::Node clientsNode = doc["clients"];
        if (!clientsNode) {
            loadedSuccessfully_ = false;
            return false;
        }
        if (clientsNode.IsNull()) {
            // Compatibility with stores written by the old empty-list saver,
            // which emitted `clients: ~` instead of an empty sequence.
            clients_.clear();
            loadedSuccessfully_ = true;
            return true;
        }
        if (!clientsNode.IsSequence()) {
            loadedSuccessfully_ = false;
            return false;
        }

        QList<PairedClient> loaded;
        QSet<QString> clientIds;

        for (const auto& node : clientsNode) {
            if (!node.IsMap()
                || !node["id"] || !node["id"].IsScalar()
                || !node["secret_hex"] || !node["secret_hex"].IsScalar()
                || !node["name"] || !node["name"].IsScalar()
                || !node["kind"] || !node["kind"].IsScalar()
                || !node["paired_at"] || !node["paired_at"].IsScalar()) {
                loadedSuccessfully_ = false;
                return false;
            }

            PairedClient c;
            c.clientId = QString::fromStdString(node["id"].as<std::string>());
            if (c.clientId.trimmed().isEmpty() || clientIds.contains(c.clientId)) {
                loadedSuccessfully_ = false;
                return false;
            }

            // secret_hex is stored as hex string, convert to raw bytes
            std::string hexSecret = node["secret_hex"].as<std::string>();
            if (!isValidSecretHex(hexSecret)) {
                loadedSuccessfully_ = false;
                return false;
            }
            c.secret = QByteArray::fromHex(QByteArray::fromStdString(hexSecret));
            if (c.secret.size() != 32) {
                loadedSuccessfully_ = false;
                return false;
            }

            c.name = QString::fromStdString(node["name"].as<std::string>());
            c.kind = node["kind"].as<int>();
            c.pairedAtIso = QString::fromStdString(node["paired_at"].as<std::string>());
            const YAML::Node generationNode = node["credential_generation"];
            if (generationNode) {
                if (!generationNode.IsScalar()) {
                    loadedSuccessfully_ = false;
                    return false;
                }
                c.credentialGeneration = generationNode.as<int>();
            } else {
                c.credentialGeneration = kLegacyCredentialGeneration;
            }
            if (!isKnownCredentialGeneration(c.credentialGeneration)) {
                loadedSuccessfully_ = false;
                return false;
            }

            clientIds.insert(c.clientId);
            loaded.append(c);
        }
        clients_ = std::move(loaded);
        loadedSuccessfully_ = true;
        return true;
    } catch (const std::exception&) {
        loadedSuccessfully_ = false;
        return false;
    }
}

bool PairedClientStore::save() {
    if (!loadedSuccessfully_) {
        qWarning() << "API: refusing to overwrite paired-client store after load failure:"
                   << path_;
        return false;
    }

    YAML::Node doc;
    YAML::Node clientsNode(YAML::NodeType::Sequence);

    for (const auto& c : clients_) {
        YAML::Node clientNode;
        clientNode["id"] = c.clientId.toStdString();
        clientNode["secret_hex"] = c.secret.toHex().toStdString();
        clientNode["name"] = c.name.toStdString();
        clientNode["kind"] = c.kind;
        clientNode["paired_at"] = c.pairedAtIso.toStdString();
        clientNode["credential_generation"] = c.credentialGeneration;
        clientsNode.push_back(clientNode);
    }

    doc["clients"] = clientsNode;

    const QByteArray yamlContent = QByteArray::fromStdString(YAML::Dump(doc));
    const QFileDevice::Permissions storePermissions =
        QFileDevice::ReadOwner | QFileDevice::WriteOwner;
    const QFileDevice::Permissions permissionMask =
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
        | QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
        | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;

    QSaveFile file(path_);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "API: failed to open paired-client store for writing:" << path_;
        return false;
    }

    const qint64 written = file.write(yamlContent);
    if (written != yamlContent.size()) {
        file.cancelWriting();
        qWarning() << "API: short write persisting paired-client store:" << path_;
        return false;
    }

    if (!file.setPermissions(storePermissions)
        || (file.permissions() & permissionMask) != storePermissions) {
        file.cancelWriting();
        qWarning() << "API: failed to secure paired-client store permissions:" << path_;
        return false;
    }

    if (!file.commit()) {
        qWarning() << "API: failed to atomically commit paired-client store:" << path_;
        return false;
    }
    return true;
}

std::optional<PairedClient> PairedClientStore::find(const QString& clientId) const {
    for (const auto& c : clients_) {
        if (c.clientId == clientId) {
            return c;
        }
    }
    return std::nullopt;
}

void PairedClientStore::upsert(const PairedClient& c) {
    // Remove existing if found
    for (int i = 0; i < clients_.size(); ++i) {
        if (clients_[i].clientId == c.clientId) {
            clients_.removeAt(i);
            break;
        }
    }
    clients_.append(c);
}

bool PairedClientStore::remove(const QString& clientId) {
    for (int i = 0; i < clients_.size(); ++i) {
        if (clients_[i].clientId == clientId) {
            clients_.removeAt(i);
            return true;
        }
    }
    return false;
}

QList<PairedClient> PairedClientStore::all() const {
    return clients_;
}

} // namespace oap::api
