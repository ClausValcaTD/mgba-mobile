cmake_minimum_required(VERSION 3.22)

project(mGBAMobile)

# Target Android API 24 minimum
set(ANDROID_NATIVE_API_LEVEL 24)

# Add SDL2 from third_party subdirectory
add_subdirectory(
  ../../../third_party/SDL2 
  SDL2-build
)

# Include directories
include_directories(third_party/SDL2/include)

# Compile android-main.c as shared library or executable for Android platform
add_library(mgba-mobile SHARED android-main.c android-jni.c)

# Link libraries: mgba SDL2 android log
target_link_libraries(mgba-mobile mgba SDL2 android log)
