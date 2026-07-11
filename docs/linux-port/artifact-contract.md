# Linux/Proton artifact contract

The experimental Linux/Proton port uses these canonical runtime artifacts:

```text
Linux TeamSpeak plugin: TFAR_linux_amd64.so
Arma dispatcher:        task_force_radio_pipe_x64.dll
Proton socket payload:  tfar_proton_bridge_x64.dll
```

Both Windows DLLs are installed beside one another in the TFAR mod root. The
Linux TeamSpeak plugin name is live-proven and must not impersonate a Windows
plugin filename.
