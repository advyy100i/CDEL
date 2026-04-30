#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace crypto {

bool init(const std::string& cert_path, const std::string& key_path);
void shutdown();
bool encrypt(const uint8_t* in, size_t len, std::vector<uint8_t>& out);
bool decrypt(const uint8_t* in, size_t len, std::vector<uint8_t>& out);

#ifdef ENABLE_ZERO_COPY_TLS
// Kernel TLS zero-copy path — ENABLE_ZERO_COPY_TLS never defined in any target
struct DMABuffer {
    void*  addr;
    size_t size;
    int    fd;
};
bool encryptZeroCopy(const uint8_t* in, size_t len, DMABuffer& out);
bool decryptZeroCopy(const DMABuffer& in, uint8_t* out, size_t& out_len);
#endif

}  // namespace crypto
