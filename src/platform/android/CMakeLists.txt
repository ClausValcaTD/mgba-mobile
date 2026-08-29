cmake_minimum_required(VERSION 3.22)

# Target Android API 24 minimum
set(ANDROID_NATIVE_API_LEVEL 24)

# Add SDL2 dependency
find_package(SDL2 REQUIRED)

# Include directories
include_directories(${SDL2_INCLUDE_DIRS})

# Compile android-main.c as shared library or executable for Android platform
add_library(mgba-mobile SHARED android-main.c)

# Link libraries: mgba SDL2 android log
target_link_libraries(mgba-mobile mgba ${SDL2_LIBRARIES} android log)
