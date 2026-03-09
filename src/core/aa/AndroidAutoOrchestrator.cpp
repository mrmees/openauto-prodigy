#include "AndroidAutoOrchestrator.hpp"
#include "TimedNightMode.hpp"
#include "GpioNightMode.hpp"
#include "../../core/YamlConfig.hpp"
#include "../../core/Configuration.hpp"
#include "../../core/services/IAudioService.hpp"
#include "../../core/services/AudioService.hpp"
#include "../../core/services/IEventBus.hpp"
#include "../../core/services/EqualizerService.hpp"
#include "../../core/services/IThemeService.hpp"

#include <oaa/Messenger/Messenger.hpp>
#include <oaa/control/BatteryStatusMessage.pb.h>

#include <memory>
#include <chrono>
#include <QDir>
#include "../Logging.hpp"
#include <QEventLoop>
#include <QFileInfo>
#include <QNetworkInterface>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <fcntl.h>

#ifdef HAS_BLUETOOTH
#include "BluetoothDiscoveryService.hpp"
#endif

namespace oap {
namespace aa {

AndroidAutoOrchestrator::AndroidAutoOrchestrator(
    std::shared_ptr<oap::Configuration> config,
    oap::IAudioService* audioService,
    oap::YamlConfig* yamlConfig,
    oap::IEventBus* eventBus,
    oap::EqualizerService* eqService,
    QObject* parent)
    : QObject(parent)
    , config_(std::move(config))
    , audioService_(audioService)
    , yamlConfig_(yamlConfig)
    , eventBus_(eventBus)
    , eqService_(eqService)
{
    // Wire TouchHandler to InputChannelHandler
    touchHandler_.setHandler(&inputHandler_);

    // Give VideoDecoder access to config for hw decoder selection
    videoDecoder_.setYamlConfig(yamlConfig_);

    // WiFi handler needs SSID/password from config
    if (yamlConfig_) {
        wifiHandler_ = std::make_unique<oaa::hu::WiFiChannelHandler>(
            yamlConfig_->wifiSsid(), yamlConfig_->wifiPassword(), this);
    } else {
        wifiHandler_ = std::make_unique<oaa::hu::WiFiChannelHandler>(QString(), QString(), this);
    }
}

AndroidAutoOrchestrator::~AndroidAutoOrchestrator()
{
    stop();
}

void AndroidAutoOrchestrator::start()
{
    qCInfo(lcAA) << "Starting Android Auto service (wireless mode)";

    setState(WaitingForDevice, "Initializing...");

    // Start TCP listener
    uint16_t port = config_ ? config_->tcpPort() : 5288;

    // Disconnect first to prevent accumulation if start() is called multiple times
    disconnect(&tcpServer_, &QTcpServer::newConnection,
               this, &AndroidAutoOrchestrator::onNewConnection);
    connect(&tcpServer_, &QTcpServer::newConnection,
            this, &AndroidAutoOrchestrator::onNewConnection);

    if (!tcpServer_.listen(QHostAddress::Any, port)) {
        qCCritical(lcAA) << "Failed to listen on port" << port
                        << ":" << tcpServer_.errorString();
        setState(Disconnected, QString("TCP listen failed: %1").arg(tcpServer_.errorString()));
        return;
    }

    // Set FD_CLOEXEC on listener socket (prevent fork inheritance)
    auto fd = tcpServer_.socketDescriptor();
    if (fd != -1) {
        int flags = ::fcntl(fd, F_GETFD);
        if (flags >= 0)
            ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }

    qCInfo(lcAA) << "TCP listener started on port" << port;

#ifdef HAS_BLUETOOTH
    if (config_ && config_->wirelessEnabled()) {
        QString wifiIface = QStringLiteral("wlan0");
        if (yamlConfig_)
            wifiIface = yamlConfig_->wifiInterface();
        btDiscovery_ = new BluetoothDiscoveryService(config_, wifiIface, this);
        connect(btDiscovery_, &BluetoothDiscoveryService::phoneWillConnect,
                this, [this]() {
                    setState(Connecting, "Phone connecting via WiFi...");
                });
        connect(btDiscovery_, &BluetoothDiscoveryService::error,
                this, [this](const QString& msg) {
                    qCWarning(lcAA) << "BT error:" << msg;
                });
        btDiscovery_->start();
        qCInfo(lcAA) << "Bluetooth discovery started";
    }
#endif

    setState(WaitingForDevice,
             QString("Waiting for wireless connection on port %1...").arg(port));
}

void AndroidAutoOrchestrator::stop()
{
    qCInfo(lcAA) << "Stopping Android Auto service";

    stopConnectionWatchdog();

    // Graceful shutdown: send ShutdownRequest and wait for phone to acknowledge
    if (session_ && (state_ == Connected || state_ == Backgrounded)) {
        qCDebug(lcAA) << "Sending graceful shutdown to phone";
        session_->stop(7);  // POWER_DOWN — app is exiting

        // Spin a local event loop so the message goes out and we get the response
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        connect(session_, &oaa::AASession::disconnected, &loop, &QEventLoop::quit);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(2000);  // 2s max wait
        loop.exec();

        if (timeout.isActive()) {
            qCDebug(lcAA) << "Phone acknowledged shutdown";
        } else {
            qCDebug(lcAA) << "Shutdown timeout — proceeding with teardown";
        }
    }

    if (nightProvider_) {
        nightProvider_->stop();
        nightProvider_.reset();
    }

    teardownSession();

#ifdef HAS_BLUETOOTH
    if (btDiscovery_) {
        btDiscovery_->stop();
        delete btDiscovery_;
        btDiscovery_ = nullptr;
    }
#endif

    tcpServer_.close();
    setState(Disconnected, "Stopped");
}

void AndroidAutoOrchestrator::disconnectSession()
{
    if (!session_ || (state_ != Connected && state_ != Backgrounded)) {
        qCDebug(lcAA) << "disconnectSession: no active session";
        return;
    }

    qCInfo(lcAA) << "Disconnecting AA session (USER_SELECTION)";

    // Send ShutdownRequest with USER_SELECTION (reason 1) and return immediately.
    // The session has a 5s internal timeout; when the ack arrives (or times out),
    // it emits disconnected → onSessionDisconnected handles teardown normally.
    // No blocking event loop here — that causes re-entrancy crashes.
    session_->stop(1);
}

void AndroidAutoOrchestrator::disconnectAndRetrigger()
{
    pendingReconnect_ = true;
    disconnectSession();
}

void AndroidAutoOrchestrator::onNewConnection()
{
    QTcpSocket* socket = tcpServer_.nextPendingConnection();
    if (!socket) return;

    qCInfo(lcAA) << "Wireless AA connection from"
            << socket->peerAddress().toString()
            << ":" << socket->peerPort();

    // Tear down any existing session first (reconnect scenario)
    if (session_) {
        qCWarning(lcAA) << "Already have active connection — tearing down for reconnect";
        teardownSession();
    }

    setState(Connecting, QString("Wireless connection from %1...")
             .arg(socket->peerAddress().toString()));

    // TCP socket tuning
    auto fd = socket->socketDescriptor();
    if (fd != -1) {
        int yes = 1;
        // Disable Nagle — send touch events immediately, don't buffer small packets
        ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
        ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
        int idle = 5, interval = 3, count = 3;
        ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
        ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
        ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));

