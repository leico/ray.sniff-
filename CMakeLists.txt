cmake_minimum_required(VERSION 3.25)

# Universal Binary (Apple Silicon + Intel)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")

# arm64 requires macOS 11 anyway. max-sdk-base defaults to 10.11, which also
# lacks the C++17 aligned new used by the lock-free queue.
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 11.0)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

add_subdirectory(max-sdk-base)

project(ray.sniff
  VERSION 0.2.0
  DESCRIPTION "Convert network packets into an audio signal"
  LANGUAGES C CXX)

set(GIT_VERSION_TAG "${PROJECT_VERSION}")
set(AUTHOR_DOMAIN "com.leico")

option(RAYSNIFF_BUILD_TESTS "Build the unit tests (Catch2)" OFF)
set(RAYSNIFF_SANITIZER "" CACHE STRING "Build with a sanitizer: thread, address or undefined")
set_property(CACHE RAYSNIFF_SANITIZER PROPERTY STRINGS "" thread address undefined)

if(RAYSNIFF_SANITIZER)
  # ThreadSanitizer and AddressSanitizer cannot be combined; use a separate build directory for each.
  add_compile_options(-fsanitize=${RAYSNIFF_SANITIZER} -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=${RAYSNIFF_SANITIZER})
endif()

add_subdirectory(source/core)

# A test build makes only the tests. The external is written to externals/ in
# the source tree, so a test build (often arm64 only, or with a sanitizer)
# would overwrite the Universal one made by the normal build.
if(RAYSNIFF_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
else()
  add_subdirectory("source/ray.sniff~")
endif()
