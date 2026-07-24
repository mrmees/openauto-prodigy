// Integration-ish smoke test: decodes real fixture files through QMediaPlayer's
// ffmpeg backend with NO audio device (QAudioBufferOutput tap only), verifying
// PCM lands in a fake IAudioService, metadata is read, and EOM auto-advances.
#include <QtTest/QtTest>
#include "plugins/media_player/PlaybackEngine.hpp"
#include "core/services/IAudioService.hpp"
#include "core/services/AudioService.hpp"  // AudioStreamHandle definition

using oap::plugins::PlaybackEngine;

class FakeAudioService : public oap::IAudioService {
public:
    oap::AudioStreamHandle* createStream(const QString& name, int priority,
                                         int sampleRate, int channels,
                                         const QString&, int) override {
        lastName = name; lastPriority = priority;
        lastRate = sampleRate; lastChannels = channels;
        ++created;
        return &handle;
    }
    void destroyStream(oap::AudioStreamHandle*) override { ++destroyed; }
    int writeAudio(oap::AudioStreamHandle*, const uint8_t*, int size) override {
        bytesWritten += size;
        return size;
    }
    void setMasterVolume(int) override {}
    int masterVolume() const override { return 100; }
    void requestAudioFocus(oap::AudioStreamHandle*, oap::AudioFocusType t) override {
        ++focusRequests; lastFocusType = t;
    }
    void releaseAudioFocus(oap::AudioStreamHandle*) override { ++focusReleases; }
    void setOutputDevice(const QString&) override {}
    void setInputDevice(const QString&) override {}
    QString outputDevice() const override { return "auto"; }
    QString inputDevice() const override { return "auto"; }

    oap::AudioStreamHandle handle;
    QString lastName;
    int lastPriority = 0, lastRate = 0, lastChannels = 0;
    int created = 0, destroyed = 0, focusRequests = 0, focusReleases = 0;
    qint64 bytesWritten = 0;
    oap::AudioFocusType lastFocusType = oap::AudioFocusType::Gain;
};

class TestPlaybackEngine : public QObject {
    Q_OBJECT
private slots:
    void testPlaysFixtureToCompletion();
    void testMetadataAndState();
    void testErrorOnGarbagePath();
    void testReleaseAudioResourcesIdempotent();
private:
    QString fixture(const char* name) const {
        return QStringLiteral(TEST_DATA_DIR "/media/") + QLatin1String(name);
    }
    QString fixtureError(const QString& path, const QSignalSpy& errors) const {
        QStringList messages;
        for (const auto& arguments : errors)
            messages.append(arguments.value(0).toString());
        return QStringLiteral("fixture=%1; PlaybackEngine errors: %2")
            .arg(path, messages.join(QStringLiteral(" | ")));
    }
};

void TestPlaybackEngine::testPlaysFixtureToCompletion() {
    FakeAudioService audio;
    PlaybackEngine eng;
    eng.setAudioService(&audio);
    QSignalSpy finished(&eng, &PlaybackEngine::trackFinished);
    QSignalSpy errors(&eng, &PlaybackEngine::errorOccurred);

    const QString path = fixture("tone-44k.mp3");
    eng.playFile(path);
    // 0.5 s fixture; allow generous slack for backend spin-up.
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1 || errors.count() > 0, 15000);
    QVERIFY2(errors.isEmpty(), qPrintable(fixtureError(path, errors)));

    QCOMPARE(audio.created, 1);
    QCOMPARE(audio.lastName, QString("Local Media"));
    // Priority 50: all music sources share one class; focus recency breaks ties.
    QCOMPARE(audio.lastPriority, 50);
    QCOMPARE(audio.lastRate, 48000);
    QCOMPARE(audio.lastChannels, 2);
    // 0.5 s @ 48kHz stereo S16 = 96000 bytes; tolerate codec padding/trim.
    QVERIFY2(audio.bytesWritten > 96000 * 0.5 && audio.bytesWritten < 96000 * 2.0,
             qPrintable(QString("bytesWritten=%1").arg(audio.bytesWritten)));
    QVERIFY(audio.focusRequests >= 1);
    QVERIFY(audio.focusReleases >= 1);   // released on EOM/stop
}

void TestPlaybackEngine::testMetadataAndState() {
    FakeAudioService audio;
    PlaybackEngine eng;
    eng.setAudioService(&audio);
    QSignalSpy meta(&eng, &PlaybackEngine::metadataChanged);
    QSignalSpy finished(&eng, &PlaybackEngine::trackFinished);
    QSignalSpy errors(&eng, &PlaybackEngine::errorOccurred);

    const QString path = fixture("tone-48k.flac");
    eng.playFile(path);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1 || errors.count() > 0, 15000);
    QVERIFY2(errors.isEmpty(), qPrintable(fixtureError(path, errors)));

    QVERIFY(meta.count() >= 1);
    QCOMPARE(eng.title(), QString("Tone 48"));
    QCOMPARE(eng.artist(), QString("Fixture Artist"));
    QVERIFY(eng.duration() >= 400 && eng.duration() <= 700);  // ~500 ms
    QCOMPARE(eng.playbackState(), 0);   // stopped after EOM
}

void TestPlaybackEngine::testErrorOnGarbagePath() {
    FakeAudioService audio;
    PlaybackEngine eng;
    eng.setAudioService(&audio);
    QSignalSpy errors(&eng, &PlaybackEngine::errorOccurred);
    eng.playFile("/nonexistent/nope.mp3");
    QTRY_VERIFY_WITH_TIMEOUT(errors.count() >= 1, 10000);
}

// Fix (gate re-run 2026-07-09): plugin shutdown() calls releaseAudioResources()
// to free the AudioService stream BEFORE the service (an earlier app child) is
// destroyed. The destructor then calls the same method — it must be idempotent
// (stream_ nulled) so the post-shutdown ~PlaybackEngine does not double-release
// / use-after-free.
void TestPlaybackEngine::testReleaseAudioResourcesIdempotent() {
    FakeAudioService audio;
    PlaybackEngine eng;
    eng.setAudioService(&audio);
    QSignalSpy errors(&eng, &PlaybackEngine::errorOccurred);

    const QString path = fixture("tone-44k.mp3");
    eng.playFile(path);
    // Wait until a stream actually exists (audio began flowing).
    QTRY_VERIFY_WITH_TIMEOUT(audio.created == 1 || errors.count() > 0, 15000);
    QVERIFY2(errors.isEmpty(), qPrintable(fixtureError(path, errors)));

    QCOMPARE(audio.destroyed, 0);            // stream lives during playback
    eng.releaseAudioResources();             // shutdown() path
    QCOMPARE(audio.destroyed, 1);            // released exactly once
    const int releasesAfterFirst = audio.focusReleases;

    eng.releaseAudioResources();             // simulates later ~PlaybackEngine
    QCOMPARE(audio.destroyed, 1);            // idempotent: no second destroy
    QCOMPARE(audio.focusReleases, releasesAfterFirst);  // no re-release on null stream
}

QTEST_GUILESS_MAIN(TestPlaybackEngine)
#include "test_playback_engine.moc"
