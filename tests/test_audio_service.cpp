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

    void testCubicVolumeCurve()
    {
        QCOMPARE(oap::AudioService::cubicVolume(0),   0.0f);
        QCOMPARE(oap::AudioService::cubicVolume(100), 1.0f);
        QVERIFY(qAbs(oap::AudioService::cubicVolume(50) - 0.125f) < 1e-6f);
    }

    void testOptionsFactoriesNullWithoutPipeWire()
    {
        // Only meaningful when PW is unavailable; skip otherwise.
        oap::AudioService svc;
        if (svc.isAvailable()) QSKIP("PipeWire available; factory paths bench-verified");
        oap::AudioService::PlaybackStreamOptions po; po.name = "T";
        QCOMPARE(svc.createStreamWithOptions(po), nullptr);
        oap::AudioService::CaptureStreamOptions co; co.name = "C";
        QCOMPARE(svc.openCaptureStreamWithOptions(co), nullptr);
        svc.setStreamActive(nullptr, true);   // must not crash
        svc.resetStreamRing(nullptr);         // must not crash
    }

    void testFocusRecencyBreaksTies()
    {
        oap::AudioStreamHandle a, b;
        a.priority = 50; a.hasFocus = true; a.focusSequence = 1;
        b.priority = 50; b.hasFocus = true; b.focusSequence = 2;
        QCOMPARE(oap::AudioService::selectDominant({&a, &b}), &b);
        a.focusSequence = 3;                    // a re-requests focus
        QCOMPARE(oap::AudioService::selectDominant({&a, &b}), &a);
    }
    void testHigherPriorityStillWinsRegardlessOfRecency()
    {
        oap::AudioStreamHandle music, speech;
        music.priority = 50;  music.hasFocus = true; music.focusSequence = 99;
        speech.priority = 60; speech.hasFocus = true; speech.focusSequence = 1;
        QCOMPARE(oap::AudioService::selectDominant({&music, &speech}), &speech);
    }
    void testNoFocusReturnsNull()
    {
        oap::AudioStreamHandle a; a.priority = 50; a.hasFocus = false;
        QCOMPARE(oap::AudioService::selectDominant({&a}), nullptr);
    }
};

QTEST_MAIN(TestAudioService)
#include "test_audio_service.moc"
