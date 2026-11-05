cmake_minimum_required(VERSION 3.28)

project(BatteryGuard
    VERSION 1.0.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(batteryguard_host
    src/main.c
    src/sensor_sim.c
    src/battery.c
    src/protocol.c
)

target_include_directories(batteryguard_host PRIVATE
    include
)

target_compile_options(batteryguard_host PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

target_link_libraries(batteryguard_host PRIVATE m)


# Automated unit tests
enable_testing()

add_executable(batteryguard_tests
    tests/test_battery.c
    src/battery.c
)

target_include_directories(batteryguard_tests PRIVATE
    include
)

target_compile_options(batteryguard_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

target_link_libraries(batteryguard_tests PRIVATE m)

add_test(
    NAME batteryguard_protection_tests
    COMMAND batteryguard_tests
)

# Protocol unit tests
add_executable(batteryguard_protocol_tests
    tests/test_protocol.c
    src/protocol.c
)

target_include_directories(batteryguard_protocol_tests PRIVATE
    include
)

target_compile_options(batteryguard_protocol_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(
    NAME batteryguard_protocol_tests
    COMMAND batteryguard_protocol_tests
)