        // FD_CLOEXEC for child process safety
        int flags = ::fcntl(fd, F_GETFD);
        if (flags >= 0)
            ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }

    activeSocket_ = socket;

    // Create transport
    transport_ = new oaa::TCPTransport(this);
    transport_->setSocket(socket);
    transport_->start();

    // Build session config from YAML
    QString btMac = "00:00:00:00:00:00";
#ifdef HAS_BLUETOOTH
    if (btDiscovery_)
        btMac = btDiscovery_->localAddress();
#endif
    // Look up wlan0 MAC address for WiFi BSSID field
    QString wifiBssid;
    QString wifiIface = yamlConfig_ ? yamlConfig_->wifiInterface() : QStringLiteral("wlan0");
    for (const auto& iface : QNetworkInterface::allInterfaces()) {
        if (iface.name() == wifiIface) {
            wifiBssid = iface.hardwareAddress();
            break;
        }
    }

    ServiceDiscoveryBuilder builder(yamlConfig_, btMac,
                                     yamlConfig_ ? yamlConfig_->wifiSsid() : QString(),
                                     yamlConfig_ ? yamlConfig_->wifiPassword() : QString(),
                                     wifiBssid);
    if (displayW_ > 0 && displayH_ > 0)
        builder.setDisplayDimensions(displayW_, displayH_);
    builder.setNavbarThickness(navbarThickness_);
    oaa::SessionConfig config = builder.build();

