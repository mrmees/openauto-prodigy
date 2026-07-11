#include "core/api/ApiRequestHandlers.hpp"

#include "core/api/ApiInboundState.hpp"
#include "core/services/ActionRegistry.hpp"
#include "core/services/INotificationService.hpp"
#include "core/services/IPhoneStateService.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QVariantMap>
#include <QDebug>
#include <QTimeZone>

#include <array>
#include <cmath>

namespace pb = prodigy::api::v1;

namespace oap::api {

namespace {

// Reserved head-unit prefixes — client registrations under these are rejected.
constexpr std::array<const char*, 9> kReservedPrefixes = {
    "app.", "aa.", "navbar.", "theme.", "media.", "phone.", "system.",
    "overlay.", "api."};

// Bare JSON value -> QVariant. Wrapping in a single-element array lets one
// parse path handle bare scalars ("85"), strings ("\"x\""), objects and arrays
// uniformly. Invalid/empty JSON yields an invalid QVariant.
QVariant jsonToVariant(const QString& json) {
    QJsonParseError err{};
    const QByteArray wrapped = QByteArrayLiteral("[") + json.toUtf8() +
                               QByteArrayLiteral("]");
    const QJsonDocument doc = QJsonDocument::fromJson(wrapped, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray() ||
        doc.array().isEmpty())
        return QVariant();
    return doc.array().at(0).toVariant();
}

// QVariant -> bare JSON string (the inverse of jsonToVariant): serialize as a
// one-element array and strip the enclosing brackets.
QString variantToJson(const QVariant& v) {
    const QJsonArray arr{QJsonValue::fromVariant(v)};
    QByteArray bytes = QJsonDocument(arr).toJson(QJsonDocument::Compact);
    if (bytes.size() >= 2 && bytes.front() == '[' && bytes.back() == ']')
        bytes = bytes.mid(1, bytes.size() - 2);
    return QString::fromUtf8(bytes);
}

// Request-scoped (non-fatal) Error — sent via sendMessage, never closeWithError
// (which tears the session down). Used for dismiss NOT_FOUND.
void sendError(ApiSession* session, quint64 requestId, pb::ErrorCode code,
               const QString& text) {
    pb::ApiMessage resp;
    auto* err = resp.mutable_error();
    err->set_code(code);
    err->set_message(text.toStdString());
    session->sendMessage(requestId, resp);
}

} // namespace

ApiRequestHandlers::ApiRequestHandlers(Deps deps, QObject* parent)
    : QObject(parent), deps_(deps) {}

bool ApiRequestHandlers::hasReservedPrefix(const QString& id) {
    for (const char* prefix : kReservedPrefixes)
        if (id.startsWith(QLatin1String(prefix)))
            return true;
    return false;
}

// ---- Routing ---------------------------------------------------------------

void ApiRequestHandlers::handleRequest(ApiSession* session, quint64 requestId,
                                       const pb::ApiMessage& msg) {
    switch (msg.payload_case()) {
        case pb::ApiMessage::kListActionsRequest:
            handleListActions(session, requestId); break;
        case pb::ApiMessage::kDispatchActionRequest:
            handleDispatchAction(session, requestId, msg); break;
        case pb::ApiMessage::kRegisterActionsRequest:
            handleRegisterActions(session, requestId, msg); break;
        case pb::ApiMessage::kUnregisterActionsRequest:
            handleUnregisterActions(session, requestId, msg); break;

        case pb::ApiMessage::kPostNotificationRequest:
            handlePostNotification(session, requestId, msg); break;
        case pb::ApiMessage::kDismissNotificationRequest:
            handleDismissNotification(session, requestId, msg); break;

        case pb::ApiMessage::kDialRequest:
            handlePhoneCommand(session, requestId, PhoneOp::Dial,
                               QString::fromStdString(msg.dial_request().number()));
            break;
        case pb::ApiMessage::kAnswerCallRequest:
            handlePhoneCommand(session, requestId, PhoneOp::Answer, QString());
            break;
        case pb::ApiMessage::kHangupRequest:
            handlePhoneCommand(session, requestId, PhoneOp::Hangup, QString());
            break;
        case pb::ApiMessage::kSendDtmfRequest:
            handlePhoneCommand(session, requestId, PhoneOp::SendDtmf,
                               QString::fromStdString(msg.send_dtmf_request().tones()));
            break;

        case pb::ApiMessage::kGpsReport:
        case pb::ApiMessage::kBatteryReport:
        case pb::ApiMessage::kConnectivityReport:
        case pb::ApiMessage::kTimeReport:
            handleReport(session, msg);   // fire-and-forget: never responds
            break;

        default:
            session->closeWithError(requestId, pb::ERROR_CODE_INVALID_REQUEST,
                                    QStringLiteral("unroutable payload"));
            break;
    }
}

void ApiRequestHandlers::sessionClosed(ApiSession* session) {
    // Auto-unregister every action this session owned (the invariant that a
    // client action is dead once its session is gone). Iterate a snapshot of
    // the keys since we mutate the map inside the loop.
    const QList<QString> ids = clientOwners_.keys();
    for (const QString& id : ids) {
        if (clientOwners_.value(id) == session) {
            if (deps_.actions) deps_.actions->unregisterAction(id);
            clientOwners_.remove(id);
            clientLabels_.remove(id);
        }
    }
    // Notifications are not auto-dismissed on disconnect, but ownership
    // tracking for this (now dead) session is dropped.
    notificationOwners_.remove(session);

    // Clear every report type this session owned (legacy
    // CompanionListenerService parity: clearClientSession() drops everything
    // the departing companion had reported). A non-owner closing must never
    // touch a report type it doesn't own.
    if (session == gpsOwner_) {
        gpsOwner_ = nullptr;
        if (deps_.inbound) deps_.inbound->clearGps();
    }
    if (session == batteryOwner_) {
        batteryOwner_ = nullptr;
        if (deps_.inbound) deps_.inbound->clearBattery();
    }
    if (session == connectivityOwner_) {
        connectivityOwner_ = nullptr;
        if (deps_.inbound)
            deps_.inbound->setConnectivity(QString(), false, 0, QString());
    }
    // Owner-presence follows the surviving owners (false once none remain).
    recomputeOwnerPresence();
}

void ApiRequestHandlers::recomputeOwnerPresence() {
    const bool present =
        gpsOwner_ != nullptr || batteryOwner_ != nullptr || connectivityOwner_ != nullptr;
    if (deps_.inbound)
        deps_.inbound->setOwnerPresent(present);
}

// ---- Actions ---------------------------------------------------------------

void ApiRequestHandlers::handleListActions(ApiSession* session, quint64 requestId) {
    pb::ApiMessage resp;
    auto* la = resp.mutable_list_actions_response();
    if (deps_.actions) {
        for (const QString& id : deps_.actions->registeredActions()) {
            auto* info = la->add_actions();
            info->set_id(id.toStdString());
            info->set_label(clientLabels_.value(id).toStdString());
            info->set_client_owned(clientOwners_.contains(id));
        }
    }
    session->sendMessage(requestId, resp);
}

void ApiRequestHandlers::handleDispatchAction(ApiSession* session, quint64 requestId,
                                              const pb::ApiMessage& msg) {
    const auto& req = msg.dispatch_action_request();
    const QString id = QString::fromStdString(req.id());
    QVariant payload;
    if (req.has_payload_json())
        payload = jsonToVariant(QString::fromStdString(req.payload_json()));

    const bool dispatched = deps_.actions && deps_.actions->dispatch(id, payload);

    pb::ApiMessage resp;
    resp.mutable_dispatch_action_response()->set_dispatched(dispatched);
    session->sendMessage(requestId, resp);
}

void ApiRequestHandlers::handleRegisterActions(ApiSession* session, quint64 requestId,
                                               const pb::ApiMessage& msg) {
    const auto& req = msg.register_actions_request();
    pb::ApiMessage resp;
    auto* rr = resp.mutable_register_actions_response();

    for (const auto& spec : req.actions()) {
        const QString id = QString::fromStdString(spec.id());
        const QString label = QString::fromStdString(spec.label());
        auto* result = rr->add_results();
        result->set_id(spec.id());

        if (!deps_.actions) {
            result->set_accepted(false);
            result->set_reason("registry unavailable");
            continue;
        }
        if (deps_.actions->registeredActions().contains(id)) {
            result->set_accepted(false);
            result->set_reason("duplicate id");
            continue;
        }
        if (hasReservedPrefix(id)) {
            result->set_accepted(false);
            result->set_reason("reserved prefix");
            continue;
        }

        deps_.actions->registerAction(id, [this, id](const QVariant& payload) {
            forwardInvocation(id, payload);
        });
        clientOwners_[id] = session;
        clientLabels_[id] = label;
        result->set_accepted(true);
    }
    session->sendMessage(requestId, resp);
}

void ApiRequestHandlers::handleUnregisterActions(ApiSession* session, quint64 requestId,
                                                 const pb::ApiMessage& msg) {
    const auto& req = msg.unregister_actions_request();
    for (const auto& idStr : req.ids()) {
        const QString id = QString::fromStdString(idStr);
        // Only remove ids this session actually owns.
        if (clientOwners_.value(id, nullptr) == session) {
            if (deps_.actions) deps_.actions->unregisterAction(id);
            clientOwners_.remove(id);
            clientLabels_.remove(id);
        }
    }
    pb::ApiMessage resp;
    resp.mutable_ack();
    session->sendMessage(requestId, resp);
}

void ApiRequestHandlers::forwardInvocation(const QString& id, const QVariant& payload) {
    ApiSession* owner = clientOwners_.value(id, nullptr);
    if (!owner) return;   // owner disconnected — should already be unregistered
    pb::ApiMessage msg;
    auto* ev = msg.mutable_action_invoked();
    ev->set_id(id.toStdString());
    if (payload.isValid() && !payload.isNull())
        ev->set_payload_json(variantToJson(payload).toStdString());
    owner->sendMessage(0, msg);   // server-initiated -> request_id 0
}

// ---- Notifications ---------------------------------------------------------

void ApiRequestHandlers::handlePostNotification(ApiSession* session, quint64 requestId,
                                                const pb::ApiMessage& msg) {
    const auto& req = msg.post_notification_request();

    QVariantMap map;
    map["kind"] = "toast";
    map["message"] = QString::fromStdString(req.message());
    const QString clientId =
        session->clientId().isEmpty() ? QStringLiteral("localhost") : session->clientId();
    map["sourcePluginId"] = QStringLiteral("api:") + clientId;
    // proto3 optional: an explicit 0 reaches the service as 0; ABSENT -> 50.
    map["priority"] = req.has_priority()
                          ? static_cast<int>(qBound(0u, req.priority(), 100u))
                          : 50;
    map["ttlMs"] = static_cast<int>(req.ttl_ms());

    QString notifId;
    if (deps_.notifications)
        notifId = deps_.notifications->post(map);
    notificationOwners_[session].insert(notifId);

    pb::ApiMessage resp;
    resp.mutable_post_notification_response()->set_notification_id(notifId.toStdString());
    session->sendMessage(requestId, resp);
}

void ApiRequestHandlers::handleDismissNotification(ApiSession* session, quint64 requestId,
                                                   const pb::ApiMessage& msg) {
    const QString id = QString::fromStdString(msg.dismiss_notification_request().notification_id());
    auto it = notificationOwners_.find(session);
    if (it != notificationOwners_.end() && it->contains(id)) {
        if (deps_.notifications) deps_.notifications->dismiss(id);
        it->remove(id);
        pb::ApiMessage resp;
        resp.mutable_ack();
        session->sendMessage(requestId, resp);
    } else {
        // Unknown id or a notification this session does not own -> NOT_FOUND
        // (deliberately indistinguishable). Request-scoped: no disconnect.
        sendError(session, requestId, pb::ERROR_CODE_NOT_FOUND,
                  QStringLiteral("notification not found"));
    }
}

// ---- Phone commands --------------------------------------------------------

pb::PhoneCommandResult ApiRequestHandlers::phoneCommand(PhoneOp op, const QString& arg,
                                                        QString* detail) {
    // Capability flag false -> UNAVAILABLE (never contradicts the snapshot's
    // PhoneCapabilities, which mirror telephonyAvailable()).
    if (!deps_.phone || !deps_.phone->telephonyAvailable()) {
        *detail = QStringLiteral("telephony unavailable");
        return pb::PHONE_COMMAND_RESULT_UNAVAILABLE;
    }
    bool dispatched = false;
    switch (op) {
        case PhoneOp::Dial:     dispatched = deps_.phone->dial(arg); break;
        case PhoneOp::Answer:   dispatched = deps_.phone->answer(); break;
        case PhoneOp::Hangup:   dispatched = deps_.phone->hangup(); break;
        case PhoneOp::SendDtmf: dispatched = deps_.phone->sendDtmf(arg); break;
    }
    if (!dispatched) {
        // Capability was true but the call-state guard rejected the command.
        *detail = QStringLiteral("rejected in current call state");
        return pb::PHONE_COMMAND_RESULT_FAILED;
    }
    return pb::PHONE_COMMAND_RESULT_OK;
}

void ApiRequestHandlers::handlePhoneCommand(ApiSession* session, quint64 requestId,
                                            PhoneOp op, const QString& arg) {
    QString detail;
    const pb::PhoneCommandResult result = phoneCommand(op, arg, &detail);

    pb::ApiMessage resp;
    auto* pcr = resp.mutable_phone_command_response();
    pcr->set_result(result);
    if (!detail.isEmpty())
        pcr->set_detail(detail.toStdString());
    session->sendMessage(requestId, resp);
}

// ---- Inbound companion reports (fire-and-forget, no response) ---------------

void ApiRequestHandlers::handleReport(ApiSession* session, const pb::ApiMessage& msg) {
    if (!deps_.inbound) return;

    switch (msg.payload_case()) {
        case pb::ApiMessage::kGpsReport: {
            const auto& r = msg.gps_report();
            const double lat = r.latitude();
            const double lon = r.longitude();
            if (!std::isfinite(lat) || !std::isfinite(lon) ||
                lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
                qWarning() << "API: dropping malformed GpsReport lat=" << lat
                           << "lon=" << lon;
                return;
            }
            deps_.inbound->setGps(lat, lon, r.speed_mps(), r.bearing_deg(),
                                  r.accuracy_m(), r.age_ms());
            gpsOwner_ = session;
            recomputeOwnerPresence();
            break;
        }
        case pb::ApiMessage::kBatteryReport: {
            const auto& r = msg.battery_report();
            if (r.percent() > 100) {
                qWarning() << "API: dropping out-of-range BatteryReport percent="
                           << r.percent();
                return;
            }
            deps_.inbound->setBattery(static_cast<int>(r.percent()), r.charging());
            batteryOwner_ = session;
            recomputeOwnerPresence();
            break;
        }
        case pb::ApiMessage::kConnectivityReport: {
            const auto& r = msg.connectivity_report();
            // Route is advertised only when the phone has upstream internet AND
            // the proxy is up; the raw internet_available bit is not separately
            // exposed in v1 (a running proxy with no upstream internet is not a
            // usable route).
            const bool active = r.internet_available() && r.socks5_active();
            const uint32_t port = r.socks5_port();
            const QString host = session->peerHost();   // proxy host = peer
            if (active && (port == 0 || port > 65535 || host.isEmpty())) {
                qWarning() << "API: dropping malformed ConnectivityReport port="
                           << port << "host=" << host;
                return;
            }
            const QString password = r.has_socks5_password()
                                         ? QString::fromStdString(r.socks5_password())
                                         : QString();
            deps_.inbound->setConnectivity(host, active,
                                           static_cast<quint16>(port), password);
            // Route ownership follows the reporting session: an active report
            // claims ownership; an inactive report releases it (whoever
            // reports inactive is the last writer, matching the existing
            // last-writer-wins global-state model).
            connectivityOwner_ = active ? session : nullptr;
            recomputeOwnerPresence();
            break;
        }
        case pb::ApiMessage::kTimeReport: {
            const auto& r = msg.time_report();
            const qint64 t = r.unix_time_ms();
            if (t <= 0) {
                qWarning() << "API: dropping malformed TimeReport unix_time_ms=" << t;
                return;
            }
            deps_.inbound->setTime(t);

            // timezone_id is optional (v1.1) -- validate and forward
            // separately; an invalid zone drops ONLY the zone, the time
            // report above still applies.
            if (r.has_timezone_id()) {
                const QByteArray tz = QByteArray::fromStdString(r.timezone_id());
                if (QTimeZone::isTimeZoneIdAvailable(tz)) {
                    deps_.inbound->setTimezone(QString::fromUtf8(tz));
                } else {
                    qWarning() << "API: dropping malformed TimeReport.timezone_id="
                               << tz;
                }
            }
            break;
        }
        default:
            break;   // unreachable (routed only for the four report cases)
    }
}

} // namespace oap::api
