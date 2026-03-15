#include "core/system/HostapdConfig.hpp"

#include <QFile>

#include "core/YamlConfig.hpp"

namespace oap {

std::optional<HostapdWifiCredentials> parseHostapdWifiCredentials(const QByteArray& configText)
{
    HostapdWifiCredentials credentials;

    const QList<QByteArray> lines = configText.split('\n');
    for (const QByteArray& rawLine : lines) {
        const QByteArray line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        if (line.startsWith("ssid=")) {
            credentials.ssid = QString::fromUtf8(line.mid(5));
        } else if (line.startsWith("wpa_passphrase=")) {
            credentials.passphrase = QString::fromUtf8(line.mid(15));
        }
    }

    if (credentials.ssid.isEmpty() || credentials.passphrase.isEmpty())
        return std::nullopt;

    return credentials;
}

std::optional<HostapdWifiCredentials> loadHostapdWifiCredentials(const QString& filePath)
{
    QFile hostapdFile(filePath);
    if (!hostapdFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return std::nullopt;

    return parseHostapdWifiCredentials(hostapdFile.readAll());
}

bool syncWifiCredentials(YamlConfig& config, const HostapdWifiCredentials& credentials)
{
    bool changed = false;

    if (!credentials.ssid.isEmpty() && credentials.ssid != config.wifiSsid()) {
        config.setWifiSsid(credentials.ssid);
        changed = true;
    }

    if (!credentials.passphrase.isEmpty() && credentials.passphrase != config.wifiPassword()) {
        config.setWifiPassword(credentials.passphrase);
        changed = true;
    }

    return changed;
}

bool syncWifiCredentialsFromHostapd(YamlConfig& config, const QByteArray& hostapdConfigText)
{
    auto credentials = parseHostapdWifiCredentials(hostapdConfigText);
    if (!credentials.has_value())
        return false;

    return syncWifiCredentials(config, *credentials);
}

} // namespace oap
