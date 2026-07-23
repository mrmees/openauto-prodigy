#pragma once

// ApiServer — the External API v1 composition root. Owns the two listeners
// (TCP + WebSocket), enforces the peer-admission policy, wraps every accepted
// connection in a transport + ApiSession, builds the shared session
// dependencies (capabilities, per-topic snapshots, request sink), fans the
// five topic publishers out to subscribed sessions, and drives secure-code pairing
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
#include <QPointer>
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
class DisplayInfo;
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
    oap::DisplayInfo* display = nullptr;                    // nullable
};

class ApiServer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(bool pairingActive READ pairingActive NOTIFY pairingChanged)
    Q_PROPERTY(QString pairingCode READ pairingCode NOTIFY pairingChanged)
    Q_PROPERTY(QString pairingQrDataUri READ pairingQrDataUri NOTIFY pairingChanged)
public:
    explicit ApiServer(ApiServiceRefs refs, QObject* parent = nullptr);
    ~ApiServer() override;

    bool start();     // reads api.* config; false if disabled or both listens fail
    void stop();      // idempotent; destroys publishers before other teardown
    // Final application shutdown: stop sessions/listeners and detach actions
    // while the registry is still alive. start() can register them again.
    void shutdown();
    // True while at least one listener is bound. main.cpp exposes ApiService
    // unconditionally, so QML gates the pairing UI on this — a pairing window
    // on a non-running server is zombie UI (no listener to pair through).
    bool isRunning() const { return started_; }

    quint16 tcpPort() const;   // actual bound port (config 0 -> ephemeral)
    quint16 wsPort() const;
    ApiInboundState* inboundState();   // for main.cpp wiring (Task 13)

    Q_INVOKABLE void startPairing();   // also registered as action api.pairing.start
    Q_INVOKABLE void cancelPairing();  // also registered as action api.pairing.cancel
    bool pairingActive() const;
    QString pairingCode() const;
    // QR for the open pairing window ("" when closed). Rendered lazily per
    // read so it can never go stale against the window state, whatever order
    // pairingChanged consumers fire in.
    QString pairingQrDataUri() const;
    int sessionCount() const;

    // Companion-scanner contract (kept stable, additive-only):
    // prodigy://pair?host=&tcp=&ws=&code=&ssid=  — ssid percent-encoded
    // (Android can redact the AA-owned network's SSID, so the companion
    // persists it from the QR for reconnect). Pure static seam, unit-tested
    // like inApSubnet/peerAllowed below.
    static QString pairingQrPayload(const QString& host, quint16 tcpPort,
                                    quint16 wsPort, const QString& code,
                                    const QString& ssid);

    // Test seam: rebinds the paired-client store to a scratch path BEFORE
    // start(), so tests never touch ~/.openauto/api_clients.yaml.
    void setStorePathForTest(const QString& path);

    // Test seam: the live QR payload string (what pairingQrDataUri encodes),
    // so tests can pin the config-to-QR wiring without decoding the PNG.
    QString pairingQrPayloadForTest() const;

    // Peer-admission policy exposed as pure static seams for unit testing
    // (Task 15 addendum). inApSubnet() performs the v4-mapped-v6 normalization;
    // the 2-arg peerAllowed() is the full admission decision for an explicit
    // exposeLan flag. The private instance peerAllowed() below delegates to it
    // with exposeLan_ — behavior is identical to the pre-refactor member.
    static bool inApSubnet(const QHostAddress& addr);
    static bool peerAllowed(const QHostAddress& addr, bool exposeLan);

signals:
    void pairingChanged();
    void runningChanged();

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
    void registerPairingActions();
    void unregisterPairingActions();

    ApiServiceRefs refs_;
    QPointer<oap::ActionRegistry> actions_;
    bool pairingActionsRegistered_ = false;

    // Auth / pairing (created in ctor so api.pairing.* actions work pre-start()).
    std::unique_ptr<PairedClientStore> store_;
    QString storePath_;
    PairingManager* pairing_ = nullptr;
    ApiInboundState* inbound_ = nullptr;
    ApiRequestHandlers* handlers_ = nullptr;

    // Listeners (created in start()).
    QTcpServer* tcpServer_ = nullptr;
    QWebSocketServer* wsServer_ = nullptr;
    bool started_ = false;   // guards double-invocation of start(); see stop()

    QList<TopicPublisher*> publishers_;
    QList<ApiSession*> sessions_;

    // Config-derived, captured in start().
    bool exposeLan_ = false;
    qint64 maxQueueBytes_ = 1048576;
    int pairingTimeoutS_ = 120;
    int handshakeTimeoutMs_ = 5000;
    QString serverName_;
    QString pairingSsid_;   // connection.wifi_ap.ssid, read in start(); QR field
    QString appVersion_;
    QString serverId_;   // stable head-unit identity (v1.1); minted+persisted in start()
};

} // namespace oap::api
