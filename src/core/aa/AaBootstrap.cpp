#include "AaBootstrap.hpp"
#include "AndroidAutoService.hpp"
#include "EvdevTouchReader.hpp"
#include "../../ui/ApplicationController.hpp"
#include "../../ui/ApplicationTypes.hpp"
#include "../../core/Configuration.hpp"
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFile>

namespace oap {
namespace aa {

AaRuntime startAa(std::shared_ptr<oap::Configuration> config,
                   oap::ApplicationController* appController,
                   QQmlApplicationEngine* engine,
                   QObject* parent)
{
    AaRuntime runtime;

    // Create AA service
    runtime.service = new AndroidAutoService(config, parent);

    // Expose AA objects to QML
    engine->rootContext()->setContextProperty("AndroidAutoService", runtime.service);
    engine->rootContext()->setContextProperty("VideoDecoder", runtime.service->videoDecoder());
    engine->rootContext()->setContextProperty("TouchHandler", runtime.service->touchHandler());

    // Auto-switch to AA view on connect, back to launcher on disconnect
    QObject::connect(runtime.service, &AndroidAutoService::connectionStateChanged,
                     appController, [service = runtime.service, appController]() {
        if (service->connectionState() == AndroidAutoService::Connected) {
            appController->navigateTo(oap::ApplicationTypes::AndroidAuto);
        } else if (service->connectionState() == AndroidAutoService::Disconnected
                   || service->connectionState() == AndroidAutoService::WaitingForDevice) {
            if (appController->currentApplication() == oap::ApplicationTypes::AndroidAuto) {
                appController->navigateTo(oap::ApplicationTypes::Launcher);
            }
        }
    });

    // Start evdev touch reader if device exists (Pi only, not dev VM)
    QString touchDevice = "/dev/input/event4";  // TODO: make configurable via config
    if (QFile::exists(touchDevice)) {
        runtime.touchReader = new EvdevTouchReader(
            runtime.service->touchHandler(),
            touchDevice.toStdString(),
            4095, 4095,   // evdev axis range
            1280, 720,    // AA touch coordinate space
            1024, 600,    // physical display resolution
            parent);
        runtime.touchReader->start();
    }

    // Start AA service
    runtime.service->start();

    return runtime;
}

void stopAa(AaRuntime& runtime)
{
    if (runtime.touchReader) {
        runtime.touchReader->requestStop();
        runtime.touchReader->wait();
    }
    // AndroidAutoService cleanup happens via QObject parent deletion
}

} // namespace aa
} // namespace oap
