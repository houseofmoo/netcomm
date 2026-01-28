#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# Usage:
#   ./build.sh [--debug | --release] [--clean] [--run] [--log]
# ------------------------------------------------------------
# Examples:
#   ./build.sh --debug
#   ./build.sh --release --clean
# ============================================================

PRESET="linux-debug"
BUILD_DIR="build"
CLEAN="false"
RUN_AFTER_BUILD="false"
LOG="0"

usage() {
  echo "Usage: ./build.sh [--debug|--release] [--clean] [--run] [--log]"
}

# -------- parse args --------
while [[ $# -gt 0 ]]; do
  case "$1" in
    --run)
      RUN_AFTER_BUILD="true"
      shift
      ;;
    --debug)
      PRESET="linux-debug"
      shift
      ;;
    --release)
      PRESET="linux-release"
      shift
      ;;
    --clean)
      CLEAN="true"
      shift
      ;;
    --log)
      LOG="1"
      shift
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

# -------- optional clean --------
if [[ "$CLEAN" == "true" ]]; then
  if [[ -d "$BUILD_DIR" ]]; then
    echo "Cleaning build directory \"$BUILD_DIR\"..."
    rm -rf -- "$BUILD_DIR"
  else
    echo "Build directory \"$BUILD_DIR\" does not exist, skipping clean"
  fi
fi

echo
echo "Running CMake configure preset \"$PRESET\""
cmake --preset "$PRESET" -DEROIL_ELOG_ENABLED="$LOG"

echo
echo "Building project for preset \"$PRESET\""
cmake --build --preset "$PRESET" --parallel

echo
echo "Build completed successfully for preset \"$PRESET\""

if [[ "$RUN_AFTER_BUILD" == "true" ]]; then
  echo
  echo "Running the built executable..."
  if [[ -x "./run.sh" ]]; then
    ./run.sh
  else
    # fallback: allow run.sh without +x
    bash ./run.sh
  fi
fi
