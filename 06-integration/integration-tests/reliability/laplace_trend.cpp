#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <limits>

/*
 * Laplace Trend Approximation Tool (Phase 06 Integration)
 * Parses reliability_history.csv and provides a coarse MTBF trend classification.
 * This does NOT reproduce IEEE 1633 text; implementation based on general reliability
 * growth trend principles: increasing MTBF suggests reliability growth.
 *
 * Output format (stdout):
 *   LAPLACE_TREND: <INCREASING|DECREASING|STABLE|INSUFFICIENT_DATA>
 *   Slope: <value>
 *   MTBF_First: <value>
 *   MTBF_Last: <value>
 *   MTBF_Delta: <value>
 *
 * Exit code is always 0 (non-gating) so this can be used as observational evidence.
 */

struct RunData {
    std::size_t runIndex; // sequential index starting at 1
    double mtbf;          // MTBF value from history row
};

static bool parse_history(const std::string &path, std::vector<RunData> &out) {
    std::ifstream in(path.c_str(), std::ios::in);
    if (!in.is_open()) {
        return false;
    }
    std::string header; if (!std::getline(in, header)) return false;
    std::string line; std::size_t idx = 1;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string field; std::vector<std::string> cols;
        while (std::getline(ss, field, ',')) cols.push_back(field);
        // Expect at least: RunTimestamp,Iterations,Passed,Failures,PassRate,MTBF,...
        if (cols.size() < 6) continue;
        try {
            double mtbf = std::stod(cols[5]);
            out.push_back(RunData{idx++, mtbf});
        } catch (...) {
            // ignore malformed row
        }
    }
    return true;
}

int main(int argc, char** argv) {
    std::string history_path = "reliability_history.csv";
    if (argc > 1) history_path = argv[1];

    std::vector<RunData> runs;
    if (!parse_history(history_path, runs)) {
        std::cout << "LAPLACE_TREND: INSUFFICIENT_DATA\nReason: history file not readable\n";
        return 0;
    }
    if (runs.size() < 3) {
        std::cout << "LAPLACE_TREND: INSUFFICIENT_DATA\nReason: need >=3 runs for trend\n";
        return 0;
    }

    // Compute simple least-squares slope of MTBF over run index
    double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumX2 = 0.0;
    for (auto &r : runs) {
        double x = static_cast<double>(r.runIndex);
        double y = r.mtbf;
        sumX += x; sumY += y; sumXY += x * y; sumX2 += x * x;
    }
    double n = static_cast<double>(runs.size());
    double denom = (n * sumX2 - sumX * sumX);
    double slope = 0.0;
    if (denom != 0.0) {
        slope = (n * sumXY - sumX * sumY) / denom;
    }

    double first = runs.front().mtbf;
    double last = runs.back().mtbf;
    double delta = last - first;

    // Classification thresholds (heuristic):
    //   slope > 0.01 => INCREASING
    //   slope < -0.01 => DECREASING
    //   else STABLE
    const double POS_THRESH = 0.01;
    const double NEG_THRESH = -0.01;
    std::string trend;
    if (slope > POS_THRESH) trend = "INCREASING";
    else if (slope < NEG_THRESH) trend = "DECREASING";
    else trend = "STABLE";

    std::cout << "LAPLACE_TREND: " << trend << "\n";
    std::cout << "Slope: " << slope << "\n";
    std::cout << "MTBF_First: " << first << "\n";
    std::cout << "MTBF_Last: " << last << "\n";
    std::cout << "MTBF_Delta: " << delta << "\n";
    std::cout << "Runs: " << runs.size() << "\n";
    std::cout << "Source: " << history_path << "\n";
    return 0;
}
