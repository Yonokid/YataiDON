# YataiDON Windows Updater
#
# Expected GitHub release assets:
#   checksums-windows.sha256    sha256sum-format, relative paths from install dir
#   update-windows.tar.gz       binary + dlls + shader
#
# Usage (standalone):   powershell -ExecutionPolicy Bypass -File update.ps1
# Usage (from game):    update.bat --wait-pid <PID>

param(
    [string]$WaitPid = ""
)

$ErrorActionPreference = "Stop"

$Repo        = "yonokid/YataiDON"
$ApiUrl      = "https://api.github.com/repos/$Repo/releases/tags/updater"
$InstallDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$VersionFile = Join-Path $InstallDir ".version"
$TmpDir      = Join-Path $env:TEMP "YataiDON-update-$([System.IO.Path]::GetRandomFileName())"
New-Item -ItemType Directory -Path $TmpDir | Out-Null

function Log { param($msg) Write-Host "[update] $msg" }
function Die { param($msg) Write-Host "[update] Error: $msg" -ForegroundColor Red; exit 1 }

try {
    # --- Fetch release metadata ---
    Log "Checking for updates..."
    $Release         = Invoke-RestMethod -Uri $ApiUrl -TimeoutSec 10
    $LatestReleaseId = [string]$Release.id
    $LocalReleaseId  = if (Test-Path $VersionFile) { (Get-Content $VersionFile -Raw).Trim() } else { "" }
    Log "Local release id: $LocalReleaseId | Latest: $LatestReleaseId ($($Release.tag_name))"

    $AssetMap = @{}
    foreach ($asset in $Release.assets) { $AssetMap[$asset.name] = $asset.browser_download_url }

    # --- Download checksums ---
    if (-not $AssetMap.ContainsKey("checksums-windows.sha256")) {
        Die "No checksums-windows.sha256 in release $($Release.tag_name)"
    }
    $ChecksumsPath = Join-Path $TmpDir "checksums.sha256"
    Invoke-WebRequest -Uri $AssetMap["checksums-windows.sha256"] -OutFile $ChecksumsPath -TimeoutSec 30

    # --- Check main package ---
    $NeedPackage = $false
    foreach ($line in Get-Content $ChecksumsPath) {
        $parts = $line -split '\s+', 2
        if ($parts.Count -lt 2) { continue }
        $expectedHash = $parts[0].ToUpper()
        $relPath      = $parts[1].TrimStart('*').Replace('/', '\')
        $localFile    = Join-Path $InstallDir $relPath

        if (Test-Path $localFile) {
            $actualHash = (Get-FileHash $localFile -Algorithm SHA256).Hash
            if ($actualHash -ne $expectedHash) { $NeedPackage = $true; break }
        } else {
            $NeedPackage = $true; break
        }
    }

    if (-not $NeedPackage) {
        Log "Already up to date."
        Set-Content $VersionFile $LatestReleaseId
        exit 0
    }

    Log "Updates needed -- package: $([int]$NeedPackage)"

    # --- Wait for game process if requested ---
    if ($WaitPid -ne "") {
        Log "Waiting for game (PID $WaitPid) to exit..."
        $proc = Get-Process -Id ([int]$WaitPid) -ErrorAction SilentlyContinue
        if ($proc) { $proc.WaitForExit() }
    }

    # --- Download and extract main package ---
    if (-not $AssetMap.ContainsKey("update-windows.tar.gz")) {
        Die "No update-windows.tar.gz in release $($Release.tag_name)"
    }
    $TarPath = Join-Path $TmpDir "update-windows.tar.gz"
    Log "Downloading update-windows.tar.gz..."
    Invoke-WebRequest -Uri $AssetMap["update-windows.tar.gz"] -OutFile $TarPath
    Log "Extracting..."
    & tar -xzf $TarPath -C $InstallDir
    if ($LASTEXITCODE -ne 0) { Die "tar extraction failed" }
    Log "Package applied."

    Set-Content $VersionFile $LatestReleaseId
    Log "Update complete ($($Release.tag_name)). Restart YataiDON to apply."

} finally {
    Remove-Item -Recurse -Force $TmpDir -ErrorAction SilentlyContinue
}
