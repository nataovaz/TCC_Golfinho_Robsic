#!/usr/bin/env bash
# run_build_and_open.sh
# Compila (se necessário) e abre o Campus Itabira no Unreal Editor.
#
# Flags:
#   --no-build   Pula a compilação e abre o editor diretamente.
#   --build-only Apenas compila, sem abrir o editor.
set -euo pipefail

ENGINE_ROOT="$HOME/UnrealEngine-5.5.4"
PROJECT_ROOT="/home/natan/Documentos/TCC/tcc_extraido/Campus_Itabira"
PROJECT_FILE="$PROJECT_ROOT/Campus_Itabira.uproject"
MAP_PATH="/Game/CampusItabira"

detect_ros_root() {
    if [ -n "${UE_ROS_ROOT:-}" ] && [ -d "${UE_ROS_ROOT}/lib" ] && [ -d "${UE_ROS_ROOT}/include" ]; then
        printf '%s\n' "${UE_ROS_ROOT}"
        return 0
    fi

    local candidates=(
        "/opt/ros/jazzy"
        "$HOME/miniforge3/envs/ros_jazzy_env"
        "$HOME/ros2_jazzy/ros2-linux"
    )

    local candidate
    for candidate in "${candidates[@]}"; do
        if [ -d "${candidate}/lib" ] && [ -d "${candidate}/include" ]; then
            printf '%s\n' "${candidate}"
            return 0
        fi
    done

    return 1
}

if ROS_ROOT="$(detect_ros_root)"; then
    export UE_ROS_ROOT="$ROS_ROOT"
    export LD_LIBRARY_PATH="$ROS_ROOT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

    if [ -f "$ROS_ROOT/setup.bash" ]; then
        set +u
        # shellcheck source=/dev/null
        source "$ROS_ROOT/setup.bash"
        set -u
    fi

    echo "[ros] Runtime detectado: $ROS_ROOT"
else
    echo "[ros] Aviso: nenhum runtime Jazzy detectado; usando fallback do plugin."
fi

# Limita ações paralelas: deixa 2 núcleos livres para o sistema não travar.
# Mínimo de 2 para não degradar demais builds em máquinas com poucos núcleos.
TOTAL_CORES="$(nproc)"
if [ "$TOTAL_CORES" -gt 4 ]; then
    DEFAULT_PARALLEL=$(( TOTAL_CORES - 2 ))
else
    DEFAULT_PARALLEL=2
fi
MAX_PARALLEL_ACTIONS="${MAX_PARALLEL_ACTIONS:-$DEFAULT_PARALLEL}"

DO_BUILD=true
DO_OPEN=true

for arg in "$@"; do
    case "$arg" in
        --no-build)   DO_BUILD=false ;;
        --build-only) DO_OPEN=false  ;;
    esac
done

# ── Compilação ─────────────────────────────────────────────────────────────
if $DO_BUILD; then
    echo "[build] Iniciando compilação com $MAX_PARALLEL_ACTIONS ações paralelas..."
    "$ENGINE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" \
      Campus_ItabiraEditor \
      Linux \
      Development \
      "-Project=$PROJECT_FILE" \
      "-MaxParallelActions=$MAX_PARALLEL_ACTIONS" \
      -NoUBALocal \
      -NoUBA
    echo "[build] Concluído."

    # Pausa para liberar RAM/CPU antes de abrir o editor.
    if $DO_OPEN; then
        echo "[build] Aguardando 3 s para liberar memória antes de abrir o editor..."
        sleep 3
    fi
fi

# ── Abertura do editor ──────────────────────────────────────────────────────
if $DO_OPEN; then
    echo "[editor] Abrindo UnrealEditor..."
    exec "$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor" \
      "$PROJECT_FILE" \
      "$MAP_PATH" \
      -nosound \
      -NOTEXTURESTREAMING \
      -ReduceThreadUsage
fi
