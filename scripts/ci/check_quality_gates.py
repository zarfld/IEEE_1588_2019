#!/usr/bin/env python3
"""Quality gate enforcement script.
Reads gcovr JSON summary and quality-gates.yml; exits non-zero if coverage below minimum.
Traceability:
  Phase: 05-implementation
  Design: DES-I-007 (health/metrics integration), DES-C-010 (time sync reliability)
  Requirements: REQ-NF-REL-003 (observability), REQ-NF-REL-005 (quality gates)
"""

import argparse, json, sys, pathlib

try:
    import yaml  # PyYAML expected on runner
except ImportError:
    print("PyYAML not installed; cannot enforce gates", file=sys.stderr)
    sys.exit(2)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--coverage', required=True, help='gcovr JSON summary file')
    ap.add_argument('--gates', required=True, help='quality-gates.yml file')
    args = ap.parse_args()

    cov_path = pathlib.Path(args.coverage)
    gates_path = pathlib.Path(args.gates)
    if not cov_path.exists() or not gates_path.exists():
        print("Coverage or gates file missing", file=sys.stderr)
        sys.exit(3)

    data = json.loads(cov_path.read_text(encoding='utf-8'))
    gates = yaml.safe_load(gates_path.read_text(encoding='utf-8'))

    # Try multiple gcovr JSON shapes to extract total line coverage.
    # Preferred: gcovr --json-summary with 'line_coverage' as fraction (0..1).
    covered = None  # percent (0..100)

    # 1) Newer gcovr summary: fraction 0..1
    line_fraction = data.get('line_coverage')
    if isinstance(line_fraction, (int, float)):
        covered = float(line_fraction) * 100.0

    # 2) Some gcovr versions use 'line_percent' (already 0..100)
    if covered is None:
        line_percent = data.get('line_percent')
        if isinstance(line_percent, (int, float)):
            covered = float(line_percent)

    # 3) Totals style keys (e.g., lines_covered/lines)
    if covered is None:
        totals = (
            data.get('totals')
            or data.get('summary')
            or data.get('gcovr_summary')
            or {}
        )
        lines_total = totals.get('lines') or totals.get('lines_total')
        lines_covered = totals.get('lines_covered') or totals.get('covered_lines')
        if isinstance(lines_total, (int, float)) and isinstance(lines_covered, (int, float)) and lines_total:
            covered = (float(lines_covered) / float(lines_total)) * 100.0

    # 4) Cobertura-like summary (rare in this path): 'line-rate' (0..1)
    if covered is None:
        line_rate = data.get('line-rate') or data.get('line_rate')
        if isinstance(line_rate, (int, float)):
            covered = float(line_rate) * 100.0

    if covered is None:
        print("Unable to determine line coverage from coverage.json (expected keys like line_coverage, line_percent, or totals)", file=sys.stderr)
        sys.exit(4)
    minimum = gates.get('coverage', {}).get('minimum', 80)

    print(f"Line coverage: {covered:.2f}% (minimum {minimum}%)")
    if covered < minimum:
        print("QUALITY GATE FAILURE: Coverage below threshold", file=sys.stderr)
        sys.exit(1)
    print("Coverage gate PASSED")

if __name__ == '__main__':
    main()