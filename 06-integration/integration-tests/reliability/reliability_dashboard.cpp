#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>

struct Row { 
    std::string ts; double iterations=0, passed=0, failures=0, passRate=0, mtbf=0, critical=0, duration=0; 
};

static std::string trim(const std::string &s){
    auto b = s.find_first_not_of(" \t\r\n");
    if (b==std::string::npos) return "";
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e-b+1);
}

int main(int argc, char** argv) {
    std::string path = argc > 1 ? argv[1] : std::string("reliability_history.csv");
    std::ifstream in(path.c_str());
    if (!in.good()) {
        std::cout << "DASHBOARD: NO_DATA (" << path << ")\n";
        return 0; // non-gating utility
    }
    std::string header; std::getline(in, header);
    // Columns expected: RunTimestamp,Iterations,Passed,Failures,PassRate,MTBF,CriticalFailures,DurationSec
    std::vector<Row> rows; rows.reserve(64);
    std::string line;
    while (std::getline(in, line)) {
        if (trim(line).empty()) continue;
        std::stringstream ss(line);
        std::string col;
        Row r;
        std::getline(ss, r.ts, ',');
        std::getline(ss, col, ','); r.iterations = std::atof(col.c_str());
        std::getline(ss, col, ','); r.passed = std::atof(col.c_str());
        std::getline(ss, col, ','); r.failures = std::atof(col.c_str());
        std::getline(ss, col, ','); r.passRate = std::atof(col.c_str());
        std::getline(ss, col, ','); r.mtbf = std::atof(col.c_str());
        std::getline(ss, col, ','); r.critical = std::atof(col.c_str());
        std::getline(ss, col, ','); r.duration = std::atof(col.c_str());
        rows.push_back(r);
    }
    if (rows.empty()) {
        std::cout << "DASHBOARD: NO_DATA (empty)\n";
        return 0;
    }
    const Row &last = rows.back();
    // Failure intensity (per hour) if duration>0 else 0
    double failureIntensity = (last.duration > 0.0) ? (last.failures / last.duration) : 0.0;

    // Simple slope on MTBF vs index (least-squares)
    int n = static_cast<int>(rows.size());
    int useN = std::min(n, 10); // last up to 10 runs
    double sx=0, sy=0, sxx=0, sxy=0;
    for (int i=0;i<useN;i++) {
        int idx = n - useN + i; // trailing window
        double x = static_cast<double>(i);
        double y = rows[idx].mtbf;
        sx += x; sy += y; sxx += x*x; sxy += x*y;
    }
    double denom = useN*sxx - sx*sx;
    double slope = (denom != 0.0) ? (useN*sxy - sx*sy) / denom : 0.0;
    const char* trend = "STABLE";
    if (useN >= 3) {
        if (slope > 0.01) trend = "INCREASING"; // arbitrary small threshold
        else if (slope < -0.01) trend = "DECREASING";
    } else {
        trend = "INSUFFICIENT_DATA";
    }

    // Compute rolling averages over last 5 runs (if available)
    int win = std::min(n, 5);
    double avgPass=0, avgMtbf=0, avgFI=0; // FI approximate using failures/duration per run averaged
    for (int i=n-win;i<n;i++) {
        avgPass += rows[i].passRate;
        avgMtbf += rows[i].mtbf;
        double fi = (rows[i].duration>0.0) ? (rows[i].failures/rows[i].duration) : 0.0;
        avgFI += fi;
    }
    avgPass /= win; avgMtbf /= win; avgFI /= win;

    std::cout << "DASHBOARD: runs=" << n
              << ", last_pass_rate=" << last.passRate
              << ", last_mtbf=" << last.mtbf
              << ", last_failure_intensity=" << failureIntensity
              << ", avg5_pass_rate=" << avgPass
              << ", avg5_mtbf=" << avgMtbf
              << ", avg5_failure_intensity=" << avgFI
              << ", mtbf_trend=" << trend
              << "\n";
    return 0;
}
