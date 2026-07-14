#include <QtTest>
#include <QSignalSpy>
#include <QHostAddress>

#include <cmath>
#include <limits>

#include "core/api/ApiSession.hpp"
#include "core/api/ApiTransport.hpp"
#include "core/api/ApiRequestHandlers.hpp"
#include "core/api/ApiInboundState.hpp"
#include "core/api/ApiSerializers.hpp"
#include "core/services/ActionRegistry.hpp"
#include "core/services/NotificationService.hpp"
#include "core/services/PhoneStateService.hpp"
#include "api/api.pb.h"

namespace pb = prodigy::api::v1;
using oap::api::ApiSession;
using oap::api::ApiSessionDeps;
using oap::api::ApiInboundState;
using oap::api::ApiRequestHandlers;
using oap::api::IApiTransport;

// -----------------------------------------------------------------------------
// In-file fake transport (copied — tests may not share headers). Records sent
// frames, has a settable peer address, and injects inbound messages by
// emitting the signal. close() emits closed() synchronously (drives teardown).
// -----------------------------------------------------------------------------
class FakeTransport : public IApiTransport {
    Q_OBJECT
public:
    QList<QByteArray> sent;
    QHostAddress peer = QHostAddress(QHostAddress::LocalHost);
    qint64 fakeBytesToWrite = 0;

    void sendMessage(const QByteArray& serialized) override { sent.append(serialized); }
    qint64 bytesToWrite() const override { return fakeBytesToWrite; }
    void close() override { emit closed(); }
    void abort() override { emit closed(); }
    QHostAddress peerAddress() const override { return peer; }

    void injectMessage(const QByteArray& bytes) { emit messageReceived(bytes); }
};

// -----------------------------------------------------------------------------
namespace {

QByteArray serialize(const pb::ApiMessage& m) {
    std::string s;
    m.SerializeToString(&s);
    return QByteArray::fromStdString(s);
}

pb::ApiMessage parse(const QByteArray& bytes) {
    pb::ApiMessage m;
    m.ParseFromArray(bytes.constData(), bytes.size());
    return m;
}

QByteArray clientHello() {
    pb::ApiMessage m;
    m.set_request_id(1);
    auto* h = m.mutable_client_hello();
    h->set_requested_api_version_major(1);
    h->set_client_name("TestClient");
    h->set_client_kind(pb::CLIENT_KIND_DIAGNOSTIC);
    return serialize(m);
}

QByteArray batteryReport(int percent, bool charging) {
    pb::ApiMessage m;
    auto* r = m.mutable_battery_report();
    r->set_percent(percent);
    r->set_charging(charging);
    return serialize(m);
}

QByteArray gpsReport(double lat, double lon) {
    pb::ApiMessage m;
    auto* r = m.mutable_gps_report();
    r->set_latitude(lat);
    r->set_longitude(lon);
    return serialize(m);
}

QByteArray connectivityReport(bool internet, bool socks, quint16 port) {
    pb::ApiMessage m;
    auto* r = m.mutable_connectivity_report();
    r->set_internet_available(internet);
    r->set_socks5_active(socks);
    r->set_socks5_port(port);
    return serialize(m);
}

} // namespace

// -----------------------------------------------------------------------------
class TestApiRequestHandlers : public QObject {
    Q_OBJECT
private slots:
    void testListContainsRegistered();
    void testDispatchUnknownFalse();
    void testRegisterReservedPrefixRejected();
    void testRegisterDuplicateRejected();
    void testClientActionRoundTrip();
    void testDisconnectUnregisters();
    void testNotificationOwnership();
    void testNotificationPriorityZeroHonored();
    void testAllPhoneCommandsUnavailable();
    void testPhoneCommandsFollowCapability();
    void testGpsReportUpdatesState();
    void testGpsReportForwardsAllFields();
    void testGpsReportRejectsInvalidExtras();
    void testClearGpsResetsAllFields();
    void testGpsReportAcceptsBoundaryBearing();
    void testGpsRejectedReportLeavesStateUnchanged();
    void testInactiveConnectivityReportMarksPresence();
    void testTimeOnlyReportMarksPresence();
    void testReportOwnerCloseClearsState();
    void testNonOwnerCloseKeepsReportState();
    void testGpsStaleAfterThreshold();
    void testProxyActiveReflectsConnectivity();
    void testConnectivityEmitsProxyRoute();
    void testOwnerSessionCloseClearsRoute();
    void testNonOwnerSessionCloseLeavesRoute();
    void testInactiveReportReleasesOwnership();
    void testNewOwnerTakesOver();
    void testTimeReportSignal();
    void testTimeReportValidTimezoneEmitsBoth();
    void testTimeReportInvalidTimezoneDropsZoneOnly();
    void testTimeReportNoTimezoneNoZoneSignal();
    void testUnroutablePayloadClosesSession();
    void testLivenessExpiryClearsReportingRole();
    void testLivenessExpiryTearsProxyRoute();
    void testLivenessExpirySparesNonReportingRoles();
    void testLivenessRevivalOnNextReport();
    void testLivenessPerSessionIndependence();
    void testLivenessBoundaryNotExpiredAtThreshold();
    void testLivenessTimerArmsAndDisarms();
};

