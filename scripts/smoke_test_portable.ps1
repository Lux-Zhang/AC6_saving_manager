param(
    [Parameter(Mandatory = $true)][string]$ReleaseRoot,
    [string]$ArtifactsRoot = "artifacts/live_acceptance"
)

$exe = Join-Path $ReleaseRoot "AC6 saving manager.exe"
$providerProof = Join-Path $ArtifactsRoot "smoke-provider-proof.json"
$preflightJson = Join-Path $ArtifactsRoot "smoke-preflight.json"
$preflightTxt = Join-Path $ArtifactsRoot "smoke-preflight.txt"

New-Item -ItemType Directory -Force -Path $ArtifactsRoot | Out-Null

& $exe --provider-proof-json $providerProof
python scripts/run_preflight_check.py --release-root $ReleaseRoot --json-out $preflightJson --text-out $preflightTxt
