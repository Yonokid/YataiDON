# YataiDON Windows Updater
#
# Expected GitHub release assets:
#   checksums-windows.sha256    sha256sum-format, relative paths from install dir
#   update-windows.tar.gz       binary + dlls + shader
#
# Skins with a .skin-repo file are updated by comparing against checksums.sha256
# fetched from the skin repo's branch HEAD. No local version state is kept.
# .skin-repo format: line 1 = repo URL, line 2 (optional) = branch (default: main)
#
# Usage (standalone):   powershell -ExecutionPolicy Bypass -File update.ps1
# Usage (from game):    update.bat --wait-pid <PID>

param(
    [string]$WaitPid = ""
)

$ErrorActionPreference = "Stop"

$Repo        = "yonokid/YataiDON"
$ApiUrl      = "https://api.github.com/repos/$Repo/releases/latest"
$InstallDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$VersionFile = Join-Path $InstallDir ".version"
$TmpDir      = Join-Path $env:TEMP "YataiDON-update-$([System.IO.Path]::GetRandomFileName())"
New-Item -ItemType Directory -Path $TmpDir | Out-Null

function Log { param($msg) Write-Host "[update] $msg" }
function Die { param($msg) Write-Host "[update] Error: $msg" -ForegroundColor Red; exit 1 }

function Get-SkinFileUrl {
    param([string]$RepoUrl, [string]$Branch, [string]$FilePath)
    $base = $RepoUrl -replace '\.git$', ''
    if ($base -match 'github\.com') {
        $ownerRepo = ($base -split 'github\.com/')[-1]
        return "https://raw.githubusercontent.com/$ownerRepo/$Branch/$FilePath"
    } else {
        return "$base/raw/branch/$Branch/$FilePath"
    }
}

try {
    # --- Fetch release metadata ---
    Log "Checking for updates..."
    $Release         = Invoke-RestMethod -Uri $ApiUrl -TimeoutSec 10
    $LatestReleaseId = [string]$Release.id
    $LocalReleaseId  = if (Test-Path $VersionFile) { (Get-Content $VersionFile -Raw).Trim() } else { "" }
    Log "Local release id: $LocalReleaseId | Latest: $LatestReleaseId ($($Release.tag_name))"

    if ($LocalReleaseId -eq $LatestReleaseId) {
        Log "Already up to date."
        exit 0
    }

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
        $relPath      = $parts[1].Replace('/', '\')
        $localFile    = Join-Path $InstallDir $relPath

        if (Test-Path $localFile) {
            $actualHash = (Get-FileHash $localFile -Algorithm SHA256).Hash
            if ($actualHash -ne $expectedHash) { $NeedPackage = $true; break }
        } else {
            $NeedPackage = $true; break
        }
    }

    # --- Check installed skins ---
    # Fetch checksums.sha256 from each skin's repo and compare against local files.
    $SkinUpdates = [System.Collections.Generic.List[object]]::new()
    $SkinsDir = Join-Path $InstallDir "Skins"
    if (Test-Path $SkinsDir) {
        foreach ($skinDir in Get-ChildItem -Path $SkinsDir -Directory) {
            $skinRepoFile = Join-Path $skinDir.FullName ".skin-repo"
            if (-not (Test-Path $skinRepoFile)) { continue }

            $lines   = @(Get-Content $skinRepoFile)
            $repoUrl = $lines[0].Trim()
            $branch  = if ($lines.Count -gt 1 -and $lines[1].Trim() -ne '') { $lines[1].Trim() } else { "main" }

            $checksumsUrl      = Get-SkinFileUrl -RepoUrl $repoUrl -Branch $branch -FilePath "checksums.sha256"
            $skinChecksumsPath = Join-Path $TmpDir "skin-checksums-$($skinDir.Name).sha256"
            try {
                Invoke-WebRequest -Uri $checksumsUrl -OutFile $skinChecksumsPath -TimeoutSec 30
            } catch {
                Log "Warning: could not fetch checksums for $($skinDir.Name) — skipping"
                continue
            }

            $skinNeedsUpdate = $false
            foreach ($cline in Get-Content $skinChecksumsPath) {
                $cparts = $cline -split '\s+', 2
                if ($cparts.Count -lt 2) { continue }
                $expectedHash = $cparts[0].ToUpper()
                $relPath      = $cparts[1]
                $localFile    = Join-Path $skinDir.FullName ($relPath.Replace('/', '\'))

                if (Test-Path $localFile) {
                    $actualHash = (Get-FileHash $localFile -Algorithm SHA256).Hash
                    if ($actualHash -eq $expectedHash) { continue }
                }
                $skinNeedsUpdate = $true
                break
            }

            if (-not $skinNeedsUpdate) { continue }
            $SkinUpdates.Add(@($skinDir.FullName, $repoUrl, $branch, $skinChecksumsPath))
        }
    }

    if (-not $NeedPackage -and $SkinUpdates.Count -eq 0) {
        Log "Already up to date."
        Set-Content $VersionFile $LatestReleaseId
        exit 0
    }

    Log "Updates needed — package: $([int]$NeedPackage) | skins: $($SkinUpdates.Count)"

    # --- Wait for game process if requested ---
    if ($WaitPid -ne "") {
        Log "Waiting for game (PID $WaitPid) to exit..."
        $proc = Get-Process -Id ([int]$WaitPid) -ErrorAction SilentlyContinue
        if ($proc) { $proc.WaitForExit() }
    }

    # --- Download and extract main package ---
    if ($NeedPackage) {
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
    }

    # --- Update skins ---
    foreach ($update in $SkinUpdates) {
        $skinDirPath, $repoUrl, $branch, $skinChecksumsPath = $update
        $skinName = Split-Path -Leaf $skinDirPath
        Log "Updating $skinName..."

        $changed = 0
        foreach ($cline in Get-Content $skinChecksumsPath) {
            $cparts = $cline -split '\s+', 2
            if ($cparts.Count -lt 2) { continue }
            $expectedHash = $cparts[0].ToUpper()
            $relPath      = $cparts[1]
            $localFile    = Join-Path $skinDirPath ($relPath.Replace('/', '\'))

            if (Test-Path $localFile) {
                $actualHash = (Get-FileHash $localFile -Algorithm SHA256).Hash
                if ($actualHash -eq $expectedHash) { continue }
            }

            $rawUrl    = Get-SkinFileUrl -RepoUrl $repoUrl -Branch $branch -FilePath $relPath
            $parentDir = Split-Path -Parent $localFile
            if (-not (Test-Path $parentDir)) { New-Item -ItemType Directory -Path $parentDir | Out-Null }
            try {
                Invoke-WebRequest -Uri $rawUrl -OutFile "$localFile.tmp"
                Move-Item -Force "$localFile.tmp" $localFile
                $changed++
            } catch {
                Log "Warning: failed to download $relPath"
                Remove-Item -Force "$localFile.tmp" -ErrorAction SilentlyContinue
            }
        }

        Log "$skinName`: $changed file(s) updated."
    }

    Set-Content $VersionFile $LatestReleaseId
    Log "Update complete ($($Release.tag_name)). Restart YataiDON to apply."

} finally {
    Remove-Item -Recurse -Force $TmpDir -ErrorAction SilentlyContinue
}
