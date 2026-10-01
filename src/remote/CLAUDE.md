# src/remote: remote compute (qurcuma-server and wire protocol)

Plan and decisions: `docs/WP-remote-compute-vr.md`. Stage R1 (protocol, server, loopback test) is in; client (`RemoteBackend`, SSH tunnel) is stage R2.

- `protocol.*`: JSON control messages, binary frames and file uploads (little-endian, float32 positions), file policy (`applyFilePolicy`). Qt Core/Gui only.
- `serversession.*`: one authenticated WebSocket connection driving a `LocalBackend`; session directory `<root>/<stamp>/{in,out}` is the process working directory, so a server process serves one session.
- `server_main.cpp`: `qurcuma-server --port N --token T [--root D] [--once]`, listens on 127.0.0.1 only, token of at least 16 characters.
- A new curcuma parameter that names a file must be added to `uploadParamKeys()` or `blockedParamKeys()`; the server rejects other path-like strings.
- The server must call `initialize_generated_registry()` before any run (as `src/main.cpp` does), else the first optimisation throws "Parameter ... not found in module".
- `configToJson` adds `optSingleShot` to `simConfigToJson`; lessons do not store it.
- Slow link: the session drops frames above 4 MiB of pending output (latest wins); `finished` reports `framesSent`/`framesDropped`.
- Tests: `test_remote_protocol` (format, file policy), `test_remote_loopback` (server and client in one process; final geometry equals the local worker's). Built only if Qt6::WebSockets is found.
- Status: 🤖 AI-generated, ⚙️ machine-tested on one 3-atom GFN-FF optimisation and unit checks; not run across two machines, MD and GPU not covered.
