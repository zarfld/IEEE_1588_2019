<#
.SYNOPSIS
    Establish traceability links between migrated GitHub Issues
.DESCRIPTION
    Updates issue descriptions to replace text references with GitHub Issue links (#N)
.PARAMETER MappingFile
    JSON file from Phase 2 migration (issue-mapping-*.json)
.PARAMETER RepoOwner
    GitHub repository owner
.PARAMETER RepoName
    GitHub repository name
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$MappingFile,
    
    [Parameter(Mandatory=$true)]
    [string]$RepoOwner,
    
    [Parameter(Mandatory=$true)]
    [string]$RepoName
)

# Load issue mapping (FilePath → IssueNumber)
$mapping = Get-Content -Path $MappingFile | ConvertFrom-Json

# Build reverse mapping (ID → IssueNumber)
$idToIssue = @{}
foreach ($entry in $mapping.PSObject.Properties) {
    $filePath = $entry.Name
    $issueNumber = $entry.Value
    
    # Extract ID from file path (e.g., REQ-F-001 from "REQ-F-001-user-authentication.md")
    if ($filePath -match '([A-Z]+-[A-Z]+-\d+|[A-Z]+-\d+)') {
        $id = $matches[1]
        $idToIssue[$id] = $issueNumber
    }
}

Write-Host "Loaded $($idToIssue.Count) ID → Issue mappings" -ForegroundColor Cyan

# Process each migrated issue
foreach ($entry in $mapping.PSObject.Properties) {
    $issueNumber = $entry.Value
    
    Write-Host "`nProcessing issue #$issueNumber..." -ForegroundColor Yellow
    
    # Get current issue body
    $issue = gh issue view $issueNumber --repo "$RepoOwner/$RepoName" --json body | ConvertFrom-Json
    $body = $issue.body
    
    # Replace text references with #N links
    $updatedBody = $body
    $replacements = 0
    
    foreach ($id in $idToIssue.Keys) {
        $targetIssue = $idToIssue[$id]
        
        # Replace patterns like "REQ-F-001" or "Traces to: REQ-F-001"
        if ($updatedBody -match [regex]::Escape($id)) {
            $updatedBody = $updatedBody -replace [regex]::Escape($id), "#$targetIssue"
            $replacements++
            Write-Host "  Replaced $id → #$targetIssue" -ForegroundColor Green
        }
    }
    
    if ($replacements -gt 0) {
        # Update issue body
        gh issue edit $issueNumber --repo "$RepoOwner/$RepoName" --body "$updatedBody"
        Write-Host "  ✅ Updated issue #$issueNumber ($replacements links)" -ForegroundColor Green
    } else {
        Write-Host "  ⏭️  No changes needed for issue #$issueNumber" -ForegroundColor Gray
    }
    
    Start-Sleep -Milliseconds 300
}

Write-Host "`n✅ Traceability linking complete!" -ForegroundColor Green
