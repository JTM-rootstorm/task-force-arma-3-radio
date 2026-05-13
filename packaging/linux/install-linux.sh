#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TS3_PLUGIN_DIR="${TFAR_TS3_PLUGIN_DIR:-}"
ARMA_MOD_DIR="${TFAR_ARMA_MOD_DIR:-}"

backup_copy() {
    local src="$1"
    local dest="$2"
    mkdir -p "$(dirname "$dest")"
    if [[ -e "$dest" ]]; then
        cp -a "$dest" "$dest.bak.$(date +%Y%m%d%H%M%S)"
    fi
    cp -a "$src" "$dest"
}

if [[ -z "$TS3_PLUGIN_DIR" ]]; then
    for candidate in "$HOME/.var/app/com.teamspeak.TeamSpeak3/.ts3client/plugins" "$HOME/.ts3client/plugins" "$HOME/.local/share/TeamSpeak 3/plugins"; do
        if [[ -d "$candidate" ]]; then
            TS3_PLUGIN_DIR="$candidate"
            break
        fi
    done
fi

if [[ -z "$TS3_PLUGIN_DIR" ]]; then
    echo "Set TFAR_TS3_PLUGIN_DIR to your TeamSpeak plugin directory." >&2
    exit 1
fi

echo "Installing TeamSpeak plugin to: $TS3_PLUGIN_DIR"
if [[ -e "$TS3_PLUGIN_DIR/TFAR_linux_x64.so" ]]; then
    mv "$TS3_PLUGIN_DIR/TFAR_linux_x64.so" "$TS3_PLUGIN_DIR/TFAR_linux_x64.so.disabled.$(date +%Y%m%d%H%M%S)"
fi
if [[ -e "$TS3_PLUGIN_DIR/TFAR_win64_x64.so" ]]; then
    mv "$TS3_PLUGIN_DIR/TFAR_win64_x64.so" "$TS3_PLUGIN_DIR/TFAR_win64_x64.so.disabled.$(date +%Y%m%d%H%M%S)"
fi
backup_copy "$ROOT_DIR/plugins/linux/TFAR_win64_linux_amd64.so" "$TS3_PLUGIN_DIR/TFAR_win64_linux_amd64.so"

if [[ -n "$ARMA_MOD_DIR" ]]; then
    echo "Installing Proton bridge DLL under: $ARMA_MOD_DIR"
    backup_copy "$ROOT_DIR/arma-extension/task_force_radio_pipe_x64.dll" "$ARMA_MOD_DIR/task_force_radio_pipe_x64.dll"
else
    echo "TFAR_ARMA_MOD_DIR not set; skipping Proton bridge DLL install."
fi

echo "Done. Restart TeamSpeak, then launch Arma 3 under Proton."
