#pragma once

#include <oaa/Session/SessionConfig.hpp>
#include <QString>
#include <QStringList>
#include <cstdint>
#include <utility>

#include "ProjectedDisplayConfig.hpp"

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
                                     const QString& wifiPassword = {},
                                     const QString& wifiBssid = {});

    oaa::SessionConfig build() const;

    /// Number of video configs produced by build() from the same codec set.
    uint32_t videoConfigCount() const;
    uint32_t videoConfigCount(ProjectedDisplayRole role) const;

    void setProtocolVersion(oaa::ProtocolVersion version);
    void setProjectedClusterConfig(const ProjectedClusterConfig& config);

    /// Override display dimensions for margin calculations (detected > config fallback).
    void setDisplayDimensions(int w, int h);

    /// Override navbar thickness (DPI-scaled). Default: 56px (unscaled).
    void setNavbarThickness(int thickness);

    /// Compute content dimensions (video res minus margins) for a given config.
    /// Returns {contentW, contentH}. Used by both buildInputDescriptor (touch_screen_config)
    /// and EvdevTouchReader (touch output coordinate space) -- single source of truth.
    static std::pair<int,int> computeContentDimensions(
        int videoW, int videoH, int displayW, int displayH,
        bool navbarDuringAA, const QString& navbarEdge, int navbarThickness = 56);

private:
    QByteArray buildVideoDescriptor() const;
    QByteArray buildClusterVideoDescriptor() const;
    QByteArray buildMediaAudioDescriptor() const;
    QByteArray buildSpeechAudioDescriptor() const;
    QByteArray buildSystemAudioDescriptor() const;
    QByteArray buildInputDescriptor() const;
    QByteArray buildClusterInputDescriptor() const;
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
    QString wifiBssid_;
    QStringList videoCodecNames_;
    oaa::ProtocolVersion protocolVersion_ = oaa::kGalVersion1_7;
    ProjectedClusterConfig projectedClusterConfig_;
    int overrideDisplayW_ = 0;  // 0 = use yamlConfig values
    int overrideDisplayH_ = 0;
    int navbarThickness_ = 56;
};

} // namespace aa
} // namespace oap
