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

source "$SCRIPT_DIR/unreal-audio-env.sh"
configure_unreal_audio_env

"$UNREAL_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" \
	"$PROJECT_ROOT/Stargame.uproject" \
	-unattended \
	-nop4 \
	-nosplash \
	-NullRHI \
	-ExecCmds="Automation RunTests ${TEST_FILTER}; Quit" \
	-TestExit="Automation Test Queue Empty" \
	"$@"
