# EME/Widevine probe — manual verification tool for DRM playback
# (spec 2026-07-07-web-surface-strategy §Slice 1.3). Not part of the
# default build: cmake --build . --target eme-probe
if(TARGET Qt6::WebEngineQuick)
    add_executable(eme-probe EXCLUDE_FROM_ALL
        main.cpp
        probe.qrc
        ${CMAKE_SOURCE_DIR}/src/core/WidevineCdm.cpp
    )
    target_include_directories(eme-probe PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(eme-probe PRIVATE Qt6::Gui Qt6::Qml Qt6::WebEngineQuick)
endif()