    // Create session
    session_ = new oaa::AASession(transport_, config, this);

    // Tell video handler how many configs we advertised (1 resolution × 2 codecs)
    videoHandler_.setNumVideoConfigs(2);

    // Register all known channel handlers.
    //
    // Registered: Control(0), Input(1), Sensor(2), Video(3), MediaAudio(4),
    //   SpeechAudio(5), SystemAudio(6), AVInput(7), Bluetooth(8),
    //   Navigation(9), MediaStatus(10), PhoneStatus(11), WiFi(14)
    //
    // Intentionally unregistered (no handler yet):
    //   Channel 12, 13 — purpose unknown; any messages on these channels
    //   will be logged by AASession with [AA:unhandled] prefix + hex payload
    //   for future protocol discovery.
    session_->registerChannel(oaa::ChannelId::Video, &videoHandler_);
    session_->registerChannel(oaa::ChannelId::MediaAudio, &mediaAudioHandler_);
    session_->registerChannel(oaa::ChannelId::SpeechAudio, &speechAudioHandler_);
    session_->registerChannel(oaa::ChannelId::SystemAudio, &systemAudioHandler_);
    session_->registerChannel(oaa::ChannelId::Input, &inputHandler_);
    session_->registerChannel(oaa::ChannelId::Sensor, &sensorHandler_);
    session_->registerChannel(oaa::ChannelId::Bluetooth, &btHandler_);
    session_->registerChannel(oaa::ChannelId::WiFi, wifiHandler_.get());
    session_->registerChannel(oaa::ChannelId::AVInput, &avInputHandler_);
    session_->registerChannel(oaa::ChannelId::Navigation, &navHandler_);
    session_->registerChannel(oaa::ChannelId::MediaStatus, &mediaStatusHandler_);
    session_->registerChannel(oaa::ChannelId::PhoneStatus, &phoneStatusHandler_);

    // Connect session signals
    connect(session_, &oaa::AASession::stateChanged,
            this, &AndroidAutoOrchestrator::onSessionStateChanged);
    connect(session_, &oaa::AASession::disconnected,
            this, &AndroidAutoOrchestrator::onSessionDisconnected);
    startProtocolCapture();

    // Disconnect handler signals from previous session (prevents duplicates on reconnect)
    videoHandler_.disconnect(&videoDecoder_);
    videoHandler_.disconnect(this);
    mediaAudioHandler_.disconnect(this);
    speechAudioHandler_.disconnect(this);
    systemAudioHandler_.disconnect(this);

    // Wire video frames to decoder
    qRegisterMetaType<std::shared_ptr<const QByteArray>>();
    connect(&videoHandler_, &oaa::hu::VideoChannelHandler::videoFrameData,
            &videoDecoder_, &VideoDecoder::decodeFrame, Qt::QueuedConnection);

    // Push decoded frames to the sink as soon as they're ready (signal-driven).
    // The signal is emitted from the decode worker thread; Qt auto-queues it to
    // the main thread, so setVideoFrame() runs in the GUI thread as required.
    // This replaces the old 16ms polling timer, eliminating 0-16ms of display jitter.
    videoDecoder_.disconnect(this);  // prevent duplicate connections on reconnect
    connect(&videoDecoder_, &VideoDecoder::frameReady, this, [this]() {
        QVideoFrame frame = videoDecoder_.takeLatestFrame();
        if (frame.isValid()) {
            QVideoSink* sink = videoDecoder_.videoSink();
            if (sink)
                sink->setVideoFrame(frame);
        }
    });

