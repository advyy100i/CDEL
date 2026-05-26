#include <cstdio>
#include <vector>
#include <cstdint>

#ifdef PROTOCOL_V2
// V2 framing: 4-byte magic + 4-byte length + payload
static std::vector<uint8_t> frame(const uint8_t* data, uint32_t len) {
    std::vector<uint8_t> out;
    out.push_back(0xAB); out.push_back(0xCD);
    out.push_back(0x02); out.push_back(0x00);  // version
    out.push_back((len >> 24) & 0xFF);
    out.push_back((len >> 16) & 0xFF);
    out.push_back((len >>  8) & 0xFF);
    out.push_back( len        & 0xFF);
    out.insert(out.end(), data, data + len);
    return out;
}
#else
// V1 framing: 1-byte magic + 2-byte length — PROTOCOL_V2 always ON, so dead.
static std::vector<uint8_t> frame(const uint8_t* data, uint32_t len) {
    std::vector<uint8_t> out;
    out.push_back(0xAB);
    out.push_back((len >> 8) & 0xFF);
    out.push_back( len       & 0xFF);
    out.insert(out.end(), data, data + len);
    return out;
}

// V1-specific heartbeat — also dead.
static std::vector<uint8_t> heartbeat_v1() {
    return {0xAB, 0x00, 0x00};
}
#endif  // PROTOCOL_V2

int main() {
    uint8_t payload[] = {1, 2, 3};
    auto pkt = frame(payload, 3);
    std::printf("packet size: %zu\n", pkt.size());
    return 0;
}
