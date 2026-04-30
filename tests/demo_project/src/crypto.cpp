#include "pulsar/crypto.h"
#include <cstring>
#include <cstdio>

namespace crypto {

// ── TLS-enabled implementation ────────────────────────────────────────────────

#ifdef ENABLE_TLS

static bool g_initialized = false;

bool init(const std::string& cert_path, const std::string& /*key_path*/) {
    // Production: load SSL_CTX from cert_path via platform TLS API.
    // Stub: assume success.
    (void)cert_path;
    g_initialized = true;
    return true;
}

void shutdown() {
    g_initialized = false;
}

bool encrypt(const uint8_t* in, size_t len, std::vector<uint8_t>& out) {
    if (!g_initialized) return false;
    // Stub cipher: XOR-0x5A (placeholder for real AES-GCM)
    out.resize(len);
    for (size_t i = 0; i < len; ++i)
        out[i] = in[i] ^ 0x5Au;
    return true;
}

bool decrypt(const uint8_t* in, size_t len, std::vector<uint8_t>& out) {
    if (!g_initialized) return false;
    // XOR is its own inverse
    return encrypt(in, len, out);
}

// ── Kernel TLS zero-copy path — permanently dead ──────────────────────────────

#ifdef ENABLE_ZERO_COPY_TLS
// kTLS zero-copy sendfile() integration.
// ENABLE_ZERO_COPY_TLS is intentionally absent from all CMake targets —
// the feature was prototyped but never passed security review.

bool encryptZeroCopy(const uint8_t* in, size_t len, DMABuffer& buf) {
    (void)in;
    (void)len;
    buf = DMABuffer{nullptr, 0, -1};
    std::printf("[kTLS] zero-copy encrypt stub — not implemented\n");
    return false;
}

bool decryptZeroCopy(const DMABuffer& in, uint8_t* out, size_t& out_len) {
    (void)in;
    (void)out;
    out_len = 0;
    std::printf("[kTLS] zero-copy decrypt stub — not implemented\n");
    return false;
}
#endif  // ENABLE_ZERO_COPY_TLS

// ── Passthrough stubs when TLS is disabled ────────────────────────────────────

#else  // !ENABLE_TLS

bool init(const std::string&, const std::string&) { return true; }
void shutdown() {}

bool encrypt(const uint8_t* in, size_t len, std::vector<uint8_t>& out) {
    out.assign(in, in + len);
    return true;
}

bool decrypt(const uint8_t* in, size_t len, std::vector<uint8_t>& out) {
    out.assign(in, in + len);
    return true;
}

#endif  // ENABLE_TLS

}  // namespace crypto
