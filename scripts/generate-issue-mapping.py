#!/usr/bin/env python3
"""Generate issue mapping JSON from GitHub Issues for link conversion.

Extracts requirement IDs from issue titles and creates mapping:
{
    "REQ-F-001": 46,
    "REQ-F-002": 47,
    "StR-001": 18,
    ...
}

This mapping is used by link-migrated-issues.ps1 to convert text references
to GitHub issue numbers (#N) in issue bodies.
"""

import os
import sys
import json
import re
from github import Github

def extract_requirement_id(title: str) -> str | None:
    """Extract requirement ID from issue title.
    
    Examples:
        "REQ-F-001: User Authentication" → "REQ-F-001"
        "StR-022: Timing Requirements" → "StR-022"
        "REQ-PPS-001: PPS Detection" → "REQ-PPS-001"
    """
    # Match patterns: REQ-F-001, StR-001, REQ-PPS-001, REQ-STK-PTP-001, etc.
    match = re.match(r'^([A-Z]+-[A-Z0-9]+-\d+|[A-Z]+-\d+):', title)
    if match:
        return match.group(1)
    return None

def main():
    token = os.environ.get('GITHUB_TOKEN')
    if not token:
        print("Error: GITHUB_TOKEN environment variable not set", file=sys.stderr)
        return 1
    
    repo_full_name = os.environ.get('GITHUB_REPOSITORY', 'zarfld/IEEE_1588_2019')
    
    print(f"Fetching issues from {repo_full_name}...", file=sys.stderr)
    
    gh = Github(token)
    repo = gh.get_repo(repo_full_name)
    
    mapping = {}
    issues = repo.get_issues(state='all')
    
    for issue in issues:
        req_id = extract_requirement_id(issue.title)
        if req_id:
            mapping[req_id] = issue.number
            print(f"  {req_id} → #{issue.number}: {issue.title[:50]}...", file=sys.stderr)
    
    print(f"\nGenerated mapping for {len(mapping)} requirements", file=sys.stderr)
    
    # Output JSON to stdout
    print(json.dumps(mapping, indent=2, sort_keys=True))
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
