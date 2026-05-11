# TFAR Proton Runtime Loader

The Arma-visible extension remains:

```text
task_force_radio_pipe_x64.dll
```

That DLL now selects its transport at runtime. Native Windows keeps using the existing `SharedMemoryTransfer` path. Wine/Proton can load an adjacent bridge payload:

```text
tfar_proton_bridge_x64.dll
```

The bridge DLL owns `SocketTransfer` and connects to the native Linux TeamSpeak plugin bridge server on `127.0.0.1:47333` by default.

## Loader Policy

Selection happens once per process on the first `RVExtension` call.

```text
TFAR_DISABLE_PROTON_BRIDGE=1
  Always use SharedMemoryTransfer.

TFAR_FORCE_PROTON_BRIDGE=1
  Try the adjacent bridge DLL even on Windows.
  If loading fails, RVExtension returns "TFAR Proton bridge unavailable".

Wine plus STEAM_COMPAT_DATA_PATH, STEAM_COMPAT_CLIENT_INSTALL_PATH, SteamGameId, or SteamAppId
  Try the adjacent bridge DLL.
  If loading fails, fall back to SharedMemoryTransfer.

Wine plus adjacent tfar_proton_bridge_x64.dll
  Try the adjacent bridge DLL.
  If loading fails, fall back to SharedMemoryTransfer.

Anything else
  Use SharedMemoryTransfer.
```

Enabled boolean values are `1`, `true`, `yes`, and `on`, case-insensitive.

## Placement Rule

The loader only looks beside `task_force_radio_pipe_x64.dll`:

```text
<TFAR mod root>/task_force_radio_pipe_x64.dll
<TFAR mod root>/tfar_proton_bridge_x64.dll
```

It does not call `LoadLibraryW` with a bare filename and does not search the current working directory.

For a Steam Workshop install, the mod root is often:

```text
<SteamLibrary>/steamapps/workshop/content/107410/894678801
```

Manual installs should use the root of the TFAR mod folder, at the same level as `addons`.

## Bridge ABI

`tfar_proton_bridge_x64.dll` exports only the dispatcher ABI:

```cpp
uint32_t __stdcall TFARBridge_GetApiVersion();
void __stdcall TFARBridge_RVExtension(char* output, int outputSize, const char* input);
void __stdcall TFARBridge_Shutdown();
```

The current API version is `1`. The bridge DLL must not export plain `RVExtension`; Arma should only load `task_force_radio_pipe_x64.dll`.

## Bridge Settings

The socket bridge keeps the existing variables:

```text
TFAR_BRIDGE_HOST=127.0.0.1
TFAR_BRIDGE_PORT=47333
TFAR_BRIDGE_TOKEN=<optional token>
```

The TeamSpeak-side token enforcement variable remains separate:

```text
TFAR_REQUIRE_BRIDGE_TOKEN=1
```

## Packaging Status

This loader split is a binary architecture change only. Mod signing, Workshop packaging, and upstream PR mechanics are deferred.
