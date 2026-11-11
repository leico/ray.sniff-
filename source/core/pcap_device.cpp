#include "pcap_device.h"

#include <sys/socket.h>

namespace raysniff {

namespace {

bool has_ipv4_address(const pcap_if_t* device) {
    for (const pcap_addr_t* a = device->addresses; a != nullptr; a = a->next) {
        if (a->addr != nullptr && a->addr->sa_family == AF_INET) return true;
    }
    return false;
}

int score(const pcap_if_t* device) {
    int s = 0;
    if ( device->flags & PCAP_IF_UP)      s += 1;
    if ( device->flags & PCAP_IF_RUNNING) s += 2;
    if ((device->flags & PCAP_IF_CONNECTION_STATUS) == PCAP_IF_CONNECTION_STATUS_CONNECTED) s += 4;
    if (has_ipv4_address(device))         s += 8;
    return s;
}

} // namespace

std::string choose_default_device(const pcap_if_t* devices) {
    const pcap_if_t* best = nullptr;
    int best_score = -1;
    for (const pcap_if_t* d = devices; d != nullptr; d = d->next) {
        if (d->flags & PCAP_IF_LOOPBACK) continue;
        const int s = score(d);
        if (s > best_score) {
            best = d;
            best_score = s;
        }
    }
    return best != nullptr ? best->name : "";
}

std::string find_default_device(std::string& error) {
    char errbuf[PCAP_ERRBUF_SIZE] = {};
    pcap_if_t* devices = nullptr;
    if (pcap_findalldevs(&devices, errbuf) == -1) {
        error = errbuf;
        return {};
    }
    std::string name = choose_default_device(devices);
    pcap_freealldevs(devices);
    if (name.empty()) error = "no capture device found";
    return name;
}

} // namespace raysniff
