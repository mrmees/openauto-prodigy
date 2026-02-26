#include <signal.h>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <memory>
#include "core/Configuration.hpp"
#include "core/YamlConfig.hpp"
#include "core/services/ConfigService.hpp"
#include "core/services/ThemeService.hpp"
#include "core/services/AudioService.hpp"
#include "core/services/IpcServer.hpp"
#include "core/services/EventBus.hpp"
#include "core/services/ActionRegistry.hpp"
#include "core/services/NotificationService.hpp"
#include "core/services/CompanionListenerService.hpp"
#include "core/services/SystemServiceClient.hpp"
#include "ui/NotificationModel.hpp"
#include "core/plugin/HostContext.hpp"
#include "core/plugin/PluginManager.hpp"
#include "plugins/android_auto/AndroidAutoPlugin.hpp"
#include "plugins/bt_audio/BtAudioPlugin.hpp"
#include "plugins/phone/PhonePlugin.hpp"
#include "ui/ApplicationController.hpp"
#include "ui/PluginModel.hpp"
#include "ui/PluginViewHost.hpp"
#include "ui/LauncherModel.hpp"
#include "ui/AudioDeviceModel.hpp"
#include "ui/CodecCapabilityModel.hpp"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("OpenAuto Prodigy");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("OpenAutoProdigy");
    app.setWindowIcon(QIcon(":/icons/prodigy-64.png"));

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
        // Sync YAML values into legacy Configuration (used by BT service, etc.)
        config->setWifiSsid(yamlConfig->wifiSsid());
        config->setWifiPassword(yamlConfig->wifiPassword());
        config->setTcpPort(yamlConfig->tcpPort());
        config->setVideoFps(yamlConfig->videoFps());
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
        // Fall back to bundled theme next to the executable
        QString bundledTheme = QCoreApplication::applicationDirPath() + "/../../config/themes/default";
        themeService->loadTheme(bundledTheme);
    }

    // --- Audio service (PipeWire) ---
    auto audioService = new oap::AudioService(&app);

    // Apply initial audio config from YAML
    auto outputDev = yamlConfig->valueByPath("audio.output_device").toString();
    if (outputDev.isEmpty()) outputDev = "auto";
    audioService->setOutputDevice(outputDev);
    audioService->setInputDevice(yamlConfig->microphoneDevice());
    audioService->setMasterVolume(yamlConfig->masterVolume());

    // --- Plugin infrastructure ---
    auto configService = std::make_unique<oap::ConfigService>(yamlConfig.get(), yamlPath);
    auto hostContext = std::make_unique<oap::HostContext>();
    hostContext->setConfigService(configService.get());
    hostContext->setThemeService(themeService);
    hostContext->setAudioService(audioService);

    // --- EventBus ---
    auto eventBus = new oap::EventBus(&app);
    hostContext->setEventBus(eventBus);

    // --- ActionRegistry ---
    auto actionRegistry = new oap::ActionRegistry(&app);
    hostContext->setActionRegistry(actionRegistry);

    // --- NotificationService ---
    auto notificationService = new oap::NotificationService(&app);
    hostContext->setNotificationService(notificationService);

    // --- Companion Listener ---
    oap::CompanionListenerService* companionListener = nullptr;
    QVariant companionEnabledVar = yamlConfig->valueByPath("companion.enabled");
    bool companionEnabled = companionEnabledVar.isValid() ? companionEnabledVar.toBool() : true;
    QVariant companionPortVar = yamlConfig->valueByPath("companion.port");
    int companionPort = companionPortVar.isValid() ? companionPortVar.toInt() : 9876;
    qInfo() << "Companion: enabled=" << companionEnabled << "port=" << companionPort;
    if (companionEnabled) {
        companionListener = new oap::CompanionListenerService(&app);
        companionListener->setWifiSsid(config->wifiSsid());
        companionListener->loadOrGenerateVehicleId();
        QFile secretFile(QDir::homePath() + "/.openauto/companion.key");
        if (secretFile.open(QIODevice::ReadOnly)) {
            QByteArray secret = secretFile.readAll().trimmed();
            companionListener->setSharedSecret(QString::fromUtf8(secret));
            qInfo() << "Companion: loaded secret from" << secretFile.fileName()
                     << "(" << secret.length() << "bytes)";
        } else {
            qWarning() << "Companion: no secret file at" << secretFile.fileName()
                        << "— pairing required";
        }
        if (companionListener->start(companionPort)) {
            qInfo() << "Companion: listening on port" << companionPort;
        } else {
            qWarning() << "Companion: FAILED to bind port" << companionPort;
        }
        hostContext->setCompanionListenerService(companionListener);
    } else {
        qInfo() << "Companion: disabled in config";
    }

    oap::PluginManager pluginManager(&app);

    // Register static (compiled-in) plugins
    auto aaPlugin = new oap::plugins::AndroidAutoPlugin(config, yamlConfig.get(), &app);
    pluginManager.registerStaticPlugin(aaPlugin);

    auto btAudioPlugin = new oap::plugins::BtAudioPlugin(&app);
    pluginManager.registerStaticPlugin(btAudioPlugin);

    auto phonePlugin = new oap::plugins::PhonePlugin(&app);
    pluginManager.registerStaticPlugin(phonePlugin);

    // Discover dynamic plugins from user directory
    pluginManager.discoverPlugins(QDir::homePath() + "/.openauto/plugins");

    // Initialize all plugins (static + dynamic)
    pluginManager.initializeAll(hostContext.get());

    // --- IPC server for web config panel ---
    auto ipcServer = new oap::IpcServer(&app);
    ipcServer->setConfig(yamlConfig.get(), yamlPath);
    ipcServer->setThemeService(themeService);
    ipcServer->setAudioService(audioService);
    ipcServer->setPluginManager(&pluginManager);
    if (companionListener)
        ipcServer->setCompanionListenerService(companionListener);
    ipcServer->start();

    // --- System service client (IPC to openauto-system daemon) ---
    auto* systemClient = new oap::SystemServiceClient(&app);
    if (companionListener)
        companionListener->setSystemServiceClient(systemClient);

    QQuickStyle::setStyle("Material");

    QQmlApplicationEngine engine;

    // Plugin model for QML nav strip (needs engine for PluginRuntimeContext)
    auto pluginModel = new oap::PluginModel(&pluginManager, &engine, &app);

    auto launcherModel = new oap::LauncherModel(yamlConfig.get(), &app);
    auto notificationModel = new oap::NotificationModel(notificationService, &app);

    // Register built-in actions (after pluginModel exists)
    actionRegistry->registerAction("app.quit", [](const QVariant&) {
        QGuiApplication::quit();
    });
    actionRegistry->registerAction("app.home", [pluginModel](const QVariant&) {
        pluginModel->setActivePlugin(QString());
    });
    actionRegistry->registerAction("theme.toggle", [themeService](const QVariant&) {
        themeService->toggleMode();
    });

    engine.rootContext()->setContextProperty("ActionRegistry", actionRegistry);
    engine.rootContext()->setContextProperty("ThemeService", themeService);
    engine.rootContext()->setContextProperty("ApplicationController", appController);
    engine.rootContext()->setContextProperty("PluginModel", pluginModel);
    engine.rootContext()->setContextProperty("LauncherModel", launcherModel);
    engine.rootContext()->setContextProperty("NotificationModel", notificationModel);
    engine.rootContext()->setContextProperty("NotificationService", notificationService);

    // Expose PhonePlugin globally for IncomingCallOverlay in Shell.qml
    engine.rootContext()->setContextProperty("PhonePlugin", phonePlugin);

    engine.rootContext()->setContextProperty("AudioService", audioService);

    auto* outputDeviceModel = new oap::AudioDeviceModel(
        oap::AudioDeviceModel::Output, audioService->deviceRegistry(), audioService);
    auto* inputDeviceModel = new oap::AudioDeviceModel(
        oap::AudioDeviceModel::Input, audioService->deviceRegistry(), audioService);
    engine.rootContext()->setContextProperty("AudioOutputDeviceModel", outputDeviceModel);
    engine.rootContext()->setContextProperty("AudioInputDeviceModel", inputDeviceModel);

    auto* codecCapModel = new oap::CodecCapabilityModel(&app);
    engine.rootContext()->setContextProperty("CodecCapabilityModel", codecCapModel);

    engine.rootContext()->setContextProperty("ConfigService", configService.get());

    if (companionListener)
        engine.rootContext()->setContextProperty("CompanionService", companionListener);

    engine.rootContext()->setContextProperty("SystemService", systemClient);

    // Qt 6.5+ uses /qt/qml/ prefix, Qt 6.4 uses direct URI prefix
    QUrl url(QStringLiteral("qrc:/OpenAutoProdigy/main.qml"));
    if (QFile::exists(QStringLiteral(":/qt/qml/OpenAutoProdigy/main.qml")))
        url = QUrl(QStringLiteral("qrc:/qt/qml/OpenAutoProdigy/main.qml"));

    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    // Wire PluginViewHost to the QML host item
    auto* rootObj = engine.rootObjects().first();
    auto* hostItem = rootObj->findChild<QQuickItem*>("pluginContentHost");
    if (hostItem)
        pluginModel->viewHost()->setHostItem(hostItem);

    // Wire AA plugin activation/deactivation to PluginModel
    // NOTE: Must be after host item wiring — loadView requires hostItem_ to be set.
    QObject::connect(aaPlugin, &oap::plugins::AndroidAutoPlugin::requestActivation,
                     pluginModel, [pluginModel]() {
        pluginModel->setActivePlugin("org.openauto.android-auto");
    });
    QObject::connect(aaPlugin, &oap::plugins::AndroidAutoPlugin::requestDeactivation,
                     pluginModel, [pluginModel]() {
        if (pluginModel->activePluginId() == "org.openauto.android-auto")
            pluginModel->setActivePlugin(QString());
    });

    // SIGUSR1 → disconnect AA session (ShutdownRequest + teardown, keep listening)
    static oap::plugins::AndroidAutoPlugin* g_aaPlugin = aaPlugin;
    signal(SIGUSR1, [](int) {
        QMetaObject::invokeMethod(g_aaPlugin, [](){ g_aaPlugin->stopAA(); },
                                   Qt::QueuedConnection);
    });

    int ret = app.exec();

    // Teardown order matters: deactivate plugin view (uses QML engine)
    // BEFORE engine is destroyed (stack-local), BEFORE plugin shutdown.
    pluginModel->setActivePlugin(QString());

    pluginManager.shutdownAll();

    return ret;
}
