# Linux Build Notes

## Protocol Smoke Test

```sh
cmake -S ts -B build/tfar-linux -DTFAR_BRIDGE_PROTOCOL_TESTS=ON
cmake --build build/tfar-linux
ctest --test-dir build/tfar-linux --output-on-failure
```

## Experimental TeamSpeak Plugin

```sh
cmake -S ts -B build/tfar-linux -DTFAR_LINUX_PLUGIN=ON
cmake --build build/tfar-linux --target TFAR_linux_x64
```

Expected artifact:

```text
build/tfar-linux/TFAR_linux_x64.so
```

This target builds a native Linux TeamSpeak plugin with the TFAR command processor, audio/radio runtime, Linux bridge server, Clunk, DSP filters, and SQLite linked into `TFAR_linux_x64.so`.

Before installing or packaging the artifact, verify that it loads and has no unresolved plugin-owned symbols:

```sh
python3 -c 'import ctypes; lib=ctypes.CDLL("build/tfar-linux/TFAR_linux_x64.so"); lib.ts3plugin_apiVersion.restype=ctypes.c_int; print(lib.ts3plugin_apiVersion())'
ldd -r build/tfar-linux/TFAR_linux_x64.so
```

The supported Linux TeamSpeak baseline is TeamSpeak 3 Client 3.6.2. It expects plugin API 26, so `ts3plugin_apiVersion()` must return 26 for that client line.

## Proton Bridge DLL

Build `extensions/task_force_radio_pipe/task_force_radio_pipe.vcxproj` as usual, but add this preprocessor definition for the Proton bridge variant:

```text
TFAR_USE_SOCKET_BRIDGE
```

Without that definition, the DLL keeps the native Windows shared-memory path.

Optional bridge settings for Proton:

```text
TFAR_BRIDGE_HOST=127.0.0.1
TFAR_BRIDGE_PORT=47333
TFAR_BRIDGE_TOKEN=<token>
```
