// @satisfies STR-USE-001 - API Documentation (public headers documented)
// Purpose: Scan include directory for presence of key documented headers and minimal Doxygen markers.
// NOTE: This is a lightweight evidence test; full doc generation validated separately in CI.

#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#if __cplusplus >= 201703L
#include <filesystem>
#define USE_FS 1
#else
#define USE_FS 0
#endif

int main(){
#if USE_FS
    // C++17 path: use std::filesystem to locate and scan headers
    namespace fs = std::filesystem;
    const fs::path includeDir = fs::path(SOURCE_DIR)/"include";
    if(!fs::exists(includeDir)) return 1;
    std::vector<std::string> required = {
        "clocks.hpp",
        "IEEE/1588/PTP/2019/types.hpp",
        "IEEE/1588/PTP/2019/messages.hpp"
    };
    int missing = 0; int lackingDoc = 0;
    for(const auto& rel : required){
        fs::path p = includeDir / rel;
        if(!fs::exists(p)) { ++missing; continue; }
        std::ifstream in(p.string());
        std::string line; bool hasFileTag=false; bool hasBrief=false;
        for(int i=0;i<200 && std::getline(in,line);++i){
            if(line.find("@file")!=std::string::npos) hasFileTag=true;
            if(line.find("@brief")!=std::string::npos) hasBrief=true;
            if(hasFileTag && hasBrief) break;
        }
        if(!(hasFileTag && hasBrief)) ++lackingDoc;
    }
    if(missing>0) return 2;
    if(lackingDoc>0) return 3;
    std::puts("api_docs_presence: PASS");
    return 0;
#else
    // C++14 fallback: check a fixed list of important headers using ifstream only
    int missing = 0; int lackingDoc = 0;
    struct FileInfo { const char* path; } required[] = {
        {"include/clocks.hpp"},
        {"include/IEEE/1588/PTP/2019/types.hpp"},
        {"include/IEEE/1588/PTP/2019/messages.hpp"}
    };
    for(auto &fi : required){
        std::string full = std::string(SOURCE_DIR) + "/" + fi.path;
        std::ifstream in(full);
        if(!in.good()){ ++missing; continue; }
        std::string line; bool hasFile=false, hasBrief=false;
        for(int i=0;i<200 && std::getline(in,line);++i){
            if(line.find("@file")!=std::string::npos) hasFile=true;
            if(line.find("@brief")!=std::string::npos) hasBrief=true;
            if(hasFile && hasBrief) break;
        }
        if(!(hasFile && hasBrief)) ++lackingDoc;
    }
    if(missing>0) return 2;
    if(lackingDoc>0) return 3;
    std::puts("api_docs_presence: PASS (C++14 fallback)");
    return 0;
#endif
}
