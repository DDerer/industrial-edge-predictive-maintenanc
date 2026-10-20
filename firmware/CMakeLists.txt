add_library(edge_firmware_protocol STATIC src/edge_protocol.c)
target_include_directories(edge_firmware_protocol PUBLIC include)
set_target_properties(edge_firmware_protocol PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
target_compile_options(edge_firmware_protocol PRIVATE -Wall -Wextra -Wpedantic -Wconversion)

if(BUILD_TESTING)
    add_executable(firmware_protocol_test
        tests/protocol_test.cpp
        ../gateway/src/protocol.cpp
    )
    target_include_directories(firmware_protocol_test PRIVATE ../gateway/include)
    target_link_libraries(firmware_protocol_test PRIVATE edge_firmware_protocol)
    target_compile_options(firmware_protocol_test PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    add_test(NAME firmware_protocol_compatibility COMMAND firmware_protocol_test)

    add_executable(firmware_uart_demo_test
        tests/uart_demo_test.cpp
        examples/edge_uart_demo.c
        ../gateway/src/protocol.cpp
    )
    target_include_directories(firmware_uart_demo_test PRIVATE
        tests examples ../gateway/include)
    target_link_libraries(firmware_uart_demo_test PRIVATE edge_firmware_protocol)
    target_compile_options(firmware_uart_demo_test PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    add_test(NAME firmware_uart_demo COMMAND firmware_uart_demo_test)
endif()
