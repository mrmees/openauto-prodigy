#pragma once

#include "MediaTagReader.hpp"
#include <QAbstractListModel>
#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QVariantList>
#include <QVector>

namespace oap {
namespace plugins {

/// One scanned file (produced by MediaScanner, Task 4).
struct MediaTrackRecord {
    QString path;
    QString volumeKey;   // root/volume identity supplied by the scan caller
                         // (udisks IdUUID for hot-plugged volumes, canonical
                         // path hash for config dirs — Codex P1: never
                         // recomputed after unmount)
    MediaTrackInfo info;
    QString artFile;     // cached album-art absolute path, or empty
};

/// Provisional album-bucket key — the ONE grouping shared by
/// MediaLibrary::rebuild() pass 1 and MediaScanner's art pass (Codex P1:
/// art keys must not drift from library grouping). Prefixes:
/// 'a' = albumartist bucket, 'd' = dir-scoped (VA detection), 'u' =
/// per-artist unknown-album (never VA).
inline QString mediaAlbumBucketKey(const MediaTrackInfo& t, const QString& path) {
    if (!t.albumArtist.isEmpty())
        return QStringLiteral("a\x1f") + t.albumArtist.toLower()
               + QLatin1Char('\x1f') + t.album.toLower();
    if (!t.album.isEmpty())
        return QStringLiteral("d\x1f") + t.album.toLower() + QLatin1Char('\x1f')
               + QFileInfo(path).absolutePath().toLower();
    return QStringLiteral("u\x1f")
           + (!t.artist.isEmpty() ? t.artist.toLower()
                                  : QStringLiteral("unknown artist"));
}

class LibraryListModel;  // internal generic name/key/subtitle/artUrl/path model

/// In-memory Artist -> Album -> Track index + the three QML list models.
/// Library heuristics informed by Yarock (GPL-3.0, github.com/sebaro/Yarock)
/// and Strawberry (GPL-3.0); no code copied. Design §8 amendment 2026-07-09.
class MediaLibrary : public QObject {
    Q_OBJECT
public:
    explicit MediaLibrary(QObject* parent = nullptr);
    ~MediaLibrary() override;

    QObject* artistsModel() const;
    QObject* albumsModel() const;
    QObject* tracksModel() const;

    void setTracks(QVector<MediaTrackRecord> all);
    void removeVolume(const QString& volumeKey);
    int trackCount() const { return tracks_.size(); }

    Q_INVOKABLE QVariantList albumsForArtist(const QString& artistKey) const;
    Q_INVOKABLE QVariantList tracksForAlbum(const QString& albumKey) const;
    Q_INVOKABLE QStringList trackPathsForAlbum(const QString& albumKey) const;
    Q_INVOKABLE QStringList allTrackPathsSorted() const;
    Q_INVOKABLE QString artistsModelKeyAt(int row) const;
    Q_INVOKABLE QString albumsModelKeyAt(int row) const;

signals:
    void libraryChanged();

private:
    void rebuild();

    struct AlbumMeta { QString name, artUrl; bool compilation = false; };

    QVector<MediaTrackRecord> tracks_;
    LibraryListModel* artists_;
    LibraryListModel* albums_;
    LibraryListModel* trackList_;
    // albumKey -> ordered track indices into tracks_; artistKey -> albumKeys
    QHash<QString, QVector<int>> albumTracks_;
    QHash<QString, QStringList> artistAlbums_;
    QHash<QString, AlbumMeta> albumMeta_;
};

} // namespace plugins
} // namespace oap

Q_DECLARE_METATYPE(oap::plugins::MediaTrackRecord)
Q_DECLARE_METATYPE(QVector<oap::plugins::MediaTrackRecord>)
