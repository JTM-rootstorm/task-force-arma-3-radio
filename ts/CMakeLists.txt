cmake_minimum_required(VERSION 3.16)
project(tfar_linux_bridge LANGUAGES CXX)

option(TFAR_LINUX_PLUGIN "Build the experimental native Linux TeamSpeak bridge plugin skeleton" ON)
option(TFAR_BRIDGE_PROTOCOL_TESTS "Build TFAR bridge protocol smoke tests" ON)
option(TFAR_REQUIRE_BRIDGE_TOKEN "Require bridge token in the Linux bridge server" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(TFAR_BRIDGE_PROTOCOL_TESTS)
    enable_testing()
    add_executable(tfar_bridge_protocol_smoke
        ../tests/bridge_protocol_smoke.cpp
    )
    target_include_directories(tfar_bridge_protocol_smoke PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    add_test(NAME tfar_bridge_protocol_smoke COMMAND tfar_bridge_protocol_smoke)
endif()

if(TFAR_LINUX_PLUGIN)
    if(WIN32)
        message(FATAL_ERROR "TFAR_LINUX_PLUGIN is for native Linux builds. Use the Visual Studio projects for Windows.")
    endif()

    add_library(TFAR_linux_x64 SHARED
        src/linux/LinuxPluginEntry.cpp
        src/platform/Platform.cpp
        src/transport/LinuxBridgeServer.cpp
    )
    target_compile_definitions(TFAR_linux_x64 PRIVATE
        TFAR_LINUX_PLUGIN=1
        $<$<BOOL:${TFAR_REQUIRE_BRIDGE_TOKEN}>:TFAR_REQUIRE_BRIDGE_TOKEN=1>
    )
    target_compile_options(TFAR_linux_x64 PRIVATE
        -fPIC
        -fvisibility=hidden
    )
    target_include_directories(TFAR_linux_x64 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    target_link_libraries(TFAR_linux_x64 PRIVATE pthread)
    set_target_properties(TFAR_linux_x64 PROPERTIES
        PREFIX ""
        OUTPUT_NAME "TFAR_linux_x64"
        SUFFIX ".so"
    )
endif()
