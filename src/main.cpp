#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#ifdef HAS_SYSTEMD
#include <systemd/sd-daemon.h>
#endif
#ifdef HAS_WEBENGINE
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#include <QWebEngineUrlScheme>
#include <QtWebEngineQuick/QQuickWebEngineProfile>
#include "core/webwidget/WebWidgetContentResolver.hpp"
#include "core/webwidget/WebWidgetSchemeHandler.hpp"
#include "core/widget/WebWidgetScanner.hpp"
#include "core/WidevineCdm.hpp"
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
#include "core/system/HostapdConfig.hpp"
#include "ui/SettingsInputBoundary.hpp"
#include "core/YamlConfig.hpp"
#include "core/services/ConfigService.hpp"
#include "core/services/ThemeService.hpp"
#include "core/services/DisplayService.hpp"
#include "core/services/AudioService.hpp"
#include "core/services/IpcServer.hpp"
#include "core/services/EventBus.hpp"
#include "core/services/ActionRegistry.hpp"
#include "core/services/OverlayService.hpp"
#include "core/services/NotificationService.hpp"
#include "core/services/CompanionListenerService.hpp"
#include "core/services/WeatherService.hpp"
#include "core/services/SystemServiceClient.hpp"
#include "core/services/BluetoothManager.hpp"
#include "core/services/EqualizerService.hpp"
#include "core/services/ProjectionStatusProvider.hpp"
#include "core/services/PhoneStateService.hpp"
#include "core/services/TelephonyClient.hpp"
#include "core/services/CallAudioPolicy.hpp"
#include "core/audio/ScoNodeMonitor.hpp"
#include "core/services/MediaStatusService.hpp"
#include "core/api/ApiServer.hpp"
#include "ui/NotificationModel.hpp"
#include "core/plugin/HostContext.hpp"
#include "core/plugin/PluginManager.hpp"
#include "plugins/android_auto/AndroidAutoPlugin.hpp"
#include "core/aa/AndroidAutoOrchestrator.hpp"
#include "core/aa/NavigationDataBridge.hpp"
#include "core/aa/ManeuverIconProvider.hpp"
#include <oaa/HU/Handlers/MediaStatusChannelHandler.hpp>
#include "plugins/bt_audio/BtAudioPlugin.hpp"
#include "plugins/media_player/MediaPlayerPlugin.hpp"
#include "plugins/media_player/MediaArtProvider.hpp"
#include "plugins/phone/PhonePlugin.hpp"
#include "plugins/equalizer/EqualizerPlugin.hpp"
#include "ui/ApplicationController.hpp"
#include "ui/NavbarController.hpp"
#include "core/aa/EvdevCoordBridge.hpp"
#include "ui/PluginModel.hpp"
#include "ui/PluginViewHost.hpp"

#include "ui/AudioDeviceModel.hpp"
#include "ui/CodecCapabilityModel.hpp"
#include "ui/DisplayInfo.hpp"
#include "ui/GestureOverlayController.hpp"
#include "core/widget/WidgetRegistry.hpp"
#include "core/widget/WidgetTypes.hpp"
#include "ui/WidgetPickerModel.hpp"
#include "ui/WidgetGridModel.hpp"
#include "ui/WidgetContextFactory.hpp"
#include "ui/DashboardManager.hpp"
#include <QQuickWindow>
#include <QWindow>
#include <QSocketNotifier>
#include <QDateTime>
#include <QProcess>
#include <QTimeZone>
#include <algorithm>
#include <cmath>

// Mirrors CompanionListenerService::adjustClock (src/core/services/
// CompanionListenerService.cpp) so RTC-less clock stepping from the phone's
// wall-clock report survives the legacy companion service's retirement: same
// 30s trigger threshold, same 5-minute-backward guard requiring 3 consecutive
// agreeing reports before stepping backward, same timedatectl invocation. Safe
// to run alongside the legacy path — both are keyed off the same phone clock
// and the threshold prevents thrashing if both ever fire.
static void adjustClockFromApiTimeReport(qint64 phoneTimeMs)
{
    static int backwardJumpCount = 0;
    static qint64 lastBackwardTarget = 0;

    qint64 piTimeMs = QDateTime::currentMSecsSinceEpoch();
    qint64 deltaMs = phoneTimeMs - piTimeMs;

    // Only adjust if delta > 30 seconds
    if (qAbs(deltaMs) < 30000) return;

    // Backward jump protection: reject >5min backward unless 3 consecutive agree
    if (deltaMs < -300000) {
        if (phoneTimeMs == lastBackwardTarget) {
            backwardJumpCount++;
        } else {
            backwardJumpCount = 1;
            lastBackwardTarget = phoneTimeMs;
        }
        if (backwardJumpCount < 3) return;  // Need 3 agreements
    }
    backwardJumpCount = 0;
    lastBackwardTarget = 0;

    // Set via timedatectl (polkit-authorized)
    // Qt 6.4: Qt::UTC, Qt 6.5+: QTimeZone::UTC (suppress deprecation on 6.8)
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QDateTime newTime = QDateTime::fromMSecsSinceEpoch(phoneTimeMs, Qt::UTC);
    QT_WARNING_POP
    QString timeStr = newTime.toString("yyyy-MM-dd hh:mm:ss");

    QProcess proc;
    proc.start("timedatectl", {"set-time", timeStr});
    proc.waitForFinished(5000);

    if (proc.exitCode() == 0) {
        qCInfo(lcCore) << "API: clock adjusted by" << deltaMs << "ms"
                << "(" << piTimeMs << "->" << phoneTimeMs << ")";
    } else {
        qCWarning(lcCore) << "API: timedatectl failed:" << proc.readAllStandardError();
    }
}

