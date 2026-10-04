param(
    [string]$Path = (Join-Path $PSScriptRoot '..\assets-local\GMS.zip'),
    [string]$Pattern = '*_CP.GMS',
    [int]$TopBlocks = 8,
    [switch]$Json
)

$ErrorActionPreference = 'Stop'

function Convert-ToHex([byte[]]$Bytes) {
    if (-not $Bytes -or $Bytes.Length -eq 0) { return '' }
    return ([BitConverter]::ToString($Bytes) -replace '-', '').ToLowerInvariant()
}

function Convert-TailToText([byte[]]$Bytes) {
    if (-not $Bytes -or $Bytes.Length -eq 0) { return '' }

    $parts = foreach ($b in $Bytes) {
        if ($b -eq 13) { '\r' }
        elseif ($b -eq 10) { '\n' }
        elseif ($b -eq 9) { '\t' }
        elseif ($b -ge 32 -and $b -le 126) { [char]$b }
        else { ('\x{0:x2}' -f $b) }
    }
    return ($parts -join '')
}

function Test-TailPrintable([byte[]]$Bytes) {
    foreach ($b in $Bytes) {
        if ($b -eq 9 -or $b -eq 10 -or $b -eq 13) { continue }
        if ($b -ge 32 -and $b -le 126) { continue }
        return $false
    }
    return $true
}

function Get-GmsStats([string]$Name, [byte[]]$Bytes) {
    $fullBlockBytes = [int]($Bytes.Length - ($Bytes.Length % 8))
    $blockCount = [int]($fullBlockBytes / 8)
    $counts = @{}

    for ($i = 0; $i -lt $blockCount; $i++) {
        $block = New-Object byte[] 8
        [Array]::Copy($Bytes, $i * 8, $block, 0, 8)
        $hex = Convert-ToHex $block
        if ($counts.ContainsKey($hex)) { $counts[$hex]++ }
        else { $counts[$hex] = 1 }
    }

    $repeatedKinds = 0
    $repeatedOccurrences = 0
    $maxRepeat = 0
    foreach ($count in $counts.Values) {
        if ($count -gt 1) {
            $repeatedKinds++
            $repeatedOccurrences += ($count - 1)
        }
        if ($count -gt $maxRepeat) { $maxRepeat = $count }
    }

    $fullBlockBytes16 = [int]($Bytes.Length - ($Bytes.Length % 16))
    $blockCount16 = [int]($fullBlockBytes16 / 16)
    $counts16 = @{}

    for ($i = 0; $i -lt $blockCount16; $i++) {
        $block16 = New-Object byte[] 16
        [Array]::Copy($Bytes, $i * 16, $block16, 0, 16)
        $hex16 = Convert-ToHex $block16
        if ($counts16.ContainsKey($hex16)) { $counts16[$hex16]++ }
        else { $counts16[$hex16] = 1 }
    }

    $repeatedOccurrences16 = 0
    $maxRepeat16 = 0
    foreach ($count in $counts16.Values) {
        if ($count -gt 1) {
            $repeatedOccurrences16 += ($count - 1)
        }
        if ($count -gt $maxRepeat16) { $maxRepeat16 = $count }
    }

    $tailLength = $Bytes.Length - $fullBlockBytes
    if ($tailLength -gt 0) {
        $tail = New-Object byte[] $tailLength
        [Array]::Copy($Bytes, $fullBlockBytes, $tail, 0, $tailLength)
    } else {
        $tail = New-Object byte[] 0
    }

    $prefixLength = [Math]::Min(32, $Bytes.Length)
    if ($prefixLength -gt 0) {
        $prefix = New-Object byte[] $prefixLength
        [Array]::Copy($Bytes, 0, $prefix, 0, $prefixLength)
    } else {
        $prefix = New-Object byte[] 0
    }

    $top = @(
        $counts.GetEnumerator() |
            Sort-Object -Property @{ Expression = 'Value'; Descending = $true }, @{ Expression = 'Name'; Descending = $false } |
            Select-Object -First ([Math]::Max(0, $TopBlocks)) |
            ForEach-Object {
                [pscustomobject]@{
                    BlockHex = $_.Name
                    Count = $_.Value
                }
            }
    )

    return [pscustomobject]@{
        Name = $Name
        Size = $Bytes.Length
        Mod8 = ($Bytes.Length % 8)
        FullBlocks = $blockCount
        UniqueBlocks = $counts.Count
        RepeatedBlockKinds = $repeatedKinds
        RepeatedOccurrences = $repeatedOccurrences
        MaxRepeat = $maxRepeat
        FullBlocks16 = $blockCount16
        UniqueBlocks16 = $counts16.Count
        RepeatedOccurrences16 = $repeatedOccurrences16
        MaxRepeat16 = $maxRepeat16
        TailHex = Convert-ToHex $tail
        TailText = Convert-TailToText $tail
        First32Hex = Convert-ToHex $prefix
        TopBlocks = $top
    }
}

function Read-AllBytesFromStream([System.IO.Stream]$Stream) {
    $memory = New-Object System.IO.MemoryStream
    try {
        $Stream.CopyTo($memory)
        return $memory.ToArray()
    } finally {
        $memory.Dispose()
    }
}

