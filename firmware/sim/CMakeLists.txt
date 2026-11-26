# The Arcade's WebAssembly module: the firmware's lamps component and lighting driver
# from firmware/src, with the simulated output stage of fakes/ in place of the FlexIO2 one.
# Built with Emscripten through emcmake, as tools/arcade's "npm run wasm" runs it.
cmake_minimum_required(VERSION 3.24)
project(pinball_simulator CXX)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "The simulator builds with Emscripten: emcmake cmake ...")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)
set(FIRMWARE ${CMAKE_CURRENT_SOURCE_DIR}/..)

# FastLED at the version platformio.ini pins
include(FetchContent)
FetchContent_Declare(fastled
    GIT_REPOSITORY https://github.com/FastLED/FastLED.git
    GIT_TAG 3.10.10
    GIT_SHALLOW TRUE
    # the simulator lists FastLED's files itself, so FastLED's own CMakeLists stays unread
    SOURCE_SUBDIR none)
FetchContent_MakeAvailable(fastled)
set(FASTLED_SRC ${fastled_SOURCE_DIR}/src)

# FastLED builds as a few files that each include a part of the library
add_library(fastled STATIC
    ${FASTLED_SRC}/fl/build/src.cpp
    ${FASTLED_SRC}/fl/build/fl.stl+.cpp
    ${FASTLED_SRC}/fl/build/fl.gfx+.cpp
    ${FASTLED_SRC}/fl/build/fl.math+.cpp
    ${FASTLED_SRC}/fl/build/fl.system+.cpp
    ${FASTLED_SRC}/fl/build/fl.log+.cpp
    ${FASTLED_SRC}/fl/build/fl.channels+.cpp
    ${FASTLED_SRC}/fl/build/fl.task+.cpp)
target_include_directories(fastled PUBLIC ${FASTLED_SRC})
# the firmware's fake Arduino.h gives Serial, so FastLED leaves its Arduino emulation out
target_compile_definitions(fastled PUBLIC FASTLED_NO_ARDUINO_STUBS)
target_compile_options(fastled PRIVATE -w -O2)

add_executable(simulator
    simulator.cpp
    fastled-platform.cpp
    ${FIRMWARE}/src/components/lamps.cpp
    ${FIRMWARE}/src/components/lamp-effects.cpp
    ${FIRMWARE}/src/drivers/lighting.cpp
    ${FIRMWARE}/src/logging/debug-log.cpp
    ${FIRMWARE}/src/logging/debug-output.cpp
    ${FIRMWARE}/src/logging/debug-trace.cpp
    ${FIRMWARE}/src/logging/log-line.cpp
    ${FIRMWARE}/src/logging/logger.cpp
    ${FIRMWARE}/src/logging/warning-list.cpp
    ${FIRMWARE}/src/io/card-appender.cpp)
# A quoted include looks in fakes/ before src/, so the simulated output stage takes the place of
# the FlexIO2 one, as the computer tests' fakes do.
target_compile_options(simulator PRIVATE
    -iquote ${CMAKE_CURRENT_SOURCE_DIR}/fakes
    -Wall -Wextra -Werror -fno-exceptions
    # optimized, so the module loads and runs fast, with the firmware's DEBUG_LOG() on as in its
    # debug build, which never defines NDEBUG
    -O2)
target_include_directories(simulator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/fakes ${FIRMWARE}/src)
target_compile_definitions(simulator PRIVATE DEBUG)
target_link_libraries(simulator PRIVATE fastled)
target_link_options(simulator PRIVATE
    -lembind
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    -sEXPORT_NAME=createSimulator
    -sENVIRONMENT=web,node
    -sALLOW_MEMORY_GROWTH=1
    -O2
    -fno-exceptions)
set_target_properties(simulator PROPERTIES SUFFIX ".mjs")
