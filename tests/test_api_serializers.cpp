#include <QtTest>
#include <QRegularExpression>

#include "core/api/ApiSerializers.hpp"
#include "core/services/MediaStatusService.hpp"
#include "core/services/ProjectionStatusProvider.hpp"
#include "core/services/ThemeService.hpp"
#include "core/services/BluetoothManager.hpp"
#include "core/services/IConfigService.hpp"
#include "core/services/PhoneStateService.hpp"
#include "core/services/INavigationProvider.hpp"

namespace pb = prodigy::api::v1;
using namespace oap::api::serial;

// ~15-line fake INavigationProvider: plain member fields, no signal wiring
// needed since buildNavigationStatus only reads the getters.
class FakeNavProvider : public oap::INavigationProvider {
public:
    bool navActive() const override { return navActive_; }
    QString roadName() const override { return roadName_; }
    int maneuverType() const override { return maneuverType_; }
    int turnDirection() const override { return turnDirection_; }
    QString formattedDistance() const override { return formattedDistance_; }
    int distanceMeters() const override { return distanceMeters_; }
    bool hasManeuverIcon() const override { return false; }
    int iconVersion() const override { return 0; }

    bool navActive_ = true;
    QString roadName_;
    int maneuverType_ = 0;
    int turnDirection_ = 0;
    QString formattedDistance_;
    int distanceMeters_ = 0;
};

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
    void testPhoneIdleEmptyCallsAndCapabilitiesFalse();
    void testPhoneRingingIncoming();
    void testPhoneAnswerActiveEchoesStartedAt();
    void testPhoneHangupEmptyCalls();
    void testPhoneCapabilitiesTrackTelephonyAvailable();
    void testPhoneHoldSwapAndMultipartyAlwaysFalse();
    void testNavManeuverTable();
    void testNavInactivePassesThroughFields();
    void testNavDistanceMetersPopulated();
    void testNavTurnSideHybridFallback();
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

void TestApiSerializers::testPhoneIdleEmptyCallsAndCapabilitiesFalse() {
    // Constructor takes parent only; do NOT call startDBusMonitoring() —
    // this exercises mock mode.
    oap::PhoneStateService phone;

    pb::PhoneStatus status = buildPhoneStatus(phone, 0);
    QCOMPARE(status.calls_size(), 0);
    QCOMPARE(status.hfp_connected(), phone.phoneConnected());
    QCOMPARE(QString::fromStdString(status.device_name()), phone.deviceName());
    QVERIFY(!status.capabilities().can_dial());
    QVERIFY(!status.capabilities().can_answer());
    QVERIFY(!status.capabilities().can_hangup());
    QVERIFY(!status.capabilities().can_send_dtmf());
    QVERIFY(!status.capabilities().can_hold_swap());
    QVERIFY(!status.capabilities().can_multiparty());
}

void TestApiSerializers::testPhoneRingingIncoming() {
    oap::PhoneStateService phone;
    phone.setIncomingCall("+15551234567", "Alice");

    pb::PhoneStatus status = buildPhoneStatus(phone, 0);
    QCOMPARE(status.calls_size(), 1);
    const auto& call = status.calls(0);
    QCOMPARE(call.state(), pb::CALL_STATE_INCOMING);
    QCOMPARE(QString::fromStdString(call.line_identification()), QString("+15551234567"));
    QCOMPARE(QString::fromStdString(call.display_name()), QString("Alice"));
    QCOMPARE(call.started_at_unix_ms(), qint64(0));
}

void TestApiSerializers::testPhoneAnswerActiveEchoesStartedAt() {
    oap::PhoneStateService phone;
    phone.setIncomingCall("+15551234567", "Alice");
    QVERIFY(phone.answer());

    const qint64 startedAt = 1720000000123LL;
    pb::PhoneStatus status = buildPhoneStatus(phone, startedAt);
    QCOMPARE(status.calls_size(), 1);
    const auto& call = status.calls(0);
    QCOMPARE(call.state(), pb::CALL_STATE_ACTIVE);
    QCOMPARE(call.started_at_unix_ms(), startedAt);
    // Still echoes the caller identity captured at ring time.
    QCOMPARE(QString::fromStdString(call.line_identification()), QString("+15551234567"));
}

void TestApiSerializers::testPhoneHangupEmptyCalls() {
    oap::PhoneStateService phone;
    phone.setIncomingCall("+15551234567", "Alice");
    QVERIFY(phone.answer());
    QVERIFY(phone.hangup());

    pb::PhoneStatus status = buildPhoneStatus(phone, 1720000000123LL);
    QCOMPARE(status.calls_size(), 0);
}

void TestApiSerializers::testPhoneCapabilitiesTrackTelephonyAvailable() {
    oap::PhoneStateService phone;

    pb::PhoneStatus before = buildPhoneStatus(phone, 0);
    QVERIFY(!before.capabilities().can_dial());
    QVERIFY(!before.capabilities().can_answer());
    QVERIFY(!before.capabilities().can_hangup());
    QVERIFY(!before.capabilities().can_send_dtmf());

    phone.onTelephonyAvailable(true);

    pb::PhoneStatus after = buildPhoneStatus(phone, 0);
    QVERIFY(after.capabilities().can_dial());
    QVERIFY(after.capabilities().can_answer());
    QVERIFY(after.capabilities().can_hangup());
    QVERIFY(after.capabilities().can_send_dtmf());
}

