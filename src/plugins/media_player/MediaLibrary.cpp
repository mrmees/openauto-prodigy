#include "MediaLibrary.hpp"

#include <QFileInfo>
#include <QSet>
#include <QUrl>
#include <algorithm>
#include <climits>

namespace oap {
namespace plugins {

namespace {
const QString kVaKeyPrefix = QStringLiteral("\x1fVA\x1f");
const QString kVaDisplay = QStringLiteral("Various Artists");
const QString kUnknownArtist = QStringLiteral("Unknown Artist");
const QString kUnknownAlbum = QStringLiteral("Unknown Album");

qint64 trackSortKey(const MediaTrackInfo& t) {   // §8: disc*1000 + track
    return qint64(t.discNo) * 1000 + t.trackNo;
}
} // namespace

/// Generic list model: rows of {name, key, subtitle, artUrl, path}.
class LibraryListModel : public QAbstractListModel {
public:
    enum Roles { NameRole = Qt::UserRole + 1, KeyRole, SubtitleRole, ArtUrlRole,
                 PathRole, CompilationRole };
    struct Row { QString name, key, subtitle, artUrl, path; bool compilation = false; };

    explicit LibraryListModel(QObject* parent) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : rows_.size();
    }
    QVariant data(const QModelIndex& idx, int role) const override {
        if (!idx.isValid() || idx.column() != 0 || idx.row() >= rows_.size()) return {};
        const Row& r = rows_.at(idx.row());
        switch (role) {
        case NameRole: return r.name;
        case KeyRole: return r.key;
        case SubtitleRole: return r.subtitle;
        case ArtUrlRole: return r.artUrl;
        case PathRole: return r.path;
        case CompilationRole: return r.compilation;
        }
        return {};
    }
    QHash<int, QByteArray> roleNames() const override {
        return { {NameRole, "name"}, {KeyRole, "key"}, {SubtitleRole, "subtitle"},
                 {ArtUrlRole, "artUrl"}, {PathRole, "path"},
                 {CompilationRole, "compilation"} };
    }
    void reset(QVector<Row> rows) {
        beginResetModel();
        rows_ = std::move(rows);
        endResetModel();
    }
    QString keyAt(int row) const { return rows_.value(row).key; }

private:
    QVector<Row> rows_;
};

MediaLibrary::MediaLibrary(QObject* parent)
    : QObject(parent),
      artists_(new LibraryListModel(this)),
      albums_(new LibraryListModel(this)),
      trackList_(new LibraryListModel(this)) {}

MediaLibrary::~MediaLibrary() = default;

QObject* MediaLibrary::artistsModel() const { return artists_; }
QObject* MediaLibrary::albumsModel() const { return albums_; }
QObject* MediaLibrary::tracksModel() const { return trackList_; }
QString MediaLibrary::artistsModelKeyAt(int row) const { return artists_->keyAt(row); }
QString MediaLibrary::albumsModelKeyAt(int row) const { return albums_->keyAt(row); }

void MediaLibrary::setTracks(QVector<MediaTrackRecord> all) {
    tracks_ = std::move(all);
    rebuild();
}

void MediaLibrary::removeVolume(const QString& volumeKey) {
    tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                                 [&](const MediaTrackRecord& r) { return r.volumeKey == volumeKey; }),
                  tracks_.end());
    rebuild();
}

