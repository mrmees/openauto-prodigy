#include "AndroidAutoPlugin.hpp"
#include "core/aa/AndroidAutoService.hpp"
#include "core/aa/EvdevTouchReader.hpp"
#include "core/plugin/IHostContext.hpp"
#include "ui/ApplicationController.hpp"
#include "ui/ApplicationTypes.hpp"
#include "core/Configuration.hpp"
#include <QQmlContext>
#include <QFile>

namespace oap {
namespace plugins {

AndroidAutoPlugin::AndroidAutoPlugin(std::shared_ptr<oap::Configuration> config,
                                     oap::ApplicationController* appController,
                                     QObject* parent)
    : QObject(parent)
    , config_(std::move(config))
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

    // Create AA service (uses legacy Configuration — will migrate to IConfigService)
    aaService_ = new oap::aa::AndroidAutoService(config_, this);

    // Connect navigation: auto-switch to AA on connect, back to launcher on disconnect.
    // TODO: Replace ApplicationController dependency with IEventBus publish.
    QObject::connect(aaService_, &oap::aa::AndroidAutoService::connectionStateChanged,
                     appController_, [this]() {
        using CS = oap::aa::AndroidAutoService;
        if (aaService_->connectionState() == CS::Connected) {
            appController_->navigateTo(oap::ApplicationTypes::AndroidAuto);
        } else if (aaService_->connectionState() == CS::Disconnected
                   || aaService_->connectionState() == CS::WaitingForDevice) {
            if (appController_->currentApplication() == oap::ApplicationTypes::AndroidAuto) {
                appController_->navigateTo(oap::ApplicationTypes::Launcher);
            }
        }
    });

    // Start evdev touch reader if device exists (Pi only, not dev VM)
    QString touchDevice = QStringLiteral("/dev/input/event4");  // TODO: configurable
    if (QFile::exists(touchDevice)) {
        touchReader_ = new oap::aa::EvdevTouchReader(
            aaService_->touchHandler(),
            touchDevice.toStdString(),
            4095, 4095,   // evdev axis range
            1280, 720,    // AA touch coordinate space
            1024, 600,    // physical display resolution
            this);
        touchReader_->start();
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
    return QUrl(QStringLiteral("qrc:/icons/android-auto.svg"));
}

} // namespace plugins
} // namespace oap
