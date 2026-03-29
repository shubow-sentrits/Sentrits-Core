# VibeEverywhere

VibeEverywhere is a remote session runtime and control plane for AI coding CLIs.

The runtime is implemented in C++20 with CMake, Ninja, and Clang/LLVM.

## Current Runtime Shape

- macOS and Linux only
- PTY-backed session execution currently uses a shared POSIX `forkpty` backend behind an `IPtyProcess` factory seam
- one PTY per session
- many observers, one controller
- host-local `session-start` and `session-attach` use a privileged low-latency local controller stream
- remote web and mobile clients use the HTTP and WebSocket observer/control API, including the dedicated remote controller WebSocket
- file watching, process-tree inspection, and resource monitoring remain partially implemented seams rather than fully complete platform subsystems

## Repository Surfaces

- `vibe-hostd`
  - runtime daemon, host-local admin/API surface, pairing, session management
- `frontend/`
  - maintained in-repo host-admin workspace
- `/Users/shubow/dev/VibeEverywhere-Client`
  - maintained browser remote client
- `/Users/shubow/dev/VibeEverywhereIOS`
  - maintained iOS client
- `deprecated/web/`
  - legacy daemon-served plain HTML host and remote browser UIs kept for compatibility

## Start Here

- [VIBING.md](/Users/shubow/dev/VibeEverywhere/VIBING.md)
- [build_and_test.md](/Users/shubow/dev/VibeEverywhere/development_memo/build_and_test.md)
- [architecture_refined.md](/Users/shubow/dev/VibeEverywhere/development_memo/architecture_refined.md)
- [api_and_event_schema.md](/Users/shubow/dev/VibeEverywhere/development_memo/api_and_event_schema.md)
- [packaging_architecture.md](/Users/shubow/dev/VibeEverywhere/development_memo/packaging_architecture.md)
- [development_memo/README.md](/Users/shubow/dev/VibeEverywhere/development_memo/README.md)