// Companion TimeReport.timezone_id (v1.1, ApiInboundState::timezoneReported).
// Reports can arrive ~continuously (once shortly after connect at minimum,
// but nothing stops a client from sending more) -- skip the timedatectl call
// entirely when the reported zone already matches the system zone, so a
// steady stream of reports doesn't spam the polkit-authorized call.
static void adjustTimezoneFromApiTimeReport(const QString& ianaId)
{
    if (ianaId.toUtf8() == QTimeZone::systemTimeZoneId())
        return;

    QProcess proc;
    proc.start("timedatectl", {"set-timezone", ianaId});
    proc.waitForFinished(5000);

    if (proc.exitCode() == 0) {
        qCInfo(lcCore) << "API: timezone adjusted to" << ianaId;
    } else {
        qCWarning(lcCore) << "API: timedatectl set-timezone failed:"
                          << proc.readAllStandardError();
    }
}

// Self-pipe for async-signal-safe Unix signal handling. POSIX signal handlers
// may only touch this pipe; a QSocketNotifier on the main thread does the real
// (Qt) work. See Qt docs "Calling Qt Functions From Unix Signal Handlers".
static int g_signalFds[2] = {-1, -1};

int main(int argc, char *argv[])
{
#ifdef HAS_WEBENGINE
    // Chromium requires custom schemes registered before the app object
    // exists (design §3/§9); initialize() must also precede QGuiApplication.
    //
    // Scheme is Secure (trustworthy origin so ws://127.0.0.1 connects) but
    // deliberately NOT LocalAccessAllowed — widget pages must not load
    // file:/qrc: subresources; all content flows through the prodigy://
    // resolver jail (design §7; erratum vs design §3's flag list,
    // final-review 2026-07-07).
    {
        QWebEngineUrlScheme scheme("prodigy");
        scheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
        scheme.setFlags(QWebEngineUrlScheme::SecureScheme);
        QWebEngineUrlScheme::registerScheme(scheme);
    }
    // Widevine CDM auto-wiring (spec 2026-07-07-web-surface-strategy §Slice 1):
    // point Chromium at the system CDM so DRM (EME) content can play. Must
    // happen before initialize(); an operator-supplied widevine-path in
    // QTWEBENGINE_CHROMIUM_FLAGS wins.
    {
        const QString cdm = oap::resolveWidevineCdmPath(oap::widevineCdmCandidates());
        const QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
        const QByteArray updated = oap::appendWidevineFlag(flags, cdm);
        if (updated != flags) {
            qputenv("QTWEBENGINE_CHROMIUM_FLAGS", updated);
            qCInfo(lcCore) << "Widevine CDM wired:" << cdm;
        } else if (cdm.isEmpty()) {
            qCInfo(lcCore) << "No Widevine CDM found — DRM content unavailable";
        } else {
            qCInfo(lcCore) << "Widevine flags preset by environment — leaving untouched";
        }
    }
    QtWebEngineQuick::initialize();
#endif
    QGuiApplication app(argc, argv);
    app.setApplicationName("OpenAuto Prodigy");
    app.setApplicationVersion(QStringLiteral(OAP_VERSION));
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

    // Sync WiFi credentials from hostapd.conf (single source of truth)
    {
        auto credentials = oap::loadHostapdWifiCredentials(QStringLiteral("/etc/hostapd/hostapd.conf"));
        if (credentials.has_value() && oap::syncWifiCredentials(*yamlConfig, *credentials)) {
            qCInfo(lcCore) << "WiFi credentials synced from hostapd for SSID:"
                           << credentials->ssid;
            yamlConfig->save(yamlPath);
        }
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

    // --- Config service (moved before theme loading for persistence wiring) ---
    auto configService = std::make_unique<oap::ConfigService>(yamlConfig.get(), yamlPath);

    // Live-toggle verbose logging from settings UI
    QObject::connect(configService.get(), &oap::ConfigService::configChanged,
        [](const QString& path, const QVariant& value) {
            if (path == "logging.verbose") {
                oap::setVerbose(value.toBool());
                qCInfo(lcCore) << "Verbose logging" << (value.toBool() ? "enabled" : "disabled") << "(via settings)";
            }
        });

    themeService->setConfigService(configService.get());

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

    // --- OverlayService ---
    auto overlayService = new oap::OverlayService(actionRegistry, &app);

    // --- Pairing dialog: migrated onto the overlay framework ---
    {
        oap::OverlayService::OverlayDescriptor d;
        d.id = QStringLiteral("pairing");
        d.qmlComponent = QUrl(QStringLiteral("qrc:/OpenAutoProdigy/PairingDialog.qml"));
        d.band = oap::OverlayService::ZBand::SystemModal;
        overlayService->registerOverlay(d);
        // Visibility rides the action path (rail R4): state source stays authoritative.
        auto syncPairing = [actionRegistry, bluetoothManager]() {
            actionRegistry->dispatch(bluetoothManager->isPairingActive()
                ? QStringLiteral("overlay.pairing.show")
                : QStringLiteral("overlay.pairing.hide"));
        };
        QObject::connect(bluetoothManager, &oap::BluetoothManager::pairingActiveChanged,
                         overlayService, syncPairing);
        syncPairing();

        // State authority (design §4.4): if anything shows the pairing overlay
        // while BluetoothManager says pairing is inactive, immediately hide it
        // through the same action path. Safe reentrancy: setVisible mutates
        // before emitting, so the nested hide sees visible==false and stops.
        QObject::connect(overlayService, &oap::OverlayService::overlayVisibilityChanged,
                         bluetoothManager,
                         [actionRegistry, bluetoothManager](const QString& id, bool visible) {
                             if (id == QLatin1String("pairing") && visible && !bluetoothManager->isPairingActive())
                                 actionRegistry->dispatch(QStringLiteral("overlay.pairing.hide"));
                         });
    }

    hostContext->setOverlayService(overlayService);

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

    auto mediaPlayerPlugin = new oap::plugins::MediaPlayerPlugin(&app);
    auto* mediaArtProvider = new oap::plugins::MediaArtProvider();
    mediaPlayerPlugin->setArtProvider(mediaArtProvider);  // non-owning; engine owns it (see addImageProvider below)
    pluginManager.registerStaticPlugin(mediaPlayerPlugin);

    // --- Core phone state service (owns HFP D-Bus + call state machine) ---
    auto phoneStateService = new oap::PhoneStateService(&app);
    phoneStateService->setNotificationService(notificationService);
    phoneStateService->setSettleGraceMs(
        yamlConfig->valueByPath("phone.settle_grace_ms").toInt());
    phoneStateService->startDBusMonitoring();
    hostContext->setCallStateProvider(phoneStateService);

    // PipeWire telephony (org.pipewire.Telephony, session bus): real HFP
    // call control. See docs/archive/plans/2026-07-05-hfp-call-audio-design.md
    auto telephonyClient = new oap::TelephonyClient(&app);
    auto scoMonitor = new oap::ScoNodeMonitor(&app);
    if (audioService->isAvailable())
        scoMonitor->start(audioService->pwThreadLoop(), audioService->pwCore());
    phoneStateService->attachTelephony(telephonyClient);
    phoneStateService->attachScoMonitor(scoMonitor);
    telephonyClient->start();

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
        const bool connected = btAudioPlugin->connectionState() == 1;
        mediaStatusService->setBtConnected(connected);
        // setBtConnected(true) clears cached metadata (fresh-session semantics).
        // If AVRCP (MediaPlayer1) connected before A2DP (drives connectionState),
        // metadata already arrived and was just cleared — re-publish it now so
        // now-playing doesn't stay blank until the next AVRCP event (which can be
        // a whole track away). Mirrors the startup seed block below.
        if (connected) {
            mediaStatusService->updateBtMetadata(btAudioPlugin->trackTitle(),
                                                  btAudioPlugin->trackArtist(),
                                                  btAudioPlugin->trackAlbum());
            mediaStatusService->updateBtPlaybackState(btAudioPlugin->playbackState());
        }
    });
    if (btAudioPlugin->connectionState() == 1) {
        mediaStatusService->setBtConnected(true);
        mediaStatusService->updateBtMetadata(btAudioPlugin->trackTitle(),
                                              btAudioPlugin->trackArtist(),
                                              btAudioPlugin->trackAlbum());
        mediaStatusService->updateBtPlaybackState(btAudioPlugin->playbackState());
    }

    // Wire MediaStatusService to the local media player plugin
    QObject::connect(mediaPlayerPlugin, &oap::plugins::MediaPlayerPlugin::metadataChanged,
                     mediaStatusService, [mediaStatusService, mediaPlayerPlugin]() {
        mediaStatusService->updateMediaPlayerMetadata(mediaPlayerPlugin->trackTitle(),
                                                      mediaPlayerPlugin->trackArtist(),
                                                      mediaPlayerPlugin->trackAlbum());
        mediaStatusService->updateMediaPlayerArt(mediaPlayerPlugin->artUrl());
    });
    QObject::connect(mediaPlayerPlugin, &oap::plugins::MediaPlayerPlugin::playbackStateChanged,
                     mediaStatusService, [mediaStatusService, mediaPlayerPlugin]() {
        mediaStatusService->updateMediaPlayerPlaybackState(mediaPlayerPlugin->playbackState());
    });
    QObject::connect(mediaPlayerPlugin, &oap::plugins::MediaPlayerPlugin::progressChanged,
                     mediaStatusService, [mediaStatusService](qint64 pos, qint64 dur) {
        mediaStatusService->updateMediaPlayerProgress(pos, dur);
    });
    QObject::connect(mediaPlayerPlugin, &oap::plugins::MediaPlayerPlugin::hasTrackChanged,
                     mediaStatusService, [mediaStatusService, mediaPlayerPlugin]() {
        mediaStatusService->setMediaPlayerConnected(mediaPlayerPlugin->hasTrack());
    });
    if (mediaPlayerPlugin->hasTrack()) {   // queue restored at initialize()
        mediaStatusService->setMediaPlayerConnected(true);
        mediaStatusService->updateMediaPlayerMetadata(mediaPlayerPlugin->trackTitle(),
                                                      mediaPlayerPlugin->trackArtist(),
                                                      mediaPlayerPlugin->trackAlbum());
        mediaStatusService->updateMediaPlayerPlaybackState(mediaPlayerPlugin->playbackState());
    }

    // BT progress into the widened surface (cheap win — BtAudioPlugin already
    // tracks position/duration from AVRCP)
    QObject::connect(btAudioPlugin, &oap::plugins::BtAudioPlugin::positionChanged,
                     mediaStatusService, [mediaStatusService, btAudioPlugin]() {
        mediaStatusService->updateBtProgress(btAudioPlugin->trackPosition(),
                                             btAudioPlugin->trackDuration());
    });

    // Playback control delegation
    {
        auto* orch = aaPlugin->orchestrator();
        mediaStatusService->setPlaybackCallbacks(
            [mediaStatusService, orch, btAudioPlugin, mediaPlayerPlugin]() {
                if (mediaStatusService->source() == "AndroidAuto" && orch)
                    orch->sendButtonPress(85);
                else if (mediaStatusService->source() == "Bluetooth" && btAudioPlugin) {
                    if (btAudioPlugin->playbackState() == 1) btAudioPlugin->pause();
                    else btAudioPlugin->play();
                }
                else if (mediaStatusService->source() == "MediaPlayer" && mediaPlayerPlugin)
                    mediaPlayerPlugin->playPause();
            },
            [mediaStatusService, orch, btAudioPlugin, mediaPlayerPlugin]() {
                if (mediaStatusService->source() == "AndroidAuto" && orch)
                    orch->sendButtonPress(87);
                else if (mediaStatusService->source() == "Bluetooth" && btAudioPlugin)
                    btAudioPlugin->next();
                else if (mediaStatusService->source() == "MediaPlayer" && mediaPlayerPlugin)
                    mediaPlayerPlugin->next();
            },
            [mediaStatusService, orch, btAudioPlugin, mediaPlayerPlugin]() {
                if (mediaStatusService->source() == "AndroidAuto" && orch)
                    orch->sendButtonPress(88);
                else if (mediaStatusService->source() == "Bluetooth" && btAudioPlugin)
                    btAudioPlugin->previous();
                else if (mediaStatusService->source() == "MediaPlayer" && mediaPlayerPlugin)
                    mediaPlayerPlugin->previous();
            }
        );
    }

    // One audible music source at a time (spec §6): starting one pauses the
    // others. AA↔local is bidirectional: an AA not-playing→playing edge
    // pauses local, and a local play-start sends KEYCODE_MEDIA_PAUSE so the
    // phone's MediaSession actually pauses (bench 2026-07-08 row 13). The AA
    // hook must be edge-triggered — the phone re-reports "playing" for a
    // moment after we send pause, and a level-triggered hook would re-pause
    // local right back.
    QObject::connect(mediaPlayerPlugin, &oap::plugins::MediaPlayerPlugin::playbackStarted,
                     btAudioPlugin, [btAudioPlugin]() {
        if (btAudioPlugin->playbackState() == 1) btAudioPlugin->pause();
    });
    QObject::connect(btAudioPlugin, &oap::plugins::BtAudioPlugin::playbackStateChanged,
                     mediaPlayerPlugin, [mediaPlayerPlugin, btAudioPlugin]() {
        if (btAudioPlugin->playbackState() == 1) mediaPlayerPlugin->pauseIfPlaying();
    });
    if (auto* orchForPolicy = aaPlugin->orchestrator()) {
        if (auto* mshForPolicy = orchForPolicy->mediaStatusHandler()) {
            auto aaPlaybackState = std::make_shared<int>(0);  // AA raw state: 2 = playing
            QObject::connect(mshForPolicy, &oaa::hu::MediaStatusChannelHandler::playbackStateChanged,
                             mediaPlayerPlugin, [mediaPlayerPlugin, aaPlaybackState](int state, const QString&) {
                const bool becamePlaying = (state == 2 && *aaPlaybackState != 2);
                *aaPlaybackState = state;
                if (becamePlaying) mediaPlayerPlugin->pauseIfPlaying();
            }, Qt::QueuedConnection);
            QObject::connect(mediaPlayerPlugin, &oap::plugins::MediaPlayerPlugin::playbackStarted,
                             orchForPolicy, [orchForPolicy, aaPlaybackState]() {
                if (*aaPlaybackState == 2 && orchForPolicy->isAaConnected())
                    orchForPolicy->sendButtonPress(127);  // KEYCODE_MEDIA_PAUSE
            });
            // The status channel emits nothing on close — without this reset a
            // disconnect-while-playing leaves the flag stuck at 2, so the next
            // reconnect's first "playing" report is not an edge (review 2026-07-09).
            // AA teardown lands on WaitingForDevice, not Disconnected (which
            // only comes from stop()/listen failure), so reset on any
            // non-projecting state — anything that is neither Connected nor
            // Backgrounded means AA is no longer echoing edges (gate re-run
            // 2026-07-09).
            QObject::connect(orchForPolicy, &oap::aa::AndroidAutoOrchestrator::connectionStateChanged,
                             mediaPlayerPlugin, [orchForPolicy, aaPlaybackState]() {
                const auto state = orchForPolicy->connectionState();
                if (state != oap::aa::AndroidAutoOrchestrator::Connected
                    && state != oap::aa::AndroidAutoOrchestrator::Backgrounded)
                    *aaPlaybackState = 0;
            });
        }
    }

    // --- Projection status provider (wraps orchestrator for narrow interface) ---
    oap::ProjectionStatusProvider* projectionStatusProvider = nullptr;
    if (auto* orch = aaPlugin->orchestrator()) {
        projectionStatusProvider = new oap::ProjectionStatusProvider(orch, &app);
        hostContext->setProjectionStatusProvider(projectionStatusProvider);
    }

    // AA-coexistence policy for call audio (design §6). With no projection
    // provider the policy is inert and SCO is always accepted.
    auto callAudioPolicy = new oap::CallAudioPolicy(
        projectionStatusProvider,
        yamlConfig->valueByPath("phone.reject_sco_during_aa").toBool(), &app);
    QObject::connect(callAudioPolicy, &oap::CallAudioPolicy::rejectScoWanted,
                     telephonyClient, &oap::TelephonyClient::setRejectSco);
    telephonyClient->setRejectSco(callAudioPolicy->wantReject());

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
        clockDesc.defaultConfig = {{"format", "24h"}, {"style", "digital"}};
        clockDesc.configSchema = {
            oap::ConfigSchemaField{
                "style", "Clock Style", oap::ConfigFieldType::Enum,
                {"Digital", "Analog"}, {"digital", "analog"},
                0, 0, 0
            },
            oap::ConfigSchemaField{
                "format", "Time Format", oap::ConfigFieldType::Enum,
                {"12-hour", "24-hour"}, {"12h", "24h"},
                0, 0, 0
            }
        };
        widgetRegistry->registerWidget(clockDesc);

        oap::WidgetDescriptor dateDesc;
        dateDesc.id = "org.openauto.date";
        dateDesc.displayName = "Date";
        dateDesc.iconName = "\ue916";  // calendar_today
        dateDesc.category = "status";
        dateDesc.description = "Day and date display";
        dateDesc.minCols = 1; dateDesc.minRows = 1;
        dateDesc.maxCols = 6; dateDesc.maxRows = 4;
        dateDesc.defaultCols = 2; dateDesc.defaultRows = 1;
        dateDesc.qmlComponent = QUrl("qrc:/OpenAutoProdigy/DateWidget.qml");
        dateDesc.defaultConfig = {{"dateOrder", "us"}};
        dateDesc.configSchema = {
            oap::ConfigSchemaField{
                "dateOrder", "Date Order", oap::ConfigFieldType::Enum,
                {"US (March 20)", "International (20 March)"}, {"us", "intl"},
                0, 0, 0
            }
        };
        widgetRegistry->registerWidget(dateDesc);

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

        // Media Player launcher tile — picker-visible (NOT singleton-seeded):
        // launcher widgets are the only way to open an app in this shell.
        oap::WidgetDescriptor mpLaunchDesc;
        mpLaunchDesc.id = "org.openauto.media-player-launcher";
        mpLaunchDesc.displayName = "Media Player";
        mpLaunchDesc.iconName = "\ue030";  // library_music
        mpLaunchDesc.category = "launcher";
        mpLaunchDesc.description = "Open the local media player";
        mpLaunchDesc.minCols = 1; mpLaunchDesc.minRows = 1;
        mpLaunchDesc.maxCols = 3; mpLaunchDesc.maxRows = 3;
        mpLaunchDesc.defaultCols = 1; mpLaunchDesc.defaultRows = 1;
        mpLaunchDesc.qmlComponent = QUrl(QStringLiteral("qrc:/OpenAutoProdigy/MediaPlayerLauncherWidget.qml"));
        widgetRegistry->registerWidget(mpLaunchDesc);
    }

    // Singleton launcher widgets (system-seeded, non-removable, hidden from picker)
    {
        oap::WidgetDescriptor slDesc;
        slDesc.id = "org.openauto.settings-launcher";
        slDesc.displayName = "Settings";
        slDesc.iconName = "\ue8b8";  // settings gear
        slDesc.category = "launcher";
        slDesc.description = "Open settings";
        slDesc.singleton = true;
        slDesc.minCols = 1; slDesc.minRows = 1;
        slDesc.maxCols = 3; slDesc.maxRows = 3;
        slDesc.defaultCols = 1; slDesc.defaultRows = 1;
        slDesc.qmlComponent = QUrl(QStringLiteral("qrc:/OpenAutoProdigy/SettingsLauncherWidget.qml"));
        widgetRegistry->registerWidget(slDesc);

        oap::WidgetDescriptor aaDesc;
        aaDesc.id = "org.openauto.aa-launcher";
        aaDesc.displayName = "Android Auto";
        aaDesc.iconName = "\ueff7";  // directions_car
        aaDesc.category = "launcher";
        aaDesc.description = "Launch Android Auto";
        aaDesc.singleton = true;
        aaDesc.minCols = 1; aaDesc.minRows = 1;
        aaDesc.maxCols = 3; aaDesc.maxRows = 3;
        aaDesc.defaultCols = 1; aaDesc.defaultRows = 1;
        aaDesc.qmlComponent = QUrl(QStringLiteral("qrc:/OpenAutoProdigy/AALauncherWidget.qml"));
        widgetRegistry->registerWidget(aaDesc);

        // --- Phase 20: Simple Widgets ---
        oap::WidgetDescriptor themeCycleDesc;
        themeCycleDesc.id = "org.openauto.theme-cycle";
        themeCycleDesc.displayName = "Theme Cycle";
        themeCycleDesc.iconName = "\ue40a";  // palette
        themeCycleDesc.category = "status";
        themeCycleDesc.description = "Tap to cycle through available themes";
        themeCycleDesc.qmlComponent = QUrl(QStringLiteral("qrc:/OpenAutoProdigy/ThemeCycleWidget.qml"));
        widgetRegistry->registerWidget(themeCycleDesc);

        oap::WidgetDescriptor batteryDesc;
        batteryDesc.id = "org.openauto.battery";
        batteryDesc.displayName = "Battery";
        batteryDesc.iconName = "\uebd4";  // battery_5_bar
        batteryDesc.category = "status";
        batteryDesc.description = "Phone battery level from companion app";
        batteryDesc.qmlComponent = QUrl(QStringLiteral("qrc:/OpenAutoProdigy/BatteryWidget.qml"));
        widgetRegistry->registerWidget(batteryDesc);

        oap::WidgetDescriptor companionStatusDesc;
        companionStatusDesc.id = "org.openauto.companion-status";
        companionStatusDesc.displayName = "Companion Status";
        companionStatusDesc.iconName = "\ue325";  // smartphone
        companionStatusDesc.category = "status";
        companionStatusDesc.description = "Companion app connection and service status";
        companionStatusDesc.qmlComponent = QUrl(QStringLiteral("qrc:/OpenAutoProdigy/CompanionStatusWidget.qml"));
        companionStatusDesc.maxCols = 4;
        companionStatusDesc.maxRows = 2;
        widgetRegistry->registerWidget(companionStatusDesc);

        oap::WidgetDescriptor aaFocusDesc;
        aaFocusDesc.id = "org.openauto.aa-focus";
        aaFocusDesc.displayName = "AA Focus";
        aaFocusDesc.iconName = "\ueff7";  // directions_car
        aaFocusDesc.category = "status";
        aaFocusDesc.description = "Toggle Android Auto projection on/off";
        aaFocusDesc.qmlComponent = QUrl(QStringLiteral("qrc:/OpenAutoProdigy/AAFocusToggleWidget.qml"));
        widgetRegistry->registerWidget(aaFocusDesc);

        // --- Phase 21: Weather Widget ---
        oap::WidgetDescriptor weatherDesc;
        weatherDesc.id = "org.openauto.weather";
        weatherDesc.displayName = "Weather";
        weatherDesc.iconName = "\ue2bd";  // thermostat
        weatherDesc.category = "status";
        weatherDesc.description = "Current weather conditions";
        weatherDesc.minCols = 1; weatherDesc.minRows = 1;
        weatherDesc.maxCols = 6; weatherDesc.maxRows = 4;
        weatherDesc.defaultCols = 2; weatherDesc.defaultRows = 2;
        weatherDesc.qmlComponent = QUrl(QStringLiteral("qrc:/OpenAutoProdigy/WeatherWidget.qml"));
        weatherDesc.defaultConfig = {{"unit", "fahrenheit"}, {"refresh", "5"}};
        weatherDesc.configSchema = {
            oap::ConfigSchemaField{
                "unit", "Temperature Unit", oap::ConfigFieldType::Enum,
                {QString::fromUtf8("\u00B0F"), QString::fromUtf8("\u00B0C")}, {"fahrenheit", "celsius"},
                0, 0, 0
            },
            oap::ConfigSchemaField{
                "refresh", "Refresh Interval", oap::ConfigFieldType::Enum,
                {"5 minutes", "15 minutes", "30 minutes", "60 minutes"}, {"5", "15", "30", "60"},
                0, 0, 0
            }
        };
        widgetRegistry->registerWidget(weatherDesc);
    }

