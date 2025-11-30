Param(
    [string]$Destination = "$Env:USERPROFILE\Downloads"
)

Write-Host "Searching for latest .bin artifact..."

$paths = @()
$paths += (Resolve-Path -Path (Join-Path $PSScriptRoot "..\..\Viessmann-Waermepumpe-in-ESPhome-with-Optolink-DIY\.local-ci\build") -ErrorAction SilentlyContinue).Path
$paths += (Join-Path $Env:LOCALAPPDATA "arduino\sketches")

$paths = $paths | Where-Object { $_ -and (Test-Path $_) }

$binFiles = @()
foreach ($p in $paths) {
    if (Test-Path $p) {
        $binFiles += Get-ChildItem -Path $p -Recurse -Filter *.bin -ErrorAction SilentlyContinue
    }
}

if (-not $binFiles -or $binFiles.Count -eq 0) {
    Write-Error "No .bin files found. Searched: $($paths -join ', ')"
    exit 1
}

$latest = $binFiles | Sort-Object LastWriteTime -Descending | Select-Object -First 1

if (-not (Test-Path $Destination)) {
    Write-Host "Destination '$Destination' does not exist. Creating..."
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$targetName = ($latest.BaseName + "-" + $timestamp + ".bin")
$targetPath = Join-Path $Destination $targetName

Copy-Item -Path $latest.FullName -Destination $targetPath -Force

Write-Host "Copied latest binary:" $latest.FullName
Write-Host "->" $targetPath

# Also write a convenience symlink (if supported)
try {
    $linkPath = Join-Path $Destination "latest-firmware.bin"
    if (Test-Path $linkPath) { Remove-Item $linkPath -Force }
    New-Item -ItemType SymbolicLink -Path $linkPath -Target $targetPath -ErrorAction Stop | Out-Null
    Write-Host "Updated symbolic link: $linkPath -> $targetName"
} catch {
    Write-Host "(Symbolic link not created: $($_.Exception.Message))"
}

Write-Host "Done."