set_property(DIRECTORY PROPERTY max::external YES)
set_property(DIRECTORY PROPERTY max::glob ON)
project(ray.sniff_tilde)

# Queue, capture and sample conversion (source/core/, also linked by the
# tests). It brings libpcap from macOS and the thread library with it.
target_link_libraries(${PROJECT_NAME} PRIVATE raysniff_core)
