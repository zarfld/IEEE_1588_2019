// @satisfies STR-USE-004 - Porting Guide
// Verifies that PORTING_GUIDE.md exists in repo root.

#include <cstdio>
#include <string>

static bool file_exists(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (f) { std::fclose(f); return true; }
    return false;
}

int main() {
    const char* root = REPO_ROOT; // absolute path from CMake
    std::string path = std::string(root) + "/PORTING_GUIDE.md";
    if (!file_exists(path.c_str())) {
        std::fprintf(stderr, "PORTING_GUIDE.md not found at repo root (%s).\n", root);
        return 1;
    }
    std::puts("Porting guide present.");
    return 0;
}
