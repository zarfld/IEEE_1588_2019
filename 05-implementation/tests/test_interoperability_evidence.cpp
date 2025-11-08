// @satisfies STR-STD-004 - Interoperability with Commercial Devices (evidence presence)
// Purpose: Guardrail evidence test that ensures interop artifacts are present in the repo
// under 07-verification-validation/test-results/interop/. This is a placeholder for
// externally collected lab evidence (pcaps, logs, reports) and prevents accidental deletion.

#include <cstdio>
#include <string>
#include <fstream>
#include <sys/stat.h>

#if __cplusplus >= 201703L
  #include <filesystem>
  #define USE_FS 1
#else
  #define USE_FS 0
#endif

#ifndef SOURCE_DIR
#define SOURCE_DIR "."
#endif

static bool pathExists(const std::string& p){ struct stat st; return stat(p.c_str(), &st) == 0; }

int main(){
    const std::string root = SOURCE_DIR;
    const std::string interopDir = root + "/07-verification-validation/test-results/interop";
    const std::string evidenceFile = interopDir + "/interop_evidence.md";

#if USE_FS
    namespace fs = std::filesystem;
    if(!fs::exists(interopDir)) { std::puts("interop dir missing"); return 1; }
    if(!fs::exists(evidenceFile)) { std::puts("interop evidence file missing"); return 2; }
    // Optional: ensure file non-empty
    if(fs::file_size(evidenceFile) == 0) { std::puts("interop evidence file empty"); return 3; }
#else
    if(!pathExists(interopDir)) { std::puts("interop dir missing"); return 1; }
    if(!pathExists(evidenceFile)) { std::puts("interop evidence file missing"); return 2; }
    // Minimal non-empty check
    std::ifstream in(evidenceFile); char c; if(!(in>>std::noskipws>>c)) { std::puts("interop evidence file empty"); return 3; }
#endif

    std::puts("interoperability_evidence: PASS (placeholder present)");
    return 0;
}
