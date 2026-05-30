#!/bin/bash
# YataiDON Linux Updater
#
# Expected GitHub release assets:
#   checksums.sha256        sha256sum-format, relative paths from install dir
#                           (excludes git-managed skins)
#   skin-manifest.tsv       path<TAB>repo_url<TAB>commit  for git-managed skins
#   YataiDON                main binary
#   shader.tar.gz           extracts to shader/
#   lib.tar.gz              extracts to lib/
#   Skins__<path>           individual files for bundled skins; '/' encoded as '__'
#
# Usage (standalone):   ./update.sh
# Usage (from game):    ./update.sh --wait-pid <PID>
#
# Requires: curl, python3, sha256sum, tar
# For git-managed skins: git

set -euo pipefail

REPO="yonokid/YataiDON"
API_URL="https://api.github.com/repos/$REPO/releases/latest"
INSTALL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION_FILE="$INSTALL_DIR/.version"
TMP_DIR="$(mktemp -d)"
WAIT_PID=""

cleanup() { rm -rf "$TMP_DIR"; }
trap cleanup EXIT

while [[ $# -gt 0 ]]; do
    case "$1" in
        --wait-pid) WAIT_PID="$2"; shift 2 ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
done

die() { echo "[update] Error: $*" >&2; exit 1; }
log() { echo "[update] $*"; }

# --- Fetch release metadata ---
log "Checking for updates..."
RELEASE_JSON=$(curl -sf --max-time 10 "$API_URL") || die "GitHub API unreachable"

LATEST_TAG=$(python3 -c "import json,sys; print(json.loads(sys.stdin.read())['tag_name'])" <<< "$RELEASE_JSON")
[ -z "$LATEST_TAG" ] && die "Could not parse tag_name from API response"

LOCAL_TAG=$(cat "$VERSION_FILE" 2>/dev/null || echo "none")
log "Local: $LOCAL_TAG | Latest: $LATEST_TAG"

python3 -c "
import json, sys
for a in json.loads(sys.stdin.read()).get('assets', []):
    print(a['name'] + '\t' + a['browser_download_url'])
" <<< "$RELEASE_JSON" > "$TMP_DIR/assets.tsv"

asset_url() {
    awk -F'\t' -v name="$1" '$1==name {print $2}' "$TMP_DIR/assets.tsv"
}

# --- Download checksums and skin manifest ---
CHECKSUMS_URL=$(asset_url "checksums.sha256")
[ -z "$CHECKSUMS_URL" ] && die "No checksums.sha256 in release $LATEST_TAG"
curl -sf --max-time 30 -o "$TMP_DIR/checksums.sha256" "$CHECKSUMS_URL"

SKIN_MANIFEST_URL=$(asset_url "skin-manifest.tsv")
if [ -n "$SKIN_MANIFEST_URL" ]; then
    curl -sf --max-time 10 -o "$TMP_DIR/skin-manifest.tsv" "$SKIN_MANIFEST_URL"
else
    touch "$TMP_DIR/skin-manifest.tsv"
fi

# --- Compare file hashes ---
declare -A NEED_ARCHIVE
declare -A NEED_FILE      # encoded_asset_name → rel_path
NEED_BINARY=0

while read -r expected_hash rel_path; do
    local_file="$INSTALL_DIR/$rel_path"
    if [ -f "$local_file" ]; then
        actual_hash=$(sha256sum "$local_file" | awk '{print $1}')
        [ "$actual_hash" = "$expected_hash" ] && continue
    fi

    case "$rel_path" in
        YataiDON)  NEED_BINARY=1 ;;
        shader/*)  NEED_ARCHIVE["shader.tar.gz"]=1 ;;
        lib/*)     NEED_ARCHIVE["lib.tar.gz"]=1 ;;
        Skins/*)
            encoded="${rel_path//\//__}"
            NEED_FILE["$encoded"]="$rel_path"
            ;;
        *) log "Warning: unrecognized path in checksums: $rel_path" ;;
    esac
done < "$TMP_DIR/checksums.sha256"

# --- Check git-managed skins ---
declare -A NEED_GIT_SKIN  # skin_path → "repo_url commit"
HAS_GIT=$(command -v git &>/dev/null && echo 1 || echo 0)

while IFS=$'\t' read -r skin_path repo_url expected_commit; do
    [ -z "$skin_path" ] && continue
    local_dir="$INSTALL_DIR/$skin_path"

    if [ "$HAS_GIT" = "1" ] && [ -d "$local_dir/.git" ]; then
        actual_commit=$(git -C "$local_dir" rev-parse HEAD 2>/dev/null || echo "none")
    else
        actual_commit="none"
    fi

    [ "$actual_commit" != "$expected_commit" ] && NEED_GIT_SKIN["$skin_path"]="$repo_url $expected_commit"
done < "$TMP_DIR/skin-manifest.tsv"

if [ $NEED_BINARY -eq 0 ] && [ ${#NEED_ARCHIVE[@]} -eq 0 ] && \
   [ ${#NEED_FILE[@]} -eq 0 ] && [ ${#NEED_GIT_SKIN[@]} -eq 0 ]; then
    log "Already up to date."
    echo "$LATEST_TAG" > "$VERSION_FILE"
    exit 0
fi

log "Updates needed — binary: $NEED_BINARY | archives: ${!NEED_ARCHIVE[*]:-none} | skin files: ${#NEED_FILE[@]} | git skins: ${!NEED_GIT_SKIN[*]:-none}"

# --- Wait for game process if requested ---
if [ -n "$WAIT_PID" ]; then
    log "Waiting for game (PID $WAIT_PID) to exit..."
    while kill -0 "$WAIT_PID" 2>/dev/null; do sleep 0.25; done
fi

# --- Binary ---
if [ $NEED_BINARY -eq 1 ]; then
    url=$(asset_url "YataiDON")
    [ -z "$url" ] && die "No download URL for YataiDON binary"
    log "Downloading binary..."
    curl -f --progress-bar -o "$TMP_DIR/YataiDON" "$url"
    mv "$TMP_DIR/YataiDON" "$INSTALL_DIR/YataiDON"
    chmod +x "$INSTALL_DIR/YataiDON"
    log "Binary updated."
fi

# --- Archives (shader, lib) ---
for archive in "${!NEED_ARCHIVE[@]}"; do
    url=$(asset_url "$archive")
    if [ -z "$url" ]; then
        log "Warning: no release asset for $archive — skipping"
        continue
    fi
    log "Downloading $archive..."
    curl -f --progress-bar -o "$TMP_DIR/$archive" "$url"
    tar -xzf "$TMP_DIR/$archive" -C "$INSTALL_DIR"
    log "$archive done."
done

# --- Individual bundled skin files ---
for encoded in "${!NEED_FILE[@]}"; do
    rel_path="${NEED_FILE[$encoded]}"
    url=$(asset_url "$encoded")
    if [ -z "$url" ]; then
        log "Warning: no release asset for $encoded — skipping"
        continue
    fi
    log "Updating $rel_path..."
    dest="$INSTALL_DIR/$rel_path"
    mkdir -p "$(dirname "$dest")"
    curl -f --progress-bar -o "$dest.tmp" "$url"
    mv "$dest.tmp" "$dest"
done

# --- Git-managed skins ---
if [ ${#NEED_GIT_SKIN[@]} -gt 0 ]; then
    [ "$HAS_GIT" = "0" ] && die "git not found — required to update skins. Install git and retry."

    for skin_path in "${!NEED_GIT_SKIN[@]}"; do
        read -r repo_url expected_commit <<< "${NEED_GIT_SKIN[$skin_path]}"
        local_dir="$INSTALL_DIR/$skin_path"
        log "Updating $skin_path from git..."

        if [ -d "$local_dir/.git" ]; then
            # Try fetching the pinned commit directly; fall back to fetching default branch
            git -C "$local_dir" fetch --depth 1 origin "$expected_commit" 2>/dev/null || \
                git -C "$local_dir" fetch --depth 1 origin
            git -C "$local_dir" reset --hard FETCH_HEAD
        else
            rm -rf "$local_dir"
            git clone --depth 1 "$repo_url" "$local_dir"
        fi
        log "$skin_path done."
    done
fi

echo "$LATEST_TAG" > "$VERSION_FILE"
log "Update complete ($LATEST_TAG). Restart YataiDON to apply."
