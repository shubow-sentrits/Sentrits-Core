# Sentrits-Hub Architecture Contract

## Purpose

This document defines the non-negotiable architecture boundary between
`Sentrits-Core` and `Sentrits-Hub`.

It is intended to prevent scope drift and avoid accidentally moving session
authority, replay ownership, or trust decisions into the Hub.

## Product Model

Sentrits is primarily a mobile-first, notification-driven developer workflow.

The core loop is:

- a long-running session runs on the host
- the host determines that attention is needed
- Hub delivers a push notification
- the user opens mobile
- the user inspects session state and replay
- the user makes a decision or briefly takes control
- the user disconnects
- the session continues on the host

Desktop complexity may come later.
Web should remain minimal in MVP.

## Invariants

These are hard rules.

### Session authority

- `Sentrits-Core` is the only authority for session lifecycle
- `Sentrits-Core` is the only authority for session state
- `Sentrits-Core` is the only authority for replay history
- `Sentrits-Core` is the only authority for controller ownership
- `Sentrits-Core` is the only authority for trusted-device access decisions

### Session persistence

- Sessions must persist independently of active client connections
- Each session must have a stable host-owned `session_id`
- Reconnecting to a session must resume the existing session, not create a new one
- Replay must remain host-owned and sequence-based

### Trust

- Hub may authenticate user accounts
- Hub may associate devices with accounts
- Host alone authenticates session participants
- Hub must never authenticate a device *into a host session* on the host’s behalf
- Host alone decides whether a device is trusted
- Host alone decides whether a client is an observer or controller

### Cloud boundary

- Hub may route devices
- Hub may exchange rendezvous metadata
- Hub may relay encrypted or opaque session traffic
- Hub must not own session state
- Hub must not enforce session permissions
- Hub must not inspect or store session payload
- Hub must not act as an observer or controller

### Privacy

- Terminal output must not be stored in Hub
- Terminal input must not be stored in Hub
- Replay history must not be stored in Hub
- Hub may store only minimal metadata required for routing, presence, and notifications

### Product surface

- Mobile is the primary intervention surface
- Mobile is for interruption, inspection, and decision
- Mobile is not the primary orchestration surface
- Web is a control-plane surface in MVP, not a full control surface
- Desktop complexity is explicitly deferred

## Non-Goals

These are not MVP goals.

- full browser terminal parity
- rich web replay UI
- multi-session orchestration in web
- heavy desktop orchestration before mobile intervention loop works
- moving session supervision or replay logic into Hub
- moving trusted-device policy into Hub
- turning Hub into a second authority for session access
- redesigning `Sentrits-Core` session semantics before Hub exists

## Sentrits-Core Responsibilities

`Sentrits-Core` owns:

- process/session execution
- session persistence
- replay buffer and sequence model
- recovered session loading
- session snapshots and output slices
- controller ownership and transfer rules
- trusted-device allowlist
- pairing acceptance/rejection
- attention and supervision state
- terminal payload handling

`Sentrits-Core` may expose:

- session inventory
- attention metadata
- minimal notification-worthy signals
- attach / replay / control surfaces over transport adapters

`Sentrits-Core` should only be changed when necessary to:

- support Hub-mediated transport
- export minimal notification signals
- support end-to-end host/device authentication over new transport paths

`Sentrits-Core` should not be changed merely to make Hub easier to build.

## Sentrits-Hub Responsibilities

`Sentrits-Hub` owns:

- account authentication
- device registration
- host registration
- presence / heartbeat tracking
- host discovery for user devices
- rendezvous candidate exchange
- relay allocation and transport forwarding
- push notification fanout

`Sentrits-Hub` may store:

- account ids
- device ids
- host ids
- presence timestamps
- relay allocation metadata
- rendezvous session metadata
- minimal session notification metadata

Minimal notification metadata means:

- host id
- session id
- session title
- coarse lifecycle state
- coarse attention state / reason

`Sentrits-Hub` must not store:

- terminal output
- terminal input
- replay history
- controller state as source of truth
- attach authorization state as source of truth

## Mobile Responsibilities

Mobile MVP responsibilities:

- receive push notifications
- list user-visible hosts
- show lightweight session inventory
- inspect a session’s replay and current state
- request control briefly when needed
- release control and disconnect cleanly

Mobile must optimize for:

- interruption
- decision
- quick intervention

Mobile is explicitly not the place for:

- multi-session orchestration
- long-form terminal-heavy workflows
- large editing flows

## Web Responsibilities

Web MVP responsibilities:

- user authentication
- device listing
- host listing
- host online/offline state
- session inventory list
- optional basic attach entry point

Web MVP must avoid:

- full terminal UI
- rich replay UI
- orchestration UI
- becoming the primary interaction surface

## Desktop Responsibilities

Desktop is later.

Desktop may eventually own:

- multi-session coordination
- richer control flows
- deeper debugging
- larger interaction surfaces

Desktop is not required to prove the core Sentrits value proposition.

## MVP Scope

MVP means:

- Hub can authenticate accounts
- devices can discover their hosts through Hub
- hosts can report presence through Hub
- Hub can deliver attention notifications
- mobile can attach to an existing host session
- host remains the only authority for session access
- replay and reconnect work against the existing Core model
- relay path works without moving session logic into Hub

MVP does not require:

- full browser control surface
- multi-session desktop UX
- deep orchestration tooling
- feature parity across mobile, desktop, and web

## Implementation Rule

Prefer this sequence:

1. preserve current `Sentrits-Core` runtime semantics
2. build Hub control-plane and notification plumbing
3. add the smallest transport/auth seam necessary in Core
4. ship mobile-first intervention flow
5. add direct-connect optimization later

Do not reverse this by building large new local UI or transport abstractions
first unless clearly required.

## Technology Direction

Recommended Hub stack:

- language: Go
- API: HTTP/JSON
- signaling: WebSocket
- database: PostgreSQL
- push: APNs first
- transport strategy:
  - MVP: relay-first
  - later: direct-first with relay fallback

This is recommended because it matches the MVP:

- thin control plane
- minimal Hub state
- strong concurrency needs
- fast delivery of mobile-first product value

## Definition of Success

The architecture is being followed if:

- `Sentrits-Core` remains the session authority
- Hub remains a control plane plus relay
- mobile becomes the primary intervention surface
- web remains intentionally minimal in MVP
- notifications drive user re-entry into long-running sessions
- reconnect and replay remain host-owned and trustworthy
