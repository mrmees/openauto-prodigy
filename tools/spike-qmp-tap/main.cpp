// Spike: does QMediaPlayer pace QAudioBufferOutput delivery in real time
// with NO QAudioOutput attached? Also checks: format conversion to
// 48kHz/S16/stereo, position()/duration() advance, seek, EndOfMedia.
//
// Usage: spike-qmp-tap <audiofile> [seekToMs]
// GO criteria: see plan Task 1 Step 4.
#include <QAudioBuffer>
#include <QAudioBufferOutput>
#include <QAudioFormat>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QTimer>
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) { fprintf(stderr, "usage: spike-qmp-tap <audiofile> [seekToMs]\n"); return 2; }
    const QString file = QString::fromLocal8Bit(argv[1]);
    const qint64 seekMs = argc > 2 ? atoll(argv[2]) : -1;

    QAudioFormat fmt;
    fmt.setSampleRate(48000);
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Int16);

    QMediaPlayer player;
    QAudioBufferOutput tap(fmt);
    player.setAudioBufferOutput(&tap);

    QElapsedTimer wall;
    wall.start();
    qint64 pcmBytes = 0;
    qint64 firstBufWallMs = -1;
    int bufCount = 0;
    bool formatOk = true;

    QObject::connect(&tap, &QAudioBufferOutput::audioBufferReceived,
                     [&](const QAudioBuffer& buf) {
        if (firstBufWallMs < 0) firstBufWallMs = wall.elapsed();
        pcmBytes += buf.byteCount();
        if (buf.format().sampleRate() != 48000 || buf.format().channelCount() != 2
            || buf.format().sampleFormat() != QAudioFormat::Int16) {
            formatOk = false;
            printf("FORMAT MISMATCH: got %d Hz / %d ch / fmt %d\n",
                   buf.format().sampleRate(), buf.format().channelCount(),
                   (int)buf.format().sampleFormat());
        }
        const double mediaS = double(pcmBytes) / (48000.0 * 2 * 2);
        const double wallS  = double(wall.elapsed() - firstBufWallMs) / 1000.0;
        if (++bufCount % 20 == 0)
            printf("buf#%-4d media=%6.2fs wall=%6.2fs ratio=%.2f playerPos=%lld ms\n",
                   bufCount, mediaS, wallS, wallS > 0.05 ? mediaS / wallS : 0.0,
                   (long long)player.position());
    });

    QObject::connect(&player, &QMediaPlayer::mediaStatusChanged,
                     [&](QMediaPlayer::MediaStatus s) {
        printf("mediaStatus=%d pos=%lld dur=%lld title='%s'\n", (int)s,
               (long long)player.position(), (long long)player.duration(),
               qPrintable(player.metaData().value(QMediaMetaData::Title).toString()));
        if (s == QMediaPlayer::EndOfMedia) {
            const double mediaS = double(pcmBytes) / (48000.0 * 2 * 2);
            const double wallS  = double(wall.elapsed() - firstBufWallMs) / 1000.0;
            printf("RESULT: pcm=%.2fs wall=%.2fs ratio=%.2f formatOk=%d bufs=%d\n",
                   mediaS, wallS, wallS > 0.05 ? mediaS / wallS : 0.0,
                   formatOk ? 1 : 0, bufCount);
            app.quit();
        }
    });

    QObject::connect(&player, &QMediaPlayer::errorOccurred,
                     [&](QMediaPlayer::Error e, const QString& msg) {
        fprintf(stderr, "ERROR %d: %s\n", (int)e, qPrintable(msg));
        app.exit(1);
    });

    player.setSource(QUrl::fromLocalFile(file));
    player.play();

    if (seekMs >= 0) {
        QTimer::singleShot(2000, &player, [&] {
            printf("SEEK -> %lld ms (pos before: %lld)\n",
                   (long long)seekMs, (long long)player.position());
            player.setPosition(seekMs);
        });
        QTimer::singleShot(3000, &player, [&] {
            printf("pos 1s after seek: %lld ms\n", (long long)player.position());
        });
    }

    QTimer::singleShot(90000, &app, [&] { fprintf(stderr, "TIMEOUT\n"); app.exit(3); });
    return app.exec();
}
