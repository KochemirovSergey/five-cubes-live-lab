#!/bin/bash
set -euo pipefail
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_FILE="$PROJECT_ROOT/unreal/FiveCubes/FiveCubes.uproject"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
if [[ -z "${DEVELOPER_DIR:-}" && -d /Applications/Xcode.app/Contents/Developer ]]; then
  export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
fi
BUILD="$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh"
EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"

if [[ ! -f "$BUILD" || ! -x "$EDITOR" ]]; then
  echo "Unreal не найден: $UE_ROOT"
  echo 'Установите UE в Epic Games Launcher. Для другого пути: UE_ROOT="/путь/UE_5.x" npm run unreal'
  exit 1
fi
if ! xcodebuild -version >/dev/null 2>&1; then
  echo 'Нужен полный Xcode, совместимый с выбранной версией Unreal. Command Line Tools недостаточно.'
  exit 1
fi
if ! xcrun metal --version >/dev/null 2>&1; then
  echo 'Metal Toolchain недоступен. Завершите настройку Xcode и выполните:'
  echo 'DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer xcodebuild -downloadComponent MetalToolchain'
  exit 1
fi
if [[ ! -f "$PROJECT_ROOT/.runtime/connection.json" ]]; then
  echo 'Сначала запустите npm start в другом терминале.'
  exit 1
fi
mkdir -p "$PROJECT_ROOT/.local_tmp/unreal"
export TMPDIR="$PROJECT_ROOT/.local_tmp/unreal"
bash "$BUILD" FiveCubesEditor Mac Development "$PROJECT_FILE" -waitmutex
exec "$EDITOR" "$PROJECT_FILE" /Engine/Maps/Entry -game -windowed -ResX=1280 -ResY=800 \
  "-LabConnectionFile=$PROJECT_ROOT/.runtime/connection.json" -log