#ifdef HAS_WEBENGINE
    // Web widget runtime: serve scanned packages over prodigy:// and
    // register them as grid widgets (design 2026-07-06-js-runtime §3-§4).
    auto* webWidgetResolver = new oap::WebWidgetContentResolver();
    auto* webWidgetSchemeHandler =
        new oap::WebWidgetSchemeHandler(webWidgetResolver, &app);
    QQuickWebEngineProfile::defaultProfile()->installUrlSchemeHandler(
        "prodigy", webWidgetSchemeHandler);
    const int webWidgetCount = oap::WebWidgetScanner::scan(
        QDir::homePath() + QStringLiteral("/.openauto/webwidgets"),
        *widgetRegistry, webWidgetResolver);
    qInfo() << "Registered" << webWidgetCount << "web widget(s) from"
            << (QDir::homePath() + QStringLiteral("/.openauto/webwidgets"));
    if (webWidgetCount > 0) {
        const QVariant apiEnabledV = configService->value(QStringLiteral("api.enabled"));
        const bool apiEnabled = apiEnabledV.isValid() ? apiEnabledV.toBool() : true;
        if (!apiEnabled)
            qWarning() << "Web widgets are registered but api.enabled is false — "
                          "they will render and spin 'connecting…' forever "
                          "(web widgets require the External API; set api.enabled: true)";
    }
