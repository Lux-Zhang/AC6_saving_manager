param(
    [Parameter(Mandatory = $true, Position = 0, ValueFromRemainingArguments = $true)]
    [string[]]$PngPaths,

    [string]$OutputDir = ".\restored_from_png",

    [string]$FinalZipName,

    [switch]$SkipMerge
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-BigEndianUInt32 {
    param(
        [byte[]]$Bytes,
        [int]$Offset
    )

    if ($Offset + 4 -gt $Bytes.Length) {
        throw "Big-endian UInt32 exceeds buffer length at offset $Offset."
    }

    return ([uint32]$Bytes[$Offset] -shl 24) -bor
           ([uint32]$Bytes[$Offset + 1] -shl 16) -bor
           ([uint32]$Bytes[$Offset + 2] -shl 8) -bor
           [uint32]$Bytes[$Offset + 3]
}

function Get-HexSha256 {
    param(
        [byte[]]$Bytes
    )

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hashBytes = $sha256.ComputeHash($Bytes)
    } finally {
        $sha256.Dispose()
    }

    return ([System.BitConverter]::ToString($hashBytes)).Replace("-", "")
}

function Read-HiddenPayload {
    param(
        [string]$PngPath
    )

    $resolvedPath = (Resolve-Path -LiteralPath $PngPath).ProviderPath
    $pngBytes = [System.IO.File]::ReadAllBytes($resolvedPath)
    $pngSignature = [byte[]](0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A)

    if ($pngBytes.Length -lt 8) {
        throw "PNG file is too small: $resolvedPath"
    }

    for ($index = 0; $index -lt $pngSignature.Length; $index++) {
        if ($pngBytes[$index] -ne $pngSignature[$index]) {
            throw "Input is not a PNG file: $resolvedPath"
        }
    }

    $offset = 8
    $chunkData = $null

    while ($offset + 12 -le $pngBytes.Length) {
        $chunkLength = [int](Get-BigEndianUInt32 -Bytes $pngBytes -Offset $offset)
        $chunkType = [System.Text.Encoding]::ASCII.GetString($pngBytes, $offset + 4, 4)
        $dataOffset = $offset + 8
        $crcOffset = $dataOffset + $chunkLength
        $nextOffset = $crcOffset + 4

        if ($nextOffset -gt $pngBytes.Length) {
            throw "PNG chunk exceeds file length in $resolvedPath"
        }

        if ($chunkType -eq "stEg") {
            $chunkData = New-Object byte[] $chunkLength
            [System.Buffer]::BlockCopy($pngBytes, $dataOffset, $chunkData, 0, $chunkLength)
            break
        }

        $offset = $nextOffset
        if ($chunkType -eq "IEND") {
            break
        }
    }

    if ($null -eq $chunkData) {
        throw "No stEg payload chunk found in $resolvedPath"
    }

    if ($chunkData.Length -lt 8) {
        throw "Hidden payload header is too short in $resolvedPath"
    }

    $magic = [System.Text.Encoding]::ASCII.GetString($chunkData, 0, 4)
    if ($magic -ne "ACV1") {
        throw "Unexpected hidden payload magic '$magic' in $resolvedPath"
    }

    $metaLength = [int](Get-BigEndianUInt32 -Bytes $chunkData -Offset 4)
    $metaOffset = 8
    $payloadOffset = $metaOffset + $metaLength

    if ($payloadOffset -gt $chunkData.Length) {
        throw "Hidden payload metadata exceeds chunk length in $resolvedPath"
    }

    $metaJson = [System.Text.Encoding]::UTF8.GetString($chunkData, $metaOffset, $metaLength)
    $meta = $metaJson | ConvertFrom-Json

    $payloadLength = $chunkData.Length - $payloadOffset
    $payloadBytes = New-Object byte[] $payloadLength
    [System.Buffer]::BlockCopy($chunkData, $payloadOffset, $payloadBytes, 0, $payloadLength)

    $actualHash = Get-HexSha256 -Bytes $payloadBytes
    $expectedHash = [string]$meta.sha256
    $expectedSize = [int64]$meta.size

    if ($payloadLength -ne $expectedSize) {
        throw "Payload size mismatch for $resolvedPath. Expected $expectedSize, got $payloadLength."
    }

    if ($actualHash.ToUpperInvariant() -ne $expectedHash.ToUpperInvariant()) {
        throw "Payload SHA-256 mismatch for $resolvedPath. Expected $expectedHash, got $actualHash."
    }

    return [pscustomobject]@{
        SourcePath   = $resolvedPath
        PayloadName  = [string]$meta.filename
        PayloadSize  = $expectedSize
        PayloadSha256 = $actualHash.ToUpperInvariant()
        PayloadBytes = $payloadBytes
    }
}

function Get-PartOrder {
    param(
        [string]$FileName
    )

    $match = [regex]::Match($FileName, "\.(\d+)$")
    if (-not $match.Success) {
        throw "Cannot determine split-part order from file name: $FileName"
    }

    return [int]$match.Groups[1].Value
}

