#pragma once

#include <QString>
#include <cstdint>

namespace oaa {

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
};

} // namespace oaa
