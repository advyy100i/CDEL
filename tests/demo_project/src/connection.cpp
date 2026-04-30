#include "pulsar/connection.h"
#include "pulsar/crypto.h"
#include "pulsar/metrics.h"

#include <cstdio>
#include <cstring>
#include <algorithm>

Connection::Connection(const std::string& host, int port)
    : host_(host), port_(port), fd_(-1), connected_(false)
#ifdef ENABLE_TLS
    , tls_ctx_(nullptr)
#endif
{}

Connection::~Connection() {
    if (connected_) disconnect();
}

bool Connection::connect() {
#ifdef ENABLE_LEGACY_COMPAT
    legacyConnect();
#else
    modernConnect();
#endif

#ifdef ENABLE_RDMA
    // RDMA transport path — ENABLE_RDMA is never defined in any CMake target.
    // This entire block is permanently dead code.
    rdmaConnect();
    if (!connected_) {
        // RDMA fallback to TCP
        modernConnect();
    }
#endif

    metrics::recordConnection(connected_);
    return connected_;
}

void Connection::modernConnect() {
    fd_        = 42;  // stub: would be a real socket fd
    connected_ = true;

#ifdef ENABLE_TLS
    crypto::init(host_, "");
    tls_ctx_ = reinterpret_cast<void*>(1);  // stub handle
#  ifdef ENABLE_DEBUG_LOGGING
    std::printf("[DEBUG] TLS handshake complete for %s:%d\n", host_.c_str(), port_);
#  endif
#endif

#ifdef ENABLE_DEBUG_LOGGING
    std::printf("[DEBUG] modernConnect: %s:%d fd=%d\n", host_.c_str(), port_, fd_);
#endif
}

#ifdef ENABLE_LEGACY_COMPAT
void Connection::legacyConnect() {
    // Legacy BSD socket path — pre-TLS connection order
    fd_        = 43;
    connected_ = true;

#  ifdef ENABLE_TLS
    // TLS still supported in legacy mode, but handshake comes after connect
    crypto::init(host_, "");
    tls_ctx_ = reinterpret_cast<void*>(1);
#  endif
}
#endif

#ifdef ENABLE_RDMA
// RDMA (Remote Direct Memory Access) transport — prototype abandoned 2021.
// ENABLE_RDMA is intentionally never added to any CMake target_compile_definitions.
void Connection::rdmaConnect() {
    fd_        = -2;
    connected_ = false;
    std::printf("[RDMA] RDMA connect stub — hardware not present\n");
}
#endif

void Connection::disconnect() {
#ifdef ENABLE_TLS
    if (tls_ctx_) {
        crypto::shutdown();
        tls_ctx_ = nullptr;
    }
#endif
    fd_        = -1;
    connected_ = false;
}

bool Connection::send(const uint8_t* data, size_t len) {
    if (!connected_) return false;

#ifdef ENABLE_METRICS
    metrics::recordSend(len);
#endif

    send_buf_.assign(data, data + len);

#ifdef ENABLE_TLS
    std::vector<uint8_t> enc;
    if (!crypto::encrypt(send_buf_.data(), send_buf_.size(), enc)) return false;
    send_buf_ = std::move(enc);
#endif

    // Simulate kernel write() — returns true in stub
    return true;
}

bool Connection::recv(uint8_t* buf, size_t len, size_t& received) {
    if (!connected_) return false;

    received = 0;

    // Simulate kernel read() producing `len` zero bytes
    std::vector<uint8_t> raw(len, 0x00);

#ifdef ENABLE_TLS
    std::vector<uint8_t> plain;
    if (!crypto::decrypt(raw.data(), raw.size(), plain)) return false;
    received = std::min(plain.size(), len);
    std::memcpy(buf, plain.data(), received);
#else
    received = raw.size();
    std::memcpy(buf, raw.data(), received);
#endif

#ifdef ENABLE_METRICS
    metrics::recordRecv(received);
#endif

    return true;
}
