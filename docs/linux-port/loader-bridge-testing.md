# Loader And Bridge Testing

## Build Artifacts

Build the Windows-side loader and bridge with:

```powershell
msbuild extensions\task_force_radio_pipe\task_force_radio_pipe.sln /p:Configuration=Release /p:Platform=x64
```

Expected artifacts:

```text
extensions/task_force_radio_pipe/x64/Release/task_force_radio_pipe_x64.dll
extensions/tfar_proton_bridge/x64/Release/tfar_proton_bridge_x64.dll
```

Build the Linux TeamSpeak plugin with:

```sh
cmake -S ts -B build/tfar-linux -DTFAR_LINUX_PLUGIN=ON -DTFAR_BRIDGE_PROTOCOL_TESTS=ON
cmake --build build/tfar-linux --config Release
ctest --test-dir build/tfar-linux --output-on-failure
```

Expected artifact:

```text
build/tfar-linux/TFAR_win64_x64.so
```

## Export Checks

`task_force_radio_pipe_x64.dll` must export plain `RVExtension`.

`tfar_proton_bridge_x64.dll` must export:

```text
TFARBridge_GetApiVersion
TFARBridge_RVExtension
TFARBridge_Shutdown
```

It must not export plain `RVExtension`.

## Forced Loader Test

Place both Windows DLLs in the same directory and run Arma, Wine, or a small loader harness with:

```text
TFAR_FORCE_PROTON_BRIDGE=1
```

Expected behavior:

- If `tfar_proton_bridge_x64.dll` is present and valid, `RVExtension` calls route through `TFARBridge_RVExtension`.
- If the bridge DLL is missing or invalid, `RVExtension` returns `TFAR Proton bridge unavailable`.
- If the Linux TeamSpeak bridge server is not running, the loaded bridge keeps the existing socket result: `Not connected to TeamSpeak`.

## Disable Override Test

Run with both variables set:

```text
TFAR_DISABLE_PROTON_BRIDGE=1
TFAR_FORCE_PROTON_BRIDGE=1
```

Expected behavior:

```text
SharedMemoryTransfer is selected.
```

Disable wins over force.

## Proton Auto Test

Install these files beside each other in the TFAR mod root:

```text
task_force_radio_pipe_x64.dll
tfar_proton_bridge_x64.dll
```

Install `TFAR_win64_x64.so` into the native Linux TeamSpeak plugin directory. For Flatpak TeamSpeak, that is usually:

```text
~/.var/app/com.teamspeak.TeamSpeak3/config/ts3client/plugins/
```

Start native Linux TeamSpeak first, then Arma 3 through Proton with TFAR loaded.

Expected behavior:

- The dispatcher detects Wine plus Proton hints or Wine plus adjacent bridge DLL.
- The dispatcher loads only the adjacent `tfar_proton_bridge_x64.dll`.
- The bridge connects to the Linux plugin on `127.0.0.1:47333`.
- TeamSpeak metadata changes to connected through the normal TFAR runtime path.
- `TS_INFO	PING` returns the expected TeamSpeak response.

## Unsafe Path Test

Put a bogus `tfar_proton_bridge_x64.dll` in the process working directory but not beside `task_force_radio_pipe_x64.dll`.

Expected behavior:

```text
The bogus working-directory DLL is ignored.
```

The loader only considers the path adjacent to the dispatcher DLL.
