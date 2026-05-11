# TFAR Linux/Proton Port

Status: alpha implementation. This branch adds the transport boundary and bridge pieces needed for a native Linux TeamSpeak plugin plus a Proton-side Arma extension DLL. It is not yet a validated playable Linux release.

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
- `ts/CMakeLists.txt` builds protocol smoke tests and an experimental Linux plugin with the TFAR command/audio runtime linked in.

The experimental `TFAR_linux_x64.so` target now packages the full TFAR command processor, audio/radio runtime, bridge server, Clunk, DSP filters, and SQLite into a native Linux TeamSpeak plugin. Direct speech and radio gameplay still need end-to-end validation under Arma 3/Proton before this can be called playable.

The Linux target baseline is TeamSpeak 3 Client 3.6.2, which expects TeamSpeak plugin API 26. The plugin must report API 26 for that client line.

TeamSpeak metadata is now updated through the normal TFAR runtime path when the Proton bridge socket connects and disconnects.

## Defaults

- Host: `127.0.0.1`
- Port: `47333`
- Token: optional unless `TFAR_REQUIRE_BRIDGE_TOKEN` is enabled
- Sync timeout: existing `PIPE_TIMEOUT`, 1000 ms

Do not bind the bridge to public interfaces.
