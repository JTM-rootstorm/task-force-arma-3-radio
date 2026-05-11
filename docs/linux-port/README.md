# TFAR Linux/Proton Port

Status: alpha scaffolding. This branch adds the transport boundary and bridge pieces needed for a native Linux TeamSpeak plugin plus a Proton-side Arma extension DLL. It is not yet a validated playable Linux release.

## Architecture

The Linux path is a transport replacement:

```text
Arma 3 under Proton
  -> task_force_radio_pipe_x64.dll built with TFAR_USE_SOCKET_BRIDGE
  -> TCP bridge on 127.0.0.1:47333
  -> TFAR_linux_x64.so native TeamSpeak plugin bridge endpoint
  -> existing TFAR command processor and audio/radio logic
```

The native Windows path remains unchanged by default:

```text
Windows Arma 3 -> SharedMemoryTransfer -> Windows TeamSpeak plugin SharedMemoryHandler
```

The bridge preserves existing TFAR command strings. Async commands keep the legacy trailing `~` marker on the Arma side, then cross the bridge as explicit async frames without the marker.

## Current Implementation

- `common/bridge/BridgeProtocol.hpp` defines the v1 length-framed protocol.
- `ts/src/transport/IGameTransport.hpp` abstracts the TeamSpeak command transport.
- `ts/src/transport/WinSharedMemoryTransport.*` adapts the existing Windows shared-memory handler.
- `ts/src/transport/LinuxBridgeServer.*` implements the Linux loopback bridge server.
- `extensions/task_force_radio_pipe/SocketTransfer.*` implements the Proton-side Winsock client, selected with `TFAR_USE_SOCKET_BRIDGE`.
- `ts/CMakeLists.txt` builds protocol smoke tests and an experimental Linux plugin skeleton.

The experimental `TFAR_linux_x64.so` target currently proves native load/shutdown and bridge listen behavior. It does not yet package the full TFAR audio/radio runtime into a playable Linux TeamSpeak plugin.

The Linux target baseline is TeamSpeak 3 Client 3.6.2, which expects TeamSpeak plugin API 26. The plugin must report API 26 for that client line.

Until the full TFAR command processor is built into the Linux plugin, sync commands return `bridge command processor unavailable` immediately. This avoids freezing Arma on repeated 1000 ms timeouts, but it also means the addon will remain disconnected rather than playable.

## Defaults

- Host: `127.0.0.1`
- Port: `47333`
- Token: optional unless `TFAR_REQUIRE_BRIDGE_TOKEN` is enabled
- Sync timeout: existing `PIPE_TIMEOUT`, 1000 ms

Do not bind the bridge to public interfaces.