#endif

    // Collect widget descriptors from plugins
    for (auto* plugin : pluginManager.plugins()) {
        for (const auto& desc : plugin->widgetDescriptors()) {
            widgetRegistry->registerWidget(desc);
        }
    }

    // --- Dashboards: per-dashboard widget grids (design 2026-07-05 §3) ---
    auto dashboardManager = new oap::DashboardManager(
        widgetRegistry, hostContext.get(), yamlConfig, yamlPath, &app);
    {
        qreal cs = displayInfo->cellSide();
        int initCols = qMax(3, static_cast<int>(std::floor(displayInfo->windowWidth() / cs)));
        int initRows = qMax(2, static_cast<int>(std::floor(displayInfo->windowHeight() / cs)));
        dashboardManager->loadFromConfig(initCols, initRows);
    }

    // Flush any pending debounced dashboard persist on quit (belt-and-
    // suspenders alongside DashboardManager's shared_ptr YamlConfig ref —
    // see EqualizerService::saveNow precedent above). yamlConfig is a
    // shared_ptr now held by dashboardManager too, so its dtor flush is
    // safe regardless of teardown order, but doing it here means the write
    // happens during normal event-loop teardown rather than at destruction.
    QObject::connect(&app, &QGuiApplication::aboutToQuit,
                     dashboardManager, &oap::DashboardManager::flushPendingPersist);

    // QML (HomeMenu.qml) is the sole authority for grid dimensions.
    // It applies snap-aware computation on top of DisplayInfo.cellSide.
    // Do NOT push non-snapped dims from C++ — that races with QML's
    // snapped values and can persist intermediate (wrong) dimensions.
    // The web config panel does not change grid dimensions directly.

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
    actionRegistry->registerAction("app.home", [pluginModel, dashboardManager](const QVariant&) {
        if (auto* m = dashboardManager->activeModel()) m->setWidgetSelected(false);
        pluginModel->setActivePlugin(QString());
    });
    actionRegistry->registerAction("app.dashboard.next",
        [dashboardManager](const QVariant&) { dashboardManager->nextDashboard(); });
    actionRegistry->registerAction("app.dashboard.previous",
        [dashboardManager](const QVariant&) { dashboardManager->previousDashboard(); });
    actionRegistry->registerAction("app.dashboard.select",
        [dashboardManager](const QVariant& v) { dashboardManager->switchTo(v.toString()); });
    actionRegistry->registerAction("theme.toggle", [themeService](const QVariant&) {
        themeService->toggleMode();
    });
    actionRegistry->registerAction("media.playPause", [mediaStatusService](const QVariant&) {
        mediaStatusService->playPause();
    });
    actionRegistry->registerAction("media.next", [mediaStatusService](const QVariant&) {
        mediaStatusService->next();
    });
    actionRegistry->registerAction("media.previous", [mediaStatusService](const QVariant&) {
        mediaStatusService->previous();
    });
    // AA button press action (used by DebugSettings via ActionRegistry.dispatch)
    if (auto* orch = aaPlugin->orchestrator()) {
        actionRegistry->registerAction("aa.sendButton", [orch](const QVariant& v) {
            orch->sendButtonPress(v.toInt());
        });
        actionRegistry->registerAction("aa.requestFocus", [orch](const QVariant&) {
            orch->requestVideoFocus();
        });
        actionRegistry->registerAction("aa.exitToCar", [orch](const QVariant&) {
            orch->requestExitToCar();
        });
    }

    // Widget command egress actions
    actionRegistry->registerAction("app.launchPlugin", [pluginModel, dashboardManager](const QVariant& v) {
        if (auto* m = dashboardManager->activeModel()) m->setWidgetSelected(false);
        pluginModel->setActivePlugin(v.toString());
    });
    actionRegistry->registerAction("app.openSettings", [pluginModel, appController, dashboardManager](const QVariant&) {
        if (auto* m = dashboardManager->activeModel()) m->setWidgetSelected(false);
        pluginModel->setActivePlugin(QString());
        appController->navigateTo(6);
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
    actionRegistry->registerAction("navbar.volume.shortHold", [appController, pluginModel, navbarController, dashboardManager](const QVariant&) {
        if (auto* m = dashboardManager->activeModel()) m->setWidgetSelected(false);
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
    actionRegistry->registerAction("navbar.clock.tap", [pluginModel, appController, dashboardManager](const QVariant&) {
        if (auto* m = dashboardManager->activeModel()) m->setWidgetSelected(false);
        pluginModel->setActivePlugin(QString());
        appController->navigateTo(0);
    });
    // Clock short-hold: open settings
    actionRegistry->registerAction("navbar.clock.shortHold", [appController, dashboardManager](const QVariant&) {
        if (auto* m = dashboardManager->activeModel()) m->setWidgetSelected(false);
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
    actionRegistry->registerAction("navbar.brightness.shortHold", [appController, pluginModel, navbarController, dashboardManager](const QVariant&) {
        if (auto* m = dashboardManager->activeModel()) m->setWidgetSelected(false);
        pluginModel->setActivePlugin(QString());
        appController->navigateTo(6);
        emit navbarController->settingsPageRequested(QStringLiteral("display"));
    });
    // Brightness long-hold: toggle night mode
    actionRegistry->registerAction("navbar.brightness.longHold", [themeService](const QVariant&) {
        themeService->toggleMode();
    });

    // --- Widget interaction mode navbar actions ---
    actionRegistry->registerAction("navbar.gear.tap", [navbarController](const QVariant&) {
        emit navbarController->widgetConfigRequested();
    });
    actionRegistry->registerAction("navbar.gear.shortHold", [](const QVariant&) {
        // No-op: gear is tap-only during widget interaction mode
    });
    actionRegistry->registerAction("navbar.gear.longHold", [](const QVariant&) {
        // No-op: gear is tap-only during widget interaction mode
    });
    actionRegistry->registerAction("navbar.trash.tap", [navbarController](const QVariant&) {
        emit navbarController->widgetDeleteRequested();
    });
    actionRegistry->registerAction("navbar.trash.shortHold", [](const QVariant&) {
        // No-op: trash is tap-only during widget interaction mode
    });
    actionRegistry->registerAction("navbar.trash.longHold", [](const QVariant&) {
        // No-op: trash is tap-only during widget interaction mode
    });

    engine.rootContext()->setContextProperty("NavbarController", navbarController);
    engine.rootContext()->setContextProperty("ActionRegistry", actionRegistry);
    engine.rootContext()->setContextProperty("OverlayService", overlayService);
    engine.rootContext()->setContextProperty("ThemeService", themeService);
    engine.rootContext()->setContextProperty("ApplicationController", appController);
    engine.rootContext()->setContextProperty("PluginModel", pluginModel);
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

    auto weatherService = new oap::WeatherService(&app);
    engine.rootContext()->setContextProperty("WeatherService", weatherService);

    // DashboardManager + per-dashboard WidgetGridModel/WidgetContextFactory.
    // NEVER cache activeModel()/activeFactory() beyond repointGridContext —
    // the active dashboard changes at runtime; re-point on every switch.
    engine.rootContext()->setContextProperty("DashboardManager", dashboardManager);
    auto repointGridContext = [&engine, dashboardManager]() {
        engine.rootContext()->setContextProperty("WidgetGridModel", dashboardManager->activeModel());
        engine.rootContext()->setContextProperty("WidgetContextFactory", dashboardManager->activeFactory());
    };
    repointGridContext();
    QObject::connect(dashboardManager, &oap::DashboardManager::activeDashboardChanged,
                     &engine, repointGridContext);

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
    engine.addImageProvider(QStringLiteral("mediaart"), mediaArtProvider);  // engine takes ownership

    // Geometry override for windowed resolution testing
    engine.rootContext()->setContextProperty("_geomW", geomW);
    engine.rootContext()->setContextProperty("_geomH", geomH);

    // External API v1 — the single external integration surface (design doc
    // docs/archive/plans/2026-07-06-external-api-v1-design.md). Every ref
    // below is an app-lifetime object (parented to &app, or — for navBridge/
    // mediaStatusService/etc. — created earlier in main() with &app as an
    // ancestor), and ApiServer itself is instantiated here, after all of them,
    // parented to &app: this satisfies the provider-outlives-server lifetime
    // contract documented at the top of ApiServer.hpp.
    oap::api::ApiServiceRefs apiRefs;
    apiRefs.media = mediaStatusService;
    apiRefs.navigation = navBridge;                 // always constructed; inert without an AA orchestrator
    apiRefs.projection = projectionStatusProvider;   // nullptr when aaPlugin has no orchestrator
    apiRefs.phone = phoneStateService;
    apiRefs.theme = themeService;
    apiRefs.notifications = notificationService;
    apiRefs.actions = actionRegistry;
    apiRefs.config = configService.get();
    apiRefs.bluetooth = bluetoothManager;
    apiRefs.display = displayInfo;
    auto* apiServer = new oap::api::ApiServer(apiRefs, &app);
    if (!apiServer->start())
        qWarning() << "[main] External API disabled or failed to start";
    engine.rootContext()->setContextProperty("ApiService", apiServer);
    // Companion phone reports (GPS / battery / connectivity) surfaced to QML
    // widgets via API v1 inbound state (design §B0b). ApiServer is constructed
    // above — well before engine.load() below — so this is set before the
    // null-guarded widgets first paint; no hoist of the ApiServer construction
    // was required. The legacy CompanionService property remains for now
    // (CompanionSettings.qml still reads it; removal is B2).
    engine.rootContext()->setContextProperty("CompanionState", apiServer->inboundState());
    QObject::connect(apiServer->inboundState(), &oap::api::ApiInboundState::proxyRouteChanged,
                     &app, [systemClient](bool active, const QString& host, quint16 port,
                                          const QString& password) {
        if (systemClient)
            systemClient->setProxyRoute(active, host, static_cast<int>(port), password);
    });
    QObject::connect(apiServer->inboundState(), &oap::api::ApiInboundState::timeReported,
                     &app, [](qint64 unixMs) { adjustClockFromApiTimeReport(unixMs); });
    QObject::connect(apiServer->inboundState(), &oap::api::ApiInboundState::timezoneReported,
                     &app, [](const QString& ianaId) { adjustTimezoneFromApiTimeReport(ianaId); });

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

    // Unix signals, handled async-signal-safely via a self-pipe. The raw
    // handlers below only write() a byte — no Qt calls, no allocation — because
    // QMetaObject::invokeMethod takes event-queue locks and can deadlock if a
    // signal (e.g. SIGTERM from `systemctl restart`) lands while the main thread
    // is inside postEvent. A QSocketNotifier drains the pipe on the main thread
    // and does the real work:
    //   SIGUSR1 → disconnect AA session (ShutdownRequest + teardown, keep listening)
    //   SIGTERM/SIGINT → clean Qt quit. Without this, `systemctl restart` kills
    //     the process before app.exec() returns, so aboutToQuit handlers and
    //     pluginManager.shutdownAll() (plugin state saves) never run
    //     (bench 2026-07-09 row 11).
    static oap::plugins::AndroidAutoPlugin* g_aaPlugin = aaPlugin;
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, g_signalFds) != 0) {
        qCWarning(lcCore) << "socketpair for signal self-pipe failed — "
                             "signal handlers not installed";
    } else {
        auto handler = [](int sig) {
            const int savedErrno = errno;
            char b = static_cast<char>(sig);
            ssize_t n = ::write(g_signalFds[0], &b, 1);
            (void)n;
            errno = savedErrno;
        };
        signal(SIGUSR1, handler);
        signal(SIGTERM, handler);
        signal(SIGINT, handler);

        auto* signalNotifier = new QSocketNotifier(g_signalFds[1],
                                                   QSocketNotifier::Read, &app);
        QObject::connect(signalNotifier, &QSocketNotifier::activated, &app, [](){
            char b = 0;
            if (::read(g_signalFds[1], &b, 1) != 1)
                return;
            switch (static_cast<int>(b)) {
            case SIGUSR1:
                if (g_aaPlugin) g_aaPlugin->stopAA();
                break;
            case SIGTERM:
            case SIGINT:
                QCoreApplication::quit();
                break;
            }
        });
    }

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
