#include <QtTest>
#include <QRegularExpression>

#include "core/api/ApiSerializers.hpp"
#include "core/services/MediaStatusService.hpp"
#include "core/services/ProjectionStatusProvider.hpp"
#include "core/services/ThemeService.hpp"
#include "core/services/BluetoothManager.hpp"
#include "core/services/IConfigService.hpp"

namespace pb = prodigy::api::v1;
using namespace oap::api::serial;

// Minimal mock ConfigService, same shape as test_bluetooth_manager.cpp, so we
// can construct a real (adapter-less) BluetoothManager in this test env.
class MockConfigService : public oap::IConfigService {
public:
    QVariant value(const QString& path) const override { return values_.value(path); }
    void setValue(const QString& path, const QVariant& value) override { values_[path] = value; }
    void save() override {}
    QVariant pluginValue(const QString&, const QString&) const override { return {}; }
    void setPluginValue(const QString&, const QString&, const QVariant&) override {}

    QMap<QString, QVariant> values_;
};

class TestApiSerializers : public QObject {
    Q_OBJECT
private slots:
    void testMediaBluetoothPlaying();
    void testMediaAaTrap();
    void testMediaNoneSource();
    void testProjectionProjecting();
    void testProjectionUnknownRawDefaultsUnspecified();
    void testSystemThemeTokensAndVersion();
    void testSystemBluetoothNullptr();
    void testSystemBluetoothDisconnectedRealManager();
};

void TestApiSerializers::testMediaBluetoothPlaying() {
    oap::MediaStatusService media;
    media.setBtConnected(true);
    media.updateBtMetadata("T", "A", "Al");
    media.updateBtPlaybackState(1);  // BT raw 1 = Playing

    pb::MediaStatus status = buildMediaStatus(media);
    QCOMPARE(QString::fromStdString(status.title()), QString("T"));
    QCOMPARE(QString::fromStdString(status.artist()), QString("A"));
    QCOMPARE(QString::fromStdString(status.album()), QString("Al"));
    QVERIFY(status.has_media());
    QCOMPARE(status.source(), pb::MEDIA_SOURCE_BLUETOOTH);
    QCOMPARE(status.playback_state(), pb::PLAYBACK_STATE_PLAYING);

    // Other BT raw codes, same source, per the normalization table.
    media.updateBtPlaybackState(0);
    QCOMPARE(buildMediaStatus(media).playback_state(), pb::PLAYBACK_STATE_STOPPED);
    media.updateBtPlaybackState(2);
    QCOMPARE(buildMediaStatus(media).playback_state(), pb::PLAYBACK_STATE_PAUSED);
    media.updateBtPlaybackState(99);  // out-of-table raw -> UNSPECIFIED
    QCOMPARE(buildMediaStatus(media).playback_state(), pb::PLAYBACK_STATE_UNSPECIFIED);
}

void TestApiSerializers::testMediaAaTrap() {
    // THE TRAP: AndroidAuto raw 1 means STOPPED, not PLAYING -- the exact
    // opposite of what the Bluetooth table says for the same raw int. If the
    // serializer ever normalizes playback state without branching on source
    // first, this is the assertion that catches it.
    oap::MediaStatusService media;
    media.setAaConnected(true);
    media.updateAaPlaybackState(1);

    pb::MediaStatus status = buildMediaStatus(media);
    QCOMPARE(status.source(), pb::MEDIA_SOURCE_ANDROID_AUTO);
    QCOMPARE(status.playback_state(), pb::PLAYBACK_STATE_STOPPED);

    media.updateAaPlaybackState(2);
    QCOMPARE(buildMediaStatus(media).playback_state(), pb::PLAYBACK_STATE_PLAYING);
    media.updateAaPlaybackState(3);
    QCOMPARE(buildMediaStatus(media).playback_state(), pb::PLAYBACK_STATE_PAUSED);
    media.updateAaPlaybackState(99);  // out-of-table raw -> UNSPECIFIED
    QCOMPARE(buildMediaStatus(media).playback_state(), pb::PLAYBACK_STATE_UNSPECIFIED);
}

void TestApiSerializers::testMediaNoneSource() {
    // No source connected: source() is "" -> MEDIA_SOURCE_NONE, and per the
    // table's last row ("source NONE | any -> UNSPECIFIED") the playback
    // state is UNSPECIFIED regardless of the raw int.
    oap::MediaStatusService media;

    pb::MediaStatus status = buildMediaStatus(media);
    QCOMPARE(status.source(), pb::MEDIA_SOURCE_NONE);
    QCOMPARE(status.playback_state(), pb::PLAYBACK_STATE_UNSPECIFIED);
    QVERIFY(!status.has_media());
}

