#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>

// Expected SRG schema
static const char* EXPECTED_HEADER = "FailureNumber,FailureTime,Severity,Operation,State,Fixed";
static const char* TAG = "SRG_EXPORT:";

static std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> cols;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, ',')) {
        cols.emplace_back(trim(cur));
    }
    return cols;
}

int main(int argc, char** argv) {
    // Defaults: input in reliability/srg_failures.csv; output in reliability/srg_export.csv
    std::string inPath = (argc > 1) ? argv[1] : std::string("reliability/srg_failures.csv");
    std::string outPath = (argc > 2) ? argv[2] : std::string("reliability/srg_export.csv");

    std::ifstream in(inPath.c_str());
    if (!in.good()) {
        std::cout << TAG << " NO_INPUT (" << inPath << ")\n";
        // Write header-only file for downstream tools to find
        std::ofstream out(outPath.c_str(), std::ios::out | std::ios::trunc);
        if (out.is_open()) out << EXPECTED_HEADER << '\n';
        return 0; // non-gating utility
    }

    std::string header; std::getline(in, header);
    auto hdrCols = split_csv(header);
    std::unordered_map<std::string, int> idx;
    for (int i = 0; i < static_cast<int>(hdrCols.size()); ++i) {
        idx[hdrCols[i]] = i;
    }
    const char* names[6] = {"FailureNumber","FailureTime","Severity","Operation","State","Fixed"};
    bool haveAll = true;
    for (auto n : names) { if (!idx.count(n)) { haveAll = false; break; } }

    std::vector<std::vector<std::string>> rows;
    std::string line;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty()) continue;
        rows.emplace_back(split_csv(t));
    }

    std::ofstream out(outPath.c_str(), std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << TAG << " OUTPUT_OPEN_FAILED (" << outPath << ")\n";
        return 1; // signal to CI, though not intended to be gating
    }

    // Always write expected header
    out << EXPECTED_HEADER << '\n';

    size_t exported = 0;
    if (haveAll) {
        for (auto &r : rows) {
            auto col = [&](const char* n){ int i = idx[n]; return i < static_cast<int>(r.size()) ? r[i] : std::string(""); };
            out << col("FailureNumber") << ','
                << col("FailureTime") << ','
                << col("Severity") << ','
                << col("Operation") << ','
                << col("State") << ','
                << col("Fixed") << '\n';
            exported++;
        }
        std::cout << TAG << " OK exported=" << exported << " from " << inPath << " -> " << outPath << "\n";
        return 0;
    } else {
        // Missing columns: still produce header-only file and report
        std::cout << TAG << " MISSING_COLUMNS exported=0 from " << inPath << " -> " << outPath << "\n";
        return 0;
    }
}
