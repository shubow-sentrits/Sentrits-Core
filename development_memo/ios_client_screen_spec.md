# iOS Client Screen Spec

## Goal

Freeze the v1 native screen behavior before implementation drifts into ad hoc SwiftUI views.

This spec assumes:

- modern SwiftUI
- data-driven navigation
- runtime-first behavior
- the new `UI_design` assets are the visual reference

## App Shell

Use a root `TabView` with four tabs:

1. Pairing
2. Inventory
3. Explorer
4. Activity

Focused session view should be presented from Explorer as a pushed destination or sheet, not as a separate root tab.

Preferred direction:

- `NavigationStack` at the app-shell level
- typed route enum for focused destinations
- stores injected through environment or explicit dependency container

## Pairing Screen

Design references:

- `UI_design/pairing`

### Purpose

- discover nearby hosts
- show saved / paired devices
- allow manual fallback
- show selected device identity
- start and complete pairing

### Sections

1. Active Discovery
- live discovered hosts from UDP listener
- host cards with:
  - display name
  - address
  - pair state if known
  - last seen freshness
- tap selects a host into the right-side/detail area

2. Manual Add / Verify
- hostname / IP
- port
- optional TLS toggle
- explicit verify action

3. Saved Devices
- paired and manually saved hosts
- alias + system display name
- remove / forget

4. Selected Host Detail
- host identity
- protocol / TLS
- saved token state
- version / capabilities if known

5. Pairing Request State
- code
- request status
- claim polling status
- rejection / expiry messaging

### Behavior

- discovery results should update continuously
- selecting a discovered host should not immediately persist it forever
- successful pairing promotes it into trusted/saved hosts

## Inventory Screen

Design references:

- `UI_design/inventory`

### Purpose

- browse sessions by device
- create session per device
- connect / disconnect / stop

### Structure

- top summary / lightweight metrics optional
- grouped sections by device
- compact session cards per group
- in-group add/new session affordance

### Session card content

- title
- session id
- lifecycle
- attention
- recent git/files hint
- connect state

### Actions

- connect
- disconnect
- stop
- create session from device section

### Navigation

- tapping connect can also open the session into Explorer
- tapping card body can show lightweight session detail if needed, but do not overload inventory first

## Explorer Screen

Design references:

- `UI_design/explorer`

### Purpose

- live workspace for connected sessions only

### Structure

1. group-tab strip
2. connected session tiles
3. quick group add/remove interactions

### Rules

- `All` always exists
- `All` shows all connected sessions
- additional tabs are group filters backed by session tags
- sessions may have multiple tags

### Tile content

- compact live terminal preview
- title
- small state markers
- request/release control
- disconnect
- stop if needed
- add/remove group tag affordance
- focus/expand affordance

### Critical constraint

Tile metadata must stay compact so terminal space dominates.

## Focused Session View

Design references:

- `UI_design/interactive_terminal_view`

### Purpose

- immersive single-session workspace

### Structure

1. terminal header
2. large terminal canvas
3. compact recent files / summary strip
4. mobile command/input controls

### Required actions

- request control
- release control
- stop session
- reconnect if needed
- resize updates

### Terminal notes

- this is where real terminal quality matters most
- if compact previews are degraded, focused view must still be correct

## Activity Screen

Design references:

- `UI_design/activity_log`

### Purpose

- chronological client/session activity stream

### Content

- discovery events
- pairing lifecycle
- connect / disconnect
- control changes
- session exit / error
- create / stop actions

### Character

- informational, not noisy telemetry
- light summary counts are acceptable
- individual entries should remain readable and scannable

## Navigation Guidance

Preferred:

- `TabView` for top-level surfaces
- `NavigationStack` for focused flows
- typed routes, e.g.:
  - `.hostDetail(hostID)`
  - `.focusedSession(sessionID)`
  - `.createSession(hostID)`

Avoid:

- string-driven navigation
- view-local boolean soup for routing
- deeply nested modal chains when a typed push route is clearer

## State Ownership Guidance

- Pairing tab should not own the app’s entire host store
- Inventory should consume a shared host/session model, not refetch everything itself
- Explorer should own connected-session presentation state
- Focused view should read from Explorer/session stores, not rebuild connection logic from scratch

## Build Sequence

1. Pairing shell + host state
2. Inventory shell + grouped cards
3. Explorer shell + connected sessions
4. Focused session view
5. Activity stream
6. terminal renderer hardening
