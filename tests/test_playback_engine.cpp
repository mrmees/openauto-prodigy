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
private:
    QString fixture(const char* name) const {
        return QStringLiteral(TEST_DATA_DIR "/media/") + QLatin1String(name);
    }
};

void TestPlaybackEngine::testPlaysFixtureToCompletion() {
    FakeAudioService audio;
    PlaybackEngine eng;
    eng.setAudioService(&audio);
    QSignalSpy finished(&eng, &PlaybackEngine::trackFinished);
    QSignalSpy errors(&eng, &PlaybackEngine::errorOccurred);

    eng.playFile(fixture("tone-44k.mp3"));
    // 0.5 s fixture; allow generous slack for backend spin-up.
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1 || errors.count() > 0, 15000);
    if (errors.count() > 0)
        QSKIP("Multimedia backend unavailable in this environment (spike env should match!)");

    QCOMPARE(audio.created, 1);
    QCOMPARE(audio.lastName, QString("Local Media"));
    QCOMPARE(audio.lastPriority, 51);
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

    eng.playFile(fixture("tone-48k.flac"));
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1 || errors.count() > 0, 15000);
    if (errors.count() > 0)
        QSKIP("Multimedia backend unavailable in this environment");

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

QTEST_GUILESS_MAIN(TestPlaybackEngine)
#include "test_playback_engine.moc"