// -----------------------------------------------------------------------------
void TestApiRequestHandlers::testListContainsRegistered() {
    oap::ActionRegistry actions;
    actions.registerAction("system.builtin", [](const QVariant&) {});
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());
    QCOMPARE(session.state(), ApiSession::State::Ready);

    // Register a client-owned action so both flavours appear in the list.
    pb::ApiMessage reg;
    reg.set_request_id(5);
    reg.mutable_register_actions_request()->add_actions()->set_id("testapp.hello");
    transport->injectMessage(serialize(reg));

    pb::ApiMessage list;
    list.set_request_id(6);
    list.mutable_list_actions_request();
    transport->injectMessage(serialize(list));

    pb::ApiMessage resp = parse(transport->sent.last());
    QCOMPARE(resp.payload_case(), pb::ApiMessage::kListActionsResponse);
    QCOMPARE(resp.request_id(), quint64(6));

    bool sawBuiltin = false, sawClient = false;
    for (const auto& info : resp.list_actions_response().actions()) {
        const QString id = QString::fromStdString(info.id());
        if (id == "system.builtin") { sawBuiltin = true; QVERIFY(!info.client_owned()); }
        if (id == "testapp.hello")  { sawClient = true;  QVERIFY(info.client_owned()); }
    }
    QVERIFY(sawBuiltin);
    QVERIFY(sawClient);
}

void TestApiRequestHandlers::testDispatchUnknownFalse() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    pb::ApiMessage d;
    d.set_request_id(3);
    d.mutable_dispatch_action_request()->set_id("does.not.exist");
    transport->injectMessage(serialize(d));

    pb::ApiMessage resp = parse(transport->sent.last());
    QCOMPARE(resp.payload_case(), pb::ApiMessage::kDispatchActionResponse);
    QVERIFY(!resp.dispatch_action_response().dispatched());
}

void TestApiRequestHandlers::testRegisterReservedPrefixRejected() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    pb::ApiMessage reg;
    reg.set_request_id(4);
    reg.mutable_register_actions_request()->add_actions()->set_id("media.hack");
    transport->injectMessage(serialize(reg));

    pb::ApiMessage resp = parse(transport->sent.last());
    QCOMPARE(resp.payload_case(), pb::ApiMessage::kRegisterActionsResponse);
    QCOMPARE(resp.register_actions_response().results_size(), 1);
    const auto& r = resp.register_actions_response().results(0);
    QVERIFY(!r.accepted());
    QVERIFY(QString::fromStdString(r.reason()).contains("reserved"));
    QVERIFY(!actions.registeredActions().contains("media.hack"));
}

void TestApiRequestHandlers::testRegisterDuplicateRejected() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    pb::ApiMessage reg1;
    reg1.set_request_id(1);
    reg1.mutable_register_actions_request()->add_actions()->set_id("testapp.dup");
    transport->injectMessage(serialize(reg1));
    QVERIFY(parse(transport->sent.last()).register_actions_response().results(0).accepted());

    pb::ApiMessage reg2;
    reg2.set_request_id(2);
    reg2.mutable_register_actions_request()->add_actions()->set_id("testapp.dup");
    transport->injectMessage(serialize(reg2));
    pb::ApiMessage resp2 = parse(transport->sent.last());
    const auto& r = resp2.register_actions_response().results(0);
    QVERIFY(!r.accepted());
    QVERIFY(QString::fromStdString(r.reason()).contains("duplicate"));
}

void TestApiRequestHandlers::testClientActionRoundTrip() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    pb::ApiMessage reg;
    reg.set_request_id(1);
    auto* spec = reg.mutable_register_actions_request()->add_actions();
    spec->set_id("testapp.ping");
    spec->set_label("Ping");
    transport->injectMessage(serialize(reg));
    QVERIFY(parse(transport->sent.last()).register_actions_response().results(0).accepted());

    // Dispatch through the registry directly (as the head unit UI would).
    const int before = transport->sent.size();
    actions.dispatch("testapp.ping", QVariant(42));
    QCOMPARE(transport->sent.size(), before + 1);

    pb::ApiMessage ev = parse(transport->sent.last());
    QCOMPARE(ev.payload_case(), pb::ApiMessage::kActionInvoked);
    QCOMPARE(ev.request_id(), quint64(0));
    QCOMPARE(QString::fromStdString(ev.action_invoked().id()), QString("testapp.ping"));
    QVERIFY(ev.action_invoked().has_payload_json());
    QCOMPARE(QString::fromStdString(ev.action_invoked().payload_json()), QString("42"));
}

void TestApiRequestHandlers::testDisconnectUnregisters() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    pb::ApiMessage reg;
    reg.set_request_id(1);
    reg.mutable_register_actions_request()->add_actions()->set_id("testapp.ephemeral");
    transport->injectMessage(serialize(reg));
    QVERIFY(actions.registeredActions().contains("testapp.ephemeral"));

    // Peer disconnect -> teardown -> sessionClosed -> auto-unregister.
    transport->close();
    QVERIFY(!actions.registeredActions().contains("testapp.ephemeral"));
}

