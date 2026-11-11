// Test helpers: small Ethernet / IPv4 packets written to a temporary .pcap
// file (design.md 8: the test captures are made by the tests themselves).

#pragma once

#include <pcap/pcap.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace raysniff_test {

using Packet = std::vector<std::uint8_t>;

// Ethernet + IPv4 + UDP / TCP header, followed by `payload_size` bytes of
// `fill`. Only the fields that BPF filters look at are meaningful.
Packet udp_packet(std::uint8_t fill, std::size_t payload_size, std::uint16_t dst_port = 9000);
Packet tcp_packet(std::uint8_t fill, std::size_t payload_size, std::uint16_t dst_port = 80);

// All packets one after another, as the queue should hold them.
Packet concat(const std::vector<Packet>& packets);

// Writes the packets to a new .pcap file in the temporary directory and
// deletes it on destruction. `linktype` is a DLT_* value (Ethernet by default).
class TempPcapFile {
public:
    explicit TempPcapFile(const std::vector<Packet>& packets, int linktype = DLT_EN10MB);
    ~TempPcapFile();
    TempPcapFile(const TempPcapFile&) = delete;
    TempPcapFile& operator=(const TempPcapFile&) = delete;

    const std::string& path() const { return path_; }

private:
    std::string path_;
};

} // namespace raysniff_test
