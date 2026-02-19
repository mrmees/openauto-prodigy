#include <QTest>
#include "core/services/AudioService.hpp"

/// Tests for AudioService.
/// These tests do NOT require a running PipeWire daemon.
/// They verify graceful degradation and API safety.
class TestAudioService : public QObject {
    Q_OBJECT

private slots:
    void constructionDoesNotCrash()
    {
        // Should succeed even without PipeWire daemon
        oap::AudioService service;
        // isAvailable() depends on whether PipeWire is running
        // We don't assert either way — just verify no crash
    }

    void createStreamReturnsNullWithoutDaemon()
    {
        oap::AudioService service;
        if (!service.isAvailable()) {
            // No PipeWire daemon — createStream should return nullptr gracefully
            auto* handle = service.createStream("test", 50);
            QVERIFY(handle == nullptr);
        } else {
            QSKIP("PipeWire daemon is running — skip no-daemon test");
        }
    }

    void destroyNullStreamIsSafe()
    {
        oap::AudioService service;
        // Should not crash
        service.destroyStream(nullptr);
    }

    void writeToNullHandleReturnsMinus1()
    {
        oap::AudioService service;
        uint8_t data[] = {0, 0, 0, 0};
        QCOMPARE(service.writeAudio(nullptr, data, 4), -1);
    }

    void masterVolumeDefaultAndSet()
    {
        oap::AudioService service;
        QCOMPARE(service.masterVolume(), 80);

        service.setMasterVolume(50);
        QCOMPARE(service.masterVolume(), 50);

        // Clamp to 0-100
        service.setMasterVolume(150);
        QCOMPARE(service.masterVolume(), 100);

        service.setMasterVolume(-10);
        QCOMPARE(service.masterVolume(), 0);
    }

    void focusRequestsAreSafe()
    {
        oap::AudioService service;
        // Should not crash even with nullptr
        service.requestAudioFocus(nullptr, oap::AudioFocusType::Gain);
        service.releaseAudioFocus(nullptr);
    }

    void createAndDestroyWithDaemon()
    {
        oap::AudioService service;
        if (!service.isAvailable()) {
            QSKIP("PipeWire daemon not available");
        }

        auto* handle = service.createStream("test-stream", 50);
        QVERIFY(handle != nullptr);
        QCOMPARE(handle->name, "test-stream");
        QCOMPARE(handle->priority, 50);

        service.destroyStream(handle);
    }
};

QTEST_MAIN(TestAudioService)
#include "test_audio_service.moc"