    // Create PipeWire audio streams with per-stream buffer sizing from config
    if (audioService_) {
        int mediaBufMs  = yamlConfig_ ? yamlConfig_->audioBufferMs("media")  : 200;
        int speechBufMs = yamlConfig_ ? yamlConfig_->audioBufferMs("speech") : 200;
        int systemBufMs = yamlConfig_ ? yamlConfig_->audioBufferMs("system") : 200;

        mediaStream_  = audioService_->createStream("AA Media",  50, 48000, 2, "auto", mediaBufMs);
        speechStream_ = audioService_->createStream("AA Speech", 60, 48000, 1, "auto", speechBufMs);
        systemStream_ = audioService_->createStream("AA System", 40, 16000, 1, "auto", systemBufMs);

        // Assign EQ engines to stream handles
        if (eqService_) {
            if (mediaStream_)
                mediaStream_->eqEngine = eqService_->engineForStream(oap::StreamId::Media);
            if (speechStream_)
                speechStream_->eqEngine = eqService_->engineForStream(oap::StreamId::Navigation);
            if (systemStream_)
                systemStream_->eqEngine = eqService_->engineForStream(oap::StreamId::Phone);
        }

        if (mediaStream_) {
            connect(&mediaAudioHandler_, &oaa::hu::AudioChannelHandler::audioDataReceived,
                    this, [this](const QByteArray& data, uint64_t) {
                        audioService_->writeAudio(mediaStream_,
                            reinterpret_cast<const uint8_t*>(data.constData()), data.size());
                    }, Qt::QueuedConnection);
        }
        if (speechStream_) {
            connect(&speechAudioHandler_, &oaa::hu::AudioChannelHandler::audioDataReceived,
                    this, [this](const QByteArray& data, uint64_t) {
                        audioService_->writeAudio(speechStream_,
                            reinterpret_cast<const uint8_t*>(data.constData()), data.size());
                    }, Qt::QueuedConnection);
        }
        if (systemStream_) {
            connect(&systemAudioHandler_, &oaa::hu::AudioChannelHandler::audioDataReceived,
                    this, [this](const QByteArray& data, uint64_t) {
                        audioService_->writeAudio(systemStream_,
                            reinterpret_cast<const uint8_t*>(data.constData()), data.size());
                    }, Qt::QueuedConnection);
        }
    }

    // Bridge AA audio focus requests to PipeWire stream ducking
    if (audioService_) {
        connect(session_, &oaa::AASession::audioFocusChanged,
                this, [this](int focusType) {
            // AudioFocusType enum: GAIN=1, GAIN_TRANSIENT=2, GAIN_NAVI=3, RELEASE=4
            switch (focusType) {
            case 1: // GAIN — media playback (exclusive)
                if (mediaStream_)
                    audioService_->requestAudioFocus(mediaStream_, oap::AudioFocusType::Gain);
                break;
            case 2: // GAIN_TRANSIENT — voice/speech (pause others)
                if (speechStream_)
                    audioService_->requestAudioFocus(speechStream_, oap::AudioFocusType::GainTransient);
                break;
            case 3: // GAIN_NAVI — navigation prompt (duck others)
                if (speechStream_)
                    audioService_->requestAudioFocus(speechStream_, oap::AudioFocusType::GainTransientMayDuck);
                break;
            case 4: // RELEASE — give up focus
                if (mediaStream_)  audioService_->releaseAudioFocus(mediaStream_);
                if (speechStream_) audioService_->releaseAudioFocus(speechStream_);
                break;
            }
        }, Qt::QueuedConnection);
    }

    // AA wire theming disabled — companion app is sole theme source (Phase 03.2)
    // connect(&videoHandler_, &oaa::hu::VideoChannelHandler::uiConfigTokensReceived,
    //         this, [this](const QMap<QString, uint32_t>& dayTokens,
    //                      const QMap<QString, uint32_t>& nightTokens) {
    //     if (themeService_) {
    //         themeService_->applyAATokens(dayTokens, nightTokens);
    //     }
    // });

    // Wire video focus changes
    connect(&videoHandler_, &oaa::hu::VideoChannelHandler::videoFocusChanged,
            this, [this](int focusMode, bool) {
                // Focus mode 1 = PROJECTED, 2 = NATIVE, 3 = NATIVE_TRANSIENT, 4 = PROJECTED_NO_INPUT_FOCUS
                if (focusMode == 2 && state_ == Connected) {
                    qCInfo(lcAA) << "Video focus lost — exit to car";
                    setState(Backgrounded, "Android Auto running in background");
                } else if (focusMode == 1 && state_ == Backgrounded) {
                    qCInfo(lcAA) << "Video focus gained — returning to projection";
                    setState(Connected, "Android Auto active");
                }
            });