void TestApiRequestHandlers::testNotificationOwnership() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* tA = new FakeTransport();
    ApiSessionDeps depsA; depsA.requests = &handler;
    ApiSession sessionA(tA, depsA);
    tA->injectMessage(clientHello());

    auto* tB = new FakeTransport();
    ApiSessionDeps depsB; depsB.requests = &handler;
    ApiSession sessionB(tB, depsB);
    tB->injectMessage(clientHello());

    // A posts.
    pb::ApiMessage post;
    post.set_request_id(1);
    post.mutable_post_notification_request()->set_message("hi");
    tA->injectMessage(serialize(post));
    pb::ApiMessage postResp = parse(tA->sent.last());
    QCOMPARE(postResp.payload_case(), pb::ApiMessage::kPostNotificationResponse);
    const QString notifId =
        QString::fromStdString(postResp.post_notification_response().notification_id());
    QVERIFY(!notifId.isEmpty());
    QCOMPARE(notifications.active().size(), 1);

    // B tries to dismiss A's notification -> NOT_FOUND, still present, B alive.
    pb::ApiMessage disB;
    disB.set_request_id(2);
    disB.mutable_dismiss_notification_request()->set_notification_id(notifId.toStdString());
    tB->injectMessage(serialize(disB));
    pb::ApiMessage bResp = parse(tB->sent.last());
    QCOMPARE(bResp.payload_case(), pb::ApiMessage::kError);
    QCOMPARE(bResp.error().code(), pb::ERROR_CODE_NOT_FOUND);
    QCOMPARE(notifications.active().size(), 1);
    QCOMPARE(sessionB.state(), ApiSession::State::Ready);

    // A dismisses its own -> Ack, gone.
    pb::ApiMessage disA;
    disA.set_request_id(3);
    disA.mutable_dismiss_notification_request()->set_notification_id(notifId.toStdString());
    tA->injectMessage(serialize(disA));
    QCOMPARE(parse(tA->sent.last()).payload_case(), pb::ApiMessage::kAck);
    QCOMPARE(notifications.active().size(), 0);
}

void TestApiRequestHandlers::testNotificationPriorityZeroHonored() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    // Explicit priority 0 must reach the service as 0 (proto3 optional has_).
    pb::ApiMessage p0;
    p0.set_request_id(1);
    auto* n0 = p0.mutable_post_notification_request();
    n0->set_message("zero");
    n0->set_priority(0);
    transport->injectMessage(serialize(p0));
    QCOMPARE(notifications.active().last().priority, 0);

    // Absent priority -> head-unit standard 50.
    pb::ApiMessage pa;
    pa.set_request_id(2);
    pa.mutable_post_notification_request()->set_message("absent");
    transport->injectMessage(serialize(pa));
    QCOMPARE(notifications.active().last().priority, 50);
}

void TestApiRequestHandlers::testAllPhoneCommandsUnavailable() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;   // telephonyAvailable() == false by default
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    auto expectUnavailable = [&](const pb::ApiMessage& req) {
        transport->injectMessage(serialize(req));
        pb::ApiMessage resp = parse(transport->sent.last());
        QCOMPARE(resp.payload_case(), pb::ApiMessage::kPhoneCommandResponse);
        QCOMPARE(resp.phone_command_response().result(),
                 pb::PHONE_COMMAND_RESULT_UNAVAILABLE);
    };

    pb::ApiMessage dial;  dial.set_request_id(1); dial.mutable_dial_request()->set_number("5551234");
    expectUnavailable(dial);
    pb::ApiMessage ans;   ans.set_request_id(2);  ans.mutable_answer_call_request();
    expectUnavailable(ans);
    pb::ApiMessage hang;  hang.set_request_id(3); hang.mutable_hangup_request();
    expectUnavailable(hang);
    pb::ApiMessage dtmf;  dtmf.set_request_id(4); dtmf.mutable_send_dtmf_request()->set_tones("1");
    expectUnavailable(dtmf);

    // Even with an incoming call ringing: capabilities are false, contract wins.
    phone.setIncomingCall("5551234", "Caller");
    pb::ApiMessage ans2;  ans2.set_request_id(5); ans2.mutable_answer_call_request();
    expectUnavailable(ans2);
}

void TestApiRequestHandlers::testPhoneCommandsFollowCapability() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    phone.onTelephonyAvailable(true);   // mock mode + capability on
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    // Dial from Idle -> OK (mock dispatch succeeds).
    pb::ApiMessage dial;
    dial.set_request_id(1);
    dial.mutable_dial_request()->set_number("5551234");
    transport->injectMessage(serialize(dial));
    QCOMPARE(parse(transport->sent.last()).phone_command_response().result(),
             pb::PHONE_COMMAND_RESULT_OK);

    // Answer with no ring -> FAILED (guard rejects, capability true).
    pb::ApiMessage ans;
    ans.set_request_id(2);
    ans.mutable_answer_call_request();
    transport->injectMessage(serialize(ans));
    QCOMPARE(parse(transport->sent.last()).phone_command_response().result(),
             pb::PHONE_COMMAND_RESULT_FAILED);

    // PhoneStatus snapshot now carries can_dial=true, can_hold_swap=false.
    pb::PhoneStatus status = oap::api::serial::buildPhoneStatus(phone, 0);
    QVERIFY(status.capabilities().can_dial());
    QVERIFY(!status.capabilities().can_hold_swap());
}

void TestApiRequestHandlers::testGpsReportUpdatesState() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    QSignalSpy spy(&inbound, &ApiInboundState::gpsChanged);

    pb::ApiMessage rpt;
    rpt.set_request_id(0);   // reports use request_id 0
    auto* g = rpt.mutable_gps_report();
    g->set_latitude(45.5);
    g->set_longitude(-122.6);
    g->set_speed_mps(13.4);
    const int before = transport->sent.size();
    transport->injectMessage(serialize(rpt));

    QCOMPARE(spy.count(), 1);
    QVERIFY(inbound.gpsValid());
    QCOMPARE(inbound.gpsLat(), 45.5);
    QCOMPARE(inbound.gpsLon(), -122.6);
    QCOMPARE(inbound.gpsSpeedMps(), 13.4);
    // Reports are fire-and-forget: no response frame is ever produced.
    QCOMPARE(transport->sent.size(), before);
}

