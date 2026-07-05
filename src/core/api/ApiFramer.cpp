#include "core/api/ApiFramer.hpp"

namespace oap::api {

ApiFramer::ApiFramer(quint32 maxFrameBytes) : maxFrameBytes_(maxFrameBytes) {}

QByteArray ApiFramer::encode(const QByteArray& payload) {
    const quint32 n = static_cast<quint32>(payload.size());
    QByteArray out;
    out.reserve(4 + payload.size());
    out.append(char((n >> 24) & 0xFF));
    out.append(char((n >> 16) & 0xFF));
    out.append(char((n >> 8) & 0xFF));
    out.append(char(n & 0xFF));
    out.append(payload);
    return out;
}

QList<QByteArray> ApiFramer::feed(const QByteArray& chunk) {
    QList<QByteArray> frames;
    if (violated_) return frames;
    buffer_.append(chunk);
    while (buffer_.size() >= 4) {
        const auto* d = reinterpret_cast<const unsigned char*>(buffer_.constData());
        const quint32 len = (quint32(d[0]) << 24) | (quint32(d[1]) << 16)
                          | (quint32(d[2]) << 8) | quint32(d[3]);
        if (len == 0 || len > maxFrameBytes_) {
            violated_ = true;
            buffer_.clear();
            return frames;
        }
        if (quint32(buffer_.size()) < 4 + len) break;
        frames.append(buffer_.mid(4, len));
        buffer_.remove(0, 4 + int(len));
    }
    return frames;
}

bool ApiFramer::violated() const { return violated_; }

} // namespace oap::api
