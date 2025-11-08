// @satisfies STR-USE-003 - Example Applications
// Verifies presence of basic example source files in examples/ directory.

#include <cstdio>
#include <cstring>
#include <string>

static bool file_exists(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (f) { std::fclose(f); return true; }
    return false;
}

int main() {
    const char* root = REPO_ROOT; // absolute path from CMake
    const char* files[] = {
        "/examples/basic_types_example.cpp",
        "/examples/simple_clock_test.cpp",
        "/examples/time_sensitive_types_example.cpp"
    };
    bool any = false;
    for (auto f : files) {
        std::string path = std::string(root) + f;
        if (file_exists(path.c_str())) { any = true; break; }
    }
    if (!any) {
        std::fprintf(stderr, "No example application sources found under %s/examples/\n", root);
        return 1;
    }
    std::puts("Example application sources present.");
    return 0;
}
