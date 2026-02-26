#include "AndroidAutoPlugin.hpp"
#include "core/aa/AndroidAutoOrchestrator.hpp"
#include "core/aa/EvdevTouchReader.hpp"
#include "core/plugin/IHostContext.hpp"
#include "core/Configuration.hpp"
#include "core/InputDeviceScanner.hpp"
#include "core/services/IAudioService.hpp"
#include "core/services/IConfigService.hpp"
#include "core/services/ConfigService.hpp"
#include <algorithm>
#include <QDebug>
#include <QQmlContext>
#include <QFile>

namespace oap {
namespace plugins {

AndroidAutoPlugin::AndroidAutoPlugin(std::shared_ptr<oap::Configuration> config,
                                     oap::YamlConfig* yamlConfig,
                                     QObject* parent)
    : QObject(parent)
    , config_(std::move(config))
    , yamlConfig_(yamlConfig)
{
}

AndroidAutoPlugin::~AndroidAutoPlugin()
{
    shutdown();
}

bool AndroidAutoPlugin::initialize(IHostContext* context)
{
    hostContext_ = context;

    // Create AA orchestrator with audio routing through PipeWire
    auto* audioService = context ? context->audioService() : nullptr;
    auto* eventBus = context ? context->eventBus() : nullptr;
    aaService_ = new oap::aa::AndroidAutoOrchestrator(config_, audioService, yamlConfig_, eventBus, this);

    // Connect navigation: emit signals for PluginModel to handle activation/deactivation.
    QObject::connect(aaService_, &oap::aa::AndroidAutoOrchestrator::connectionStateChanged,
                     this, [this]() {
        using CS = oap::aa::AndroidAutoOrchestrator;
        auto state = static_cast<CS::ConnectionState>(aaService_->connectionState());
        if (state == CS::Connected) {
            if (touchReader_) touchReader_->grab();
            emit requestActivation();
        } else if (state == CS::Backgrounded) {
            if (touchReader_) touchReader_->ungrab();
            emit requestDeactivation();
        } else if (state == CS::Disconnected
                   || state == CS::WaitingForDevice) {
            if (touchReader_) touchReader_->ungrab();
            emit requestDeactivation();
        }
    });

    // Auto-detect or read touch device from config
    QString touchDevice;
    if (hostContext_ && hostContext_->configService()) {
        touchDevice = hostContext_->configService()->value("touch.device").toString();
    }
    if (touchDevice.isEmpty()) {
        touchDevice = oap::InputDeviceScanner::findTouchDevice();
        if (!touchDevice.isEmpty()) {
            qInfo() << "[AAPlugin] Auto-detected touch device:" << touchDevice;
        }
    }

    // Read display resolution from config (default: 1024x600)
    int displayW = 1024, displayH = 600;
    if (hostContext_ && hostContext_->configService()) {
        QVariant w = hostContext_->configService()->value("display.width");
        QVariant h = hostContext_->configService()->value("display.height");
        if (w.isValid()) displayW = w.toInt();
        if (h.isValid()) displayH = h.toInt();
    }

    // Resolve AA touch coordinate space from configured video resolution
    int aaW = 1280, aaH = 720;
    if (hostContext_ && hostContext_->configService()) {
        QString res = hostContext_->configService()->value("video.resolution").toString();
        if (res == "1080p") { aaW = 1920; aaH = 1080; }
        else if (res == "480p") { aaW = 800; aaH = 480; }
    }

    if (!touchDevice.isEmpty() && QFile::exists(touchDevice)) {
        touchReader_ = new oap::aa::EvdevTouchReader(
            aaService_->touchHandler(),
            touchDevice.toStdString(),
            4095, 4095,
            aaW, aaH,
            displayW, displayH,
            this);
        touchReader_->start();
        qInfo() << "[AAPlugin] Touch:" << touchDevice
                << "display=" << displayW << "x" << displayH;

        // Configure sidebar touch exclusion
        if (hostContext_ && hostContext_->configService()) {
            QVariant sidebarEnabledVar = hostContext_->configService()->value("video.sidebar.enabled");
            bool sidebarEnabled = (sidebarEnabledVar == true || sidebarEnabledVar.toInt() == 1
                                   || sidebarEnabledVar.toString() == "true");
            if (sidebarEnabled) {
                int sidebarW = hostContext_->configService()->value("video.sidebar.width").toInt();
                QString pos = hostContext_->configService()->value("video.sidebar.position").toString();
                if (sidebarW <= 0) sidebarW = 150;
                if (pos.isEmpty()) pos = "right";
                touchReader_->setSidebar(true, sidebarW, pos.toStdString());
                touchReader_->computeLetterbox();
                qInfo() << "[AAPlugin] Sidebar touch zones:" << pos << sidebarW << "px";

                connect(touchReader_, &oap::aa::EvdevTouchReader::sidebarVolumeSet,
                        this, [this](int level) {
                    if (hostContext_ && hostContext_->audioService()) {
                        hostContext_->audioService()->setMasterVolume(level);
                        qDebug() << "[AAPlugin] Sidebar vol:" << level;
                    }
                }, Qt::QueuedConnection);

                connect(touchReader_, &oap::aa::EvdevTouchReader::sidebarHome,
                        this, [this]() {
                    qInfo() << "[AAPlugin] Sidebar home — requesting exit to car";
                    if (aaService_)
                        aaService_->requestExitToCar();
                }, Qt::QueuedConnection);
            }
        }
    } else {
        qInfo() << "[AAPlugin] No touch device found — touch input disabled";
    }

    // Watch for video setting changes — disconnect active session so phone
    // reconnects and renegotiates with the updated config
    if (hostContext_ && hostContext_->configService()) {
        auto* cfgSvc = static_cast<oap::ConfigService*>(hostContext_->configService());
        connect(cfgSvc, &oap::ConfigService::configChanged,
                this, &AndroidAutoPlugin::onConfigChanged);
    }

    // Start AA orchestrator — it needs to listen for connections immediately
    aaService_->start();

    if (hostContext_)
        hostContext_->log(LogLevel::Info, QStringLiteral("Android Auto plugin initialized"));

    return true;
}

void AndroidAutoPlugin::onConfigChanged(const QString& path, const QVariant& value)
{
    Q_UNUSED(value)
    static const QStringList videoSettings = {
        QStringLiteral("video.resolution"),
        QStringLiteral("video.fps"),
    };
    if (!videoSettings.contains(path))
        return;

    if (!aaService_) return;

    using CS = oap::aa::AndroidAutoOrchestrator;
    auto state = static_cast<CS::ConnectionState>(aaService_->connectionState());
    if (state != CS::Connected && state != CS::Backgrounded)
        return;

    qInfo() << "[AAPlugin] Video setting changed (" << path << ") — reconnecting for renegotiation";
    // Queue the disconnect — calling it synchronously from inside configChanged
    // would spin a nested event loop mid-signal-emission and crash
    QMetaObject::invokeMethod(aaService_, [this]() {
        aaService_->disconnectSession();
    }, Qt::QueuedConnection);
}

void AndroidAutoPlugin::stopAA()
{
    if (aaService_) {
        qInfo() << "[AAPlugin] Graceful AA shutdown requested";
        aaService_->disconnectSession();
    }
}

void AndroidAutoPlugin::shutdown()
{
    if (touchReader_) {
        touchReader_->requestStop();
        touchReader_->wait();
        touchReader_ = nullptr;
    }
    if (aaService_) {
        aaService_->stop();
        aaService_ = nullptr;
    }
}

void AndroidAutoPlugin::onActivated(QQmlContext* context)
{
    if (!context || !aaService_) return;

    // Expose AA objects to the plugin's QML view
    context->setContextProperty("AndroidAutoService", aaService_);
    context->setContextProperty("VideoDecoder", aaService_->videoDecoder());
    context->setContextProperty("TouchHandler", aaService_->touchHandler());

    // Re-grab touch and request video focus if returning from backgrounded state
    using CS = oap::aa::AndroidAutoOrchestrator;
    if (static_cast<CS::ConnectionState>(aaService_->connectionState()) == CS::Backgrounded) {
        if (touchReader_) touchReader_->grab();
        aaService_->requestVideoFocus();
        qInfo() << "[AAPlugin] Re-entering AA projection from background";
    }
}

void AndroidAutoPlugin::onDeactivated()
{
    if (aaService_ && aaService_->videoDecoder())
        aaService_->videoDecoder()->setVideoSink(nullptr);
}

QUrl AndroidAutoPlugin::qmlComponent() const
{
    return QUrl(QStringLiteral("qrc:/OpenAutoProdigy/AndroidAutoMenu.qml"));
}

QUrl AndroidAutoPlugin::iconSource() const
{
    return {};
}

} // namespace plugins
} // namespace oap
