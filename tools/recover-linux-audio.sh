#!/usr/bin/env bash
set -euo pipefail

systemctl --user restart wireplumber pipewire pipewire-pulse

if command -v wpctl >/dev/null 2>&1; then
	wpctl status
else
	systemctl --user --no-pager --plain status pipewire pipewire-pulse wireplumber
fi
