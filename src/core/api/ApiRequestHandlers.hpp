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

#include "core/api/ApiSession.hpp"   // ApiSession, IApiRequestSink, api.pb.h

namespace oap {
class ActionRegistry;
class INotificationService;
class IPhoneStateService;
}

namespace oap::api {

class ApiInboundState;

class ApiRequestHandlers : public QObject, public IApiRequestSink {
    Q_OBJECT
public:
    struct Deps {
        oap::ActionRegistry* actions = nullptr;
        oap::INotificationService* notifications = nullptr;
        oap::IPhoneStateService* phone = nullptr;
        ApiInboundState* inbound = nullptr;
    };
    explicit ApiRequestHandlers(Deps deps, QObject* parent = nullptr);

    // IApiRequestSink
    void handleRequest(ApiSession* session, quint64 requestId,
                       const prodigy::api::v1::ApiMessage& msg) override;
    void sessionClosed(ApiSession* session) override;

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
    // Re-derive `connected` from the live report owners: present when any
    // report type still has an owning session (connectivity owner counts).
    void recomputeOwnerPresence();

    Deps deps_;
    QHash<QString, ApiSession*> clientOwners_;   // action id -> owning session
    QHash<QString, QString> clientLabels_;        // action id -> display label
    QHash<ApiSession*, QSet<QString>> notificationOwners_;
    // Per-report-type ownership = last session to source that report; cleared
    // (and the cached state torn down) when that session closes. Legacy
    // CompanionListenerService parity (clearClientSession() clears everything
    // the departing companion had reported).
    ApiSession* gpsOwner_ = nullptr;
    ApiSession* batteryOwner_ = nullptr;
    ApiSession* connectivityOwner_ = nullptr;
};

} // namespace oap::api