$resolved = (Resolve-Path -LiteralPath $Path).Path
$results = @()
$archiveSummary = $null

if (Test-Path -LiteralPath $resolved -PathType Container) {
    $allFiles = Get-ChildItem -LiteralPath $resolved -File |
        Where-Object { $_.Name -like '*.GMS' } |
        Sort-Object Name

    $nonAligned = 0
    $printableTails = 0
    $nonAligned16 = 0
    $printableTails16 = 0

    foreach ($file in $allFiles) {
        $bytes = [IO.File]::ReadAllBytes($file.FullName)
        $tailLength = $bytes.Length % 8
        if ($tailLength -gt 0) {
            $nonAligned++
            $tail = New-Object byte[] $tailLength
            [Array]::Copy($bytes, $bytes.Length - $tailLength, $tail, 0, $tailLength)
            if (Test-TailPrintable $tail) { $printableTails++ }
        }

        $tailLength16 = $bytes.Length % 16
        if ($tailLength16 -gt 0) {
            $nonAligned16++
            $tail16 = New-Object byte[] $tailLength16
            [Array]::Copy($bytes, $bytes.Length - $tailLength16, $tail16, 0, $tailLength16)
            if (Test-TailPrintable $tail16) { $printableTails16++ }
        }

        $tailLength16 = $bytes.Length % 16
        if ($tailLength16 -gt 0) {
            $nonAligned16++
            $tail16 = New-Object byte[] $tailLength16
            [Array]::Copy($bytes, $bytes.Length - $tailLength16, $tail16, 0, $tailLength16)
            if (Test-TailPrintable $tail16) { $printableTails16++ }
        }

        if ($file.Name -like $Pattern) {
            $results += Get-GmsStats $file.Name $bytes
        }
    }

    $archiveSummary = [pscustomobject]@{
        TotalGms = $allFiles.Count
        NonAligned = $nonAligned
        PrintableTails = $printableTails
        NonAligned16 = $nonAligned16
        PrintableTails16 = $printableTails16
    }
}
elseif ([IO.Path]::GetExtension($resolved) -ieq '.zip') {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($resolved)
    try {
        $allEntries = @(
            $zip.Entries |
                Where-Object { $_.Name -and $_.Name -like '*.GMS' } |
                Sort-Object FullName
        )

        $nonAligned = 0
        $printableTails = 0

        foreach ($entry in $allEntries) {
            $stream = $entry.Open()
            try {
                $bytes = Read-AllBytesFromStream $stream
            } finally {
                $stream.Dispose()
            }

            $tailLength = $bytes.Length % 8
            if ($tailLength -gt 0) {
                $nonAligned++
                $tail = New-Object byte[] $tailLength
                [Array]::Copy($bytes, $bytes.Length - $tailLength, $tail, 0, $tailLength)
                if (Test-TailPrintable $tail) { $printableTails++ }
            }

            if ($entry.Name -like $Pattern) {
                $results += Get-GmsStats $entry.Name $bytes
            }
        }

        $archiveSummary = [pscustomobject]@{
            TotalGms = $allEntries.Count
            NonAligned = $nonAligned
            PrintableTails = $printableTails
            NonAligned16 = $nonAligned16
            PrintableTails16 = $printableTails16
        }
    } finally {
        $zip.Dispose()
    }
}
else {
    $name = [IO.Path]::GetFileName($resolved)
    if ($name -notlike $Pattern) {
        throw "File '$name' does not match pattern '$Pattern'."
    }
    $results += Get-GmsStats $name ([IO.File]::ReadAllBytes($resolved))
}

if ($results.Count -eq 0) {
    throw "No files matching '$Pattern' were found in '$resolved'."
}

if ($Json) {
    $results | ConvertTo-Json -Depth 5
    exit 0
}

if ($archiveSummary) {
    Write-Host ('8-byte check:  {0} GMS files; {1} have a 1-7 byte tail; {2}/{1} tails are printable ASCII/control text.' -f $archiveSummary.TotalGms, $archiveSummary.NonAligned, $archiveSummary.PrintableTails)
    Write-Host ('16-byte check: {0} GMS files; {1} have a 1-15 byte tail; {2}/{1} tails are printable ASCII/control text.' -f $archiveSummary.TotalGms, $archiveSummary.NonAligned16, $archiveSummary.PrintableTails16)
    Write-Host ''
}

$results |
    Select-Object Name, Size, Mod8, FullBlocks, UniqueBlocks, RepeatedOccurrences, MaxRepeat, RepeatedOccurrences16, MaxRepeat16, TailText |
    Format-Table -AutoSize

foreach ($result in $results) {
    Write-Host ''
    Write-Host $result.Name
    Write-Host ('  first32: {0}' -f $result.First32Hex)
    Write-Host ('  tail:    {0}  ({1})' -f $result.TailHex, $result.TailText)
    if ($result.TopBlocks.Count -gt 0) {
        Write-Host '  most repeated 8-byte blocks:'
        foreach ($block in $result.TopBlocks) {
            Write-Host ('    {0}  x{1}' -f $block.BlockHex, $block.Count)
        }
    }
}
