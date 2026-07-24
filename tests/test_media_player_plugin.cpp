#include <QtTest>

#include <QFile>
#include <QHash>
#include <QTemporaryDir>

#include "core/plugin/HostContext.hpp"
#include "core/services/IConfigService.hpp"
#include "plugins/media_player/MediaPlayerPlugin.hpp"

using oap::plugins::MediaPlayerPlugin;

namespace {
const QString kPluginId = QStringLiteral("org.openauto.media-player");

class FakeConfig final : public oap::IConfigService {
public:
    QVariant value(const QString& key) const override { return top.value(key); }
    void setValue(const QString& key, const QVariant& value) override { top[key] = value; }
    QVariant pluginValue(const QString& pluginId, const QString& key) const override {
        return plugin.value(pluginId + QLatin1Char('/') + key);
    }
    void setPluginValue(const QString& pluginId, const QString& key,
                        const QVariant& value) override {
        plugin[pluginId + QLatin1Char('/') + key] = value;
        writes.append(key);
    }
    void save() override { ++saveCount; }

    void seed(const QString& key, const QVariant& value) {
        plugin[kPluginId + QLatin1Char('/') + key] = value;
    }
    QVariant media(const QString& key) const { return pluginValue(kPluginId, key); }

    QHash<QString, QVariant> top;
    QHash<QString, QVariant> plugin;
    QStringList writes;
    int saveCount = 0;
};

QString fixture(const char* name) {
    return QStringLiteral(TEST_DATA_DIR "/media/") + QLatin1String(name);
}

void seedRestore(FakeConfig& cfg, const QStringList& queue, int index, qint64 position,
                 const QString& musicDir) {
    cfg.seed(QStringLiteral("music_dirs"), QStringList{musicDir});
    cfg.seed(QStringLiteral("last_queue"), queue);
    cfg.seed(QStringLiteral("last_index"), index);
    cfg.seed(QStringLiteral("last_position_ms"), position);
    cfg.seed(QStringLiteral("shuffle"), false);
    cfg.seed(QStringLiteral("repeat_mode"), 0);
}

bool deliverMount(MediaPlayerPlugin& plugin, const QString& mount) {
    return QMetaObject::invokeMethod(&plugin, "onVolumeMounted", Qt::DirectConnection,
                                     Q_ARG(QString, mount), Q_ARG(QString, QStringLiteral("test")),
                                     Q_ARG(QString, QStringLiteral("test-uuid")));
}
} // namespace

class TestMediaPlayerPlugin : public QObject {
    Q_OBJECT

private slots:
    void fallbackUsesZeroAndExactTrackUsesSavedPosition();
    void restoreErrorRemainsRetryableUntilUserTakeover();
    void seekTakesOwnershipAndPersistsRequestedPosition();
    void transportActionsTakePendingRestoreOwnership_data();
    void transportActionsTakePendingRestoreOwnership();
    void modesPersistWithoutSerializingPartialQueue();
    void shutdownQuiescesScanner();
};

void TestMediaPlayerPlugin::fallbackUsesZeroAndExactTrackUsesSavedPosition() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fallback = dir.path() + QStringLiteral("/fallback.mp3");
    QVERIFY(QFile::copy(fixture("tone-44k.mp3"), fallback));
    const QString missing = dir.path() + QStringLiteral("/missing.flac");

    {
        FakeConfig cfg;
        seedRestore(cfg, {fallback, missing}, 1, 250, dir.path());
        oap::HostContext host;
        host.setConfigService(&cfg);
        MediaPlayerPlugin plugin;
        QVERIFY(plugin.initialize(&host));
        QTRY_COMPARE_WITH_TIMEOUT(plugin.trackTitle(), QStringLiteral("Tone 44"), 10000);
        QCOMPARE(plugin.trackPosition(), qint64(0));
        QVERIFY2(cfg.writes.isEmpty(), qPrintable(cfg.writes.join(',')));
        plugin.shutdown();
    }

    {
        FakeConfig cfg;
        seedRestore(cfg, {fallback}, 0, 250, dir.path());
        oap::HostContext host;
        host.setConfigService(&cfg);
        MediaPlayerPlugin plugin;
        QVERIFY(plugin.initialize(&host));
        QTRY_COMPARE_WITH_TIMEOUT(plugin.trackTitle(), QStringLiteral("Tone 44"), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(plugin.trackPosition() - qint64(250)) <= 5, 10000);
        plugin.shutdown();
    }
}

void TestMediaPlayerPlugin::restoreErrorRemainsRetryableUntilUserTakeover() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fallback = dir.path() + QStringLiteral("/fallback.mp3");
    QVERIFY(QFile::copy(fixture("tone-44k.mp3"), fallback));
    const QString savedCurrent = dir.path() + QStringLiteral("/saved-current.flac");

    FakeConfig cfg;
    seedRestore(cfg, {fallback, savedCurrent}, 1, 200, dir.path());
    oap::HostContext host;
    host.setConfigService(&cfg);
    MediaPlayerPlugin plugin;
    QVERIFY(plugin.initialize(&host));
    QTRY_COMPARE_WITH_TIMEOUT(plugin.trackTitle(), QStringLiteral("Tone 44"), 10000);

    // Drive the production restore-error handler. It must enforce no-autoplay
    // without discarding the independently pending exact-track restore.
    QVERIFY(QMetaObject::invokeMethod(&plugin, "handleUnplayable", Qt::DirectConnection,
                                      Q_ARG(QString, QStringLiteral("test restore error"))));
    QVERIFY(QFile::copy(fixture("tone-48k.flac"), savedCurrent));
    QVERIFY(deliverMount(plugin, dir.path()));
    QTRY_COMPARE_WITH_TIMEOUT(plugin.trackTitle(), QStringLiteral("Tone 48"), 10000);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(plugin.trackPosition() - qint64(200)) <= 5, 10000);
    plugin.shutdown();
}

