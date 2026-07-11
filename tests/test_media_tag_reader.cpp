#include <QtTest>
#include "plugins/media_player/MediaTagReader.hpp"

using namespace oap::plugins;

class TestMediaTagReader : public QObject {
    Q_OBJECT
    const QString root = QStringLiteral(TEST_DATA_DIR "/media/library");
private slots:
    void readsFullTags() {
        const auto t = MediaTagReader::read(root + "/AlbumA/01-song-one.mp3");
        QVERIFY(t.valid);
        QCOMPARE(t.title, QStringLiteral("Song One"));
        QCOMPARE(t.artist, QStringLiteral("The Band"));
        QCOMPARE(t.albumArtist, QStringLiteral("The Band"));
        QCOMPARE(t.album, QStringLiteral("First Album"));
        QCOMPARE(t.genre, QStringLiteral("Rock"));
        QCOMPARE(t.year, 2001);
        QCOMPARE(t.trackNo, 1);      // parsed from "1/2"
        QCOMPARE(t.discNo, 1);
        QVERIFY(t.durationMs > 300 && t.durationMs < 1500);
        QVERIFY(t.hasEmbeddedArt);
    }
    void parsesBareTrackNumber() {
        const auto t = MediaTagReader::read(root + "/Comp/comp-one.flac");
        QVERIFY(t.valid);
        QCOMPARE(t.trackNo, 1);
        QVERIFY(t.albumArtist.isEmpty());
        QVERIFY(t.hasEmbeddedArt);   // fixture carries art for the VA test
    }
    void taglessFileFallsBackToFilenameStem() {
        const auto t = MediaTagReader::read(root + "/no-tags-here.ogg");
        QVERIFY(t.valid);
        QCOMPARE(t.title, QStringLiteral("no-tags-here"));  // §8 amendment #2
        QVERIFY(t.artist.isEmpty());
        QVERIFY(!t.hasEmbeddedArt);
    }
    void nonAudioIsInvalid() {
        QVERIFY(!MediaTagReader::read(root + "/notes.txt").valid);
        QVERIFY(!MediaTagReader::read(root + "/does-not-exist.mp3").valid);
    }
    void extractsEmbeddedArt() {
        const QByteArray art = MediaTagReader::embeddedArt(root + "/AlbumA/01-song-one.mp3");
        QVERIFY(!art.isEmpty());
        QVERIFY(!QImage::fromData(art).isNull());
        QVERIFY(MediaTagReader::embeddedArt(root + "/AlbumA/02-song-two.mp3").isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestMediaTagReader)
#include "test_media_tag_reader.moc"
