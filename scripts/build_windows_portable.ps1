param(
    [Parameter(Mandatory = $true)][string]$ThirdPartySource,
    [string]$ReleaseDir = "Release",
    [string]$BuildDir = "build/native",
    [string]$Config = "Release",
    [string]$VersionLabel = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptRoot
$nativeReleaseRoot = Join-Path $repoRoot $ReleaseDir

if (-not [string]::IsNullOrWhiteSpace($VersionLabel)) {
    Write-Output "INFO: VersionLabel is ignored by the native-only release wrapper; output is fixed to Release."
}

& (Join-Path $scriptRoot "build_native_windows.ps1") -BuildDir $BuildDir -Config $Config

& (Join-Path $scriptRoot "package_native_windows.ps1") `
    -BuildDir $BuildDir `
    -Config $Config `
    -OutputDir $nativeReleaseRoot `
    -WitchySourceDir $ThirdPartySource

Write-Output $nativeReleaseRoot
