#include "core/api/ApiAuth.hpp"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QFile>
#include <QDebug>
#include <yaml-cpp/yaml.h>

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
        QList<PairedClient> loaded;
        if (!doc["clients"]) {
            clients_.clear();
            loadedSuccessfully_ = true;
            return true;  // no clients key
        }

        for (const auto& node : doc["clients"]) {
            PairedClient c;
            c.clientId = QString::fromStdString(node["id"].as<std::string>());

            // secret_hex is stored as hex string, convert to raw bytes
            std::string hexSecret = node["secret_hex"].as<std::string>();
            c.secret = QByteArray::fromHex(QByteArray::fromStdString(hexSecret));

            c.name = QString::fromStdString(node["name"].as<std::string>());
            c.kind = node["kind"].as<int>();
            c.pairedAtIso = QString::fromStdString(node["paired_at"].as<std::string>());
            c.credentialGeneration = node["credential_generation"]
                ? node["credential_generation"].as<int>()
                : kLegacyCredentialGeneration;

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
    YAML::Node clientsNode;

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

    // Emit to string and write via QFile
    std::string yamlContent = YAML::Dump(doc);

    QFile file(path_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "API: failed to open paired-client store for writing:" << path_;
        return false;
    }

    const qint64 written = file.write(yamlContent.c_str());
    file.close();
    if (written < 0 || static_cast<size_t>(written) != yamlContent.size()) {
        qWarning() << "API: short write persisting paired-client store:" << path_;
        return false;
    }

    // Set permissions to 0600 (owner read/write only)
    QFile::setPermissions(path_, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
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
