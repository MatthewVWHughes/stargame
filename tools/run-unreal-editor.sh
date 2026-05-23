#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
UNREAL_ROOT="${UNREAL_ROOT:-/home/matthew/UnrealEngine/UE_5.7.4}"

source "$SCRIPT_DIR/unreal-audio-env.sh"
configure_unreal_audio_env

"$UNREAL_ROOT/Engine/Binaries/Linux/UnrealEditor" \
	"$PROJECT_ROOT/Stargame.uproject" \
	"$@"
