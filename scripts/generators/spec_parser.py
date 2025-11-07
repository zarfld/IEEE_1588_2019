#!/usr/bin/env python3
"""Spec Parser

Parses requirements and architecture spec markdown files to extract structured
metadata used for generation (tests, trace JSON) and validation.

Enhancement: (2025-11-07) Definition vs Reference Classification
---------------------------------------------------------------
Previously every line beginning with an ID pattern produced an item and any
repeat of that ID (even if only a reference in a linkage table) was counted as
an overall duplicate. This inflated duplicate warnings when the same ID was
legitimately referenced across matrices, linkage plans, user stories, etc.

We now classify each occurrence as either:
    - definition : The canonical declaration of the item
            * Front matter `id:` field
            * A markdown heading whose first token is an ID (e.g. `### REQ-F-001:`)
    - reference  : Any other occurrence (tables, inline lists, narrative refs)

Duplicate detection only considers multiple *definitions* of the same ID.
References no longer trigger duplicate warnings.

Output Augmentation:
    - Each item now includes a `sourceType` field: 'definition' | 'reference'
    - JSON root adds `duplicateDefinitionIds` listing IDs with >1 definition

Input Directories:
    - 02-requirements/** (functional, non-functional, use-cases, user-stories)
    - 03-architecture/** (architecture spec, decisions, views, quality scenarios)

Output:
    - build/spec-index.json : canonical list of items with IDs, type, title, refs

Traceability Model (simplified):
    Stakeholder Need (StR) -> Requirement (REQ-*) -> Design Element (ARC-*, COMP-* )
         -> Test (TEST-*)

Assumptions:
    - Each spec file has YAML front matter with 'id' or lines containing an ID
    - IDs follow taxonomy documented in templates (e.g., REQ-F-001)

"""
from __future__ import annotations
import json, re, sys, hashlib
from pathlib import Path
from typing import Dict, List, Any

ROOT = Path(__file__).resolve().parents[2]
BUILD_DIR = ROOT / 'build'
OUTPUT_FILE = BUILD_DIR / 'spec-index.json'

ID_PATTERN = re.compile(r'^(?P<id>(StR|REQ|ARC|ADR|QA|TEST)-(?:[A-Z]{4}-)?[A-Z0-9][A-Z0-9\-]*)\b')
HEADING_DEF_PATTERN = re.compile(r'^\s{0,3}#{1,6}\s+(?P<id>(StR|REQ|ARC|ADR|QA|TEST)-(?:[A-Z]{4}-)?[A-Z0-9][A-Z0-9\-]*)\b')
# Capture full identifiers with optional 4-char category prefix
# Examples: REQ-AUTH-F-001, StR-CORE-001, ADR-INFRA-001, TEST-LOGIN-001
REF_PATTERN = re.compile(r'\b(?:StR|REQ|ARC|ADR|QA|TEST)-(?:[A-Z]{4}-)?[A-Z0-9][A-Z0-9\-]*\b')

SCAN_DIRS = [
    ROOT / '02-requirements',
    ROOT / '03-architecture',
]

# Additional directories (code/tests) where TEST-* identifiers and inline requirement references
# may appear. We parse these more leniently (no front matter expected) to enrich traceability.
CODE_TEST_DIRS = [
    ROOT / '05-implementation' / 'tests',
]

FRONT_MATTER_RE = re.compile(r'^---\n(.*?)\n---\n', re.DOTALL)

# Files / paths to ignore (instructional, templates, meta guidance) – we do not want
# placeholder IDs here (e.g. REQ-F-XXX) polluting traceability metrics.
IGNORE_PATTERNS = [
    '.github/copilot-instructions.md',
    'ADR-template.md',
    'user-story-template.md',
    'architecture-spec.md',  # template root
    'requirements-spec.md',  # template root
    'spec-kit-templates/',   # entire templates directory
    '/templates/',           # any nested templates folders
    'REQUIREMENTS-ELICITATION-SESSION-',  # workshop report, not canonical spec
    'functional/test-perfect-gen.md',     # generator test file with placeholders
]

def is_ignored(path: Path) -> bool:
    rel = path.relative_to(ROOT).as_posix()
    # quick contains match for any ignore fragment
    return any(p in rel for p in IGNORE_PATTERNS)


def extract_front_matter(text: str) -> Dict[str, Any]:
    m = FRONT_MATTER_RE.match(text)
    if not m:
        return {}
    block = m.group(1)
    data: Dict[str, Any] = {}
    for line in block.splitlines():
        if ':' in line:
            k, v = line.split(':', 1)
            data[k.strip()] = v.strip()
    return data


