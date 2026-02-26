find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_configuration
    test_configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Configuration.cpp
)

target_include_directories(test_configuration PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(test_configuration PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)

target_link_libraries(test_configuration PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

# Copy test data
configure_file(data/test_config.ini ${CMAKE_CURRENT_BINARY_DIR}/data/test_config.ini COPYONLY)

add_executable(test_yaml_config
    test_yaml_config.cpp
    ${CMAKE_SOURCE_DIR}/src/core/YamlConfig.cpp
)
target_include_directories(test_yaml_config PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(test_yaml_config PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
target_link_libraries(test_yaml_config PRIVATE
    Qt6::Core
    Qt6::Test
    yaml-cpp::yaml-cpp
)
configure_file(data/test_config.yaml ${CMAKE_CURRENT_BINARY_DIR}/data/test_config.yaml COPYONLY)

add_executable(test_config_service
    test_config_service.cpp
    ${CMAKE_SOURCE_DIR}/src/core/YamlConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/core/services/ConfigService.cpp
)
target_include_directories(test_config_service PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_config_service PRIVATE
    Qt6::Core
    Qt6::Test
    yaml-cpp::yaml-cpp
)

add_executable(test_plugin_manifest
    test_plugin_manifest.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin/PluginManifest.cpp
)
target_include_directories(test_plugin_manifest PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(test_plugin_manifest PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
target_link_libraries(test_plugin_manifest PRIVATE
    Qt6::Core
    Qt6::Test
    yaml-cpp::yaml-cpp

)
configure_file(data/test_plugin.yaml ${CMAKE_CURRENT_BINARY_DIR}/data/test_plugin.yaml COPYONLY)

add_executable(test_plugin_discovery
    test_plugin_discovery.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin/PluginDiscovery.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin/PluginManifest.cpp
)
target_include_directories(test_plugin_discovery PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_plugin_discovery PRIVATE
    Qt6::Core
    Qt6::Test
    yaml-cpp::yaml-cpp

)

add_executable(test_plugin_manager
    test_plugin_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin/PluginManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin/PluginDiscovery.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin/PluginLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin/PluginManifest.cpp
)
target_include_directories(test_plugin_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_plugin_manager PRIVATE
    Qt6::Core
    Qt6::Test
    yaml-cpp::yaml-cpp

)

add_executable(test_theme_service
    test_theme_service.cpp
    ${CMAKE_SOURCE_DIR}/src/core/services/ThemeService.cpp
)
target_include_directories(test_theme_service PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(test_theme_service PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
target_link_libraries(test_theme_service PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    yaml-cpp::yaml-cpp
)
file(COPY data/themes DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data)

add_executable(test_audio_service
    test_audio_service.cpp
    ${CMAKE_SOURCE_DIR}/src/core/services/AudioService.cpp
    ${CMAKE_SOURCE_DIR}/src/core/audio/PipeWireDeviceRegistry.cpp
)
target_include_directories(test_audio_service PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${PIPEWIRE_INCLUDE_DIRS}
)
target_link_libraries(test_audio_service PRIVATE
    Qt6::Core
    Qt6::Test
    ${PIPEWIRE_LIBRARIES}
)

add_executable(test_night_mode
    test_night_mode.cpp
    ${CMAKE_SOURCE_DIR}/src/core/aa/NightModeProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/core/aa/TimedNightMode.cpp
)
target_include_directories(test_night_mode PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_night_mode PRIVATE
    Qt6::Core
    Qt6::Test

)

add_executable(test_input_device_scanner
    test_input_device_scanner.cpp
    ${CMAKE_SOURCE_DIR}/src/core/InputDeviceScanner.cpp
)
target_include_directories(test_input_device_scanner PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_input_device_scanner PRIVATE
    Qt6::Core
    Qt6::Test
)

add_executable(test_plugin_model
    test_plugin_model.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PluginModel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PluginRuntimeContext.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PluginViewHost.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin/PluginManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin/PluginDiscovery.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin/PluginLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin/PluginManifest.cpp
)
target_include_directories(test_plugin_model PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_plugin_model PRIVATE
    Qt6::Core Qt6::Quick Qt6::Test yaml-cpp::yaml-cpp)

add_executable(test_notification_service
    test_notification_service.cpp
    ${CMAKE_SOURCE_DIR}/src/core/services/NotificationService.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/NotificationModel.cpp
)
target_include_directories(test_notification_service PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_notification_service PRIVATE Qt6::Core Qt6::Test)

add_executable(test_action_registry
    test_action_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/services/ActionRegistry.cpp
)
target_include_directories(test_action_registry PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_action_registry PRIVATE Qt6::Core Qt6::Test)

add_executable(test_event_bus
    test_event_bus.cpp
    ${CMAKE_SOURCE_DIR}/src/core/services/EventBus.cpp
)
target_include_directories(test_event_bus PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_event_bus PRIVATE Qt6::Core Qt6::Test)

add_executable(test_audio_ring_buffer
    test_audio_ring_buffer.cpp
)
target_include_directories(test_audio_ring_buffer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${PIPEWIRE_INCLUDE_DIRS}
)
target_link_libraries(test_audio_ring_buffer PRIVATE
    Qt6::Core
    Qt6::Test
    ${PIPEWIRE_LIBRARIES}
)

add_executable(test_device_registry
    test_device_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/audio/PipeWireDeviceRegistry.cpp
)
target_include_directories(test_device_registry PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${PIPEWIRE_INCLUDE_DIRS}
)
target_link_libraries(test_device_registry PRIVATE
    Qt6::Core
    Qt6::Test
    ${PIPEWIRE_LIBRARIES}
)

add_executable(test_config_key_coverage
    test_config_key_coverage.cpp
    ${CMAKE_SOURCE_DIR}/src/core/YamlConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/core/services/ConfigService.cpp
)
target_include_directories(test_config_key_coverage PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_config_key_coverage PRIVATE
    Qt6::Core
    Qt6::Test
    yaml-cpp::yaml-cpp
)

enable_testing()
add_test(NAME test_night_mode COMMAND test_night_mode)
add_test(NAME test_configuration COMMAND test_configuration)
add_test(NAME test_yaml_config COMMAND test_yaml_config)
add_test(NAME test_config_service COMMAND test_config_service)
add_test(NAME test_plugin_manifest COMMAND test_plugin_manifest)
add_test(NAME test_plugin_discovery COMMAND test_plugin_discovery)
add_test(NAME test_plugin_manager COMMAND test_plugin_manager)
add_test(NAME test_theme_service COMMAND test_theme_service)
add_test(NAME test_audio_service COMMAND test_audio_service)
add_test(NAME test_input_device_scanner COMMAND test_input_device_scanner)
add_test(NAME test_plugin_model COMMAND test_plugin_model)
add_test(NAME test_event_bus COMMAND test_event_bus)
add_test(NAME test_action_registry COMMAND test_action_registry)
add_test(NAME test_notification_service COMMAND test_notification_service)
add_test(NAME test_audio_ring_buffer COMMAND test_audio_ring_buffer)
add_test(NAME test_device_registry COMMAND test_device_registry)
add_executable(test_companion_listener
    test_companion_listener.cpp
    ${CMAKE_SOURCE_DIR}/src/core/services/CompanionListenerService.cpp
)
target_include_directories(test_companion_listener PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_companion_listener PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

add_test(NAME test_config_key_coverage COMMAND test_config_key_coverage)
add_test(NAME test_companion_listener COMMAND test_companion_listener)

# Service discovery builder — uses oaa protos + YamlConfig for resolution/sidebar config
add_executable(test_service_discovery_builder
    test_service_discovery_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/core/aa/ServiceDiscoveryBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/core/YamlConfig.cpp
)
target_include_directories(test_service_discovery_builder PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/open-androidauto/include
    ${CMAKE_BINARY_DIR}/libs/open-androidauto
)
target_link_libraries(test_service_discovery_builder PRIVATE
    Qt6::Core
    Qt6::Test
    open-androidauto
    yaml-cpp::yaml-cpp
)
add_test(NAME test_service_discovery_builder COMMAND test_service_discovery_builder)

# Sensor channel handler — uses oaa IChannelHandler + sensor protos
add_executable(test_sensor_channel_handler
    test_sensor_channel_handler.cpp
)
target_include_directories(test_sensor_channel_handler PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/open-androidauto/include
    ${CMAKE_BINARY_DIR}/libs/open-androidauto
)
target_link_libraries(test_sensor_channel_handler PRIVATE
    Qt6::Core
    Qt6::Test
    open-androidauto
)
add_test(NAME test_sensor_channel_handler COMMAND test_sensor_channel_handler)

# Input channel handler — touch events + binding requests
add_executable(test_input_channel_handler
    test_input_channel_handler.cpp
)
target_include_directories(test_input_channel_handler PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/open-androidauto/include
    ${CMAKE_BINARY_DIR}/libs/open-androidauto
)
target_link_libraries(test_input_channel_handler PRIVATE
    Qt6::Core
    Qt6::Test
    open-androidauto
)
add_test(NAME test_input_channel_handler COMMAND test_input_channel_handler)

# Bluetooth channel handler — pairing requests
add_executable(test_bluetooth_channel_handler
    test_bluetooth_channel_handler.cpp
)
target_include_directories(test_bluetooth_channel_handler PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/open-androidauto/include
    ${CMAKE_BINARY_DIR}/libs/open-androidauto
)
target_link_libraries(test_bluetooth_channel_handler PRIVATE
    Qt6::Core
    Qt6::Test
    open-androidauto
)
add_test(NAME test_bluetooth_channel_handler COMMAND test_bluetooth_channel_handler)

# WiFi channel handler — security/credential requests
add_executable(test_wifi_channel_handler
    test_wifi_channel_handler.cpp
)
target_include_directories(test_wifi_channel_handler PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/open-androidauto/include
    ${CMAKE_BINARY_DIR}/libs/open-androidauto
)
target_link_libraries(test_wifi_channel_handler PRIVATE
    Qt6::Core
    Qt6::Test
    open-androidauto
)
add_test(NAME test_wifi_channel_handler COMMAND test_wifi_channel_handler)

# Integration test — non-AV channels with AASession + ReplayTransport
add_executable(test_oaa_integration
    test_oaa_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/core/aa/ServiceDiscoveryBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/core/YamlConfig.cpp
)
target_include_directories(test_oaa_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/open-androidauto/include
    ${CMAKE_BINARY_DIR}/libs/open-androidauto
)
target_link_libraries(test_oaa_integration PRIVATE
    Qt6::Core
    Qt6::Test
    open-androidauto
    yaml-cpp::yaml-cpp
)
add_test(NAME test_oaa_integration COMMAND test_oaa_integration)

# Audio channel handler — generic AV handler for media/speech/system
add_executable(test_audio_channel_handler
    test_audio_channel_handler.cpp
)
target_include_directories(test_audio_channel_handler PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/open-androidauto/include
    ${CMAKE_BINARY_DIR}/libs/open-androidauto
)
target_link_libraries(test_audio_channel_handler PRIVATE
    Qt6::Core
    Qt6::Test
    open-androidauto
)
add_test(NAME test_audio_channel_handler COMMAND test_audio_channel_handler)

# Video channel handler — H.264 frames + focus management
add_executable(test_video_channel_handler
    test_video_channel_handler.cpp
)
target_include_directories(test_video_channel_handler PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/open-androidauto/include
    ${CMAKE_BINARY_DIR}/libs/open-androidauto
)
target_link_libraries(test_video_channel_handler PRIVATE
    Qt6::Core
    Qt6::Test
    open-androidauto
)
add_test(NAME test_video_channel_handler COMMAND test_video_channel_handler)

# AVInput channel handler — microphone upstream
add_executable(test_avinput_channel_handler
    test_avinput_channel_handler.cpp
)
target_include_directories(test_avinput_channel_handler PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/open-androidauto/include
    ${CMAKE_BINARY_DIR}/libs/open-androidauto
)
target_link_libraries(test_avinput_channel_handler PRIVATE
    Qt6::Core
    Qt6::Test
    open-androidauto
)
add_test(NAME test_avinput_channel_handler COMMAND test_avinput_channel_handler)

# AndroidAutoOrchestrator — lifecycle + state management
add_executable(test_aa_orchestrator
    test_aa_orchestrator.cpp
    ${CMAKE_SOURCE_DIR}/src/core/aa/AndroidAutoOrchestrator.cpp
    ${CMAKE_SOURCE_DIR}/src/core/aa/TouchHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/core/aa/ServiceDiscoveryBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/core/aa/VideoDecoder.cpp
    ${CMAKE_SOURCE_DIR}/src/core/aa/DmaBufVideoBuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/aa/NightModeProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/core/aa/TimedNightMode.cpp
    ${CMAKE_SOURCE_DIR}/src/core/aa/GpioNightMode.cpp
    ${CMAKE_SOURCE_DIR}/src/core/YamlConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Configuration.cpp
)
target_include_directories(test_aa_orchestrator PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/open-androidauto/include
    ${CMAKE_BINARY_DIR}/libs/open-androidauto
    ${LIBAV_INCLUDE_DIRS}
)
target_link_libraries(test_aa_orchestrator PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Multimedia
    Qt6::Test
    open-androidauto
    yaml-cpp::yaml-cpp

    ${LIBAV_LIBRARIES}
)
add_test(NAME test_aa_orchestrator COMMAND test_aa_orchestrator)
set_tests_properties(test_aa_orchestrator PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(test_oap_protocol_logger
    test_oap_protocol_logger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/aa/ProtocolLogger.cpp
)
target_include_directories(test_oap_protocol_logger PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/open-androidauto/include
)
target_link_libraries(test_oap_protocol_logger PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME test_oap_protocol_logger COMMAND test_oap_protocol_logger)

add_executable(test_circular_buffer test_circular_buffer.cpp)
target_link_libraries(test_circular_buffer PRIVATE Qt6::Test open-androidauto)
add_test(NAME test_circular_buffer COMMAND test_circular_buffer)

# Codec capability detection — probes FFmpeg for hw/sw decoders
add_executable(test_codec_capability
    test_codec_capability.cpp
    ${CMAKE_SOURCE_DIR}/src/core/aa/CodecCapability.cpp
)
target_include_directories(test_codec_capability PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${LIBAV_INCLUDE_DIRS}
)
target_link_libraries(test_codec_capability PRIVATE
    Qt6::Core
    Qt6::Test
    ${LIBAV_LIBRARIES}
)
add_test(NAME test_codec_capability COMMAND test_codec_capability)

# Video frame pool — format management and allocation tracking
add_executable(test_video_frame_pool
    test_video_frame_pool.cpp
)
target_include_directories(test_video_frame_pool PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_video_frame_pool PRIVATE
    Qt6::Core
    Qt6::Multimedia
    Qt6::Test
)
add_test(NAME test_video_frame_pool COMMAND test_video_frame_pool)

# Video decode queue — keyframe detection and queue bounding
add_executable(test_video_decode_queue
    test_video_decode_queue.cpp
)
target_include_directories(test_video_decode_queue PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_video_decode_queue PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME test_video_decode_queue COMMAND test_video_decode_queue)

# GUI/Quick tests need offscreen platform when no display is available
set_tests_properties(test_configuration test_theme_service test_plugin_model
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