    // Publish AA events to plugin event bus
    if (eventBus_) {
        // Navigation events
        connect(&navHandler_, &oaa::hu::NavigationChannelHandler::navigationStateChanged,
                this, [this](bool active) {
            eventBus_->publish("aa.nav.state", QVariantMap{{"active", active}});
        });
        connect(&navHandler_, &oaa::hu::NavigationChannelHandler::navigationStepChanged,
                this, [this](const QString& instruction, const QString& destination, int maneuverType) {
            eventBus_->publish("aa.nav.step", QVariantMap{
                {"instruction", instruction},
                {"destination", destination},
                {"maneuverType", maneuverType}
            });
        });
        connect(&navHandler_, &oaa::hu::NavigationChannelHandler::navigationDistanceChanged,
                this, [this](const QString& distance, int unit) {
            eventBus_->publish("aa.nav.distance", QVariantMap{
                {"distance", distance},
                {"unit", unit}
            });
        });

        // Navigation turn event (debug logging only -- future UI consumer)
        connect(&navHandler_, &oaa::hu::NavigationChannelHandler::navigationTurnEvent,
                this, [](const QString& roadName, int maneuverType, int turnDirection,
                         const QByteArray& turnIcon, int distanceMeters, int distanceUnit) {
            qCInfo(lcAA) << "[Nav] turn:" << roadName
                          << "maneuver:" << maneuverType << "dir:" << turnDirection
                          << "dist:" << distanceMeters << "unit:" << distanceUnit
                          << "icon:" << turnIcon.size() << "bytes";
        });

        // Navigation notification (debug logging only)
        connect(&navHandler_, &oaa::hu::NavigationChannelHandler::navigationNotificationReceived,
                this, [](int stepCount, int laneCount,
                         const QString& destination, const QString& /*eta*/) {
            qCInfo(lcAA) << "[Nav] notification:" << stepCount << "steps,"
                          << laneCount << "lanes, dest:" << destination;
        });

        // Phone status events
        connect(&phoneStatusHandler_, &oaa::hu::PhoneStatusChannelHandler::callStateChanged,
                this, [this](int callState, const QString& number,
                              const QString& displayName, const QByteArray& /*contactPhoto*/) {
            eventBus_->publish("aa.phone.call", QVariantMap{
                {"callState", callState},
                {"number", number},
                {"displayName", displayName}
            });
        });
        connect(&phoneStatusHandler_, &oaa::hu::PhoneStatusChannelHandler::callsIdle,
                this, [this]() {
            eventBus_->publish("aa.phone.idle");
        });


        // Voice session command logging
        connect(session_->controlChannel(), &oaa::ControlChannel::voiceSessionSent,
                this, [](int sessionType) {
            qCDebug(lcAA) << "[Control] sent voice session request:" << sessionType;
        });

        // Media status events
        connect(&mediaStatusHandler_, &oaa::hu::MediaStatusChannelHandler::playbackStateChanged,
                this, [this](int state, const QString& appName) {
            eventBus_->publish("aa.media.state", QVariantMap{
                {"state", state},
                {"appName", appName}
            });
        });
        connect(&mediaStatusHandler_, &oaa::hu::MediaStatusChannelHandler::metadataChanged,
                this, [this](const QString& title, const QString& artist, const QString& album,
                              const QByteArray& /*albumArt*/) {
            eventBus_->publish("aa.media.metadata", QVariantMap{
                {"title", title},
                {"artist", artist},
                {"album", album}
            });
        });

        // Bluetooth auth events (debug logging only — no crypto, existing BT pairing unaffected)
        connect(&btHandler_, &oaa::hu::BluetoothChannelHandler::authDataReceived,
                this, [](const QByteArray& payload) {
            qCDebug(lcAA) << "[Bluetooth] auth data received, len:" << payload.size();
        });
        connect(&btHandler_, &oaa::hu::BluetoothChannelHandler::authResultReceived,
                this, [](const QByteArray& payload) {
            qCDebug(lcAA) << "[Bluetooth] auth result received, len:" << payload.size();
        });

        // Input haptic feedback (debug logging only — Pi has no haptic motor)
        connect(&inputHandler_, &oaa::hu::InputChannelHandler::hapticFeedbackRequested,
                this, [](int feedbackType) {
            qCDebug(lcAA) << "[Input] haptic feedback requested, type:" << feedbackType;
        });
    }

