#include <signal.h>
#ifdef HAS_SYSTEMD
#include <systemd/sd-daemon.h>
#endif
#include <QGuiApplication>
#include <QScreen>
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
#include <QtQml/qqml.h>
#include "core/Logging.hpp"
#include "ui/SettingsInputBoundary.hpp"
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
#include "core/services/ProjectionStatusProvider.hpp"
#include "core/services/PhoneStateService.hpp"
#include "core/services/MediaStatusService.hpp"
#include "ui/NotificationModel.hpp"
#include "core/plugin/HostContext.hpp"
#include "core/plugin/PluginManager.hpp"
#include "plugins/android_auto/AndroidAutoPlugin.hpp"
#include "core/aa/AndroidAutoOrchestrator.hpp"
#include "core/aa/NavigationDataBridge.hpp"
#include "core/aa/ManeuverIconProvider.hpp"
#include <oaa/HU/Handlers/MediaStatusChannelHandler.hpp>
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
#include "ui/GestureOverlayController.hpp"
#include "core/widget/WidgetRegistry.hpp"
#include "core/widget/WidgetTypes.hpp"
#include "ui/WidgetPickerModel.hpp"
#include "ui/WidgetGridModel.hpp"
#include <QQuickWindow>
#include <QWindow>
#include <algorithm>
#include <cmath>

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

    // Load YAML config
    QString yamlPath = QDir::homePath() + "/.openauto/config.yaml";
    auto yamlConfig = std::make_shared<oap::YamlConfig>();
    if (QFile::exists(yamlPath)) {
        yamlConfig->load(yamlPath);
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

    // Apply screen size from config for DPI computation
    auto screenSizeVar = yamlConfig->valueByPath("display.screen_size");
    if (screenSizeVar.isValid() && screenSizeVar.toDouble() > 0)
        displayInfo->setScreenSizeInches(screenSizeVar.toDouble());

    // Wire DPI cascade and density bias to DisplayInfo
    displayInfo->setConfigScreenSizeOverride(displayInfo->screenSizeInches());
    displayInfo->setDensityBias(yamlConfig->gridDensityBias());

    qCInfo(lcCore) << "Screen size:" << displayInfo->screenSizeInches()
                   << "inches, DPI:" << displayInfo->computedDpi()
                   << "cellSide:" << displayInfo->cellSide()
                   << "densityBias:" << displayInfo->densityBias();

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

    // Apply force-dark-mode override (HU only — AA uses real night mode)
    QVariant forceDark = yamlConfig->valueByPath("display.force_dark_mode");
    if (forceDark.isValid())
        themeService->setForceDarkMode(forceDark.toBool());
    else
        themeService->setForceDarkMode(true); // default: on

    // Evaluate time-based night mode at startup so theme is correct before AA connects
    if (yamlConfig->nightModeSource() == "time") {
        QTime now = QTime::currentTime();
        QTime dayStart = QTime::fromString(yamlConfig->nightModeDayStart(), "HH:mm");
        QTime nightStart = QTime::fromString(yamlConfig->nightModeNightStart(), "HH:mm");
        if (!dayStart.isValid()) dayStart = QTime(7, 0);
        if (!nightStart.isValid()) nightStart = QTime(19, 0);
        bool night = (nightStart > dayStart)
            ? !(now >= dayStart && now < nightStart)
            : (now >= nightStart && now < dayStart);
        themeService->setNightMode(night);
    }

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

    // Live-toggle verbose logging from settings UI
    QObject::connect(configService.get(), &oap::ConfigService::configChanged,
        [](const QString& path, const QVariant& value) {
            if (path == "logging.verbose") {
                oap::setVerbose(value.toBool());
                qCInfo(lcCore) << "Verbose logging" << (value.toBool() ? "enabled" : "disabled") << "(via settings)";
            }
        });

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
    navbarController->setAudioService(audioService);
    navbarController->setDisplayService(displayService);
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
        companionListener->setWifiSsid(yamlConfig->wifiSsid());
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
        companionListener->setThemeService(themeService);

        // Set display size for companion wallpaper cropping
        if (geomW > 0 && geomH > 0) {
            companionListener->setDisplaySize(geomW, geomH);
        } else {
            auto* screen = QGuiApplication::primaryScreen();
            if (screen) {
                QRect geom = screen->geometry();
                companionListener->setDisplaySize(geom.width(), geom.height());
            }
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
    auto aaPlugin = new oap::plugins::AndroidAutoPlugin(yamlConfig.get(), &app);
    aaPlugin->setDisplayInfo(displayInfo);
    pluginManager.registerStaticPlugin(aaPlugin);

    auto btAudioPlugin = new oap::plugins::BtAudioPlugin(&app);
    pluginManager.registerStaticPlugin(btAudioPlugin);

    // --- Core phone state service (owns HFP D-Bus + call state machine) ---
    auto phoneStateService = new oap::PhoneStateService(&app);
    phoneStateService->setNotificationService(notificationService);
    phoneStateService->startDBusMonitoring();
    hostContext->setCallStateProvider(phoneStateService);

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

    // --- Data bridges for content widgets ---
    auto* navBridge = new oap::aa::NavigationDataBridge(&app);
    auto* maneuverIconProvider = new oap::aa::ManeuverIconProvider();

    // Wire nav bridge to orchestrator's navigation handler
    if (auto* orch = aaPlugin->orchestrator()) {
        navBridge->connectToHandler(orch->navigationHandler());
    }
    navBridge->setManeuverIconProvider(maneuverIconProvider);
    hostContext->setNavigationProvider(navBridge);

    // --- Core media status service (owns AA+BT source merging) ---
    auto mediaStatusService = new oap::MediaStatusService(&app);
    hostContext->setMediaStatusProvider(mediaStatusService);

    // Wire MediaStatusService to AA orchestrator
    if (auto* orch = aaPlugin->orchestrator()) {
        QObject::connect(orch, &oap::aa::AndroidAutoOrchestrator::connectionStateChanged,
                         mediaStatusService, [mediaStatusService, orch]() {
            mediaStatusService->setAaConnected(orch->isAaConnected());
        });
        auto* msHandler = orch->mediaStatusHandler();
        if (msHandler) {
            QObject::connect(msHandler, &oaa::hu::MediaStatusChannelHandler::metadataChanged,
                             mediaStatusService, [mediaStatusService](const QString& title, const QString& artist,
                                                                       const QString& album, const QByteArray&) {
                mediaStatusService->updateAaMetadata(title, artist, album);
            }, Qt::QueuedConnection);
            QObject::connect(msHandler, &oaa::hu::MediaStatusChannelHandler::playbackStateChanged,
                             mediaStatusService, [mediaStatusService](int state, const QString& app) {
                mediaStatusService->updateAaPlaybackState(state, app);
            }, Qt::QueuedConnection);
        }
    }

    // Wire MediaStatusService to BT audio plugin
    QObject::connect(btAudioPlugin, &oap::plugins::BtAudioPlugin::metadataChanged,
                     mediaStatusService, [mediaStatusService, btAudioPlugin]() {
        mediaStatusService->updateBtMetadata(btAudioPlugin->trackTitle(),
                                              btAudioPlugin->trackArtist(),
                                              btAudioPlugin->trackAlbum());
    });
    QObject::connect(btAudioPlugin, &oap::plugins::BtAudioPlugin::playbackStateChanged,
                     mediaStatusService, [mediaStatusService, btAudioPlugin]() {
        mediaStatusService->updateBtPlaybackState(btAudioPlugin->playbackState());
    });
    QObject::connect(btAudioPlugin, &oap::plugins::BtAudioPlugin::connectionStateChanged,
                     mediaStatusService, [mediaStatusService, btAudioPlugin]() {
        mediaStatusService->setBtConnected(btAudioPlugin->connectionState() == 1);
    });
    if (btAudioPlugin->connectionState() == 1) {
        mediaStatusService->setBtConnected(true);
        mediaStatusService->updateBtMetadata(btAudioPlugin->trackTitle(),
                                              btAudioPlugin->trackArtist(),
                                              btAudioPlugin->trackAlbum());
        mediaStatusService->updateBtPlaybackState(btAudioPlugin->playbackState());
    }

    // Playback control delegation
    {
        auto* orch = aaPlugin->orchestrator();
        mediaStatusService->setPlaybackCallbacks(
            [mediaStatusService, orch, btAudioPlugin]() {
                if (mediaStatusService->source() == "AndroidAuto" && orch)
                    orch->sendButtonPress(85);
                else if (mediaStatusService->source() == "Bluetooth" && btAudioPlugin) {
                    if (btAudioPlugin->playbackState() == 1) btAudioPlugin->pause();
                    else btAudioPlugin->play();
                }
            },
            [mediaStatusService, orch, btAudioPlugin]() {
                if (mediaStatusService->source() == "AndroidAuto" && orch)
                    orch->sendButtonPress(87);
                else if (mediaStatusService->source() == "Bluetooth" && btAudioPlugin)
                    btAudioPlugin->next();
            },
            [mediaStatusService, orch, btAudioPlugin]() {
                if (mediaStatusService->source() == "AndroidAuto" && orch)
                    orch->sendButtonPress(88);
                else if (mediaStatusService->source() == "Bluetooth" && btAudioPlugin)
                    btAudioPlugin->previous();
            }
        );
    }

    // --- Projection status provider (wraps orchestrator for narrow interface) ---
    oap::ProjectionStatusProvider* projectionStatusProvider = nullptr;
    if (auto* orch = aaPlugin->orchestrator()) {
        projectionStatusProvider = new oap::ProjectionStatusProvider(orch, &app);
        hostContext->setProjectionStatusProvider(projectionStatusProvider);
    }

    // --- Widget system ---
    auto widgetRegistry = new oap::WidgetRegistry(&app);

    // Register built-in standalone widgets
    {
        oap::WidgetDescriptor clockDesc;
        clockDesc.id = "org.openauto.clock";
        clockDesc.displayName = "Clock";
        clockDesc.iconName = "\ue8b5";  // schedule
        clockDesc.category = "status";
        clockDesc.description = "Current time";
        clockDesc.minCols = 1; clockDesc.minRows = 1;
        clockDesc.maxCols = 6; clockDesc.maxRows = 4;
        clockDesc.defaultCols = 2; clockDesc.defaultRows = 2;
        clockDesc.qmlComponent = QUrl("qrc:/OpenAutoProdigy/ClockWidget.qml");
        widgetRegistry->registerWidget(clockDesc);

        oap::WidgetDescriptor aaStatusDesc;
        aaStatusDesc.id = "org.openauto.aa-status";
        aaStatusDesc.displayName = "Android Auto";
        aaStatusDesc.iconName = "\ueff7";  // directions_car
        aaStatusDesc.category = "status";
        aaStatusDesc.description = "Connection status";
        aaStatusDesc.minCols = 1; aaStatusDesc.minRows = 1;
        aaStatusDesc.maxCols = 3; aaStatusDesc.maxRows = 2;
        aaStatusDesc.defaultCols = 2; aaStatusDesc.defaultRows = 1;
        aaStatusDesc.qmlComponent = QUrl("qrc:/OpenAutoProdigy/AAStatusWidget.qml");
        widgetRegistry->registerWidget(aaStatusDesc);
    }

    // Phase 06 widget stubs (pre-registered descriptors, no QML yet)
    {
        oap::WidgetDescriptor navDesc;
        navDesc.id = "org.openauto.nav-turn";
        navDesc.displayName = "Navigation";
        navDesc.iconName = "\ue55c";  // navigation
        navDesc.category = "navigation";
        navDesc.description = "Turn-by-turn directions";
        navDesc.minCols = 2; navDesc.minRows = 1;
        navDesc.maxCols = 4; navDesc.maxRows = 2;
        navDesc.defaultCols = 3; navDesc.defaultRows = 1;
        navDesc.qmlComponent = QUrl(QStringLiteral("qrc:/OpenAutoProdigy/NavigationWidget.qml"));
        widgetRegistry->registerWidget(navDesc);

        oap::WidgetDescriptor npDesc;
        npDesc.id = "org.openauto.now-playing";
        npDesc.displayName = "Now Playing";
        npDesc.iconName = "\ue030";  // music_note
        npDesc.category = "media";
        npDesc.description = "Track info & controls";
        npDesc.minCols = 2; npDesc.minRows = 1;
        npDesc.maxCols = 6; npDesc.maxRows = 2;
        npDesc.defaultCols = 3; npDesc.defaultRows = 2;
        npDesc.qmlComponent = QUrl(QStringLiteral("qrc:/OpenAutoProdigy/NowPlayingWidget.qml"));
        widgetRegistry->registerWidget(npDesc);
    }

    // Collect widget descriptors from plugins
    for (auto* plugin : pluginManager.plugins()) {
        for (const auto& desc : plugin->widgetDescriptors()) {
            widgetRegistry->registerWidget(desc);
        }
    }

    // --- Grid-based widget model (replaces pane-based WidgetPlacementModel) ---
    auto widgetGridModel = new oap::WidgetGridModel(widgetRegistry, &app);
    // Initial grid dimensions from cellSide (QML will take over once loaded)
    {
        qreal cs = displayInfo->cellSide();
        int initCols = qMax(3, static_cast<int>(std::floor(displayInfo->windowWidth() / cs)));
        int initRows = qMax(2, static_cast<int>(std::floor(displayInfo->windowHeight() / cs)));
        widgetGridModel->setGridDimensions(initCols, initRows);
    }

    // Load placements and page count from config BEFORE connecting auto-save.
    // setGridDimensions() emits placementsChanged() — connecting save first would
    // persist empty placements on startup, wiping the saved config.
    widgetGridModel->setPageCount(yamlConfig->gridPageCount());
    auto savedPlacements = yamlConfig->gridPlacements();
    if (!savedPlacements.isEmpty()) {
        widgetGridModel->setPlacements(savedPlacements, widgetRegistry);
        widgetGridModel->setNextInstanceId(yamlConfig->gridNextInstanceId());
    }
    widgetGridModel->setSavedDimensions(yamlConfig->gridSavedCols(), yamlConfig->gridSavedRows());

    // Auto-save grid placements on change (connected after load to avoid clobbering)
    auto saveGridState = [yamlConfig = yamlConfig.get(), widgetGridModel, yamlPath]() {
        yamlConfig->setGridPlacements(widgetGridModel->placements());
        yamlConfig->setGridNextInstanceId(widgetGridModel->nextInstanceId());
        yamlConfig->setGridPageCount(widgetGridModel->pageCount());
        yamlConfig->setGridSavedDims(widgetGridModel->gridColumns(), widgetGridModel->gridRows());
        yamlConfig->save(yamlPath);
    };
    QObject::connect(widgetGridModel, &oap::WidgetGridModel::placementsChanged,
                     widgetGridModel, saveGridState);
    QObject::connect(widgetGridModel, &oap::WidgetGridModel::pageCountChanged,
                     widgetGridModel, saveGridState);

    // Re-clamp grid when cellSide changes (QML drives the actual dims via
    // WidgetGridModel.setGridDimensions, but we update here as fallback for
    // non-QML paths like the web config panel)
    QObject::connect(displayInfo, &oap::DisplayInfo::cellSideChanged,
                     widgetGridModel, [widgetGridModel, displayInfo]() {
        qreal cs = displayInfo->cellSide();
        int cols = qMax(3, static_cast<int>(std::floor(displayInfo->windowWidth() / cs)));
        int rows = qMax(2, static_cast<int>(std::floor(displayInfo->windowHeight() / cs)));
        widgetGridModel->setGridDimensions(cols, rows);
    });

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

    qmlRegisterType<oap::SettingsInputBoundary>("OpenAutoProdigy", 1, 0, "SettingsInputBoundary");

    QQmlApplicationEngine engine;

    // Plugin model for QML nav strip (needs engine for PluginRuntimeContext)
    auto pluginModel = new oap::PluginModel(&pluginManager, &engine, &app);

    auto launcherModel = new oap::LauncherModel(yamlConfig.get(), &app);
    auto notificationModel = new oap::NotificationModel(notificationService, &app);

    // Register built-in actions (after pluginModel exists)
    actionRegistry->registerAction("app.quit", [](const QVariant&) {
        QGuiApplication::quit();
    });
    actionRegistry->registerAction("app.minimize", [appController](const QVariant&) {
        appController->minimize();
    });
    actionRegistry->registerAction("app.restart", [appController](const QVariant&) {
        appController->restart();
    });
    actionRegistry->registerAction("app.home", [pluginModel](const QVariant&) {
        pluginModel->setActivePlugin(QString());
    });
    actionRegistry->registerAction("theme.toggle", [themeService](const QVariant&) {
        themeService->toggleMode();
    });
    // AA button press action (used by DebugSettings via ActionRegistry.dispatch)
    if (auto* orch = aaPlugin->orchestrator()) {
        actionRegistry->registerAction("aa.sendButton", [orch](const QVariant& v) {
            orch->sendButtonPress(v.toInt());
        });
    }

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

    // Expose call state provider for IncomingCallOverlay in Shell.qml
    engine.rootContext()->setContextProperty("CallStateProvider", static_cast<QObject*>(phoneStateService));

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

    engine.rootContext()->setContextProperty("WidgetGridModel", widgetGridModel);
    engine.rootContext()->setContextProperty("WidgetRegistry", widgetRegistry);

    auto widgetPickerModel = new oap::WidgetPickerModel(widgetRegistry, &app);
    engine.rootContext()->setContextProperty("WidgetPickerModel", widgetPickerModel);
    engine.rootContext()->setContextProperty("SystemService", systemClient);
    engine.rootContext()->setContextProperty("BluetoothManager", bluetoothManager);
    engine.rootContext()->setContextProperty("PairedDevicesModel", bluetoothManager->pairedDevicesModel());
    // Projection status provider for widgets and debug settings
    engine.rootContext()->setContextProperty("ProjectionStatus", static_cast<QObject*>(projectionStatusProvider));

    // Provider-backed root-context properties for widgets
    engine.rootContext()->setContextProperty("NavigationProvider", static_cast<QObject*>(navBridge));
    engine.rootContext()->setContextProperty("MediaStatus", static_cast<QObject*>(mediaStatusService));

    // Navigation icon image provider
    engine.addImageProvider(QStringLiteral("navicon"), maneuverIconProvider);

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

    // Wire DisplayInfo to actual window dimensions + QScreen DPI + fullscreen state
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

            // Wire QScreen DPI at startup
            auto* screen = rootWindow->screen();
            if (screen) {
                displayInfo->setQScreenDpi(screen->physicalDotsPerInch());
                // Track DPI changes on current screen
                QObject::connect(screen, &QScreen::physicalDotsPerInchChanged,
                                 displayInfo, &oap::DisplayInfo::setQScreenDpi);
            }

            // Wire fullscreen state
            displayInfo->setFullscreen(rootWindow->visibility() == QWindow::FullScreen);
            QObject::connect(rootWindow, &QWindow::visibilityChanged,
                             displayInfo, [displayInfo](QWindow::Visibility v) {
                displayInfo->setFullscreen(v == QWindow::FullScreen);
            });

            // Handle window moving to a different monitor — reconnect DPI signal
            QObject::connect(rootWindow, &QWindow::screenChanged,
                             displayInfo, [displayInfo](QScreen* newScreen) {
                if (newScreen) {
                    displayInfo->setQScreenDpi(newScreen->physicalDotsPerInch());
                    // Note: old screen's signal auto-disconnects when screen is destroyed.
                    // Connect to new screen's DPI change signal.
                    QObject::connect(newScreen, &QScreen::physicalDotsPerInchChanged,
                                     displayInfo, &oap::DisplayInfo::setQScreenDpi);
                }
            });
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

    // --- Gesture overlay controller ---
    auto* gestureController = new oap::GestureOverlayController(&app);
    if (auto* bridge = aaPlugin->coordBridge())
        gestureController->setCoordBridge(bridge);
    gestureController->setAudioService(audioService);
    gestureController->setDisplayService(displayService);
    gestureController->setActionRegistry(actionRegistry);

    // Connect 3-finger gesture to GestureOverlay via controller
    QObject::connect(aaPlugin, &oap::plugins::AndroidAutoPlugin::gestureTriggered,
                     &app, [&engine, gestureController, displayInfo]() {
        auto* root = engine.rootObjects().value(0);
        if (!root) return;
        auto* overlay = root->findChild<QObject*>("gestureOverlay");
        if (!overlay) return;
        auto* overlayItem = qobject_cast<QQuickItem*>(overlay);
        if (!overlayItem) return;

        gestureController->showOverlay(overlayItem,
                                       displayInfo->windowWidth(),
                                       displayInfo->windowHeight());
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
