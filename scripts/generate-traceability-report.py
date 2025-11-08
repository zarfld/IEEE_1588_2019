#!/usr/bin/env python3
"""
Generate Stakeholder Requirements to Test Cases Traceability Report

This script:
1. Parses stakeholder requirements from YAML front matter
2. Extracts @satisfies tags from test files
3. Parses CTest results for pass/fail status
4. Generates traceability matrix
5. Calculates requirement coverage
6. Enforces 75% threshold

Standards: IEEE 1012-2016 (Verification and Validation)
"""

import argparse
import re
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple
from datetime import datetime
import xml.etree.ElementTree as ET


class Requirement:
    """Stakeholder requirement with metadata"""
    def __init__(self, req_id: str, title: str, priority: str, acceptance_criteria: List[str]):
        self.req_id = req_id
        self.title = title
        self.priority = priority
        self.acceptance_criteria = acceptance_criteria
        self.test_cases: List[str] = []
        self.passing_tests: Set[str] = set()
        self.failing_tests: Set[str] = set()
    
    @property
    def status(self) -> str:
        """Calculate requirement status based on test results"""
        if not self.test_cases:
            return "📋 NO TESTS"
        if len(self.failing_tests) == 0:
            return "✅ PASSING"
        if len(self.passing_tests) > 0:
            return "⚠️ PARTIAL"
        return "❌ FAILING"
    
    @property
    def coverage(self) -> float:
        """Calculate percentage of passing tests"""
        if not self.test_cases:
            return 0.0
        return (len(self.passing_tests) / len(self.test_cases)) * 100
    
    @property
    def is_tested(self) -> bool:
        """Has at least one passing test"""
        return len(self.passing_tests) > 0


def parse_stakeholder_requirements(req_file: Path) -> Dict[str, Requirement]:
    """Parse stakeholder requirements from markdown file.

    Supports headings like:
      #### STR-STD-001: Message Format Correctness
    And priority lines within the section like:
      - **Priority**: P0 (Critical - ...)
    Also collects acceptance criteria from Success Criteria bullets or Gherkin blocks.
    """
    requirements: Dict[str, Requirement] = {}

    if not req_file.exists():
        print(f"❌ Requirements file not found: {req_file}", file=sys.stderr)
        return requirements

    content = req_file.read_text(encoding='utf-8')

    # Match requirement headers at level 3 or 4 (### or ####)
    header_re = re.compile(r"^\s*#{3,4}\s+(STR-[A-Z]+-\d+):\s+(.+)$", re.MULTILINE)

    # Build an index of sections with start/end offsets
    matches = list(header_re.finditer(content))
    for i, m in enumerate(matches):
        req_id = m.group(1).strip()
        title = m.group(2).strip()
        start = m.end()
        end = matches[i + 1].start() if i + 1 < len(matches) else len(content)
        section = content[start:end]

        # Priority extraction within section
        prio_match = re.search(r"^\s*[-*]\s*\*\*Priority\*\*:\s*(P[0-9])", section, re.MULTILINE)
        priority = prio_match.group(1) if prio_match else "P1"

        # Acceptance criteria gathering
        criteria: List[str] = []
        # 1) Bullet lines with SHALL/MUST
        for line in section.splitlines():
            s = line.strip()
            if (s.startswith("- ") or s.startswith("* ")) and ("SHALL" in s or "MUST" in s):
                criteria.append(s[2:].strip())
        # 2) Gherkin code block under "Acceptance Criteria" heading
        gherkin_block = re.search(r"Acceptance Criteria.*?```[a-zA-Z]*\n(.*?)\n```", section, re.DOTALL | re.IGNORECASE)
        if gherkin_block:
            for gl in gherkin_block.group(1).splitlines():
                gl = gl.strip()
                if gl and not gl.startswith('#'):
                    criteria.append(gl)

        # Store this requirement (inside loop – fix bug where only last was stored)
        requirements[req_id] = Requirement(req_id, title, priority, criteria)

    print(f"📋 Parsed {len(requirements)} stakeholder requirements")
    return requirements


