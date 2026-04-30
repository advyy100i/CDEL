#pragma once
#include <cstdint>
#include <vector>

enum class FrameType : uint8_t {
    DATA    = 0x00,
    HEADERS = 0x01,
    PING    = 0x06,
    GOAWAY  = 0x07,
};

struct Frame {
    FrameType type      = FrameType::DATA;
    uint8_t   flags     = 0;
    uint32_t  stream_id = 0;
    std::vector<uint8_t> payload;
};

class Protocol {
public:
    static Frame  parseFrame(const uint8_t* buf, size_t len);
    static size_t serializeFrame(const Frame& f, std::vector<uint8_t>& out);
    static bool   validateFrame(const Frame& f);

private:
    static Frame parseDefaultFrame(const uint8_t* buf, size_t len);
    static void  serializeDefault(const Frame& f, std::vector<uint8_t>& out);

#ifdef ENABLE_HTTP2
    static Frame parseHTTP2Frame(const uint8_t* buf, size_t len);
    static void  serializeHTTP2(const Frame& f, std::vector<uint8_t>& out);
#endif

#ifdef ENABLE_LEGACY_COMPAT
    static Frame parseLegacyFrame(const uint8_t* buf, size_t len);
#endif

#ifdef ENABLE_IPX_PROTOCOL
    // IPX/SPX — removed from roadmap 2003; ENABLE_IPX_PROTOCOL never defined
    static Frame parseIPXFrame(const uint8_t* buf, size_t len);
    static void  handleIPXPacket(const uint8_t* pkt, size_t len);
#endif
};
