#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build/native}"
CONFIG="${2:-Release}"
KEEP_LATEST_ONLY="${KEEP_LATEST_ONLY:-1}"

remove_stale_config_dirs() {
  local root_dir="$1"
  local keep_config="$2"
  [[ -d "$root_dir" ]] || return 0
  for candidate in Debug Release RelWithDebInfo MinSizeRel; do
    if [[ "$candidate" != "$keep_config" && -d "$root_dir/$candidate" ]]; then
      rm -rf "$root_dir/$candidate"
    fi
  done
}

cmake -S native -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --config "$CONFIG"

if [[ "$KEEP_LATEST_ONLY" == "1" ]]; then
  remove_stale_config_dirs "$BUILD_DIR" "$CONFIG"
  remove_stale_config_dirs "$BUILD_DIR/tests" "$CONFIG"
fi
