#include "AndroidAutoPlugin.hpp"
#include "core/aa/AndroidAutoService.hpp"
#include "core/aa/EvdevTouchReader.hpp"
#include "core/plugin/IHostContext.hpp"
#include "ui/ApplicationController.hpp"
#include "ui/ApplicationTypes.hpp"
#include "core/Configuration.hpp"
#include "core/InputDeviceScanner.hpp"
#include "core/services/IConfigService.hpp"
#include <boost/log/trivial.hpp>
#include <QQmlContext>
#include <QFile>

namespace oap {
namespace plugins {

AndroidAutoPlugin::AndroidAutoPlugin(std::shared_ptr<oap::Configuration> config,
                                     oap::ApplicationController* appController,
                                     oap::YamlConfig* yamlConfig,
                                     QObject* parent)
    : QObject(parent)
    , config_(std::move(config))
    , yamlConfig_(yamlConfig)
    , appController_(appController)
{
}

AndroidAutoPlugin::~AndroidAutoPlugin()
{
    shutdown();
}

bool AndroidAutoPlugin::initialize(IHostContext* context)
{
    hostContext_ = context;

    // Create AA service with audio routing through PipeWire
    auto* audioService = context ? context->audioService() : nullptr;
    aaService_ = new oap::aa::AndroidAutoService(config_, audioService, yamlConfig_, this);

    // Connect navigation: auto-switch to AA on connect, back to launcher on disconnect.
    // TODO: Replace ApplicationController dependency with IEventBus publish.
    QObject::connect(aaService_, &oap::aa::AndroidAutoService::connectionStateChanged,
                     appController_, [this]() {
        using CS = oap::aa::AndroidAutoService;
        if (aaService_->connectionState() == CS::Connected) {
            // Grab touch device for AA coordinate mapping
            if (touchReader_) touchReader_->grab();
            appController_->navigateTo(oap::ApplicationTypes::AndroidAuto);
        } else if (aaService_->connectionState() == CS::Disconnected
                   || aaService_->connectionState() == CS::WaitingForDevice) {
            // Release touch device back to Wayland for normal UI
            if (touchReader_) touchReader_->ungrab();
            if (appController_->currentApplication() == oap::ApplicationTypes::AndroidAuto) {
                appController_->navigateTo(oap::ApplicationTypes::Launcher);
            }
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
            BOOST_LOG_TRIVIAL(info) << "[AAPlugin] Auto-detected touch device: "
                                    << touchDevice.toStdString();
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

    if (!touchDevice.isEmpty() && QFile::exists(touchDevice)) {
        touchReader_ = new oap::aa::EvdevTouchReader(
            aaService_->touchHandler(),
            touchDevice.toStdString(),
            4095, 4095,   // evdev axis range (overridden by EVIOCGABS in reader)
            1280, 720,    // AA touch coordinate space (matches video resolution)
            displayW, displayH,
            this);
        touchReader_->start();
        BOOST_LOG_TRIVIAL(info) << "[AAPlugin] Touch: " << touchDevice.toStdString()
                                << " display=" << displayW << "x" << displayH;
    } else {
        BOOST_LOG_TRIVIAL(info) << "[AAPlugin] No touch device found — touch input disabled";
    }

    // Start AA service — it needs to listen for connections immediately
    aaService_->start();

    if (hostContext_)
        hostContext_->log(LogLevel::Info, QStringLiteral("Android Auto plugin initialized"));

    return true;
}

void AndroidAutoPlugin::shutdown()
{
    if (touchReader_) {
        touchReader_->requestStop();
        touchReader_->wait();
        touchReader_ = nullptr;
    }
    // AndroidAutoService cleanup happens via QObject parent deletion
    aaService_ = nullptr;
}

void AndroidAutoPlugin::onActivated(QQmlContext* context)
{
    if (!context || !aaService_) return;

    // Expose AA objects to the plugin's QML view
    context->setContextProperty("AndroidAutoService", aaService_);
    context->setContextProperty("VideoDecoder", aaService_->videoDecoder());
    context->setContextProperty("TouchHandler", aaService_->touchHandler());
}

void AndroidAutoPlugin::onDeactivated()
{
    // Child context is destroyed by PluginRuntimeContext — no cleanup needed here
}

QUrl AndroidAutoPlugin::qmlComponent() const
{
    // Points to the AA QML view in the app's resource bundle.
    // In the future this could come from the plugin's own resources.
    return QUrl(QStringLiteral("qrc:/OpenAutoProdigy/AndroidAutoMenu.qml"));
}

QUrl AndroidAutoPlugin::iconSource() const
{
    return {};  // Font-based icons — see MaterialIcon.qml (\ueff7 directions_car)
}

void AndroidAutoPlugin::setGlobalContextProperties(QQmlContext* rootContext)
{
    if (!rootContext || !aaService_) return;

    // Transition: Shell.qml references AndroidAutoService.connectionState globally.
    // Once Shell uses PluginModel.activePluginFullscreen, these can be removed.
    rootContext->setContextProperty("AndroidAutoService", aaService_);
    rootContext->setContextProperty("VideoDecoder", aaService_->videoDecoder());
    rootContext->setContextProperty("TouchHandler", aaService_->touchHandler());
}

} // namespace plugins
} // namespace oap
