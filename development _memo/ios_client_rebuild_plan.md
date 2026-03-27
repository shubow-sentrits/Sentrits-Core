# iOS Client Rebuild Plan

## Purpose

This memo translates the new iOS UI design in `/Users/shubow/dev/VibeEverywhereIOS/UI_design` into an implementation-ready plan aligned with the current `vibe-hostd` runtime.

The current iOS project is not a clean base for the new product shape. It contains useful networking and persistence fragments, but the UI and flow are still from an older MVP.

The intended result is not a patch-up. It is a controlled rebuild inside the existing iOS repo using the new design direction and the current runtime contract.

## Runtime Reality

The runtime already provides the important remote-client surface:

- `GET /discovery/info`
- `GET /host/info`
- `POST /pairing/request`
- `POST /pairing/claim`
- `GET /sessions`
- `POST /sessions`
- `GET /sessions/{id}`
- `GET /sessions/{id}/snapshot`
- `GET /sessions/{id}/file`
- `GET /sessions/{id}/tail`
- `POST /sessions/{id}/groups`
- `POST /sessions/{id}/stop`
- `POST /sessions/clear-inactive`
- WebSocket `GET /ws/overview`
- WebSocket `GET /ws/sessions/{id}`

The runtime also broadcasts UDP discovery on port `18087`.

That means the iOS client can implement true active discovery directly, without the browser helper used by the web client.

## New UI Design Reading

The new design system is "Atmospheric Control" / "The Silent Navigator":

- warm dark surfaces
- tonal layering instead of hard dividers
- premium editorial typography
- calm, tactile control rather than hacker-terminal styling

The primary app surfaces are:

1. Pairing
2. Inventory
3. Explorer
4. Activity
5. Focused terminal session view

This aligns strongly with the current product model:

- Pairing = host discovery, trust, saved devices
- Inventory = session cards grouped by device
- Explorer = connected sessions only, grouped by tags
- Activity = lightweight event/log stream
- Focused view = larger terminal + details

## Assessment Of Current iOS Repo

Useful existing pieces:

- `HostClient.swift`
- `SessionSocket.swift`
- `SavedHostsStore.swift`
- `KeychainTokenStore.swift`

These are reusable as foundations, but need API and behavior cleanup.

Current code that is not aligned with the new design:

- `ConnectView.swift`
- `PairingView.swift`
- `SessionsView.swift`
- `SessionDetailView.swift`
- most current view models

Main mismatches:

- form-heavy admin-style flow
- no real device-grouped inventory
- no connected-session explorer model
- no tag/group workflow
- no atmospheric design implementation
- terminal rendering is still lossy text stripping

## Product Scope For iOS v1

### Must have

1. Active Discovery
- listen for UDP discovery on `18087`
- maintain discovered hosts list
- dedupe by `hostId`
- allow manual host add as fallback

2. Pairing
- request pairing from selected host
- poll/claim pairing approval
- persist tokens securely
- show trusted host identity clearly

3. Inventory
- show sessions grouped by device
- compact session cards
- create session per device
- connect / disconnect / stop from inventory

4. Explorer
- show connected sessions only in `All`
- allow local group tabs
- assign/remove group tags through runtime API
- compact live tiles

5. Focused Session View
- larger terminal
- session metadata
- files/snapshot summary
- control request/release
- stop session

6. Activity
- local client log
- session/socket/pairing activity trace

### Explicit deferrals

- full file tree browsing
- editing files
- watch notifications / push notifications
- provider-specific phase inference beyond current runtime payload
- advanced settings/config management

## Architecture Recommendation

## UI stack

- SwiftUI for all screens
- modern data-driven navigation via `NavigationStack` / typed destinations
- root tab shell via `TabView`
- modal / sheet for focused session view and create-session forms
- prefer modern SwiftUI state patterns:
  - `@Observable` for shared stores where practical
  - `@State` / `@Bindable` at view edges
  - `async/await` for all network flows
  - task-driven refresh (`.task`, `.refreshable`, `task(id:)`)

## State

Use a clear app store split rather than screen-local networking:

- `HostsStore`
  - discovered hosts
  - saved hosts
  - paired hosts
  - selected host

