# Static library of all app code (shared between main executable and tests)
add_library(openauto-core STATIC
    core/Logging.cpp
    core/YamlConfig.cpp
    core/InputDeviceScanner.cpp
    core/system/HostapdConfig.cpp
    core/aa/VideoDecoder.cpp
    core/aa/DmaBufVideoBuffer.cpp
    core/aa/TouchHandler.cpp
    core/aa/TouchRouter.cpp
    core/aa/EvdevCoordBridge.cpp
    core/aa/EvdevTouchReader.cpp
    core/aa/NightModeProvider.cpp
    core/aa/TimedNightMode.cpp
    core/aa/ThemeNightMode.cpp
    core/aa/GpioNightMode.cpp
    core/aa/CodecCapability.cpp
    core/aa/ServiceDiscoveryBuilder.cpp
    core/aa/AndroidAutoOrchestrator.cpp
    core/aa/MediaDataBridge.cpp
    core/aa/NavigationDataBridge.cpp
    core/aa/AndroidAutoRuntimeBridge.cpp
    ui/ApplicationController.cpp
    ui/PluginModel.cpp
    ui/PluginRuntimeContext.cpp
    ui/PluginViewHost.cpp

    ui/AudioDeviceModel.cpp
    ui/PairedDevicesModel.cpp
    ui/CodecCapabilityModel.cpp
    ui/NavbarController.cpp
    ui/GestureOverlayController.cpp
    ui/SettingsInputBoundary.cpp
    ui/DisplayInfo.cpp
    core/plugin/HostContext.cpp
    core/services/ConfigService.cpp
    core/services/ThemeService.cpp
    core/services/DisplayService.cpp
    core/services/AudioService.cpp
    core/audio/PipeWireDeviceRegistry.cpp
    core/audio/EqualizerEngine.cpp
    core/services/IpcServer.cpp
    core/services/EventBus.cpp
    core/services/ActionRegistry.cpp
    core/services/NotificationService.cpp
    core/services/CompanionListenerService.cpp
    core/services/WeatherService.cpp
    core/services/SystemServiceClient.cpp
    core/services/BluetoothManager.cpp
    core/services/EqualizerService.cpp
    core/services/IProjectionStatusProvider.cpp
    core/services/INavigationProvider.cpp
    core/services/IMediaStatusProvider.cpp
    core/services/ICallStateProvider.cpp
    core/services/ProjectionStatusProvider.cpp
    core/services/IPhoneStateService.cpp
    core/services/PhoneStateService.cpp
    core/services/IMediaStatusService.cpp
    core/services/MediaStatusService.cpp
    ui/NotificationModel.cpp
    core/plugin/PluginManifest.cpp
    core/plugin/PluginDiscovery.cpp
    core/plugin/PluginLoader.cpp
    core/plugin/PluginManager.cpp
    plugins/android_auto/AndroidAutoPlugin.cpp
    plugins/bt_audio/BtAudioPlugin.cpp
    plugins/phone/PhonePlugin.cpp
    plugins/equalizer/EqualizerPlugin.cpp
    core/widget/WidgetRegistry.cpp
    ui/WidgetInstanceContext.cpp
    ui/WidgetContextFactory.cpp
    ui/WidgetPlacementModel.cpp
    ui/WidgetPickerModel.cpp
    ui/WidgetGridModel.cpp
    ${CMAKE_SOURCE_DIR}/libs/qrcodegen/qrcodegen.cpp
)

target_include_directories(openauto-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs/prodigy-oaa-protocol/include
    ${CMAKE_BINARY_DIR}/libs/prodigy-oaa-protocol
    ${CMAKE_SOURCE_DIR}/libs/qrcodegen
    ${LIBAV_INCLUDE_DIRS}
    ${PIPEWIRE_INCLUDE_DIRS}
)

target_link_libraries(openauto-core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Network
    Qt6::DBus
    prodigy-oaa-protocol
    OpenSSL::SSL
    OpenSSL::Crypto
    protobuf::libprotobuf
    ${LIBAV_LIBRARIES}
    ${PIPEWIRE_LIBRARIES}
    yaml-cpp::yaml-cpp
)

set_target_properties(openauto-core PROPERTIES AUTOMOC ON)

