#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Local traceability validation script (mirrors GitHub Actions workflow)
.DESCRIPTION
    Validates requirements traceability without needing to push to GitHub.
    Useful for pre-commit validation and local development.
.PARAMETER GenerateReport
    Generate full traceability report (default: true)
.EXAMPLE
    .\validate-traceability-local.ps1
    .\validate-traceability-local.ps1 -GenerateReport $false
#>

param(
    [Parameter(Mandatory=$false)]
    [bool]$GenerateReport = $true
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

# Configuration
$MIN_TRACEABILITY_COVERAGE = 95
$PYTHON_VERSION = "3.11"
$REPO_OWNER = "zarfld"
$REPO_NAME = "IEEE_1588_2019"

Write-Host "🔗 Requirements Traceability Validation" -ForegroundColor Cyan
Write-Host "Standard: ISO/IEC/IEEE 29148:2018" -ForegroundColor Gray
Write-Host ""

# Check Python installation
Write-Host "🐍 Checking Python installation..." -ForegroundColor Yellow
try {
    $pythonVersion = python --version 2>&1
    Write-Host "  ✓ $pythonVersion" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Python not found. Please install Python $PYTHON_VERSION" -ForegroundColor Red
    exit 1
}

# Check for required packages
Write-Host "📦 Checking Python dependencies..." -ForegroundColor Yellow
$requiredPackages = @("requests", "PyGithub", "pyyaml")
foreach ($package in $requiredPackages) {
    $installed = python -c "import $package" 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ $package" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $package not found, installing..." -ForegroundColor Yellow
        pip install $package --quiet
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  ✓ $package installed" -ForegroundColor Green
        } else {
            Write-Host "  ✗ Failed to install $package" -ForegroundColor Red
            exit 1
        }
    }
}

# Check GitHub token
Write-Host "🔑 Checking GitHub authentication..." -ForegroundColor Yellow
if ($env:GITHUB_TOKEN) {
    Write-Host "  ✓ GITHUB_TOKEN environment variable set" -ForegroundColor Green
} else {
    Write-Host "  ⚠️  GITHUB_TOKEN not set, checking gh CLI..." -ForegroundColor Yellow
    try {
        $ghStatus = gh auth status 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  ✓ GitHub CLI authenticated" -ForegroundColor Green
            # Export token for Python script
            $env:GITHUB_TOKEN = (gh auth token)
        } else {
            Write-Host "  ✗ GitHub authentication required" -ForegroundColor Red
            Write-Host "     Run: gh auth login" -ForegroundColor Gray
            Write-Host "     Or set: `$env:GITHUB_TOKEN = 'ghp_xxx'" -ForegroundColor Gray
            exit 1
        }
    } catch {
        Write-Host "  ✗ GitHub CLI not available and GITHUB_TOKEN not set" -ForegroundColor Red
        Write-Host "     Install gh CLI: winget install GitHub.cli" -ForegroundColor Gray
        Write-Host "     Or set: `$env:GITHUB_TOKEN = 'ghp_xxx'" -ForegroundColor Gray
        exit 1
    }
}

# Set environment variables for Python script
$env:GITHUB_REPOSITORY = "$REPO_OWNER/$REPO_NAME"
$env:REPO_OWNER = $REPO_OWNER
$env:REPO_NAME = $REPO_NAME

# Create reports directory
if (-not (Test-Path "reports")) {
    New-Item -ItemType Directory -Path "reports" | Out-Null
}

Write-Host ""
Write-Host "📊 Generating traceability report from GitHub Issues..." -ForegroundColor Yellow

# Generate traceability report
$reportPath = "reports/traceability-matrix.md"
try {
    python scripts/github-traceability-report.py > $reportPath
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  ✗ Failed to generate traceability report" -ForegroundColor Red
        exit 1
    }
    Write-Host "  ✓ Traceability report generated: $reportPath" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Error generating report: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "📈 Extracting traceability statistics..." -ForegroundColor Yellow

# Extract statistics from report
$reportContent = Get-Content $reportPath -Raw

# Total requirements
if ($reportContent -match 'Total requirements:\s+\*\*(\d+)\*\*') {
    $totalReqs = [int]$matches[1]
    Write-Host "  Total Requirements: $totalReqs" -ForegroundColor Cyan
} else {
    Write-Host "  ✗ Could not extract total requirements" -ForegroundColor Red
    $totalReqs = 0
}

# Orphaned requirements
$orphansSection = $reportContent -split '## Orphaned Requirements' | Select-Object -Last 1
$orphansSection = $orphansSection -split '##' | Select-Object -First 1
$orphanCount = ([regex]::Matches($orphansSection, '^\| #', [System.Text.RegularExpressions.RegexOptions]::Multiline)).Count
Write-Host "  Orphaned Requirements: $orphanCount" -ForegroundColor $(if ($orphanCount -eq 0) { "Green" } else { "Yellow" })

