#include "core/aa/CodecCapability.hpp"

#include <QDebug>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace oap {
namespace aa {

// Decoder candidates per codec: name, isHardware
struct DecoderCandidate {
    const char* codec;
    const char* decoder;
    bool isHardware;
};

static const DecoderCandidate kCandidates[] = {
    // H.264
    {"h264", "h264_v4l2m2m", true},
    {"h264", "h264_vaapi",   true},
    {"h264", "h264",         false},

    // H.265 / HEVC
    {"h265", "hevc_v4l2m2m", true},
    {"h265", "hevc_vaapi",   true},
    {"h265", "hevc",         false},

    // VP9
    {"vp9", "vp9_v4l2m2m",  true},
    {"vp9", "vp9_vaapi",     true},
    {"vp9", "libvpx-vp9",   false},
    {"vp9", "vp9",           false},

    // AV1
    {"av1", "av1_v4l2m2m",  true},
    {"av1", "av1_vaapi",     true},
    {"av1", "libdav1d",      false},
    {"av1", "av1",           false},
};

std::map<std::string, CodecInfo> CodecCapability::probe()
{
    std::map<std::string, CodecInfo> result;

    for (const auto& c : kCandidates) {
        const AVCodec* dec = avcodec_find_decoder_by_name(c.decoder);
        if (!dec) {
            continue;
        }

        DecoderInfo info{c.decoder, c.isHardware};
        auto& ci = result[c.codec];

        if (c.isHardware) {
            ci.hardware.push_back(std::move(info));
            qInfo() << "CodecCapability: found HW decoder" << c.decoder << "for" << c.codec;
        } else {
            ci.software.push_back(std::move(info));
            qInfo() << "CodecCapability: found SW decoder" << c.decoder << "for" << c.codec;
        }
    }

    return result;
}

std::vector<std::string> CodecCapability::availableCodecs(
    const std::map<std::string, CodecInfo>& caps)
{
    std::vector<std::string> codecs;
    codecs.reserve(caps.size());

    for (const auto& [name, info] : caps) {
        if (!info.hardware.empty() || !info.software.empty()) {
            codecs.push_back(name);
        }
    }

    return codecs;
}

} // namespace aa
} // namespace oap