qt_add_executable(openauto-prodigy
    main.cpp
)

# Tell Qt the QML type name for each file (override subdirectory-based naming)
set_source_files_properties(../qml/main.qml PROPERTIES
    QT_RESOURCE_ALIAS "main.qml"
)
set_source_files_properties(../qml/controls/NormalText.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "NormalText"
    QT_RESOURCE_ALIAS "NormalText.qml"
)
set_source_files_properties(../qml/controls/SpecialText.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SpecialText"
    QT_RESOURCE_ALIAS "SpecialText.qml"
)
set_source_files_properties(../qml/controls/Icon.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "Icon"
    QT_RESOURCE_ALIAS "Icon.qml"
)
set_source_files_properties(../qml/controls/MaterialIcon.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "MaterialIcon"
    QT_RESOURCE_ALIAS "MaterialIcon.qml"
)
set_source_files_properties(../qml/controls/Tile.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "Tile"
    QT_RESOURCE_ALIAS "Tile.qml"
)
set_source_files_properties(../qml/controls/SectionHeader.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SectionHeader"
    QT_RESOURCE_ALIAS "SectionHeader.qml"
)
set_source_files_properties(../qml/controls/SettingsToggle.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SettingsToggle"
    QT_RESOURCE_ALIAS "SettingsToggle.qml"
)
set_source_files_properties(../qml/controls/SettingsSlider.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SettingsSlider"
    QT_RESOURCE_ALIAS "SettingsSlider.qml"
)
set_source_files_properties(../qml/controls/SettingsListItem.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SettingsListItem"
    QT_RESOURCE_ALIAS "SettingsListItem.qml"
)
set_source_files_properties(../qml/controls/FullScreenPicker.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "FullScreenPicker"
    QT_RESOURCE_ALIAS "FullScreenPicker.qml"
)
set_source_files_properties(../qml/controls/SegmentedButton.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SegmentedButton"
    QT_RESOURCE_ALIAS "SegmentedButton.qml"
)
set_source_files_properties(../qml/controls/ReadOnlyField.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "ReadOnlyField"
    QT_RESOURCE_ALIAS "ReadOnlyField.qml"
)
set_source_files_properties(../qml/controls/SettingsRow.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SettingsRow"
    QT_RESOURCE_ALIAS "SettingsRow.qml"
)
set_source_files_properties(../qml/controls/SettingsHoldArea.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SettingsHoldArea"
    QT_RESOURCE_ALIAS "SettingsHoldArea.qml"
)
set_source_files_properties(../qml/controls/SettingsScrollHints.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SettingsScrollHints"
    QT_RESOURCE_ALIAS "SettingsScrollHints.qml"
)
set_source_files_properties(../qml/controls/BackHoldArea.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "BackHoldArea"
    QT_RESOURCE_ALIAS "BackHoldArea.qml"
)
set_source_files_properties(../qml/controls/InfoBanner.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "InfoBanner"
    QT_RESOURCE_ALIAS "InfoBanner.qml"
)
set_source_files_properties(../qml/controls/EqBandSlider.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "EqBandSlider"
    QT_RESOURCE_ALIAS "EqBandSlider.qml"
)
set_source_files_properties(../qml/controls/ElevatedButton.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "ElevatedButton"
    QT_RESOURCE_ALIAS "ElevatedButton.qml"
)
set_source_files_properties(../qml/controls/FilledButton.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "FilledButton"
    QT_RESOURCE_ALIAS "FilledButton.qml"
)
set_source_files_properties(../qml/controls/OutlinedButton.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "OutlinedButton"
    QT_RESOURCE_ALIAS "OutlinedButton.qml"
)
set_source_files_properties(../qml/controls/Toast.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "Toast"
    QT_RESOURCE_ALIAS "Toast.qml"
)
set_source_files_properties(../qml/controls/UiMetrics.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "UiMetrics"
    QT_QML_SINGLETON_TYPE TRUE
    QT_RESOURCE_ALIAS "UiMetrics.qml"
)
set_source_files_properties(../qml/components/Wallpaper.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "Wallpaper"
    QT_RESOURCE_ALIAS "Wallpaper.qml"
)
set_source_files_properties(../qml/components/Navbar.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "Navbar"
    QT_RESOURCE_ALIAS "Navbar.qml"
)
set_source_files_properties(../qml/components/NavbarControl.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "NavbarControl"
    QT_RESOURCE_ALIAS "NavbarControl.qml"
)
set_source_files_properties(../qml/components/NavbarPopup.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "NavbarPopup"
    QT_RESOURCE_ALIAS "NavbarPopup.qml"
)
set_source_files_properties(../qml/components/Shell.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "Shell"
    QT_RESOURCE_ALIAS "Shell.qml"
)
set_source_files_properties(../qml/components/GestureOverlay.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "GestureOverlay"
    QT_RESOURCE_ALIAS "GestureOverlay.qml"
)
set_source_files_properties(../qml/components/PairingDialog.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "PairingDialog"
    QT_RESOURCE_ALIAS "PairingDialog.qml"
)
set_source_files_properties(../qml/components/FirstRunBanner.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "FirstRunBanner"
    QT_RESOURCE_ALIAS "FirstRunBanner.qml"
)
set_source_files_properties(../qml/components/NotificationArea.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "NotificationArea"
    QT_RESOURCE_ALIAS "NotificationArea.qml"
)
set_source_files_properties(../qml/components/ExitDialog.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "ExitDialog"
    QT_RESOURCE_ALIAS "ExitDialog.qml"
)


