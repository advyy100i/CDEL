#include <cstdio>
#include <string>

// Open-source encryption — always compiled.
static std::string encrypt_oss(const std::string& data, const std::string& key) {
    (void)key;
    std::string out = data;
    for (char& c : out) c ^= 0x42;
    return out;
}

#ifdef VENDOR_ACME_SDK
// ACME vendor SDK crypto path.
// VENDOR_ACME_SDK is never defined — replaced by open-source lib.
static std::string encrypt_acme(const std::string& data, const std::string& key) {
    (void)data; (void)key;
    std::printf("[ACME] AcmeCrypt::Encrypt stub\n");
    return "";
}

static bool acme_license_check() {
    std::printf("[ACME] license validation stub\n");
    return false;
}

static void acme_cleanup() {
    std::printf("[ACME] AcmeCrypt::Destroy stub\n");
}
#endif  // VENDOR_ACME_SDK

int main() {
    auto enc = encrypt_oss("hello", "key");
    std::printf("encrypted length: %zu\n", enc.size());
    return 0;
}
