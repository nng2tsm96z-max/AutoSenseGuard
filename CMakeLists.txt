cmake_minimum_required(VERSION 3.16)
project(AutoSenseGuard VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(autosense_core
    src/diagnostic.cpp
    src/csv_logger.cpp
    src/protocol.cpp
)
target_include_directories(autosense_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(autosense_core PUBLIC Threads::Threads)

if(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(autosense_server apps/autosense_server.cpp)
    target_link_libraries(autosense_server PRIVATE autosense_core Threads::Threads)

    add_executable(sensor_simulator apps/sensor_simulator.cpp)
    target_link_libraries(sensor_simulator PRIVATE autosense_core Threads::Threads)
else()
    message(FATAL_ERROR "AutoSenseGuard v1.0 currently supports macOS and Linux (POSIX sockets).")
endif()

enable_testing()
add_executable(diagnostic_test tests/diagnostic_test.cpp)
target_link_libraries(diagnostic_test PRIVATE autosense_core)
add_test(NAME diagnostic_test COMMAND diagnostic_test)
