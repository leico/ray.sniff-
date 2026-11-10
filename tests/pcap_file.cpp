#include "pcap_file.h"

#include <pcap/pcap.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

namespace raysniff_test {

namespace {

constexpr std::uint8_t kProtocolTcp = 6;
constexpr std::uint8_t kProtocolUdp = 17;

void put16(Packet& p, std::uint16_t v) {
    p.push_back(static_cast<std::uint8_t>(v >> 8));
    p.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

void put32(Packet& p, std::uint32_t v) {
    put16(p, static_cast<std::uint16_t>(v >> 16));
    put16(p, static_cast<std::uint16_t>(v & 0xFFFF));
}

// Ethernet (14 bytes) + IPv4 (20 bytes) for 10.0.0.1 -> 10.0.0.2.
Packet ethernet_ipv4(std::uint8_t protocol, std::size_t ip_payload_size) {
    Packet p;
    for (int i = 0; i < 6; ++i) p.push_back(0x02);  // destination MAC
    for (int i = 0; i < 6; ++i) p.push_back(0x04);  // source MAC
    put16(p, 0x0800);                               // EtherType: IPv4

    p.push_back(0x45);                              // version 4, header 20 bytes
    p.push_back(0x00);                              // TOS
    put16(p, static_cast<std::uint16_t>(20 + ip_payload_size));
    put16(p, 0x0000);                               // identification
    put16(p, 0x4000);                               // don't fragment
    p.push_back(64);                                // TTL
    p.push_back(protocol);
    put16(p, 0x0000);                               // checksum (not checked by BPF)
    put32(p, 0x0A000001);                           // 10.0.0.1
    put32(p, 0x0A000002);                           // 10.0.0.2
    return p;
}

} // namespace

Packet udp_packet(std::uint8_t fill, std::size_t payload_size, std::uint16_t dst_port) {
    Packet p = ethernet_ipv4(kProtocolUdp, 8 + payload_size);
    put16(p, 50000);                                // source port
    put16(p, dst_port);
    put16(p, static_cast<std::uint16_t>(8 + payload_size));
    put16(p, 0x0000);                               // checksum
    p.insert(p.end(), payload_size, fill);
    return p;
}

Packet tcp_packet(std::uint8_t fill, std::size_t payload_size, std::uint16_t dst_port) {
    Packet p = ethernet_ipv4(kProtocolTcp, 20 + payload_size);
    put16(p, 50000);                                // source port
    put16(p, dst_port);
    put32(p, 1);                                    // sequence number
    put32(p, 0);                                    // acknowledgment number
    p.push_back(0x50);                              // header 20 bytes
    p.push_back(0x18);                              // PSH, ACK
    put16(p, 65535);                                // window
    put16(p, 0x0000);                               // checksum
    put16(p, 0x0000);                               // urgent pointer
    p.insert(p.end(), payload_size, fill);
    return p;
}

Packet concat(const std::vector<Packet>& packets) {
    Packet all;
    for (const auto& p : packets) all.insert(all.end(), p.begin(), p.end());
    return all;
}

TempPcapFile::TempPcapFile(const std::vector<Packet>& packets, int linktype) {
    static std::atomic<int> counter{0};
    path_ = (std::filesystem::temp_directory_path() /
             ("raysniff_test_" + std::to_string(getpid()) + "_" + std::to_string(counter++) + ".pcap"))
                .string();

    pcap_t* dead = pcap_open_dead(linktype, 65535);
    if (dead == nullptr) throw std::runtime_error("pcap_open_dead failed");
    pcap_dumper_t* dumper = pcap_dump_open(dead, path_.c_str());
    if (dumper == nullptr) {
        const std::string error = pcap_geterr(dead);
        pcap_close(dead);
        throw std::runtime_error("pcap_dump_open failed: " + error);
    }

    long usec = 0;
    for (const auto& packet : packets) {
        pcap_pkthdr header{};
        header.ts.tv_sec = 1;
        header.ts.tv_usec = static_cast<int>(usec += 1000);
        header.caplen = static_cast<bpf_u_int32>(packet.size());
        header.len = header.caplen;
        pcap_dump(reinterpret_cast<u_char*>(dumper), &header, packet.data());
    }
    pcap_dump_close(dumper);
    pcap_close(dead);
}

TempPcapFile::~TempPcapFile() {
    std::remove(path_.c_str());
}

} // namespace raysniff_test
