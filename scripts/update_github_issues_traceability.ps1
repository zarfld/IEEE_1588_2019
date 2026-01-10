# Update GitHub Issues with Traceability Information
# Generated from: reports/tracability-mapping.md
# Date: 2025-12-03

$owner = "zarfld"
$repo = "IEEE_1588_2019"

# GitHub Personal Access Token (set this environment variable)
$token = $env:GITHUB_TOKEN
if (-not $token) {
    Write-Error "Please set GITHUB_TOKEN environment variable with your GitHub Personal Access Token"
    exit 1
}

$headers = @{
    "Authorization" = "token $token"
    "Accept" = "application/vnd.github.v3+json"
}

function Add-TraceabilityComment {
    param(
        [int]$IssueNumber,
        [string]$RequirementId,
        [string]$SpecMapping,
        [string]$ParentRequirements,
        [string]$ChildRequirements,
        [string]$ADRReferences,
        [string]$ARCCReferences,
        [string]$QASCReferences,
        [string]$TESTReferences
    )
    
    $body = "## 📊 Enhanced Traceability (Auto-Generated)`n`n"
    $body += "### Requirement Details`n"
    
    if ($RequirementId -and $RequirementId -ne "N/A") {
        $body += "- **Requirement ID**: $RequirementId`n"
    }
    
    if ($SpecMapping -and $SpecMapping -ne "-") {
        $body += "- **Spec STR Mapping**: $SpecMapping`n"
    }
    
    $body += "- **GitHub Issue**: #$IssueNumber`n`n"
    
    $body += "### Traceability Links`n"
    
    if ($ParentRequirements -and $ParentRequirements -ne "-") {
        $body += "**Parent Requirements (Traces to)**:`n"
        $parents = $ParentRequirements -split ',' | ForEach-Object { $_.Trim() }
        foreach ($parent in $parents) {
            if ($parent -match '#(\d+)') {
                $body += "- $parent`n"
            } else {
                $body += "- $parent`n"
            }
        }
        $body += "`n"
    }
    
    if ($ChildRequirements -and $ChildRequirements -ne "-") {
        $body += "**Child Requirements (Refined by)**:`n"
        $children = $ChildRequirements -split ',' | ForEach-Object { $_.Trim() }
        foreach ($child in $children) {
            if ($child -match '#(\d+)') {
                $body += "- $child`n"
            } else {
                $body += "- $child`n"
            }
        }
        $body += "`n"
    }
    
    if ($ADRReferences -and $ADRReferences -ne "-") {
        $body += "**Architecture Decisions**:`n"
        $adrs = $ADRReferences -split ',' | ForEach-Object { $_.Trim() }
        foreach ($adr in $adrs) {
            $body += "- $adr`n"
        }
        $body += "`n"
    }
    
    if ($ARCCReferences -and $ARCCReferences -ne "-") {
        $body += "**Architecture Components**:`n"
        $arcs = $ARCCReferences -split ',' | ForEach-Object { $_.Trim() }
        foreach ($arc in $arcs) {
            $body += "- $arc`n"
        }
        $body += "`n"
    }
    
    if ($TESTReferences -and $TESTReferences -ne "-") {
        $body += "**Test References**:`n"
        $tests = $TESTReferences -split ',' | ForEach-Object { $_.Trim() }
        foreach ($test in $tests) {
            $body += "- $test`n"
        }
        $body += "`n"
    }
    
    $body += "### References`n"
    $body += "- **Standards**: ISO/IEC/IEEE 29148:2018, IEEE 1588-2019`n"
    $body += "- **Documentation**: See [Traceability Matrix](../reports/tracability-mapping.md)`n`n"
    $body += "---`n"
    $body += "*This traceability information was automatically generated from the project's requirements traceability matrix.*"
    
    $commentData = @{
        body = $body
    } | ConvertTo-Json
    
    try {
        $uri = "https://api.github.com/repos/$owner/$repo/issues/$IssueNumber/comments"
        $response = Invoke-RestMethod -Uri $uri -Method Post -Headers $headers -Body $commentData -ContentType "application/json"
        Write-Host "✅ Updated issue #$IssueNumber" -ForegroundColor Green
        return $true
    }
    catch {
        Write-Host "❌ Failed to update issue #$IssueNumber : $_" -ForegroundColor Red
        return $false
    }
}

