#include "core/api/ApiSession.hpp"

#include <QTimer>
#include <QUuid>
#include <QRandomGenerator>
#include <utility>

#include "core/api/ApiTransport.hpp"
#include "core/api/ApiAuth.hpp"
#include "core/api/PairingManager.hpp"

namespace pb = prodigy::api::v1;

namespace oap::api {

namespace {

QByteArray randomBytes(int count) {
    QByteArray bytes(count, '\0');
    for (int i = 0; i < count; ++i)
        bytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    return bytes;
}

// Map a status-event payload to its subscription Topic. Non-status payloads
// (or an unparseable envelope) map to TOPIC_UNSPECIFIED and are never
// delivered.
pb::Topic topicForPayload(pb::ApiMessage::PayloadCase c) {
    switch (c) {
        case pb::ApiMessage::kMediaStatus:      return pb::TOPIC_MEDIA;
        case pb::ApiMessage::kNavigationStatus: return pb::TOPIC_NAVIGATION;
        case pb::ApiMessage::kProjectionStatus: return pb::TOPIC_PROJECTION;
        case pb::ApiMessage::kPhoneStatus:      return pb::TOPIC_PHONE;
        case pb::ApiMessage::kSystemStatus:     return pb::TOPIC_SYSTEM;
        default:                                return pb::TOPIC_UNSPECIFIED;
    }
}

} // namespace

ApiSession::ApiSession(IApiTransport* transport, ApiSessionDeps deps, QObject* parent)
    : QObject(parent), transport_(transport), deps_(std::move(deps)) {
    // The session owns the transport for lifetime purposes; parent-child
    // destruction reclaims it. Teardown never deletes it synchronously.
    if (transport_) {
        transport_->setParent(this);
        connect(transport_, &IApiTransport::messageReceived,
                this, &ApiSession::onMessageReceived);
        connect(transport_, &IApiTransport::closed,
                this, &ApiSession::onTransportClosed);
    }

    handshakeTimer_ = new QTimer(this);
    handshakeTimer_->setSingleShot(true);
    connect(handshakeTimer_, &QTimer::timeout, this, &ApiSession::onHandshakeTimeout);
    handshakeTimer_->start(deps_.handshakeTimeoutMs);
}

bool ApiSession::trusted() const {
    return peerTrustOverride_.value_or(
        transport_ ? transport_->peerAddress().isLoopback() : false);
}

bool ApiSession::subscribedTo(pb::Topic t) const {
    return subscriptions_.contains(static_cast<int>(t));
}

QString ApiSession::peerHost() const {
    return transport_ ? transport_->peerAddress().toString() : QString();
}

// ---- Inbound dispatch ------------------------------------------------------

void ApiSession::onMessageReceived(const QByteArray& serialized) {
    if (state_ == State::Closed) return;

    pb::ApiMessage m;
    if (!m.ParseFromArray(serialized.constData(), serialized.size())) {
        closeWithError(0, pb::ERROR_CODE_INVALID_REQUEST, "parse failure");
        return;
    }

    switch (state_) {
        case State::ExpectHello:    handleExpectHello(m); break;
        case State::AuthPending:    handleAuthPending(m); break;
        case State::PairingPending: handlePairingPending(m); break;
        case State::Ready:          handleReady(m); break;
        case State::Closed:         break;
    }
}

void ApiSession::onTransportClosed() {
    teardown();
}

void ApiSession::onHandshakeTimeout() {
    closeWithError(0, pb::ERROR_CODE_INVALID_REQUEST, "handshake timeout");
}

// ---- Handshake -------------------------------------------------------------

void ApiSession::handleExpectHello(const pb::ApiMessage& m) {
    if (m.payload_case() != pb::ApiMessage::kClientHello) {
        closeWithError(0, pb::ERROR_CODE_INVALID_REQUEST, "expected client_hello");
        return;
    }

    const auto& hello = m.client_hello();
    if (hello.requested_api_version_major() != 1) {
        closeWithError(m.request_id(), pb::ERROR_CODE_UNSUPPORTED_VERSION,
                       "unsupported api version");
        return;
    }

    helloName_ = QString::fromStdString(hello.client_name());
    helloKind_ = static_cast<int>(hello.client_kind());

    if (trusted()) {
        clientId_.clear();
        clientName_ = helloName_;
        goReady(m.request_id(), QString());
        return;
    }

    const auto& auth = hello.auth();
    if (!auth.client_id().empty()) {
        QString cid = QString::fromStdString(auth.client_id());
        auto client = deps_.store ? deps_.store->find(cid) : std::optional<PairedClient>{};
        if (!client) {
            sendAuthReject(m.request_id(), "unknown client");
            return;
        }
        authClientId_ = cid;
        nonce_ = randomBytes(32);
        pb::ApiMessage challenge;
        challenge.mutable_auth_required()->set_nonce(nonce_.constData(), nonce_.size());
        sendMessage(m.request_id(), challenge);
        if (state_ == State::ExpectHello) state_ = State::AuthPending;
        return;
    }

    if (auth.pairing_request()) {
        if (!deps_.pairing || !deps_.pairing->windowOpen()) {
            sendAuthReject(m.request_id(), "pairing window closed");
            return;
        }
        nonce_ = deps_.pairing->makeNonce();
        QByteArray salt = deps_.pairing->currentSalt();
        pb::ApiMessage challenge;
        auto* pc = challenge.mutable_pairing_challenge();
        pc->set_nonce(nonce_.constData(), nonce_.size());
        pc->set_salt(salt.constData(), salt.size());
        sendMessage(m.request_id(), challenge);
        if (state_ == State::ExpectHello) state_ = State::PairingPending;
        return;
    }

    sendAuthReject(m.request_id(), "authentication required");
}

void ApiSession::handleAuthPending(const pb::ApiMessage& m) {
    if (m.payload_case() != pb::ApiMessage::kAuthResponse) {
        closeWithError(0, pb::ERROR_CODE_INVALID_REQUEST, "expected auth_response");
        return;
    }

    const auto& resp = m.auth_response();
    auto client = deps_.store ? deps_.store->find(authClientId_)
                              : std::optional<PairedClient>{};
    if (!client) {
        sendAuthReject(m.request_id(), "unknown client");
        return;
    }

    // Verify against the nonce WE issued — never anything client-supplied.
    QByteArray expected = hmacProof(client->secret, nonce_);
    QByteArray proof = QByteArray::fromStdString(resp.proof());
    if (constantTimeEquals(expected, proof)) {
        clientId_ = authClientId_;
        clientName_ = client->name;
        goReady(m.request_id(), QString());
    } else {
        sendAuthReject(m.request_id(), "authentication failed");
    }
}

void ApiSession::handlePairingPending(const pb::ApiMessage& m) {
    if (m.payload_case() != pb::ApiMessage::kPairingResponse) {
        closeWithError(0, pb::ERROR_CODE_INVALID_REQUEST, "expected pairing_response");
        return;
    }
    if (!deps_.pairing) {
        sendAuthReject(m.request_id(), "pairing unavailable");
        return;
    }

    const auto& resp = m.pairing_response();
    QByteArray proof = QByteArray::fromStdString(resp.proof());
    // completePairing verifies proof against the remembered nonce internally.
    auto granted = deps_.pairing->completePairing(nonce_, proof, helloName_, helloKind_);
    if (granted) {
        clientId_ = *granted;
        clientName_ = helloName_;
        goReady(m.request_id(), *granted);
    } else {
        sendAuthReject(m.request_id(), "pairing failed");
    }
}

void ApiSession::goReady(quint64 requestId, const QString& grantedClientId) {
    state_ = State::Ready;
    if (handshakeTimer_) handshakeTimer_->stop();

    pb::ApiMessage msg;
    auto* sh = msg.mutable_server_hello();
    sh->set_api_version_major(1);
    sh->set_api_version_minor(0);
    sh->set_server_name(deps_.serverName.toStdString());
    sh->set_app_version(deps_.appVersion.toStdString());
    sh->set_session_id(
        QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString());
    if (!grantedClientId.isEmpty())
        sh->set_granted_client_id(grantedClientId.toStdString());
    if (deps_.capabilities)
        *sh->mutable_capabilities() = deps_.capabilities();

    sendMessage(requestId, msg);
    if (state_ == State::Ready)
        emit becameReady();
}

// ---- Ready-state routing ---------------------------------------------------

void ApiSession::handleReady(const pb::ApiMessage& m) {
    switch (m.payload_case()) {
        case pb::ApiMessage::kSubscribeRequest:       handleSubscribe(m); break;
        case pb::ApiMessage::kUnsubscribeRequest:     handleUnsubscribe(m); break;
        case pb::ApiMessage::kGetCapabilitiesRequest: handleGetCapabilities(m); break;
        case pb::ApiMessage::kPing:                   handlePing(m); break;
        default:                                      forwardToSink(m); break;
    }
}

void ApiSession::handleSubscribe(const pb::ApiMessage& m) {
    const auto& req = m.subscribe_request();
    pb::ApiMessage resp;
    auto* sr = resp.mutable_subscribe_response();

    QList<QByteArray> pendingSnapshots;
    for (int i = 0; i < req.topics_size(); ++i) {
        pb::Topic t = req.topics(i);
        QByteArray snap = deps_.snapshotFor ? deps_.snapshotFor(t) : QByteArray();
        bool accepted = !snap.isEmpty();

        auto* result = sr->add_results();
        result->set_topic(t);
        result->set_accepted(accepted);
        if (accepted) {
            subscriptions_.insert(static_cast<int>(t));
            pendingSnapshots.append(snap);
        } else {
            result->set_reason("topic unavailable");
        }
    }

    // SubscribeResponse first, then a full snapshot for each accepted topic.
    sendMessage(m.request_id(), resp);
    for (const QByteArray& snap : pendingSnapshots) {
        if (tornDown_) return;
        writeOrTeardown(snap);
    }
}

void ApiSession::handleUnsubscribe(const pb::ApiMessage& m) {
    const auto& req = m.unsubscribe_request();
    for (int i = 0; i < req.topics_size(); ++i)
        subscriptions_.remove(static_cast<int>(req.topics(i)));

    pb::ApiMessage resp;
    resp.mutable_ack();
    sendMessage(m.request_id(), resp);
}

void ApiSession::handleGetCapabilities(const pb::ApiMessage& m) {
    pb::ApiMessage resp;
    auto* cr = resp.mutable_capabilities_response();
    if (deps_.capabilities)
        *cr->mutable_capabilities() = deps_.capabilities();
    sendMessage(m.request_id(), resp);
}

void ApiSession::handlePing(const pb::ApiMessage& m) {
    pb::ApiMessage resp;
    resp.mutable_pong();
    sendMessage(m.request_id(), resp);   // Pong echoes the request_id
}

void ApiSession::forwardToSink(const pb::ApiMessage& m) {
    quint64 id = m.request_id();
    if (!deps_.requests) {
        closeWithError(id, pb::ERROR_CODE_INTERNAL, "no handler");
        return;
    }
    deps_.requests->handleRequest(this, id, m);
}

// ---- Outbound status delivery ----------------------------------------------

void ApiSession::deliver(const QByteArray& envelopeBytes) {
    if (state_ != State::Ready) return;

    pb::ApiMessage m;
    if (!m.ParseFromArray(envelopeBytes.constData(), envelopeBytes.size()))
        return;

    pb::Topic t = topicForPayload(m.payload_case());
    if (t == pb::TOPIC_UNSPECIFIED || !subscribedTo(t))
        return;

    writeOrTeardown(envelopeBytes);
}

// ---- Writes ----------------------------------------------------------------

void ApiSession::sendMessage(quint64 requestId, pb::ApiMessage msg) {
    msg.set_request_id(requestId);
    std::string bytes;
    msg.SerializeToString(&bytes);
    writeOrTeardown(QByteArray::fromStdString(bytes));
}

void ApiSession::writeOrTeardown(const QByteArray& bytes) {
    if (tornDown_ || !transport_) return;
    if (transport_->bytesToWrite() + static_cast<qint64>(bytes.size())
            > deps_.maxQueueBytes) {
        teardown();   // slow consumer — disconnect, never buffer
        return;
    }
    transport_->sendMessage(bytes);
}

void ApiSession::sendRaw(const pb::ApiMessage& msg) {
    // Best-effort terminal write (Error / AuthReject). Never cap-checks (it
    // would re-enter teardown) and never writes after teardown.
    if (tornDown_ || !transport_) return;
    std::string bytes;
    msg.SerializeToString(&bytes);
    transport_->sendMessage(QByteArray::fromStdString(bytes));
}

void ApiSession::closeWithError(quint64 requestId, pb::ErrorCode code,
                                const QString& text) {
    pb::ApiMessage msg;
    msg.set_request_id(requestId);
    auto* err = msg.mutable_error();
    err->set_code(code);
    err->set_message(text.toStdString());
    sendRaw(msg);
    teardown();
}

void ApiSession::sendAuthReject(quint64 requestId, const QString& reason) {
    pb::ApiMessage msg;
    msg.set_request_id(requestId);
    msg.mutable_auth_reject()->set_reason(reason.toStdString());
    sendRaw(msg);
    teardown();
}

// ---- Teardown (single, idempotent path) ------------------------------------

void ApiSession::teardown() {
    if (tornDown_) return;   // guard: close paths can re-enter within one frame
    tornDown_ = true;
    state_ = State::Closed;
    if (handshakeTimer_) handshakeTimer_->stop();
    if (deps_.requests) deps_.requests->sessionClosed(this);
    if (transport_) transport_->close();   // may synchronously re-enter teardown
    emit terminated();                      // exactly once
}

} // namespace oap::api
