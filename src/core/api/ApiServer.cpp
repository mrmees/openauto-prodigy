#include "core/api/ApiServer.hpp"

#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QAbstractSocket>
#include <QDir>
#include <QVariant>

#include "core/api/ApiTransport.hpp"
#include "core/api/ApiPublishers.hpp"
#include "core/api/ApiRequestHandlers.hpp"
#include "core/api/PairingManager.hpp"
#include "core/api/ApiAuth.hpp"

#include "core/services/IConfigService.hpp"
#include "core/services/ActionRegistry.hpp"

namespace pb = prodigy::api::v1;

namespace oap::api {

namespace {

constexpr quint32 kMaxFrameBytes = 262144;

// AP subnet membership with v4-mapped-v6 normalization: a peer arriving as
// ::ffff:10.0.0.5 must be tested as the IPv4 it maps to. toIPv4Address()
// yields the embedded v4 (nonzero) for v4-mapped-v6 and 0 for a genuine IPv6.
bool inApSubnet(const QHostAddress& addr) {
    QHostAddress a = addr;
    if (a.protocol() != QAbstractSocket::IPv4Protocol) {
        const quint32 v4 = addr.toIPv4Address();
        if (v4 != 0)
            a = QHostAddress(v4);
    }
    return a.isInSubnet(QHostAddress(QStringLiteral("10.0.0.0")), 24);
}

} // namespace

ApiServer::ApiServer(ApiServiceRefs refs, QObject* parent)
    : QObject(parent), refs_(refs) {
    storePath_ = QDir::homePath() + QStringLiteral("/.openauto/api_clients.yaml");
    rebindPairing();

    inbound_ = new ApiInboundState(this);

    ApiRequestHandlers::Deps hd;
    hd.actions = refs_.actions;
    hd.notifications = refs_.notifications;
    hd.phone = refs_.phone;
    hd.inbound = inbound_;
    handlers_ = new ApiRequestHandlers(hd, this);

    // Expose pairing as actions too, so a UI button or a companion client can
    // open/close the window without a direct ApiServer handle. Registered in
    // the ctor (not start()) so dispatch works even before listening.
    if (refs_.actions) {
        refs_.actions->registerAction(QStringLiteral("api.pairing.start"),
            [this](const QVariant&) { startPairing(); });
        refs_.actions->registerAction(QStringLiteral("api.pairing.cancel"),
            [this](const QVariant&) { cancelPairing(); });
    }
}

ApiServer::~ApiServer() {
    stop();
    // Delete pairing_ before the store_ unique_ptr member is destroyed: it
    // holds a raw pointer into store_ (harmless if left dangling, but explicit
    // ordering keeps the invariant honest).
    delete pairing_;
    pairing_ = nullptr;
}

void ApiServer::rebindPairing() {
    delete pairing_;
    pairing_ = nullptr;
    store_ = std::make_unique<PairedClientStore>(storePath_);
    store_->load();
    pairing_ = new PairingManager(store_.get(), this);
    connect(pairing_, &PairingManager::windowChanged, this, &ApiServer::pairingChanged);
}

void ApiServer::setStorePathForTest(const QString& path) {
    storePath_ = path;
    rebindPairing();
}

bool ApiServer::start() {
    auto cfgInt = [this](const char* key, int def) {
        const QVariant v = refs_.config ? refs_.config->value(QString::fromLatin1(key))
                                        : QVariant();
        return v.isValid() ? v.toInt() : def;
    };
    auto cfgBool = [this](const char* key, bool def) {
        const QVariant v = refs_.config ? refs_.config->value(QString::fromLatin1(key))
                                        : QVariant();
        return v.isValid() ? v.toBool() : def;
    };
    auto cfgStr = [this](const char* key, const QString& def) {
        const QVariant v = refs_.config ? refs_.config->value(QString::fromLatin1(key))
                                        : QVariant();
        return v.isValid() ? v.toString() : def;
    };

    if (!cfgBool("api.enabled", true))
        return false;

    exposeLan_ = cfgBool("api.expose_lan", false);
    const QVariant mq = refs_.config
        ? refs_.config->value(QStringLiteral("api.max_queue_bytes")) : QVariant();
    maxQueueBytes_ = mq.isValid() ? mq.toLongLong() : 1048576;
    pairingTimeoutS_ = cfgInt("api.pairing_timeout_s", 120);
    handshakeTimeoutMs_ = cfgInt("api.handshake_timeout_ms", 5000);

    serverName_ = cfgStr("identity.head_unit_name", QStringLiteral("OpenAuto Prodigy"));
    const QString sw = cfgStr("identity.sw_version", QString());
    appVersion_ = sw + QStringLiteral(" (" OAP_GIT_HASH ")");

    const quint16 tcpPort = static_cast<quint16>(cfgInt("api.tcp_port", 9810));
    const quint16 wsPort  = static_cast<quint16>(cfgInt("api.ws_port", 9811));

    createPublishers();

    tcpServer_ = new QTcpServer(this);
    connect(tcpServer_, &QTcpServer::newConnection, this, &ApiServer::onNewTcpConnection);
    const bool tcpOk = tcpServer_->listen(QHostAddress::Any, tcpPort);

    wsServer_ = new QWebSocketServer(QStringLiteral("prodigy-api"),
                                     QWebSocketServer::NonSecureMode, this);
    connect(wsServer_, &QWebSocketServer::newConnection,
            this, &ApiServer::onNewWebSocketConnection);
    const bool wsOk = wsServer_->listen(QHostAddress::Any, wsPort);

    return tcpOk || wsOk;
}

void ApiServer::stop() {
    // 1. Destroy publishers FIRST. Each owns a 0-ms coalesce timer whose
    //    deferred buildEnvelope() reads its provider on the next event-loop
    //    turn; a provider may be torn down right after stop(), so no deferred
    //    snapshot may outlive it. Deleting a publisher cancels its timer and
    //    severs both its provider connections and its statusReady fan-out.
    qDeleteAll(publishers_);
    publishers_.clear();

    // 2. Stop accepting new connections.
    if (tcpServer_) { tcpServer_->close(); tcpServer_->deleteLater(); tcpServer_ = nullptr; }
    if (wsServer_)  { wsServer_->close();  wsServer_->deleteLater();  wsServer_ = nullptr; }

    // 3. Tear down live sessions. Clear the list first so any terminated()
    //    handler finds nothing to remove, notify the request sink so any
    //    client-owned actions/notifications are released (keeps a later
    //    start() clean), then delete. Deleting bypasses teardown()'s
    //    transport close, so sessionClosed() is invoked exactly once here.
    const QList<ApiSession*> live = sessions_;
    sessions_.clear();
    for (ApiSession* s : live)
        if (handlers_) handlers_->sessionClosed(s);
    qDeleteAll(live);
}

// ---- Peer admission --------------------------------------------------------

bool ApiServer::peerAllowed(const QHostAddress& addr) const {
    return addr.isLoopback() || inApSubnet(addr) || exposeLan_;
}

void ApiServer::onNewTcpConnection() {
    while (QTcpSocket* sock = tcpServer_->nextPendingConnection()) {
        if (!peerAllowed(sock->peerAddress())) {
            sock->abort();          // no protocol bytes to a rejected peer
            sock->deleteLater();
            continue;
        }
        adoptSession(new TcpApiTransport(sock, kMaxFrameBytes));
    }
}

void ApiServer::onNewWebSocketConnection() {
    while (QWebSocket* ws = wsServer_->nextPendingConnection()) {
        if (!peerAllowed(ws->peerAddress())) {
            ws->close();            // WS close frame only, no ApiMessage
            ws->deleteLater();
            continue;
        }
        adoptSession(new WsApiTransport(ws, kMaxFrameBytes));
    }
}

void ApiServer::adoptSession(IApiTransport* transport) {
    ApiSession* session = new ApiSession(transport, buildDeps(), this);
    sessions_.append(session);
    connect(session, &ApiSession::terminated, this, [this, session]() {
        sessions_.removeOne(session);
        session->deleteLater();
    });
}

// ---- Session dependencies --------------------------------------------------

ApiSessionDeps ApiServer::buildDeps() const {
    ApiSessionDeps deps;
    deps.pairing = pairing_;
    deps.store = store_.get();
    deps.requests = handlers_;
    deps.serverName = serverName_;
    deps.appVersion = appVersion_;
    deps.maxQueueBytes = maxQueueBytes_;
    deps.handshakeTimeoutMs = handshakeTimeoutMs_;
    deps.capabilities = [this]() { return buildCapabilities(); };
    deps.snapshotFor = [this](pb::Topic t) { return snapshotFor(t); };
    return deps;
}

pb::Capabilities ApiServer::buildCapabilities() const {
    pb::Capabilities caps;
    // supported_topics = exactly the topics with a live provider (publishers_
    // is only populated for non-null providers).
    for (TopicPublisher* pub : publishers_)
        caps.add_supported_topics(pub->topic());
    // Static phone-command surface shape: ALL-FALSE in v1 (api.proto
    // Capabilities.phone). RUNTIME availability lives in the truthful
    // PhoneCapabilities inside each PhoneStatus (Task 7) — clients MUST check
    // that before enabling call UI.
    caps.mutable_phone();
    return caps;
}

QByteArray ApiServer::snapshotFor(pb::Topic t) const {
    for (TopicPublisher* pub : publishers_)
        if (pub->topic() == t)
            return pub->snapshotBytes();
    return QByteArray();   // no provider for this topic -> per-topic rejection
}

// ---- Publishers ------------------------------------------------------------

void ApiServer::createPublishers() {
    if (refs_.media)      wirePublisher(new MediaPublisher(refs_.media, this));
    if (refs_.navigation) wirePublisher(new NavigationPublisher(refs_.navigation, this));
    if (refs_.projection) wirePublisher(new ProjectionPublisher(refs_.projection, this));
    if (refs_.phone)      wirePublisher(new PhonePublisher(refs_.phone, this));
    if (refs_.theme)      wirePublisher(new SystemPublisher(refs_.theme, appVersion_,
                                                            refs_.bluetooth, this));
}

void ApiServer::wirePublisher(TopicPublisher* pub) {
    publishers_.append(pub);
    connect(pub, &TopicPublisher::statusReady, this,
            [this](pb::Topic topic, const QByteArray& bytes) {
        // Iterate a COPY: deliver() may tear a slow session down, whose
        // terminated() handler mutates sessions_ mid-fan-out. A torn-down
        // session lingers (deleteLater) so its pointer stays valid this turn,
        // and deliver() no-ops once state != Ready. Belt-and-braces gate per
        // the plan even though deliver() re-checks internally.
        const QList<ApiSession*> targets = sessions_;
        for (ApiSession* s : targets)
            if (s->state() == ApiSession::State::Ready && s->subscribedTo(topic))
                s->deliver(bytes);
    });
}

// ---- Pairing / accessors ---------------------------------------------------

void ApiServer::startPairing() {
    if (pairing_) pairing_->startWindow(pairingTimeoutS_);
}

void ApiServer::cancelPairing() {
    if (pairing_) pairing_->cancelWindow();
}

bool ApiServer::pairingActive() const {
    return pairing_ && pairing_->windowOpen();
}

QString ApiServer::pairingPin() const {
    return pairing_ ? pairing_->currentPin() : QString();
}

int ApiServer::sessionCount() const {
    return static_cast<int>(sessions_.size());
}

ApiInboundState* ApiServer::inboundState() {
    return inbound_;
}

quint16 ApiServer::tcpPort() const {
    return tcpServer_ ? tcpServer_->serverPort() : 0;
}

quint16 ApiServer::wsPort() const {
    return wsServer_ ? wsServer_->serverPort() : 0;
}

} // namespace oap::api
