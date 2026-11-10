#include "pcap_device.h"

#include <catch2/catch_test_macros.hpp>

#include <netinet/in.h>
#include <sys/socket.h>

#include <vector>

using raysniff::choose_default_device;

namespace {

// A fake pcap_findalldevs() list. The structures point into each other, so
// the list is built once and never copied.
struct FakeDevices {
    struct Entry {
        const char* name;
        bpf_u_int32 flags;
        bool ipv4;
    };

    explicit FakeDevices(const std::vector<Entry>& entries) : devices(entries.size()), addresses(entries.size()) {
        ipv4_address.sin_family = AF_INET;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            devices[i].name = const_cast<char*>(entries[i].name);
            devices[i].flags = entries[i].flags;
            if (entries[i].ipv4) {
                addresses[i].addr = reinterpret_cast<sockaddr*>(&ipv4_address);
                devices[i].addresses = &addresses[i];
            }
            if (i + 1 < entries.size()) devices[i].next = &devices[i + 1];
        }
    }

    const pcap_if_t* head() const { return devices.empty() ? nullptr : &devices[0]; }

    std::vector<pcap_if_t> devices;
    std::vector<pcap_addr_t> addresses;
    sockaddr_in ipv4_address{};
};

constexpr bpf_u_int32 kUpRunning = PCAP_IF_UP | PCAP_IF_RUNNING;
constexpr bpf_u_int32 kConnected = PCAP_IF_CONNECTION_STATUS_CONNECTED;

} // namespace

TEST_CASE("default device: a connected interface with IPv4 beats a VPN tunnel", "[device]") {
    // pcap_lookupdev() would pick utun0, the first non-loopback device.
    const FakeDevices list({
        {"lo0", PCAP_IF_LOOPBACK | kUpRunning, true},
        {"utun0", kUpRunning, false},
        {"en0", kUpRunning | kConnected, true},
        {"en1", kUpRunning | kConnected, false},
    });
    CHECK(choose_default_device(list.head()) == "en0");
}

TEST_CASE("default device: loopback is never chosen", "[device]") {
    const FakeDevices list({{"lo0", PCAP_IF_LOOPBACK | kUpRunning | kConnected, true}});
    CHECK(choose_default_device(list.head()).empty());
}

TEST_CASE("default device: an empty list gives no device", "[device]") {
    CHECK(choose_default_device(nullptr).empty());
}

TEST_CASE("default device: the first one wins a tie", "[device]") {
    const FakeDevices list({
        {"en0", kUpRunning, true},
        {"en1", kUpRunning, true},
    });
    CHECK(choose_default_device(list.head()) == "en0");
}
