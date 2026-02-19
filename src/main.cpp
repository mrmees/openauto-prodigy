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
#include "core/aa/AaBootstrap.hpp"
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

    oap::PluginManager pluginManager(&app);
    pluginManager.discoverPlugins(QDir::homePath() + "/.openauto/plugins");
    pluginManager.initializeAll(hostContext.get());

    // --- Legacy UI controllers (will be replaced by plugin system) ---
    auto theme = new oap::ThemeController(config, &app);
    auto appController = new oap::ApplicationController(&app);

    // Plugin model for QML nav strip
    auto pluginModel = new oap::PluginModel(&pluginManager, &app);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("ThemeController", theme);
    engine.rootContext()->setContextProperty("ApplicationController", appController);
    engine.rootContext()->setContextProperty("PluginModel", pluginModel);

    // Qt 6.5+ uses /qt/qml/ prefix, Qt 6.4 uses direct URI prefix
    QUrl url(QStringLiteral("qrc:/OpenAutoProdigy/main.qml"));
    if (QFile::exists(QStringLiteral(":/qt/qml/OpenAutoProdigy/main.qml")))
        url = QUrl(QStringLiteral("qrc:/qt/qml/OpenAutoProdigy/main.qml"));

    // AA bootstrap: creates service, touch reader, connects signals,
    // sets QML context properties, starts the service.
    // Will be replaced by AndroidAutoPlugin in Phase E.
    auto aaRuntime = oap::aa::startAa(config, appController, &engine, &app);

    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    int ret = app.exec();

    oap::aa::stopAa(aaRuntime);
    pluginManager.shutdownAll();

    return ret;
}
