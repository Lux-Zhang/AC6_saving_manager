param(
    [Parameter(Mandatory = $true)][string]$ReleaseRoot,
    [string]$ArtifactsRoot = "artifacts/live_acceptance",
    [string]$SampleSave = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$resolvedReleaseRoot = [System.IO.Path]::GetFullPath($ReleaseRoot)
$resolvedArtifactsRoot = [System.IO.Path]::GetFullPath($ArtifactsRoot)
New-Item -ItemType Directory -Force -Path $resolvedArtifactsRoot | Out-Null

$exe = Join-Path $resolvedReleaseRoot "AC6 saving manager.exe"
$manifest = Join-Path $resolvedReleaseRoot "third_party\\third_party_manifest.json"
$witchy = Join-Path $resolvedReleaseRoot "third_party\\WitchyBND\\WitchyBND.exe"
$reportPath = Join-Path $resolvedArtifactsRoot "smoke-baseline.json"
$probeOutputPath = Join-Path $resolvedArtifactsRoot "smoke-open-save.txt"

if (-not (Test-Path -LiteralPath $exe)) {
    throw "portable exe does not exist: $exe"
}
if (-not (Test-Path -LiteralPath $manifest)) {
    throw "third-party manifest does not exist: $manifest"
}
if (-not (Test-Path -LiteralPath $witchy)) {
    throw "WitchyBND sidecar does not exist: $witchy"
}

$report = [ordered]@{
    release_root = $resolvedReleaseRoot
    exe_exists = $true
    manifest_exists = $true
    witchy_exists = $true
    sample_save = $null
    open_save_probe = [ordered]@{
        attempted = $false
        exit_code = $null
        output_path = $null
    }
}

if (-not [string]::IsNullOrWhiteSpace($SampleSave)) {
    $resolvedSampleSave = [System.IO.Path]::GetFullPath($SampleSave)
    if (-not (Test-Path -LiteralPath $resolvedSampleSave)) {
        throw "sample save does not exist: $resolvedSampleSave"
    }

    $report.sample_save = $resolvedSampleSave
    $report.open_save_probe.attempted = $true

    $probeOutput = & $exe --probe-open-save $resolvedSampleSave 2>&1
    $probeExitCode = $LASTEXITCODE
    $probeOutput | Set-Content -LiteralPath $probeOutputPath -Encoding UTF8

    $report.open_save_probe.exit_code = $probeExitCode
    $report.open_save_probe.output_path = $probeOutputPath

    if ($probeExitCode -ne 0) {
        $report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $reportPath -Encoding UTF8
        throw "native open-save probe failed with exit code $probeExitCode. See $probeOutputPath"
    }
}

$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $reportPath -Encoding UTF8
Write-Output $reportPath
