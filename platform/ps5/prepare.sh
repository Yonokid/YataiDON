#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
vendor="$root/platform/ps5/vendor"
mkdir -p "$vendor"
checkout() {
  local name=$1 url=$2 revision=$3
  if [[ ! -d "$vendor/$name" ]]; then
    git clone --no-checkout --depth 1 "$url" "$vendor/$name"
    git -C "$vendor/$name" fetch --depth 1 origin "$revision"
    git -C "$vendor/$name" checkout --detach "$revision"
  fi
  [[ $(git -C "$vendor/$name" rev-parse HEAD) == "$revision" ]] || {
    echo "Unexpected $name revision; preserving the existing dependency checkout." >&2; exit 1;
  }
  [[ -z $(git -C "$vendor/$name" status --porcelain --untracked-files=no) ]] || {
    echo "Tracked edits in $name; preserving the existing dependency checkout." >&2; exit 1;
  }
}
checkout raylib https://github.com/raysan5/raylib.git 02566453880e06bec97f5b8160fa6fe1e98397ca
checkout SDL https://github.com/ps5-payload-dev/SDL.git 8c56053f13ca13a0c050de613706ff69eb615836
python3 "$root/platform/ps5/prepare-game.py"
