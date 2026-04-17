#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build/native}"
OUTPUT_DIR="${2:-Release}"
KEEP_LATEST_ONLY="${KEEP_LATEST_ONLY:-1}"

remove_stale_sibling_releases() {
  local current_output_dir
  current_output_dir="$(cd "$1" && pwd -P)"
  local parent_dir
  parent_dir="$(dirname "$current_output_dir")"
  if [[ "$(basename "$parent_dir")" != "release" ]]; then
    return 0
  fi
  find "$parent_dir" -mindepth 1 -maxdepth 1 -type d ! -samefile "$current_output_dir" -exec rm -rf {} +
}

cmake --install "$BUILD_DIR" --prefix "$OUTPUT_DIR"

if [[ "$KEEP_LATEST_ONLY" == "1" ]]; then
  remove_stale_sibling_releases "$OUTPUT_DIR"
fi
