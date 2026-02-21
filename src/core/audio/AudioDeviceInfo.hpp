#pragma once
#include <QString>
#include <cstdint>

namespace oap {

struct AudioDeviceInfo {
    uint32_t registryId = 0;   // PipeWire session-scoped ID
    QString nodeName;           // node.name — used as config key
    QString description;        // node.description — display name
    QString mediaClass;         // "Audio/Sink", "Audio/Source", "Audio/Duplex"
};

} // namespace oap
