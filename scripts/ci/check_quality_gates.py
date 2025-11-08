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

    # gcovr JSON summary: line_coverage is fraction (0..1)
    line_fraction = data.get('line_coverage')
    if line_fraction is None:
        print("line_coverage missing in coverage.json", file=sys.stderr)
        sys.exit(4)
    covered = line_fraction * 100.0
    minimum = gates.get('coverage', {}).get('minimum', 80)

    print(f"Line coverage: {covered:.2f}% (minimum {minimum}%)")
    if covered < minimum:
        print("QUALITY GATE FAILURE: Coverage below threshold", file=sys.stderr)
        sys.exit(1)
    print("Coverage gate PASSED")

if __name__ == '__main__':
    main()