#include <signal.h>
#ifdef HAS_SYSTEMD
#include <systemd/sd-daemon.h>
#endif
#include <QGuiApplication>
#include <QCommandLineParser>
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
#include "core/Logging.hpp"
#include "core/YamlConfig.hpp"
#include "core/services/ConfigService.hpp"
#include "core/services/ThemeService.hpp"
#include "core/services/DisplayService.hpp"
#include "core/services/AudioService.hpp"
#include "core/services/IpcServer.hpp"
#include "core/services/EventBus.hpp"
#include "core/services/ActionRegistry.hpp"
#include "core/services/NotificationService.hpp"
#include "core/services/CompanionListenerService.hpp"
#include "core/services/SystemServiceClient.hpp"
#include "core/services/BluetoothManager.hpp"
#include "core/services/EqualizerService.hpp"
#include "ui/NotificationModel.hpp"
#include "core/plugin/HostContext.hpp"
#include "core/plugin/PluginManager.hpp"
#include "plugins/android_auto/AndroidAutoPlugin.hpp"
#include "plugins/bt_audio/BtAudioPlugin.hpp"
#include "plugins/phone/PhonePlugin.hpp"
#include "plugins/equalizer/EqualizerPlugin.hpp"
#include "ui/ApplicationController.hpp"
#include "ui/NavbarController.hpp"
#include "core/aa/EvdevCoordBridge.hpp"
#include "ui/PluginModel.hpp"
#include "ui/PluginViewHost.hpp"
#include "ui/LauncherModel.hpp"
#include "ui/AudioDeviceModel.hpp"
#include "ui/CodecCapabilityModel.hpp"
#include "ui/DisplayInfo.hpp"
#include <QQuickWindow>
#include <algorithm>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("OpenAuto Prodigy");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("OpenAutoProdigy");
    app.setWindowIcon(QIcon(":/icons/prodigy-64.png"));

    // --- CLI argument parsing ---
    QCommandLineParser parser;
    parser.setApplicationDescription("OpenAuto Prodigy — Wireless Android Auto head unit");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption verboseOption("verbose",
                                     "Enable verbose debug logging for all components");
    parser.addOption(verboseOption);

    QCommandLineOption logFileOption("log-file",
                                     "Write log output to file in addition to stderr",
                                     "path");
    parser.addOption(logFileOption);

    QCommandLineOption geometryOption("geometry",
                                      "Run windowed at WxH resolution (for testing)",
                                      "WxH");
    parser.addOption(geometryOption);

    parser.process(app);

    // --- Geometry override (windowed mode for resolution testing) ---
    int geomW = 0, geomH = 0;
    if (parser.isSet(geometryOption)) {
        auto parts = parser.value(geometryOption).split('x');
        if (parts.size() == 2) {
            geomW = parts[0].toInt();
            geomH = parts[1].toInt();
        }
        if (geomW <= 0 || geomH <= 0) {
            qCritical() << "Invalid --geometry format. Use WxH (e.g., --geometry 800x480)";
            return 1;
        }
    }

    // --- Install log handler EARLY (before any other initialization) ---
    if (parser.isSet(logFileOption))
        oap::setLogFile(parser.value(logFileOption));
    oap::installLogHandler();

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

    // --- Configure logging from CLI + YAML ---
    bool cliVerbose = parser.isSet(verboseOption);
    bool cfgVerbose = yamlConfig->valueByPath("logging.verbose").toBool();
    if (cliVerbose || cfgVerbose) {
        oap::setVerbose(true);
        qCInfo(lcCore) << "Verbose logging enabled" << (cliVerbose ? "(CLI)" : "(config)");
    } else {
        QStringList debugCategories = yamlConfig->valueByPath("logging.debug_categories").toStringList();
        if (!debugCategories.isEmpty()) {
            oap::setDebugCategories(debugCategories);
            qCInfo(lcCore) << "Debug categories:" << debugCategories;
        }
    }

    // --- DisplayInfo (window dimensions bridge for QML UiMetrics) ---
    auto* displayInfo = new oap::DisplayInfo(&app);
    if (geomW > 0 && geomH > 0) {
        displayInfo->setWindowSize(geomW, geomH);
        qCInfo(lcCore) << "Geometry override:" << geomW << "x" << geomH << "(windowed mode)";
    }
    // DisplayInfo defaults to 1024x600, overwritten by QQuickWindow signals

    // Log active UI overrides
    {
        auto logUiOverride = [&](const char* key) {
            auto v = yamlConfig->valueByPath(key);
            if (v.isValid() && v.toDouble() > 0)
                qCInfo(lcCore) << "UI override:" << key << "=" << v.toString();
        };
        logUiOverride("ui.scale");
        logUiOverride("ui.fontScale");
        for (const char* tok : {"rowH","touchMin","fontTitle","fontBody","fontSmall",
                                 "fontHeading","fontTiny","headerH","iconSize","radius",
                                 "radiusSmall","radiusLarge","tileW","tileH",
                                 "trackThick","trackThin","knobSize","knobSizeSmall",
                                 "albumArt","callBtnSize","overlayBtnW","overlayBtnH"}) {
            logUiOverride((std::string("ui.tokens.") + tok).c_str());
        }
    }

    auto appController = new oap::ApplicationController(&app);

    // --- Theme service ---
    auto themeService = new oap::ThemeService(&app);

    // Scan theme directories: user themes first (override bundled), then bundled
    QStringList themeSearchPaths;
    themeSearchPaths << QDir::homePath() + "/.openauto/themes";
    themeSearchPaths << QCoreApplication::applicationDirPath() + "/../../config/themes";
    themeService->scanThemeDirectories(themeSearchPaths);

    // Load theme from config (or default)
    QString savedTheme = yamlConfig->valueByPath("display.theme").toString();
    if (savedTheme.isEmpty()) savedTheme = "default";
    if (!themeService->setTheme(savedTheme)) {
        qCWarning(lcCore) << "Failed to load theme:" << savedTheme << "- falling back to default";
        themeService->setTheme("default");
    }

    // Load user's wallpaper override (empty = theme default, "none" = no wallpaper, file:// = custom)
    QVariant savedWallpaper = yamlConfig->valueByPath("display.wallpaper_override");
    if (savedWallpaper.isValid())
        themeService->setWallpaperOverride(savedWallpaper.toString());

    // --- Display service (brightness) ---
    auto displayService = new oap::DisplayService(&app);
    QVariant savedBrightness = yamlConfig->valueByPath("display.brightness");
    if (savedBrightness.isValid())
        displayService->setBrightness(savedBrightness.toInt());

    // --- Audio service (PipeWire) ---
    auto audioService = new oap::AudioService(&app);

    // Apply initial audio config from YAML
    auto outputDev = yamlConfig->valueByPath("audio.output_device").toString();
    if (outputDev.isEmpty()) outputDev = "auto";
    audioService->setOutputDevice(outputDev);
    audioService->setInputDevice(yamlConfig->microphoneDevice());
    audioService->setMasterVolume(yamlConfig->masterVolume());

    // --- Equalizer service (depends on YamlConfig) ---
    auto eqService = new oap::EqualizerService(yamlConfig.get(), &app);

    // Flush EQ config on shutdown
    QObject::connect(&app, &QGuiApplication::aboutToQuit, eqService, &oap::EqualizerService::saveNow);

    // --- Plugin infrastructure ---
    auto configService = std::make_unique<oap::ConfigService>(yamlConfig.get(), yamlPath);
    auto hostContext = std::make_unique<oap::HostContext>();
    hostContext->setConfigService(configService.get());
    hostContext->setThemeService(themeService);
    hostContext->setDisplayService(displayService);
    hostContext->setAudioService(audioService);
    hostContext->setEqualizerService(eqService);

    // --- Bluetooth manager ---
    auto* bluetoothManager = new oap::BluetoothManager(configService.get(), &app);
    hostContext->setBluetoothService(bluetoothManager);

    // --- EventBus ---
    auto eventBus = new oap::EventBus(&app);
    hostContext->setEventBus(eventBus);

    // --- ActionRegistry ---
    auto actionRegistry = new oap::ActionRegistry(&app);
    hostContext->setActionRegistry(actionRegistry);

    // --- NavbarController ---
    auto navbarController = new oap::NavbarController(&app);
    navbarController->setActionRegistry(actionRegistry);
    // Read edge and LHD config
    {
        auto edgeVar = yamlConfig->valueByPath("navbar.edge");
        if (edgeVar.isValid() && !edgeVar.toString().isEmpty())
            navbarController->setEdge(edgeVar.toString());
        auto lhdVar = yamlConfig->valueByPath("identity.left_hand_drive");
        if (lhdVar.isValid())
            navbarController->setLeftHandDrive(lhdVar.toBool());
    }

    // --- NotificationService ---
    auto notificationService = new oap::NotificationService(&app);
    hostContext->setNotificationService(notificationService);

    // --- Companion Listener ---
    oap::CompanionListenerService* companionListener = nullptr;
    QVariant companionEnabledVar = yamlConfig->valueByPath("companion.enabled");
    bool companionEnabled = companionEnabledVar.isValid() ? companionEnabledVar.toBool() : true;
    QVariant companionPortVar = yamlConfig->valueByPath("companion.port");
    int companionPort = companionPortVar.isValid() ? companionPortVar.toInt() : 9876;
    qCInfo(lcCore) << "Companion: enabled=" << companionEnabled << "port=" << companionPort;
    if (companionEnabled) {
        companionListener = new oap::CompanionListenerService(&app);
        companionListener->setWifiSsid(config->wifiSsid());
        companionListener->loadOrGenerateVehicleId();
        QFile secretFile(QDir::homePath() + "/.openauto/companion.key");
        if (secretFile.open(QIODevice::ReadOnly)) {
            QByteArray secret = secretFile.readAll().trimmed();
            companionListener->setSharedSecret(QString::fromUtf8(secret));
            qCInfo(lcCore) << "Companion: loaded secret from" << secretFile.fileName()
                     << "(" << secret.length() << "bytes)";
        } else {
            qCWarning(lcCore) << "Companion: no secret file at" << secretFile.fileName()
                        << "— pairing required";
        }
        if (companionListener->start(companionPort)) {
            qCInfo(lcCore) << "Companion: listening on port" << companionPort;
        } else {
            qCWarning(lcCore) << "Companion: FAILED to bind port" << companionPort;
        }
        hostContext->setCompanionListenerService(companionListener);
    } else {
        qCInfo(lcCore) << "Companion: disabled in config";
    }

    oap::PluginManager pluginManager(&app);

    // Register static (compiled-in) plugins
    auto aaPlugin = new oap::plugins::AndroidAutoPlugin(config, yamlConfig.get(), &app);
    aaPlugin->setDisplayInfo(displayInfo);
    pluginManager.registerStaticPlugin(aaPlugin);

    auto btAudioPlugin = new oap::plugins::BtAudioPlugin(&app);
    pluginManager.registerStaticPlugin(btAudioPlugin);

    auto phonePlugin = new oap::plugins::PhonePlugin(&app);
    pluginManager.registerStaticPlugin(phonePlugin);

    auto eqPlugin = new oap::plugins::EqualizerPlugin(&app);
    pluginManager.registerStaticPlugin(eqPlugin);

    // Discover dynamic plugins from user directory
    pluginManager.discoverPlugins(QDir::homePath() + "/.openauto/plugins");

    // Initialize BT before plugins so they see a ready BT service
    bluetoothManager->initialize();

    // Initialize all plugins (static + dynamic)
    pluginManager.initializeAll(hostContext.get());

    // Wire EvdevCoordBridge from AA plugin to NavbarController for touch zones
    if (auto* bridge = aaPlugin->coordBridge()) {
        navbarController->setCoordBridge(bridge);
    }

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
    if (companionListener && systemClient) {
        QObject::connect(systemClient, &oap::SystemServiceClient::connectedChanged, systemClient, [=]() {
            if (systemClient->isConnected()) {
                companionListener->syncProxyRoute();
            }
        });
    }

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

    // --- Navbar action handlers ---
    // Volume tap: show volume popup
    actionRegistry->registerAction("navbar.volume.tap", [navbarController](const QVariant&) {
        // Determine which control index is volume
        for (int i = 0; i < 3; ++i) {
            if (navbarController->controlRole(i) == "volume") {
                navbarController->showPopup(i);
                break;
            }
        }
    });
    // Volume short-hold: open audio/EQ settings
    actionRegistry->registerAction("navbar.volume.shortHold", [appController, pluginModel, navbarController](const QVariant&) {
        pluginModel->setActivePlugin(QString());
        appController->navigateTo(6);
        emit navbarController->settingsPageRequested(QStringLiteral("audio"));
    });
    // Volume long-hold: mute toggle
    {
        static int previousVolume = 80;
        actionRegistry->registerAction("navbar.volume.longHold", [audioService](const QVariant&) {
            if (audioService->masterVolume() > 0) {
                previousVolume = audioService->masterVolume();
                audioService->setMasterVolume(0);
            } else {
                audioService->setMasterVolume(previousVolume > 0 ? previousVolume : 80);
            }
        });
    }
    // Clock tap: go home
    actionRegistry->registerAction("navbar.clock.tap", [pluginModel, appController](const QVariant&) {
        pluginModel->setActivePlugin(QString());
        appController->navigateTo(0);
    });
    // Clock short-hold: open settings
    actionRegistry->registerAction("navbar.clock.shortHold", [appController](const QVariant&) {
        appController->navigateTo(6);
    });
    // Clock long-hold: show power menu
    actionRegistry->registerAction("navbar.clock.longHold", [navbarController](const QVariant&) {
        navbarController->showPopup(1);  // center control = clock
    });
    // Brightness tap: show brightness popup
    actionRegistry->registerAction("navbar.brightness.tap", [navbarController](const QVariant&) {
        for (int i = 0; i < 3; ++i) {
            if (navbarController->controlRole(i) == "brightness") {
                navbarController->showPopup(i);
                break;
            }
        }
    });
    // Brightness short-hold: open display settings
    actionRegistry->registerAction("navbar.brightness.shortHold", [appController, pluginModel, navbarController](const QVariant&) {
        pluginModel->setActivePlugin(QString());
        appController->navigateTo(6);
        emit navbarController->settingsPageRequested(QStringLiteral("display"));
    });
    // Brightness long-hold: toggle night mode
    actionRegistry->registerAction("navbar.brightness.longHold", [themeService](const QVariant&) {
        themeService->toggleMode();
    });

    engine.rootContext()->setContextProperty("NavbarController", navbarController);
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
    engine.rootContext()->setContextProperty("DisplayService", displayService);

    auto* outputDeviceModel = new oap::AudioDeviceModel(
        oap::AudioDeviceModel::Output, audioService->deviceRegistry(), audioService);
    auto* inputDeviceModel = new oap::AudioDeviceModel(
        oap::AudioDeviceModel::Input, audioService->deviceRegistry(), audioService);
    engine.rootContext()->setContextProperty("AudioOutputDeviceModel", outputDeviceModel);
    engine.rootContext()->setContextProperty("AudioInputDeviceModel", inputDeviceModel);

    auto* codecCapModel = new oap::CodecCapabilityModel(&app);
    engine.rootContext()->setContextProperty("CodecCapabilityModel", codecCapModel);

    engine.rootContext()->setContextProperty("EqualizerService", eqService);
    engine.rootContext()->setContextProperty("ConfigService", configService.get());
    engine.rootContext()->setContextProperty("DisplayInfo", displayInfo);

    if (companionListener)
        engine.rootContext()->setContextProperty("CompanionService", companionListener);

    engine.rootContext()->setContextProperty("SystemService", systemClient);
    engine.rootContext()->setContextProperty("BluetoothManager", bluetoothManager);
    engine.rootContext()->setContextProperty("PairedDevicesModel", bluetoothManager->pairedDevicesModel());

    // Geometry override for windowed resolution testing
    engine.rootContext()->setContextProperty("_geomW", geomW);
    engine.rootContext()->setContextProperty("_geomH", geomH);

    // Qt 6.5+ uses /qt/qml/ prefix, Qt 6.4 uses direct URI prefix
    QUrl url(QStringLiteral("qrc:/OpenAutoProdigy/main.qml"));
    if (QFile::exists(QStringLiteral(":/qt/qml/OpenAutoProdigy/main.qml")))
        url = QUrl(QStringLiteral("qrc:/qt/qml/OpenAutoProdigy/main.qml"));

    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    // Wire DisplayInfo to actual window dimensions
    {
        auto* rootWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (rootWindow) {
            auto updateSize = [displayInfo, rootWindow]() {
                int w = rootWindow->width(), h = rootWindow->height();
                if (w > 0 && h > 0)
                    displayInfo->setWindowSize(w, h);
            };
            QObject::connect(rootWindow, &QQuickWindow::widthChanged, displayInfo, updateSize);
            QObject::connect(rootWindow, &QQuickWindow::heightChanged, displayInfo, updateSize);
            updateSize();  // push initial real values
        }
    }

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

    // Connect 3-finger gesture to GestureOverlay
    QObject::connect(aaPlugin, &oap::plugins::AndroidAutoPlugin::gestureTriggered,
                     &app, [&engine, aaPlugin, displayInfo, audioService, displayService, themeService, actionRegistry]() {
        auto* root = engine.rootObjects().value(0);
        if (!root) return;
        auto* overlay = root->findChild<QObject*>("gestureOverlay");
        if (!overlay) return;

        QMetaObject::invokeMethod(overlay, "show");

        // Register full-screen evdev zone at priority 200 to intercept all touches
        // during overlay visibility (QML MouseAreas don't work during EVIOCGRAB)
        auto* bridge = aaPlugin->coordBridge();
        if (!bridge) return;

        int dw = displayInfo->windowWidth();
        int dh = displayInfo->windowHeight();

        bridge->updateZone(
            std::string("gesture-overlay"), 200,
            0.0f, 0.0f,
            static_cast<float>(dw), static_cast<float>(dh),
            [overlay, audioService, displayService, actionRegistry, displayInfo](int /*slot*/, float rawX, float rawY, oap::aa::TouchEvent event) {
                // Bridge evdev touches to overlay controls during EVIOCGRAB.
                // Convert evdev (0-4095) to pixel coordinates.
                int dw = displayInfo->windowWidth();
                int dh = displayInfo->windowHeight();
                if (dw <= 0 || dh <= 0) return;

                float px = (rawX / 4095.0f) * dw;
                float py = (rawY / 4095.0f) * dh;

                // Panel geometry: centered, ~70% width (capped at 500px), ~50% height
                float panelW = std::min(dw * 0.7f, 500.0f);
                float panelX = (dw - panelW) / 2.0f;
                float panelH = dh * 0.5f;
                float panelY = (dh - panelH) / 2.0f;

                // Outside panel -- still consumed (prevents AA forwarding) but no action
                if (px < panelX || px > panelX + panelW || py < panelY || py > panelY + panelH)
                    return;

                // Normalize position within panel (0-1)
                float relX = (px - panelX) / panelW;
                float relY = (py - panelY) / panelH;

                // Panel layout (approximate vertical regions):
                // 0.00-0.10: "Quick Controls" title
                // 0.10-0.40: Volume slider row
                // 0.40-0.70: Brightness slider row
                // 0.70-1.00: Action buttons row

                if (relY >= 0.10f && relY < 0.40f) {
                    // Volume slider region
                    if (event == oap::aa::TouchEvent::Down || event == oap::aa::TouchEvent::Move) {
                        float sliderRelX = std::clamp((relX - 0.12f) / 0.72f, 0.0f, 1.0f);
                        int volume = static_cast<int>(sliderRelX * 100.0f);
                        QMetaObject::invokeMethod(audioService, [audioService, volume]() {
                            audioService->setMasterVolume(volume);
                        }, Qt::QueuedConnection);
                    }
                } else if (relY >= 0.40f && relY < 0.70f) {
                    // Brightness slider region
                    if (event == oap::aa::TouchEvent::Down || event == oap::aa::TouchEvent::Move) {
                        float sliderRelX = std::clamp((relX - 0.12f) / 0.72f, 0.0f, 1.0f);
                        int brightness = 5 + static_cast<int>(sliderRelX * 95.0f);  // 5-100
                        QMetaObject::invokeMethod(displayService, [displayService, brightness]() {
                            displayService->setBrightness(brightness);
                        }, Qt::QueuedConnection);
                    }
                } else if (relY >= 0.70f && event == oap::aa::TouchEvent::Down) {
                    // Button region -- 3 buttons evenly spaced
                    if (relX < 0.33f) {
                        // Home button
                        QMetaObject::invokeMethod(qApp, [actionRegistry]() {
                            actionRegistry->dispatch("app.home");
                        }, Qt::QueuedConnection);
                        QMetaObject::invokeMethod(overlay, "dismiss", Qt::QueuedConnection);
                    } else if (relX < 0.66f) {
                        // Day/Night toggle
                        QMetaObject::invokeMethod(qApp, [actionRegistry]() {
                            actionRegistry->dispatch("theme.toggle");
                        }, Qt::QueuedConnection);
                    } else {
                        // Close button
                        QMetaObject::invokeMethod(overlay, "dismiss", Qt::QueuedConnection);
                    }
                }
            });

        // Deregister zone when overlay becomes invisible (covers all dismiss paths:
        // timer, close button, home button -- they all set visible=false via dismiss())
        // Per research Pitfall 2: wire to visible property change, not individual dismiss callers.
        static QMetaObject::Connection visConn;
        // Disconnect any previous connection to avoid stacking
        if (visConn) QObject::disconnect(visConn);
        auto* overlayItem = qobject_cast<QQuickItem*>(overlay);
        if (overlayItem) {
            visConn = QObject::connect(overlayItem, &QQuickItem::visibleChanged,
                overlayItem, [bridge, overlayItem]() {
                    if (!overlayItem->isVisible()) {
                        bridge->removeZone(std::string("gesture-overlay"));
                    }
                });
        }
    });

    // SIGUSR1 → disconnect AA session (ShutdownRequest + teardown, keep listening)
    static oap::plugins::AndroidAutoPlugin* g_aaPlugin = aaPlugin;
    signal(SIGUSR1, [](int) {
        QMetaObject::invokeMethod(g_aaPlugin, [](){ g_aaPlugin->stopAA(); },
                                   Qt::QueuedConnection);
    });

    // --- systemd integration (Type=notify + watchdog) ---
