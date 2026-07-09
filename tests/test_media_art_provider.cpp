#include <QtTest/QtTest>
#include <QImage>
#include "plugins/media_player/MediaArtProvider.hpp"

using oap::plugins::MediaArtProvider;

class TestMediaArtProvider : public QObject {
    Q_OBJECT
private slots:
    void testEmptyByDefault();
    void testSetArtBumpsRevisionAndServesImage();
    void testClearArt();
    void testRequestedSizeScalesButReportsOriginalSize();
};

void TestMediaArtProvider::testEmptyByDefault() {
    MediaArtProvider p;
    QCOMPARE(p.currentUrl(), QString());
    QSize size;
    const QImage img = p.requestImage("current/0", &size, QSize());
    QVERIFY(!img.isNull());          // placeholder pixel, never a null image
}

void TestMediaArtProvider::testSetArtBumpsRevisionAndServesImage() {
    MediaArtProvider p;
    QImage art(64, 64, QImage::Format_RGB32);
    art.fill(Qt::red);
    p.setCurrentArt(art);
    const QString url1 = p.currentUrl();
    QVERIFY(url1.startsWith("image://mediaart/current/"));
    QSize size;
    const QImage served = p.requestImage("current/1", &size, QSize());
    QCOMPARE(served.size(), QSize(64, 64));

    QImage art2(32, 32, QImage::Format_RGB32);
    art2.fill(Qt::blue);
    p.setCurrentArt(art2);
    QVERIFY(p.currentUrl() != url1);  // revision changed -> QML cache busted
}

void TestMediaArtProvider::testClearArt() {
    MediaArtProvider p;
    QImage art(8, 8, QImage::Format_RGB32);
    art.fill(Qt::green);
    p.setCurrentArt(art);
    QVERIFY(!p.currentUrl().isEmpty());
    p.setCurrentArt(QImage());
    QCOMPARE(p.currentUrl(), QString());
}

void TestMediaArtProvider::testRequestedSizeScalesButReportsOriginalSize() {
    MediaArtProvider p;
    QImage art(64, 32, QImage::Format_RGB32);
    art.fill(Qt::red);
    p.setCurrentArt(art);
    QSize size;
    const QImage served = p.requestImage("current/1", &size, QSize(32, 32));
    QCOMPARE(size, QSize(64, 32));             // original size per Qt contract
    QCOMPARE(served.size(), QSize(32, 16));    // scaled, KeepAspectRatio
}

QTEST_GUILESS_MAIN(TestMediaArtProvider)
#include "test_media_art_provider.moc"
