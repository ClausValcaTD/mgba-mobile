cmake_minimum_required(VERSION 3.22)
project(mGBAMobile)

set(ANDROID_NATIVE_API_LEVEL 24)
set(MGBA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

set(USE_SQLITE3 OFF CACHE BOOL "" FORCE)
set(USE_DISCORD_RPC OFF CACHE BOOL "" FORCE)
set(BUILD_SDL OFF CACHE BOOL "" FORCE)
set(BUILD_QT OFF CACHE BOOL "" FORCE)
set(MGBA_DISABLE_FRONTENDS ON CACHE BOOL "" FORCE)

add_subdirectory(${MGBA_ROOT}/third_party/SDL2 SDL2-build)
add_subdirectory(${MGBA_ROOT} mgba-core-build)

add_library(mgba-mobile SHARED
    android-main.c
    android-jni.c
    ${MGBA_ROOT}/src/platform/sdl/sdl-events.c
    ${MGBA_ROOT}/src/platform/sdl/sdl-audio.c
    ${MGBA_ROOT}/src/platform/sdl/sw-sdl2.c
    ${MGBA_ROOT}/src/platform/sdl/gl-common.c
)

target_include_directories(mgba-mobile PRIVATE
    ${MGBA_ROOT}/include
    ${MGBA_ROOT}/src
    ${MGBA_ROOT}/src/platform/sdl
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay
    ${MGBA_ROOT}/third_party/SDL2/include
)

target_link_libraries(mgba-mobile mgba SDL2 android log)