// GpsReport must forward all six fields (lat/lon/speed already covered above;
// this pins bearing/accuracy/age, which the legacy path silently dropped).
void TestApiRequestHandlers::testGpsReportForwardsAllFields() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    QSignalSpy spy(&inbound, &ApiInboundState::gpsChanged);

    pb::ApiMessage rpt;
    rpt.set_request_id(0);
    auto* g = rpt.mutable_gps_report();
    g->set_latitude(45.5);
    g->set_longitude(-122.6);
    g->set_speed_mps(13.4);
    g->set_bearing_deg(275.0);
    g->set_accuracy_m(4.2);
    g->set_age_ms(120);
    transport->injectMessage(serialize(rpt));

    QCOMPARE(spy.count(), 1);
    QVERIFY(inbound.gpsValid());
    QCOMPARE(inbound.gpsBearing(), 275.0);
    QCOMPARE(inbound.gpsAccuracy(), 4.2);
    QCOMPARE(inbound.gpsSpeedMps(), 13.4);
    // gpsSpeed is a legacy alias for gpsSpeedMps.
    QCOMPARE(inbound.property("gpsSpeed").toDouble(), 13.4);
    // Fresh 120 ms fix is well under the 30 s window.
    QVERIFY(!inbound.gpsStale());
    // Any accepted report marks an owner present.
    QVERIFY(inbound.connected());
}

// GPS extras (speed/bearing/accuracy) must meet the proto contract: all finite,
// speed >= 0, accuracy >= 0, bearing in [0, 360). A violation drops the whole
// report (parity with the lat/lon rejection) and never reaches shared state.
void TestApiRequestHandlers::testGpsReportRejectsInvalidExtras() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    QSignalSpy spy(&inbound, &ApiInboundState::gpsChanged);

    auto sendGps = [&](double speed, double bearing, double accuracy) {
        pb::ApiMessage rpt;
        rpt.set_request_id(0);
        auto* g = rpt.mutable_gps_report();
        g->set_latitude(45.5);
        g->set_longitude(-122.6);
        g->set_speed_mps(speed);
        g->set_bearing_deg(bearing);
        g->set_accuracy_m(accuracy);
        transport->injectMessage(serialize(rpt));
    };

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    sendGps(nan, 90.0, 5.0);    // NaN speed
    sendGps(10.0, 90.0, inf);   // infinite accuracy
    sendGps(10.0, 360.0, 5.0);  // bearing == 360 (out of [0,360))
    sendGps(10.0, -1.0, 5.0);   // bearing < 0
    sendGps(-1.0, 90.0, 5.0);   // negative speed
    sendGps(10.0, 90.0, -1.0);  // negative accuracy

    // Every report was dropped: no state change, no owner presence.
    QCOMPARE(spy.count(), 0);
    QVERIFY(!inbound.gpsValid());
    QVERIFY(!inbound.connected());
}

// Finding 2: clearGps must reset ALL cached GPS values (legacy parity), not
// just the valid flag — otherwise stale coordinates leak through readers
// (e.g. IpcServer::handleCompanionStatus) after the owner disconnects.
void TestApiRequestHandlers::testClearGpsResetsAllFields() {
    ApiInboundState inbound;
    inbound.setGps(45.5, -122.6, 13.4, 275.0, 4.2, 120);
    QVERIFY(inbound.gpsValid());

    QSignalSpy spy(&inbound, &ApiInboundState::gpsChanged);
    inbound.clearGps();

    QCOMPARE(spy.count(), 1);            // single emission
    QVERIFY(!inbound.gpsValid());
    QVERIFY(inbound.gpsStale());
    QCOMPARE(inbound.gpsLat(), 0.0);
    QCOMPARE(inbound.gpsLon(), 0.0);
    QCOMPARE(inbound.gpsSpeedMps(), 0.0);
    QCOMPARE(inbound.gpsBearing(), 0.0);
    QCOMPARE(inbound.gpsAccuracy(), 0.0);

    // Idempotent: a second clear on already-cleared state emits nothing.
    inbound.clearGps();
    QCOMPARE(spy.count(), 1);
}

void TestApiRequestHandlers::testGpsReportAcceptsBoundaryBearing() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    QSignalSpy spy(&inbound, &ApiInboundState::gpsChanged);

    auto sendBearing = [&](double bearing) {
        pb::ApiMessage rpt;
        rpt.set_request_id(0);
        auto* g = rpt.mutable_gps_report();
        g->set_latitude(45.5);
        g->set_longitude(-122.6);
        g->set_speed_mps(0.0);       // 0 = unknown, still valid
        g->set_bearing_deg(bearing);
        g->set_accuracy_m(0.0);      // 0 = unknown, still valid
        transport->injectMessage(serialize(rpt));
    };

    sendBearing(0.0);      // lower boundary accepted
    QCOMPARE(spy.count(), 1);
    QCOMPARE(inbound.gpsBearing(), 0.0);

    sendBearing(359.9);    // just under the upper bound accepted
    QCOMPARE(spy.count(), 2);
    QCOMPARE(inbound.gpsBearing(), 359.9);
    QVERIFY(inbound.gpsValid());
}

