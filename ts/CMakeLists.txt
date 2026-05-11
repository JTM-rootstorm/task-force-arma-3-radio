cmake_minimum_required(VERSION 3.16)
project(tfar_linux_bridge LANGUAGES C CXX)

option(TFAR_LINUX_PLUGIN "Build the experimental native Linux TeamSpeak bridge plugin" ON)
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

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/compat-include)
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/compat-include/DspFilters)
        file(CREATE_LINK
            ${CMAKE_CURRENT_SOURCE_DIR}/include/dspfilters
            ${CMAKE_CURRENT_BINARY_DIR}/compat-include/DspFilters
            SYMBOLIC)
    endif()

    set(TFAR_RUNTIME_SOURCES
        src/antennaManager.cpp
        src/clientData.cpp
        src/CommandProcessor.cpp
        src/datatypes.cpp
        src/DebugUI.cpp
        src/helpers.cpp
        src/Logger.cpp
        src/PlaybackHandler.cpp
        src/platform/Platform.cpp
        src/plugin.cpp
        src/profilers.cpp
        src/ProfilerTracy.cpp
        src/SampleBuffer.cpp
        src/serverData.cpp
        src/task_force_radio.cpp
        src/Teamspeak.cpp
        src/transport/LinuxBridgeServer.cpp
        ../ts/include/clunk/buffer.cpp
        ../ts/include/clunk/clunk_ex.cpp
        ../ts/include/clunk/clunk_logger.cpp
        ../ts/include/clunk/context.cpp
        ../ts/include/clunk/distance_model.cpp
        ../ts/include/clunk/hrtf.cpp
        ../ts/include/clunk/kemar.c
        ../ts/include/clunk/object.cpp
        ../ts/include/clunk/sample.cpp
        ../ts/include/clunk/source.cpp
        ../ts/include/clunk/sse_fft_context.cpp
        ../ts/include/clunk/stream.cpp
        ../ts/include/clunk/wav_file.cpp
        ../ts/include/dspfilters/source/Bessel.cpp
        ../ts/include/dspfilters/source/Biquad.cpp
        ../ts/include/dspfilters/source/Butterworth.cpp
        ../ts/include/dspfilters/source/Cascade.cpp
        ../ts/include/dspfilters/source/ChebyshevI.cpp
        ../ts/include/dspfilters/source/ChebyshevII.cpp
        ../ts/include/dspfilters/source/Custom.cpp
        ../ts/include/dspfilters/source/Design.cpp
        ../ts/include/dspfilters/source/Elliptic.cpp
        ../ts/include/dspfilters/source/Filter.cpp
        ../ts/include/dspfilters/source/Legendre.cpp
        ../ts/include/dspfilters/source/Param.cpp
        ../ts/include/dspfilters/source/PoleFilter.cpp
        ../ts/include/dspfilters/source/RBJ.cpp
        ../ts/include/dspfilters/source/RootFinder.cpp
        ../ts/include/dspfilters/source/State.cpp
        ../ts/include/simpleSource/SimpleComp.cpp
        ../ts/include/simpleSource/SimpleEnvelope.cpp
        ../ts/include/simpleSource/SimpleGate.cpp
        ../ts/include/simpleSource/SimpleLimit.cpp
        ../ts/include/sqlite3/sqlite3.c
    )

    add_library(TFAR_linux_x64 SHARED
        ${TFAR_RUNTIME_SOURCES}
    )
    target_compile_definitions(TFAR_linux_x64 PRIVATE
        TFAR_LINUX_PLUGIN=1
        X64BUILD=1
        CLUNKAPI=
        CLUNK_USES_SSE=1
        $<$<BOOL:${TFAR_REQUIRE_BRIDGE_TOKEN}>:TFAR_REQUIRE_BRIDGE_TOKEN=1>
    )
    target_compile_options(TFAR_linux_x64 PRIVATE
        -fPIC
        -fvisibility=hidden
        -fpermissive
        -msse2
    )
    target_include_directories(TFAR_linux_x64 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/dspfilters
        ${CMAKE_CURRENT_SOURCE_DIR}/include/simpleSource
        ${CMAKE_CURRENT_BINARY_DIR}/compat-include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    target_link_libraries(TFAR_linux_x64 PRIVATE pthread)
    set_target_properties(TFAR_linux_x64 PROPERTIES
        PREFIX ""
        OUTPUT_NAME "TFAR_linux_x64"
        SUFFIX ".so"
    )
endif()
