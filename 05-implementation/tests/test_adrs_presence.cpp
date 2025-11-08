// @satisfies STR-MAINT-003 - Architectural Decision Records (ADRs)
// Verifies at least one ADR file exists with expected naming in 03-architecture/decisions/

#include <cstdio>
#include <vector>
#include <string>

static bool file_exists(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (f) { std::fclose(f); return true; }
    return false;
}

int main() {
    // Probe upward to handle possible working directory differences.
    const char* root = REPO_ROOT; // Provided by CMake definition
    const char* files[] = {
        "/03-architecture/decisions/ADR-001-hardware-abstraction-interfaces.md",
        "/03-architecture/decisions/ADR-002-ieee-standards-layering.md",
        "/03-architecture/decisions/ADR-003-ieee-1588-2019-implementation-strategy.md",
        "/03-architecture/decisions/ADR-013-ieee-1588-2019-multi-layered-architecture.md"
    };
    bool any = false;
    for (auto f : files) {
        std::string path = std::string(root) + f;
        if (file_exists(path.c_str())) { any = true; break; }
    }
    if (!any) {
        std::fprintf(stderr, "No ADR markdown found using REPO_ROOT '%s' in 03-architecture/decisions/\n", root);
        return 1;
    }
    std::printf("ADR presence verified.\n");
    return 0;
}
