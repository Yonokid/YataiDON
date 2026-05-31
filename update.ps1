# YataiDON Windows Updater
#
# Expected GitHub release assets:
#   checksums-windows.sha256    sha256sum-format, relative paths from install dir
#   update-windows.tar.gz       binary + dlls + shader
#
# Skins with a .skin-repo file are updated independently from the skin's own repo.
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

function Get-SkinRawUrl {
    param([string]$RepoUrl, [string]$Commit, [string]$FilePath)
    $base = $RepoUrl -replace '\.git$', ''
    if ($base -match 'github\.com') {
        $base = $base -replace 'github\.com', 'raw.githubusercontent.com'
        return "$base/$Commit/$FilePath"
    } else {
        return "$base/raw/commit/$Commit/$FilePath"
    }
}

function Get-LatestSkinCommit {
    param([string]$RepoUrl, [string]$Branch = "main")
    $base = $RepoUrl.TrimEnd('/') -replace '\.git$', ''
    if ($base -match 'github\.com') {
        $ownerRepo = ($base -split 'github\.com/')[-1]
        $data = Invoke-RestMethod -Uri "https://api.github.com/repos/$ownerRepo/commits/HEAD" -TimeoutSec 10
        return $data.sha
    } else {
        $uri    = [System.Uri]$base
        $path   = $uri.AbsolutePath.Trim('/')
        $host   = "$($uri.Scheme)://$($uri.Host)"
        $data   = Invoke-RestMethod -Uri "$host/api/v1/repos/$path/branches/$Branch" -TimeoutSec 10
        return $data.commit.id
    }
}

try {
    # --- Fetch release metadata ---
    Log "Checking for updates..."
    $Release   = Invoke-RestMethod -Uri $ApiUrl -TimeoutSec 10
    $LatestTag = $Release.tag_name
    $LocalTag  = if (Test-Path $VersionFile) { (Get-Content $VersionFile -Raw).Trim() } else { "none" }
    Log "Local: $LocalTag | Latest: $LatestTag"

    $AssetMap = @{}
    foreach ($asset in $Release.assets) { $AssetMap[$asset.name] = $asset.browser_download_url }

    # --- Download checksums ---
    if (-not $AssetMap.ContainsKey("checksums-windows.sha256")) {
        Die "No checksums-windows.sha256 in release $LatestTag"
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
    $SkinUpdates = [System.Collections.Generic.List[object]]::new()
    $SkinsDir = Join-Path $InstallDir "Skins"
    if (Test-Path $SkinsDir) {
        foreach ($skinDir in Get-ChildItem -Path $SkinsDir -Directory) {
            $skinRepoFile = Join-Path $skinDir.FullName ".skin-repo"
            if (-not (Test-Path $skinRepoFile)) { continue }

            $lines   = Get-Content $skinRepoFile
            $repoUrl = $lines[0].Trim()
            $branch  = if ($lines.Count -gt 1 -and $lines[1].Trim() -ne '') { $lines[1].Trim() } else { "main" }

            try {
                $latestCommit = Get-LatestSkinCommit -RepoUrl $repoUrl -Branch $branch
            } catch {
                Log "Warning: could not fetch latest commit for $($skinDir.Name) — skipping"
                continue
            }

            $localVersionFile = Join-Path $skinDir.FullName ".skin-version"
            $localCommit = if (Test-Path $localVersionFile) { (Get-Content $localVersionFile -Raw).Trim() } else { "none" }
            if ($localCommit -eq $latestCommit) { continue }

            $SkinUpdates.Add(@($skinDir.FullName, $repoUrl, $latestCommit))
        }
    }

    if (-not $NeedPackage -and $SkinUpdates.Count -eq 0) {
        Log "Already up to date."
        Set-Content $VersionFile $LatestTag
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
            Die "No update-windows.tar.gz in release $LatestTag"
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
        $skinDirPath, $repoUrl, $latestCommit = $update
        $skinName = Split-Path -Leaf $skinDirPath
        Log "Checking $skinName..."

        $checksumsUrl      = Get-SkinRawUrl -RepoUrl $repoUrl -Commit $latestCommit -FilePath "checksums.sha256"
        $skinChecksumsPath = Join-Path $TmpDir "skin-checksums.sha256"
        try {
            Invoke-WebRequest -Uri $checksumsUrl -OutFile $skinChecksumsPath -TimeoutSec 30
        } catch {
            Log "Warning: could not fetch checksums for $skinName — skipping"
            continue
        }

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

            $rawUrl    = Get-SkinRawUrl -RepoUrl $repoUrl -Commit $latestCommit -FilePath $relPath
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

        Set-Content (Join-Path $skinDirPath ".skin-version") $latestCommit
        Log "$skinName`: $changed file(s) updated."
    }

    Set-Content $VersionFile $LatestTag
    Log "Update complete ($LatestTag). Restart YataiDON to apply."

} finally {
    Remove-Item -Recurse -Force $TmpDir -ErrorAction SilentlyContinue
}
