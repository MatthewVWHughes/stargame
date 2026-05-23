#!/usr/bin/env bash

configure_unreal_audio_env() {
	local requested_driver="${STARGAME_AUDIO_DRIVER:-${SDL_AUDIODRIVER:-${SDL_AUDIO_DRIVER:-auto}}}"
	local selected_driver="$requested_driver"

	if [[ "$requested_driver" == "auto" ]]; then
		selected_driver="alsa"
	fi

	export SDL_AUDIO_DRIVER="$selected_driver"
	export SDL_AUDIODRIVER="$selected_driver"

	if [[ "$selected_driver" == "pulseaudio" ]]; then
		export PULSE_LATENCY_MSEC="${PULSE_LATENCY_MSEC:-60}"
	else
		unset PULSE_LATENCY_MSEC
	fi

	echo "Stargame audio backend: ${selected_driver}" >&2
}
