# Linux Build Notes

## Protocol Smoke Test

```sh
cmake -S ts -B build/tfar-linux -DTFAR_BRIDGE_PROTOCOL_TESTS=ON
cmake --build build/tfar-linux
ctest --test-dir build/tfar-linux --output-on-failure
```

## Experimental TeamSpeak Plugin Skeleton

```sh
cmake -S ts -B build/tfar-linux -DTFAR_LINUX_PLUGIN=ON
cmake --build build/tfar-linux --target TFAR_linux_x64
```

Expected artifact:

```text
build/tfar-linux/TFAR_linux_x64.so
```

This target is a Linux bridge skeleton for native load/shutdown and bridge protocol work. The full TFAR runtime still needs Linux portability work in the wider audio/UI/plugin source before this can be called playable.

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
