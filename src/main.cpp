#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDir>
#include <QFile>
#include <memory>
#include "core/Configuration.hpp"
#include "core/YamlConfig.hpp"
#include "core/services/ConfigService.hpp"
#include "core/services/ThemeService.hpp"
#include "core/services/AudioService.hpp"
#include "core/plugin/HostContext.hpp"
#include "core/plugin/PluginManager.hpp"
#include "plugins/android_auto/AndroidAutoPlugin.hpp"
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

    auto appController = new oap::ApplicationController(&app);

    // --- Theme service ---
    auto themeService = new oap::ThemeService(&app);
    QString themePath = QDir::homePath() + "/.openauto/themes/default";
    if (!themeService->loadTheme(themePath)) {
        // No user theme installed — ThemeService returns transparent for all colors.
        // On Pi, the install script will deploy the default theme.
    }

    // --- Audio service (PipeWire) ---
    auto audioService = new oap::AudioService(&app);

    // --- Plugin infrastructure ---
    auto configService = std::make_unique<oap::ConfigService>(yamlConfig.get(), yamlPath);
    auto hostContext = std::make_unique<oap::HostContext>();
    hostContext->setConfigService(configService.get());
    hostContext->setThemeService(themeService);
    hostContext->setAudioService(audioService);

    oap::PluginManager pluginManager(&app);

    // Register AA as a static (compiled-in) plugin
    auto aaPlugin = new oap::plugins::AndroidAutoPlugin(config, appController, &app);
    pluginManager.registerStaticPlugin(aaPlugin);

    // Discover dynamic plugins from user directory
    pluginManager.discoverPlugins(QDir::homePath() + "/.openauto/plugins");

    // Initialize all plugins (static + dynamic)
    pluginManager.initializeAll(hostContext.get());

    // Plugin model for QML nav strip
    auto pluginModel = new oap::PluginModel(&pluginManager, &app);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("ThemeService", themeService);
    engine.rootContext()->setContextProperty("ApplicationController", appController);
    engine.rootContext()->setContextProperty("PluginModel", pluginModel);

    // Transition: expose AA objects globally for Shell.qml fullscreen check.
    // TODO: Remove once Shell uses PluginModel.activePluginFullscreen.
    aaPlugin->setGlobalContextProperties(engine.rootContext());

    // Qt 6.5+ uses /qt/qml/ prefix, Qt 6.4 uses direct URI prefix
    QUrl url(QStringLiteral("qrc:/OpenAutoProdigy/main.qml"));
    if (QFile::exists(QStringLiteral(":/qt/qml/OpenAutoProdigy/main.qml")))
        url = QUrl(QStringLiteral("qrc:/qt/qml/OpenAutoProdigy/main.qml"));

    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    int ret = app.exec();

    pluginManager.shutdownAll();

    return ret;
}
