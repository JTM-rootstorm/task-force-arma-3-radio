# Transport Behavior Baseline

The existing Windows transport behavior to preserve:

- The TeamSpeak plugin polls for game connection state.
- The game/extension sends synchronous command strings and waits for a response.
- The game/extension sends asynchronous command strings by appending `~`.
- The TeamSpeak plugin strips `~` before queueing async commands.
- `DFRAME~` can return `NEEDCFG` on the extension side when config refresh is required.
- `MISSIONEND~` shuts down the shared-memory game side.
- Sync calls use `PIPE_TIMEOUT`, currently 1000 ms.

The bridge keeps those semantics by translating the legacy suffix into `SyncCommand` or `AsyncCommand` frame types. Command payloads are otherwise unchanged.

Windows native remains the reference behavior until direct speech and radio gameplay are validated under Proton with native Linux TeamSpeak.

## Phase 4 Linux Bridge Baseline

The Linux `TFAR_linux_x64.so` target now links the full TFAR `CommandProcessor`, TeamSpeak callback surface, Clunk audio stack, DSP filters, SQLite, and the loopback `LinuxBridgeServer`.

On Linux, `plugin.cpp` creates `LinuxBridgeServer` as the `IGameTransport`. The shared command loop is otherwise the same transport-agnostic loop:

- If the bridge is disconnected, the loop sleeps briefly and keeps polling for reconnect.
- On connection, `onGameConnected` fires and TeamSpeak metadata is updated through the normal TFAR path.
- On disconnect, `onGameDisconnected` fires and normal cleanup runs.
- Sync bridge frames call `CommandProcessor::processCommand()` and send a bridge `Response`.
- Async bridge frames call `CommandProcessor::queueCommand()` and are handled on the command processor worker thread.
- `setConfigNeedsRefresh()` is still called before command polling so the extension side can observe `NEEDCFG` for async data-frame traffic.

The local validation baseline for this phase is:

- `cmake -S ts -B build/tfar-linux -DTFAR_LINUX_PLUGIN=ON -DTFAR_BRIDGE_PROTOCOL_TESTS=ON`
- `cmake --build build/tfar-linux --config Release`
- `ctest --test-dir build/tfar-linux --output-on-failure`
- Loading `build/tfar-linux/TFAR_linux_x64.so` with `ctypes.CDLL` succeeds.
- `ts3plugin_apiVersion()` returns `26` for the Linux TeamSpeak 3.6.2 target.
- `ldd -r build/tfar-linux/TFAR_linux_x64.so` reports no unresolved plugin-owned symbols.

## Command Inventory

Synchronous commands expected from the game side:

- `TS_INFO\tPING` returns `PONG`; this is the addon-side TeamSpeak-enabled probe.
- `TS_INFO\tVERSION` returns the TFAR plugin version.
- `TS_INFO\tSERVER`, `SERVERUID`, `CHANNEL`, and `CHANNELID` return current TeamSpeak context.
- `POS` queues position handling asynchronously, then falls through to the speaking-state response path.
- `IS_SPEAKING` returns two status bits for one nickname.
- `IS_SPEAKING_BULK` returns two status bits per requested nickname.
- `RECV_FREQS` returns the current receiving-frequency array when client data exists.

Asynchronous commands expected from the game side:

- `FREQ` updates radio/frequency/player state.
- `POS` updates unit position, speaking permissions, vehicle, terrain, and spectating flags.
- `KILLED` marks unit liveness and applies automatic mute rules.
- `TRACK` is a no-op on Linux because update/telemetry web calls are Windows-only.
- `DFRAME` advances the current data frame.
- `SPEAKERS` updates speaker placement data.
- `TANGENT`, `TANGENT_LR`, and `TANGENT_DD` update transmit state, play radio sounds, and send TeamSpeak plugin commands.
- `RELEASE_ALL_TANGENTS` releases forced transmit state.
- `SETCFG` updates TFAR plugin configuration.
- `MISSIONEND` is accepted as the legacy extension-side mission shutdown marker.
- `RadioTwrAdd` and `RadioTwrDel` update radio tower data.
- `collectDebugInfo` is accepted on Linux but reports that full debug collection is not implemented yet.

## Watch Points

- A response of `bridge command processor unavailable` now indicates an old skeleton plugin artifact is installed.
- Flatpak TeamSpeak still needs loopback network access; otherwise the native plugin cannot accept the Proton DLL connection.
- The Linux plugin load check must include `ldd -r` or equivalent because plain linking can still leave unresolved shared-object symbols.
- Direct speech and radio transmission remain the gameplay proof points before calling the Linux port playable.
