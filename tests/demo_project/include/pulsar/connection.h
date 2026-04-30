#pragma once
#include <cstdint>
#include <string>
#include <vector>

class Connection {
public:
    explicit Connection(const std::string& host, int port);
    ~Connection();

    bool connect();
    void disconnect();
    bool send(const uint8_t* data, size_t len);
    bool recv(uint8_t* buf, size_t len, size_t& received);

    bool isConnected() const { return connected_; }
    const std::string& host() const { return host_; }
    int port() const { return port_; }

private:
    std::string host_;
    int  port_;
    int  fd_;
    bool connected_;
    std::vector<uint8_t> send_buf_;

#ifdef ENABLE_TLS
    void* tls_ctx_;  // opaque TLS context handle
#endif

    void modernConnect();

#ifdef ENABLE_LEGACY_COMPAT
    void legacyConnect();
#endif

#ifdef ENABLE_RDMA
    // RDMA transport — ENABLE_RDMA is never defined in any CMake target
    void rdmaConnect();
#endif
};
