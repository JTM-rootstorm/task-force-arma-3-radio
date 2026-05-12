# Linux Bridge Architecture

## Components

### Native Linux TeamSpeak plugin

Build artifact: `TFAR_linux_x64.so`

The Linux plugin owns the bridge server. It binds to `127.0.0.1`, accepts one Proton-side Arma extension connection, receives framed TFAR commands, and exposes them through `IGameTransport`.

### Arma extension runtime loader

Build artifact: `task_force_radio_pipe_x64.dll`

The DLL remains a Windows DLL because Arma 3 under Proton loads Windows extensions. It is still the only Arma-visible extension and still exports plain `RVExtension`.

On native Windows it uses `SharedMemoryTransfer`. Under Wine/Proton, or when forced by `TFAR_FORCE_PROTON_BRIDGE=1`, it attempts to load `tfar_proton_bridge_x64.dll` from the same directory.

### Proton bridge DLL

Build artifact: `tfar_proton_bridge_x64.dll`

The bridge DLL exports the small `TFARBridge_*` C ABI and owns `SocketTransfer`. It must sit beside `task_force_radio_pipe_x64.dll`; Arma should not load it directly.

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
