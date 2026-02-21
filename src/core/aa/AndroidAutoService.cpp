#include "AndroidAutoService.hpp"
#include "ServiceFactory.hpp"
#include "VideoService.hpp"
#include "TimedNightMode.hpp"
#include "GpioNightMode.hpp"

#include "../../core/YamlConfig.hpp"

#include <QMetaObject>
#include <boost/log/trivial.hpp>

#include <aasdk/TCP/TCPWrapper.hpp>
#include <aasdk/TCP/TCPEndpoint.hpp>
#include <aasdk/Transport/SSLWrapper.hpp>
#include <aasdk/Transport/TCPTransport.hpp>
#include <aasdk/Messenger/Cryptor.hpp>
#include <aasdk/Messenger/MessageInStream.hpp>
#include <aasdk/Messenger/MessageOutStream.hpp>
#include <aasdk/Messenger/Messenger.hpp>

#include <netinet/tcp.h>
#include <sys/socket.h>

#ifdef HAS_BLUETOOTH
#include "BluetoothDiscoveryService.hpp"
#endif

namespace oap {
namespace aa {

static constexpr int kAsioThreadCount = 4;

AndroidAutoService::AndroidAutoService(
    std::shared_ptr<oap::Configuration> config,
    oap::IAudioService* audioService,
    oap::YamlConfig* yamlConfig,
    QObject* parent)
    : QObject(parent)
    , config_(std::move(config))
    , audioService_(audioService)
    , yamlConfig_(yamlConfig)
{
    videoDecoder_ = new VideoDecoder(this);
    touchHandler_ = new TouchHandler(this);
}

AndroidAutoService::~AndroidAutoService()
{
    stop();
}

void AndroidAutoService::start()
{
    BOOST_LOG_TRIVIAL(info) << "[AAService] Starting Android Auto service (wireless mode)";

    setState(WaitingForDevice, "Initializing...");

    // Create ASIO io_service
    ioService_ = std::make_unique<boost::asio::io_service>();
    ioWork_ = std::make_unique<boost::asio::io_service::work>(*ioService_);

    // Start ASIO worker threads
    for (int i = 0; i < kAsioThreadCount; ++i) {
        asioThreads_.emplace_back([this, i]() {
            BOOST_LOG_TRIVIAL(info) << "[AAService] ASIO thread " << i << " started";
            ioService_->run();
            BOOST_LOG_TRIVIAL(info) << "[AAService] ASIO thread " << i << " stopped";
        });
    }

    // SSL wrapper (shared across connections)
    sslWrapper_ = std::make_shared<aasdk::transport::SSLWrapper>();

    // TCP wrapper
    tcpWrapper_ = std::make_shared<aasdk::tcp::TCPWrapper>();

    // Start TCP listener for wireless AA
    startTCPListener();

#ifdef HAS_BLUETOOTH
    // Start Bluetooth discovery service
    if (config_->wirelessEnabled()) {
        QString wifiIface = QStringLiteral("wlan0");
        if (yamlConfig_) {
            wifiIface = yamlConfig_->wifiInterface();
        }
        btService_ = new BluetoothDiscoveryService(config_, wifiIface, this);
        connect(btService_, &BluetoothDiscoveryService::phoneWillConnect,
                this, [this]() {
                    setState(Connecting, "Phone connecting via WiFi...");
                });
        connect(btService_, &BluetoothDiscoveryService::error,
                this, [this](const QString& msg) {
                    BOOST_LOG_TRIVIAL(error) << "[AAService] BT error: " << msg.toStdString();
                });
        btService_->start();
        BOOST_LOG_TRIVIAL(info) << "[AAService] Bluetooth discovery started";
    }
#endif

    uint16_t port = config_->tcpPort();
    setState(WaitingForDevice,
             QString("Waiting for wireless connection on port %1...").arg(port));
}

void AndroidAutoService::stop()
{
    BOOST_LOG_TRIVIAL(info) << "[AAService] Stopping Android Auto service";

    // Stop night mode provider
    if (nightProvider_) {
        nightProvider_->stop();
        nightProvider_.reset();
    }

    // Stop entity
    if (entity_) {
        entity_->stop();
        entity_.reset();
    }

#ifdef HAS_BLUETOOTH
    if (btService_) {
        btService_->stop();
        delete btService_;
        btService_ = nullptr;
    }
#endif

    // Cancel TCP acceptor
    if (tcpAcceptor_) {
        boost::system::error_code ec;
        tcpAcceptor_->close(ec);
        tcpAcceptor_.reset();
    }

    // Stop ASIO
    if (ioWork_) {
        ioWork_.reset();
    }
    if (ioService_) {
        ioService_->stop();
    }

    for (auto& t : asioThreads_) {
        if (t.joinable()) t.join();
    }
    asioThreads_.clear();

    tcpWrapper_.reset();
    sslWrapper_.reset();
    ioService_.reset();

    setState(Disconnected, "Stopped");
}

void AndroidAutoService::setState(ConnectionState state, const QString& message)
{
    // Thread-safe: may be called from ASIO threads
    QMetaObject::invokeMethod(this, [this, state, message]() {
        if (state_ != state) {
            state_ = state;
            emit connectionStateChanged();
        }
        if (statusMessage_ != message) {
            statusMessage_ = message;
            emit statusMessageChanged();
        }
    }, Qt::QueuedConnection);
}

void AndroidAutoService::startTCPListener()
{
    uint16_t port = config_->tcpPort();
    try {
        if (!tcpAcceptor_) {
            auto endpoint = boost::asio::ip::tcp::endpoint(
                boost::asio::ip::tcp::v4(), port);

            tcpAcceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(*ioService_, endpoint);
            tcpAcceptor_->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

            BOOST_LOG_TRIVIAL(info) << "[AAService] TCP listener started on port " << port;
        }

        auto socket = std::make_shared<boost::asio::ip::tcp::socket>(*ioService_);
        tcpAcceptor_->async_accept(*socket,
            [this, socket](const boost::system::error_code& ec) {
                if (!ec) {
                    onTCPConnection(socket);
                } else if (ec != boost::asio::error::operation_aborted) {
                    BOOST_LOG_TRIVIAL(error) << "[AAService] TCP accept error: " << ec.message();
                }
                // Accept next connection
                if (tcpAcceptor_ && tcpAcceptor_->is_open()) {
                    startTCPListener();
                }
            });
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "[AAService] Failed to start TCP listener: " << e.what();
    }
}

void AndroidAutoService::onTCPConnection(std::shared_ptr<boost::asio::ip::tcp::socket> socket)
{
    auto remoteEndpoint = socket->remote_endpoint();
    BOOST_LOG_TRIVIAL(info) << "[AAService] Wireless AA connection from "
                            << remoteEndpoint.address().to_string()
                            << ":" << remoteEndpoint.port();

    if (entity_) {
        BOOST_LOG_TRIVIAL(warning) << "[AAService] Already have active connection — tearing down old session for reconnect";
        if (nightProvider_) {
            nightProvider_->stop();
            nightProvider_.reset();
        }
        entity_->stop();
        entity_.reset();
    }

    setState(Connecting, QString("Wireless connection from %1...")
             .arg(QString::fromStdString(remoteEndpoint.address().to_string())));

    // Enable TCP keepalive so we detect dead connections (e.g. phone WiFi killed)
    try {
        auto fd = socket->native_handle();
        int yes = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
        int idle = 5;     // seconds before first keepalive probe
        int interval = 3; // seconds between probes
        int count = 3;    // failed probes before connection is dead
        ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
        ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
        ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
        BOOST_LOG_TRIVIAL(debug) << "[AAService] TCP keepalive enabled (idle=5s, interval=3s, count=3)";
    } catch (...) {
        BOOST_LOG_TRIVIAL(warning) << "[AAService] Failed to set TCP keepalive";
    }

    // Keep a reference so the watchdog can check socket health
    activeSocket_ = socket;

    try {
        auto tcpEndpoint = std::make_shared<aasdk::tcp::TCPEndpoint>(*tcpWrapper_, std::move(socket));
        auto transport = std::make_shared<aasdk::transport::TCPTransport>(*ioService_, std::move(tcpEndpoint));
        startEntity(std::move(transport));
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "[AAService] Failed to create TCP transport: " << e.what();
        setState(WaitingForDevice, QString("TCP error: %1").arg(e.what()));
    }
}

void AndroidAutoService::startEntity(aasdk::transport::ITransport::Pointer transport)
{
    BOOST_LOG_TRIVIAL(info) << "[AAService] Creating Android Auto entity";

    // Create cryptor
    auto cryptor = std::make_shared<aasdk::messenger::Cryptor>(sslWrapper_);
    cryptor->init();

    // Create messenger
    auto messageInStream = std::make_shared<aasdk::messenger::MessageInStream>(
        *ioService_, transport, cryptor);
    auto messageOutStream = std::make_shared<aasdk::messenger::MessageOutStream>(
        *ioService_, transport, cryptor);
    auto messenger = std::make_shared<aasdk::messenger::Messenger>(
        *ioService_, std::move(messageInStream), std::move(messageOutStream));

    // Create night mode provider based on config
    nightProvider_.reset();
    if (yamlConfig_) {
        QString nightSource = yamlConfig_->nightModeSource();
        if (nightSource == "theme") {
            // ThemeNightMode requires ThemeService* which isn't accessible here.
            // Fall back to time-based and log a warning.
            BOOST_LOG_TRIVIAL(warning) << "[AAService] Night mode source 'theme' not yet wired — falling back to 'time'";
            nightProvider_ = std::make_unique<aa::TimedNightMode>(
                yamlConfig_->nightModeDayStart(),
                yamlConfig_->nightModeNightStart());
        } else if (nightSource == "gpio") {
            nightProvider_ = std::make_unique<aa::GpioNightMode>(
                yamlConfig_->nightModeGpioPin(),
                yamlConfig_->nightModeGpioActiveHigh());
        } else {
            // Default: time-based (covers "time" and any unknown value)
            nightProvider_ = std::make_unique<aa::TimedNightMode>(
                yamlConfig_->nightModeDayStart(),
                yamlConfig_->nightModeNightStart());
        }
        // nightProvider_ is a QObject with a QTimer — must start on the Qt
        // main thread or QTimer::start() silently does nothing.
        QMetaObject::invokeMethod(nightProvider_.get(), [this]() {
            nightProvider_->start();
        }, Qt::QueuedConnection);
        BOOST_LOG_TRIVIAL(info) << "[AAService] Night mode provider created (source="
                                << nightSource.toStdString() << ")";
    }

    // Create services via factory
    auto result = ServiceFactory::create(*ioService_, messenger, config_, videoDecoder_, touchHandler_, audioService_, yamlConfig_, nightProvider_.get());
    videoService_ = result.videoService;

    // Connect video focus changes (ASIO thread → Qt main thread)
    if (videoService_) {
        connect(videoService_.get(), &VideoService::videoFocusChanged,
                this, [this](bool focused) {
            if (!focused) {
                BOOST_LOG_TRIVIAL(info) << "[AAService] Video focus lost — exit to car";
                setState(Backgrounded, "Android Auto running in background");
            } else if (state_ == Backgrounded) {
                BOOST_LOG_TRIVIAL(info) << "[AAService] Video focus gained — returning to projection";
                setState(Connected, "Android Auto active");
            }
        }, Qt::QueuedConnection);
    }

    // Create entity (it creates its own control channel from its strand)
    entity_ = std::make_shared<AndroidAutoEntity>(
        *ioService_, std::move(cryptor), messenger, std::move(result.services), yamlConfig_);

    entity_->start(*this);
}

// IAndroidAutoEntityEventHandler callbacks (called from ASIO thread)

void AndroidAutoService::onConnected()
{
    BOOST_LOG_TRIVIAL(info) << "[AAService] Android Auto connected!";
    setState(Connected, "Android Auto active");
    // Must start watchdog on the main thread — QTimer requires a Qt event loop
    QMetaObject::invokeMethod(this, [this]() { startConnectionWatchdog(); },
                              Qt::QueuedConnection);
}

void AndroidAutoService::requestVideoFocus()
{
    if (videoService_ && state_ == Backgrounded) {
        BOOST_LOG_TRIVIAL(info) << "[AAService] Requesting video focus (returning from background)";
        videoService_->setVideoFocus(VideoFocusMode::Projection);
        setState(Connected, "Android Auto active");
    }
}

void AndroidAutoService::onProjectionFocusLost()
{
    BOOST_LOG_TRIVIAL(info) << "[AAService] Projection focus lost — exit to car (session stays alive)";
    setState(Backgrounded, "Android Auto running in background");
}

void AndroidAutoService::onDisconnected()
{
    BOOST_LOG_TRIVIAL(info) << "[AAService] Android Auto disconnected";
    QMetaObject::invokeMethod(this, [this]() { stopConnectionWatchdog(); },
                              Qt::QueuedConnection);

    if (nightProvider_) {
        nightProvider_->stop();
        nightProvider_.reset();
    }

    if (entity_) {
        entity_->stop();
        entity_.reset();
    }
    videoService_.reset();
    activeSocket_.reset();

    uint16_t port = config_->tcpPort();
    setState(WaitingForDevice,
             QString("Waiting for wireless connection on port %1...").arg(port));

    // Restart TCP listener so we can accept a new connection
    startTCPListener();
}

void AndroidAutoService::onError(const std::string& message)
{
    BOOST_LOG_TRIVIAL(error) << "[AAService] Error: " << message;
    QMetaObject::invokeMethod(this, [this]() { stopConnectionWatchdog(); },
                              Qt::QueuedConnection);

    if (nightProvider_) {
        nightProvider_->stop();
        nightProvider_.reset();
    }

    if (entity_) {
        entity_->stop();
        entity_.reset();
    }
    videoService_.reset();
    activeSocket_.reset();

    setState(WaitingForDevice, QString("Error: %1").arg(QString::fromStdString(message)));

    // Restart TCP listener so we can accept a reconnection
    startTCPListener();
}

void AndroidAutoService::startConnectionWatchdog()
{
    stopConnectionWatchdog();

    connect(&watchdogTimer_, &QTimer::timeout, this, [this]() {
        if ((state_ != Connected && state_ != Backgrounded) || !activeSocket_) {
            stopConnectionWatchdog();
            return;
        }

        // Check if socket is still alive using TCP_INFO
        struct tcp_info info{};
        socklen_t len = sizeof(info);
        int fd = activeSocket_->native_handle();

        if (::getsockopt(fd, IPPROTO_TCP, TCP_INFO, &info, &len) < 0) {
            BOOST_LOG_TRIVIAL(warning) << "[AAService] Watchdog: getsockopt failed, forcing disconnect";
            onDisconnected();
            return;
        }

        // TCP_ESTABLISHED = 1, anything else means it's dying or dead
        if (info.tcpi_state != 1) {
            BOOST_LOG_TRIVIAL(info) << "[AAService] Watchdog: TCP state=" << (int)info.tcpi_state
                                    << " (not ESTABLISHED), forcing disconnect";
            onDisconnected();
            return;
        }

        // Check retransmit backoff — exponential backoff level stays high when
        // the peer is unreachable. backoff >= 3 means multiple retransmit rounds
        // have failed (typically 15+ seconds of no response).
        bool dead = false;
        if (info.tcpi_backoff >= 3) {
            BOOST_LOG_TRIVIAL(info) << "[AAService] Watchdog: retransmit backoff="
                                    << (int)info.tcpi_backoff
                                    << ", peer unreachable — closing socket";
            dead = true;
        } else if (info.tcpi_retransmits > 4) {
            BOOST_LOG_TRIVIAL(info) << "[AAService] Watchdog: " << (int)info.tcpi_retransmits
                                    << " retransmits — closing socket";
            dead = true;
        }

        if (dead) {
            // Close the socket — this causes ASIO read errors which trigger
            // onDisconnected() through the normal aasdk error path.
            boost::system::error_code ec;
            activeSocket_->close(ec);
            stopConnectionWatchdog();
            return;
        }
    });

    watchdogTimer_.start(2000);  // check every 2 seconds
    BOOST_LOG_TRIVIAL(debug) << "[AAService] Connection watchdog started";
}

void AndroidAutoService::stopConnectionWatchdog()
{
    if (watchdogTimer_.isActive()) {
        watchdogTimer_.stop();
        disconnect(&watchdogTimer_, nullptr, this, nullptr);
        BOOST_LOG_TRIVIAL(debug) << "[AAService] Connection watchdog stopped";
    }
}

} // namespace aa
} // namespace oap
