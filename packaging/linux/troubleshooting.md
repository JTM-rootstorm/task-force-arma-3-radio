# TFAR Linux Bridge Alpha Troubleshooting

See `docs/linux-port/troubleshooting.md` in the source tree for the full checklist.

Start with these checks:

- Native Linux TeamSpeak loads `TFAR_win64_linux_amd64.so`.
- Stale `TFAR_linux_x64.so` and `TFAR_win64_x64.so` copies are removed or disabled.
- Arma under Proton loads the bridge-built `task_force_radio_pipe_x64.dll`.
- Both sides use the same bridge port and token.
- No other process is using `127.0.0.1:47333`.
- TeamSpeak is restarted after plugin installation.
