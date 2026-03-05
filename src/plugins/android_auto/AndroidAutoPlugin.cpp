#include "AndroidAutoPlugin.hpp"
#include "core/aa/AndroidAutoOrchestrator.hpp"
#include "core/aa/EvdevTouchReader.hpp"
#include "core/aa/EvdevCoordBridge.hpp"
#include "core/aa/ServiceDiscoveryBuilder.hpp"
#include "core/plugin/IHostContext.hpp"
#include "core/Configuration.hpp"
#include "core/InputDeviceScanner.hpp"
#include "core/services/IAudioService.hpp"
#include "core/services/IConfigService.hpp"
#include "core/services/ConfigService.hpp"
#include "core/services/EqualizerService.hpp"
#include "ui/DisplayInfo.hpp"
#include <algorithm>
#include "../../core/Logging.hpp"
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

void AndroidAutoPlugin::setDisplayInfo(DisplayInfo* info)
{
    displayInfo_ = info;
}


bool AndroidAutoPlugin::initialize(IHostContext* context)
{
    hostContext_ = context;

    // Create AA orchestrator with audio routing through PipeWire
    auto* audioService = context ? context->audioService() : nullptr;
    auto* eventBus = context ? context->eventBus() : nullptr;
    auto* eqService = context ? dynamic_cast<oap::EqualizerService*>(context->equalizerService()) : nullptr;
    aaService_ = new oap::aa::AndroidAutoOrchestrator(config_, audioService, yamlConfig_, eventBus, eqService, this);

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
            qCInfo(lcAA) << "Auto-detected touch device:" << touchDevice;
        }
    }

    // Read display resolution: prefer detected dimensions from DisplayInfo,
    // fall back to config values (default: 1024x600)
    int displayW = 1024, displayH = 600;
    if (displayInfo_ && displayInfo_->windowWidth() > 0 && displayInfo_->windowHeight() > 0) {
        displayW = displayInfo_->windowWidth();
        displayH = displayInfo_->windowHeight();
        qCInfo(lcAA) << "Display dimensions: detected" << displayW << "x" << displayH;
    } else {
        qCInfo(lcAA) << "Display dimensions: using defaults" << displayW << "x" << displayH;
    }

    // Pass detected dimensions to AA orchestrator for margin calculations
    if (displayInfo_ && displayInfo_->windowWidth() > 0 && displayInfo_->windowHeight() > 0) {
        aaService_->setDisplayDimensions(displayInfo_->windowWidth(), displayInfo_->windowHeight());
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
            aaW, aaH,
            displayW, displayH,
            this);
        touchReader_->start();
        qCInfo(lcAA) << "Touch:" << touchDevice
                << "display=" << displayW << "x" << displayH;

        // Create EvdevCoordBridge for external zone registration (navbar, gesture overlay)
        coordBridge_ = new oap::aa::EvdevCoordBridge(&touchReader_->router(), this);
        coordBridge_->setDisplayMapping(displayW, displayH, 4095, 4095);
        touchReader_->setCoordBridge(coordBridge_);

        // Relay 3-finger gesture to QML overlay (works regardless of grab state)
        connect(touchReader_, &oap::aa::EvdevTouchReader::gestureDetected,
                this, &AndroidAutoPlugin::gestureTriggered,
                Qt::QueuedConnection);

        // Configure navbar dimensions for touch letterbox calculation
        bool navbarDuringAA = true;
        QString navEdge = "bottom";
        if (hostContext_ && hostContext_->configService()) {
            QVariant showDuringAA = hostContext_->configService()->value("navbar.show_during_aa");
            navbarDuringAA = (showDuringAA.isNull() || showDuringAA.toBool());
            if (navbarDuringAA) {
                navEdge = hostContext_->configService()->value("navbar.edge").toString();
                if (navEdge.isEmpty()) navEdge = "bottom";
                touchReader_->setNavbar(true, 56, navEdge.toStdString());
                qCInfo(lcAA) << "Navbar touch zone:" << navEdge << "56px";
            }
        }

        // Compute content dimensions to match touch_screen_config
        // Uses same calculation as ServiceDiscoveryBuilder (single source of truth)
        auto [contentW, contentH] = oap::aa::ServiceDiscoveryBuilder::computeContentDimensions(
            aaW, aaH, displayW, displayH, navbarDuringAA, navEdge);
        touchReader_->setContentDimensions(contentW, contentH);
        touchReader_->computeLetterbox();
        qCInfo(lcAA) << "Content dims:" << contentW << "x" << contentH
                     << "(video:" << aaW << "x" << aaH << ")";
    } else {
        qCInfo(lcAA) << "No touch device found — touch input disabled";
    }

    // Wire dynamic display dimension updates from window resize detection
    if (displayInfo_) {
        connect(displayInfo_, &DisplayInfo::windowSizeChanged, this, [this]() {
            int w = displayInfo_->windowWidth();
            int h = displayInfo_->windowHeight();
            if (w > 0 && h > 0) {
                if (touchReader_)
                    touchReader_->setDisplayDimensions(w, h);
                if (aaService_)
                    aaService_->setDisplayDimensions(w, h);
            }
        });
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
    static const QStringList videoSettings = {
        QStringLiteral("video.resolution"),
        QStringLiteral("video.fps"),
    };
    if (!videoSettings.contains(path))
        return;

    // Update touch coordinate mapping for the new resolution
    if (path == QLatin1String("video.resolution") && touchReader_) {
        int aaW = 1280, aaH = 720;
        QString res = value.toString();
        if (res == QLatin1String("1080p")) { aaW = 1920; aaH = 1080; }
        else if (res == QLatin1String("480p")) { aaW = 800; aaH = 480; }
        touchReader_->setAAResolution(aaW, aaH);

        // Recompute content dimensions for the new video resolution
        int displayW = 1024, displayH = 600;
        if (displayInfo_ && displayInfo_->windowWidth() > 0)
            displayW = displayInfo_->windowWidth();
        if (displayInfo_ && displayInfo_->windowHeight() > 0)
            displayH = displayInfo_->windowHeight();

        bool navbarDuringAA = true;
        QString navEdge = "bottom";
        if (hostContext_ && hostContext_->configService()) {
            QVariant showVal = hostContext_->configService()->value("navbar.show_during_aa");
            navbarDuringAA = (showVal.isNull() || showVal.toBool());
            navEdge = hostContext_->configService()->value("navbar.edge").toString();
            if (navEdge.isEmpty()) navEdge = "bottom";
        }

        auto [contentW, contentH] = oap::aa::ServiceDiscoveryBuilder::computeContentDimensions(
            aaW, aaH, displayW, displayH, navbarDuringAA, navEdge);
        touchReader_->setContentDimensions(contentW, contentH);
        qCInfo(lcAA) << "Content dims updated:" << contentW << "x" << contentH
                     << "(video:" << aaW << "x" << aaH << ")";
    }

    if (!aaService_) return;

    using CS = oap::aa::AndroidAutoOrchestrator;
    auto state = static_cast<CS::ConnectionState>(aaService_->connectionState());
    if (state != CS::Connected && state != CS::Backgrounded)
        return;

    qCInfo(lcAA) << "Video setting changed (" << path << ") — reconnecting for renegotiation";
    // Queue the disconnect — calling it synchronously from inside configChanged
    // would spin a nested event loop mid-signal-emission and crash
    QMetaObject::invokeMethod(aaService_, [this]() {
        aaService_->disconnectAndRetrigger();
    }, Qt::QueuedConnection);
}

