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
- Confirm the Proton DLL was built with `TFAR_USE_SOCKET_BRIDGE`.
- Confirm `TFAR_BRIDGE_PORT` matches on both sides.
- Confirm `TFAR_BRIDGE_TOKEN` matches if token enforcement is enabled.
- Check whether another local process is using the configured port.

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
