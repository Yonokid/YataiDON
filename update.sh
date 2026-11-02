#!/bin/bash
# YataiDON Linux Updater
#
# Expected GitHub release assets:
#   checksums-linux.sha256  sha256sum-format, relative paths from install dir
#   update-linux.tar.gz     binary + shader + lib
#
# Usage (standalone):   ./update.sh
# Usage (from game):    ./update.sh --wait-pid <PID>
#
# Requires: curl, python3, sha256sum, tar

set -euo pipefail

REPO="yonokid/YataiDON"
API_URL="https://api.github.com/repos/$REPO/releases/tags/updater"
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
RELEASE_JSON=$(curl -sfL --max-time 10 "$API_URL") || die "GitHub API unreachable"

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

# --- Download checksums ---
CHECKSUMS_URL=$(asset_url "checksums-linux.sha256")
[ -z "$CHECKSUMS_URL" ] && die "No checksums-linux.sha256 in release $LATEST_TAG"
curl -sfL --max-time 30 -o "$TMP_DIR/checksums.sha256" "$CHECKSUMS_URL"

# --- Check main package ---
NEED_PACKAGE=0
while read -r expected_hash rel_path; do
    rel_path="${rel_path#\*}"
    local_file="$INSTALL_DIR/$rel_path"
    if [ -f "$local_file" ]; then
        actual_hash=$(sha256sum "$local_file" | awk '{print $1}')
        [ "$actual_hash" = "$expected_hash" ] && continue
    fi
    NEED_PACKAGE=1
    break
done < "$TMP_DIR/checksums.sha256"

if [ $NEED_PACKAGE -eq 0 ]; then
    log "Already up to date."
    echo "$LATEST_TAG" > "$VERSION_FILE"
    exit 0
fi

log "Updates needed — package: $NEED_PACKAGE"

# --- Wait for game process if requested ---
if [ -n "$WAIT_PID" ]; then
    log "Waiting for game (PID $WAIT_PID) to exit..."
    while kill -0 "$WAIT_PID" 2>/dev/null; do sleep 0.25; done
fi

# --- Download and extract main package ---
url=$(asset_url "update-linux.tar.gz")
[ -z "$url" ] && die "No update-linux.tar.gz in release $LATEST_TAG"
log "Downloading update-linux.tar.gz..."
curl -fL --progress-bar -o "$TMP_DIR/update-linux.tar.gz" "$url"
log "Extracting..."
tar -xzf "$TMP_DIR/update-linux.tar.gz" -C "$INSTALL_DIR"
chmod +x "$INSTALL_DIR/YataiDON"
log "Package applied."

echo "$LATEST_TAG" > "$VERSION_FILE"
log "Update complete ($LATEST_TAG). Restart YataiDON to apply."
