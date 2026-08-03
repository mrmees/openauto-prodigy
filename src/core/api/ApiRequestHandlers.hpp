#pragma once

// ApiRequestHandlers — the IApiRequestSink implementation. ApiSession routes
// every Ready-state request it does not handle inline (subscribe/caps/ping)
// to this bridge, which fans them out to the head unit's real services:
//
//   * Actions      -> oap::ActionRegistry (list/dispatch/register/unregister).
//                     Client-registered actions are owned by the session that
//                     created them and auto-unregistered on disconnect.
//   * Notifications -> oap::INotificationService (post/dismiss). A session may
//                     dismiss only what it posted.
//   * Phone cmds   -> oap::IPhoneStateService, gated by telephonyAvailable():
//                     a capability flag and a command result never contradict
//                     (frozen phone.proto contract).
//   * Reports      -> ApiInboundState. Fire-and-forget: NO response is ever
//                     sent; malformed reports are logged and dropped.
//
// Anything unroutable closes the session with INVALID_REQUEST.
// Main thread only.

#include <QObject>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVariant>
#include <QTimer>
#include <QElapsedTimer>

#include <functional>

#include "core/api/ApiSession.hpp"   // ApiSession, IApiRequestSink, api.pb.h

namespace oap {
class ActionRegistry;
namespace data { class DataRegistry; }
class INotificationService;
class IPhoneStateService;
}

namespace oap::api {

class ApiInboundState;
class ApiDataBridge;

class ApiRequestHandlers : public QObject, public IApiRequestSink {
    Q_OBJECT
public:
    struct Deps {
        oap::ActionRegistry* actions = nullptr;
        oap::INotificationService* notifications = nullptr;
        oap::IPhoneStateService* phone = nullptr;
        ApiInboundState* inbound = nullptr;
        oap::data::DataRegistry* dataRegistry = nullptr;
    };
    explicit ApiRequestHandlers(Deps deps, QObject* parent = nullptr);

    // IApiRequestSink
    void handleRequest(ApiSession* session, quint64 requestId,
                       const prodigy::api::v1::ApiMessage& msg) override;
    void sessionClosed(ApiSession* session) override;

    // Liveness expiry (B2 design §5): a reporting session whose last accepted
    // report is older than the threshold loses its reporting role — presence,
    // per-type report ownership, cached inbound state, proxy route. Its
    // registered actions, notifications, and the socket itself are untouched;
    // a later accepted report re-registers it exactly like a first report.
    // The sweep runs on a coarse timer only while reporting sessions exist.
    // Threshold = 30 s (~30 missed beats at the companion's ~1 Hz contract
    // cadence, symmetric with the GPS staleness window).
    void setLivenessThresholdMs(int ms) { livenessThresholdMs_ = ms; }
    void setLivenessNowFnForTest(std::function<qint64()> fn) { livenessNow_ = std::move(fn); }
    bool livenessTimerActiveForTest() const { return livenessTimer_.isActive(); }
    void expireStaleReportingSessions();

private:
    enum class PhoneOp { Dial, Answer, Hangup, SendDtmf };

    void handleListActions(ApiSession* session, quint64 requestId);
    void handleDispatchAction(ApiSession* session, quint64 requestId,
                              const prodigy::api::v1::ApiMessage& msg);
    void handleRegisterActions(ApiSession* session, quint64 requestId,
                               const prodigy::api::v1::ApiMessage& msg);
    void handleUnregisterActions(ApiSession* session, quint64 requestId,
                                 const prodigy::api::v1::ApiMessage& msg);
    void handlePostNotification(ApiSession* session, quint64 requestId,
                                const prodigy::api::v1::ApiMessage& msg);
    void handleDismissNotification(ApiSession* session, quint64 requestId,
                                   const prodigy::api::v1::ApiMessage& msg);
    void handlePhoneCommand(ApiSession* session, quint64 requestId, PhoneOp op,
                            const QString& arg);
    void handleReport(ApiSession* session, const prodigy::api::v1::ApiMessage& msg);

    // Capability flag false -> UNAVAILABLE (never contradicts the snapshot).
    // Guard-rejected dispatch (wrong call state) -> FAILED. Dispatched -> OK.
    prodigy::api::v1::PhoneCommandResult phoneCommand(PhoneOp op, const QString& arg,
                                                      QString* detail);
    void forwardInvocation(const QString& id, const QVariant& payload);
    static bool hasReservedPrefix(const QString& id);
    // Re-derive `connected` from the set of live reporting sessions: present
    // whenever any session has sourced a report and not yet closed. This is
    // presence, deliberately decoupled from route ownership below — an inactive
    // connectivity or time-only report still marks a companion present.
    void recomputeOwnerPresence();

    // Strip the session's reporting role: presence-set membership, per-type
    // report ownership + the cached state each owned (GPS, battery, proxy
    // route). Shared by sessionClosed() and liveness expiry. Never touches
    // actions/notifications/the socket.
    void clearReportingState(ApiSession* session);
    // Accepted-report bookkeeping: presence set + liveness stamp + timer arm.
    void noteReportAccepted(ApiSession* session);
    void updateLivenessTimer();
    qint64 livenessNowMs() const;

    Deps deps_;
    ApiDataBridge* dataBridge_ = nullptr;
    QHash<QString, ApiSession*> clientOwners_;   // action id -> owning session
    QHash<QString, QString> clientLabels_;        // action id -> display label
    QHash<ApiSession*, QSet<QString>> notificationOwners_;
    // Presence set: every session that has delivered any accepted companion
    // report (GPS, battery, connectivity — active or not — or time). Drives
    // `connected`; a session is removed on close or when its reporting role
    // expires (liveness). Separate from the per-report owners below, which
    // govern cached-state teardown and proxy-route ownership, NOT presence.
    QSet<ApiSession*> reportingSessions_;
    // Per-report-type ownership = last session to source that report; cleared —
    // with the cached state — by clearReportingState() when the owning session
    // closes or expires.
    ApiSession* gpsOwner_ = nullptr;
    ApiSession* batteryOwner_ = nullptr;
    ApiSession* connectivityOwner_ = nullptr;

    QHash<ApiSession*, qint64> lastReportMs_;   // monotonic ms of last accepted report
    int livenessThresholdMs_ = 30000;
    QTimer livenessTimer_;
    QElapsedTimer livenessClock_;
    std::function<qint64()> livenessNow_;       // test seam; default = livenessClock_
};

} // namespace oap::api
