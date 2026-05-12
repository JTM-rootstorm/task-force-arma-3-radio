# Linux Port Troubleshooting

## TeamSpeak Does Not Show the Plugin

- Confirm `TFAR_linux_x64.so` is in the TeamSpeak plugin directory.
- Confirm the file is readable and executable.
- Check TeamSpeak client logs for plugin API mismatch.
- Confirm the build architecture is x86_64.

Common plugin paths to check:

```text
~/.ts3client/plugins/
~/.local/share/TeamSpeak 3/plugins/
```

Flatpak TeamSpeak may use a sandboxed path.

## Arma Says TeamSpeak Is Disconnected

- Confirm the Linux plugin is loaded and listening on `127.0.0.1:<port>`.
- Confirm both `task_force_radio_pipe_x64.dll` and `tfar_proton_bridge_x64.dll` are installed beside each other in the TFAR mod root.
- Confirm `TFAR_BRIDGE_PORT` matches on both sides.
- Confirm `TFAR_BRIDGE_TOKEN` matches if token enforcement is enabled.
- Check whether another local process is using the configured port.
- If the DLL receives `bridge command processor unavailable`, an old skeleton plugin artifact is still installed.
- For loader checks, set `TFAR_FORCE_PROTON_BRIDGE=1`; a missing or invalid bridge DLL should return `TFAR Proton bridge unavailable`.

## TeamSpeak Shows No Game Connection

- The Linux plugin updates TFAR client metadata only after the Proton bridge socket connects.
- Flatpak TeamSpeak 3 should have `shared=network`; without that permission, Proton cannot reach the plugin's loopback listener.
- If the metadata stays disconnected but Arma is running, confirm the installed `tfar_proton_bridge_x64.dll` contains the socket bridge strings such as `TFAR_BRIDGE_PORT` and `tfar-arma-extension`.
- If metadata shows connected but Arma still reports TeamSpeak disconnected, verify `TS_INFO\tPING` reaches the Linux plugin and returns `PONG`.

## Connection Works But Radio Audio Is Broken

- Compare command timing logs against the Windows shared-memory baseline.
- Confirm metadata updates show the correct game-connected state.
- Test direct speech before radio.
- Test short-range radio before long-range or vehicle/intercom cases.

## Works Once, Then Fails After Restart

- Restart TeamSpeak and confirm the bridge server binds again.
- Restart Arma under Proton and confirm the DLL reconnects lazily.
- Clear stale token/port environment variables.
- Check for an old process still holding the port.
