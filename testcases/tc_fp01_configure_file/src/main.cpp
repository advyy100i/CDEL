#include "config.h"   // brings in #define HAVE_NETWORK_STACK 1
#include <cstdio>

// This block IS live at compile time — HAVE_NETWORK_STACK is defined via
// config.h.  But the tool will report it as HIGH dead because Phase 1
// never sees the -D flag (it's in the generated header, not compile_commands).
#ifdef HAVE_NETWORK_STACK
int open_connection(const char* host, int port) {
    std::printf("Connecting to %s:%d\n", host, port);
    return 0;
}

void close_connection(int fd) {
    std::printf("Closing fd=%d\n", fd);
}
#endif  // HAVE_NETWORK_STACK

int main() {
    int fd = open_connection("localhost", 8080);
    close_connection(fd);
    return 0;
}
