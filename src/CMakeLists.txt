qt_add_executable(openauto-prodigy
    main.cpp
    core/Configuration.cpp
    core/aa/AndroidAutoService.cpp
    core/aa/AndroidAutoEntity.cpp
    core/aa/ServiceFactory.cpp
    core/aa/VideoDecoder.cpp
    core/aa/VideoService.cpp
    core/aa/TouchHandler.cpp
    core/aa/EvdevTouchReader.cpp
    ui/ThemeController.cpp
    ui/ApplicationController.cpp
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
set_source_files_properties(../qml/controls/Tile.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "Tile"
    QT_RESOURCE_ALIAS "Tile.qml"
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
set_source_files_properties(../qml/applications/launcher/LauncherMenu.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "LauncherMenu"
    QT_RESOURCE_ALIAS "LauncherMenu.qml"
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

qt_add_qml_module(openauto-prodigy
    URI OpenAutoProdigy
    VERSION 1.0
    QML_FILES
        ../qml/main.qml
        ../qml/controls/NormalText.qml
        ../qml/controls/SpecialText.qml
        ../qml/controls/Icon.qml
        ../qml/controls/Tile.qml
        ../qml/components/Clock.qml
        ../qml/components/Wallpaper.qml
        ../qml/components/TopBar.qml
        ../qml/components/BottomBar.qml
        ../qml/applications/launcher/LauncherMenu.qml
        ../qml/applications/android_auto/AndroidAutoMenu.qml
        ../qml/applications/settings/SettingsMenu.qml
        ../qml/applications/home/HomeMenu.qml
    RESOURCES
        ../resources/resources.qrc
)

target_include_directories(openauto-prodigy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs/aasdk/include
    ${CMAKE_BINARY_DIR}/libs/aasdk
    ${LIBAV_INCLUDE_DIRS}
)

target_link_libraries(openauto-prodigy PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Network
    aasdk
    aasdk_proto
    Boost::system
    Boost::log
    Boost::log_setup
    OpenSSL::SSL
    OpenSSL::Crypto
    protobuf::libprotobuf
    ${LIBAV_LIBRARIES}
    yaml-cpp::yaml-cpp
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
