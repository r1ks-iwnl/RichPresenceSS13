cmake_minimum_required(VERSION 3.30)
project(RichPresenceSS13 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

# Main Executable
add_executable(RPSS13 
    src/main.cpp
    src/server.cpp
    src/discord_rpc.cpp
    src/game_detector.cpp
    src/byond_topic.cpp
)

target_include_directories(RPSS13 PRIVATE src)

target_link_libraries(RPSS13 PRIVATE 
    nlohmann_json::nlohmann_json
    ws2_32
    iphlpapi
)

# Test Suite
enable_testing()

add_executable(RPSS13_Tests
    tests/test_main.cpp
    src/server.cpp
    src/discord_rpc.cpp
    src/game_detector.cpp
    src/byond_topic.cpp
)

target_include_directories(RPSS13_Tests PRIVATE src)

target_link_libraries(RPSS13_Tests PRIVATE 
    nlohmann_json::nlohmann_json
    ws2_32
    iphlpapi
)

add_test(NAME RPSS13_Tests COMMAND RPSS13_Tests)