void AndroidAutoPlugin::stopAA()
{
    if (aaService_) {
        qCInfo(lcAA) << "Graceful AA shutdown requested";
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

    // Re-grab touch when returning to AA view (may have been ungrabbed by onDeactivated)
    using CS = oap::aa::AndroidAutoOrchestrator;
    auto connState = static_cast<CS::ConnectionState>(aaService_->connectionState());
    if (connState == CS::Connected || connState == CS::Backgrounded) {
        if (touchReader_) touchReader_->grab();
        if (connState == CS::Backgrounded) {
            aaService_->requestVideoFocus();
        }
        qCInfo(lcAA) << "Re-entering AA projection (state:" << aaService_->connectionState() << ")";
    }
}

void AndroidAutoPlugin::onDeactivated()
{
    if (aaService_ && aaService_->videoDecoder())
        aaService_->videoDecoder()->setVideoSink(nullptr);

    // Release touch grab so Wayland/Qt can handle touch on the home screen.
    // The AA connection may still be alive (backgrounded), but the view is gone.
    if (touchReader_)
        touchReader_->ungrab();
}

QUrl AndroidAutoPlugin::qmlComponent() const
{
    return QUrl(QStringLiteral("qrc:/OpenAutoProdigy/AndroidAutoMenu.qml"));
}

QUrl AndroidAutoPlugin::iconSource() const
{
    return {};
}

bool AndroidAutoPlugin::wantsFullscreen() const
{
    // When navbar.show_during_aa is true (default), AA does NOT want fullscreen
    // so that the navbar remains visible. When false, AA takes the full screen.
    if (hostContext_ && hostContext_->configService()) {
        QVariant showDuringAA = hostContext_->configService()->value("navbar.show_during_aa");
        bool result = !(showDuringAA.isNull() || showDuringAA.toBool());
        qCInfo(lcAA) << "wantsFullscreen:" << result
                     << "show_during_aa:" << showDuringAA
                     << "isNull:" << showDuringAA.isNull();
        return result;
    }
    qCWarning(lcAA) << "wantsFullscreen: no hostContext/configService, defaulting to true";
    return true;
}

} // namespace plugins
} // namespace oap
