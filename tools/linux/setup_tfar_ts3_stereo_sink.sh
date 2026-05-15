#!/usr/bin/env bash
set -euo pipefail

sink_name="${TFAR_TS3_SINK_NAME:-tfar_ts3_stereo}"
display_name="${TFAR_TS3_DISPLAY_NAME:-TFAR_TS3_Stereo}"
master_sink="${TFAR_TS3_MASTER_SINK:-alsa_output.usb-Focusrite_Scarlett_16i16_4th_Gen_S6XAXC948001BF-00.multichannel-output}"
channel_map="${TFAR_TS3_CHANNEL_MAP:-front-left,front-right}"
master_channel_map="${TFAR_TS3_MASTER_CHANNEL_MAP:-aux0,aux1}"

if ! command -v pactl >/dev/null 2>&1; then
    echo "pactl is required, but it was not found in PATH." >&2
    exit 1
fi

if pactl list short sinks | awk '{print $2}' | grep -Fxq "${sink_name}"; then
    echo "TFAR TeamSpeak stereo sink already exists: ${sink_name}"
    exit 0
fi

if ! pactl list short sinks | awk '{print $2}' | grep -Fxq "${master_sink}"; then
    echo "Master sink not found: ${master_sink}" >&2
    echo "Available sinks:" >&2
    pactl list short sinks >&2
    exit 1
fi

module_id="$(
    pactl load-module module-remap-sink \
        sink_name="${sink_name}" \
        sink_properties="device.description=${display_name}" \
        master="${master_sink}" \
        channels=2 \
        channel_map="${channel_map}" \
        master_channel_map="${master_channel_map}" \
        remix=no
)"

echo "Created TFAR TeamSpeak stereo sink '${sink_name}' as module ${module_id}."
echo "Select '${display_name}' / '${sink_name}' as the TeamSpeak playback device."
