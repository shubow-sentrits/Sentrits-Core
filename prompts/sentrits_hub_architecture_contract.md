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
- Hub issues a stable `device_id` per registered device; this is an account-level
  identity, not a session access credential
- Host alone authenticates session participants
- Hub must never authenticate a device *into a host session* on the host’s behalf
- Host maps Hub `device_id` to a local pairing record and makes all access
  decisions using that local record; Hub identity is input to host policy only
- Host alone decides whether a device is trusted
- Host alone decides whether a client is an observer or controller
- Internet pairing reuses the existing Core `StartPairing → ApprovePairing →
  ClaimApprovedPairing` flow with one schema extension: `PairingRecord` gains
  an optional `hub_device_id` field to carry the Hub-issued device identity
  alongside the host-generated `device_id` and `bearer_token`; Hub forwards
  pairing messages as transport only and never approves pairings on behalf of
  the host

### Identity and Connection Model

Two identity concepts must not be conflated:

- `device_id` (Hub-issued) — stable principal, persisted on the device and
  stored as an optional `hub_device_id` field on the host's `PairingRecord`
  after internet pairing. Does not replace the host-generated `device_id` or
  `bearer_token` in that record. Lets the host recognize a returning
  Hub-mediated device across sessions and connections.
- `client_id` — per-connection handle, Core-generated, ephemeral. Identifies a
  single attached connection. One device may hold multiple concurrent connections.

Hub `device_id` is carried as authenticated metadata on a connection. It does
not replace `client_id`. Controller semantics (one controller per session) apply
at the `client_id` level, unchanged.

### Cloud boundary

- Hub may route devices
- Hub may exchange rendezvous metadata
- Hub may relay encrypted or opaque session traffic
- Hub must not own session state
- Hub must not enforce session permissions
- Hub must not inspect or store session payload
- Hub must not act as an observer or controller

### Attention and Session Metadata

- Core pushes coarse session/attention metadata to Hub over the outbound
  heartbeat channel on state change
- Hub polling Core is not used; it is incompatible with NAT-traversal requirements
- The only metadata Hub may store for notification purposes:
  - host id, session id, session title, coarse lifecycle state,
    coarse attention state / reason
- Hub stores no terminal output, no replay data, no controller state

### Privacy

- Terminal output must not be stored in Hub
- Terminal input must not be stored in Hub
- Replay history must not be stored in Hub
- Hub may store only minimal metadata required for routing, presence, and notifications

### Graceful Degradation

This is a hard requirement.

- If Hub is unreachable, `Sentrits-Core` must continue to operate over
  localhost and LAN with zero session degradation
- No session may be paused, lost, or degraded because Hub is down
- Hub connectivity must be treated as optional at the Core runtime level

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

- store and use the Hub-issued host credential for outbound Hub authentication
- add minimal outbound Hub client (heartbeat, session metadata push, attention
  event push) — required because Core is server-only today and NAT traversal
  requires Core to initiate outbound connections to Hub
- support Hub-mediated transport adapter layer
- support end-to-end host/device authentication handshake over new transport paths
- extend `PairingRecord` (`include/vibe/auth/pairing.h`) with an optional
  `hub_device_id` field and update serialization in `src/store/file_stores.cpp`
  to persist it — this is the only pairing schema change required; all existing
  pairing logic, host-generated `device_id`, and `bearer_token` remain unchanged

`Sentrits-Core` should not be changed merely to make Hub easier to build.

### Core-Change Guardrail

Source of truth for session/replay/control model:

- `src/service/session_manager.cpp`
- `include/vibe/session/session_output_buffer.h`
- `src/session/session_output_buffer.cpp`
- `src/net/http_server.cpp`

`host_id` is already generated and persisted by `GenerateHostId()` /
`EnsureHostIdentity()` in `src/store/host_config_store.cpp`, called at init
from `src/net/local_auth.cpp`. Phase 1 relies on this existing behavior — no
new generation logic is needed.

Any proposed Core change that does not map directly to one of the permitted
reasons above must be rejected and reconsidered at the Hub or protocol layer.

## Sentrits-Hub Responsibilities

`Sentrits-Hub` owns:

- account authentication
- device registration and `device_id` issuance
- host registration and host credential (host token) issuance
- presence / heartbeat tracking (receives pushes from Core, does not poll)
- host discovery for user devices
- rendezvous candidate exchange
- relay allocation and transport forwarding
- push notification fanout
- forwarding pairing messages between device and host (transport only — Hub
  never approves or stores pairing decisions)

`Sentrits-Hub` may store:

- account ids
- device ids
- host ids
- Hub-issued host credentials (host tokens)
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

## Client SDK / Transport Library Responsibilities

A client-side transport/signaling library is a required MVP workstream. Without
it there is no delivery surface for mobile or web clients over Hub.

The client SDK owns:

- Hub discovery and account authentication
- Device registration and `device_id` persistence
- Host listing and presence polling
- Relay-first session attach and replay over Hub-mediated transport
- Pairing flow over forwarded relay messages
- Push notification token registration with Hub
- Reconnect and replay resume by `session_id` + last known sequence
- Direct-connect negotiation as a later optimization after relay-first is proven

The client SDK must not:

- make session access decisions
- store session replay or terminal output
- bypass host-side auth and pairing

Mobile is the primary target. A minimal web equivalent is needed for the
control-plane dashboard.

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

- Hub can authenticate accounts and issue stable `device_id` and host tokens
- devices can discover their hosts through Hub
- hosts can report presence through Hub (Core pushes over outbound channel)
- Hub can deliver attention notifications triggered by Core push
- mobile can attach to an existing host session over Hub relay
- host remains the only authority for session access
- replay and reconnect work against the existing Core model
- relay path works without moving session logic into Hub
- pairing works over forwarded relay messages using existing Core pairing flow
- Core continues to work over LAN/localhost if Hub is unreachable
- client SDK exists for mobile to discover, connect, and attach

MVP does not require:

- full browser control surface
- multi-session desktop UX
- deep orchestration tooling
- feature parity across mobile, desktop, and web
- direct peer-to-peer connect (relay-first proves the architecture; direct is an optimization)

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

## Implementation Rule

Prefer this sequence:

1. preserve current `Sentrits-Core` runtime semantics
2. build Hub control-plane and notification plumbing
3. add the smallest transport/auth seam necessary in Core (outbound Hub client,
   host token storage, transport adapter)
4. ship mobile-first intervention flow
5. add direct-connect optimization later

Do not reverse this by building large new local UI or transport abstractions
first unless clearly required.

## Definition of Success

The architecture is being followed if:

- `Sentrits-Core` remains the session authority
- Hub remains a control plane plus relay
- Core operates normally when Hub is unreachable
- mobile becomes the primary intervention surface
- web remains intentionally minimal in MVP
- notifications drive user re-entry into long-running sessions
- reconnect and replay remain host-owned and trustworthy
- `device_id` (stable principal) and `client_id` (per-connection handle) are
  kept distinct in all implementation decisions
- internet pairing uses the existing Core pairing flow forwarded through relay,
  with only the addition of an optional `hub_device_id` field on `PairingRecord`
  — not a new Hub-approved pairing mechanism
- relay-first is the MVP-A success bar; direct connect is a post-MVP-A optimization
