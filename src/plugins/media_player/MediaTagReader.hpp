#pragma once

#include <QByteArray>
#include <QString>

#include <atomic>

namespace oap {
namespace plugins {

/// One track's tags as read from the container. Library heuristics
/// informed by Yarock (GPL-3.0, github.com/sebaro/Yarock) and Strawberry
/// (GPL-3.0); no code copied.
struct MediaTrackInfo {
    QString title, artist, albumArtist, album, genre;
    int year = 0, trackNo = 0, discNo = 0;
    qint64 durationMs = 0;
    bool hasEmbeddedArt = false;
    bool valid = false;
};

/// libavformat tag/art reader — header-only demux, no decode (except the
/// attached_pic stream, which ships pre-encoded). Zero new deps: avformat
/// joins the avcodec/avutil modules the video path already links.
namespace MediaTagReader {
MediaTrackInfo read(const QString& path,
                    const std::atomic_bool* cancelled = nullptr);
QByteArray embeddedArt(const QString& path,
                       const std::atomic_bool* cancelled = nullptr);
}

} // namespace plugins
} // namespace oap
