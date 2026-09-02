cmake_minimum_required(VERSION 3.22)

project(mGBAMobile)

# Target Android API 24 minimum
set(ANDROID_NATIVE_API_LEVEL 24)

# Add SDL2 from third_party subdirectory
add_subdirectory(
  ../../../third_party/SDL2 
  SDL2-build
)

# Compile android-main.c as shared library or executable for Android platform
add_library(mgba-mobile SHARED android-main.c android-jni.c)

# Include mGBA source directories
target_include_directories(mgba-mobile PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    third_party/SDL2/include
)

# Link libraries: mgba SDL2 android log
target_link_libraries(mgba-mobile mgba SDL2 android log)
