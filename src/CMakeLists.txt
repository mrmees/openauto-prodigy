qt_add_executable(openauto-prodigy
    main.cpp
    core/Configuration.cpp
    core/YamlConfig.cpp
    core/InputDeviceScanner.cpp
    core/aa/AndroidAutoService.cpp
    core/aa/AndroidAutoEntity.cpp
    core/aa/ServiceFactory.cpp
    core/aa/VideoDecoder.cpp
    core/aa/VideoService.cpp
    core/aa/TouchHandler.cpp
    core/aa/EvdevTouchReader.cpp
    core/aa/NightModeProvider.cpp
    core/aa/TimedNightMode.cpp
    core/aa/ThemeNightMode.cpp
    core/aa/GpioNightMode.cpp
    core/aa/ProtocolLogger.cpp
    core/aa/ServiceDiscoveryBuilder.cpp
    core/aa/handlers/SensorChannelHandler.cpp
    core/aa/handlers/InputChannelHandler.cpp
    core/aa/handlers/BluetoothChannelHandler.cpp
    core/aa/handlers/WiFiChannelHandler.cpp
    core/aa/handlers/AudioChannelHandler.cpp
    core/aa/handlers/VideoChannelHandler.cpp
    ui/ApplicationController.cpp
    ui/PluginModel.cpp
    ui/PluginRuntimeContext.cpp
    ui/PluginViewHost.cpp
    ui/LauncherModel.cpp
    ui/AudioDeviceModel.cpp
    core/plugin/HostContext.cpp
    core/services/ConfigService.cpp
    core/services/ThemeService.cpp
    core/services/AudioService.cpp
    core/audio/PipeWireDeviceRegistry.cpp
    core/services/IpcServer.cpp
    core/services/EventBus.cpp
    core/services/ActionRegistry.cpp
    core/services/NotificationService.cpp
    core/services/CompanionListenerService.cpp
    ui/NotificationModel.cpp
    core/plugin/PluginManifest.cpp
    core/plugin/PluginDiscovery.cpp
    core/plugin/PluginLoader.cpp
    core/plugin/PluginManager.cpp
    plugins/android_auto/AndroidAutoPlugin.cpp
    plugins/bt_audio/BtAudioPlugin.cpp
    plugins/phone/PhonePlugin.cpp
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
set_source_files_properties(../qml/controls/InfoBanner.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "InfoBanner"
    QT_RESOURCE_ALIAS "InfoBanner.qml"
)
set_source_files_properties(../qml/controls/UiMetrics.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "UiMetrics"
    QT_QML_SINGLETON_TYPE TRUE
    QT_RESOURCE_ALIAS "UiMetrics.qml"
)
set_source_files_properties(../qml/components/Clock.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "Clock"
    QT_RESOURCE_ALIAS "Clock.qml"
)
set_source_files_properties(../qml/components/Wallpaper.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "Wallpaper"
    QT_RESOURCE_ALIAS "Wallpaper.qml"
)
set_source_files_properties(../qml/components/TopBar.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "TopBar"
    QT_RESOURCE_ALIAS "TopBar.qml"
)
set_source_files_properties(../qml/components/BottomBar.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "BottomBar"
    QT_RESOURCE_ALIAS "BottomBar.qml"
)
set_source_files_properties(../qml/components/Shell.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "Shell"
    QT_RESOURCE_ALIAS "Shell.qml"
)
set_source_files_properties(../qml/components/NavStrip.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "NavStrip"
    QT_RESOURCE_ALIAS "NavStrip.qml"
)
set_source_files_properties(../qml/components/GestureOverlay.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "GestureOverlay"
    QT_RESOURCE_ALIAS "GestureOverlay.qml"
)
set_source_files_properties(../qml/components/NotificationArea.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "NotificationArea"
    QT_RESOURCE_ALIAS "NotificationArea.qml"
)
set_source_files_properties(../qml/components/ExitDialog.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "ExitDialog"
    QT_RESOURCE_ALIAS "ExitDialog.qml"
)
set_source_files_properties(../qml/applications/launcher/LauncherMenu.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "LauncherMenu"
    QT_RESOURCE_ALIAS "LauncherMenu.qml"
)
set_source_files_properties(../qml/applications/android_auto/AndroidAutoMenu.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "AndroidAutoMenu"
    QT_RESOURCE_ALIAS "AndroidAutoMenu.qml"
)
set_source_files_properties(../qml/applications/android_auto/Sidebar.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "Sidebar"
    QT_RESOURCE_ALIAS "Sidebar.qml"
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
set_source_files_properties(../qml/applications/settings/VideoSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "VideoSettings"
    QT_RESOURCE_ALIAS "VideoSettings.qml"
)
set_source_files_properties(../qml/applications/settings/SystemSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SystemSettings"
    QT_RESOURCE_ALIAS "SystemSettings.qml"
)
set_source_files_properties(../qml/applications/settings/CompanionSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "CompanionSettings"
    QT_RESOURCE_ALIAS "CompanionSettings.qml"
)
set_source_files_properties(../qml/applications/settings/AboutSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "AboutSettings"
    QT_RESOURCE_ALIAS "AboutSettings.qml"
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
        ../qml/controls/FullScreenPicker.qml
        ../qml/controls/SegmentedButton.qml
        ../qml/controls/ReadOnlyField.qml
        ../qml/controls/InfoBanner.qml
        ../qml/controls/UiMetrics.qml
        ../qml/components/Clock.qml
        ../qml/components/Wallpaper.qml
        ../qml/components/TopBar.qml
        ../qml/components/BottomBar.qml
        ../qml/components/Shell.qml
        ../qml/components/NavStrip.qml
        ../qml/components/GestureOverlay.qml
        ../qml/components/NotificationArea.qml
        ../qml/components/ExitDialog.qml
        ../qml/applications/launcher/LauncherMenu.qml
        ../qml/applications/android_auto/AndroidAutoMenu.qml
        ../qml/applications/android_auto/Sidebar.qml
        ../qml/applications/settings/SettingsMenu.qml
        ../qml/applications/home/HomeMenu.qml
        ../qml/applications/bt_audio/BtAudioView.qml
        ../qml/applications/phone/PhoneView.qml
        ../qml/applications/phone/IncomingCallOverlay.qml
        ../qml/applications/settings/DisplaySettings.qml
        ../qml/applications/settings/AudioSettings.qml
        ../qml/applications/settings/ConnectionSettings.qml
        ../qml/applications/settings/VideoSettings.qml
        ../qml/applications/settings/SystemSettings.qml
        ../qml/applications/settings/CompanionSettings.qml
        ../qml/applications/settings/AboutSettings.qml
    RESOURCES
        ../resources/resources.qrc
)

target_include_directories(openauto-prodigy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs/aasdk/include
    ${CMAKE_BINARY_DIR}/libs/aasdk
    ${CMAKE_SOURCE_DIR}/libs/open-androidauto/include
    ${CMAKE_BINARY_DIR}/libs/open-androidauto
    ${LIBAV_INCLUDE_DIRS}
    ${PIPEWIRE_INCLUDE_DIRS}
)

target_link_libraries(openauto-prodigy PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Network
    Qt6::DBus
    aasdk
    aasdk_proto
    open-androidauto
    Boost::system
    Boost::log
    Boost::log_setup
    OpenSSL::SSL
    OpenSSL::Crypto
    protobuf::libprotobuf
    ${LIBAV_LIBRARIES}
    ${PIPEWIRE_LIBRARIES}
    yaml-cpp::yaml-cpp
)

# Git hash for ServiceDiscoveryResponse sw_build field
target_compile_definitions(openauto-prodigy PRIVATE
    OAP_GIT_HASH="${OAP_GIT_HASH}"
)

# Bluetooth discovery — optional, not available on all dev platforms (e.g. WSL2)
if(TARGET Qt6::Bluetooth)
    target_sources(openauto-prodigy PRIVATE
        core/aa/BluetoothDiscoveryService.cpp
    )
    target_link_libraries(openauto-prodigy PRIVATE Qt6::Bluetooth)
    target_compile_definitions(openauto-prodigy PRIVATE HAS_BLUETOOTH)
    message(STATUS "Qt6::Bluetooth found — Bluetooth discovery enabled")
else()
    message(STATUS "Qt6::Bluetooth NOT found — Bluetooth discovery disabled (wireless requires manual TCP)")
endif()