def extract_test_metadata(test_dirs: List[Path]) -> Dict[str, List[str]]:
    """Extract @satisfies tags from test files across multiple directories.

    Returns: Dict mapping test_file::test_name -> [STR-XXX-NNN, ...]
    """
    test_to_requirements: Dict[str, List[str]] = {}

    # Pattern for @satisfies tags: // @satisfies STR-XXX-NNN - Description
    satisfies_pattern = re.compile(r'//\s*@satisfies\s+(STR-[A-Z]+-\d+)')

    # Pattern for test names (various formats)
    test_patterns = [
        re.compile(r'TEST_CASE\("([^"]+)"\s*,\s*"\[([^\]]+)\]"\)'),  # Catch2
        re.compile(r'TEST\(([^,]+),\s*([^\)]+)\)'),  # Google Test
        re.compile(r'void\s+(test_\w+)\s*\('),  # Plain functions
    ]

    for base_dir in test_dirs:
        if not base_dir.exists():
            continue
        for test_file in base_dir.rglob('*.cpp'):
            if test_file.name.startswith('test_') or 'tests' in str(test_file.parent).replace('\\', '/'):
                content = test_file.read_text(encoding='utf-8')
                # Build a display path relative to repository root if possible
                try:
                    relative_path = test_file.relative_to(Path.cwd())
                except ValueError:
                    relative_path = test_file

                # Associate @satisfies tags to the next test declaration encountered
                lines = content.split('\n')
                current_satisfies: List[str] = []
                found_any_test_decl = False
                found_any_satisfies = False

                for line in lines:
                    m = satisfies_pattern.search(line)
                    if m:
                        current_satisfies.append(m.group(1))
                        found_any_satisfies = True

                    for pattern in test_patterns:
                        test_match = pattern.search(line)
                        if test_match:
                            if len(test_match.groups()) == 2:
                                test_name = f"{test_match.group(1)}::{test_match.group(2)}"
                            else:
                                test_name = test_match.group(1)
                            test_id = f"{relative_path}::{test_name}"
                            if current_satisfies:
                                test_to_requirements[test_id] = current_satisfies.copy()
                            current_satisfies = []
                            found_any_test_decl = True
                            break

                # Fallback: if we saw @satisfies tags but no test macro, map by file stem
                if found_any_satisfies and not found_any_test_decl:
                    stem = test_file.stem
                    # normalize common prefix
                    if stem.startswith('test_'):
                        pseudo_name = stem[len('test_'):]
                    else:
                        pseudo_name = stem
                    test_id = f"{relative_path}::{pseudo_name}"
                    # If no current_satisfies (because they were flushed), rescan to collect all
                    if not current_satisfies:
                        current_satisfies = [m.group(1) for m in satisfies_pattern.finditer(content)]
                    if current_satisfies:
                        test_to_requirements[test_id] = current_satisfies.copy()

    print(f"🔍 Found {len(test_to_requirements)} tests with @satisfies tags")
    return test_to_requirements


def parse_ctest_results(test_results_file: Path) -> Tuple[Set[str], Set[str]]:
    """Parse CTest results to get passing/failing tests
    
    Returns: (passing_tests, failing_tests) as sets of test names
    """
    passing = set()
    failing = set()
    
    if not test_results_file.exists():
        print(f"⚠️  Test results not found: {test_results_file}", file=sys.stderr)
        return passing, failing
    
    # Try to parse as XML first (CTest XML output)
    try:
        tree = ET.parse(test_results_file)
        root = tree.getroot()
        
        for test in root.findall('.//Test'):
            name_elem = test.find('Name')
            status_elem = test.find('Status')
            
            if name_elem is not None and status_elem is not None:
                test_name = name_elem.text
                status = status_elem.text
                
                if status == 'passed':
                    passing.add(test_name)
                else:
                    failing.add(test_name)
        
        print(f"✅ Parsed CTest XML: {len(passing)} passing, {len(failing)} failing")
        return passing, failing
    
    except ET.ParseError:
        pass  # Not XML, try text format
    
    # Parse text format (LastTest.log)
    try:
        content = test_results_file.read_text(encoding='utf-8')
    except UnicodeDecodeError:
        # Fallback for Windows-encoded logs
        content = test_results_file.read_text(encoding='latin-1', errors='ignore')
    
    # Try common CTest text formats
    # Format A: "Test #N: name .... Passed" (not present in this repo's log)
    legacy_pattern = re.compile(r'Test\s+#\d+:\s+(\S+)\s+\.+\s+(Passed|Failed|\*\*\*Failed)')
    for match in legacy_pattern.finditer(content):
        test_name = match.group(1)
        status = match.group(2)
        if status == 'Passed':
            passing.add(test_name)
        else:
            failing.add(test_name)

    # Format B (observed): blocks like
    # "N/M Testing: test_name" ... later a line "Test Passed." or "Test Failed."
    if not passing and not failing:
        block_pattern = re.compile(r'^(\d+)/(\d+)\s+Testing:\s+([^\r\n]+)\s*\n(?:(?!^\d+/\d+\s+Testing:).)*?^Test\s+(Passed|Failed)\.', re.MULTILINE | re.DOTALL)
        for m in block_pattern.finditer(content):
            name = m.group(3).strip()
            status = m.group(4)
            if status == 'Passed':
                passing.add(name)
            else:
                failing.add(name)

    print(f"✅ Parsed CTest log: {len(passing)} passing, {len(failing)} failing")
    return passing, failing


