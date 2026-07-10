#include <QtTest>
#include <QAbstractListModel>
#include "plugins/media_player/MediaLibrary.hpp"

using namespace oap::plugins;

namespace {
MediaTrackRecord rec(const QString& path, const QString& title, const QString& artist,
                     const QString& albumArtist, const QString& album,
                     int track = 0, int disc = 0, const QString& vol = QStringLiteral("v1")) {
    MediaTrackRecord r;
    r.path = path; r.volumeKey = vol;
    r.info.title = title; r.info.artist = artist; r.info.albumArtist = albumArtist;
    r.info.album = album; r.info.trackNo = track; r.info.discNo = disc; r.info.valid = true;
    return r;
}
QStringList names(QObject* model) {
    auto* m = qobject_cast<QAbstractListModel*>(model);
    QStringList out;
    for (int i = 0; i < m->rowCount(); ++i)
        out << m->data(m->index(i, 0), m->roleNames().key("name")).toString();
    return out;
}
} // namespace

class TestMediaLibrary : public QObject {
    Q_OBJECT
private slots:
    void albumArtistGroupsAlbum() {
        MediaLibrary lib;
        lib.setTracks({ rec("/a/1.mp3", "One", "Band feat. Guest", "Band", "LP", 1),
                        rec("/a/2.mp3", "Two", "Band",             "Band", "LP", 2) });
        QCOMPARE(names(lib.albumsModel()), QStringList{QStringLiteral("LP")});
        QCOMPARE(names(lib.artistsModel()), QStringList{QStringLiteral("Band")});
    }
    void variousArtistsCollapse() {
        // Same album, differing artists, NO albumartist -> ONE VA album.
        MediaLibrary lib;
        lib.setTracks({ rec("/c/1.flac", "C1", "Artist X", "", "Hits Comp", 1),
                        rec("/c/2.flac", "C2", "Artist Y", "", "Hits Comp", 2) });
        QCOMPARE(names(lib.albumsModel()), QStringList{QStringLiteral("Hits Comp")});
        QCOMPARE(names(lib.artistsModel()), QStringList{QStringLiteral("Various Artists")});
    }
    void sameArtistNoAlbumArtistDoesNotCollapseToVA() {
        MediaLibrary lib;
        lib.setTracks({ rec("/b/1.mp3", "B1", "Solo", "", "Solo LP", 1),
                        rec("/b/2.mp3", "B2", "Solo", "", "Solo LP", 2) });
        QCOMPARE(names(lib.artistsModel()), QStringList{QStringLiteral("Solo")});
    }
    void sameAlbumNameDifferentDirsStaySeparate() {
        // Two artists each shipping "Greatest Hits" in their own folder,
        // neither tagged with albumartist — must NOT merge into VA.
        MediaLibrary lib;
        lib.setTracks({ rec("/x/greatest/1.mp3", "One", "Artist A", "", "Greatest Hits", 1),
                        rec("/y/greatest/1.mp3", "Uno", "Artist B", "", "Greatest Hits", 1) });
        QCOMPARE(qobject_cast<QAbstractListModel*>(lib.albumsModel())->rowCount(), 2);
        QCOMPARE(names(lib.artistsModel()),
                 (QStringList{QStringLiteral("Artist A"), QStringLiteral("Artist B")}));
    }
    void sameAlbumAcrossDirsMergesForOneArtist() {
        // Codex P1: multi-folder album by ONE artist (no albumartist) must
        // resolve to ONE album — the merge, not an insert-overwrite.
        MediaLibrary lib;
        lib.setTracks({ rec("/disc1/1.mp3", "T1", "Band", "", "Double LP", 1, 1),
                        rec("/disc2/1.mp3", "T2", "Band", "", "Double LP", 1, 2) });
        QCOMPARE(qobject_cast<QAbstractListModel*>(lib.albumsModel())->rowCount(), 1);
        QCOMPARE(lib.trackPathsForAlbum(lib.albumsModelKeyAt(0)).size(), 2);
    }
    void missingAlbumNeverBecomesVA() {
        // Codex P1: two artists' untagged-album files in ONE directory are
        // per-artist Unknown Album buckets, not a false compilation.
        MediaLibrary lib;
        lib.setTracks({ rec("/mix/a.mp3", "A", "Artist X", "", "", 0),
                        rec("/mix/b.mp3", "B", "Artist Y", "", "", 0) });
        QCOMPARE(qobject_cast<QAbstractListModel*>(lib.albumsModel())->rowCount(), 2);
        QVERIFY(!names(lib.artistsModel()).contains(QStringLiteral("Various Artists")));
    }
    void soleArtistResolvedFromAnyTrack() {
        // Codex P1: first track has no artist tag; the bucket's single
        // distinct artist (from track 2) names the album.
        MediaLibrary lib;
        lib.setTracks({ rec("/s/1.mp3", "One", "", "", "Solo LP", 1),
                        rec("/s/2.mp3", "Two", "Solo", "", "Solo LP", 2) });
        QCOMPARE(names(lib.artistsModel()), QStringList{QStringLiteral("Solo")});
    }
    void compilationRoleExposed() {
        MediaLibrary lib;
        lib.setTracks({ rec("/c/1.flac", "C1", "Artist X", "", "Hits Comp", 1),
                        rec("/c/2.flac", "C2", "Artist Y", "", "Hits Comp", 2) });
        auto* m = qobject_cast<QAbstractListModel*>(lib.albumsModel());
        QCOMPARE(m->data(m->index(0, 0), m->roleNames().key("compilation")).toBool(), true);
    }
    void artPropagatesAcrossMergedDirs() {
        // Codex re-run P1: scanner art is provisional-bucket scoped; the
        // merged final album must propagate art to its artless records.
        auto withArt = rec("/disc1/1.mp3", "T1", "Band", "", "Double LP", 1, 1);
        withArt.artFile = QStringLiteral("/cache/art/x.jpg");
        MediaLibrary lib;
        lib.setTracks({ withArt,
                        rec("/disc2/1.mp3", "T2", "Band", "", "Double LP", 1, 2) });
        const auto rows = lib.tracksForAlbum(lib.albumsModelKeyAt(0));
        QCOMPARE(rows.size(), 2);
        for (const QVariant& v : rows)
            QVERIFY(!v.toMap().value(QStringLiteral("artUrl")).toString().isEmpty());
    }
    void unknownBuckets() {
        MediaLibrary lib;
        lib.setTracks({ rec("/u/x.mp3", "x", "", "", "") });
        QCOMPARE(names(lib.artistsModel()), QStringList{QStringLiteral("Unknown Artist")});
        QCOMPARE(names(lib.albumsModel()), QStringList{QStringLiteral("Unknown Album")});
    }
    void discFoldedTrackOrder() {
        MediaLibrary lib;
        lib.setTracks({ rec("/d/d2t1.mp3", "D2T1", "B", "B", "Double", 1, 2),
                        rec("/d/d1t2.mp3", "D1T2", "B", "B", "Double", 2, 1),
                        rec("/d/d1t1.mp3", "D1T1", "B", "B", "Double", 1, 1) });
        const QString key = lib.albumsModelKeyAt(0);
        QCOMPARE(lib.trackPathsForAlbum(key),
                 (QStringList{QStringLiteral("/d/d1t1.mp3"), QStringLiteral("/d/d1t2.mp3"),
                              QStringLiteral("/d/d2t1.mp3")}));
    }
    void removeVolumeDropsItsTracks() {
        MediaLibrary lib;
        lib.setTracks({ rec("/usb/1.mp3", "U1", "A", "A", "UsbAlbum", 1, 0, "usbvol"),
                        rec("/home/1.mp3", "H1", "B", "B", "HomeAlbum", 1, 0, "homevol") });
        QCOMPARE(lib.trackCount(), 2);
        lib.removeVolume(QStringLiteral("usbvol"));
        QCOMPARE(lib.trackCount(), 1);
        QCOMPARE(names(lib.albumsModel()), QStringList{QStringLiteral("HomeAlbum")});
    }
    void drillDownAlbumsForArtist() {
        MediaLibrary lib;
        lib.setTracks({ rec("/a/1.mp3", "1", "Band", "Band", "LP1", 1),
                        rec("/a/2.mp3", "2", "Band", "Band", "LP2", 1),
                        rec("/z/1.mp3", "z", "Other", "Other", "ZLP", 1) });
        const QString bandKey = lib.artistsModelKeyAt(0);  // "Band" (alpha before Other)
        QCOMPARE(lib.albumsForArtist(bandKey).size(), 2);
    }
};

QTEST_APPLESS_MAIN(TestMediaLibrary)
#include "test_media_library.moc"
