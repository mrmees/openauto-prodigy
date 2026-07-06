#pragma once

// ApiSession — per-connection handshake state machine, subscription routing,
// and outbound backpressure enforcement for the External API v1.
//
// One ApiSession owns exactly one IApiTransport. It drives the handshake
// (ClientHello -> ServerHello, optionally via AuthRequired/AuthResponse or
// PairingChallenge/PairingResponse), gates every message against the current
// state, routes Ready-state requests (subscribe/unsubscribe/get_capabilities/
// ping handled inline, everything else forwarded to an IApiRequestSink), and
// enforces a per-client outbound byte cap: a slow consumer is DISCONNECTED,
// never buffered.
//
// Threading: lives entirely on the Qt main thread. Teardown is single-path
// and idempotent; terminated() is emitted exactly once. The transport may
// deliver a message and then close within one reentrant call frame — the
// session tolerates this.

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QSet>
#include <functional>
#include <optional>

#include "api/api.pb.h"

class QTimer;

namespace oap::api {

class IApiTransport;
class PairingManager;
class PairedClientStore;
class ApiSession;

// Everything the session needs injected; ApiServer fills this once.
struct ApiSessionDeps {
    PairingManager* pairing = nullptr;
    PairedClientStore* store = nullptr;
    // Topics currently servable + static capabilities snapshot builder.
    std::function<prodigy::api::v1::Capabilities()> capabilities;
    // Snapshot bytes for one topic, or empty if unavailable.
    std::function<QByteArray(prodigy::api::v1::Topic)> snapshotFor;
    // Non-handshake, non-subscribe requests (Task 11 handler). May be null.
    class IApiRequestSink* requests = nullptr;
    QString serverName;
    QString appVersion;
    qint64 maxQueueBytes = 1048576;
    int handshakeTimeoutMs = 5000;
};

// Interface Task 11 implements; session forwards routed requests here.
class IApiRequestSink {
public:
    virtual ~IApiRequestSink() = default;
    // ctx.requestId echoes into the response the sink sends via
    // session->sendMessage.
    virtual void handleRequest(ApiSession* session, quint64 requestId,
                               const prodigy::api::v1::ApiMessage& msg) = 0;
    virtual void sessionClosed(ApiSession* session) = 0;
};

class ApiSession : public QObject {
    Q_OBJECT
public:
    enum class State { ExpectHello, AuthPending, PairingPending, Ready, Closed };

    ApiSession(IApiTransport* transport /*ownership*/, ApiSessionDeps deps,
               QObject* parent = nullptr);

    State state() const { return state_; }
    QString clientId() const { return clientId_; }    // "" for localhost-trusted
    QString clientName() const { return clientName_; }
    bool subscribedTo(prodigy::api::v1::Topic t) const;

    void deliver(const QByteArray& envelopeBytes);           // enforces queue cap
    void sendMessage(quint64 requestId, prodigy::api::v1::ApiMessage msg);
    void closeWithError(quint64 requestId, prodigy::api::v1::ErrorCode code,
                        const QString& text);

    void setPeerTrustOverrideForTest(std::optional<bool> trusted) {
        peerTrustOverride_ = trusted;
    }

signals:
    void becameReady();
    void terminated();   // emitted exactly once, from the single teardown path

private slots:
    void onMessageReceived(const QByteArray& serialized);
    void onTransportClosed();
    void onHandshakeTimeout();

private:
    // Handshake state handlers.
    void handleExpectHello(const prodigy::api::v1::ApiMessage& m);
    void handleAuthPending(const prodigy::api::v1::ApiMessage& m);
    void handlePairingPending(const prodigy::api::v1::ApiMessage& m);
    void handleReady(const prodigy::api::v1::ApiMessage& m);

    // Ready-state inline handlers.
    void handleSubscribe(const prodigy::api::v1::ApiMessage& m);
    void handleUnsubscribe(const prodigy::api::v1::ApiMessage& m);
    void handleGetCapabilities(const prodigy::api::v1::ApiMessage& m);
    void handlePing(const prodigy::api::v1::ApiMessage& m);
    void forwardToSink(const prodigy::api::v1::ApiMessage& m);

    // Transitions / terminal messages.
    void goReady(quint64 requestId, const QString& grantedClientId);
    void sendAuthReject(quint64 requestId, const QString& reason);
    void teardown();

    // Low-level writes. writeOrTeardown enforces the queue cap; sendRaw is a
    // best-effort terminal write (Error / AuthReject) that never re-tears-down.
    void writeOrTeardown(const QByteArray& bytes);
    void sendRaw(const prodigy::api::v1::ApiMessage& msg);

    bool trusted() const;

    IApiTransport* transport_;
    ApiSessionDeps deps_;
    State state_ = State::ExpectHello;
    QTimer* handshakeTimer_ = nullptr;
    bool tornDown_ = false;

    std::optional<bool> peerTrustOverride_;

    // Handshake memory. nonce_ is the challenge WE issued; proofs are always
    // verified against it, never against anything client-supplied.
    QByteArray nonce_;
    QString authClientId_;   // client id we issued an AuthRequired to
    QString helloName_;
    int helloKind_ = 0;

    QString clientId_;
    QString clientName_;

    QSet<int> subscriptions_;   // prodigy::api::v1::Topic values
};

} // namespace oap::api
