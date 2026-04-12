# Tracing And Debugging

This document is the current maintainer reference for runtime tracing in `Sentrits-Core`.

Use code as the source of truth. Update this file when trace scope or debugging workflow changes.

## Runtime Trace Gate

Current debug traces are gated in [`include/vibe/base/debug_trace.h`](/Users/shubow/dev/Sentrits-Core/include/vibe/base/debug_trace.h).

Behavior:

- traces are compiled out when `NDEBUG` is defined
- in non-`Release` builds, traces are emitted only when `SENTRITS_DEBUG_TRACE` is enabled
- traces go to `stderr`

Enable tracing for a local debug run:

```bash
SENTRITS_DEBUG_TRACE=1 ./build/sentrits serve
```

Disable explicitly:

```bash
SENTRITS_DEBUG_TRACE=0 ./build/sentrits serve
```

Important build note:

- `Debug` builds keep trace code available
- `Release`, `RelWithDebInfo`, and `MinSizeRel` typically define `NDEBUG`, so these traces are compiled out

## Trace Format

Current trace lines look like:

```text
[sentrits-debug][1776035979522][core.focus][send_input.escape] session=s_1 bytes=24 text="..." hex=...
```

Fields:

- timestamp in epoch milliseconds
- scope
- event
- event-specific detail payload

## Current Trace Scopes

The main live scopes today are:

- `core.node`
  - session summary transitions and coarse runtime state changes
- `core.focus`
  - snapshot requests/responses
  - viewport updates
  - local attach/control handoff
  - controller input filtering
- `core.terminal`
  - PTY append summaries
  - screen/bootstrap summaries
  - row-style inspection around the cursor

Representative events:

- `core.node.summary.transition`
- `core.focus.snapshot.request`
- `core.focus.snapshot.response`
- `core.focus.viewport.update`
- `core.focus.local.handshake`
- `core.focus.local.control.lost`
- `core.focus.local.viewport.flush`
- `core.focus.send_input.escape`
- `core.focus.send_input.filtered`
- `core.terminal.append.escape`
- `core.terminal.append.state`
- `core.terminal.screen.bootstrap`
- `core.terminal.viewport.bootstrap`
- `core.terminal.screen.rows`
- `core.terminal.viewport.rows`

## Common Debug Workflows

### Inspect Session State While Reproducing

Start the daemon with traces enabled:

```bash
SENTRITS_DEBUG_TRACE=1 ./build/sentrits serve
```

Useful CLI checks in another terminal:

```bash
./build/sentrits host status
./build/sentrits session list
./build/sentrits session show s_1
```

### Trace Attach / Control Handoff Issues

Use a `Debug` build, then reproduce with tracing enabled:

```bash
SENTRITS_DEBUG_TRACE=1 ./build/sentrits serve
```

Watch for:

- `core.focus.local.handshake`
- `core.focus.local.bootstrap`
- `core.focus.local.control.lost`
- `core.focus.local.control.reacquired`
- `core.focus.local.viewport.flush`
- `core.focus.send_input.escape`
- `core.focus.send_input.filtered`

This is the right trace set for:

- local attach ghosting
- observer/control transitions
- controller-specific terminal capability replies
- shared terminal appearance changing after control handoff

### Trace Snapshot / Bootstrap Problems

Watch for:

- `core.focus.snapshot.request`
- `core.focus.snapshot.response`
- `core.focus.viewport.update`
- `core.terminal.screen.bootstrap`
- `core.terminal.viewport.bootstrap`
- `core.terminal.screen.rows`
- `core.terminal.viewport.rows`

This is the right trace set for:

- wrong first frame after connect
- duplicate bottom rows
- missing visible rows
- color/style differences between raw PTY output and snapshot bootstrap

### Interpret Controller Input Replies

When debugging terminal-compatibility issues, `core.focus.send_input.escape` is the most useful event.

Examples:

- DSR cursor position reply:
  - `\e[4;3R`
- device attributes reply:
  - `\e[?1;2c`
- focus in/out:
  - `\e[I`
  - `\e[O`
- terminal color reports:
  - `\e]10;rgb:...`
  - `\e]11;rgb:...`

Current behavior:

- `OSC 10` and `OSC 11` controller replies are filtered before they reach the PTY
- filtering is logged as `core.focus.send_input.filtered`

That filter exists because controller-specific terminal background/foreground reports were mutating the shared session prompt styling across clients.

### Row-Level Terminal Inspection

`core.terminal.screen.rows` and `core.terminal.viewport.rows` summarize rows around the cursor.

They are useful for checking:

- the visible text near the prompt/cursor
- whether non-default foreground/background colors are already present in the host screen model
- whether a suspected style leak was introduced by the PTY/app or by bootstrap emission

Example shape:

```text
[sentrits-debug][...][core.terminal][screen.rows] row=12 text="› Explain this codebase" nonDefaultFg=false nonDefaultBg=true bg={rgb:83,134,185}
```

Interpretation:

- if the bad style already appears in `screen.rows`, the host terminal state already contains it
- if it only appears after bootstrap emission, the snapshot/bootstrap path is suspect

### Test Before And After Trace Changes

At minimum, run the affected focused test subset after adding or changing trace-driven behavior.

Common examples:

```bash
ctest --test-dir build --output-on-failure -R 'SessionManagerTest|SessionRuntimeTest'
ctest --test-dir build --output-on-failure -R 'TerminalMultiplexerTest|SessionSnapshotTest|HttpJsonTest'
```

If local attach behavior changed, also smoke:

1. start a live session locally
2. attach from a host terminal
3. observe from a second client
4. request/release control
5. resize both views

### Capture Guidance

When collecting logs for a bug:

- keep one trace capture from the host daemon
- record the exact client action order
- include the terminal app or client type used
- note window sizes if resizing is involved
- prefer short reproductions over long noisy sessions

For client-related issues, keep the host trace authoritative and align it with:

- web browser console/network logs in `Sentrits-Web`
- Xcode console logs in `Sentrits-IOS`

## Scope Boundary

This document only covers current `Sentrits-Core` tracing.

Client-specific trace points belong in the client repos:

- Web: `Sentrits-Web`
- iOS: `Sentrits-IOS`