def link_tests_to_requirements(
    requirements: Dict[str, Requirement],
    test_to_requirements: Dict[str, List[str]],
    passing_tests: Set[str],
    failing_tests: Set[str]
) -> None:
    """Link test results to requirements"""
    
    for test_id, req_ids in test_to_requirements.items():
        # Extract test name from test_id (remove file path)
        test_name = test_id.split('::')[-1]

        # Check if test passed or failed (direct/substring match)
        test_passed = any(test_name in t for t in passing_tests)
        test_failed = any(test_name in t for t in failing_tests)

        # Heuristic: if not matched, try a keyword-based match for names like bmca_* vs file-based names
        if not test_passed and not test_failed:
            # Extract informative token (longest segment > 3 chars)
            tokens = [tok for tok in re.split(r'[_\-]+', test_name) if len(tok) > 3]
            key = max(tokens, key=len) if tokens else test_name
            cand_pass = [t for t in passing_tests if key in t]
            cand_fail = [t for t in failing_tests if key in t]
            # Only accept if unambiguous single candidate
            if len(cand_pass) == 1 and not cand_fail:
                test_passed = True
            elif len(cand_fail) == 1 and not cand_pass:
                test_failed = True
        
        for req_id in req_ids:
            if req_id in requirements:
                req = requirements[req_id]
                req.test_cases.append(test_id)
                
                if test_passed:
                    req.passing_tests.add(test_id)
                elif test_failed:
                    req.failing_tests.add(test_id)


def generate_report(
    requirements: Dict[str, Requirement],
    output_file: Path,
    threshold: float = 75.0
) -> int:
    """Generate traceability report markdown
    
    Returns: 0 if coverage >= threshold, 1 otherwise
    """
    
    # Filter P0/P1 requirements
    p0_p1_requirements = {
        req_id: req for req_id, req in requirements.items()
        if req.priority in ['P0', 'P1']
    }
    
    # Calculate coverage
    total_requirements = len(p0_p1_requirements)
    tested_requirements = sum(1 for req in p0_p1_requirements.values() if req.is_tested)
    
    if total_requirements > 0:
        coverage_pct = (tested_requirements / total_requirements) * 100
    else:
        coverage_pct = 0.0
    
    # Count status breakdown
    status_counts = {
        '✅ PASSING': 0,
        '⚠️ PARTIAL': 0,
        '📋 NO TESTS': 0,
        '❌ FAILING': 0
    }
    
    for req in p0_p1_requirements.values():
        status_counts[req.status] = status_counts.get(req.status, 0) + 1
    
    # Generate report
    report_lines = [
        "# Stakeholder Requirements to Test Cases Traceability Report",
        "",
        f"**Generated**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S UTC')}",
        "**Standard**: IEEE 1012-2016 (Verification and Validation)",
        "",
        "## Summary Statistics",
        "",
        "### Overall Coverage",
        "",
        f"**Total Stakeholder Requirements (P0+P1)**: {total_requirements}",
        f"**Requirements with Passing Tests**: {tested_requirements}",
        f"**Requirements without Tests**: {total_requirements - tested_requirements}",
        "",
        f"**Stakeholder Requirement Coverage**: **{coverage_pct:.1f}%** {'✅' if coverage_pct >= threshold else '❌'}",
        f"**Threshold**: {threshold}%",
        "",
        "### Status Breakdown",
        "",
        "| Status | Count | Percentage |",
        "|--------|-------|------------|",
    ]
    
    for status, count in status_counts.items():
        pct = (count / total_requirements * 100) if total_requirements > 0 else 0
        report_lines.append(f"| {status} | {count} | {pct:.1f}% |")
    
    report_lines.extend([
        "",
        "## Requirements Detail",
        ""
    ])
    
    # Sort requirements by ID
    sorted_reqs = sorted(p0_p1_requirements.items(), key=lambda x: x[0])
    
    for req_id, req in sorted_reqs:
        report_lines.extend([
            f"### {req_id}: {req.title} ({req.priority})",
            "",
            "**Acceptance Criteria**:",
            ""
        ])
        
        for criteria in req.acceptance_criteria:
            report_lines.append(f"- {criteria}")
        
        report_lines.extend([
            "",
            f"**Linked Test Cases**: {len(req.test_cases)} tests "
            f"({len(req.passing_tests)} passing, {len(req.failing_tests)} failing)",
            "",
            "| Test Case | Status |",
            "|-----------|--------|",
        ])
        
        if req.test_cases:
            for test_id in sorted(req.test_cases):
                if test_id in req.passing_tests:
                    status = "✅"
                elif test_id in req.failing_tests:
                    status = "❌"
                else:
                    status = "⚠️"
                
                # Shorten test_id for display
                display_id = test_id.replace('tests/', '').replace('.cpp', '')
                report_lines.append(f"| `{display_id}` | {status} |")
        else:
            report_lines.append("| *(no tests)* | 📋 |")
        
        report_lines.extend([
            "",
            f"**Coverage**: {req.coverage:.0f}%",
            f"**Status**: {req.status}",
            "",
            "---",
            ""
        ])
    
    # Requirements needing attention
    untested = [req for req in p0_p1_requirements.values() if not req.is_tested]
    if untested:
        report_lines.extend([
            "## Requirements Needing Attention",
            ""
        ])
        
        for i, req in enumerate(untested, 1):
            report_lines.append(f"{i}. **{req.req_id}** ({req.priority}): {req.title}")
        
        report_lines.append("")
    
    # Write report
    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text('\n'.join(report_lines), encoding='utf-8')
    
    print(f"\n📊 Traceability Report Generated")
    print(f"   Output: {output_file}")
    print(f"   Coverage: {coverage_pct:.1f}% (threshold: {threshold}%)")
    print(f"   Status: {'✅ PASS' if coverage_pct >= threshold else '❌ FAIL'}")
    
    return 0 if coverage_pct >= threshold else 1