    // Wire phone battery level from ControlChannel unknownMessage (msg 0x0017)
    connect(session_->controlChannel(), &oaa::ControlChannel::unknownMessage,
            this, [this](uint16_t msgId, const QByteArray& payload) {
        if (msgId == 0x0017) {  // BatteryStatusNotification
            oaa::proto::messages::BatteryStatusNotification batt;
            if (batt.ParseFromArray(payload.constData(), payload.size())) {
                int level = batt.has_battery_level() ? static_cast<int>(batt.battery_level()) : -1;
                if (phoneBatteryLevel_ != level) {
                    phoneBatteryLevel_ = level;
                    emit phoneBatteryChanged();
                }
            }
        }
    });

    // Wire phone signal strength from PhoneStatusChannelHandler
    connect(&phoneStatusHandler_, &oaa::hu::PhoneStatusChannelHandler::signalStrengthChanged,
            this, [this](int strength) {
        if (phoneSignalStrength_ != strength) {
            phoneSignalStrength_ = strength;
            emit phoneSignalChanged();
        }
    });

    // Create and wire night mode provider
    nightProvider_.reset();
    if (yamlConfig_) {
        QString nightSource = yamlConfig_->nightModeSource();
        if (nightSource == "gpio") {
            nightProvider_ = std::make_unique<GpioNightMode>(
                yamlConfig_->nightModeGpioPin(),
                yamlConfig_->nightModeGpioActiveHigh());
        } else {
            nightProvider_ = std::make_unique<TimedNightMode>(
                yamlConfig_->nightModeDayStart(),
                yamlConfig_->nightModeNightStart());
        }

        // Connect night mode to sensor handler
        connect(nightProvider_.get(), &NightModeProvider::nightModeChanged,
                &sensorHandler_, &oaa::hu::SensorChannelHandler::pushNightMode);

        nightProvider_->start();
        qCInfo(lcAA) << "Night mode provider started (source=" << nightSource << ")";
    }

    // Start protocol handshake
    session_->start();
}

void AndroidAutoOrchestrator::onSessionStateChanged(oaa::SessionState state)
{
    switch (state) {
    case oaa::SessionState::Active:
        qCInfo(lcAA) << "Android Auto connected!";
        setState(Connected, "Android Auto active");
        startConnectionWatchdog();
        break;
    case oaa::SessionState::Connecting:
    case oaa::SessionState::VersionExchange:
    case oaa::SessionState::TLSHandshake:
        qCDebug(lcAA) << "Session state: negotiating protocol";
        setState(Connecting, "Negotiating protocol...");
        break;
    case oaa::SessionState::ServiceDiscovery:
        qCDebug(lcAA) << "Session state: service discovery";
        setState(Connecting, "Service discovery...");
        break;
    case oaa::SessionState::ShuttingDown:
        qCInfo(lcAA) << "Session shutting down";
        break;
    case oaa::SessionState::Disconnected:
        onSessionDisconnected(oaa::DisconnectReason::Normal);
        break;
    default:
        break;
    }
}

void AndroidAutoOrchestrator::onSessionDisconnected(oaa::DisconnectReason reason)
{
    qCInfo(lcAA) << "Disconnected, reason:" << static_cast<int>(reason);
    stopConnectionWatchdog();

    if (nightProvider_) {
        nightProvider_->stop();
        nightProvider_.reset();
    }

    teardownSession();

    uint16_t port = config_ ? config_->tcpPort() : 5288;
    setState(WaitingForDevice,
             QString("Waiting for wireless connection on port %1...").arg(port));

#ifdef HAS_BLUETOOTH
    if (pendingReconnect_ && btDiscovery_) {
        pendingReconnect_ = false;
        // Give the phone ~500ms to process the disconnect before re-sending
        // the WifiStartRequest. Firing immediately causes the phone to ignore it.
        QTimer::singleShot(500, this, [this]() {
            if (btDiscovery_)
                btDiscovery_->retrigger();
        });
    }
#endif
    pendingReconnect_ = false;
}

