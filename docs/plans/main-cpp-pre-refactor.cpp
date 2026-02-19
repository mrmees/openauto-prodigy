#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDir>
#include <QFile>
#include <memory>
#include "core/Configuration.hpp"
#include "core/aa/AndroidAutoService.hpp"
#include "core/aa/EvdevTouchReader.hpp"
#include "ui/ThemeController.hpp"
#include "ui/ApplicationController.hpp"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("OpenAuto Prodigy");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("OpenAutoProdigy");

    auto config = std::make_shared<oap::Configuration>();

    // Load from default path if exists; otherwise use built-in defaults
    QString configPath = QDir::homePath() + "/.openauto/openauto_system.ini";
    if (QFile::exists(configPath))
        config->load(configPath);

    auto theme = new oap::ThemeController(config, &app);
    auto appController = new oap::ApplicationController(&app);
    auto aaService = new oap::aa::AndroidAutoService(config, &app);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("ThemeController", theme);
    engine.rootContext()->setContextProperty("ApplicationController", appController);
    engine.rootContext()->setContextProperty("AndroidAutoService", aaService);
    engine.rootContext()->setContextProperty("VideoDecoder", aaService->videoDecoder());
    engine.rootContext()->setContextProperty("TouchHandler", aaService->touchHandler());

    // Qt 6.5+ uses /qt/qml/ prefix, Qt 6.4 uses direct URI prefix
    QUrl url(QStringLiteral("qrc:/OpenAutoProdigy/main.qml"));
    if (QFile::exists(QStringLiteral(":/qt/qml/OpenAutoProdigy/main.qml")))
        url = QUrl(QStringLiteral("qrc:/qt/qml/OpenAutoProdigy/main.qml"));

    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    // Auto-switch to AA view on connect, back to launcher on disconnect
    QObject::connect(aaService, &oap::aa::AndroidAutoService::connectionStateChanged,
                     appController, [aaService, appController]() {
        if (aaService->connectionState() == oap::aa::AndroidAutoService::Connected) {
            appController->navigateTo(oap::ApplicationTypes::AndroidAuto);
        } else if (aaService->connectionState() == oap::aa::AndroidAutoService::Disconnected
                   || aaService->connectionState() == oap::aa::AndroidAutoService::WaitingForDevice) {
            if (appController->currentApplication() == oap::ApplicationTypes::AndroidAuto) {
                appController->navigateTo(oap::ApplicationTypes::Launcher);
            }
        }
    });

    // Start evdev touch reader if device exists (Pi only, not dev VM)
    oap::aa::EvdevTouchReader* touchReader = nullptr;
    QString touchDevice = "/dev/input/event4";  // TODO: make configurable
    if (QFile::exists(touchDevice)) {
        touchReader = new oap::aa::EvdevTouchReader(
            aaService->touchHandler(),
            touchDevice.toStdString(),
            4095, 4095,   // evdev axis range (read from device at runtime)
            1280, 720,    // AA touch coordinate space (must match video resolution)
            1024, 600,    // physical display resolution
            &app);
        touchReader->start();
    }

    // Start AA service after QML is loaded
    aaService->start();

    int ret = app.exec();

    if (touchReader) {
        touchReader->requestStop();
        touchReader->wait();
    }

    return ret;
}