void MediaLibrary::rebuild() {
    albumTracks_.clear();
    artistAlbums_.clear();
    albumMeta_.clear();

    struct AlbumAgg {
        QString display, artistDisplay, artUrl;
        QSet<QString> trackArtistsLower;
        QHash<QString, QString> artistDisplayByLower;  // lower -> first-seen case
        bool compilation = false;
        int firstOrdinal = INT_MAX;   // lowest track index — deterministic
                                      // "first-seen" across QHash merges
        QVector<int> trackIdx;
    };
    const auto mergeInto = [](AlbumAgg& dst, const AlbumAgg& src) {
        // Display fields follow the LOWEST ordinal (input order), not hash
        // iteration order (Codex re-run P2 — determinism).
        if (dst.display.isEmpty() || src.firstOrdinal < dst.firstOrdinal) {
            dst.display = src.display;
            if (!src.artistDisplay.isEmpty()) dst.artistDisplay = src.artistDisplay;
        }
        if (dst.artUrl.isEmpty()) dst.artUrl = src.artUrl;
        dst.trackArtistsLower.unite(src.trackArtistsLower);
        for (auto it = src.artistDisplayByLower.begin(); it != src.artistDisplayByLower.end(); ++it)
            if (!dst.artistDisplayByLower.contains(it.key()))
                dst.artistDisplayByLower.insert(it.key(), it.value());
        dst.compilation = dst.compilation || src.compilation;
        dst.firstOrdinal = qMin(dst.firstOrdinal, src.firstOrdinal);
        dst.trackIdx += src.trackIdx;
    };

    // Pass 1: provisional buckets (Codex P1 fixes: empty-album tracks never
    // enter VA detection; resolution MERGES rather than inserts).
    //  - albumartist present:           (albumartist, album) — unambiguous
    //  - no albumartist, album tagged:  (album, parent dir) — VA detection
    //    scope: a compilation's tracks share a folder; two artists' distinct
    //    same-named albums live in different folders (Strawberry trick; a
    //    multi-folder untagged compilation degrades to one album per folder)
    //  - no albumartist, album MISSING: (artist ?: unknown) — per-artist
    //    "Unknown Album" bucket (§8 #2); NEVER a VA candidate
    QHash<QString, AlbumAgg> keyed, dirScoped, unknownAlbum;
    for (int i = 0; i < tracks_.size(); ++i) {
        const MediaTrackInfo& t = tracks_[i].info;
        const bool hasAlbumArtist = !t.albumArtist.isEmpty();
        const bool hasAlbum = !t.album.isEmpty();
        // Shared keying: mediaAlbumBucketKey (MediaLibrary.hpp) is the single
        // source of truth; the prefix routes the bucket map.
        const QString bucketKey = mediaAlbumBucketKey(t, tracks_[i].path);
        QHash<QString, AlbumAgg>* bucketMap =
            hasAlbumArtist ? &keyed : hasAlbum ? &dirScoped : &unknownAlbum;
        AlbumAgg& a = (*bucketMap)[bucketKey];
        a.firstOrdinal = qMin(a.firstOrdinal, i);
        if (a.display.isEmpty()) {
            a.display = hasAlbum ? t.album : kUnknownAlbum;
            if (hasAlbumArtist) a.artistDisplay = t.albumArtist;
        }
        if (!t.artist.isEmpty()) {
            a.trackArtistsLower.insert(t.artist.toLower());
            if (!a.artistDisplayByLower.contains(t.artist.toLower()))
                a.artistDisplayByLower.insert(t.artist.toLower(), t.artist);
        }
        if (a.artUrl.isEmpty() && !tracks_[i].artFile.isEmpty())
            a.artUrl = QUrl::fromLocalFile(tracks_[i].artFile).toString();
        a.trackIdx.append(i);
    }

    // Pass 2: resolve to final albums, MERGING on key collision so the same
    // (artist, album) reached via different routes/dirs stays ONE album.
    // Final keys: normal = artistLower + '\x1f' + albumLower;
    //             VA     = '\x1fVA\x1f' + albumLower + '\x1f' + dirLower.
    QHash<QString, AlbumAgg> finalAlbums;
    const auto soleArtistDisplay = [&](const AlbumAgg& a) {
        if (a.trackArtistsLower.isEmpty()) return kUnknownArtist;
        return a.artistDisplayByLower.value(*a.trackArtistsLower.begin());
    };
    for (auto it = keyed.begin(); it != keyed.end(); ++it) {
        AlbumAgg a = it.value();
        const QString key = a.artistDisplay.toLower() + QLatin1Char('\x1f') + a.display.toLower();
        mergeInto(finalAlbums[key], a);
    }
    for (auto it = dirScoped.begin(); it != dirScoped.end(); ++it) {
        AlbumAgg a = it.value();
        if (a.trackArtistsLower.size() > 1) {
            a.artistDisplay = kVaDisplay;
            a.compilation = true;
            mergeInto(finalAlbums[kVaKeyPrefix + it.key()], a);
        } else {
            a.artistDisplay = soleArtistDisplay(a);
            mergeInto(finalAlbums[a.artistDisplay.toLower() + QLatin1Char('\x1f')
                                  + a.display.toLower()], a);
        }
    }
    for (auto it = unknownAlbum.begin(); it != unknownAlbum.end(); ++it) {
        AlbumAgg a = it.value();
        a.artistDisplay = soleArtistDisplay(a);
        mergeInto(finalAlbums[a.artistDisplay.toLower() + QLatin1Char('\x1f')
                              + a.display.toLower()], a);
    }

    // Art propagation (Codex re-run P1): the scanner groups art by
    // PROVISIONAL buckets, so a final album merged across directories can
    // hold artless records from the art-free directory — give every record
    // in a final album the album's first non-empty artFile.
    for (auto it = finalAlbums.begin(); it != finalAlbums.end(); ++it) {
        QString art;
        for (int i : it.value().trackIdx)
            if (!tracks_[i].artFile.isEmpty()) { art = tracks_[i].artFile; break; }
        if (art.isEmpty()) continue;
        for (int i : it.value().trackIdx)
            if (tracks_[i].artFile.isEmpty()) tracks_[i].artFile = art;
        if (it.value().artUrl.isEmpty())
            it.value().artUrl = QUrl::fromLocalFile(art).toString();
    }

    // Order tracks inside each album; build model rows. Iterate albums in
    // firstOrdinal order so artist display casing is deterministic too.
    QVector<LibraryListModel::Row> albumRows, artistRows, trackRows;
    QHash<QString, QString> artistKeyToDisplay;
    QStringList orderedKeys = finalAlbums.keys();
    std::sort(orderedKeys.begin(), orderedKeys.end(),
              [&](const QString& l, const QString& r) {
                  return finalAlbums[l].firstOrdinal < finalAlbums[r].firstOrdinal;
              });

    for (const QString& albumKey : orderedKeys) {
        auto it = finalAlbums.find(albumKey);
        AlbumAgg& a = it.value();
        std::sort(a.trackIdx.begin(), a.trackIdx.end(), [this](int l, int r) {
            const auto& lt = tracks_[l].info; const auto& rt = tracks_[r].info;
            if (trackSortKey(lt) != trackSortKey(rt)) return trackSortKey(lt) < trackSortKey(rt);
            const int c = lt.title.compare(rt.title, Qt::CaseInsensitive);
            if (c != 0) return c < 0;
            return tracks_[l].path < tracks_[r].path;
        });
        albumTracks_.insert(it.key(), a.trackIdx);
        albumMeta_.insert(it.key(), {a.display, a.artUrl, a.compilation});
        const QString artistKey = a.artistDisplay.toLower();
        artistKeyToDisplay.insert(artistKey, a.artistDisplay);
        artistAlbums_[artistKey].append(it.key());
        albumRows.append({a.display, it.key(), a.artistDisplay, a.artUrl, {},
                          a.compilation});
    }

    std::sort(albumRows.begin(), albumRows.end(), [](const auto& l, const auto& r) {
        const int c = l.name.compare(r.name, Qt::CaseInsensitive);
        return c != 0 ? c < 0 : l.subtitle.compare(r.subtitle, Qt::CaseInsensitive) < 0;
    });

    for (auto it = artistAlbums_.begin(); it != artistAlbums_.end(); ++it)
        artistRows.append({artistKeyToDisplay.value(it.key()), it.key(),
                           QStringLiteral("%1 album(s)").arg(it.value().size()), {}, {}});
    std::sort(artistRows.begin(), artistRows.end(), [](const auto& l, const auto& r) {
        return l.name.compare(r.name, Qt::CaseInsensitive) < 0;
    });

    for (int i = 0; i < tracks_.size(); ++i)
        trackRows.append({tracks_[i].info.title, tracks_[i].path, tracks_[i].info.artist,
                          tracks_[i].artFile.isEmpty()
                              ? QString()
                              : QUrl::fromLocalFile(tracks_[i].artFile).toString(),
                          tracks_[i].path});
    std::sort(trackRows.begin(), trackRows.end(), [](const auto& l, const auto& r) {
        const int c = l.name.compare(r.name, Qt::CaseInsensitive);
        return c != 0 ? c < 0 : l.path < r.path;
    });

    artists_->reset(std::move(artistRows));
    albums_->reset(std::move(albumRows));
    trackList_->reset(std::move(trackRows));
    emit libraryChanged();
}