# Issue update data from tracability-mapping.md
# Format: IssueNumber, RequirementId, SpecMapping, ParentRequirements, ChildRequirements, ADR, ARC-C, QA-SC, TEST

$issues = @(
    @{N=16; Req="REQ-STK-PTP-002"; Spec="✅ STR-PERF-002"; Parent="-"; Child="REQ-SYS-PTP-005, REQ-SYS-PTP-007"; ADR="-"; ARC="-"; QA="-"; TEST="-"},
    @{N=17; Req="REQ-STK-PTP-004"; Spec="✅ STR-PORT-001"; Parent="-"; Child="REQ-SYS-PTP-006"; ADR="-"; ARC="-"; QA="-"; TEST="-"},
    @{N=18; Req="REQ-STK-PTP-001"; Spec="✅ STR-PERF-001"; Parent="-"; Child="REQ-SYS-PTP-001"; ADR="-"; ARC="-"; QA="-"; TEST="-"},
    @{N=19; Req="REQ-STK-PTP-010"; Spec="✅ STR-PERF-003"; Parent="-"; Child="-"; ADR="-"; ARC="-"; QA="-"; TEST="-"},
    @{N=20; Req="REQ-STK-PTP-009"; Spec="❌ None (Post-MVP)"; Parent="-"; Child="REQ-SYS-PTP-002"; ADR="-"; ARC="-"; QA="-"; TEST="-"},
    @{N=21; Req="REQ-STK-PTP-006"; Spec="✅ STR-SEC-001"; Parent="-"; Child="REQ-SYS-PTP-003"; ADR="-"; ARC="-"; QA="-"; TEST="-"},
    @{N=22; Req="REQ-STK-PTP-008"; Spec="❌ None (Post-MVP)"; Parent="-"; Child="REQ-SYS-PTP-004"; ADR="-"; ARC="-"; QA="-"; TEST="-"},
    @{N=34; Req="REQ-SYS-PTP-002"; Spec="❌ None (Post-MVP)"; Parent="#19 (REQ-STK-PTP-003), #20 (REQ-STK-PTP-009)"; Child="-"; ADR="ADR-013"; ARC="ARCH-1588-003-MultiDomain"; QA="-"; TEST="-"},
    @{N=35; Req="REQ-SYS-PTP-005"; Spec="✅ STR-PERF-002"; Parent="#16, #24, #28, #30, REQ-NF-P-002"; Child="-"; ADR="-"; ARC="ARCH-1588-002-StateMachine"; QA="-"; TEST="TEST-1588-STATE-001"},
    @{N=36; Req="REQ-SYS-PTP-001"; Spec="✅ STR-PERF-001"; Parent="#18, REQ-NF-P-001, REQ-F-003, REQ-F-004"; Child="-"; ADR="-"; ARC="ARCH-1588-002-StateMachine"; QA="-"; TEST="TEST-1588-STATE-001"},
    @{N=42; Req="REQ-SYS-PTP-006"; Spec="✅ STR-PORT-001"; Parent="#17, #26, REQ-F-005, REQ-NF-M-001"; Child="-"; ADR="ADR-001"; ARC="ARCH-1588-001-HAL"; QA="-"; TEST="-"}
)

Write-Host "`n🚀 Starting GitHub Issues Traceability Update`n" -ForegroundColor Cyan
Write-Host "Repository: $owner/$repo" -ForegroundColor Yellow
Write-Host "Total issues to update: $($issues.Count)`n" -ForegroundColor Yellow

$successCount = 0
$failCount = 0

foreach ($issue in $issues) {
    $success = Add-TraceabilityComment `
        -IssueNumber $issue.N `
        -RequirementId $issue.Req `
        -SpecMapping $issue.Spec `
        -ParentRequirements $issue.Parent `
        -ChildRequirements $issue.Child `
        -ADRReferences $issue.ADR `
        -ARCCReferences $issue.ARC `
        -QASCReferences $issue.QA `
        -TESTReferences $issue.TEST
    
    if ($success) {
        $successCount++
    } else {
        $failCount++
    }
    
    # Rate limiting: GitHub allows 5000 requests/hour for authenticated users
    Start-Sleep -Milliseconds 500
}

Write-Host "`n📊 Summary:" -ForegroundColor Cyan
Write-Host "  ✅ Successfully updated: $successCount issues" -ForegroundColor Green
Write-Host "  ❌ Failed: $failCount issues" -ForegroundColor Red
Write-Host "`nDone!`n" -ForegroundColor Cyan
