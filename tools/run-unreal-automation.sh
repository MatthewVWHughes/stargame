#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
	echo "usage: $0 <AutomationTestFilter> [extra UnrealEditor-Cmd args...]" >&2
	exit 2
fi

TEST_FILTER="$1"
shift

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
UNREAL_ROOT="${UNREAL_ROOT:-/home/matthew/UnrealEngine/UE_5.7.4}"

export SDL_AUDIO_DRIVER="${SDL_AUDIO_DRIVER:-pulseaudio}"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-pulseaudio}"
export PULSE_LATENCY_MSEC="${PULSE_LATENCY_MSEC:-60}"

"$UNREAL_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" \
	"$PROJECT_ROOT/Stargame.uproject" \
	-unattended \
	-nop4 \
	-nosplash \
	-NullRHI \
	-ExecCmds="Automation RunTests ${TEST_FILTER}; Quit" \
	-TestExit="Automation Test Queue Empty" \
	"$@"