void TestMediaPlayerPlugin::seekTakesOwnershipAndPersistsRequestedPosition() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fallback = dir.path() + QStringLiteral("/fallback.mp3");
    QVERIFY(QFile::copy(fixture("tone-44k.mp3"), fallback));
    const QString savedCurrent = dir.path() + QStringLiteral("/saved-current.flac");

    FakeConfig cfg;
    seedRestore(cfg, {fallback, savedCurrent}, 1, 300, dir.path());
    oap::HostContext host;
    host.setConfigService(&cfg);
    MediaPlayerPlugin plugin;
    QVERIFY(plugin.initialize(&host));
    QTRY_COMPARE_WITH_TIMEOUT(plugin.trackTitle(), QStringLiteral("Tone 44"), 10000);
    QTRY_VERIFY_WITH_TIMEOUT(plugin.trackDuration() > 0, 10000);

    cfg.writes.clear();
    plugin.seekTo(175);
    QCOMPARE(cfg.media(QStringLiteral("last_position_ms")).toLongLong(), qint64(175));
    QCOMPARE(cfg.media(QStringLiteral("last_queue")).toStringList(), QStringList{fallback});
    QVERIFY(cfg.saveCount > 0);

    QVERIFY(QFile::copy(fixture("tone-48k.flac"), savedCurrent));
    QVERIFY(deliverMount(plugin, dir.path()));
    QTest::qWait(300);
    QCOMPARE(plugin.trackTitle(), QStringLiteral("Tone 44"));
    plugin.shutdown();
}

void TestMediaPlayerPlugin::transportActionsTakePendingRestoreOwnership_data() {
    QTest::addColumn<QString>("method");
    QTest::newRow("play-pause") << QStringLiteral("playPause");
    QTest::newRow("next") << QStringLiteral("next");
    QTest::newRow("previous") << QStringLiteral("previous");
}

void TestMediaPlayerPlugin::transportActionsTakePendingRestoreOwnership() {
    QFETCH(QString, method);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fallback = dir.path() + QStringLiteral("/fallback.mp3");
    QVERIFY(QFile::copy(fixture("tone-44k.mp3"), fallback));
    const QString savedCurrent = dir.path() + QStringLiteral("/saved-current.flac");

    FakeConfig cfg;
    seedRestore(cfg, {fallback, savedCurrent}, 1, 100, dir.path());
    oap::HostContext host;
    host.setConfigService(&cfg);
    MediaPlayerPlugin plugin;
    QVERIFY(plugin.initialize(&host));
    QTRY_COMPARE_WITH_TIMEOUT(plugin.trackTitle(), QStringLiteral("Tone 44"), 10000);

    QVERIFY(QMetaObject::invokeMethod(&plugin, method.toLatin1().constData(), Qt::DirectConnection));
    QVERIFY(QFile::copy(fixture("tone-48k.flac"), savedCurrent));
    QVERIFY(deliverMount(plugin, dir.path()));
    QTest::qWait(300);
    QCOMPARE(plugin.trackTitle(), QStringLiteral("Tone 44"));
    plugin.shutdown();
}

void TestMediaPlayerPlugin::modesPersistWithoutSerializingPartialQueue() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fallback = dir.path() + QStringLiteral("/fallback.mp3");
    QVERIFY(QFile::copy(fixture("tone-44k.mp3"), fallback));
    const QString missing = dir.path() + QStringLiteral("/missing.flac");
    const QStringList rawQueue{fallback, missing};

    FakeConfig cfg;
    seedRestore(cfg, rawQueue, 1, 200, dir.path());
    oap::HostContext host;
    host.setConfigService(&cfg);
    MediaPlayerPlugin plugin;
    QVERIFY(plugin.initialize(&host));
    QTRY_COMPARE_WITH_TIMEOUT(plugin.trackTitle(), QStringLiteral("Tone 44"), 10000);
    QVERIFY2(cfg.writes.isEmpty(), qPrintable(cfg.writes.join(',')));

    plugin.toggleShuffle();
    QCOMPARE(cfg.media(QStringLiteral("shuffle")).toBool(), true);
    QCOMPARE(cfg.media(QStringLiteral("last_queue")).toStringList(), rawQueue);
    QVERIFY(!cfg.writes.contains(QStringLiteral("last_queue")));
    const int savesAfterShuffle = cfg.saveCount;

    cfg.writes.clear();
    plugin.cycleRepeat();
    QCOMPARE(cfg.media(QStringLiteral("repeat_mode")).toInt(), 1);
    QCOMPARE(cfg.media(QStringLiteral("last_queue")).toStringList(), rawQueue);
    QVERIFY(!cfg.writes.contains(QStringLiteral("last_queue")));
    QCOMPARE(cfg.saveCount, savesAfterShuffle + 1);
    plugin.shutdown();
}

void TestMediaPlayerPlugin::shutdownQuiescesScanner() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QFile::copy(fixture("tone-44k.mp3"),
                        dir.path() + QStringLiteral("/shutdown-scan.mp3")));

    FakeConfig cfg;
    cfg.seed(QStringLiteral("music_dirs"), QStringList{dir.path()});
    oap::HostContext host;
    host.setConfigService(&cfg);
    MediaPlayerPlugin plugin;
    QVERIFY(plugin.initialize(&host));
    // initialize() starts a generation before returning; no event-loop turn
    // has delivered completion yet.
    QVERIFY(plugin.libraryScanning());
    plugin.shutdown();
    QVERIFY(!plugin.libraryScanning());
}

QTEST_GUILESS_MAIN(TestMediaPlayerPlugin)
#include "test_media_player_plugin.moc"
