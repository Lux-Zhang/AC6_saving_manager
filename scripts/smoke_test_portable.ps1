param(
    [Parameter(Mandatory = $true)][string]$ReleaseRoot,
    [string]$ArtifactsRoot = "artifacts/live_acceptance"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$exe = Join-Path $ReleaseRoot "AC6 saving manager.exe"
$providerProof = Join-Path $ArtifactsRoot "smoke-provider-proof.json"
$preflightJson = Join-Path $ArtifactsRoot "smoke-preflight.json"
$preflightTxt = Join-Path $ArtifactsRoot "smoke-preflight.txt"
$smokeReport = Join-Path $ArtifactsRoot "smoke-baseline.json"

New-Item -ItemType Directory -Force -Path $ArtifactsRoot | Out-Null

& $exe --provider-proof-json $providerProof
python scripts/run_preflight_check.py --release-root $ReleaseRoot --json-out $preflightJson --text-out $preflightTxt
python scripts/write_smoke_report.py --release-root $ReleaseRoot --provider-proof $providerProof --preflight-report $preflightJson --output $smokeReport