void TestApiRequestHandlers::testGpsRejectedReportLeavesStateUnchanged() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    // Establish a valid fix.
    pb::ApiMessage good;
    good.set_request_id(0);
    auto* g = good.mutable_gps_report();
    g->set_latitude(45.5);
    g->set_longitude(-122.6);
    g->set_speed_mps(12.0);
    g->set_bearing_deg(100.0);
    g->set_accuracy_m(3.0);
    transport->injectMessage(serialize(good));

    QSignalSpy spy(&inbound, &ApiInboundState::gpsChanged);

    // A subsequent malformed report (NaN speed) must not mutate the good state.
    pb::ApiMessage bad;
    bad.set_request_id(0);
    auto* b = bad.mutable_gps_report();
    b->set_latitude(10.0);
    b->set_longitude(10.0);
    b->set_speed_mps(std::numeric_limits<double>::quiet_NaN());
    b->set_bearing_deg(200.0);
    b->set_accuracy_m(1.0);
    transport->injectMessage(serialize(bad));

    QCOMPARE(spy.count(), 0);            // rejected report emitted nothing
    QCOMPARE(inbound.gpsLat(), 45.5);   // prior fix intact
    QCOMPARE(inbound.gpsBearing(), 100.0);
    QCOMPARE(inbound.gpsSpeedMps(), 12.0);
}

// Presence (`connected`) must reflect a live reporting session regardless of
// route ownership: a companion reporting INACTIVE connectivity is still
// present, and drops presence only when it closes.
void TestApiRequestHandlers::testInactiveConnectivityReportMarksPresence() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    // Inactive connectivity: no usable route, so connectivityOwner stays null,
    // but the session IS reporting -> present.
    pb::ApiMessage c;
    c.set_request_id(0);
    auto* r = c.mutable_connectivity_report();
    r->set_internet_available(false);
    r->set_socks5_active(false);
    transport->injectMessage(serialize(c));

    QVERIFY(!inbound.proxyActive());   // route not active
    QVERIFY(inbound.connected());      // but a reporting session is present

    transport->close();
    QVERIFY(!inbound.connected());     // presence drops when it disconnects
}

// A time-only reporting session (no GPS/battery/connectivity) also counts as a
// present companion.
void TestApiRequestHandlers::testTimeOnlyReportMarksPresence() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    pb::ApiMessage t;
    t.set_request_id(0);
    t.mutable_time_report()->set_unix_time_ms(Q_INT64_C(1751731200000));
    transport->injectMessage(serialize(t));

    QVERIFY(inbound.connected());

    transport->close();
    QVERIFY(!inbound.connected());
}

// A report's owning session disconnecting must clear that report's cached
// state (owner-tracked clear) and drop owner-presence when nothing remains.
void TestApiRequestHandlers::testReportOwnerCloseClearsState() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    pb::ApiMessage gps;
    gps.set_request_id(0);
    auto* g = gps.mutable_gps_report();
    g->set_latitude(45.5);
    g->set_longitude(-122.6);
    transport->injectMessage(serialize(gps));

    pb::ApiMessage bat;
    bat.set_request_id(0);
    auto* b = bat.mutable_battery_report();
    b->set_percent(77);
    b->set_charging(true);
    transport->injectMessage(serialize(bat));

    QVERIFY(inbound.gpsValid());
    QCOMPARE(inbound.phoneBattery(), 77);
    QVERIFY(inbound.connected());

    // Owner disconnects -> every report type it sourced is cleared.
    transport->close();

    QVERIFY(!inbound.gpsValid());
    QVERIFY(inbound.gpsStale());
    QCOMPARE(inbound.phoneBattery(), -1);
    QVERIFY(!inbound.connected());
}

// A session that never sourced a report closing must not touch another
// session's cached state, and owner-presence must remain true.
void TestApiRequestHandlers::testNonOwnerCloseKeepsReportState() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* tA = new FakeTransport();
    ApiSessionDeps depsA; depsA.requests = &handler;
    ApiSession sessionA(tA, depsA);
    tA->injectMessage(clientHello());

    auto* tB = new FakeTransport();
    ApiSessionDeps depsB; depsB.requests = &handler;
    ApiSession sessionB(tB, depsB);
    tB->injectMessage(clientHello());

    // A sources GPS + battery -> A owns both.
    pb::ApiMessage gps;
    gps.set_request_id(0);
    auto* g = gps.mutable_gps_report();
    g->set_latitude(45.5);
    g->set_longitude(-122.6);
    tA->injectMessage(serialize(gps));

    pb::ApiMessage bat;
    bat.set_request_id(0);
    bat.mutable_battery_report()->set_percent(77);
    tA->injectMessage(serialize(bat));
    QVERIFY(inbound.connected());

    // Non-owner B disconnects -> A's state and presence untouched.
    tB->close();

    QVERIFY(inbound.gpsValid());
    QCOMPARE(inbound.phoneBattery(), 77);
    QVERIFY(inbound.connected());
}

// gpsStale flips true once the effective age crosses the threshold. The
// injectable threshold keeps the test off the real 30 s clock.
void TestApiRequestHandlers::testGpsStaleAfterThreshold() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    inbound.setStaleThresholdMs(50);
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    pb::ApiMessage gps;
    gps.set_request_id(0);
    auto* g = gps.mutable_gps_report();
    g->set_latitude(45.5);
    g->set_longitude(-122.6);
    g->set_age_ms(0);
    transport->injectMessage(serialize(gps));

    // Fresh fix is not stale...
    QVERIFY(!inbound.gpsStale());
    // ...but goes stale once the effective age exceeds the 50 ms window.
    QTRY_VERIFY(inbound.gpsStale());
}

// proxyActive mirrors the SOCKS5 route state (the property that fixes the
// dead proxyStatus read), and a connectivity owner counts toward `connected`.
void TestApiRequestHandlers::testProxyActiveReflectsConnectivity() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    pb::ApiMessage c;
    c.set_request_id(0);
    auto* r = c.mutable_connectivity_report();
    r->set_internet_available(true);
    r->set_socks5_active(true);
    r->set_socks5_port(1080);
    transport->injectMessage(serialize(c));

    QVERIFY(inbound.proxyActive());
    QVERIFY(inbound.connected());   // connectivity owner counts

    // Owner disconnects -> route torn down, proxy inactive, presence dropped.
    transport->close();
    QVERIFY(!inbound.proxyActive());
    QVERIFY(!inbound.connected());
}