def main():
    parser = argparse.ArgumentParser(
        description='Generate stakeholder requirements to test cases traceability report'
    )
    parser.add_argument(
        '--requirements',
        type=Path,
        required=True,
        help='Path to stakeholder requirements markdown file'
    )
    parser.add_argument(
        '--test-dirs',
        type=str,
        required=True,
        help='Semicolon- or comma-separated list of test directories to scan'
    )
    parser.add_argument(
        '--test-results',
        type=Path,
        required=True,
        help='Path to CTest results (XML or LastTest.log)'
    )
    parser.add_argument(
        '--output',
        type=Path,
        required=True,
        help='Output markdown file path'
    )
    parser.add_argument(
        '--threshold',
        type=float,
        default=75.0,
        help='Minimum coverage threshold percentage (default: 75)'
    )
    parser.add_argument(
        '--fail-under-threshold',
        action='store_true',
        help='Exit with code 1 if coverage < threshold'
    )
    
    args = parser.parse_args()
    
    print("🔍 Generating Traceability Report")
    print(f"   Requirements: {args.requirements}")
    print(f"   Test Directories: {args.test_dirs}")
    print(f"   Test Results: {args.test_results}")
    print(f"   Output: {args.output}")
    print(f"   Threshold: {args.threshold}%")
    print()
    
    # Parse stakeholder requirements
    requirements = parse_stakeholder_requirements(args.requirements)
    if not requirements:
        print("❌ No requirements found", file=sys.stderr)
        return 2
    
    # Extract test metadata
    # Normalize list of test dirs
    raw_list = re.split(r'[;,]', args.test_dirs)
    test_dirs = [Path(p.strip()) for p in raw_list if p.strip()]
    test_to_requirements = extract_test_metadata(test_dirs)
    
    # Parse test results
    passing_tests, failing_tests = parse_ctest_results(args.test_results)
    
    # Link tests to requirements
    link_tests_to_requirements(
        requirements,
        test_to_requirements,
        passing_tests,
        failing_tests
    )
    
    # Generate report
    exit_code = generate_report(
        requirements,
        args.output,
        args.threshold
    )
    
    if args.fail_under_threshold and exit_code != 0:
        return exit_code
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