set_source_files_properties(../qml/components/WidgetConfigSheet.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "WidgetConfigSheet"
    QT_RESOURCE_ALIAS "WidgetConfigSheet.qml"
)
set_source_files_properties(../qml/components/WidgetPickerSheet.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "WidgetPickerSheet"
    QT_RESOURCE_ALIAS "WidgetPickerSheet.qml"
)
set_source_files_properties(../qml/widgets/ClockWidget.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "ClockWidget"
    QT_RESOURCE_ALIAS "ClockWidget.qml"
    QT_QML_SKIP_CACHEGEN TRUE
)
set_source_files_properties(../qml/widgets/DateWidget.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "DateWidget"
    QT_RESOURCE_ALIAS "DateWidget.qml"
    QT_QML_SKIP_CACHEGEN TRUE
)

set_source_files_properties(../qml/widgets/NavigationWidget.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "NavigationWidget"
    QT_RESOURCE_ALIAS "NavigationWidget.qml"
    QT_QML_SKIP_CACHEGEN TRUE
)
set_source_files_properties(../qml/widgets/NowPlayingWidget.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "NowPlayingWidget"
    QT_RESOURCE_ALIAS "NowPlayingWidget.qml"
    QT_QML_SKIP_CACHEGEN TRUE
)
set_source_files_properties(../qml/widgets/SettingsLauncherWidget.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SettingsLauncherWidget"
    QT_RESOURCE_ALIAS "SettingsLauncherWidget.qml"
    QT_QML_SKIP_CACHEGEN TRUE
)
set_source_files_properties(../qml/widgets/AALauncherWidget.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "AALauncherWidget"
    QT_RESOURCE_ALIAS "AALauncherWidget.qml"
    QT_QML_SKIP_CACHEGEN TRUE
)
set_source_files_properties(../qml/widgets/ThemeCycleWidget.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "ThemeCycleWidget"
    QT_RESOURCE_ALIAS "ThemeCycleWidget.qml"
    QT_QML_SKIP_CACHEGEN TRUE
)
set_source_files_properties(../qml/widgets/BatteryWidget.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "BatteryWidget"
    QT_RESOURCE_ALIAS "BatteryWidget.qml"
    QT_QML_SKIP_CACHEGEN TRUE
)
set_source_files_properties(../qml/widgets/CompanionStatusWidget.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "CompanionStatusWidget"
    QT_RESOURCE_ALIAS "CompanionStatusWidget.qml"
    QT_QML_SKIP_CACHEGEN TRUE
)
set_source_files_properties(../qml/widgets/AAFocusToggleWidget.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "AAFocusToggleWidget"
    QT_RESOURCE_ALIAS "AAFocusToggleWidget.qml"
    QT_QML_SKIP_CACHEGEN TRUE
)
set_source_files_properties(../qml/widgets/WeatherWidget.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "WeatherWidget"
    QT_RESOURCE_ALIAS "WeatherWidget.qml"
    QT_QML_SKIP_CACHEGEN TRUE
)

