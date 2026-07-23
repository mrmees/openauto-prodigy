#include <QTest>
#include <QCoreApplication>
#include <QSignalSpy>
#include "core/services/AudioService.hpp"
#include <algorithm>
#include <climits>
#include <cstring>
#include <memory>

namespace {
QStringList capturedMessages;
void captureMessages(QtMsgType, const QMessageLogContext&, const QString& message)
{
    capturedMessages.append(message);
}
}

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

    void playbackRingCapacityIsBoundedAndTotal()
    {
        int normalized = -1;
        QCOMPARE(oap::AudioService::playbackRingCapacityBytes(
                     48000, 2, 100, &normalized), 131072u);
        QCOMPARE(normalized, 500);

        QCOMPARE(oap::AudioService::playbackRingCapacityBytes(
                     48000, 2, 5000, &normalized), 1048576u);
        QCOMPARE(normalized, 5000);

        QCOMPARE(oap::AudioService::playbackRingCapacityBytes(
                     48000, 2, INT_MAX, &normalized), 1048576u);
        QCOMPARE(normalized, 5000);

        QCOMPARE(oap::AudioService::playbackRingCapacityBytes(
                     0, 2, 500, &normalized), 0u);
        QCOMPARE(normalized, 0);
        QCOMPARE(oap::AudioService::playbackRingCapacityBytes(
                     INT_MAX, 2, 500, &normalized), 0u);
        QCOMPARE(oap::AudioService::playbackRingCapacityBytes(
                     48000, 3, 500, &normalized), 0u);
    }

    void malformedPlaybackBuffersAreRejected()
    {
        oap::AudioStreamHandle handle;
        handle.bytesPerFrame = 4;
        handle.disableRateMatching = true;
        handle.ringBuffer = std::make_unique<oap::AudioRingBuffer>(1024);

        pw_buffer pw{};
        QVERIFY(!oap::AudioService::fillPlaybackBuffer(&handle, &pw));

        spa_buffer spa{};
        pw.buffer = &spa;
        QVERIFY(!oap::AudioService::fillPlaybackBuffer(&handle, &pw));

        spa_data data{};
        spa.n_datas = 1;
        spa.datas = &data;
        data.maxsize = 64;
        QVERIFY(!oap::AudioService::fillPlaybackBuffer(&handle, &pw));

        uint8_t memory[64];
        data.data = memory;
        QVERIFY(!oap::AudioService::fillPlaybackBuffer(&handle, &pw));

        spa_chunk chunk{};
        data.chunk = &chunk;
        handle.bytesPerFrame = 0;
        QVERIFY(!oap::AudioService::fillPlaybackBuffer(&handle, &pw));
    }

    void validPlaybackBufferIsFilledAndSilenced()
    {
        oap::AudioStreamHandle handle;
        handle.bytesPerFrame = 4;
        handle.channels = 2;
        handle.disableRateMatching = true;
        handle.ringBuffer = std::make_unique<oap::AudioRingBuffer>(1024);
        const uint8_t input[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        QCOMPARE(handle.ringBuffer->write(input, sizeof(input)), 8u);

        uint8_t memory[64];
        std::memset(memory, 0xCC, sizeof(memory));
        spa_chunk chunk{};
        spa_data data{};
        data.data = memory;
        data.maxsize = sizeof(memory);
        data.chunk = &chunk;
        spa_buffer spa{};
        spa.n_datas = 1;
        spa.datas = &data;
        pw_buffer pw{};
        pw.buffer = &spa;
        pw.requested = 4;

        QVERIFY(oap::AudioService::fillPlaybackBuffer(&handle, &pw));
        QCOMPARE(chunk.offset, 0u);
        QCOMPARE(chunk.stride, 4);
        QCOMPARE(chunk.size, 16u);
        QCOMPARE(pw.size, 4u);
        QCOMPARE(std::memcmp(memory, input, sizeof(input)), 0);
        for (int i = 8; i < 16; ++i)
            QCOMPARE(memory[i], static_cast<uint8_t>(0));
    }

    void streamErrorLogsWithoutRecoveryHookAndHookStillDispatches()
    {
        oap::AudioStreamHandle handle;
        handle.name = QStringLiteral("headless-test-stream");
        bool hookCalled = false;
        handle.onStreamError = [&hookCalled]() { hookCalled = true; };
        handle.errorContext = QCoreApplication::instance();

        capturedMessages.clear();
        QtMessageHandler previous = qInstallMessageHandler(captureMessages);
        oap::AudioService::onStreamStateChanged(
            &handle, PW_STREAM_STATE_STREAMING, PW_STREAM_STATE_ERROR,
            "synthetic failure");
        QCoreApplication::processEvents();
        qInstallMessageHandler(previous);

        QVERIFY(hookCalled);
        QVERIFY(std::any_of(capturedMessages.cbegin(), capturedMessages.cend(),
            [](const QString& message) {
                return message.contains("headless-test-stream")
                    && message.contains("synthetic failure");
            }));

        // Logging is independent from recovery-hook presence.
        handle.onStreamError = {};
        capturedMessages.clear();
        previous = qInstallMessageHandler(captureMessages);
        oap::AudioService::onStreamStateChanged(
            &handle, PW_STREAM_STATE_STREAMING, PW_STREAM_STATE_ERROR,
            "diagnostic only");
        QCoreApplication::processEvents();
        qInstallMessageHandler(previous);
        QVERIFY(std::any_of(capturedMessages.cbegin(), capturedMessages.cend(),
            [](const QString& message) {
                return message.contains("diagnostic only");
            }));
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

    // setCaptureCallback discriminates mutability by the explicit
    // captureCallbackImmutable flag, NOT by "a callback is already installed".
    // A legacy handle (no pre-connect callback) keeps replace/clear semantics
    // through its first set and beyond; an options-path handle is refused.
    // Operates directly on handles — no PipeWire daemon required.
    void testSetCaptureCallbackImmutabilityByFlag()
    {
        oap::AudioService svc;

        // --- Legacy handle: mutable set -> replace -> clear(nullptr). ---
        oap::AudioStreamHandle legacy;
        legacy.isCapture = true;
        QVERIFY(!legacy.captureCallbackImmutable);
        QVERIFY(!static_cast<bool>(legacy.captureCallback));

        int tag = 0;
        svc.setCaptureCallback(&legacy, [&tag](const uint8_t*, int){ tag = 1; });
        QVERIFY(static_cast<bool>(legacy.captureCallback));
        QVERIFY(legacy.captureCallbackActive.load());

        // Replace works — first set did NOT lock the legacy handle.
        svc.setCaptureCallback(&legacy, [&tag](const uint8_t*, int){ tag = 2; });
        QVERIFY(static_cast<bool>(legacy.captureCallback));
        QVERIFY(legacy.captureCallbackActive.load());

        // Clear via nullptr disables through the atomic guard.
        svc.setCaptureCallback(&legacy, nullptr);
        QVERIFY(!static_cast<bool>(legacy.captureCallback));
        QVERIFY(!legacy.captureCallbackActive.load());

        // --- Options-path handle: pre-connect callback => immutable => refused. ---
        oap::AudioStreamHandle opt;
        opt.isCapture = true;
        opt.captureCallback = [](const uint8_t*, int){};
        opt.captureCallbackImmutable = true;
        opt.captureCallbackActive.store(true);

        svc.setCaptureCallback(&opt, nullptr);              // refused — no clear
        QVERIFY(static_cast<bool>(opt.captureCallback));    // still installed
        QVERIFY(opt.captureCallbackActive.load());          // still active
    }

    void masterVolumeSignalOnlyOnChange()
    {
        oap::AudioService svc;
        QSignalSpy spy(&svc, &oap::AudioService::masterVolumeChanged);
        svc.setMasterVolume(50);
        QCOMPARE(spy.count(), 1);
        svc.setMasterVolume(50);           // same value — no signal
        QCOMPARE(spy.count(), 1);
        svc.setMasterVolume(200);          // clamps to 100 — one signal
        QCOMPARE(spy.count(), 2);
        svc.setMasterVolume(150);          // clamps to 100 again — no signal
        QCOMPARE(spy.count(), 2);
    }

    void masterVolumeGetterReentrantFromSignal()
    {
        // Regression: the no-PipeWire branch used to emit while holding
        // mutex_; a direct-connected slot reading masterVolume() deadlocked.
        // Only the !threadLoop_ branch had the bug — skip when a live
        // PipeWire daemon means that branch isn't the one under test
        // (file convention, and pre-fix this test would HANG there... on the
        // buggy branch; the skip keeps the red run bounded either way).
        oap::AudioService svc;
        if (svc.isAvailable())
            QSKIP("PipeWire daemon running — no-daemon branch not exercised");
        int seen = -1;
        QObject::connect(&svc, &oap::AudioService::masterVolumeChanged,
                         [&]() { seen = svc.masterVolume(); });
        svc.setMasterVolume(42);
        QCOMPARE(seen, 42);
    }
};

QTEST_MAIN(TestAudioService)
#include "test_audio_service.moc"
