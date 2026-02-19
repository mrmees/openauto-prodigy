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
    Boost::log
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
    Boost::log
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
    Boost::log
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

enable_testing()
add_test(NAME test_configuration COMMAND test_configuration)
add_test(NAME test_yaml_config COMMAND test_yaml_config)
add_test(NAME test_config_service COMMAND test_config_service)
add_test(NAME test_plugin_manifest COMMAND test_plugin_manifest)
add_test(NAME test_plugin_discovery COMMAND test_plugin_discovery)
add_test(NAME test_plugin_manager COMMAND test_plugin_manager)
add_test(NAME test_theme_service COMMAND test_theme_service)
