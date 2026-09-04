cmake_minimum_required(VERSION 3.22)
project(mGBAMobile)

set(ANDROID_NATIVE_API_LEVEL 26)

# Enable ZIP and 7z archive support (lets mGBA load ROMs from .zip/.7z natively)
set(USE_LZMA ON)
set(ENABLE_VFS ON)
set(ENABLE_DIRECTORIES ON)

# Pull in the mGBA core library from repo root
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    mgba-core-build
)

add_library(mgba-mobile SHARED
    android-main.c
    android-jni.c
)

target_include_directories(mgba-mobile PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# No SDL. ANativeWindow + AAudio come free with the NDK via 'android' and 'aaudio'
target_link_libraries(mgba-mobile
    mgba
    android
    aaudio
    log
)