void AndroidAutoOrchestrator::startProtocolCapture()
{
    if (!session_ || !session_->messenger() || !yamlConfig_) {
        stopProtocolCapture();
        return;
    }

    const QVariant enabledVar = yamlConfig_->valueByPath("connection.protocol_capture.enabled");
    const bool enabled = enabledVar.isValid() ? enabledVar.toBool() : false;
    if (!enabled) {
        stopProtocolCapture();
        return;
    }

    QString path = yamlConfig_->valueByPath("connection.protocol_capture.path").toString().trimmed();
    if (path.isEmpty()) {
        path = "/tmp/oaa-protocol-capture.jsonl";
    }

    const QFileInfo captureInfo(path);
    const QString captureDir = captureInfo.absolutePath();
    if (!captureDir.isEmpty()) {
        QDir().mkpath(captureDir);
    }

    QString format = yamlConfig_->valueByPath("connection.protocol_capture.format")
                         .toString().trimmed().toLower();
    if (format.isEmpty()) {
        format = "jsonl";
    }

    const QVariant includeMediaVar = yamlConfig_->valueByPath("connection.protocol_capture.include_media");
    const bool includeMedia = includeMediaVar.isValid() ? includeMediaVar.toBool() : false;

    if (!protocolLogger_) {
        protocolLogger_ = std::make_unique<oaa::ProtocolLogger>();
    }

    protocolLogger_->detach();
    protocolLogger_->close();
    protocolLogger_->setFormat(format == "jsonl"
        ? oaa::ProtocolLogger::OutputFormat::Jsonl
        : oaa::ProtocolLogger::OutputFormat::Tsv);
    protocolLogger_->setIncludeMedia(includeMedia);
    protocolLogger_->open(path.toStdString());
    if (!protocolLogger_->isOpen()) {
        qCWarning(lcAA) << "Protocol capture enabled but failed to open:" << path;
        return;
    }

    protocolLogger_->attach(session_->messenger());
    qCInfo(lcAA) << "Protocol capture active:"
            << "path=" << path
            << "format=" << format
            << "include_media=" << includeMedia;
}

void AndroidAutoOrchestrator::stopProtocolCapture()
{
    if (!protocolLogger_) {
        return;
    }

    protocolLogger_->detach();
    protocolLogger_->close();
}

void AndroidAutoOrchestrator::teardownSession()
{
    stopProtocolCapture();

    // Reset phone status properties (no stale data after disconnect)
    if (phoneBatteryLevel_ != -1) {
        phoneBatteryLevel_ = -1;
        emit phoneBatteryChanged();
    }
    if (phoneSignalStrength_ != -1) {
        phoneSignalStrength_ = -1;
        emit phoneSignalChanged();
    }

    // Disconnect frame-ready signal to stop pushing frames during teardown
    videoDecoder_.disconnect(this);

    if (session_) {
        // Disconnect all signals from session_ to us BEFORE scheduling deletion.
        // This prevents onSessionDisconnected from being called a second time if
        // the 'disconnected' signal is also queued.
        session_->disconnect(this);
        videoHandler_.disconnect(this);
        mediaAudioHandler_.disconnect(this);
        speechAudioHandler_.disconnect(this);
        systemAudioHandler_.disconnect(this);
        videoHandler_.disconnect(&videoDecoder_);
        phoneStatusHandler_.disconnect(this);

        // deleteLater() instead of delete: teardownSession() is often called from
        // within onSessionStateChanged(), which is a direct-connection slot of
        // session_->stateChanged. Deleting the sender synchronously while inside
        // its signal dispatch causes UAF when Qt's dispatch machinery resumes.
        // deleteLater() defers the delete to the next event loop iteration.
        session_->deleteLater();
        session_ = nullptr;
    }
    if (transport_) {
        transport_->deleteLater();
        transport_ = nullptr;
    }
    activeSocket_ = nullptr;  // owned by transport

    // Destroy audio streams
    if (audioService_) {
        if (mediaStream_) { audioService_->destroyStream(mediaStream_); mediaStream_ = nullptr; }
        if (speechStream_) { audioService_->destroyStream(speechStream_); speechStream_ = nullptr; }
        if (systemStream_) { audioService_->destroyStream(systemStream_); systemStream_ = nullptr; }
    }
}

