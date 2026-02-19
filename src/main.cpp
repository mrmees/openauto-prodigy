#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDir>
#include <QFile>
#include <memory>
#include "core/Configuration.hpp"
#include "core/YamlConfig.hpp"
#include "core/services/ConfigService.hpp"
#include "core/plugin/HostContext.hpp"
#include "core/plugin/PluginManager.hpp"
#include "core/aa/AndroidAutoService.hpp"
#include "core/aa/EvdevTouchReader.hpp"
#include "ui/ThemeController.hpp"
#include "ui/ApplicationController.hpp"
#include "ui/PluginModel.hpp"

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

    // Try YAML config first, fall back to INI
    QString yamlPath = QDir::homePath() + "/.openauto/config.yaml";
    auto yamlConfig = std::make_shared<oap::YamlConfig>();
    if (QFile::exists(yamlPath)) {
        yamlConfig->load(yamlPath);
    } else if (QFile::exists(configPath)) {
        // Legacy INI — migrate values to YamlConfig
        yamlConfig->setWifiSsid(config->wifiSsid());
        yamlConfig->setWifiPassword(config->wifiPassword());
        yamlConfig->setTcpPort(config->tcpPort());
        yamlConfig->setVideoFps(config->videoFps());
        // Save as YAML for next boot
        QDir().mkpath(QDir::homePath() + "/.openauto");
        yamlConfig->save(yamlPath);
    }

    // --- Plugin infrastructure ---
    auto configService = std::make_unique<oap::ConfigService>(yamlConfig.get(), yamlPath);
    auto hostContext = std::make_unique<oap::HostContext>();
    hostContext->setConfigService(configService.get());

    // PluginManager must be destroyed BEFORE plugins it manages
    // (but static plugins are destroyed by QObject parent, so order matters less)
    oap::PluginManager pluginManager(&app);
    pluginManager.discoverPlugins(QDir::homePath() + "/.openauto/plugins");
    pluginManager.initializeAll(hostContext.get());

    // --- Legacy UI controllers (will be replaced by plugin system) ---
    auto theme = new oap::ThemeController(config, &app);
    auto appController = new oap::ApplicationController(&app);

    // --- AA service (will become AndroidAutoPlugin in Phase E) ---
    auto aaService = new oap::aa::AndroidAutoService(config, &app);

    // Plugin model for QML nav strip
    auto pluginModel = new oap::PluginModel(&pluginManager, &app);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("ThemeController", theme);
    engine.rootContext()->setContextProperty("ApplicationController", appController);
    engine.rootContext()->setContextProperty("PluginModel", pluginModel);
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

    // Explicit shutdown before automatic destruction
    pluginManager.shutdownAll();

    return ret;
}