void TestApiRequestHandlers::testConnectivityEmitsProxyRoute() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    QSignalSpy spy(&inbound, &ApiInboundState::proxyRouteChanged);

    // WITH password.
    pb::ApiMessage c1;
    c1.set_request_id(0);
    auto* r1 = c1.mutable_connectivity_report();
    r1->set_internet_available(true);
    r1->set_socks5_active(true);
    r1->set_socks5_port(1080);
    r1->set_socks5_password("hunter2");
    transport->injectMessage(serialize(c1));

    QCOMPARE(spy.count(), 1);
    {
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toBool(), true);              // active
        QCOMPARE(args.at(1).toString(), QString("127.0.0.1"));  // peer host
        QCOMPARE(args.at(2).toUInt(), 1080u);             // port
        QCOMPARE(args.at(3).toString(), QString("hunter2"));    // password
    }
    QVERIFY(inbound.internetAvailable());
    QCOMPARE(inbound.proxyAddress(), QString("socks5://127.0.0.1:1080"));

    // WITHOUT password -> empty password field.
    pb::ApiMessage c2;
    c2.set_request_id(0);
    auto* r2 = c2.mutable_connectivity_report();
    r2->set_internet_available(true);
    r2->set_socks5_active(true);
    r2->set_socks5_port(1080);
    transport->injectMessage(serialize(c2));

    QCOMPARE(spy.count(), 1);
    {
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(3).toString(), QString());       // no password
    }

    // Mixed bits: proxy process running but phone has no upstream internet ->
    // not a usable route (proxyRouteChanged still fires, but with active=false).
    pb::ApiMessage c3;
    c3.set_request_id(0);
    auto* r3 = c3.mutable_connectivity_report();
    r3->set_internet_available(false);
    r3->set_socks5_active(true);
    r3->set_socks5_port(1080);
    transport->injectMessage(serialize(c3));

    QCOMPARE(spy.count(), 1);
    {
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toBool(), false);              // active
    }
    QVERIFY(!inbound.internetAvailable());
    QVERIFY(inbound.proxyAddress().isEmpty());

    // Mixed bits: phone has upstream internet but the proxy isn't up -> also
    // not a usable route.
    pb::ApiMessage c4;
    c4.set_request_id(0);
    auto* r4 = c4.mutable_connectivity_report();
    r4->set_internet_available(true);
    r4->set_socks5_active(false);
    transport->injectMessage(serialize(c4));

    QCOMPARE(spy.count(), 1);
    {
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toBool(), false);              // active
    }
    QVERIFY(!inbound.internetAvailable());
    QVERIFY(inbound.proxyAddress().isEmpty());
}

// Task D — proxy-route teardown on companion session disconnect. Route
// ownership follows the session that last reported it active; the owner's
// disconnect must tear the route down (legacy CompanionListenerService
// parity). A non-owner disconnecting must never touch it.

void TestApiRequestHandlers::testOwnerSessionCloseClearsRoute() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    QSignalSpy spy(&inbound, &ApiInboundState::proxyRouteChanged);

    // Session reports an active route -> becomes owner.
    pb::ApiMessage c1;
    c1.set_request_id(0);
    auto* r1 = c1.mutable_connectivity_report();
    r1->set_internet_available(true);
    r1->set_socks5_active(true);
    r1->set_socks5_port(1080);
    transport->injectMessage(serialize(c1));
    QCOMPARE(spy.count(), 1);
    spy.clear();
    QVERIFY(inbound.internetAvailable());

    // Owner disconnects -> route must be torn down.
    transport->close();

    QCOMPARE(spy.count(), 1);
    const QList<QVariant> args = spy.takeFirst();
    QCOMPARE(args.at(0).toBool(), false);
    QVERIFY(!inbound.internetAvailable());
}

void TestApiRequestHandlers::testNonOwnerSessionCloseLeavesRoute() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* tA = new FakeTransport();
    ApiSessionDeps depsA; depsA.requests = &handler;
    ApiSession sessionA(tA, depsA);
    tA->injectMessage(clientHello());

    auto* tB = new FakeTransport();
    ApiSessionDeps depsB; depsB.requests = &handler;
    ApiSession sessionB(tB, depsB);
    tB->injectMessage(clientHello());

    QSignalSpy spy(&inbound, &ApiInboundState::proxyRouteChanged);

    // A reports active -> A becomes owner. B never reports anything.
    pb::ApiMessage c1;
    c1.set_request_id(0);
    auto* r1 = c1.mutable_connectivity_report();
    r1->set_internet_available(true);
    r1->set_socks5_active(true);
    r1->set_socks5_port(1080);
    tA->injectMessage(serialize(c1));
    QCOMPARE(spy.count(), 1);
    spy.clear();

    // Non-owner B disconnects -> must not touch the route.
    tB->close();

    QCOMPARE(spy.count(), 0);
    QVERIFY(inbound.internetAvailable());
}

