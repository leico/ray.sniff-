include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.16.1
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(raysniff_tests
  test_queue_size.cpp
  test_locked_memory.cpp
  test_byte_ring.cpp
  test_packet_queue.cpp
)
target_link_libraries(raysniff_tests PRIVATE raysniff_core Catch2::Catch2WithMain)
target_compile_options(raysniff_tests PRIVATE -Wall -Wextra)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)
catch_discover_tests(raysniff_tests)
