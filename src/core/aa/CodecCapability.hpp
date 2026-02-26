#pragma once

#include <string>
#include <vector>
#include <map>

namespace oap {
namespace aa {

struct DecoderInfo {
    std::string name;       // e.g. "h264_v4l2m2m"
    bool isHardware;
};

struct CodecInfo {
    std::vector<DecoderInfo> hardware;
    std::vector<DecoderInfo> software;
};

class CodecCapability {
public:
    /// Probe FFmpeg for available decoders. Returns map of codec name -> CodecInfo.
    static std::map<std::string, CodecInfo> probe();

    /// Return list of codec names that have at least one available decoder.
    static std::vector<std::string> availableCodecs(
        const std::map<std::string, CodecInfo>& caps);
};

} // namespace aa
} // namespace oap
