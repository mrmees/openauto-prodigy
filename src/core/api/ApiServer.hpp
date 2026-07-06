#pragma once

// ApiServer — the External API v1 composition root. Owns the two listeners
// (TCP + WebSocket), enforces the peer-admission policy, wraps every accepted
// connection in a transport + ApiSession, builds the shared session
// dependencies (capabilities, per-topic snapshots, request sink), fans the
// five topic publishers out to subscribed sessions, and drives PIN pairing
// (exposed both as Q_INVOKABLE methods and as api.pairing.* actions).
//
// LIFETIME CONTRACT: every non-null pointer in ApiServiceRefs MUST outlive the
// server's *started* lifetime (from start() until stop()/destruction). The
// publishers created in start() hold raw provider pointers and each owns a
// 0-ms coalesce timer whose deferred buildEnvelope() reads its provider on the
// next event-loop turn — a provider destroyed while the server runs would be a
// use-after-free. stop() therefore destroys the publishers FIRST, before any
// other teardown, so no deferred snapshot can outlive a provider. In
// production (main.cpp, Task 13) the refs are app-lifetime singletons, so this
// holds trivially.
//
// Threading: main thread only (Qt event loop).

#include <QObject>
#include <QString>
#include <QList>
#include <QHostAddress>
#include <memory>

#include "core/api/ApiSession.hpp"        // ApiSessionDeps, ApiSession, pb types
#include "core/api/ApiInboundState.hpp"

class QTcpServer;
class QWebSocketServer;

namespace oap {
class IMediaStatusProvider;
class INavigationProvider;
class IProjectionStatusProvider;
class IPhoneStateService;
class ThemeService;
class INotificationService;
class ActionRegistry;
class IConfigService;
class BluetoothManager;
} // namespace oap

namespace oap::api {

class IApiTransport;
class TopicPublisher;
class PairingManager;
class PairedClientStore;
class ApiRequestHandlers;

// Filled once in main.cpp (Task 13). Nullable members degrade gracefully: a
// null provider means its topic is absent from Capabilities.supported_topics
// and its snapshot is unavailable (per-topic subscribe rejection).
struct ApiServiceRefs {
    oap::IMediaStatusProvider* media = nullptr;
    oap::INavigationProvider* navigation = nullptr;        // nullable
    oap::IProjectionStatusProvider* projection = nullptr;  // nullable
    oap::IPhoneStateService* phone = nullptr;
    oap::ThemeService* theme = nullptr;
    oap::INotificationService* notifications = nullptr;
    oap::ActionRegistry* actions = nullptr;
    oap::IConfigService* config = nullptr;
    oap::BluetoothManager* bluetooth = nullptr;            // nullable
};

class ApiServer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool pairingActive READ pairingActive NOTIFY pairingChanged)
    Q_PROPERTY(QString pairingPin READ pairingPin NOTIFY pairingChanged)
public:
    explicit ApiServer(ApiServiceRefs refs, QObject* parent = nullptr);
    ~ApiServer() override;

    bool start();     // reads api.* config; false if disabled or both listens fail
    void stop();      // idempotent; destroys publishers before other teardown

    quint16 tcpPort() const;   // actual bound port (config 0 -> ephemeral)
    quint16 wsPort() const;
    ApiInboundState* inboundState();   // for main.cpp wiring (Task 13)

    Q_INVOKABLE void startPairing();   // also registered as action api.pairing.start
    Q_INVOKABLE void cancelPairing();  // also registered as action api.pairing.cancel
    bool pairingActive() const;
    QString pairingPin() const;
    int sessionCount() const;

    // Test seam: rebinds the paired-client store to a scratch path BEFORE
    // start(), so tests never touch ~/.openauto/api_clients.yaml.
    void setStorePathForTest(const QString& path);

signals:
    void pairingChanged();

private slots:
    void onNewTcpConnection();
    void onNewWebSocketConnection();

private:
    bool peerAllowed(const QHostAddress& addr) const;
    void adoptSession(IApiTransport* transport);
    ApiSessionDeps buildDeps() const;
    prodigy::api::v1::Capabilities buildCapabilities() const;
    QByteArray snapshotFor(prodigy::api::v1::Topic t) const;
    void createPublishers();
    void wirePublisher(TopicPublisher* pub);
    void rebindPairing();

    ApiServiceRefs refs_;

    // Auth / pairing (created in ctor so api.pairing.* actions work pre-start()).
    std::unique_ptr<PairedClientStore> store_;
    QString storePath_;
    PairingManager* pairing_ = nullptr;
    ApiInboundState* inbound_ = nullptr;
    ApiRequestHandlers* handlers_ = nullptr;

    // Listeners (created in start()).
    QTcpServer* tcpServer_ = nullptr;
    QWebSocketServer* wsServer_ = nullptr;

    QList<TopicPublisher*> publishers_;
    QList<ApiSession*> sessions_;

    // Config-derived, captured in start().
    bool exposeLan_ = false;
    qint64 maxQueueBytes_ = 1048576;
    int pairingTimeoutS_ = 120;
    int handshakeTimeoutMs_ = 5000;
    QString serverName_;
    QString appVersion_;
};

} // namespace oap::api