QVariantList MediaLibrary::albumsForArtist(const QString& artistKey) const {
    // Aggregate-backed (not first-track) and name-sorted (Codex P2).
    QStringList keys = artistAlbums_.value(artistKey);
    std::sort(keys.begin(), keys.end(), [this](const QString& l, const QString& r) {
        return albumMeta_.value(l).name.compare(albumMeta_.value(r).name,
                                                Qt::CaseInsensitive) < 0;
    });
    QVariantList out;
    for (const QString& albumKey : keys) {
        const AlbumMeta meta = albumMeta_.value(albumKey);
        QVariantMap m;
        m.insert(QStringLiteral("key"), albumKey);
        m.insert(QStringLiteral("name"), meta.name);
        m.insert(QStringLiteral("artUrl"), meta.artUrl);
        m.insert(QStringLiteral("compilation"), meta.compilation);
        m.insert(QStringLiteral("trackCount"), albumTracks_.value(albumKey).size());
        out.append(m);
    }
    return out;
}

QVariantList MediaLibrary::tracksForAlbum(const QString& albumKey) const {
    // Rich rows for the drill-down track list (Codex P2): index order here is
    // EXACTLY the order trackPathsForAlbum() returns, so playAlbumFromPath()
    // can resolve a tapped row's PATH back to its current album index.
    QVariantList out;
    for (int i : albumTracks_.value(albumKey)) {
        const MediaTrackRecord& r = tracks_[i];
        QVariantMap m;
        m.insert(QStringLiteral("title"), r.info.title);
        m.insert(QStringLiteral("artist"), r.info.artist);
        m.insert(QStringLiteral("path"), r.path);
        m.insert(QStringLiteral("artUrl"), r.artFile.isEmpty()
                     ? QString() : QUrl::fromLocalFile(r.artFile).toString());
        m.insert(QStringLiteral("trackNo"), r.info.trackNo);
        m.insert(QStringLiteral("discNo"), r.info.discNo);
        out.append(m);
    }
    return out;
}

QStringList MediaLibrary::trackPathsForAlbum(const QString& albumKey) const {
    QStringList out;
    for (int i : albumTracks_.value(albumKey)) out << tracks_[i].path;
    return out;
}

QStringList MediaLibrary::allTrackPathsSorted() const {
    QStringList out;
    for (int i = 0; i < trackList_->rowCount(); ++i)
        out << trackList_->keyAt(i);   // track rows key == path
    return out;
}

} // namespace plugins
} // namespace oap
