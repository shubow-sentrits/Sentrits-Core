# iOS Client Agent Prompts

These prompts are branch-ready and assume modern SwiftUI architecture.

## Agent 1: Discovery + Pairing

```text
Work in: /Users/shubow/dev/VibeEverywhereIOS

Goal:
Implement the new Pairing tab and active discovery flow using modern SwiftUI patterns.

Required style:
- use `NavigationStack` and typed navigation where needed
- prefer store-driven / data-driven view composition over view-local imperative flow
- use async/await
- avoid legacy MVVM boilerplate if a cleaner observable store is more appropriate

Reference:
- UI: `UI_design/pairing`
- design system: `UI_design/vibeops_atmospheric/DESIGN.md`
- runtime plan: `/Users/shubow/dev/VibeEverywhere/development _memo/ios_client_rebuild_plan.md`
- Moonlight reference for discovery/pairing behavior only:
  - `/Users/shubow/dev/moonlight-ios/Limelight/Network/DiscoveryManager.m`
  - `/Users/shubow/dev/moonlight-ios/Limelight/Network/DiscoveryWorker.m`
  - `/Users/shubow/dev/moonlight-ios/Limelight/Network/MDNSManager.m`
  - `/Users/shubow/dev/moonlight-ios/Limelight/Network/PairManager.m`

Requested work:
1. Build true UDP discovery on iOS for runtime broadcasts on `18087`.
2. Maintain discovered-host state with TTL / last-seen semantics.
3. Merge discovered hosts with saved/paired hosts.
4. Build the Pairing tab UI in the new visual direction.
5. Support:
   - select discovered host
   - manual host add fallback
   - pairing request
   - pairing claim polling
   - token persistence
6. Add tests where practical for discovery decoding and host merge logic.

Non-goals:
- Inventory
- Explorer
- full terminal renderer

Verification:
- build
- run tests

Final response:
- summarize changes
- list files changed
- report verification
```

## Agent 2: Inventory

```text
Work in: /Users/shubow/dev/VibeEverywhereIOS

Goal:
Implement the Inventory tab using the new grouped-by-device session model.

Required style:
- modern SwiftUI
- data-driven navigation
- compact card hierarchy

Reference:
- UI: `UI_design/inventory`
- runtime plan: `/Users/shubow/dev/VibeEverywhere/development _memo/ios_client_screen_spec.md`

Requested work:
1. Build grouped device sections.
2. Show compact session cards with:
   - title
   - session id
   - lifecycle
   - attention
   - small git/files hint
   - connect state
3. Add:
   - per-device create session
   - connect
   - disconnect
   - stop
4. Keep inventory driven by shared host/session state, not one-off fetches.
5. Add tests for grouping and mapping logic where practical.

Non-goals:
- Explorer
- focused session view

Verification:
- build
- run tests

Final response:
- summarize changes
- list files changed
- report verification
```

## Agent 3: Explorer + Focused View

```text
Work in: /Users/shubow/dev/VibeEverywhereIOS

Goal:
Implement the Explorer tab and focused session view with modern SwiftUI navigation.

Required style:
- `NavigationStack` with typed routes
- compact explorer tiles, detailed focused destination
- group/tag views are data-driven

Reference:
- UI: `UI_design/explorer`
- UI: `UI_design/interactive_terminal_view`
- screen spec: `/Users/shubow/dev/VibeEverywhere/development _memo/ios_client_screen_spec.md`

Requested work:
1. Explorer `All` shows connected sessions only.
2. Add local group tabs based on session tags.
3. Add tag add/remove actions through runtime group-tag API.
4. Render compact session tiles with terminal previews.
5. Add focused session destination with:
   - larger terminal
   - request/release control
   - stop session
   - summary / recent files area
6. Keep state centralized; avoid rebuilding websocket logic per random subview.

Important:
- `All` is permanent
- focused destination is where the heavy details belong

Verification:
- build
- run tests

Final response:
- summarize changes
- list files changed
- report verification
```

## Agent 4: Terminal Renderer

```text
Work in: /Users/shubow/dev/VibeEverywhereIOS

Goal:
Replace the current lossy terminal rendering path with a real terminal solution.

Required style:
- isolate renderer behind an adapter
- keep the app’s session/explorer logic independent of renderer details

Reference:
- focused terminal design in `UI_design/interactive_terminal_view`
- current runtime PTY behavior already proven in the web client/runtime-served client

Requested work:
1. Evaluate and integrate SwiftTerm or equivalent.
2. Preserve:
   - websocket output
   - input
   - resize
   - reconnect seed path
3. Make focused view correct first.
4. Allow compact explorer previews to remain lighter-weight if needed.
5. Add tests for adapter logic where practical.

Verification:
- build
- run tests

Final response:
- summarize renderer choice and tradeoffs
- list files changed
- report verification
```

## Agent 5: Activity + Polish

```text
Work in: /Users/shubow/dev/VibeEverywhereIOS

Goal:
Implement the Activity tab and bring the app shell into the new atmospheric visual system.

Reference:
- UI: `UI_design/activity_log`
- design system: `UI_design/vibeops_atmospheric/DESIGN.md`

Requested work:
1. Build the Activity tab with a bounded client/session event log.
2. Capture discovery, pairing, socket, control, exit, and error events.
3. Apply the visual language consistently across:
   - tab shell
   - cards
   - surfaces
   - spacing
4. Keep the UI premium and calm, not telemetry-heavy.

Verification:
- build
- run tests

Final response:
- summarize changes
- list files changed
- report verification
```
