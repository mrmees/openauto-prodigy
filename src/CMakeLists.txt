qt_add_executable(openauto-prodigy
    main.cpp
)

qt_add_qml_module(openauto-prodigy
    URI OpenAutoProdigy
    VERSION 1.0
    QML_FILES
        ../qml/main.qml
    RESOURCES
        ../resources/resources.qrc
)

target_link_libraries(openauto-prodigy PRIVATE
    Qt6::Core
    Qt6::Quick
)
