param(
    [string]$BuildDir = "build/native",
    [string]$Config = "Release",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$QtPrefix = "",
    [bool]$KeepLatestOnly = $true
)

$ErrorActionPreference = "Stop"

function Remove-StaleConfigDir {
    param(
        [string]$RootDir,
        [string]$KeepConfig
    )

    if (-not (Test-Path -LiteralPath $RootDir)) {
        return
    }

    Get-ChildItem -LiteralPath $RootDir -Directory |
        Where-Object { $_.Name -in @("Debug", "Release", "RelWithDebInfo", "MinSizeRel") -and $_.Name -ne $KeepConfig } |
        ForEach-Object {
            Remove-Item -LiteralPath $_.FullName -Recurse -Force
        }
}

if ([string]::IsNullOrWhiteSpace($QtPrefix)) {
    if (-not $env:CMAKE_PREFIX_PATH) {
        $qmake = Get-Command qmake -ErrorAction SilentlyContinue
        if ($qmake) {
            $QtPrefix = Split-Path (Split-Path $qmake.Source -Parent) -Parent
        }
    }
} elseif (-not $env:CMAKE_PREFIX_PATH) {
    $env:CMAKE_PREFIX_PATH = $QtPrefix
}

if ($QtPrefix -and -not $env:CMAKE_PREFIX_PATH) {
    $env:CMAKE_PREFIX_PATH = $QtPrefix
}

cmake -S native -B $BuildDir -G $Generator -A x64
cmake --build $BuildDir --config $Config

if ($KeepLatestOnly) {
    Remove-StaleConfigDir -RootDir $BuildDir -KeepConfig $Config
    Remove-StaleConfigDir -RootDir (Join-Path $BuildDir "tests") -KeepConfig $Config
}