- `InventoryStore`
  - per-host sessions
  - merged session inventory
  - session sorting / filtering

- `ExplorerStore`
  - connected sessions
  - local group tabs
  - focused session
  - socket lifecycles

- `ActivityStore`
  - bounded client event log

## Networking

- keep `HostClient` as REST client, but expand it
- keep `SessionSocket` as per-session WS client
- add `OverviewSocket` or a multi-host overview manager for `/ws/overview`
- add `DiscoveryListener` using UDP sockets on iOS
- use `/Users/shubow/dev/moonlight-ios` as reference for discovery/pairing patterns only
  - useful reference areas:
    - `DiscoveryManager`
    - `DiscoveryWorker`
    - `MDNSManager`
    - `PairManager`
- do not copy Moonlight’s product model or legacy Objective-C structure directly
- use it as a network-behavior reference, not as an architecture template

## Persistence

- keep tokens in Keychain
- keep saved hosts and local aliases in app storage
- do not persist transient discovered-only hosts forever unless user saves/pairs them

## Terminal

The current `TerminalEngine` is not sufficient for the new app.

For v1, choose one of these paths explicitly:

### Preferred

Use a real terminal renderer such as SwiftTerm or equivalent terminal emulation layer.

Reason:

- PTY output is already proven to need real escape-sequence handling
- the web client and old runtime-served client both showed that lossy text mode is not good enough

### Acceptable fallback

Keep the current text-mode terminal only for an early scaffold branch, but do not call it production-ready.

Recommendation: treat terminal rendering as a first-class track, not an afterthought.

## Screen Mapping

### Pairing tab

Purpose:

- show active discovery
- show saved/trusted hosts
- start pairing
- inspect selected host identity

Sections:

- Active discovery results
- Manual add / verify fallback
- Saved devices
- Selected host details
- Pairing request status

### Inventory tab

Purpose:

- browse sessions grouped by device
- create session per device
- connect / disconnect / stop

Card essentials:

- title
- session id
- lifecycle
- attention
- small git/files hint
- connect state

### Explorer tab

Purpose:

- workspace for connected sessions only

Rules:

- `All` always shows every connected session
- extra tabs are group filters backed by session tags
- local create-group flow
- easy add/remove tag interactions

### Activity tab

Purpose:

- restore the old runtime-client style log stream
- keep lightweight and chronological

### Focused session view

Purpose:

- immersive terminal
- larger control surface
- summary/details moved out of compact explorer tiles

## Suggested Build Order

1. Data model + runtime contract cleanup
2. Active discovery + host store
3. Pairing flow
4. Inventory screen
5. Explorer shell with connected sessions
6. Focused session view
7. Real terminal renderer integration
8. Activity tab
9. Polish and smoke testing

## Testing Strategy

### Unit / integration

- host normalization / dedupe
- discovery packet decoding
- pairing polling state
- session inventory merge/group logic
- group tag mutations

### UI tests

- discovery to pairing flow
- create session from inventory
- connect session into explorer
- open focused view
- stop session from inventory and focused view

### Manual smoke

- real UDP discovery on LAN
- pairing approval against host admin
- multi-session explorer
- terminal control handoff

## Recommended Parallel Split

### Track A: Foundation + Models

- new app stores
- normalized models
- host/session mapping

### Track B: Discovery + Pairing

- UDP listener
- saved/discovered host merge
- pairing and token persistence

### Track C: Inventory + Session Actions

- grouped inventory
- create/connect/disconnect/stop

### Track D: Explorer + Focused Session

- connected-session workspace
- group tabs
- focused view shell

### Track E: Terminal Renderer

- SwiftTerm or equivalent integration
- input / resize / output correctness

### Track F: Activity + Polish

- client event log
- copy refinements
- motion/spacing polish

## Bottom Line

The iOS client should be treated as a new first-class native client, not as a salvage pass on the old MVP UI.

The runtime is already mature enough to support it:

- real discovery
- real pairing claim flow
- session inventory
- group tags
- control semantics
- administrative stop

The main technical risk is terminal rendering quality, so that should be isolated early and handled explicitly.
