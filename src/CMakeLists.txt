qt_add_executable(openauto-prodigy
    main.cpp
    core/Configuration.cpp
    ui/ThemeController.cpp
    ui/ApplicationController.cpp
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

target_link_libraries(openauto-prodigy PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
)
