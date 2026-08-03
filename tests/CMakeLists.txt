find_package(Qt6 REQUIRED COMPONENTS Test)

# Helper: add a test that links against openauto-core (includes, libs all inherited)
function(oap_add_test NAME)
    cmake_parse_arguments(ARG "DBUS_SESSION" "" "SOURCES;EXTRA_LIBS;DEFS" ${ARGN})
    add_executable(${NAME} ${ARG_SOURCES})
    target_link_libraries(${NAME} PRIVATE openauto-core Qt6::Test ${ARG_EXTRA_LIBS})
    if(ARG_DEFS)
        target_compile_definitions(${NAME} PRIVATE ${ARG_DEFS})
    endif()
    if(ARG_DBUS_SESSION)
        add_test(NAME ${NAME} COMMAND dbus-run-session -- $<TARGET_FILE:${NAME}>)
    else()
        add_test(NAME ${NAME} COMMAND ${NAME})
    endif()
endfunction()

# --- Fixture plugins: real .so files for dynamic-load validation tests ---
# Layout matches PluginManager's naming rule: <dir>/plugin.yaml +
# lib<last-id-segment>.so
function(oap_add_fixture_plugin TARGET SRC OUT_NAME DIRNAME MANIFEST)
    add_library(${TARGET} MODULE fixtures/${SRC})
    target_link_libraries(${TARGET} PRIVATE openauto-core)
    set_target_properties(${TARGET} PROPERTIES
        OUTPUT_NAME ${OUT_NAME}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fixture_plugins/${DIRNAME})
    configure_file(data/${MANIFEST}
        ${CMAKE_CURRENT_BINARY_DIR}/fixture_plugins/${DIRNAME}/plugin.yaml COPYONLY)
endfunction()

oap_add_fixture_plugin(fixture_stale_plugin    fixture_stale_plugin.cpp    stale    stale    fixture_stale.yaml)
oap_add_fixture_plugin(fixture_imposter_plugin fixture_imposter_plugin.cpp other    imposter fixture_imposter.yaml)
oap_add_fixture_plugin(fixture_valid_plugin    fixture_valid_plugin.cpp    valid    valid    fixture_valid.yaml)
oap_add_fixture_plugin(fixture_throwing_plugin fixture_throwing_plugin.cpp throwing throwing fixture_throwing.yaml)

# --- Core tests ---