void TestApiRequestHandlers::testInactiveReportReleasesOwnership() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    QSignalSpy spy(&inbound, &ApiInboundState::proxyRouteChanged);

    // Reports active, then inactive -> ownership already released by the
    // report itself (rule: whoever reports inactive is the last writer).
    pb::ApiMessage c1;
    c1.set_request_id(0);
    auto* r1 = c1.mutable_connectivity_report();
    r1->set_internet_available(true);
    r1->set_socks5_active(true);
    r1->set_socks5_port(1080);
    transport->injectMessage(serialize(c1));
    QCOMPARE(spy.count(), 1);

    pb::ApiMessage c2;
    c2.set_request_id(0);
    auto* r2 = c2.mutable_connectivity_report();
    r2->set_internet_available(false);
    r2->set_socks5_active(false);
    transport->injectMessage(serialize(c2));
    QCOMPARE(spy.count(), 2);

    // Session (no longer owner) disconnects -> no further emission.
    transport->close();
    QCOMPARE(spy.count(), 2);
}

void TestApiRequestHandlers::testNewOwnerTakesOver() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* tA = new FakeTransport();
    ApiSessionDeps depsA; depsA.requests = &handler;
    ApiSession sessionA(tA, depsA);
    tA->injectMessage(clientHello());

    auto* tB = new FakeTransport();
    ApiSessionDeps depsB; depsB.requests = &handler;
    ApiSession sessionB(tB, depsB);
    tB->injectMessage(clientHello());

    QSignalSpy spy(&inbound, &ApiInboundState::proxyRouteChanged);

    // A reports active -> A is owner.
    pb::ApiMessage c1;
    c1.set_request_id(0);
    auto* r1 = c1.mutable_connectivity_report();
    r1->set_internet_available(true);
    r1->set_socks5_active(true);
    r1->set_socks5_port(1080);
    tA->injectMessage(serialize(c1));
    QCOMPARE(spy.count(), 1);

    // B reports active -> last-writer-wins, B is now owner.
    pb::ApiMessage c2;
    c2.set_request_id(0);
    auto* r2 = c2.mutable_connectivity_report();
    r2->set_internet_available(true);
    r2->set_socks5_active(true);
    r2->set_socks5_port(1080);
    tB->injectMessage(serialize(c2));
    QCOMPARE(spy.count(), 2);

    // A (no longer owner) disconnects -> no emission.
    tA->close();
    QCOMPARE(spy.count(), 2);

    // B (current owner) disconnects -> route torn down.
    tB->close();
    QCOMPARE(spy.count(), 3);
    const QList<QVariant> args = spy.last();
    QCOMPARE(args.at(0).toBool(), false);
}

void TestApiRequestHandlers::testTimeReportSignal() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    QSignalSpy spy(&inbound, &ApiInboundState::timeReported);

    pb::ApiMessage t;
    t.set_request_id(0);
    t.mutable_time_report()->set_unix_time_ms(Q_INT64_C(1751731200000));
    transport->injectMessage(serialize(t));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toLongLong(), Q_INT64_C(1751731200000));
}

void TestApiRequestHandlers::testTimeReportValidTimezoneEmitsBoth() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    QSignalSpy timeSpy(&inbound, &ApiInboundState::timeReported);
    QSignalSpy tzSpy(&inbound, &ApiInboundState::timezoneReported);

    pb::ApiMessage t;
    t.set_request_id(0);
    auto* tr = t.mutable_time_report();
    tr->set_unix_time_ms(Q_INT64_C(1751731200000));
    tr->set_timezone_id("America/Chicago");
    transport->injectMessage(serialize(t));

    QCOMPARE(timeSpy.count(), 1);
    QCOMPARE(tzSpy.count(), 1);
    QCOMPARE(tzSpy.takeFirst().at(0).toString(), QString("America/Chicago"));
}

void TestApiRequestHandlers::testTimeReportInvalidTimezoneDropsZoneOnly() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    QSignalSpy timeSpy(&inbound, &ApiInboundState::timeReported);
    QSignalSpy tzSpy(&inbound, &ApiInboundState::timezoneReported);

    pb::ApiMessage t;
    t.set_request_id(0);
    auto* tr = t.mutable_time_report();
    tr->set_unix_time_ms(Q_INT64_C(1751731200000));
    tr->set_timezone_id("Not/AZone");
    transport->injectMessage(serialize(t));

    // Invalid zone is dropped, but the time report itself still applies.
    QCOMPARE(timeSpy.count(), 1);
    QCOMPARE(tzSpy.count(), 0);
}

void TestApiRequestHandlers::testTimeReportNoTimezoneNoZoneSignal() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    QSignalSpy timeSpy(&inbound, &ApiInboundState::timeReported);
    QSignalSpy tzSpy(&inbound, &ApiInboundState::timezoneReported);

    pb::ApiMessage t;
    t.set_request_id(0);
    t.mutable_time_report()->set_unix_time_ms(Q_INT64_C(1751731200000));
    // No timezone_id set at all.
    transport->injectMessage(serialize(t));

    QCOMPARE(timeSpy.count(), 1);
    QCOMPARE(tzSpy.count(), 0);
}

void TestApiRequestHandlers::testUnroutablePayloadClosesSession() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());
    QSignalSpy terminatedSpy(&session, &ApiSession::terminated);

    // A client-sent Ack is not a routable request -> INVALID_REQUEST + close.
    pb::ApiMessage bad;
    bad.set_request_id(9);
    bad.mutable_ack();
    transport->injectMessage(serialize(bad));

    pb::ApiMessage resp = parse(transport->sent.last());
    QCOMPARE(resp.payload_case(), pb::ApiMessage::kError);
    QCOMPARE(resp.error().code(), pb::ERROR_CODE_INVALID_REQUEST);
    QCOMPARE(terminatedSpy.count(), 1);
    QCOMPARE(session.state(), ApiSession::State::Closed);
}

