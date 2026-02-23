find_package(Qt6 REQUIRED COMPONENTS Test)

function(oaa_add_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE open-androidauto Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

oaa_add_test(test_version test_version.cpp)
oaa_add_test(test_tcp_transport test_tcp_transport.cpp)
