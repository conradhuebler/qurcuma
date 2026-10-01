# src/remote: remote compute (qurcuma-server and wire protocol)

Plan and decisions: `docs/WP-remote-compute-vr.md`. Stages R1 (protocol, server), R2 (client, ssh tunnel, dock selection) and R3 (connection loss, capabilities) are in.

- `protocol.*`: JSON control messages, binary frames and file uploads (little-endian, float32 positions), file policy (`applyFilePolicy`). Qt Core/Gui only.
- `serversession.*`: one authenticated WebSocket connection driving a `LocalBackend`; session directory `<root>/<stamp>/{in,out}` is the process working directory, so a server process serves one session.
- `server_main.cpp`: `qurcuma-server --port N --token T [--root D] [--once]`, listens on 127.0.0.1 only, token of at least 16 characters.
- Connection loss: the server keeps a run going for `--grace` seconds (default 60) with the grab force cleared; the client reconnects (hello with `attach` and the session name, restarting the ssh forward if needed) for 60 s. A run that ends meanwhile hands its `finished` message to the returning client. After the grace period the run is stopped and the session ends. `RemoteServer` holds the one-session policy.
- The server ignores SIGHUP/SIGPIPE so the run survives the ssh session that started it. A connection that never authenticates does not end the session (it idles out after 60 s).
- `welcome` carries `gpuBackends` (compile-time of the server); the dock stores them per host (`remote/gpu/<host>`) and shows them in the GPU combo, all three until the host was connected once.
- A new curcuma parameter that names a file must be added to `uploadParamKeys()` or `blockedParamKeys()`; the server rejects other path-like strings.
- The server must call `initialize_generated_registry()` before any run (as `src/main.cpp` does), else the first optimisation throws "Parameter ... not found in module".
- `configToJson` adds `optSingleShot` to `simConfigToJson`; lessons do not store it.
- Slow link: the session drops frames above 4 MiB of pending output (latest wins); `finished` reports `framesSent`/`framesDropped`.
- `remotebackend.*`: `SimulationBackend` over a WebSocket; reads files referenced by the config on this machine and uploads them, writes the trajectory (`writeTrajectory`) from the received frames, no fallback to a local run. `sshtunnel.*`: starts `qurcuma-server --port 0 --token-stdin --once` over `ssh` (BatchMode, token via stdin) and forwards a local port; `QURCUMA_SSH` replaces the ssh binary (tests).
- The host list of the "Compute on" combo (Simulation dock, `QURCUMA_REMOTE`) comes from `~/.ssh/config`; the choice and the server command per host are stored in QSettings (`remote/...`).
- Tests: `test_remote_protocol` (format, file policy), `test_remote_loopback` (server and client in one process; final geometry equals the local worker's; RemoteBackend, uploads, trajectory, failure without fallback), `test_remote_ssh` (RemoteBackend through SshTunnel with a python stand-in for ssh), `test_remote_reconnect` (drop during MD and return, no return within the grace period, run ended while away). Built only if Qt6::WebSockets is found.
- Status: 🤖 AI-generated, ⚙️ machine-tested on one 3-atom GFN-FF optimisation and unit checks; not run with a real ssh or across two machines, MD, GPU and the dock UI not covered.
