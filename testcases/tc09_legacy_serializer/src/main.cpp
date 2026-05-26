#include <cstdio>
#include <string>
#include <vector>

// Current JSON serializer — always compiled.
static std::string serialize_json(const std::vector<int>& data) {
    std::string out = "[";
    for (size_t i = 0; i < data.size(); ++i) {
        if (i) out += ",";
        out += std::to_string(data[i]);
    }
    return out + "]";
}

#ifdef LEGACY_BINARY_FORMAT
// Binary serialization from pre-2022.
// LEGACY_BINARY_FORMAT never defined in any target — dead code.
static std::vector<uint8_t> serialize_binary(const std::vector<int>& data) {
    std::vector<uint8_t> out;
    out.push_back(0xDE); out.push_back(0xAD); // magic
    for (int v : data) {
        out.push_back((v >> 24) & 0xFF);
        out.push_back((v >> 16) & 0xFF);
        out.push_back((v >>  8) & 0xFF);
        out.push_back( v        & 0xFF);
    }
    return out;
}

static std::vector<int> deserialize_binary(const uint8_t* buf, size_t len) {
    (void)buf; (void)len;
    return {};
}
#endif  // LEGACY_BINARY_FORMAT

int main() {
    std::vector<int> data = {1, 2, 3, 42};
    std::printf("%s\n", serialize_json(data).c_str());
    return 0;
}
