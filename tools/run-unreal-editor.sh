#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
UNREAL_ROOT="${UNREAL_ROOT:-/home/matthew/UnrealEngine/UE_5.7.4}"

export SDL_AUDIO_DRIVER="${SDL_AUDIO_DRIVER:-pulseaudio}"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-pulseaudio}"
export PULSE_LATENCY_MSEC="${PULSE_LATENCY_MSEC:-60}"

"$UNREAL_ROOT/Engine/Binaries/Linux/UnrealEditor" \
	"$PROJECT_ROOT/Stargame.uproject" \
	"$@"