function Resolve-PngInputs {
    param(
        [string[]]$InputPaths
    )

    $resolvedFiles = New-Object System.Collections.Generic.List[string]

    foreach ($inputPath in $InputPaths) {
        $resolved = Resolve-Path -LiteralPath $inputPath -ErrorAction Stop
        foreach ($entry in $resolved) {
            $fullPath = $entry.ProviderPath
            if (Test-Path -LiteralPath $fullPath -PathType Container) {
                Get-ChildItem -LiteralPath $fullPath -File -Filter *.png |
                    Sort-Object Name |
                    ForEach-Object { [void]$resolvedFiles.Add($_.FullName) }
            } else {
                [void]$resolvedFiles.Add($fullPath)
            }
        }
    }

    $uniqueFiles = @($resolvedFiles | Sort-Object -Unique)
    if (-not $uniqueFiles -or $uniqueFiles.Count -eq 0) {
        throw "No PNG files were resolved from the provided input paths."
    }

    return $uniqueFiles
}

function Merge-Parts {
    param(
        [string[]]$PartPaths,
        [string]$DestinationPath
    )

    $destinationFullPath = [System.IO.Path]::GetFullPath($DestinationPath)
    $parent = Split-Path -Path $destinationFullPath -Parent
    if ($parent) {
        [System.IO.Directory]::CreateDirectory($parent) | Out-Null
    }

    $output = [System.IO.File]::Open($destinationFullPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    try {
        foreach ($partPath in $PartPaths) {
            $input = [System.IO.File]::OpenRead($partPath)
            try {
                $input.CopyTo($output)
            } finally {
                $input.Dispose()
            }
        }
    } finally {
        $output.Dispose()
    }
}

$resolvedPngPaths = Resolve-PngInputs -InputPaths $PngPaths
$resolvedOutputDir = [System.IO.Path]::GetFullPath($OutputDir)
$partsDir = Join-Path $resolvedOutputDir "parts"
$normalizedPngDir = Join-Path $resolvedOutputDir "normalized_pngs"
[System.IO.Directory]::CreateDirectory($partsDir) | Out-Null
[System.IO.Directory]::CreateDirectory($normalizedPngDir) | Out-Null

$payloadRecords = @()

foreach ($pngPath in $resolvedPngPaths) {
    $payload = Read-HiddenPayload -PngPath $pngPath
    $payloadRecords += [pscustomobject]@{
        Source      = $payload.SourcePath
        Name        = $payload.PayloadName
        Size        = $payload.PayloadSize
        Sha256      = $payload.PayloadSha256
        PayloadBytes = $payload.PayloadBytes
        PartOrder   = Get-PartOrder -FileName $payload.PayloadName
    }
}

$selectedParts = @()
foreach ($group in ($payloadRecords | Group-Object PartOrder | Sort-Object Name)) {
    $uniqueHashes = @($group.Group | Select-Object -ExpandProperty Sha256 -Unique)
    if ($uniqueHashes.Count -gt 1) {
        throw "Part $($group.Name) has conflicting payload hashes across multiple PNG files."
    }

    $primary = $group.Group | Sort-Object Source | Select-Object -First 1
    $selectedParts += $primary

    if ($group.Count -gt 1) {
        $ignored = ($group.Group | Sort-Object Source | Select-Object -Skip 1 | Select-Object -ExpandProperty Source)
        foreach ($ignoredSource in $ignored) {
            Write-Host ("IGNORED DUPLICATE SOURCE {0} for part {1}" -f $ignoredSource, $group.Name)
        }
    }
}

$writtenParts = @()
$selectedParts = $selectedParts | Sort-Object PartOrder

for ($index = 0; $index -lt $selectedParts.Count; $index++) {
    $part = $selectedParts[$index]
    $canonicalPngName = "acvi_hidden_{0:D2}.png" -f $part.PartOrder
    $normalizedPngPath = Join-Path $normalizedPngDir $canonicalPngName
    [System.IO.File]::Copy($part.Source, $normalizedPngPath, $true)

    $outputPath = Join-Path $partsDir $part.Name
    [System.IO.File]::WriteAllBytes($outputPath, $part.PayloadBytes)
    $writtenParts += [pscustomobject]@{
        Source        = $part.Source
        NormalizedPng = $normalizedPngPath
        Extracted     = $outputPath
        Name          = $part.Name
        Size          = $part.Size
        Sha256        = $part.Sha256
        PartOrder     = $part.PartOrder
    }
}

$writtenParts | ForEach-Object {
    Write-Host ("NORMALIZED {0} -> {1}" -f $_.Source, $_.NormalizedPng)
    Write-Host ("EXTRACTED {0} -> {1} ({2} bytes, SHA256 {3})" -f $_.NormalizedPng, $_.Extracted, $_.Size, $_.Sha256)
}

if (-not $SkipMerge) {
    if (-not $FinalZipName) {
        $firstName = $writtenParts[0].Name
        $FinalZipName = [regex]::Replace($firstName, "\.\d+$", "")
        if (-not $FinalZipName) {
            $FinalZipName = "restored.zip"
        }
    }

    $finalZipPath = Join-Path $resolvedOutputDir $FinalZipName
    Merge-Parts -PartPaths $writtenParts.Extracted -DestinationPath $finalZipPath
    $finalZipHash = (Get-FileHash -LiteralPath $finalZipPath -Algorithm SHA256).Hash.ToUpperInvariant()
    $finalZipSize = (Get-Item -LiteralPath $finalZipPath).Length
    Write-Host ("MERGED -> {0} ({1} bytes, SHA256 {2})" -f $finalZipPath, $finalZipSize, $finalZipHash)
}
