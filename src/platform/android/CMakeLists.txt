cmake_minimum_required(VERSION 3.22)
project(mGBAMobile)

set(ANDROID_NATIVE_API_LEVEL 26)

# Build mGBA core only — no frontends, no optional deps, no PNG/FFmpeg/etc
set(LIBMGBA_ONLY ON CACHE BOOL "" FORCE)
set(M_CORE_GBA ON CACHE BOOL "" FORCE)
set(M_CORE_GB  ON CACHE BOOL "" FORCE)
set(USE_LZMA   ON CACHE BOOL "" FORCE)
set(USE_ZLIB   OFF CACHE BOOL "" FORCE)
set(USE_PNG    OFF CACHE BOOL "" FORCE)
set(USE_FFMPEG OFF CACHE BOOL "" FORCE)
set(USE_MINIZIP OFF CACHE BOOL "" FORCE)
set(USE_LIBZIP  OFF CACHE BOOL "" FORCE)
set(USE_SQLITE3 OFF CACHE BOOL "" FORCE)
set(USE_ELF     OFF CACHE BOOL "" FORCE)
set(USE_LUA     OFF CACHE BOOL "" FORCE)
set(USE_JSON_C  OFF CACHE BOOL "" FORCE)
set(USE_FREETYPE OFF CACHE BOOL "" FORCE)
set(USE_DISCORD_RPC OFF CACHE BOOL "" FORCE)
set(ENABLE_SCRIPTING OFF CACHE BOOL "" FORCE)
set(ENABLE_DEBUGGERS OFF CACHE BOOL "" FORCE)
set(BUILD_QT  OFF CACHE BOOL "" FORCE)
set(BUILD_SDL OFF CACHE BOOL "" FORCE)

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
