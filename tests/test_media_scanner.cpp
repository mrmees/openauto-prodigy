#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "plugins/media_player/MediaScanner.hpp"

using namespace oap::plugins;

class TestMediaScanner : public QObject {
    Q_OBJECT
    QString fixtures() const { return QStringLiteral(TEST_DATA_DIR "/media/library"); }

    MediaScanner::Root rootFor(const QString& p) const {
        return {QStringLiteral("root"), p, MediaScanner::rootKeyForPath(p)};
    }
    QVector<MediaTrackRecord> runScan(MediaScanner& s, const QString& root) {
        QSignalSpy done(&s, &MediaScanner::finished);
        s.scan({rootFor(root)});
        [&]{ QVERIFY(done.wait(15000)); }();
        [&]{ QVERIFY(!s.busy()); }();       // Codex P1: false when finished fires
        [&]{ QVERIFY(!s.scanning()); }();
        return done.takeFirst().at(0).value<QVector<MediaTrackRecord>>();
    }

private slots:
    void coldScanFindsAllAudio() {
        QTemporaryDir cache;
        MediaScanner s; s.setCacheDir(cache.path());
        const auto recs = runScan(s, fixtures());
        // 5 audio fixtures; notes.txt excluded by extension filter.
        QCOMPARE(recs.size(), 5);
        for (const auto& r : recs) QVERIFY(r.info.valid);
    }
    void embeddedArtExtractedOncePerAlbum() {
        QTemporaryDir cache;
        MediaScanner s; s.setCacheDir(cache.path());
        const auto recs = runScan(s, fixtures());
        QString albumAArt;
        for (const auto& r : recs)
            if (r.info.album == QLatin1String("First Album")) {
                QVERIFY2(!r.artFile.isEmpty(), "album with embedded art must get artFile");
                if (albumAArt.isEmpty()) albumAArt = r.artFile;
                QCOMPARE(r.artFile, albumAArt);   // shared per-album, not per-track
            }
        QVERIFY(QFileInfo::exists(albumAArt));
    }
    void incrementalScanSkipsUnchanged() {
        // Copy fixtures to a writable tree so mtimes are ours to control.
        QTemporaryDir tree, cache;
        for (const QFileInfo& fi : QDir(fixtures() + "/AlbumA").entryInfoList(QDir::Files))
            QFile::copy(fi.absoluteFilePath(), tree.path() + "/" + fi.fileName());
        MediaScanner s; s.setCacheDir(cache.path());
        auto first = runScan(s, tree.path());
        QCOMPARE(first.size(), 2);
        // Second scan: cache hit for both -> tag reads == 0.
        auto second = runScan(s, tree.path());
        QCOMPARE(second.size(), 2);
        QCOMPARE(s.lastScanTagReads(), 0);        // probe counter (test hook)
        // Touch one file -> exactly one re-read.
        QFile f(tree.path() + "/01-song-one.mp3");
        f.open(QIODevice::Append); f.write("x"); f.close();
        auto third = runScan(s, tree.path());
        QCOMPARE(third.size(), 2);
        QCOMPARE(s.lastScanTagReads(), 1);
    }
    void corruptFileCachedNotReprobed() {
        // Codex gate P2: an invalid/corrupt file must be cached (valid=false)
        // so a later incremental scan never re-opens and re-probes it. Generate
        // the corrupt fixture IN-TEST (garbage bytes with a .mp3 name) alongside
        // the two real AlbumA fixtures — no committed garbage fixture.
        QTemporaryDir tree, cache;
        for (const QFileInfo& fi : QDir(fixtures() + "/AlbumA").entryInfoList(QDir::Files))
            QFile::copy(fi.absoluteFilePath(), tree.path() + "/" + fi.fileName());
        {
            // Repeated 0xA5 bytes: no MP3 frame sync (0xFF) can form, so
            // avformat finds no audio stream -> MediaTrackInfo.valid == false.
            QFile bad(tree.path() + "/99-corrupt.mp3");
            QVERIFY(bad.open(QIODevice::WriteOnly));
            bad.write(QByteArray(8192, '\xA5'));
            bad.close();
        }
        MediaScanner s; s.setCacheDir(cache.path());
        // Scan 1: 2 valid records; the corrupt file is probed (tag reads > 0)
        // but excluded from the library.
        auto first = runScan(s, tree.path());
        QCOMPARE(first.size(), 2);
        QVERIFY(s.lastScanTagReads() > 0);
        // Scan 2: everything cached — including the corrupt file. Zero tag
        // reads: the corrupt file must NOT be reprobed.
        auto second = runScan(s, tree.path());
        QCOMPARE(second.size(), 2);
        QCOMPARE(s.lastScanTagReads(), 0);
    }
    void vanishedFilesDropFromCache() {
        QTemporaryDir tree, cache;
        for (const QFileInfo& fi : QDir(fixtures() + "/Comp").entryInfoList(QDir::Files))
            QFile::copy(fi.absoluteFilePath(), tree.path() + "/" + fi.fileName());
        MediaScanner s; s.setCacheDir(cache.path());
        QCOMPARE(runScan(s, tree.path()).size(), 2);
        QFile::remove(tree.path() + "/comp-two.flac");
        QCOMPARE(runScan(s, tree.path()).size(), 1);
    }
    void hiddenFilesSkipped() {
        QTemporaryDir tree, cache;
        QFile::copy(fixtures() + "/no-tags-here.ogg", tree.path() + "/.hidden.ogg");
        QFile::copy(fixtures() + "/no-tags-here.ogg", tree.path() + "/visible.ogg");
        MediaScanner s; s.setCacheDir(cache.path());
        QCOMPARE(runScan(s, tree.path()).size(), 1);
    }
    void artFoundOnLaterGroupMember() {
        // Codex P1: first alphabetic file has NO art; the group search must
        // still find the embedded art on a later member and share it.
        QTemporaryDir tree, cache;
        QFile::copy(fixtures() + "/AlbumA/02-song-two.mp3", tree.path() + "/00-artless.mp3");
        QFile::copy(fixtures() + "/AlbumA/01-song-one.mp3", tree.path() + "/01-song-one.mp3");
        MediaScanner s; s.setCacheDir(cache.path());
        const auto recs = runScan(s, tree.path());
        QCOMPARE(recs.size(), 2);
        for (const auto& r : recs)
            QVERIFY2(!r.artFile.isEmpty(), qPrintable(r.path));
    }
    void vaCompilationSharesArt() {
        // Comp fixtures: art embedded on comp-one only; both records must
        // share it via the (album, dir) group.
        QTemporaryDir cache;
        MediaScanner s; s.setCacheDir(cache.path());
        const auto recs = runScan(s, fixtures());
        QString compArt;
        for (const auto& r : recs)
            if (r.info.album == QLatin1String("Hits Comp")) {
                QVERIFY(!r.artFile.isEmpty());
                if (compArt.isEmpty()) compArt = r.artFile;
                QCOMPARE(r.artFile, compArt);
            }
        QVERIFY(!compArt.isEmpty());
    }
    void scanWhileBusyCoalesces() {
        // Codex P1: second scan() while busy replaces the in-flight one;
        // exactly ONE finished(), reflecting the newest roots.
        QTemporaryDir treeA, treeB, cache;
        QFile::copy(fixtures() + "/no-tags-here.ogg", treeA.path() + "/a.ogg");
        QFile::copy(fixtures() + "/no-tags-here.ogg", treeB.path() + "/b1.ogg");
        QFile::copy(fixtures() + "/no-tags-here.ogg", treeB.path() + "/b2.ogg");
        MediaScanner s; s.setCacheDir(cache.path());
        QSignalSpy done(&s, &MediaScanner::finished);
        s.scan({rootFor(treeA.path())});
        s.scan({rootFor(treeB.path())});
        QVERIFY(done.wait(15000));
        QCOMPARE(done.count(), 1);
        QCOMPARE(done.takeFirst().at(0).value<QVector<MediaTrackRecord>>().size(), 2);
        QVERIFY(!s.busy());
    }
};

QTEST_MAIN(TestMediaScanner)
#include "test_media_scanner.moc"
