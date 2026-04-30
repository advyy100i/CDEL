#include "pulsar/protocol.h"
#include <cstdio>

// ── Public API ────────────────────────────────────────────────────────────────

Frame Protocol::parseFrame(const uint8_t* buf, size_t len) {
    if (len < 9) return Frame{};

    // #elif chain: exercises multi-branch ifdef coverage
#ifdef ENABLE_HTTP2
    return parseHTTP2Frame(buf, len);
#elif defined(ENABLE_LEGACY_COMPAT)
    return parseLegacyFrame(buf, len);
#else
    return parseDefaultFrame(buf, len);
#endif
}

size_t Protocol::serializeFrame(const Frame& f, std::vector<uint8_t>& out) {
#ifdef ENABLE_HTTP2
    serializeHTTP2(f, out);
#else
    serializeDefault(f, out);
#endif
    return out.size();
}

bool Protocol::validateFrame(const Frame& f) {
    // RFC 7540 §4.2: max frame size 2^24-1 bytes; stream id MSB must be 0
    if (f.payload.size() > (1u << 24)) return false;
    return (f.stream_id & 0x80000000u) == 0;
}

// ── Default framing (RFC 7540 binary layout without HTTP/2 semantics) ─────────

Frame Protocol::parseDefaultFrame(const uint8_t* buf, size_t len) {
    Frame f{};
    uint32_t payload_len = (static_cast<uint32_t>(buf[0]) << 16)
                         | (static_cast<uint32_t>(buf[1]) <<  8)
                         |  static_cast<uint32_t>(buf[2]);

    f.type      = static_cast<FrameType>(buf[3]);
    f.flags     = buf[4];
    f.stream_id = (static_cast<uint32_t>(buf[5]) << 24)
                | (static_cast<uint32_t>(buf[6]) << 16)
                | (static_cast<uint32_t>(buf[7]) <<  8)
                |  static_cast<uint32_t>(buf[8]);
    f.stream_id &= 0x7FFFFFFFu;

    if (payload_len + 9 <= len)
        f.payload.assign(buf + 9, buf + 9 + payload_len);

    return f;
}

void Protocol::serializeDefault(const Frame& f, std::vector<uint8_t>& out) {
    uint32_t plen = static_cast<uint32_t>(f.payload.size());
    out.push_back((plen >> 16) & 0xFF);
    out.push_back((plen >>  8) & 0xFF);
    out.push_back( plen        & 0xFF);
    out.push_back(static_cast<uint8_t>(f.type));
    out.push_back(f.flags);
    out.push_back((f.stream_id >> 24) & 0x7F);
    out.push_back((f.stream_id >> 16) & 0xFF);
    out.push_back((f.stream_id >>  8) & 0xFF);
    out.push_back( f.stream_id        & 0xFF);
    out.insert(out.end(), f.payload.begin(), f.payload.end());
}

// ── HTTP/2 framing (RFC 7540) ─────────────────────────────────────────────────

#ifdef ENABLE_HTTP2
Frame Protocol::parseHTTP2Frame(const uint8_t* buf, size_t len) {
    Frame f = parseDefaultFrame(buf, len);

    // Strip padding from HEADERS/DATA frames (RFC 7540 §6.2 PADDED flag = 0x08)
    if ((f.type == FrameType::HEADERS || f.type == FrameType::DATA)
            && (f.flags & 0x08u) && !f.payload.empty()) {
        uint8_t pad_len = f.payload[0];
        if (pad_len < f.payload.size()) {
            f.payload.erase(f.payload.end() - pad_len, f.payload.end());
            f.payload.erase(f.payload.begin());
        }
    }
    return f;
}

void Protocol::serializeHTTP2(const Frame& f, std::vector<uint8_t>& out) {
    // HTTP/2 wire format is identical to our default layout — constraints
    // are enforced at the semantic layer, not the byte layout.
    serializeDefault(f, out);
}
#endif  // ENABLE_HTTP2

// ── Legacy framing (4-byte header: type|flags|length16) ──────────────────────

#ifdef ENABLE_LEGACY_COMPAT
Frame Protocol::parseLegacyFrame(const uint8_t* buf, size_t len) {
    Frame f{};
    if (len < 4) return f;

    f.type  = static_cast<FrameType>(buf[0]);
    f.flags = buf[1];
    uint16_t payload_len = static_cast<uint16_t>((buf[2] << 8) | buf[3]);

    if (static_cast<size_t>(payload_len) + 4 <= len)
        f.payload.assign(buf + 4, buf + 4 + payload_len);

    return f;
}
#endif  // ENABLE_LEGACY_COMPAT

// ── IPX/SPX framing — permanently dead ───────────────────────────────────────

#ifdef ENABLE_IPX_PROTOCOL
// IPX support was scoped for v0.3 and removed from the roadmap.
// ENABLE_IPX_PROTOCOL is intentionally absent from all CMake targets.
Frame Protocol::parseIPXFrame(const uint8_t* buf, size_t len) {
    Frame f{};
    if (len < 30) return f;  // IPX header = 30 bytes

    f.type = FrameType::DATA;
    f.payload.assign(buf + 30, buf + len);
    return f;
}

void Protocol::handleIPXPacket(const uint8_t* pkt, size_t len) {
    Frame f = parseIPXFrame(pkt, len);
    if (!validateFrame(f)) {
        std::printf("[IPX] invalid frame dropped (%zu bytes)\n", len);
        return;
    }
    std::printf("[IPX] frame type=%d payload=%zu bytes\n",
                static_cast<int>(f.type), f.payload.size());
}
#endif  // ENABLE_IPX_PROTOCOL
