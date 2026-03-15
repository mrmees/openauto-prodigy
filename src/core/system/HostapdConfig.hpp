#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace oap {

class YamlConfig;

struct HostapdWifiCredentials {
    QString ssid;
    QString passphrase;
};

std::optional<HostapdWifiCredentials> parseHostapdWifiCredentials(const QByteArray& configText);
std::optional<HostapdWifiCredentials> loadHostapdWifiCredentials(const QString& filePath);
bool syncWifiCredentials(YamlConfig& config, const HostapdWifiCredentials& credentials);
bool syncWifiCredentialsFromHostapd(YamlConfig& config, const QByteArray& hostapdConfigText);

} // namespace oap
