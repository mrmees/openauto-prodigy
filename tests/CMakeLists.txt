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

add_executable(test_theme_controller
    test_theme_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/ThemeController.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Configuration.cpp
)

target_include_directories(test_theme_controller PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_theme_controller PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

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

enable_testing()
add_test(NAME test_configuration COMMAND test_configuration)
add_test(NAME test_theme_controller COMMAND test_theme_controller)
add_test(NAME test_yaml_config COMMAND test_yaml_config)
