# Linux Bridge Architecture

## Components

### Native Linux TeamSpeak plugin

Build artifact: `TFAR_linux_x64.so`

The Linux plugin owns the bridge server. It binds to `127.0.0.1`, accepts one Proton-side Arma extension connection, receives framed TFAR commands, and exposes them through `IGameTransport`.

### Proton bridge DLL

Build artifact: `task_force_radio_pipe_x64.dll`

The DLL remains a Windows DLL because Arma 3 under Proton loads Windows extensions. When built with `TFAR_USE_SOCKET_BRIDGE`, `RVExtension` calls use `SocketTransfer` instead of `SharedMemoryTransfer`.

### Shared bridge protocol

The protocol is a 16-byte little-endian header followed by a UTF-8 payload. Command payloads are the existing TFAR command strings.

Frame types include:

- `Hello` / `HelloAck`
- `Ping` / `Pong`
- `SyncCommand`
- `AsyncCommand`
- `Response`
- `Error`
- `Disconnect`

The payload cap is 64 KiB.

## Compatibility Rules

- Native Windows builds keep shared memory as the default transport.
- `RVExtension` signature is unchanged.
- Existing command strings are not rewritten.
- Socket I/O belongs to bridge/transport worker paths, not TeamSpeak audio callbacks.
- Release packaging should require a token handshake before broader distribution.
