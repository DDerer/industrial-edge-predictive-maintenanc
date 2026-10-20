add_executable(edge_collector
    src/main.cpp
    src/protocol.cpp
)

target_include_directories(edge_collector PRIVATE include)
target_compile_options(edge_collector PRIVATE -Wall -Wextra -Wpedantic -Wconversion)

install(TARGETS edge_collector RUNTIME DESTINATION bin)

if(BUILD_TESTING)
    add_executable(protocol_test
        tests/protocol_test.cpp
        src/protocol.cpp
    )
    target_include_directories(protocol_test PRIVATE include)
    target_compile_options(protocol_test PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    add_test(NAME protocol_stream_parser COMMAND protocol_test)
endif()