oap_add_test(test_yaml_config
    SOURCES test_yaml_config.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
configure_file(data/test_config.yaml ${CMAKE_CURRENT_BINARY_DIR}/data/test_config.yaml COPYONLY)

oap_add_test(test_logging SOURCES test_logging.cpp)
oap_add_test(test_hostapd_config SOURCES test_hostapd_config.cpp)
oap_add_test(test_widevine_cdm SOURCES test_widevine_cdm.cpp)

oap_add_test(test_config_service SOURCES test_config_service.cpp)
oap_add_test(test_config_key_coverage SOURCES test_config_key_coverage.cpp)
oap_add_test(test_oap_version SOURCES test_oap_version.cpp)

# --- Plugin tests ---

oap_add_test(test_plugin_manifest
    SOURCES test_plugin_manifest.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
configure_file(data/test_plugin.yaml ${CMAKE_CURRENT_BINARY_DIR}/data/test_plugin.yaml COPYONLY)

oap_add_test(test_plugin_discovery SOURCES test_plugin_discovery.cpp)
oap_add_test(test_plugin_manager SOURCES test_plugin_manager.cpp
    DEFS FIXTURE_PLUGINS_DIR="${CMAKE_CURRENT_BINARY_DIR}/fixture_plugins")
add_dependencies(test_plugin_manager
    fixture_stale_plugin fixture_imposter_plugin fixture_valid_plugin fixture_throwing_plugin)
oap_add_test(test_plugin_model SOURCES test_plugin_model.cpp)
oap_add_test(test_plugin_view_host SOURCES test_plugin_view_host.cpp)
oap_add_test(test_play_queue SOURCES test_play_queue.cpp)
oap_add_test(test_folder_model SOURCES test_folder_model.cpp)

oap_add_test(test_playback_engine
    SOURCES test_playback_engine.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
file(COPY data/media DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data)

oap_add_test(test_media_playback_policy SOURCES test_media_playback_policy.cpp)

oap_add_test(test_media_player_plugin
    SOURCES test_media_player_plugin.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)

oap_add_test(test_media_art_provider SOURCES test_media_art_provider.cpp)

oap_add_test(test_media_tag_reader
    SOURCES test_media_tag_reader.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)

oap_add_test(test_media_library SOURCES test_media_library.cpp)

oap_add_test(test_media_scanner
    SOURCES test_media_scanner.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)

oap_add_test(test_media_usb_policy SOURCES test_media_usb_policy.cpp)

oap_add_test(test_bt_audio_plugin DBUS_SESSION SOURCES test_bt_audio_plugin.cpp)
oap_add_test(test_bt_tap_controller SOURCES test_bt_tap_controller.cpp)

# --- Service tests ---

oap_add_test(test_theme_service
    SOURCES test_theme_service.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
file(COPY data/themes DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data)

oap_add_test(test_theme_install_request SOURCES test_theme_install_request.cpp)
oap_add_test(test_ipc_install_theme SOURCES test_ipc_install_theme.cpp)
oap_add_test(test_ipc_audio_config SOURCES test_ipc_audio_config.cpp)
oap_add_test(test_ipc_logging SOURCES test_ipc_logging.cpp)
oap_add_test(test_ipc_single_instance SOURCES test_ipc_single_instance.cpp)

oap_add_test(test_display_info SOURCES test_display_info.cpp)
oap_add_test(test_display_service SOURCES test_display_service.cpp)
oap_add_test(test_audio_service SOURCES test_audio_service.cpp)
oap_add_test(test_event_bus SOURCES test_event_bus.cpp)
oap_add_test(test_action_registry SOURCES test_action_registry.cpp)
oap_add_test(test_overlay_service SOURCES test_overlay_service.cpp)
oap_add_test(test_notification_service SOURCES test_notification_service.cpp)
oap_add_test(test_data_registry SOURCES test_data_registry.cpp)
oap_add_test(test_clock_sync SOURCES test_clock_sync.cpp)
oap_add_test(test_system_service_client SOURCES test_system_service_client.cpp)
oap_add_test(test_bluetooth_manager DBUS_SESSION SOURCES test_bluetooth_manager.cpp)
oap_add_test(test_projection_status_provider SOURCES test_projection_status_provider.cpp)
oap_add_test(test_phone_state_service SOURCES test_phone_state_service.cpp)
oap_add_test(test_phone_plugin SOURCES test_phone_plugin.cpp)
oap_add_test(test_telephony_client SOURCES test_telephony_client.cpp)
oap_add_test(test_sco_node_monitor SOURCES test_sco_node_monitor.cpp)
oap_add_test(test_call_audio_policy SOURCES test_call_audio_policy.cpp)
oap_add_test(test_media_status_service SOURCES test_media_status_service.cpp)

# --- Device / hardware tests ---

oap_add_test(test_night_mode SOURCES test_night_mode.cpp)
oap_add_test(test_input_device_scanner SOURCES test_input_device_scanner.cpp)
oap_add_test(test_device_registry SOURCES test_device_registry.cpp)
oap_add_test(test_paired_devices_model SOURCES test_paired_devices_model.cpp)

# --- Audio pipeline tests ---

oap_add_test(test_audio_ring_buffer SOURCES test_audio_ring_buffer.cpp)
oap_add_test(test_focus_gain SOURCES test_focus_gain.cpp)

# --- AA protocol tests ---

oap_add_test(test_projected_display_config SOURCES test_projected_display_config.cpp)
oap_add_test(test_gal_version_policy SOURCES test_gal_version_policy.cpp)
oap_add_test(test_service_discovery_builder SOURCES test_service_discovery_builder.cpp)
oap_add_test(test_sensor_channel_handler SOURCES test_sensor_channel_handler.cpp)
oap_add_test(test_input_channel_handler SOURCES test_input_channel_handler.cpp)
oap_add_test(test_bluetooth_channel_handler SOURCES test_bluetooth_channel_handler.cpp)
oap_add_test(test_wifi_channel_handler SOURCES test_wifi_channel_handler.cpp)
oap_add_test(test_audio_channel_handler SOURCES test_audio_channel_handler.cpp)
oap_add_test(test_video_channel_handler SOURCES test_video_channel_handler.cpp)
oap_add_test(test_avinput_channel_handler SOURCES test_avinput_channel_handler.cpp)
oap_add_test(test_avinput_capture_bridge SOURCES test_avinput_capture_bridge.cpp)
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
oap_add_test(test_video_decoder SOURCES test_video_decoder.cpp)
oap_add_test(test_projected_display_session SOURCES test_projected_display_session.cpp)
set_tests_properties(test_projected_display_session
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# --- Touch routing tests ---

oap_add_test(test_touch_router SOURCES test_touch_router.cpp)
oap_add_test(test_evdev_coord_bridge SOURCES test_evdev_coord_bridge.cpp)
oap_add_test(test_evdev_touch_reader SOURCES test_evdev_touch_reader.cpp)

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
oap_add_test(test_aa_cluster_widget
    SOURCES test_aa_cluster_widget.cpp
    EXTRA_LIBS Qt6::Qml Qt6::Quick Qt6::Multimedia
    DEFS TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
set_tests_properties(test_aa_cluster_widget
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

configure_file(data/test_widget_config.yaml ${CMAKE_CURRENT_BINARY_DIR}/data/test_widget_config.yaml COPYONLY)
oap_add_test(test_widget_placement_model SOURCES test_widget_placement_model.cpp)

oap_add_test(test_widget_config
    SOURCES test_widget_config.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)

oap_add_test(test_grid_dimensions SOURCES test_grid_dimensions.cpp)
oap_add_test(test_widget_grid_model SOURCES test_widget_grid_model.cpp)
oap_add_test(test_dashboard_manager SOURCES test_dashboard_manager.cpp)
oap_add_test(test_widget_contract_qml
    SOURCES test_widget_contract_qml.cpp
    EXTRA_LIBS Qt6::Quick
)
oap_add_test(test_widget_grid_remap SOURCES test_widget_grid_remap.cpp)
oap_add_test(test_widget_picker_model SOURCES test_widget_picker_model.cpp)
oap_add_test(test_web_widget_manifest SOURCES test_web_widget_manifest.cpp)
oap_add_test(test_web_widget_resolver SOURCES test_web_widget_resolver.cpp)
oap_add_test(test_web_widget_scanner SOURCES test_web_widget_scanner.cpp)

oap_add_test(test_weather_service SOURCES test_weather_service.cpp)

# --- Media bridge tests ---

oap_add_test(test_android_auto_runtime_bridge SOURCES test_android_auto_runtime_bridge.cpp)
oap_add_test(test_gesture_overlay_controller SOURCES test_gesture_overlay_controller.cpp)

oap_add_test(test_media_data_bridge SOURCES test_media_data_bridge.cpp)

# --- Navigation bridge tests ---

oap_add_test(test_navigation_data_bridge SOURCES test_navigation_data_bridge.cpp)
oap_add_test(test_navigation_lane_model SOURCES test_navigation_lane_model.cpp)
oap_add_test(test_maneuver_icon_provider SOURCES test_maneuver_icon_provider.cpp)

# --- External API proto tests ---

oap_add_test(test_api_proto_roundtrip
    SOURCES test_api_proto_roundtrip.cpp
    EXTRA_LIBS prodigy-api-proto)

oap_add_test(test_api_framing SOURCES test_api_framing.cpp)
oap_add_test(test_api_auth SOURCES test_api_auth.cpp)
oap_add_test(test_api_pairing SOURCES test_api_pairing.cpp)
oap_add_test(test_api_serializers SOURCES test_api_serializers.cpp)
oap_add_test(test_api_transports SOURCES test_api_transports.cpp)
set_tests_properties(test_api_transports PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
oap_add_test(test_api_publishers SOURCES test_api_publishers.cpp)
set_tests_properties(test_api_publishers PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
oap_add_test(test_api_session SOURCES test_api_session.cpp)
set_tests_properties(test_api_session PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
oap_add_test(test_api_request_handlers SOURCES test_api_request_handlers.cpp)
set_tests_properties(test_api_request_handlers PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
oap_add_test(test_api_server SOURCES test_api_server.cpp)
set_tests_properties(test_api_server PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
oap_add_test(test_api_loopback SOURCES test_api_loopback.cpp)
set_tests_properties(test_api_loopback PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# --- Script tests (no compilation) ---

add_test(
    NAME test_prebuilt_release_package
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/test_prebuilt_release_package.py
)
add_test(
    NAME test_prebuilt_upgrade_transaction
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/test_prebuilt_upgrade_transaction.py
)
add_test(
    NAME test_install_list_prebuilt
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/test_install_list_prebuilt.py
)
add_test(
    NAME test_install_source_lifecycle
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/test_install_source_lifecycle.py
)
add_test(
    NAME test_installer_hardware_contracts
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/test_installer_hardware_contracts.py
)
add_test(
    NAME test_validate_resolutions
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/test_validate_resolutions.py
)
add_test(
    NAME test_ops_install_contracts
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/test_ops_install_contracts.py
)
add_test(
    NAME test_ops_systemd_lifecycle
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/test_ops_systemd_lifecycle.py
)
set_tests_properties(test_ops_systemd_lifecycle PROPERTIES SKIP_RETURN_CODE 77)

# GUI/Quick tests need offscreen platform when no display is available
set_tests_properties(test_theme_service test_display_info test_display_service test_plugin_model test_plugin_view_host test_aa_orchestrator
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
set_tests_properties(test_settings_input_boundary test_settings_menu_structure test_gesture_overlay_controller test_widget_contract_qml
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
