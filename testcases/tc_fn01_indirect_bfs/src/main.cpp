#include <cstdio>

void modern_handler() {
    std::printf("modern_handler: live path\n");
}

// ENABLE_LEGACY_PATH=1, so this block IS compiled and appears in the IR.
// At runtime it is never called — dispatch[0] is always invoked.
// BFS should flag this MEDIUM, but conservative indirect-call handling
// causes it to be marked reachable due to &legacy_handler being stored
// in the dispatch table.
#ifdef ENABLE_LEGACY_PATH
void legacy_handler() {
    std::printf("legacy_handler: dead path — never dispatched to\n");
}
#endif

int main() {
    typedef void (*handler_fn)();

    handler_fn dispatch[] = {
        &modern_handler,
#ifdef ENABLE_LEGACY_PATH
        &legacy_handler,   // address taken here → BFS marks legacy_handler reachable
#endif
    };

    // Always calls index 0; legacy_handler is never actually invoked.
    dispatch[0]();
    return 0;
}
