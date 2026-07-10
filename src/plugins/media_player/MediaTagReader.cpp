#include "MediaTagReader.hpp"

#include <QFileInfo>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
}

namespace oap {
namespace plugins {
namespace MediaTagReader {

namespace {

QString tagFrom(AVDictionary* meta, const char* key) {
    // Exact-key match only — AV_DICT_IGNORE_SUFFIX would let "artist" match
    // sort keys like "artist-sort" (Codex P2).
    const AVDictionaryEntry* e = meta ? av_dict_get(meta, key, nullptr, 0) : nullptr;
    return e ? QString::fromUtf8(e->value).trimmed() : QString();
}

// Accepts "3" and "3/12" (§8 amendment #2).
int leadingInt(const QString& s) {
    const int slash = s.indexOf(QLatin1Char('/'));
    bool ok = false;
    const int v = (slash < 0 ? s : s.left(slash)).toInt(&ok);
    return ok ? v : 0;
}

struct FormatCtx {  // RAII for avformat_open_input
    AVFormatContext* ctx = nullptr;
    ~FormatCtx() { if (ctx) avformat_close_input(&ctx); }
};

const AVStream* attachedPic(const AVFormatContext* ctx) {
    for (unsigned i = 0; i < ctx->nb_streams; ++i)
        if (ctx->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC)
            return ctx->streams[i];
    return nullptr;
}

} // namespace

MediaTrackInfo read(const QString& path) {
    MediaTrackInfo info;
    FormatCtx f;
    // Bounded probing: tags live in headers; a cache-miss sweep over
    // thousands of files must not deep-probe each one (Codex P2).
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "probesize", "1048576", 0);  // 1 MiB
    const int rc = avformat_open_input(&f.ctx, path.toUtf8().constData(), nullptr, &opts);
    av_dict_free(&opts);
    if (rc != 0)
        return info;
    f.ctx->max_analyze_duration = AV_TIME_BASE;     // 1 s
    if (avformat_find_stream_info(f.ctx, nullptr) < 0)
        return info;
    // A "valid" track must contain at least one real audio stream.
    const AVStream* audio = nullptr;
    for (unsigned i = 0; i < f.ctx->nb_streams; ++i)
        if (f.ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio = f.ctx->streams[i];
            break;
        }
    if (!audio)
        return info;

    // Per-tag fallback: exact key at format level, else exact key on the
    // AUDIO stream (ogg/flac hang tags there; a format dict that only
    // carries "encoder" must not block the fallback — Codex P2).
    const auto tag = [&](const char* key) {
        QString v = tagFrom(f.ctx->metadata, key);
        if (v.isEmpty())
            v = tagFrom(audio->metadata, key);
        return v;
    };

    info.title       = tag("title");
    info.artist      = tag("artist");
    info.albumArtist = tag("album_artist");
    info.album       = tag("album");
    info.genre       = tag("genre");
    info.year        = leadingInt(tag("date").left(4));
    info.trackNo     = leadingInt(tag("track"));
    info.discNo      = leadingInt(tag("disc"));
    if (f.ctx->duration > 0)
        info.durationMs = f.ctx->duration / (AV_TIME_BASE / 1000);
    if (info.title.isEmpty())
        info.title = QFileInfo(path).completeBaseName();  // §8 amendment #2
    info.hasEmbeddedArt = attachedPic(f.ctx) != nullptr;
    info.valid = true;
    return info;
}

QByteArray embeddedArt(const QString& path) {
    FormatCtx f;
    if (avformat_open_input(&f.ctx, path.toUtf8().constData(), nullptr, nullptr) != 0)
        return {};
    if (avformat_find_stream_info(f.ctx, nullptr) < 0)
        return {};
    const AVStream* pic = attachedPic(f.ctx);
    if (!pic || pic->attached_pic.size <= 0)
        return {};
    return QByteArray(reinterpret_cast<const char*>(pic->attached_pic.data),
                      pic->attached_pic.size);
}

} // namespace MediaTagReader
} // namespace plugins
} // namespace oap
