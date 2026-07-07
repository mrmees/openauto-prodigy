#include <QtTest>
#include <QSignalSpy>
#include <QHostAddress>

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
    void testConnectivityEmitsProxyRoute();
    void testTimeReportSignal();
    void testTimeReportValidTimezoneEmitsBoth();
    void testTimeReportInvalidTimezoneDropsZoneOnly();
    void testTimeReportNoTimezoneNoZoneSignal();
    void testUnroutablePayloadClosesSession();
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

QTEST_MAIN(TestApiRequestHandlers)
#include "test_api_request_handlers.moc"
