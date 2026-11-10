// Choosing the default capture device (design.md 5).

#pragma once

#include <pcap/pcap.h>

#include <string>

namespace raysniff {

// Returns the name of the best device in a pcap_findalldevs() list, or an
// empty string if there is none. Loopback devices are skipped.
//
// pcap_lookupdev() (deprecated) picks the first non-loopback device, which on
// recent macOS is often a VPN tunnel (utun0). Here a device scores
//   +8 has an IPv4 address, +4 connected, +2 running, +1 up
// and the first device with the highest score wins.
std::string choose_default_device(const pcap_if_t* devices);

// pcap_findalldevs() + choose_default_device(). Returns an empty string and
// sets `error` on failure.
std::string find_default_device(std::string& error);

} // namespace raysniff
