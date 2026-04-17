param(
    [string]$BuildDir = "build/native",
    [string]$Config = "Release",
    [string]$OutputDir = "Release",
    [string]$WitchySourceDir = "..\\ACVIEmblemCreator\\release\\WitchyBND",
    [bool]$KeepLatestOnly = $true
)

$ErrorActionPreference = "Stop"

function Remove-StaleSiblingReleases {
    param(
        [string]$CurrentOutputDir
    )

    $resolvedOutput = [System.IO.Path]::GetFullPath($CurrentOutputDir)
    $parentDir = Split-Path -Path $resolvedOutput -Parent
    if ([string]::IsNullOrWhiteSpace($parentDir) -or -not (Test-Path -LiteralPath $parentDir)) {
        return
    }

    if ((Split-Path -Path $parentDir -Leaf) -ne "release") {
        return
    }

    Get-ChildItem -LiteralPath $parentDir -Directory |
        Where-Object { $_.FullName -ne $resolvedOutput } |
        ForEach-Object {
            Remove-Item -LiteralPath $_.FullName -Recurse -Force
        }
}

function Get-ThirdPartyFileRole {
    param(
        [string]$RelativePath,
        [string]$Entrypoint
    )

    if ($RelativePath -eq $Entrypoint) {
        return "entrypoint"
    }

    $extension = [System.IO.Path]::GetExtension($RelativePath).ToLowerInvariant()
    switch ($extension) {
        ".dll" { return "dll" }
        ".json" { return "config" }
        ".config" { return "config" }
        ".ini" { return "config" }
        ".txt" { return "config" }
        ".png" { return "asset" }
        ".jpg" { return "asset" }
        ".jpeg" { return "asset" }
        ".bin" { return "asset" }
        default { return "other" }
    }
}

function Get-RelativePathCompat {
    param(
        [string]$BasePath,
        [string]$TargetPath
    )

    if ([System.IO.Path].GetMethod("GetRelativePath", [Type[]]@([string], [string])) -ne $null) {
        return [System.IO.Path]::GetRelativePath($BasePath, $TargetPath)
    }

    $baseUri = New-Object System.Uri(([System.IO.Path]::GetFullPath($BasePath).TrimEnd('\') + '\'))
    $targetUri = New-Object System.Uri([System.IO.Path]::GetFullPath($TargetPath))
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($targetUri).ToString()).Replace('/', '\')
}

function Write-ThirdPartyManifest {
    param(
        [string]$AppRoot,
        [string]$ManifestPath,
        [string]$VersionLabel,
        [string]$SourceBaseline
    )

    $entrypoint = "third_party/WitchyBND/WitchyBND.exe"
    $toolRoot = Join-Path $AppRoot "third_party\\WitchyBND"
    if (-not (Test-Path -LiteralPath $toolRoot)) {
        throw "WitchyBND tool root does not exist: $toolRoot"
    }

    $files = @()
    Get-ChildItem -LiteralPath $toolRoot -File -Recurse |
        Sort-Object FullName |
        ForEach-Object {
            $relativePath = (Get-RelativePathCompat -BasePath $AppRoot -TargetPath $_.FullName).Replace('\', '/')
            $files += [ordered]@{
                path = $relativePath
                sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()
                required = $true
                file_role = Get-ThirdPartyFileRole -RelativePath $relativePath -Entrypoint $entrypoint
                size_bytes = [int64]$_.Length
            }
        }

    $manifest = [ordered]@{
        schema_version = 1
        name = "WitchyBND"
        source_baseline = $SourceBaseline
        version_label = $VersionLabel
        entrypoint = $entrypoint
        required = $true
        preflight_policy = "fail_closed"
        generated_at = (Get-Date).ToUniversalTime().ToString("o")
        files = $files
    }

    $json = $manifest | ConvertTo-Json -Depth 8
    Set-Content -LiteralPath $ManifestPath -Value ($json + [Environment]::NewLine) -Encoding UTF8
}

function Write-QtConf {
    param(
        [string]$AppRoot
    )

    $qtConfPath = Join-Path $AppRoot "qt.conf"
    $content = @"
[Paths]
Prefix = .
Plugins = .
Translations = translations
"@
    Set-Content -LiteralPath $qtConfPath -Value ($content + [Environment]::NewLine) -Encoding ASCII
}

if (!(Test-Path $BuildDir)) {
    throw "Build directory does not exist: $BuildDir"
}

if (Test-Path $OutputDir) {
    Remove-Item -Recurse -Force $OutputDir
}

cmake --install $BuildDir --config $Config --prefix $OutputDir

$exePath = Join-Path $OutputDir "AC6 saving manager.exe"
if (!(Test-Path $exePath)) {
    $exePath = Join-Path $OutputDir "ac6_data_manager_native.exe"
}
if (!(Test-Path $exePath)) {
    throw "Installed executable not found under $OutputDir"
}

$windeployqt = (Get-Command windeployqt -ErrorAction SilentlyContinue).Source
if ([string]::IsNullOrWhiteSpace($windeployqt)) {
    throw "windeployqt was not found in PATH"
}
& $windeployqt --release $exePath
Write-QtConf -AppRoot ([System.IO.Path]::GetFullPath($OutputDir))

$resolvedWitchySource = Resolve-Path $WitchySourceDir -ErrorAction Stop
$toolRoot = Join-Path $OutputDir "third_party\\WitchyBND"
New-Item -ItemType Directory -Force -Path $toolRoot | Out-Null
Copy-Item -Path (Join-Path $resolvedWitchySource "*") -Destination $toolRoot -Recurse -Force

$manifestPath = Join-Path $OutputDir "third_party\\third_party_manifest.json"
Write-ThirdPartyManifest `
    -AppRoot ([System.IO.Path]::GetFullPath($OutputDir)) `
    -ManifestPath $manifestPath `
    -VersionLabel "ACVIEmblemCreator-sidecar" `
    -SourceBaseline "ACVIEmblemCreator release/WitchyBND"

$manifestTemplate = Join-Path $OutputDir "third_party\\manifest.template.json"
if (Test-Path $manifestTemplate) {
    Remove-Item -Force $manifestTemplate
}

if ($KeepLatestOnly) {
    Remove-StaleSiblingReleases -CurrentOutputDir $OutputDir
}