void TestApiSerializers::testProjectionProjecting() {
    QObject src;
    src.setProperty("connectionState", 3);
    src.setProperty("statusMessage", "ok");
    oap::ProjectionStatusProvider provider(&src);

    pb::ProjectionStatus status = buildProjectionStatus(provider);
    QCOMPARE(status.state(), pb::PROJECTION_STATE_PROJECTING);
    QCOMPARE(QString::fromStdString(status.status_message()), QString("ok"));
}

void TestApiSerializers::testProjectionUnknownRawDefaultsUnspecified() {
    QObject src;
    src.setProperty("connectionState", 0);
    src.setProperty("statusMessage", "");
    oap::ProjectionStatusProvider disconnected(&src);
    QCOMPARE(buildProjectionStatus(disconnected).state(), pb::PROJECTION_STATE_DISCONNECTED);

    src.setProperty("connectionState", 1);
    QCOMPARE(buildProjectionStatus(disconnected).state(), pb::PROJECTION_STATE_WAITING_FOR_DEVICE);
    src.setProperty("connectionState", 2);
    QCOMPARE(buildProjectionStatus(disconnected).state(), pb::PROJECTION_STATE_CONNECTING);
    src.setProperty("connectionState", 4);
    QCOMPARE(buildProjectionStatus(disconnected).state(), pb::PROJECTION_STATE_BACKGROUNDED);
    src.setProperty("connectionState", 42);  // out-of-table raw -> UNSPECIFIED, never static_cast
    QCOMPARE(buildProjectionStatus(disconnected).state(), pb::PROJECTION_STATE_UNSPECIFIED);
}

void TestApiSerializers::testSystemThemeTokensAndVersion() {
    oap::ThemeService theme;
    theme.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

    pb::SystemStatus status = buildSystemStatus(theme, "1.0.0 (deadbeef)", nullptr);

    QCOMPARE(status.night_mode(), theme.realNightMode());
    QCOMPARE(QString::fromStdString(status.theme_id()), theme.currentThemeId());
    QCOMPARE(QString::fromStdString(status.app_version()), QString("1.0.0 (deadbeef)"));

    QCOMPARE(status.theme_tokens_size(), 42);
    QRegularExpression hexColor("^#[0-9a-f]{6}$");
    for (const auto& kv : status.theme_tokens()) {
        QVERIFY2(hexColor.match(QString::fromStdString(kv.second)).hasMatch(),
                 qPrintable(QString("token '%1' = '%2' is not #rrggbb")
                                .arg(QString::fromStdString(kv.first))
                                .arg(QString::fromStdString(kv.second))));
    }

    // These six tokens are derived getters, not YAML-backed colors -- assert
    // against the getters' own .name() (not hardcoded hex) so a regression in
    // activeColor()'s routing can't hide behind the generic hex-format check
    // above. Compare as strings, not QColor objects: surface-tint-high/highest
    // are computed blends via fromRgbF, whose 16-bit-per-channel rounding
    // doesn't necessarily match a QColor parsed back from an 8-bit hex
    // string bit-for-bit, even when both display the same #rrggbb.
    auto tokenName = [&status](const char* name) {
        return QString::fromStdString(status.theme_tokens().at(name));
    };
    QCOMPARE(tokenName("success"), theme.success().name());
    QCOMPARE(tokenName("on-success"), theme.onSuccess().name());
    QCOMPARE(tokenName("warning"), theme.warning().name());
    QCOMPARE(tokenName("on-warning"), theme.onWarning().name());
    QCOMPARE(tokenName("surface-tint-high"), theme.surfaceTintHigh().name());
    QCOMPARE(tokenName("surface-tint-highest"), theme.surfaceTintHighest().name());
}

void TestApiSerializers::testSystemBluetoothNullptr() {
    oap::ThemeService theme;
    theme.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

    pb::SystemStatus status = buildSystemStatus(theme, "1.0.0 (deadbeef)", nullptr);
    QVERIFY(!status.bluetooth().connected());
    QVERIFY(status.bluetooth().device_name().empty());
}

void TestApiSerializers::testSystemBluetoothDisconnectedRealManager() {
    oap::ThemeService theme;
    theme.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

    MockConfigService config;
    oap::BluetoothManager bt(&config);  // no adapter in test env -> disconnected

    pb::SystemStatus status = buildSystemStatus(theme, "1.0.0 (deadbeef)", &bt);
    QVERIFY(!status.bluetooth().connected());
    QVERIFY(status.bluetooth().device_name().empty());
}

QTEST_MAIN(TestApiSerializers)
#include "test_api_serializers.moc"
