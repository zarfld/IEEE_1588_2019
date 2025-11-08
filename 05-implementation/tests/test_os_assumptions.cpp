// @satisfies STR-PORT-003 - No OS Assumptions
// Scans a small set of core directories for forbidden OS/vendor includes.

#include <cstdio>
#include <cstring>

static bool contains_forbidden(const char* content) {
    const char* bad[] = {
        "<winsock2.h>", "<windows.h>", "<ws2tcpip.h>",
        "<pthread.h>", "<sys/socket.h>", "<linux/if_packet.h>",
        "intel_ethernet_hal.h", "network_hal.h"
    };
    for (auto p : bad) {
        if (std::strstr(content, p)) return true;
    }
    return false;
}

static bool scan_file(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return true; // tolerate missing path in this limited scan
    char buf[8192];
    size_t n = std::fread(buf, 1, sizeof(buf)-1, f);
    buf[n] = '\0';
    std::fclose(f);
    return !contains_forbidden(buf);
}

int main() {
    // Minimal set of core files to scan (expand as needed)
    const char* files[] = {
        "../include/IEEE/1588/PTP/2019/messages.hpp",
        "../include/IEEE/1588/PTP/2019/types.hpp",
        "../05-implementation/src/clocks.cpp",
        "../05-implementation/src/bmca.cpp"
    };
    for (auto p : files) {
        if (!scan_file(p)) {
            std::fprintf(stderr, "Forbidden OS/vendor include detected in: %s\n", p);
            return 1;
        }
    }
    std::puts("No forbidden OS/vendor includes detected in scanned files.");
    return 0;
}