def parse_file(path: Path) -> List[Dict[str, Any]]:
    text = path.read_text(encoding='utf-8', errors='ignore')
    fm = extract_front_matter(text)
    items: List[Dict[str, Any]] = []
    primary_id = fm.get('id')
    if primary_id:
        items.append(build_item(primary_id, fm.get('title') or path.stem, path, text, source_type='definition'))
    for raw_line in text.splitlines():
        stripped = raw_line.strip('\n')
        heading_match = HEADING_DEF_PATTERN.match(stripped)
        line_core = stripped.strip('# ').strip()
        m = ID_PATTERN.match(line_core)
        if not m:
            continue
        # Extract all valid IDs present in the line (comma separated etc.)
        ids_in_line = [tok for tok in REF_PATTERN.findall(line_core)]
        for idx, id_ in enumerate(ids_in_line):
            if primary_id and id_ == primary_id:
                continue
            if any(i['id'] == id_ for i in items):
                continue
            # Determine if this line constitutes a definition (markdown heading starting with ID)
            is_definition = bool(heading_match and heading_match.group('id') == id_ and idx == 0)
            if idx == 0:
                remainder = line_core.split(id_, 1)[1].strip(' -:,')
                title = remainder or path.stem
            else:
                title = path.stem
            items.append(build_item(id_, title, path, text, source_type='definition' if is_definition else 'reference'))
    return items


def build_item(id_: str, title: str, path: Path, full_text: str, source_type: str = 'reference') -> Dict[str, Any]:
    refs = sorted({r for r in REF_PATTERN.findall(full_text) if r != id_})
    sha = hashlib.sha1(full_text.encode('utf-8')).hexdigest()[:8]
    return {
        'id': id_,
        'title': title,
        'path': str(path.relative_to(ROOT)),
        'references': refs,
        'hash': sha,
        'sourceType': source_type,
    }


def main() -> int:
    BUILD_DIR.mkdir(exist_ok=True)
    all_items: List[Dict[str, Any]] = []
    for base in SCAN_DIRS:
        if not base.exists():
            continue
        for path in base.rglob('*.md'):
            if path.name.startswith('README'):
                continue
            if is_ignored(path):
                continue
            try:
                all_items.extend(parse_file(path))
            except Exception as e:
                print(f"WARN: failed to parse {path}: {e}", file=sys.stderr)

    # Parse test source files for TEST-* identifiers and requirement references.
    test_id_pattern = re.compile(r'\b(TEST-[A-Z0-9\-]+)\b')
    req_ref_pattern = re.compile(r'\b(REQ-[A-Z0-9\-]+)\b')
    for tdir in CODE_TEST_DIRS:
        if not tdir.exists():
            continue
        for ext in ('*.cpp','*.cc','*.c','*.hpp','*.h','*.py'):
            for src in tdir.rglob(ext):
                try:
                    text = src.read_text(encoding='utf-8', errors='ignore')
                except Exception as e:
                    print(f"WARN: failed to read test file {src}: {e}", file=sys.stderr)
                    continue
                test_ids = sorted(set(test_id_pattern.findall(text)))
                if not test_ids:
                    continue
                req_refs = sorted({r for r in req_ref_pattern.findall(text)})
                for tid in test_ids:
                    all_items.append({
                        'id': tid,
                        'title': src.stem,
                        'path': str(src.relative_to(ROOT)),
                        'references': req_refs,
                        'hash': hashlib.sha1((tid+text).encode('utf-8')).hexdigest()[:8],
                    })
    # De-duplicate by ID (first occurrence kept) while separately tracking duplicate *definitions*
    seen: Dict[str, int] = {}
    dedup: List[Dict[str, Any]] = []
    duplicate_definitions: Dict[str, int] = {}
    for item in all_items:
        iid = item['id']
        if iid not in seen:
            seen[iid] = 1
            dedup.append(item)
        else:
            # Only flag if this is an additional definition (not just a reference)
            if item.get('sourceType') == 'definition':
                duplicate_definitions[iid] = duplicate_definitions.get(iid, 1) + 1
            # References are ignored for duplicate purposes
            continue
    if duplicate_definitions:
        print("⚠️ Duplicate definition(s) detected (keeping first definition occurrence):", file=sys.stderr)
        for k, count in duplicate_definitions.items():
            print(f"  - {k} (extra definitions: {count})", file=sys.stderr)
    OUTPUT_FILE.write_text(
        json.dumps({
            'items': dedup,
            'duplicateDefinitionIds': list(duplicate_definitions.keys()),
            'ignoredPatterns': IGNORE_PATTERNS
        }, indent=2),
        encoding='utf-8'
    )
    print(f"Wrote {OUTPUT_FILE} with {len(dedup)} unique items (duplicate definitions: {len(duplicate_definitions)}) (ignored patterns active)")
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
