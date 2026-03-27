# iOS Client Build Prompts

These prompts assume work in:

- `/Users/shubow/dev/VibeEverywhereIOS`

They are written to support parallel tracks against the current runtime.

## Prompt 1: Foundation / Model Rewrite

```text
Work in: /Users/shubow/dev/VibeEverywhereIOS

Goal:
Prepare the iOS app foundation for the new UI and runtime contract. Do not preserve the old form-based screen structure unless it directly helps the new design.

Context:
- The new UI design lives under `UI_design/`
- The current runtime already supports:
  - discovery info
  - pairing request + claim
  - sessions list/create/stop
  - snapshot/file/tail
  - session group tags
  - overview/session websockets
- Existing repo code has some reusable pieces:
  - `HostClient.swift`
  - `SessionSocket.swift`
  - `SavedHostsStore.swift`
  - `KeychainTokenStore.swift`
- Existing screen/view-model code is mostly old MVP scaffolding and not the target shape

Requested work:
1. Define clean native models for:
   - discovered host
   - saved/paired host
   - session summary
   - session snapshot
   - explorer tile/session tab state
   - activity log entry
2. Refactor networking models to match current runtime payloads.
3. Introduce a clearer store structure for:
   - hosts
   - inventory
   - explorer
   - activity
4. Keep code modular and ready for the new tabs:
   - Pairing
   - Inventory
   - Explorer
   - Activity
5. Add unit tests where practical for model mapping and store logic.

Non-goals:
- final UI polish
- UDP discovery implementation
- full terminal renderer integration

Verification:
- build the app
- run unit tests

Final response:
- summarize architecture changes
- list files changed
- report verification
```

## Prompt 2: Active Discovery + Pairing

```text
Work in: /Users/shubow/dev/VibeEverywhereIOS

Goal:
Implement true active discovery and pairing flow for the iOS client using the current runtime.

Context:
- Runtime host broadcasts UDP discovery on port `18087`
- Runtime also exposes `GET /discovery/info`
- Runtime pairing flow supports:
  - `POST /pairing/request`
  - `POST /pairing/claim`
- iOS can receive UDP directly, unlike the web client

Requested work:
1. Add a UDP discovery listener for the runtime broadcast payload.
2. Maintain a discovered-host list with TTL/last-seen behavior.
3. Merge discovered hosts with saved/paired hosts cleanly.
4. Dedupe by `hostId` when available.
5. Keep manual host add as fallback.
6. Implement pairing UI/state flow aligned with the new design:
   - select discovered host
   - request pairing
   - show approval code
   - poll claim until approved/rejected/expired
   - persist token securely
7. Add tests where practical for:
   - discovery payload decoding
   - dedupe/merge logic
   - pairing state transitions

Important:
- do not regress manual fallback
- do not fake discovery through manual verify

Verification:
- build
- run tests

Final response:
- summarize what discovery flow is now supported
- list files changed
- report verification
```

## Prompt 3: Inventory Screen

```text
Work in: /Users/shubow/dev/VibeEverywhereIOS

Goal:
Build the new native Inventory tab using the `UI_design/inventory` direction and the current runtime session model.

Requested work:
1. Implement a device-grouped inventory screen.
2. Show compact session cards grouped by host/device.
3. Add per-device create-session action.
4. Add per-session:
   - connect
   - disconnect
   - stop
5. Keep card info compact:
   - title
   - session id
   - lifecycle
   - attention
   - small git/files hint
6. Preserve the atmospheric visual language from the design docs.
7. Add tests for inventory grouping/filtering logic where practical.

Non-goals:
- full explorer workspace
- full focused session view

Verification:
- build
- run tests

Final response:
- summarize UI and runtime behavior added
- list files changed
- report verification
```

## Prompt 4: Explorer + Focused Session

```text
Work in: /Users/shubow/dev/VibeEverywhereIOS

Goal:
Implement the Explorer tab and focused session view aligned with `UI_design/explorer` and `UI_design/interactive_terminal_view`.

Requested work:
1. Explorer `All` view shows connected sessions only.
2. Add local group tabs.
3. Group tabs filter connected sessions by runtime `groupTags`.
4. Allow add/remove group tags for sessions through runtime API.
5. Render compact session tiles with small live terminal previews.
6. Add a focused session view with:
   - larger terminal
   - request/release control
   - stop session
   - session detail summary
   - recent file or snapshot section
7. Keep compact tiles light and give terminal space priority.

Important:
- `All` always shows all connected sessions
- focused session view is the place for heavier detail, not the compact tile

Verification:
- build
- run tests

Final response:
- summarize explorer/focused-view behavior
- list files changed
- report verification
```

## Prompt 5: Terminal Renderer

```text
Work in: /Users/shubow/dev/VibeEverywhereIOS

Goal:
Replace the current lossy text terminal rendering with a proper terminal rendering approach suitable for the new app.

Context:
- Current `TerminalEngine` strips ANSI and is not good enough
- Runtime websocket output is real PTY data
- Terminal correctness matters for focused session view

Requested work:
1. Integrate a real terminal rendering solution (prefer SwiftTerm or equivalent).
2. Keep session socket input/output/resize aligned with current runtime contract.
3. Support:
   - output rendering
   - typed input
   - resize updates
   - reconnect seed from snapshot/tail if needed
4. Keep the implementation modular so compact preview tiles can reuse a lighter presentation.
5. Add tests around any terminal adapter logic that is practical.

Non-goals:
- custom keyboard design beyond what is needed for a working MVP

Verification:
- build
- run tests
- describe any remaining limitations honestly

Final response:
- summarize renderer choice and tradeoffs
- list files changed
- report verification
```

## Prompt 6: Activity + Final Polish

```text
Work in: /Users/shubow/dev/VibeEverywhereIOS

Goal:
Add the Activity tab and align the app with the atmospheric mobile design system.

Requested work:
1. Implement the Activity tab as a lightweight chronological client/session event log.
2. Capture:
   - discovery events
   - pairing events
   - session connection events
   - control changes
   - session exit/error events
3. Apply the atmospheric design language consistently:
   - warm dark surfaces
   - no hard divider dependence
   - calm editorial spacing
   - compact but premium cards
4. Polish tab shell and transitions.
5. Add UI tests for critical navigation flows where practical.

Verification:
- build
- run tests

Final response:
- summarize activity/polish changes
- list files changed
- report verification
```
