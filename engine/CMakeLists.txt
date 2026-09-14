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
set(RUBBERBAND_LIB_DIR ${RUBBERBAND_SOURCE_DIR}/otherbuilds/x64)

# RubberBand's vcxproj hardcodes PlatformToolset v142 (VS 2019). Override it with
# the toolset of the compiler CMake is actually using, so it builds on any VS.
# Debug engine builds need the Debug library: mixing /MDd and /MD fails to link
# with LNK2038 (_ITERATOR_DEBUG_LEVEL / RuntimeLibrary mismatch).
ExternalProject_Add(
    rubberband_external
    SOURCE_DIR ${RUBBERBAND_SOURCE_DIR}
    CONFIGURE_COMMAND ""   # No CMake configure step
    BUILD_COMMAND
        msbuild otherbuilds/rubberband-library.vcxproj
        /p:Configuration=$<IF:$<CONFIG:Debug>,Debug,Release>
        /p:Platform=x64
        /p:PlatformToolset=v${MSVC_TOOLSET_VERSION}
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
)

add_library(rubberband STATIC IMPORTED GLOBAL)

set_target_properties(rubberband PROPERTIES
        IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
        IMPORTED_LOCATION_DEBUG ${RUBBERBAND_LIB_DIR}/Debug/rubberband-library.lib
        IMPORTED_LOCATION_RELEASE ${RUBBERBAND_LIB_DIR}/Release/rubberband-library.lib
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        MAP_IMPORTED_CONFIG_MINSIZEREL Release
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
