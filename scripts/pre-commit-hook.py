#!/usr/bin/env python3
"""Pre-commit hook for spec validation and traceability checks.

This hook performs two layers of checks:
1) Per-file structural validation (YAML front matter + schema) for staged spec files
2) End-to-end traceability pipeline checks mirroring CI (spec parser → trace JSON → reports → validators)

Install:
    Copy to .git/hooks/pre-commit (or use with pre-commit framework)
    chmod +x .git/hooks/pre-commit

Usage with pre-commit framework:
    Add to .pre-commit-config.yaml:
        - repo: local
            hooks:
                - id: validate-specs
                    name: Validate Specification Files & Traceability
                    entry: python scripts/pre-commit-hook.py
                    language: python
                    files: .*
                    pass_filenames: false
"""
from __future__ import annotations
import os
import sys
import subprocess
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

@dataclass
class ValidationIssue:
    file: Path
    message: str


def run_py(script_rel: str, args: list[str] | None = None) -> int:
    """Run a repository Python script via the current interpreter, streaming output.

    Returns the subprocess return code.
    """
    cmd = [sys.executable, str(ROOT / script_rel)] + (args or [])
    print(f"▶️  {' '.join(cmd)}")
    proc = subprocess.run(cmd, cwd=ROOT)
    return proc.returncode


def get_staged_spec_files() -> list[Path]:
    """Get list of staged markdown files that might be specs."""
    try:
        result = subprocess.run(
            ['git', 'diff', '--cached', '--name-only', '--diff-filter=ACM'],
            capture_output=True,
            text=True,
            check=True
        )
        
        staged_files = result.stdout.strip().split('\n')
        
        # Filter for markdown files in phase directories
        spec_files = []
        for file_str in staged_files:
            if not file_str:
                continue
            
            file_path = ROOT / file_str
            
            # Skip if not markdown
            if not file_path.suffix == '.md':
                continue
            
            # Skip READMEs and templates
            name_lower = file_path.name.lower()
            if 'readme' in name_lower or 'template' in name_lower:
                continue
            
            # Check if in phase directory
            parts = file_path.parts
            if any(p.startswith(('01-', '02-', '03-', '04-', '05-', '06-', '07-', '08-', '09-')) for p in parts):
                spec_files.append(file_path)
        
        return spec_files
    
    except subprocess.CalledProcessError as e:
        print(f"❌ Failed to get staged files: {e}", file=sys.stderr)
        return []


def get_staged_paths() -> list[Path]:
    try:
        result = subprocess.run(
            ['git', 'diff', '--cached', '--name-only', '--diff-filter=ACMRT'],
            capture_output=True,
            text=True,
            check=True,
            cwd=ROOT,
        )
        files = [f for f in result.stdout.strip().split('\n') if f]
        return [ROOT / f for f in files]
    except subprocess.CalledProcessError:
        return []


def staged_affects_traceability(staged: list[Path]) -> bool:
    critical_dirs = [
        ROOT / '02-requirements',
        ROOT / '03-architecture',
        ROOT / '05-implementation' / 'tests',
        ROOT / 'scripts',
        ROOT / 'spec-kit-templates',
    ]
    critical_files = {
        ROOT / 'scripts' / 'validate-spec-structure.py',
        ROOT / 'scripts' / 'generate-traceability-matrix.py',
        ROOT / 'scripts' / 'validate-traceability.py',
        ROOT / 'scripts' / 'validate-trace-coverage.py',
        ROOT / 'scripts' / 'generators' / 'spec_parser.py',
        ROOT / 'scripts' / 'generators' / 'build_trace_json.py',
    }
    for p in staged:
        if p in critical_files:
            return True
        if any(str(p).startswith(str(d) + os.sep) for d in critical_dirs):
            return True
    return False


def run_per_file_structure_validation(staged_files: list[Path]) -> list[ValidationIssue]:
    """Run validate-spec-structure.py in batch mode (scripts handles discovery).

    We don't import the module (hyphenated filename); execute as a script.
    Returns list of issues (empty if ok).
    """
    print("� Running structural validation (YAML + schema) ...")
    rc = run_py('scripts/validate-spec-structure.py')
    if rc != 0:
        # We cannot get detailed issues without importing; present a generic failure.
        return [ValidationIssue(ROOT / 'scripts/validate-spec-structure.py', 'Structural validation failed')]
    return []


def run_traceability_pipeline() -> list[ValidationIssue]:
    print("🏗️  Building spec index ...")
    issues: list[ValidationIssue] = []
    if run_py('scripts/generators/spec_parser.py') != 0:
        issues.append(ValidationIssue(ROOT / 'scripts/generators/spec_parser.py', 'spec_parser failed'))
        return issues

    print("🧱 Building traceability.json ...")
    if run_py('scripts/generators/build_trace_json.py') != 0:
        issues.append(ValidationIssue(ROOT / 'scripts/generators/build_trace_json.py', 'build_trace_json failed'))
        return issues

    print("📊 Generating matrix & orphan reports ...")
    if run_py('scripts/generate-traceability-matrix.py') != 0:
        issues.append(ValidationIssue(ROOT / 'scripts/generate-traceability-matrix.py', 'matrix generation failed'))
        return issues

    print("✅ Running basic traceability validator ...")
    if run_py('scripts/validate-traceability.py') != 0:
        issues.append(ValidationIssue(ROOT / 'scripts/validate-traceability.py', 'traceability validation failed'))
        return issues

    # Coverage thresholds can be tuned via env vars if needed
    min_req = os.environ.get('TRACE_MIN_REQ', '80')
    min_adr = os.environ.get('TRACE_MIN_REQ_ADR', '70')
    min_scn = os.environ.get('TRACE_MIN_REQ_SCENARIO', '60')
    min_tst = os.environ.get('TRACE_MIN_REQ_TEST', '40')

    print("📈 Enforcing coverage thresholds ...")
    rc = run_py('scripts/validate-trace-coverage.py', [
        '--min-req', str(min_req),
        '--min-req-adr', str(min_adr),
        '--min-req-scenario', str(min_scn),
        '--min-req-test', str(min_tst),
    ])
    if rc != 0:
        issues.append(ValidationIssue(ROOT / 'scripts/validate-trace-coverage.py', 'coverage thresholds failed'))
    return issues


def main() -> int:
    print("🔍 Validating staged specification files...")
    staged_files = get_staged_spec_files()
    staged_all = get_staged_paths()

    # Always run structural validation if any spec files are staged
    issues: list[ValidationIssue] = []
    if staged_files:
        print(f"📋 Found {len(staged_files)} spec file(s) staged")
        issues.extend(run_per_file_structure_validation(staged_files))
    else:
        print("ℹ️ No staged spec files detected for per-file structural validation")

    # Run full traceability pipeline if staged changes affect specs/tests/scripts
    if staged_affects_traceability(staged_all):
        print("🧪 Running full traceability pipeline (pre-commit)")
        issues.extend(run_traceability_pipeline())
    else:
        print("ℹ️ No staged changes impacting traceability; skipping pipeline run")

    print("\n" + "=" * 60)
    if issues:
        print(f"❌ Pre-commit checks failed with {len(issues)} issue(s):")
        for it in issues:
            print(f" - {it.file.relative_to(ROOT)}: {it.message}")
        print("\nFix the issues above and try committing again.")
        print("To bypass (not recommended): git commit --no-verify")
        return 1
    print("✅ Pre-commit checks passed")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