set_source_files_properties(../qml/applications/android_auto/AndroidAutoMenu.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "AndroidAutoMenu"
    QT_RESOURCE_ALIAS "AndroidAutoMenu.qml"
)
set_source_files_properties(../qml/applications/settings/SettingsMenu.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SettingsMenu"
    QT_RESOURCE_ALIAS "SettingsMenu.qml"
)
set_source_files_properties(../qml/applications/home/HomeMenu.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "HomeMenu"
    QT_RESOURCE_ALIAS "HomeMenu.qml"
)
set_source_files_properties(../qml/applications/bt_audio/BtAudioView.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "BtAudioView"
    QT_RESOURCE_ALIAS "BtAudioView.qml"
)
set_source_files_properties(../qml/applications/phone/PhoneView.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "PhoneView"
    QT_RESOURCE_ALIAS "PhoneView.qml"
)
set_source_files_properties(../qml/applications/phone/IncomingCallOverlay.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "IncomingCallOverlay"
    QT_RESOURCE_ALIAS "IncomingCallOverlay.qml"
)
set_source_files_properties(../qml/applications/settings/AASettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "AASettings"
    QT_RESOURCE_ALIAS "AASettings.qml"
)
set_source_files_properties(../qml/applications/settings/DisplaySettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "DisplaySettings"
    QT_RESOURCE_ALIAS "DisplaySettings.qml"
)
set_source_files_properties(../qml/applications/settings/AudioSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "AudioSettings"
    QT_RESOURCE_ALIAS "AudioSettings.qml"
)
set_source_files_properties(../qml/applications/settings/ConnectionSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "ConnectionSettings"
    QT_RESOURCE_ALIAS "ConnectionSettings.qml"
)
set_source_files_properties(../qml/applications/settings/SystemSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SystemSettings"
    QT_RESOURCE_ALIAS "SystemSettings.qml"
)
set_source_files_properties(../qml/applications/settings/CompanionSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "CompanionSettings"
    QT_RESOURCE_ALIAS "CompanionSettings.qml"
)
set_source_files_properties(../qml/applications/settings/EqSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "EqSettings"
    QT_RESOURCE_ALIAS "EqSettings.qml"
)
set_source_files_properties(../qml/applications/settings/ThemeSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "ThemeSettings"
    QT_RESOURCE_ALIAS "ThemeSettings.qml"
)
set_source_files_properties(../qml/applications/settings/InformationSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "InformationSettings"
    QT_RESOURCE_ALIAS "InformationSettings.qml"
)
set_source_files_properties(../qml/applications/settings/DebugSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "DebugSettings"
    QT_RESOURCE_ALIAS "DebugSettings.qml"
)