#ifdef HAS_SYSTEMD
    // Signal systemd: app is fully initialized
    sd_notify(0, "READY=1");
    qCInfo(lcCore) << "sd_notify: READY=1 sent";

    // Watchdog heartbeat — if running under systemd with WatchdogSec,
    // fire at half the interval. Falls back to 15s if not set.
    QTimer* watchdogTimer = new QTimer(&app);
    uint64_t watchdogUsec = 0;
    if (sd_watchdog_enabled(0, &watchdogUsec) > 0 && watchdogUsec > 0) {
        int intervalMs = static_cast<int>(watchdogUsec / 1000 / 2);
        watchdogTimer->setInterval(intervalMs);
    } else {
        watchdogTimer->setInterval(15000);
    }
    QObject::connect(watchdogTimer, &QTimer::timeout, []{
        sd_notify(0, "WATCHDOG=1");
    });
    watchdogTimer->start();
    qCInfo(lcCore) << "Watchdog heartbeat started (interval:" << watchdogTimer->interval() << "ms)";

    // Signal systemd on clean shutdown
    QObject::connect(&app, &QGuiApplication::aboutToQuit, []{
        sd_notify(0, "STOPPING=1");
        qCInfo(lcCore) << "sd_notify: STOPPING=1 sent";
    });
#endif

    int ret = app.exec();

    // Teardown order matters: deactivate plugin view (uses QML engine)
    // BEFORE engine is destroyed (stack-local), BEFORE plugin shutdown.
    pluginModel->setActivePlugin(QString());

    pluginManager.shutdownAll();
    bluetoothManager->shutdown();

    return ret;
}
