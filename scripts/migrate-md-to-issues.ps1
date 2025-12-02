<#
.SYNOPSIS
    Migrate local Markdown requirements to GitHub Issues
.DESCRIPTION
    Parses Markdown files and creates corresponding GitHub Issues with proper labels, traceability, and content
.PARAMETER RepoOwner
    GitHub repository owner (e.g., 'zarfld')
.PARAMETER RepoName
    GitHub repository name (e.g., 'IEEE_1588_2019')
.PARAMETER SourceFolder
    Folder containing Markdown requirements (e.g., '02-requirements/functional')
.PARAMETER IssueType
    Type of issue to create ('stakeholder-requirement', 'functional-requirement', 'non-functional-requirement', 'architecture-decision', 'test-case')
.PARAMETER DryRun
    If specified, only simulate migration without creating issues
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$RepoOwner,
    
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    
    [Parameter(Mandatory=$true)]
    [string]$SourceFolder,
    
    [Parameter(Mandatory=$true)]
    [ValidateSet('stakeholder-requirement', 'functional-requirement', 'non-functional-requirement', 'architecture-decision', 'test-case')]
    [string]$IssueType,
    
    [switch]$DryRun
)

# Initialize tracking
$migrationLog = @()
$issueMapping = @{}  # Maps file path to issue number

# Get all Markdown files
$mdFiles = Get-ChildItem -Path $SourceFolder -Filter "*.md" -Recurse

Write-Host "Found $($mdFiles.Count) Markdown files to migrate" -ForegroundColor Cyan

foreach ($file in $mdFiles) {
    Write-Host "`nProcessing: $($file.Name)" -ForegroundColor Yellow
    
    # Parse Markdown file
    $content = Get-Content -Path $file.FullName -Raw
    
    # Extract title (first heading)
    if ($content -match '^#\s+(.+)$') {
        $title = $matches[1]
    } else {
        Write-Warning "No title found in $($file.Name), skipping..."
        continue
    }
    
    # Extract sections
    $description = ""
    $priority = "medium"
    $acceptanceCriteria = @()
    $traceabilityParent = ""
    $verifiedBy = @()
    $dependsOn = @()
    
    # Parse Description section
    if ($content -match '(?s)##\s+Description\s*\n(.+?)(?=\n##|\z)') {
        $description = $matches[1].Trim()
    }
    
    # Parse Priority
    if ($content -match '##\s+Priority\s*\n(.+)') {
        $priorityText = $matches[1].Trim()
        $priority = switch -Regex ($priorityText) {
            'Critical|P0' { 'p0' }
            'High|P1' { 'p1' }
            'Medium|P2' { 'p2' }
            'Low|P3' { 'p3' }
            default { 'p2' }
        }
    }
    
    # Parse Acceptance Criteria (task list)
    if ($content -match '(?s)##\s+Acceptance Criteria\s*\n(.+?)(?=\n##|\z)') {
        $criteriaText = $matches[1].Trim()
        $acceptanceCriteria = $criteriaText -split '\n' | Where-Object { $_ -match '^\s*-\s+\[' }
    }
    
    # Parse Traceability - Parent
    if ($content -match 'Parent:\s*(.+)') {
        $traceabilityParent = $matches[1].Trim()
    } elseif ($content -match 'Traces to:\s*(.+)') {
        $traceabilityParent = $matches[1].Trim()
    }
    
    # Parse Verified By
    if ($content -match 'Verified by:\s*(.+)') {
        $verifiedByText = $matches[1].Trim()
        $verifiedBy = $verifiedByText -split '[,\s]+' | Where-Object { $_ -match '\w+' }
    }
    
    # Parse Dependencies
    if ($content -match '(?s)##\s+Dependencies\s*\n(.+?)(?=\n##|\z)') {
        $depsText = $matches[1].Trim()
        $dependsOn = $depsText -split '\n' | Where-Object { $_ -match '^\s*-\s+' } | ForEach-Object { $_.Trim() -replace '^\s*-\s+', '' }
    }
    
    # Build GitHub Issue body
    $issueBody = @"
## Description

$description

## Acceptance Criteria

$($acceptanceCriteria -join "`n")

## Dependencies

$($dependsOn | ForEach-Object { "- $_" } | Out-String)

## Traceability

**Traces to**: $traceabilityParent

**Verified by**: $($verifiedBy -join ', ')

---

**Migrated from**: ``$($file.FullName)``  
**Original commit**: $(git log -1 --format="%H" -- $file.FullName)
"@
    
    # Determine labels
    $labels = @("type:$IssueType", "priority:$priority", "phase:migration", "migrated-from-md")
    
    if ($DryRun) {
        Write-Host "  [DRY RUN] Would create issue:" -ForegroundColor Magenta
        Write-Host "    Title: $title" -ForegroundColor White
        Write-Host "    Labels: $($labels -join ', ')" -ForegroundColor White
        Write-Host "    Body length: $($issueBody.Length) chars" -ForegroundColor White
    } else {
        # Create GitHub Issue
        try {
            $issueJson = gh issue create `
                --repo "$RepoOwner/$RepoName" `
                --title "$title" `
                --body "$issueBody" `
                --label ($labels -join ',') `
                --json number,url | ConvertFrom-Json
            
            $issueNumber = $issueJson.number
            $issueUrl = $issueJson.url
            
            Write-Host "  ✅ Created issue #$issueNumber" -ForegroundColor Green
            Write-Host "     URL: $issueUrl" -ForegroundColor Cyan
            
            # Store mapping
            $issueMapping[$file.FullName] = $issueNumber
            
            # Log migration
            $migrationLog += [PSCustomObject]@{
                FilePath = $file.FullName
                IssueNumber = $issueNumber
                IssueUrl = $issueUrl
                Title = $title
                Type = $IssueType
                Priority = $priority
                Status = 'Migrated'
                Timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
            }
            
            # Add original file as comment (for reference)
            $fileComment = @"
### Original File Content

``````markdown
$content
``````

**File preserved in git history**: ``git log --follow -- $($file.FullName)``
"@
            
            gh issue comment $issueNumber --repo "$RepoOwner/$RepoName" --body "$fileComment" | Out-Null
            
            # Small delay to avoid rate limiting
            Start-Sleep -Milliseconds 500
            
        } catch {
            Write-Error "Failed to create issue for $($file.Name): $_"
            $migrationLog += [PSCustomObject]@{
                FilePath = $file.FullName
                IssueNumber = 'N/A'
                IssueUrl = 'N/A'
                Title = $title
                Type = $IssueType
                Priority = $priority
                Status = "Failed: $_"
                Timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
            }
        }
    }
}

# Export migration log
$logFile = "migration-log-$(Get-Date -Format 'yyyyMMdd-HHmmss').csv"
$migrationLog | Export-Csv -Path $logFile -NoTypeInformation

Write-Host "`n✅ Migration complete!" -ForegroundColor Green
Write-Host "   Log saved to: $logFile" -ForegroundColor Cyan
Write-Host "   Total migrated: $($migrationLog.Where({$_.Status -eq 'Migrated'}).Count)" -ForegroundColor Green
Write-Host "   Total failed: $($migrationLog.Where({$_.Status -like 'Failed*'}).Count)" -ForegroundColor Red

# Export issue mapping (for Phase 3: linking)
$mappingFile = "issue-mapping-$(Get-Date -Format 'yyyyMMdd-HHmmss').json"
$issueMapping | ConvertTo-Json | Out-File -FilePath $mappingFile

Write-Host "   Issue mapping saved to: $mappingFile" -ForegroundColor Cyan