qt_add_qml_module(openauto-prodigy
    URI OpenAutoProdigy
    VERSION 1.0
    QML_FILES
        ../qml/main.qml
        ../qml/controls/NormalText.qml
        ../qml/controls/SpecialText.qml
        ../qml/controls/Icon.qml
        ../qml/controls/MaterialIcon.qml
        ../qml/controls/Tile.qml
        ../qml/controls/SectionHeader.qml
        ../qml/controls/SettingsToggle.qml
        ../qml/controls/SettingsSlider.qml
        ../qml/controls/SettingsListItem.qml
        ../qml/controls/FullScreenPicker.qml
        ../qml/controls/SegmentedButton.qml
        ../qml/controls/ReadOnlyField.qml
        ../qml/controls/SettingsRow.qml
        ../qml/controls/SettingsHoldArea.qml
        ../qml/controls/SettingsScrollHints.qml
        ../qml/controls/BackHoldArea.qml
        ../qml/controls/InfoBanner.qml
        ../qml/controls/EqBandSlider.qml
        ../qml/controls/ElevatedButton.qml
        ../qml/controls/FilledButton.qml
        ../qml/controls/OutlinedButton.qml
        ../qml/controls/Toast.qml
        ../qml/controls/UiMetrics.qml
        ../qml/components/Wallpaper.qml
        ../qml/components/Shell.qml
        ../qml/components/Navbar.qml
        ../qml/components/NavbarControl.qml
        ../qml/components/NavbarPopup.qml
        ../qml/components/GestureOverlay.qml
        ../qml/components/PairingDialog.qml
        ../qml/components/FirstRunBanner.qml
        ../qml/components/NotificationArea.qml
        ../qml/components/ExitDialog.qml
        ../qml/components/WidgetConfigSheet.qml
        ../qml/components/WidgetPickerSheet.qml
        ../qml/widgets/ClockWidget.qml
        ../qml/widgets/DateWidget.qml

        ../qml/widgets/NavigationWidget.qml
        ../qml/widgets/NowPlayingWidget.qml
        ../qml/widgets/SettingsLauncherWidget.qml
        ../qml/widgets/AALauncherWidget.qml
        ../qml/widgets/ThemeCycleWidget.qml
        ../qml/widgets/BatteryWidget.qml
        ../qml/widgets/CompanionStatusWidget.qml
        ../qml/widgets/AAFocusToggleWidget.qml
        ../qml/widgets/WeatherWidget.qml

        ../qml/applications/android_auto/AndroidAutoMenu.qml
        ../qml/applications/settings/SettingsMenu.qml
        ../qml/applications/home/HomeMenu.qml
        ../qml/applications/bt_audio/BtAudioView.qml
        ../qml/applications/phone/PhoneView.qml
        ../qml/applications/phone/IncomingCallOverlay.qml
        ../qml/applications/settings/AASettings.qml
        ../qml/applications/settings/DisplaySettings.qml
        ../qml/applications/settings/AudioSettings.qml
        ../qml/applications/settings/ConnectionSettings.qml
        ../qml/applications/settings/SystemSettings.qml
        ../qml/applications/settings/CompanionSettings.qml
        ../qml/applications/settings/EqSettings.qml
        ../qml/applications/settings/ThemeSettings.qml
        ../qml/applications/settings/InformationSettings.qml
        ../qml/applications/settings/DebugSettings.qml
    RESOURCES
        ../resources/resources.qrc
)

if(OAP_SKIP_QML_PLUGIN_IMPORT)
    set_target_properties(openauto-prodigy PROPERTIES QT_NO_IMPORT_QML_PLUGINS ON)
endif()

target_link_libraries(openauto-prodigy PRIVATE openauto-core)

# Git hash for ServiceDiscoveryResponse sw_build field
target_compile_definitions(openauto-core PUBLIC
    OAP_GIT_HASH="${OAP_GIT_HASH}"
)

# systemd notification — optional, for Type=notify service unit + watchdog
if(SYSTEMD_FOUND)
    target_compile_definitions(openauto-prodigy PRIVATE HAS_SYSTEMD)
    target_link_libraries(openauto-prodigy PRIVATE PkgConfig::SYSTEMD)
    message(STATUS "libsystemd found — sd_notify integration enabled")
else()
    message(STATUS "libsystemd NOT found — sd_notify integration disabled")
endif()

# Bluetooth discovery — optional, not available on all dev platforms (e.g. WSL2)
if(TARGET Qt6::Bluetooth)
    target_sources(openauto-core PRIVATE
        core/aa/BluetoothDiscoveryService.cpp
    )
    # BlueZ C library for direct SDP record registration via legacy SDP socket.
    # ProfileManager1 D-Bus API conflicts with QBluetoothServer's RFCOMM binding.
    find_library(BLUEZ_LIB bluetooth)
    if(BLUEZ_LIB)
        target_link_libraries(openauto-core PUBLIC Qt6::Bluetooth ${BLUEZ_LIB})
    else()
        target_link_libraries(openauto-core PUBLIC Qt6::Bluetooth)
        message(WARNING "libbluetooth not found — SDP registration will fail at runtime")
    endif()
    target_compile_definitions(openauto-core PUBLIC HAS_BLUETOOTH)
    message(STATUS "Qt6::Bluetooth found — Bluetooth discovery enabled")
else()
    message(STATUS "Qt6::Bluetooth NOT found — Bluetooth discovery disabled (wireless requires manual TCP)")
endif()
