#include <cstdio>
extern void send_message(const char* msg);

int main() {
    send_message("server hello");
    return 0;
}
