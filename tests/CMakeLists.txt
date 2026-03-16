find_package(Qt6 REQUIRED COMPONENTS Test)

# Helper: add a test that links against openauto-core (includes, libs all inherited)
function(oap_add_test NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;EXTRA_LIBS;DEFS" ${ARGN})
    add_executable(${NAME} ${ARG_SOURCES})
    target_link_libraries(${NAME} PRIVATE openauto-core Qt6::Test ${ARG_EXTRA_LIBS})
    if(ARG_DEFS)
        target_compile_definitions(${NAME} PRIVATE ${ARG_DEFS})
    endif()
    add_test(NAME ${NAME} COMMAND ${NAME})
endfunction()

# --- Core tests ---

oap_add_test(test_yaml_config
    SOURCES test_yaml_config.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
configure_file(data/test_config.yaml ${CMAKE_CURRENT_BINARY_DIR}/data/test_config.yaml COPYONLY)

oap_add_test(test_logging SOURCES test_logging.cpp)
oap_add_test(test_hostapd_config SOURCES test_hostapd_config.cpp)

oap_add_test(test_config_service SOURCES test_config_service.cpp)
oap_add_test(test_config_key_coverage SOURCES test_config_key_coverage.cpp)

# --- Plugin tests ---

oap_add_test(test_plugin_manifest
    SOURCES test_plugin_manifest.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
configure_file(data/test_plugin.yaml ${CMAKE_CURRENT_BINARY_DIR}/data/test_plugin.yaml COPYONLY)

oap_add_test(test_plugin_discovery SOURCES test_plugin_discovery.cpp)
oap_add_test(test_plugin_manager SOURCES test_plugin_manager.cpp)
oap_add_test(test_plugin_model SOURCES test_plugin_model.cpp)

# --- Service tests ---

oap_add_test(test_theme_service
    SOURCES test_theme_service.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
file(COPY data/themes DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data)

oap_add_test(test_display_info SOURCES test_display_info.cpp)
oap_add_test(test_display_service SOURCES test_display_service.cpp)
oap_add_test(test_audio_service SOURCES test_audio_service.cpp)
oap_add_test(test_event_bus SOURCES test_event_bus.cpp)
oap_add_test(test_action_registry SOURCES test_action_registry.cpp)
oap_add_test(test_notification_service SOURCES test_notification_service.cpp)
oap_add_test(test_companion_listener SOURCES test_companion_listener.cpp)
oap_add_test(test_bluetooth_manager SOURCES test_bluetooth_manager.cpp)
oap_add_test(test_projection_status_provider SOURCES test_projection_status_provider.cpp)
oap_add_test(test_phone_state_service SOURCES test_phone_state_service.cpp)
oap_add_test(test_media_status_service SOURCES test_media_status_service.cpp)

# --- Device / hardware tests ---

oap_add_test(test_night_mode SOURCES test_night_mode.cpp)
oap_add_test(test_input_device_scanner SOURCES test_input_device_scanner.cpp)
oap_add_test(test_device_registry SOURCES test_device_registry.cpp)
oap_add_test(test_paired_devices_model SOURCES test_paired_devices_model.cpp)

# --- Audio pipeline tests ---

oap_add_test(test_audio_ring_buffer SOURCES test_audio_ring_buffer.cpp)

# --- AA protocol tests ---

oap_add_test(test_service_discovery_builder SOURCES test_service_discovery_builder.cpp)
oap_add_test(test_sensor_channel_handler SOURCES test_sensor_channel_handler.cpp)
oap_add_test(test_input_channel_handler SOURCES test_input_channel_handler.cpp)
oap_add_test(test_bluetooth_channel_handler SOURCES test_bluetooth_channel_handler.cpp)
oap_add_test(test_wifi_channel_handler SOURCES test_wifi_channel_handler.cpp)
oap_add_test(test_audio_channel_handler SOURCES test_audio_channel_handler.cpp)
oap_add_test(test_video_channel_handler SOURCES test_video_channel_handler.cpp)
oap_add_test(test_avinput_channel_handler SOURCES test_avinput_channel_handler.cpp)
oap_add_test(test_navigation_channel_handler SOURCES test_navigation_channel_handler.cpp)
oap_add_test(test_media_status_channel_handler SOURCES test_media_status_channel_handler.cpp)
oap_add_test(test_oaa_integration SOURCES test_oaa_integration.cpp)
oap_add_test(test_aa_orchestrator SOURCES test_aa_orchestrator.cpp)
if(TARGET Qt6::Bluetooth)
    oap_add_test(test_bt_discovery_service SOURCES test_bt_discovery_service.cpp)
    set_tests_properties(test_bt_discovery_service PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
oap_add_test(test_indicator_mapping SOURCES test_indicator_mapping.cpp)
oap_add_test(test_circular_buffer SOURCES test_circular_buffer.cpp)
oap_add_test(test_codec_capability SOURCES test_codec_capability.cpp)
oap_add_test(test_video_frame_pool SOURCES test_video_frame_pool.cpp)
oap_add_test(test_video_decode_queue SOURCES test_video_decode_queue.cpp)

# --- Touch routing tests ---

oap_add_test(test_touch_router SOURCES test_touch_router.cpp)
oap_add_test(test_evdev_coord_bridge SOURCES test_evdev_coord_bridge.cpp)

# --- Navbar tests ---

oap_add_test(test_navbar_controller SOURCES test_navbar_controller.cpp)
oap_add_test(test_settings_input_boundary
    SOURCES test_settings_input_boundary.cpp
    EXTRA_LIBS Qt6::Quick
)
oap_add_test(test_settings_menu_structure
    SOURCES test_settings_menu_structure.cpp
    DEFS TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# --- DSP tests ---

oap_add_test(test_biquad_filter SOURCES test_biquad_filter.cpp)
oap_add_test(test_soft_limiter SOURCES test_soft_limiter.cpp)
oap_add_test(test_equalizer_engine SOURCES test_equalizer_engine.cpp)
oap_add_test(test_equalizer_presets SOURCES test_equalizer_presets.cpp)
oap_add_test(test_equalizer_service SOURCES test_equalizer_service.cpp)

# --- Widget tests ---

oap_add_test(test_widget_types SOURCES test_widget_types.cpp)
oap_add_test(test_widget_registry SOURCES test_widget_registry.cpp)
oap_add_test(test_widget_instance_context SOURCES test_widget_instance_context.cpp)
oap_add_test(test_widget_plugin_integration SOURCES test_widget_plugin_integration.cpp)

configure_file(data/test_widget_config.yaml ${CMAKE_CURRENT_BINARY_DIR}/data/test_widget_config.yaml COPYONLY)
oap_add_test(test_widget_placement_model SOURCES test_widget_placement_model.cpp)

oap_add_test(test_widget_config
    SOURCES test_widget_config.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)

oap_add_test(test_grid_dimensions SOURCES test_grid_dimensions.cpp)
oap_add_test(test_widget_grid_model SOURCES test_widget_grid_model.cpp)
oap_add_test(test_widget_contract_qml
    SOURCES test_widget_contract_qml.cpp
    EXTRA_LIBS Qt6::Quick
)
oap_add_test(test_widget_grid_remap SOURCES test_widget_grid_remap.cpp)
oap_add_test(test_widget_picker_model SOURCES test_widget_picker_model.cpp)

# --- Media bridge tests ---

oap_add_test(test_android_auto_runtime_bridge SOURCES test_android_auto_runtime_bridge.cpp)
oap_add_test(test_gesture_overlay_controller SOURCES test_gesture_overlay_controller.cpp)

oap_add_test(test_media_data_bridge SOURCES test_media_data_bridge.cpp)

# --- Navigation bridge tests ---

oap_add_test(test_navigation_data_bridge SOURCES test_navigation_data_bridge.cpp)
oap_add_test(test_maneuver_icon_provider SOURCES test_maneuver_icon_provider.cpp)

# --- Script tests (no compilation) ---

add_test(
    NAME test_prebuilt_release_package
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/test_prebuilt_release_package.py
)
add_test(
    NAME test_install_list_prebuilt
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/test_install_list_prebuilt.py
)

# GUI/Quick tests need offscreen platform when no display is available
set_tests_properties(test_theme_service test_display_service test_plugin_model test_aa_orchestrator
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
set_tests_properties(test_settings_input_boundary test_settings_menu_structure test_gesture_overlay_controller test_widget_contract_qml
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
