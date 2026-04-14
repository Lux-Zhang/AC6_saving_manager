param(
    [Parameter(Mandatory = $true)][string]$ReleaseRoot,
    [string]$OutputDir = "artifacts/live_acceptance"
)

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$checklistPath = Join-Path $OutputDir "checklist-manual.md"
$content = @"
# ZPX-GE77 Live Acceptance Capture

- release_root: $ReleaseRoot
- machine: ZPX-GE77
- operator: fill manually
- huorong: pass | manual-allow | blocked

## Steps
- [ ] unzip portable zip
- [ ] first launch exe
- [ ] preflight
- [ ] third-party gate
- [ ] dry-run
- [ ] apply -> fresh destination
- [ ] readback
- [ ] rollback -> fresh destination
- [ ] second launch
- [ ] game verify

## Verdict
- [ ] live-pass
- [ ] conditional-pass
- [ ] fail
- [ ] release-pending
"@

Set-Content -LiteralPath $checklistPath -Value $content -Encoding UTF8
Write-Output $checklistPath