# Unverified requirements
$unverifiedSection = $reportContent -split '## Requirements Without Tests' | Select-Object -Last 1
$unverifiedSection = $unverifiedSection -split '##' | Select-Object -First 1
$unverifiedCount = ([regex]::Matches($unverifiedSection, '^\| #', [System.Text.RegularExpressions.RegexOptions]::Multiline)).Count
Write-Host "  Unverified Requirements: $unverifiedCount" -ForegroundColor $(if ($unverifiedCount -eq 0) { "Green" } else { "Yellow" })

# Calculate coverage
if ($totalReqs -gt 0) {
    $coverage = [math]::Round((($totalReqs - $orphanCount) * 100.0) / $totalReqs, 2)
    Write-Host "  Traceability Coverage: $coverage%" -ForegroundColor $(if ($coverage -ge $MIN_TRACEABILITY_COVERAGE) { "Green" } else { "Red" })
} else {
    $coverage = 0
    Write-Host "  ⚠️  No requirements found in GitHub Issues" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "🔍 Validating traceability coverage..." -ForegroundColor Yellow

# Validate coverage threshold
$validationPassed = $true

if ($totalReqs -eq 0) {
    Write-Host "  ⚠️  No requirements found in GitHub Issues" -ForegroundColor Yellow
    Write-Host "     Expected issues with labels: type:stakeholder-requirement, type:requirement:functional, etc." -ForegroundColor Gray
    $validationPassed = $false
} elseif ($coverage -lt $MIN_TRACEABILITY_COVERAGE) {
    Write-Host "  ✗ Traceability coverage ${coverage}% is below minimum ${MIN_TRACEABILITY_COVERAGE}%" -ForegroundColor Red
    Write-Host ""
    Write-Host "  Orphaned requirements found:" -ForegroundColor Yellow
    $orphansLines = $reportContent -split "`n" | Where-Object { $_ -match '^\| #' -and $_ -notmatch '^\| Issue' }
    $orphansLines | Select-Object -First 10 | ForEach-Object { Write-Host "    $_" -ForegroundColor Gray }
    if ($orphansLines.Count -gt 10) {
        Write-Host "    ... and $($orphansLines.Count - 10) more" -ForegroundColor Gray
    }
    $validationPassed = $false
} else {
    Write-Host "  ✓ Traceability coverage ${coverage}% meets minimum ${MIN_TRACEABILITY_COVERAGE}%" -ForegroundColor Green
}

# Check orphans
Write-Host ""
if ($orphanCount -gt 0) {
    Write-Host "⚠️  Warning: $orphanCount orphaned requirements found" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Requirements without parent links:" -ForegroundColor Yellow
    $orphansTable = $reportContent -split '## Orphaned Requirements' | Select-Object -Last 1
    $orphansTable = $orphansTable -split '##' | Select-Object -First 1
    $orphansLines = $orphansTable -split "`n" | Where-Object { $_ -match '^\| #' -and $_ -notmatch '^\| Issue' }
    $orphansLines | Select-Object -First 10 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
    if ($orphansLines.Count -gt 10) {
        Write-Host "  ... and $($orphansLines.Count - 10) more" -ForegroundColor Gray
    }
    Write-Host ""
    Write-Host "💡 Action: Add 'Traces to: #N' links to orphaned requirements" -ForegroundColor Cyan
} else {
    Write-Host "✅ No orphaned requirements found" -ForegroundColor Green
}

# Summary
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Gray
Write-Host "📋 VALIDATION SUMMARY" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Gray
Write-Host ""
Write-Host "  Total Requirements:        $totalReqs" -ForegroundColor White
Write-Host "  Orphaned:                  $orphanCount" -ForegroundColor $(if ($orphanCount -eq 0) { "Green" } else { "Yellow" })
Write-Host "  Unverified:                $unverifiedCount" -ForegroundColor $(if ($unverifiedCount -eq 0) { "Green" } else { "Yellow" })
Write-Host "  Traceability Coverage:     $coverage% $(if ($coverage -ge $MIN_TRACEABILITY_COVERAGE) { '✓' } else { '✗' })" -ForegroundColor $(if ($coverage -ge $MIN_TRACEABILITY_COVERAGE) { "Green" } else { "Red" })
Write-Host "  Minimum Required:          ${MIN_TRACEABILITY_COVERAGE}%" -ForegroundColor Gray
Write-Host ""
Write-Host "  Report Location:           $reportPath" -ForegroundColor Cyan
Write-Host ""

if ($validationPassed) {
    Write-Host "✅ TRACEABILITY VALIDATION PASSED" -ForegroundColor Green
    exit 0
} else {
    Write-Host "❌ TRACEABILITY VALIDATION FAILED" -ForegroundColor Red
    Write-Host ""
    Write-Host "Review the report for details: $reportPath" -ForegroundColor Yellow
    exit 1
}
