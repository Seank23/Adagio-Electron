cmake_minimum_required(VERSION 3.20)
project(AdagioEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.h
)

add_executable(AdagioEngine ${ENGINE_SOURCES})

FetchContent_Declare(
  kfr
  GIT_REPOSITORY https://github.com/kfrlib/kfr.git
  GIT_TAG        6.3.1
)
FetchContent_MakeAvailable(kfr)

FetchContent_Declare(
  ixwebsocket
  GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(ixwebsocket)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

include(ExternalProject)

set(RUBBERBAND_SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/rubberband)

ExternalProject_Add(
    rubberband_external
    SOURCE_DIR ${RUBBERBAND_SOURCE_DIR}
    CONFIGURE_COMMAND ""   # No CMake configure step
    BUILD_COMMAND
        msbuild otherbuilds/rubberband-library.vcxproj
        /p:Configuration=Release
        /p:Platform=x64
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
)

add_library(rubberband STATIC IMPORTED GLOBAL)

set_target_properties(rubberband PROPERTIES
        IMPORTED_LOCATION
        ${RUBBERBAND_SOURCE_DIR}/otherbuilds/x64/Release/rubberband-library.lib
        INTERFACE_INCLUDE_DIRECTORIES
        ${RUBBERBAND_SOURCE_DIR}
    )
add_dependencies(rubberband rubberband_external)

if (MSVC)
    add_compile_options(/fp:fast /arch:AVX2 /permissive)
endif()

target_include_directories(AdagioEngine PRIVATE include ${kfr_INCLUDE_DIRS} build/_deps/ixwebsocket-src)
target_link_libraries(AdagioEngine PRIVATE kfr_dft kfr_dsp kfr_io ixwebsocket nlohmann_json::nlohmann_json rubberband)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES ${ENGINE_SOURCES})
