#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$HOME/UnrealEngine-5.5.4"
PROJECT_ROOT="/home/natan/Documentos/TCC/tcc_extraido/Campus_Itabira"
PROJECT_FILE="$PROJECT_ROOT/Campus_Itabira.uproject"
MAP_PATH="/Game/CampusItabira"
MAX_PARALLEL_ACTIONS="${MAX_PARALLEL_ACTIONS:-$(nproc)}"

"$ENGINE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" \
  Campus_ItabiraEditor \
  Linux \
  Development \
  "-Project=$PROJECT_FILE" \
  "-MaxParallelActions=$MAX_PARALLEL_ACTIONS" \
  -NoUBALocal \
  -NoUBA

exec "$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor" "$PROJECT_FILE" "$MAP_PATH"