void TestApiRequestHandlers::testLivenessExpiryClearsReportingRole() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});
    qint64 fakeNow = 0;
    handler.setLivenessNowFnForTest([&] { return fakeNow; });

    auto* transport = new FakeTransport();
    ApiSessionDeps deps; deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());
    transport->injectMessage(batteryReport(80, true));
    transport->injectMessage(gpsReport(45.0, -93.0));
    QVERIFY(inbound.connected());
    QCOMPARE(inbound.phoneBattery(), 80);
    QVERIFY(inbound.gpsValid());

    fakeNow = 30001;   // strictly past the 30 s default threshold
    handler.expireStaleReportingSessions();
    QVERIFY(!inbound.connected());
    QCOMPARE(inbound.phoneBattery(), -1);
    QVERIFY(!inbound.gpsValid());
}

void TestApiRequestHandlers::testLivenessExpiryTearsProxyRoute() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});
    qint64 fakeNow = 0;
    handler.setLivenessNowFnForTest([&] { return fakeNow; });

    auto* transport = new FakeTransport();
    ApiSessionDeps deps; deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());
    transport->injectMessage(connectivityReport(true, true, 1080));
    QVERIFY(inbound.proxyActive());

    QSignalSpy routeSpy(&inbound, &ApiInboundState::proxyRouteChanged);
    fakeNow = 30001;
    handler.expireStaleReportingSessions();
    QVERIFY(!inbound.proxyActive());
    QVERIFY(routeSpy.count() >= 1);
    QCOMPARE(routeSpy.last().at(0).toBool(), false);   // route torn down
}

void TestApiRequestHandlers::testLivenessExpirySparesNonReportingRoles() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});
    qint64 fakeNow = 0;
    handler.setLivenessNowFnForTest([&] { return fakeNow; });

    auto* transport = new FakeTransport();
    ApiSessionDeps deps; deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());

    pb::ApiMessage reg;
    reg.set_request_id(5);
    reg.mutable_register_actions_request()->add_actions()->set_id("testapp.hello");
    transport->injectMessage(serialize(reg));
    transport->injectMessage(batteryReport(50, false));

    fakeNow = 30001;
    handler.expireStaleReportingSessions();
    QVERIFY(!inbound.connected());                       // reporting role expired...
    QCOMPARE(session.state(), ApiSession::State::Ready);  // ...but the session lives
    QVERIFY(actions.registeredActions().contains("testapp.hello"));  // ...and keeps its action
}

void TestApiRequestHandlers::testLivenessRevivalOnNextReport() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});
    qint64 fakeNow = 0;
    handler.setLivenessNowFnForTest([&] { return fakeNow; });

    auto* transport = new FakeTransport();
    ApiSessionDeps deps; deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());
    transport->injectMessage(batteryReport(80, true));

    fakeNow = 30001;
    handler.expireStaleReportingSessions();
    QVERIFY(!inbound.connected());

    transport->injectMessage(batteryReport(75, true));   // wedged phone woke up
    QVERIFY(inbound.connected());
    QCOMPARE(inbound.phoneBattery(), 75);
}

void TestApiRequestHandlers::testLivenessPerSessionIndependence() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});
    qint64 fakeNow = 0;
    handler.setLivenessNowFnForTest([&] { return fakeNow; });

    auto* transportA = new FakeTransport();
    ApiSessionDeps depsA; depsA.requests = &handler;
    ApiSession sessionA(transportA, depsA);
    transportA->injectMessage(clientHello());
    transportA->injectMessage(batteryReport(80, true));

    auto* transportB = new FakeTransport();
    ApiSessionDeps depsB; depsB.requests = &handler;
    ApiSession sessionB(transportB, depsB);
    fakeNow = 20000;
    transportB->injectMessage(clientHello());
    transportB->injectMessage(gpsReport(45.0, -93.0));

    fakeNow = 31000;   // A is 31 s stale, B only 11 s
    handler.expireStaleReportingSessions();
    QCOMPARE(inbound.phoneBattery(), -1);   // A's battery cleared
    QVERIFY(inbound.gpsValid());            // B's GPS intact
    QVERIFY(inbound.connected());           // presence survives via B
}

void TestApiRequestHandlers::testLivenessBoundaryNotExpiredAtThreshold() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});
    qint64 fakeNow = 0;
    handler.setLivenessNowFnForTest([&] { return fakeNow; });

    auto* transport = new FakeTransport();
    ApiSessionDeps deps; deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());
    transport->injectMessage(batteryReport(80, true));

    fakeNow = 30000;   // exactly the threshold: NOT expired (strict >)
    handler.expireStaleReportingSessions();
    QVERIFY(inbound.connected());

    fakeNow = 30001;
    handler.expireStaleReportingSessions();
    QVERIFY(!inbound.connected());
}

void TestApiRequestHandlers::testLivenessTimerArmsAndDisarms() {
    oap::ActionRegistry actions;
    oap::NotificationService notifications;
    oap::PhoneStateService phone;
    ApiInboundState inbound;
    ApiRequestHandlers handler({&actions, &notifications, &phone, &inbound});

    QVERIFY(!handler.livenessTimerActiveForTest());   // idle until a report

    auto* transport = new FakeTransport();
    ApiSessionDeps deps; deps.requests = &handler;
    ApiSession session(transport, deps);
    transport->injectMessage(clientHello());
    transport->injectMessage(batteryReport(80, true));
    QVERIFY(handler.livenessTimerActiveForTest());

    transport->close();   // session teardown -> reporting set empties
    QVERIFY(!handler.livenessTimerActiveForTest());
}

QTEST_MAIN(TestApiRequestHandlers)
#include "test_api_request_handlers.moc"
