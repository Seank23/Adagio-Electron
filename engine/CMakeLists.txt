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

if (MSVC)
    add_compile_options(/fp:fast /arch:AVX2 /permissive)
endif()

target_include_directories(AdagioEngine PRIVATE include ${kfr_INCLUDE_DIRS} build/_deps/ixwebsocket-src)
target_link_libraries(AdagioEngine PRIVATE kfr_dft kfr_dsp kfr_io ixwebsocket)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES ${ENGINE_SOURCES})
