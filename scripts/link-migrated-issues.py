#!/usr/bin/env python3
"""Establish traceability links between migrated GitHub Issues.

Updates issue bodies to replace text references (REQ-F-001) with GitHub
issue number links (#215) using the issue mapping JSON.

Usage:
    python scripts/link-migrated-issues.py issue-mapping-2025-12-02.json
"""

import os
import sys
import json
import re
import time
from pathlib import Path
from github import Github

def load_mapping(mapping_file: Path) -> dict:
    """Load ID → Issue Number mapping from JSON file."""
    with open(mapping_file, 'r', encoding='utf-8') as f:
        return json.load(f)

def replace_text_refs_with_links(body: str, id_to_issue: dict) -> tuple[str, int]:
    """Replace text requirement IDs with GitHub issue links.
    
    Returns:
        tuple: (updated_body, replacement_count)
    """
    updated_body = body
    replacements = 0
    
    # Sort IDs by length (longest first) to avoid partial replacements
    # e.g., replace "REQ-STK-PTP-001" before "REQ-STK"
    sorted_ids = sorted(id_to_issue.keys(), key=len, reverse=True)
    
    for req_id in sorted_ids:
        issue_num = id_to_issue[req_id]
        
        # Create regex pattern that matches the ID but not when already #N
        # Negative lookbehind for # to avoid replacing already-linked refs
        pattern = rf'(?<!#)\b{re.escape(req_id)}\b'
        
        # Count matches first
        matches = re.findall(pattern, updated_body)
        if matches:
            # Replace with GitHub issue link
            updated_body = re.sub(pattern, f'#{issue_num}', updated_body)
            replacements += len(matches)
            print(f"  Replaced {req_id} → #{issue_num} ({len(matches)} occurrences)")
    
    return updated_body, replacements

def main():
    if len(sys.argv) < 2:
        print("Usage: python link-migrated-issues.py <mapping-file.json> [--dry-run]", file=sys.stderr)
        return 1
    
    mapping_file = Path(sys.argv[1])
    if not mapping_file.exists():
        print(f"Error: Mapping file not found: {mapping_file}", file=sys.stderr)
        return 1
    
    # Check for dry-run flag
    dry_run = '--dry-run' in sys.argv or os.environ.get('DRY_RUN', '').lower() == 'true'
    
    # Get GitHub credentials
    token = os.environ.get('GITHUB_TOKEN')
    if not token:
        print("Error: GITHUB_TOKEN environment variable not set", file=sys.stderr)
        return 1
    
    repo_full_name = os.environ.get('GITHUB_REPOSITORY', 'zarfld/IEEE_1588_2019')
    
    print(f"🔗 Establishing traceability links in {repo_full_name}...")
    print(f"📄 Loading mapping from {mapping_file}")
    
    # Load mapping
    id_to_issue = load_mapping(mapping_file)
    print(f"✅ Loaded {len(id_to_issue)} ID → Issue mappings\n")
    
    # Connect to GitHub
    gh = Github(token)
    repo = gh.get_repo(repo_full_name)
    
    # Process each issue in the mapping
    total_issues = len(set(id_to_issue.values()))
    processed = 0
    updated = 0
    total_replacements = 0
    
    # Get unique issue numbers
    issue_numbers = sorted(set(id_to_issue.values()))
    
    for issue_num in issue_numbers:
        processed += 1
        print(f"\n[{processed}/{total_issues}] Processing issue #{issue_num}...")
        
        try:
            issue = repo.get_issue(issue_num)
            original_body = issue.body or ""
            
            # Replace text references with #N links
            updated_body, replacements = replace_text_refs_with_links(original_body, id_to_issue)
            
            if replacements > 0:
                if dry_run:
                    print(f"  🔍 Would update issue #{issue_num} ({replacements} links) [DRY RUN]")
                else:
                    # Update issue body
                    issue.edit(body=updated_body)
                    updated += 1
                    total_replacements += replacements
                    print(f"  ✅ Updated issue #{issue_num} ({replacements} links)")
                    
                    # Rate limiting - be nice to GitHub API
                    time.sleep(0.5)
            else:
                print(f"  ⏭️  No changes needed for issue #{issue_num}")
        
        except Exception as e:
            print(f"  ❌ Error updating issue #{issue_num}: {e}", file=sys.stderr)
            continue
    
    print(f"\n{'='*60}")
    if dry_run:
        print(f"🔍 Dry run complete - no issues were modified")
    else:
        print(f"✅ Traceability linking complete!")
    print(f"{'='*60}")
    print(f"Processed: {processed} issues")
    if dry_run:
        print(f"Would update: {updated} issues")
    else:
        print(f"Updated: {updated} issues")
    print(f"Total replacements: {total_replacements} links")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
