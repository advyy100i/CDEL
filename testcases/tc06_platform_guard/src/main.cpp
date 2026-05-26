#include <cstdio>

#ifdef UNIX_PLATFORM
static int open_socket(int port) {
    // POSIX socket creation stub
    (void)port;
    std::printf("[UNIX] socket opened on port %d\n", port);
    return 42;
}
#endif

#ifdef WINDOWS_PLATFORM
// Windows-specific WinSock implementation.
// WINDOWS_PLATFORM is never defined in any CMake target — dead code.
static int open_socket(int port) {
    (void)port;
    std::printf("[WIN32] WSAStartup + socket stub\n");
    return -1;
}
static void cleanup_winsock() {
    std::printf("[WIN32] WSACleanup\n");
}
#endif

int main() {
    int fd = open_socket(8080);
    std::printf("socket fd: %d\n", fd);
    return 0;
}
