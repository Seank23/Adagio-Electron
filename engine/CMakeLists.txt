cmake_minimum_required(VERSION 3.20)
project(AdagioEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ADAGIO_BUILD_TESTS "Build the AdagioTests unit test target" ON)
option(ADAGIO_ENABLE_PROFILING "Write engine/build/Application_Profile.json at runtime" OFF)

include(FetchContent)

# The wire protocol is described once, in protocol/protocol.json, and read by both
# sides: this generates the C++ half of it, and app/ui imports the same file.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateProtocol.cmake)
set(ADAGIO_PROTOCOL_JSON ${CMAKE_CURRENT_SOURCE_DIR}/../protocol/protocol.json)
set(ADAGIO_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${ADAGIO_GENERATED_DIR})
adagio_generate_protocol_header(${ADAGIO_PROTOCOL_JSON} ${ADAGIO_GENERATED_DIR}/Protocol.generated.h)

# Everything except main.cpp goes into AdagioCore so the test target can link the
# engine without pulling in a second main(). AdagioEngine is the thin executable.
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.h
)
list(REMOVE_ITEM ENGINE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

add_library(AdagioCore STATIC ${ENGINE_SOURCES})
add_executable(AdagioEngine src/main.cpp)
target_link_libraries(AdagioEngine PRIVATE AdagioCore)

# kfr only defaults KFR_ENABLE_DFT to ON under Clang; for MSVC it is OFF, which
# drops the kfr_dft target entirely and leaves FFTProcessor's kfr::dft calls
# unresolved at link time. A fresh clone cannot build without this.
set(KFR_ENABLE_DFT ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  kfr
  GIT_REPOSITORY https://github.com/kfrlib/kfr.git
  GIT_TAG        6.3.1
)
FetchContent_MakeAvailable(kfr)

# kfr points every configuration's libraries at the same kfr-build/lib folder, so a
# Release build overwrites the Debug libs (or vice versa). MSBuild still considers
# them up to date, and the link fails with LNK2038. Give each configuration its own
# folder. Single-config generators (Ninja) use a build tree per config, so skip them.
function(separate_kfr_config_outputs dir)
    get_property(targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(type ${target} TYPE)
        if(type STREQUAL "STATIC_LIBRARY")
            foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
                string(TOUPPER ${config} config_upper)
                set_target_properties(${target} PROPERTIES
                    ARCHIVE_OUTPUT_DIRECTORY_${config_upper} ${kfr_BINARY_DIR}/lib/${config})
            endforeach()
        endif()
    endforeach()
    get_property(subdirs DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(subdir IN LISTS subdirs)
        separate_kfr_config_outputs(${subdir})
    endforeach()
endfunction()
separate_kfr_config_outputs(${kfr_SOURCE_DIR})

# We only use IXWebSocket's server over localhost, so permessage-deflate (and the
# zlib it drags in) buys us nothing. Without this, IXWebSocket's
# find_package(ZLIB REQUIRED) fails on Windows, where there is no system zlib.
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
set(USE_TLS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  ixwebsocket
  GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
  GIT_TAG        v12.0.1
)
FetchContent_MakeAvailable(ixwebsocket)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

# RubberBand has no CMake build, but it ships RubberBandSingle.cpp, one translation
# unit that compiles the whole library with its built-in FFT and resampler. Building
# that as our own static library needs no msbuild, no hand-copied checkout, and
# works with any generator and configuration. It must stay where it is in the
# source tree, because it includes the rest of the library by relative path.
FetchContent_Declare(
    rubberband
    GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
    GIT_TAG        v4.0.0
)
FetchContent_MakeAvailable(rubberband)

add_library(rubberband STATIC ${rubberband_SOURCE_DIR}/single/RubberBandSingle.cpp)
target_include_directories(rubberband PUBLIC ${rubberband_SOURCE_DIR})
if (MSVC)
    # Its own sources, not ours: keep the build log to our warnings.
    target_compile_options(rubberband PRIVATE /W0)
    # What its own Visual Studio project defines: windows.h's min/max macros break
    # std::min, and M_PI needs _USE_MATH_DEFINES.
    target_compile_definitions(rubberband PRIVATE __MSVC__ NOMINMAX _USE_MATH_DEFINES)
endif()

if (MSVC)
    # These have to be target options. add_compile_options() only feeds targets
    # created after the call, and add_executable() used to run 87 lines earlier,
    # so the flags never reached the engine (B1). PUBLIC so AdagioEngine and
    # AdagioTests compile with the same floating-point model as AdagioCore.
    target_compile_options(AdagioCore PUBLIC /fp:fast /arch:AVX2 /permissive-)

    # windows.h defines min and max as macros, which turn every kfr::min call
    # into a syntax error in whichever translation unit happens to reach a
    # Windows header first. Define it once rather than ordering includes by hand.
    target_compile_definitions(AdagioCore PUBLIC NOMINMAX)
endif()

# The profiler writes from every thread through one unguarded stream and never
# closes its session, so it stays off unless a build explicitly asks for it.
if (ADAGIO_ENABLE_PROFILING)
    target_compile_definitions(AdagioCore PUBLIC ADAGIO_PROFILE=1)
endif()

# ixwebsocket already exports its own include directory, so there is no need to
# hardcode a path under build/_deps (which only existed for one build tree).
target_include_directories(AdagioCore PUBLIC include ${ADAGIO_GENERATED_DIR} ${kfr_INCLUDE_DIRS})
target_link_libraries(AdagioCore PUBLIC kfr_dft kfr_dsp kfr_io ixwebsocket nlohmann_json::nlohmann_json rubberband)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES ${ENGINE_SOURCES})

if (ADAGIO_BUILD_TESTS)
    enable_testing()

    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.5.3
    )
    FetchContent_MakeAvailable(doctest)

    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
    add_executable(AdagioTests ${TEST_SOURCES})
    target_link_libraries(AdagioTests PRIVATE AdagioCore doctest::doctest)

    list(APPEND CMAKE_MODULE_PATH ${doctest_SOURCE_DIR}/scripts/cmake)
    include(doctest)
    doctest_discover_tests(AdagioTests)
endif()
