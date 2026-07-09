#!/usr/bin/env bash
# provision-lanyard.sh — factory-program the ble_prov partition (Go CLI wrapper)
#
# WHEN TO USE
#   Once per physical unit at factory / first bring-up, or after a full chip
#   erase that wiped ble_prov. Normal `idf.py flash` of the app does NOT
#   require re-running this — identity lives in a separate flash partition.
#
# REQUIREMENTS
#   - Go 1.21+ (for the blob builder)
#   - esptool.py or esptool (from ESP-IDF / pip) when flashing
#   - idf.py on PATH if you pass -flash-app
#
# EXAMPLES
#   ./tools/provision-lanyard.sh -port /dev/ttyACM0 -serial ILSS-LY-0001
#   ./tools/provision-lanyard.sh -serial ILSS-LY-0001 -out-only -out ble_prov.bin
#   ./tools/provision-lanyard.sh -port /dev/ttyACM0 -serial ILSS-LY-0001 -flash-app
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOL_DIR="$ROOT/tools/provision-lanyard"

if ! command -v go >/dev/null 2>&1; then
  echo "error: Go is required. Install from https://go.dev/dl/ then re-run." >&2
  exit 1
fi

cd "$TOOL_DIR"
exec go run . "$@"
