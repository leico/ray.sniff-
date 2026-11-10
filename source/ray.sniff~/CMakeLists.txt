set_property(DIRECTORY PROPERTY max::external YES)
set_property(DIRECTORY PROPERTY max::glob ON)
project(ray.sniff_tilde)

# libpcap bundled with macOS (/usr/lib/libpcap.A.dylib, x86_64 + arm64e)
target_link_libraries(${PROJECT_NAME} PRIVATE pcap)
