#pragma once

#include <oaa/Session/SessionConfig.hpp>
#include <QString>
#include <utility>

namespace oap { class YamlConfig; }

namespace oap {
namespace aa {

/// Builds a fully-populated SessionConfig with pre-serialized channel
/// descriptors for AA service discovery.
class ServiceDiscoveryBuilder {
public:
    explicit ServiceDiscoveryBuilder(oap::YamlConfig* yamlConfig = nullptr,
                                     const QString& btMacAddress = "00:00:00:00:00:00",
                                     const QString& wifiSsid = {},
                                     const QString& wifiPassword = {});

    oaa::SessionConfig build() const;

    /// Override display dimensions for margin calculations (detected > config fallback).
    void setDisplayDimensions(int w, int h);

    /// Compute content dimensions (video res minus margins) for a given config.
    /// Returns {contentW, contentH}. Used by both buildInputDescriptor (touch_screen_config)
    /// and EvdevTouchReader (touch output coordinate space) -- single source of truth.
    static std::pair<int,int> computeContentDimensions(
        int videoW, int videoH, int displayW, int displayH,
        bool navbarDuringAA, const QString& navbarEdge, int navbarThickness = 56);

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
    QByteArray buildNavigationDescriptor() const;
    QByteArray buildMediaStatusDescriptor() const;
    QByteArray buildPhoneStatusDescriptor() const;

    // Shared margin calculation for video/input
    void calcMargins(int remoteW, int remoteH,
                     int& marginW, int& marginH) const;

    // Shared viewport calculation (navbar-aware)
    void calcNavbarViewport(int& viewportW, int& viewportH) const;

    oap::YamlConfig* yamlConfig_ = nullptr;
    QString btMacAddress_;
    QString wifiSsid_;
    QString wifiPassword_;
    int overrideDisplayW_ = 0;  // 0 = use yamlConfig values
    int overrideDisplayH_ = 0;
};

} // namespace aa
} // namespace oap
