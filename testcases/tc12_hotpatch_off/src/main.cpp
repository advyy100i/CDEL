#include <cstdio>

static void dispatch(int request_id) {
    std::printf("dispatching request %d\n", request_id);
}

#ifdef ENABLE_HOT_PATCH
// Runtime function patching via mprotect + memcpy.
// ENABLE_HOT_PATCH defaults OFF and is never applied — dead code.
struct PatchEntry {
    void*  target;
    void*  replacement;
    size_t size;
};

static bool apply_patch(const PatchEntry& p) {
    (void)p;
    std::printf("[hotpatch] applying patch at %p\n", p.target);
    return false;
}

static void revert_all_patches() {
    std::printf("[hotpatch] reverting all patches\n");
}
#endif  // ENABLE_HOT_PATCH

int main() {
    for (int i = 0; i < 3; ++i)
        dispatch(i);
    return 0;
}
