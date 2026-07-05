#include "core/api/ApiAuth.hpp"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QFile>
#include <QSaveFile>
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace oap::api {

QByteArray deriveSecret(const QString& pin, const QByteArray& salt) {
    return QCryptographicHash::hash(pin.toUtf8() + salt, QCryptographicHash::Sha256);
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
        return true;  // missing file is OK
    }

    try {
        YAML::Node doc = YAML::LoadFile(path_.toStdString());
        if (!doc["clients"]) {
            return true;  // no clients key
        }

        clients_.clear();
        for (const auto& node : doc["clients"]) {
            PairedClient c;
            c.clientId = QString::fromStdString(node["id"].as<std::string>());

            // secret_hex is stored as hex string, convert to raw bytes
            std::string hexSecret = node["secret_hex"].as<std::string>();
            c.secret = QByteArray::fromHex(QByteArray::fromStdString(hexSecret));

            c.name = QString::fromStdString(node["name"].as<std::string>());
            c.kind = node["kind"].as<int>();
            c.pairedAtIso = QString::fromStdString(node["paired_at"].as<std::string>());

            clients_.append(c);
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void PairedClientStore::save() {
    YAML::Node doc;
    YAML::Node clientsNode;

    for (const auto& c : clients_) {
        YAML::Node clientNode;
        clientNode["id"] = c.clientId.toStdString();
        clientNode["secret_hex"] = c.secret.toHex().toStdString();
        clientNode["name"] = c.name.toStdString();
        clientNode["kind"] = c.kind;
        clientNode["paired_at"] = c.pairedAtIso.toStdString();
        clientsNode.push_back(clientNode);
    }

    doc["clients"] = clientsNode;

    // Emit to string and write via QFile
    std::string yamlContent = YAML::Dump(doc);

    QFile file(path_);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(yamlContent.c_str());
        file.close();

        // Set permissions to 0600 (owner read/write only)
        QFile::setPermissions(path_, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
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
