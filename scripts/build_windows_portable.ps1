param(
    [Parameter(Mandatory = $true)][string]$ThirdPartySource,
    [Parameter(Mandatory = $true)][string]$VersionLabel,
    [string]$ReleaseBaseDir = "artifacts/release",
    [switch]$SkipPyInstaller
)

$args = @(
    "scripts/build_windows_portable.py",
    "--third-party-source", $ThirdPartySource,
    "--version-label", $VersionLabel,
    "--release-base-dir", $ReleaseBaseDir
)
if ($SkipPyInstaller) {
    $args += "--skip-pyinstaller"
}

python @args
