#pragma once

#include <QString>
#include <QList>
#include <QByteArray>
#include <cstdint>

namespace oaa {

struct ChannelConfig {
    uint8_t channelId = 0;
    QByteArray descriptor;  // Pre-serialized ChannelDescriptor protobuf
};

struct SessionConfig {
    uint16_t protocolMajor = 1;
    uint16_t protocolMinor = 7;

    QString headUnitName = "OpenAuto Prodigy";
    QString carModel = "Universal";
    QString carYear = "2025";
    QString carSerial = "0000000000000001";
    bool leftHandDrive = true;
    QString manufacturer = "OpenAuto";
    QString model = "Prodigy";
    QString swBuild = "dev";
    QString swVersion = "0.4.0";
    bool canPlayNativeMediaDuringVr = true;

    // Timeouts (ms)
    int versionTimeout = 5000;
    int handshakeTimeout = 10000;
    int discoveryTimeout = 10000;
    int pingInterval = 5000;
    int pingTimeout = 15000;

    // Channel capabilities — each entry is a pre-serialized ChannelDescriptor
    // Prodigy builds these from its config; library inserts them into
    // ServiceDiscoveryResponse.
    QList<ChannelConfig> channels;
};

} // namespace oaa
