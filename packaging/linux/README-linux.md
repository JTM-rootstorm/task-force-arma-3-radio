# TFAR Linux Bridge Alpha Package

This package is expected to contain:

```text
plugins/linux/TFAR_win64_x64.so
arma-extension/task_force_radio_pipe_x64.dll
resources/radio-sounds/
docs/bridge-protocol.md
troubleshooting.md
install-linux.sh
```

Install the TeamSpeak plugin into your native Linux TeamSpeak 3 plugin directory, then install the Proton bridge DLL into the TFAR Arma mod extension path used by Arma 3 under Proton.

The Linux TeamSpeak plugin is intentionally installed as `TFAR_win64_x64.so`. TeamSpeak derives the plugin-command namespace from the filename; this name keeps the Linux plugin interoperable with existing Windows TFAR clients that send and receive commands under the `TFAR_win64` namespace.

Environment variables:

```text
TFAR_TS3_PLUGIN_DIR
TFAR_ARMA_MOD_DIR
TFAR_BRIDGE_PORT
TFAR_BRIDGE_TOKEN
TFAR_BRIDGE_CONFIG
```

The bridge binds to `127.0.0.1` by default. Do not expose it on a public interface.
