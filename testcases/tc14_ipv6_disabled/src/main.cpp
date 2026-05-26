#include <cstdio>
#include <cstring>

static int bind_ipv4(int port) {
    std::printf("[IPv4] binding port %d\n", port);
    return 42;
}

#ifdef ENABLE_IPV6
// IPv6 dual-stack support — ENABLE_IPV6 never applied; dead code.
static int bind_ipv6(int port) {
    std::printf("[IPv6] binding port %d (AF_INET6)\n", port);
    return 43;
}

static bool is_ipv6_address(const char* addr) {
    return addr && std::strchr(addr, ':') != nullptr;
}

static void set_ipv6_only(int fd, bool only) {
    (void)fd; (void)only;
    std::printf("[IPv6] IPV6_V6ONLY = %d\n", (int)only);
}
#endif  // ENABLE_IPV6

int main() {
    int fd = bind_ipv4(9000);
    std::printf("bound fd: %d\n", fd);
    return 0;
}
