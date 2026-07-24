#include <QtTest>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <QMutex>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThread>
#include "plugins/media_player/MediaScanner.hpp"

#include <atomic>
#include <thread>

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
        if (done.isEmpty() && !done.wait(15000)) {
            QTest::qFail(qPrintable(QStringLiteral("scan timed out for root: %1").arg(root)),
                         __FILE__, __LINE__);
            return {};
        }
        if (s.busy() || s.scanning()) {
            QTest::qFail(qPrintable(QStringLiteral(
                             "scan finished with active state for root %1 (busy=%2, scanning=%3)")
                             .arg(root).arg(s.busy()).arg(s.scanning())),
                         __FILE__, __LINE__);
            return {};
        }
        if (done.count() != 1 || done.first().isEmpty()) {
            QTest::qFail(qPrintable(QStringLiteral(
                             "scan produced an invalid finished signal for root %1 (count=%2)")
                             .arg(root).arg(done.count())),
                         __FILE__, __LINE__);
            return {};
        }
        return done.first().at(0).value<QVector<MediaTrackRecord>>();
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
    void symlinkRootEscapeBlocked() {
#ifdef Q_OS_UNIX
        // Codex gate re-run P2: a symlink on an untrusted stick pointing OUTSIDE
        // the scan root must NOT let the walk escape and index files elsewhere.
        // Build a root with one real audio file + a symlink to a SECOND temp dir
        // holding another audio file; the scan must find only the in-root file.
        QTemporaryDir tree, outside, cache;
        QFile::copy(fixtures() + "/no-tags-here.ogg", tree.path() + "/inside.ogg");
        QFile::copy(fixtures() + "/no-tags-here.ogg", outside.path() + "/outside.ogg");
        // link named tree/escape -> the outside dir (a symlink to a directory).
        QVERIFY(QFile::link(outside.path(), tree.path() + "/escape"));
        MediaScanner s; s.setCacheDir(cache.path());
        const auto recs = runScan(s, tree.path());
        QCOMPARE(recs.size(), 1);
        QVERIFY2(recs.first().path.endsWith(QLatin1String("/inside.ogg")),
                 qPrintable(recs.first().path));
#else
        QSKIP("symlink root-escape guard test requires Q_OS_UNIX");
#endif
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

    void stopCancelsActiveAndPendingWorkThenAllowsRestart() {
        QTemporaryDir cache;
        MediaScanner s;
        s.setCacheDir(cache.path());
        QSignalSpy state(&s, &MediaScanner::scanningChanged);
        QSignalSpy done(&s, &MediaScanner::finished);

        // No event-loop turn between start/coalesce/stop: even if the worker
        // finishes quickly, its completion is queued and must be invalidated.
        s.scan({rootFor(fixtures())});
        s.scan({rootFor(fixtures() + QStringLiteral("/AlbumA"))});
        QVERIFY(s.scanning());
        s.stop();
        QVERIFY(!s.busy());
        QVERIFY(!s.scanning());
        QCOMPARE(done.count(), 0);
        QCOMPARE(state.count(), 2);  // one start edge, one explicit stop edge

        // Idempotent and silent once already quiescent.
        s.stop();
        QCOMPARE(state.count(), 2);

        // A fresh generation remains usable; a stale completion from the
        // cancelled worker cannot clear or publish over it.
        const auto records = runScan(s, fixtures() + QStringLiteral("/AlbumA"));
        QCOMPARE(records.size(), 2);
        QVERIFY(!s.busy());
        QVERIFY(!s.scanning());
    }

    void checkpointsCoverWorkerPhasesAndStopInterruptsInFlightEntry() {
        QTemporaryDir cache;
        QSemaphore enteredEntry;
        QSemaphore releaseEntry;
        std::atomic_bool blocked{false};
        MediaScanner s;
        s.setCacheDir(cache.path());
        QMutex phaseMutex;
        QStringList phases;
        s.setCheckpointHookForTest([&](const char* phase) {
            QMutexLocker lock(&phaseMutex);
            phases.append(QString::fromLatin1(phase));
        });
        QCOMPARE(runScan(s, fixtures()).size(), 5);
        for (const QString& expected : {QStringLiteral("root"), QStringLiteral("cache"),
                                        QStringLiteral("traversal"), QStringLiteral("entry"),
                                        QStringLiteral("tags"),
                                        QStringLiteral("art"), QStringLiteral("rewrite")})
            QVERIFY2(phases.contains(expected), qPrintable(expected));

        s.setCheckpointHookForTest([&](const char* phase) {
            if (qstrcmp(phase, "entry") != 0 || blocked.exchange(true)) return;
            enteredEntry.release();
            releaseEntry.acquire();
        });
        QSignalSpy done(&s, &MediaScanner::finished);
        s.scan({rootFor(fixtures())});
        if (!enteredEntry.tryAcquire(1, 15000)) {
            releaseEntry.release();
            s.stop();
            QFAIL("scanner never reached directory-entry phase");
        }
        std::thread unblocker([&] {
            QThread::msleep(50);
            releaseEntry.release();
        });
        QElapsedTimer elapsed;
        elapsed.start();
        s.stop();
        unblocker.join();
        QVERIFY2(elapsed.elapsed() < 2000, "in-flight checkpoint stop was not bounded");
        QCOMPARE(done.count(), 0);
        QVERIFY(!s.busy());
        QVERIFY(!s.scanning());
    }
};

QTEST_MAIN(TestMediaScanner)
#include "test_media_scanner.moc"