void AndroidAutoOrchestrator::requestVideoFocus()
{
    if (state_ == Backgrounded) {
        qCInfo(lcAA) << "Requesting video focus (returning from background)";
        videoHandler_.requestVideoFocus(true);
        setState(Connected, "Android Auto active");
    }
}

void AndroidAutoOrchestrator::requestExitToCar()
{
    if (state_ == Connected) {
        qCInfo(lcAA) << "Requesting exit to car";
        videoHandler_.requestVideoFocus(false);
        setState(Backgrounded, "Exited to car");
    }
}

void AndroidAutoOrchestrator::sendButtonPress(int keycode)
{
    qCDebug(lcAA) << "[Input] sendButtonPress keycode:" << keycode;
    auto ts = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    inputHandler_.sendButtonEvent(static_cast<uint32_t>(keycode), true, ts);
    inputHandler_.sendButtonEvent(static_cast<uint32_t>(keycode), false, ts + 50000);
}

void AndroidAutoOrchestrator::setState(ConnectionState state, const QString& message)
{
    if (state_ != state) {
        state_ = state;
        emit connectionStateChanged();
    }
    if (statusMessage_ != message) {
        statusMessage_ = message;
        emit statusMessageChanged();
    }
}

void AndroidAutoOrchestrator::startConnectionWatchdog()
{
    // Unconditionally disconnect to prevent lambda accumulation across reconnects
    disconnect(&watchdogTimer_, nullptr, this, nullptr);
    watchdogTimer_.stop();

    connect(&watchdogTimer_, &QTimer::timeout, this, [this]() {
        if ((state_ != Connected && state_ != Backgrounded) || !activeSocket_) {
            stopConnectionWatchdog();
            return;
        }

        auto fd = activeSocket_->socketDescriptor();
        if (fd == -1) {
            qCWarning(lcAA) << "Watchdog: socket descriptor invalid";
            teardownSession();
            uint16_t port = config_ ? config_->tcpPort() : 5288;
            setState(WaitingForDevice,
                     QString("Waiting for wireless connection on port %1...").arg(port));
            return;
        }

        struct tcp_info info{};
        socklen_t len = sizeof(info);
        if (::getsockopt(fd, IPPROTO_TCP, TCP_INFO, &info, &len) < 0) {
            qCWarning(lcAA) << "Watchdog: getsockopt failed, forcing disconnect";
            onSessionDisconnected(oaa::DisconnectReason::TransportError);
            return;
        }

        // TCP_ESTABLISHED = 1
        if (info.tcpi_state != 1) {
            qCDebug(lcAA) << "Watchdog: TCP state="
                     << (int)info.tcpi_state << "(not ESTABLISHED), forcing disconnect";
            onSessionDisconnected(oaa::DisconnectReason::TransportError);
            return;
        }

        // Check retransmit backoff — exponential backoff level stays high when
        // the peer is unreachable
        bool dead = false;
        if (info.tcpi_backoff >= 3) {
            qCDebug(lcAA) << "Watchdog: backoff=" << (int)info.tcpi_backoff
                     << ", peer unreachable";
            dead = true;
        } else if (info.tcpi_retransmits > 4) {
            qCDebug(lcAA) << "Watchdog:" << (int)info.tcpi_retransmits
                     << "retransmits";
            dead = true;
        }

        if (dead) {
            activeSocket_->abort();
            onSessionDisconnected(oaa::DisconnectReason::TransportError);
        }
    });

    watchdogTimer_.start(2000);
    qCDebug(lcAA) << "Connection watchdog started";
}

void AndroidAutoOrchestrator::stopConnectionWatchdog()
{
    if (watchdogTimer_.isActive()) {
        watchdogTimer_.stop();
        disconnect(&watchdogTimer_, nullptr, this, nullptr);
        qCDebug(lcAA) << "Connection watchdog stopped";
    }
}

} // namespace aa
} // namespace oap
