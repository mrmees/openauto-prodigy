#pragma once

#include <oaa/Session/SessionConfig.hpp>
#include <QString>

namespace oap { class YamlConfig; }

namespace oap {
namespace aa {

/// Builds a fully-populated SessionConfig with pre-serialized channel
/// descriptors for AA service discovery. Replaces the scattered fillFeatures()
/// methods from the old aasdk service stubs.
class ServiceDiscoveryBuilder {
public:
    explicit ServiceDiscoveryBuilder(oap::YamlConfig* yamlConfig = nullptr,
                                     const QString& btMacAddress = "00:00:00:00:00:00",
                                     const QString& wifiSsid = {},
                                     const QString& wifiPassword = {});

    oaa::SessionConfig build() const;

private:
    QByteArray buildVideoDescriptor() const;
    QByteArray buildMediaAudioDescriptor() const;
    QByteArray buildSpeechAudioDescriptor() const;
    QByteArray buildSystemAudioDescriptor() const;
    QByteArray buildInputDescriptor() const;
    QByteArray buildSensorDescriptor() const;
    QByteArray buildBluetoothDescriptor() const;
    QByteArray buildWifiDescriptor() const;
    QByteArray buildAVInputDescriptor() const;

    // Shared margin calculation for video/input
    void calcMargins(int remoteW, int remoteH,
                     int& marginW, int& marginH) const;

    oap::YamlConfig* yamlConfig_ = nullptr;
    QString btMacAddress_;
    QString wifiSsid_;
    QString wifiPassword_;
};

} // namespace aa
} // namespace oap
