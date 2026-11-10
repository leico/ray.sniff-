cmake_minimum_required(VERSION 3.25)

# Universal Binary (Apple Silicon + Intel)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")

add_subdirectory(max-sdk-base)

project(ray.sniff
  VERSION 0.2.0
  DESCRIPTION "Convert network packets into an audio signal"
  LANGUAGES C CXX)

set(GIT_VERSION_TAG "${PROJECT_VERSION}")
set(AUTHOR_DOMAIN "com.leico")

add_subdirectory("source/ray.sniff~")
