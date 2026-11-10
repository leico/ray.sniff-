# Max-independent parts of ray.sniff~ (design.md 8).
# Linked by the external and by the unit tests.

add_library(raysniff_core STATIC
  locked_memory.cpp
  byte_ring.cpp
  packet_queue.cpp
  pcap_device.cpp
  pcap_source.cpp
  capture_thread.cpp
  capture_session.cpp
)

target_include_directories(raysniff_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(raysniff_core PUBLIC cxx_std_17)
set_target_properties(raysniff_core PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(raysniff_core PRIVATE -Wall -Wextra -Wpedantic)

# The libpcap that comes with macOS (design.md 5).
find_package(Threads REQUIRED)
target_link_libraries(raysniff_core PUBLIC pcap Threads::Threads)