void TestApiSerializers::testPhoneHoldSwapAndMultipartyAlwaysFalse() {
    // Frozen contract: can_hold_swap/can_multiparty are hard-false in v1
    // regardless of telephony availability or call state.
    oap::PhoneStateService phone;
    phone.onTelephonyAvailable(true);
    phone.setIncomingCall("+15551234567", "Alice");
    QVERIFY(phone.answer());

    pb::PhoneStatus status = buildPhoneStatus(phone, 42);
    QVERIFY(!status.capabilities().can_hold_swap());
    QVERIFY(!status.capabilities().can_multiparty());
}

void TestApiSerializers::testNavManeuverTable() {
    FakeNavProvider nav;
    nav.navActive_ = true;
    nav.roadName_ = "Main St";
    nav.formattedDistance_ = "500 ft";

    auto check = [&](int raw, pb::ManeuverType type, pb::TurnSide side) {
        nav.maneuverType_ = raw;
        // No fallback data here (turnDirection unknown/0) -- this table is
        // exercising mapManeuver()'s own code->side mapping in isolation.
        // The turnDirection() fallback (used only when the maneuver code
        // itself is silent on side) is covered separately by
        // testNavTurnSideHybridFallback.
        nav.turnDirection_ = 0;
        pb::NavigationStatus status = buildNavigationStatus(nav);
        QCOMPARE(status.maneuver(), type);
        QCOMPARE(status.turn_side(), side);
    };

    check(7, pb::MANEUVER_TYPE_TURN, pb::TURN_SIDE_LEFT);
    check(14, pb::MANEUVER_TYPE_ON_RAMP, pb::TURN_SIDE_RIGHT);
    check(36, pb::MANEUVER_TYPE_STRAIGHT, pb::TURN_SIDE_UNSPECIFIED);
    check(39, pb::MANEUVER_TYPE_DESTINATION, pb::TURN_SIDE_UNSPECIFIED);
    check(51, pb::MANEUVER_TYPE_OTHER, pb::TURN_SIDE_UNSPECIFIED);
    check(33, pb::MANEUVER_TYPE_ROUNDABOUT_ENTER_AND_EXIT, pb::TURN_SIDE_UNSPECIFIED);
}

void TestApiSerializers::testNavInactivePassesThroughFields() {
    FakeNavProvider nav;
    nav.navActive_ = false;
    nav.roadName_ = "";
    nav.formattedDistance_ = "";
    nav.maneuverType_ = 0;

    pb::NavigationStatus status = buildNavigationStatus(nav);
    QVERIFY(!status.nav_active());
    QCOMPARE(status.maneuver(), pb::MANEUVER_TYPE_UNSPECIFIED);
    QCOMPARE(status.turn_side(), pb::TURN_SIDE_UNSPECIFIED);
    // FakeNavProvider defaults distanceMeters_ to 0.
    QCOMPARE(status.distance_meters(), 0);
}

void TestApiSerializers::testNavDistanceMetersPopulated() {
    FakeNavProvider nav;
    nav.navActive_ = true;
    nav.roadName_ = "Main St";
    nav.formattedDistance_ = "500 ft";
    nav.distanceMeters_ = 500;

    pb::NavigationStatus status = buildNavigationStatus(nav);
    QCOMPARE(status.distance_meters(), 500);
}

void TestApiSerializers::testNavTurnSideHybridFallback() {
    // turn_side hybrid (design doc §8.2, DECIDED 2026-07-06 after Codex
    // review of PR #12): the maneuver-code table is PRIMARY; turnDirection()
    // (raw oaa TurnSide.Enum: 0=UNKNOWN, 1=LEFT, 2=RIGHT) is consulted only
    // as a FALLBACK when the code itself yields TURN_SIDE_UNSPECIFIED.
    FakeNavProvider nav;
    nav.navActive_ = true;

    // (a) Roundabout code 33 encodes no side -> fallback to
    // turnDirection()=LEFT(1) wins.
    nav.maneuverType_ = 33;
    nav.turnDirection_ = 1;
    pb::NavigationStatus a = buildNavigationStatus(nav);
    QCOMPARE(a.maneuver(), pb::MANEUVER_TYPE_ROUNDABOUT_ENTER_AND_EXIT);
    QCOMPARE(a.turn_side(), pb::TURN_SIDE_LEFT);

    // (b) Code 7 (TURN/LEFT) already has a primary side -> it wins even
    // though turnDirection() disagrees (RIGHT=2).
    nav.maneuverType_ = 7;
    nav.turnDirection_ = 2;
    pb::NavigationStatus b = buildNavigationStatus(nav);
    QCOMPARE(b.maneuver(), pb::MANEUVER_TYPE_TURN);
    QCOMPARE(b.turn_side(), pb::TURN_SIDE_LEFT);

    // (c) Unmapped code 51 -> MANEUVER_TYPE_OTHER with no primary side ->
    // fallback to turnDirection()=RIGHT(2).
    nav.maneuverType_ = 51;
    nav.turnDirection_ = 2;
    pb::NavigationStatus c = buildNavigationStatus(nav);
    QCOMPARE(c.maneuver(), pb::MANEUVER_TYPE_OTHER);
    QCOMPARE(c.turn_side(), pb::TURN_SIDE_RIGHT);

    // (d) Code 36 (STRAIGHT) has no primary side, and turnDirection()=0
    // (UNKNOWN) has nothing to offer either -> stays unspecified.
    nav.maneuverType_ = 36;
    nav.turnDirection_ = 0;
    pb::NavigationStatus d = buildNavigationStatus(nav);
    QCOMPARE(d.maneuver(), pb::MANEUVER_TYPE_STRAIGHT);
    QCOMPARE(d.turn_side(), pb::TURN_SIDE_UNSPECIFIED);
}

QTEST_MAIN(TestApiSerializers)
#include "test_api_serializers.moc"
