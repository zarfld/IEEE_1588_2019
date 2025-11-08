// @satisfies STR-PORT-004 - Cross-Platform Build System

#include <cstdio>
#include <iostream>

static bool exists(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (f) {
        std::fclose(f);
        return true;
    }
    return false;
}

int main() {
    // Try to locate repository root by checking upward prefixes
    const char* prefixes[] = {"", "../", "../../", "../../../", "../../../../", "../../../../../"};
    const char* cmakelists_rel = "CMakeLists.txt";
    const char* config_rel = "cmake/IEEE1588_2019Config.cmake.in";

    const char* found_prefix = 0;
    for (size_t i = 0; i < sizeof(prefixes)/sizeof(prefixes[0]); ++i) {
        std::string probe = std::string(prefixes[i]) + cmakelists_rel;
        if (exists(probe.c_str())) {
            found_prefix = prefixes[i];
            break;
        }
    }

    if (!found_prefix) {
        std::cerr << "Missing CMakeLists.txt when probing parent directories." << "\n";
        return 2;
    }

    std::string cmakelists_path = std::string(found_prefix) + cmakelists_rel;
    std::string config_in_path = std::string(found_prefix) + config_rel;
    if (!exists(cmakelists_path.c_str())) {
        std::cerr << "Missing CMakeLists.txt at repo root: " << cmakelists_path << "\n";
        return 2;
    }
    if (!exists(config_in_path.c_str())) {
        std::cerr << "Missing CMake package config template: " << config_in_path << "\n";
        return 3;
    }

    // Non-fatal evidence file from CTest
    const char* last_test_log = "Testing/Temporary/LastTest.log";
    if (!exists(last_test_log)) {
        std::cerr << "Warning: CTest LastTest.log not found at: " << last_test_log << "\n";
    }

    std::cout << "CMake build system artifacts present." << std::endl;
    return 0;
}
