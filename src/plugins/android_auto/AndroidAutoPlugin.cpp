#include "AndroidAutoPlugin.hpp"
#include "core/aa/AndroidAutoOrchestrator.hpp"
#include "core/aa/AndroidAutoRuntimeBridge.hpp"
#include "core/aa/EvdevTouchReader.hpp"
#include "core/aa/EvdevCoordBridge.hpp"
#include "core/aa/ServiceDiscoveryBuilder.hpp"
#include "core/plugin/IHostContext.hpp"
#include "core/services/IAudioService.hpp"
#include "core/services/IConfigService.hpp"
#include "core/services/ConfigService.hpp"
#include "core/services/EqualizerService.hpp"
#include "ui/DisplayInfo.hpp"
#include <algorithm>
#include <cmath>
#include "../../core/Logging.hpp"
#include <QQmlContext>
#include <QFile>

namespace oap {
namespace plugins {

AndroidAutoPlugin::AndroidAutoPlugin(oap::YamlConfig* yamlConfig,
                                     QObject* parent)
    : QObject(parent)
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

oap::aa::EvdevCoordBridge* AndroidAutoPlugin::coordBridge() const
{
    return runtimeBridge_ ? runtimeBridge_->coordBridge() : nullptr;
}

bool AndroidAutoPlugin::initialize(IHostContext* context)
{
    hostContext_ = context;

    // Create AA orchestrator with audio routing through PipeWire
    auto* audioService = context ? context->audioService() : nullptr;
    auto* eventBus = context ? context->eventBus() : nullptr;
    auto* eqService = context ? dynamic_cast<oap::EqualizerService*>(context->equalizerService()) : nullptr;
    auto* configService = context ? context->configService() : nullptr;
    aaService_ = new oap::aa::AndroidAutoOrchestrator(configService, audioService, yamlConfig_, eventBus, eqService, this);

    // Give orchestrator access to theme service for UiConfigRequest sending
    if (context)
        aaService_->setThemeService(context->themeService());

    // Connect navigation: emit signals for PluginModel to handle activation/deactivation.
    QObject::connect(aaService_, &oap::aa::AndroidAutoOrchestrator::connectionStateChanged,
                     this, [this]() {
        using CS = oap::aa::AndroidAutoOrchestrator;
        auto state = static_cast<CS::ConnectionState>(aaService_->connectionState());
        if (state == CS::Connected) {
            if (runtimeBridge_) runtimeBridge_->grab();
            emit requestActivation();
        } else if (state == CS::Backgrounded) {
            if (runtimeBridge_) runtimeBridge_->ungrab();
            emit requestDeactivation();
        } else if (state == CS::Disconnected
                   || state == CS::WaitingForDevice) {
            if (runtimeBridge_) runtimeBridge_->ungrab();
            emit requestDeactivation();
        }
    });

    // Delegate touch/display/navbar platform policy to the runtime bridge
    runtimeBridge_ = new oap::aa::AndroidAutoRuntimeBridge(this);
    runtimeBridge_->setup(aaService_, displayInfo_, configService);

    // Relay 3-finger gesture from bridge to plugin signal
    connect(runtimeBridge_, &oap::aa::AndroidAutoRuntimeBridge::gestureTriggered,
            this, &AndroidAutoPlugin::gestureTriggered,
            Qt::QueuedConnection);

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
    if (path == QLatin1String("video.resolution") && runtimeBridge_ && runtimeBridge_->touchReader()) {
        auto* touchReader = runtimeBridge_->touchReader();
        int aaW = 1280, aaH = 720;
        QString res = value.toString();
        if (res == QLatin1String("1080p")) { aaW = 1920; aaH = 1080; }
        else if (res == QLatin1String("480p")) { aaW = 800; aaH = 480; }
        touchReader->setAAResolution(aaW, aaH);

        // Recompute content dimensions for the new video resolution
        int displayW = runtimeBridge_->displayWidth();
        int displayH = runtimeBridge_->displayHeight();

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
        touchReader->setContentDimensions(contentW, contentH);
        qCInfo(lcAA) << "Content dims updated:" << contentW << "x" << contentH
                     << "(video:" << aaW << "x" << aaH << ")";
    }

    if (!aaService_) return;

    using CS = oap::aa::AndroidAutoOrchestrator;
    auto state = static_cast<CS::ConnectionState>(aaService_->connectionState());
    if (state != CS::Connected && state != CS::Backgrounded)
        return;

    qCInfo(lcAA) << "Video setting changed (" << path << ") — reconnecting for renegotiation";
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
    if (runtimeBridge_) {
        runtimeBridge_->shutdown();
        // runtimeBridge_ is parented to this, will be deleted automatically
        runtimeBridge_ = nullptr;
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
        if (runtimeBridge_) runtimeBridge_->grab();
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
    if (runtimeBridge_)
        runtimeBridge_->ungrab();
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
