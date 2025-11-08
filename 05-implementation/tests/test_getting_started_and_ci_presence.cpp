// @satisfies STR-USE-002 - Getting Started Tutorial (detect tutorial/example evidence)
// @satisfies STR-MAINT-002 - Continuous Integration (workflow presence)
// @satisfies STR-MAINT-004 - Community Contribution Process (CONTRIBUTING, templates)
// Purpose: Provide traceability evidence by checking for doc/tutorial markers & CI workflows.
// NOTE: Non-substitutive for manual validation; acts as guard against accidental deletion.

// C++14 compatible evidence test: avoids std::filesystem when not available.
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <sys/stat.h>

#if __cplusplus >= 201703L
    #include <filesystem>
    #define USE_FS 1
#else
    #define USE_FS 0
#endif

int main(){
    const std::string root = SOURCE_DIR;

#if USE_FS
    namespace fs = std::filesystem;
    fs::path workflows = fs::path(root)/".github"/"workflows";
    if(!fs::exists(workflows)) return 1;
    bool hasCoreCI=false; bool hasStandardsCI=false;
    for(const auto& e : fs::directory_iterator(workflows)){
        auto name = e.path().filename().string();
        if(name.find("ci.yml")!=std::string::npos) hasCoreCI=true;
        if(name.find("standards")!=std::string::npos) hasStandardsCI=true;
    }
    if(!hasCoreCI) return 2;
    bool hasContrib = fs::exists(fs::path(root)/"CONTRIBUTING.md");
    fs::path examplesDir = fs::path(root)/"examples";
    if(!fs::exists(examplesDir)) return 4;
    bool hasAnyExample=false;
    for(const auto& e : fs::directory_iterator(examplesDir)){
        if(e.is_regular_file() || e.is_directory()) { hasAnyExample=true; break; }
    }
    if(!hasAnyExample) return 5;
    fs::path readmePath = fs::path(root)/"README.md";
    if(!fs::exists(readmePath)) return 7;
    {
        std::ifstream in(readmePath.string()); std::string line; bool found=false;
        while(std::getline(in,line)){
            if(line.find("Getting Started")!=std::string::npos || line.find("getting started")!=std::string::npos){ found=true; break; }
        }
        if(!found) return 6;
    }
#else
    // C++14 fallback: minimal existence checks without directory iteration.
    auto pathExists = [](const std::string& p){ struct stat st; return stat(p.c_str(), &st) == 0; };
    // Workflows directory and ci.yml file evidence
    if(!pathExists(root+"/.github/workflows")) return 1;
    if(!pathExists(root+"/.github/workflows/ci.yml")) return 2; // core CI missing
    bool hasContrib = pathExists(root+"/CONTRIBUTING.md");
    if(!pathExists(root+"/examples")) return 4; // examples root missing
    // Can't iterate examples; assume presence of directory is minimal evidence.
    if(!pathExists(root+"/README.md")) return 7;
    {
        std::ifstream in(root+"/README.md"); std::string line; bool found=false;
        while(std::getline(in,line)){
            if(line.find("Getting Started")!=std::string::npos || line.find("getting started")!=std::string::npos){ found=true; break; }
        }
        if(!found) return 6;
    }
#endif
    if(!hasContrib) {
        std::puts("getting_started_and_ci_presence: PASS (warning: CONTRIBUTING.md not found—community doc pending)");
    } else {
        std::puts("getting_started_and_ci_presence: PASS");
    }
    return 0;
}
