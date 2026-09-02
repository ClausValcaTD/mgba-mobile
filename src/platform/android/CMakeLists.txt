cmake_minimum_required(VERSION 3.22)

project(mGBAMobile)

set(ANDROID_NATIVE_API_LEVEL 24)

set(MGBA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

# Build SDL2
add_subdirectory(
    ${MGBA_ROOT}/third_party/SDL2
    SDL2-build
)

# Build mGBA core
set(MGBA_DISABLE_FRONTENDS ON CACHE BOOL "" FORCE)
set(BUILD_SDL OFF CACHE BOOL "" FORCE)
set(BUILD_QT OFF CACHE BOOL "" FORCE)
add_subdirectory(${MGBA_ROOT} mgba-core-build)

# Our Android library
add_library(mgba-mobile SHARED
    android-main.c
    android-jni.c
)

target_include_directories(mgba-mobile PRIVATE
    ${MGBA_ROOT}/include
    ${MGBA_ROOT}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay
    ${MGBA_ROOT}/third_party/SDL2/include
)

target_link_libraries(mgba-mobile
    mgba
    SDL2
    android
    log
)
