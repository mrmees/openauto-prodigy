// tests/test_media_status_service.cpp
#include <QtTest/QtTest>
#include "core/services/MediaStatusService.hpp"
#include "core/services/IMediaStatusProvider.hpp"

class TestMediaStatusService : public QObject {
    Q_OBJECT
private slots:
    void testImplementsIMediaStatusProvider();
    void testInitialStateIsEmpty();
    void testAASourceTakesPriority();
    void testFallbackToBTOnAADisconnect();
    void testPlaybackControlsDelegateToActiveSource();
    void testSignalEmittedOnMetadataChange();
    void testSourceMergingPriority();
};

void TestMediaStatusService::testImplementsIMediaStatusProvider() {
    oap::MediaStatusService service;
    oap::IMediaStatusProvider* provider = &service;
    QVERIFY(provider != nullptr);
    QCOMPARE(provider->source(), QString());
}

void TestMediaStatusService::testInitialStateIsEmpty() {
    oap::MediaStatusService service;
    QVERIFY(service.title().isEmpty());
    QVERIFY(service.artist().isEmpty());
    QVERIFY(service.album().isEmpty());
    QCOMPARE(service.playbackState(), 0);
}

void TestMediaStatusService::testAASourceTakesPriority() {
    oap::MediaStatusService service;

    // Set BT metadata first
    service.updateBtMetadata("BT Song", "BT Artist", "BT Album");
    service.setBtConnected(true);

    // Then AA connects and provides metadata — should take priority
    service.setAaConnected(true);
    service.updateAaMetadata("AA Song", "AA Artist", "AA Album");

    QCOMPARE(service.title(), "AA Song");
    QCOMPARE(service.artist(), "AA Artist");
    QCOMPARE(service.source(), "AndroidAuto");
}

void TestMediaStatusService::testFallbackToBTOnAADisconnect() {
    oap::MediaStatusService service;

    service.setBtConnected(true);
    service.updateBtMetadata("BT Song", "BT Artist", "BT Album");
    service.setAaConnected(true);
    service.updateAaMetadata("AA Song", "AA Artist", "AA Album");

    // Verify AA is active
    QCOMPARE(service.source(), "AndroidAuto");

    // Disconnect AA — should fall back to BT
    service.setAaConnected(false);
    QCOMPARE(service.source(), "Bluetooth");
    QCOMPARE(service.title(), "BT Song");
}

void TestMediaStatusService::testPlaybackControlsDelegateToActiveSource() {
    oap::MediaStatusService service;

    // Track which controls are called
    bool playCalled = false;
    bool nextCalled = false;
    bool prevCalled = false;

    service.setPlaybackCallbacks(
        [&playCalled]() { playCalled = true; },
        [&nextCalled]() { nextCalled = true; },
        [&prevCalled]() { prevCalled = true; }
    );

    service.playPause();
    service.next();
    service.previous();

    QVERIFY(playCalled);
    QVERIFY(nextCalled);
    QVERIFY(prevCalled);
}

void TestMediaStatusService::testSignalEmittedOnMetadataChange() {
    oap::MediaStatusService service;
    QSignalSpy spy(&service, &oap::IMediaStatusProvider::mediaStatusChanged);

    service.setBtConnected(true);
    service.updateBtMetadata("Song", "Artist", "Album");

    QVERIFY(spy.count() >= 1);
}

void TestMediaStatusService::testSourceMergingPriority() {
    oap::MediaStatusService service;

    // No source initially
    QCOMPARE(service.source(), QString());

    // BT connects
    service.setBtConnected(true);
    QCOMPARE(service.source(), "Bluetooth");

    // AA connects — takes priority
    service.setAaConnected(true);
    QCOMPARE(service.source(), "AndroidAuto");

    // AA disconnects — back to BT
    service.setAaConnected(false);
    QCOMPARE(service.source(), "Bluetooth");

    // BT disconnects — none
    service.setBtConnected(false);
    QCOMPARE(service.source(), QString());
}

QTEST_GUILESS_MAIN(TestMediaStatusService)
#include "test_media_status_service.moc"
