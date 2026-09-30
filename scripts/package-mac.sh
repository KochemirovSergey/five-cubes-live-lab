#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
export TMPDIR="$ROOT/.local_tmp/unreal"
mkdir -p "$TMPDIR" "$ROOT/.cache/node" "$ROOT/.runtime/package"
NODE_VERSION=22.22.3
NODE_ARCHIVE="node-v${NODE_VERSION}-darwin-arm64.tar.gz"
if [[ ! -x "$ROOT/.cache/node/node-v${NODE_VERSION}-darwin-arm64/bin/node" ]]; then
  curl --fail --location "https://nodejs.org/dist/v${NODE_VERSION}/${NODE_ARCHIVE}" -o "$ROOT/.cache/node/$NODE_ARCHIVE"
  curl --fail --location "https://nodejs.org/dist/v${NODE_VERSION}/SHASUMS256.txt" -o "$ROOT/.cache/node/SHASUMS256.txt"
  (cd "$ROOT/.cache/node" && rg " ${NODE_ARCHIVE}$" SHASUMS256.txt | shasum -a 256 -c -)
  tar -xzf "$ROOT/.cache/node/$NODE_ARCHIVE" -C "$ROOT/.cache/node"
fi
COOK_FLAG=-cook
if [[ "${SKIP_COOK:-0}" == "1" ]]; then COOK_FLAG=-skipcook; fi
bash "$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$ROOT/unreal/FiveCubes/FiveCubes.uproject" -noP4 -platform=Mac -clientconfig=Development -build "$COOK_FLAG" -stage -pak -archive -archivedirectory="$ROOT/.runtime/package" -map=/Engine/Maps/Entry -unattended -utf8output
python3 "$ROOT/scripts/finish-package.py"
