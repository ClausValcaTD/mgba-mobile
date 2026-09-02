cmake_minimum_required(VERSION 3.22)

project(mGBAMobile)

# Target Android API 24 minimum
set(ANDROID_NATIVE_API_LEVEL 24)

# Go up from src/platform/android/ to repo root
set(MGBA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

# Add SDL2 from third_party subdirectory
add_subdirectory(
  ${MGBA_ROOT}/third_party/SDL2
  SDL2-build
)

# Compile android-main.c as shared library
add_library(mgba-mobile SHARED
    android-main.c
    android-jni.c
)

# Include mGBA source directories
target_include_directories(mgba-mobile PRIVATE
    ${MGBA_ROOT}/include
    ${MGBA_ROOT}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay
    ${MGBA_ROOT}/third_party/SDL2/include
)

# Link libraries
target_link_libraries(mgba-mobile
    mgba
    SDL2
    android
    log
)
