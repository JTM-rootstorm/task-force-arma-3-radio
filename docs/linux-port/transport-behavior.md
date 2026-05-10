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
