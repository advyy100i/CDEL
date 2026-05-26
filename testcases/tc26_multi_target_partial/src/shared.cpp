#include <cstdio>

void send_message(const char* msg) {
    std::printf("[send] %s\n", msg);
}

#ifdef SERVER_PUSH
// Server-push notifications — only compiled for the server target.
// The shared library target (tc26_shared) does NOT have SERVER_PUSH,
// so this block is dead from the library's perspective.
void push_notify(const char* topic, const char* payload) {
    std::printf("[push] %s: %s\n", topic, payload);
}

void push_broadcast(const char* payload, int client_count) {
    std::printf("[push] broadcasting to %d clients: %s\n", client_count, payload);
}
#endif  // SERVER_PUSH
