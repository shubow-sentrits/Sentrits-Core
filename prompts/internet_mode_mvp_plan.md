# Sentrits-Hub Internet Mode MVP Plan

## Goal

Define the MVP for a new central service, `Sentrits-Hub`, that enables internet
connectivity for Sentrits hosts and clients without taking ownership of session
state, replay, authorization, or control flow away from `Sentrits-Core`.

## Product Direction

Sentrits should follow a responsibility split driven by real usage patterns,
not feature parity.

- `Sentrits-Core`
  - execution runtime
  - session state and replay
  - trust and control authority
- mobile
  - notification-driven attach
  - decision surface
  - lightweight control handoff
- desktop
  - later surface for complex orchestration and deeper interaction
- web
  - minimal control-plane dashboard only
- `Sentrits-Hub`
  - account auth
  - presence
  - rendezvous
  - relay
  - push notifications

### Primary model: mobile-first, event-driven workflow

The core Sentrits loop is:

- long-running sessions execute on the host
- the host detects attention-worthy state
- Hub pushes a notification
- the user opens mobile
- the user reviews state and replay
- the user makes a decision or briefly takes control
- the user disconnects
- the session continues independently on the host

This means the MVP should optimize for:

- persistent sessions
- replay and reconnect
- host-owned attention and intervention signals
- lightweight mobile attach

It should not optimize first for:

- rich browser terminals
- full web parity
- multi-session orchestration UI

### Secondary model: desktop later

Desktop can eventually own:

- multi-session visibility
- orchestration
- deep debugging
- larger control flows

But desktop is not the gating surface for Hub MVP.

### Web in MVP: minimal control-plane only

The web surface should be intentionally narrow:

- account authentication
- device and host listing
- host online/offline status
- session inventory list
- optional basic attach entry point

The web surface should explicitly avoid:

- full terminal UI
- rich replay UI
- multi-session orchestration
- heavy session control

Core trust statement:
- The cloud may authenticate user accounts and route devices.
- The host alone authenticates session participants.
- The cloud must never authenticate a client *to a session* on the host's behalf.
- The cloud may say "this device belongs to account X"; only the host may say
  "this device may attach to this host and this session."

Core implementation statement:
- `Sentrits-Core` remains the runtime.
- `Sentrits-Hub` is a control plane plus optional relay.
- Only touch `Sentrits-Core` where necessary to let the existing runtime speak
  to Hub-mediated transports.
- Do not redesign local session semantics just to fit the Hub.

The cloud should do only:
- device and host registration
- online presence / heartbeats
- rendezvous candidate exchange
- relay byte forwarding as fallback
- account authentication and device-to-account association

The host must continue to own:
- session lifecycle
- session replay buffer and revision history
- trusted-device allowlist
- controller / observer decisions
- all session content

The Hub should additionally own:
- account authentication
- device registration
- host presence
- push notification fanout
- rendezvous metadata
- relay transport

## Existing Sentrits-Core Runtime To Reuse Directly

### Session core already exists

- Persistent host-owned sessions with stable `session_id`
- Sessions survive client disconnects
- Replay / catch-up by monotonically increasing sequence via `GetOutputSince(...)`
- Session snapshot + tail APIs
- Explicit controller ownership with one controller and many observers
- Remote REST + WebSocket session surfaces
- Local pairing and trusted-device model
- Recovered session loading from persisted host state
- Attention state / reason already derived in runtime
- Overview and session inventory already exposed to clients

### Current Core Model Is Already Strong

`Sentrits-Core` already has the right authority split locally:
- `SessionManager` is the session authority
- persisted session records keep `session_id`, `current_sequence`, and
  `recent_terminal_tail`
- reconnecting clients already fetch snapshot + replay from host-owned state
- control ownership is already host-enforced with explicit request / release
- attention state is already computed host-side for intervention-oriented clients

So `Sentrits-Hub` should be framed as:
- adding cloud discovery / routing / relay around the current host runtime
- reusing the current session and replay model directly
- reusing host-owned attention/supervision signals for notification triggers
- tightening transport and trust boundaries
- avoiding a major pre-Hub refactor of host session semantics

This should stay an additive transport/control-plane project, not a rewrite of
session state management.

### Existing code surfaces

- Session persistence and replay:
  - [`include/vibe/service/session_manager.h`](/home/shubow/dev/Sentrits-Core/include/vibe/service/session_manager.h)
  - [`src/service/session_manager.cpp`](/home/shubow/dev/Sentrits-Core/src/service/session_manager.cpp)
  - [`src/net/http_server.cpp`](/home/shubow/dev/Sentrits-Core/src/net/http_server.cpp)
- Remote API and websocket lanes:
  - [`src/net/http_shared.cpp`](/home/shubow/dev/Sentrits-Core/src/net/http_shared.cpp)
  - [`src/net/http_server.cpp`](/home/shubow/dev/Sentrits-Core/src/net/http_server.cpp)
- Pairing / trusted devices:
  - [`src/net/http_shared.cpp`](/home/shubow/dev/Sentrits-Core/src/net/http_shared.cpp)
  - [`src/auth/default_pairing_service.cpp`](/home/shubow/dev/Sentrits-Core/src/auth/default_pairing_service.cpp)
  - [`include/vibe/store/pairing_store.h`](/home/shubow/dev/Sentrits-Core/include/vibe/store/pairing_store.h)

### Existing behavior to treat as source of truth

- `CreateSession(...)` creates stable host-owned sessions
- `LoadPersistedSessions()` restores prior host sessions into recovered state
- `GetSnapshot(...)` returns current session state
- `GetTail(...)` returns recent buffered output
- `GetOutputSince(...)` replays from a monotonic sequence point
- `RequestControl(...)` / `ReleaseControl(...)` enforce host-owned controller rules
- websocket session attach already uses sequence-based replay and resume behavior
- session summaries already expose attention state / reason / timestamps

These behaviors should be preserved and exposed over internet transport rather
than redesigned.

## Gaps To Be Solved By Sentrits-Hub

- No Hub-backed host discovery over the public internet
- No Hub rendezvous service for direct connection setup
- No Hub relay fallback transport
- Pairing is host-local today, not Hub-assisted forwarding
- No direct device identity handshake over a Hub-forwarded path
- No NAT traversal strategy
- No Hub push-notification path for attention events
- Current web scope is broader than the intended MVP dashboard role
- The Hub/Core trust boundary is not yet formalized in code or docs:
  - account auth may live in Hub
  - session participant auth must remain host-side
  - relay must stay transport-only

## MVP Hierarchy

### MVP 0: Preserve Existing Core Semantics

This is the non-negotiable baseline. Internet mode must not break these.

- [ ] Stable session identity remains host-generated and host-owned
- [ ] Replay remains host-owned with monotonic output sequence / revision
- [ ] Reconnect attaches to existing session instead of creating a new one
- [ ] One controller / many observers remains enforced by host only
- [ ] Cloud has no session state authority
- [ ] Cloud stores no terminal output, input, or replay history
- [ ] Cloud may authenticate accounts, but host alone authenticates session participants
- [ ] Existing `SessionManager` replay/control semantics stay the source of truth
- [ ] Hub integration does not require major `Sentrits-Core` session-model refactors
- [ ] Mobile-first intervention workflow is the primary product target
- [ ] Web stays control-plane only in MVP

### MVP 1: Hub Presence and Host Discovery

Minimal Hub control plane.

- [ ] Account authentication in Hub
- [ ] Device registration API
- [ ] Host registration API
- [ ] Host heartbeat / online presence
- [ ] Client can list its visible hosts from Hub
- [ ] Hub stores only routing metadata and presence metadata
- [ ] Host identity exposed to Hub uses stable `host_id`
- [ ] Device identity exposed to Hub uses stable `device_id`
- [ ] Hub stores enough lightweight session metadata to notify mobile:
  - session id
  - title
  - coarse state
  - attention state / reason

Deliverable:
- Client can see "which of my hosts are online" without yet attaching

### MVP 2: Hub Rendezvous for Direct Connection

Hub helps connect peers but does not join session logic.

- [ ] Host asks Hub for rendezvous session
- [ ] Client asks Hub to connect to host
- [ ] Hub exchanges candidate addresses between host and client
- [ ] Attempt direct peer-to-peer transport first
- [ ] Host and client complete direct host-auth handshake without trusting Hub
- [ ] Hub does not authorize session attach
- [ ] Hub-issued account/device identity is only input to host policy, never the decision itself

Deliverable:
- Best-effort direct connection path over the public internet

### MVP 3: Hub Relay Fallback

When direct path fails, fall back to Hub relay as dumb byte forwarder.

- [ ] Relay channels created by Hub for host/client pair
- [ ] Relay forwards opaque bytes only
- [ ] Relay does not decode session protocol
- [ ] Relay does not inspect or store session payload
- [ ] Host/client protocol above relay remains identical to direct path
- [ ] Reconnect over relay still resumes existing host session
- [ ] Host-side auth and session attach semantics are identical on direct and relay paths

Deliverable:
- Direct first, relay second, same host-owned semantics either way

### MVP 4: End-to-End Trust and Pairing Over Internet

Host remains the sole trust authority.

- [ ] Client presents stable device identity directly to host
- [ ] Host checks local paired-device allowlist
- [ ] Host accepts or rejects connection
- [ ] Hub cannot approve, impersonate, or inject trust
- [ ] Hub may assert account identity, but host still evaluates device trust locally
- [ ] Pairing flow works through forwarded messages when host/client are remote
- [ ] Pairing result is stored only in host local trusted-device store

Deliverable:
- Internet-capable pairing without Hub-owned permissions

### MVP 5: Remote Session Attach / Replay / Control

Reuse current session model over the new transport.

- [ ] Client queries host session inventory after authentication
- [ ] Client fetches snapshot for existing session
- [ ] Client fetches replay since revision / sequence
- [ ] Client attaches as observer by default
- [ ] Client can explicitly request controller ownership
- [ ] Host arbitrates control transfer
- [ ] Disconnect + reconnect resumes from replay gap
- [ ] Existing host replay model (`snapshot` + `GetOutputSince`) is reused directly
- [ ] Mobile attach is optimized for inspect / decide / disconnect

Deliverable:
- Internet client behavior matches current LAN / localhost semantics

### MVP 6: Notification-driven intervention

This is part of the core product value, not an optional add-on.

- [ ] Host emits lightweight attention events suitable for Hub delivery
- [ ] Hub stores only minimal notification metadata
- [ ] Hub pushes notifications to iOS via APNs
- [ ] Mobile can deep-link into host/session attach flow
- [ ] Notification content avoids session-payload leakage

Deliverable:
- user can stay in the loop away from desk

### MVP 7: Security and Privacy Hardening

Needed for serious internet use, but some parts can be phased.

- [ ] Hub stores only metadata, not session content
- [ ] Relay payload treated as opaque bytes
- [ ] Access tokens scoped only to Hub rendezvous / relay usage
- [ ] Host verifies paired device directly on every session connection
- [ ] Audit what metadata is safe to expose to cloud
- [ ] Optional end-to-end encryption design documented
- [ ] Push payloads contain only minimal session metadata

Deliverable:
- Clear privacy boundary even before full E2EE

## Recommended Delivery Order

### Phase 1: Hub control plane only

- [ ] Define cloud objects:
  - account
  - host registration
  - device registration
  - presence / heartbeat
  - rendezvous session
  - relay allocation
  - push notification subscription
- [ ] Implement Hub host heartbeat
- [ ] Implement Hub-backed client host listing
- [ ] Implement APNs push plumbing

### Phase 2: Minimal host-facing transport seam

Do not start with a large Core refactor. Only introduce the smallest seam
needed to let the current runtime speak over Hub-mediated transports.

- [ ] Define a minimal authenticated byte/message channel interface
- [ ] Keep current `SessionManager` and replay model unchanged
- [ ] Reuse current remote HTTP / WS attach semantics where practical
- [ ] Add Hub direct/relay stream implementations around the existing host APIs
- [ ] Document each required `Sentrits-Core` change and justify it as necessary glue
- [ ] Add the smallest possible attention-event export path for Hub notifications

### Phase 3: Direct connect

- [ ] Add rendezvous candidate exchange
- [ ] Add direct connect attempt
- [ ] Add host-auth handshake over direct stream

### Phase 4: Relay fallback

- [ ] Add relay byte channel
- [ ] Reuse same authenticated stream protocol over relay

### Phase 5: Internet pairing

- [ ] Add host-mediated remote pairing handshake
- [ ] Persist approved device locally on host

### Phase 6: Notification-driven mobile flow

- [ ] Emit host-side attention events
- [ ] Push to mobile through Hub
- [ ] Mobile deep-link to session inspect/attach

### Phase 7: Reconnect and recovery polish

- [ ] Resume by session id
- [ ] Replay from last revision
- [ ] Controller transfer and reconnect edge cases

## Checklist by Workstream

### Host Runtime

- [ ] Keep session manager authoritative
- [ ] Keep replay buffer authoritative
- [ ] Expose attach/resume/replay through the thinnest possible transport seam
- [ ] Add authentication handshake independent of transport path
- [ ] Avoid rewriting existing session persistence, replay, and control logic first
- [ ] Touch `Sentrits-Core` only when Hub integration requires it

### Client Runtime

- [ ] Discover hosts from cloud presence
- [ ] Attempt direct connect first
- [ ] Fall back to relay automatically
- [ ] Resume sessions using last known session id + revision

### Mobile Runtime

- [ ] Receive push notifications
- [ ] Show host/session attention list
- [ ] Attach for replay + decision
- [ ] Request/release control briefly when needed
- [ ] Optimize for interruption, not orchestration

### Web Runtime

- [ ] Account sign-in
- [ ] Device list
- [ ] Host list
- [ ] Online/offline status
- [ ] Session inventory list
- [ ] Avoid building rich terminal/control UI in MVP

### Desktop Runtime

- [ ] Explicitly out of MVP unless needed for narrow admin/testing tasks

### Cloud Service

- [ ] Account authentication API
- [ ] Registration API
- [ ] Presence / heartbeat API
- [ ] Rendezvous candidate exchange API
- [ ] Relay allocation + relay forwarding API
- [ ] Push notification API / APNs integration
- [ ] No session state storage
- [ ] No permission enforcement
- [ ] No session participant authorization

Rename in implementation/docs:
- [ ] this service is `Sentrits-Hub`, not generic "cloud"

### Trust / Security

- [ ] Host owns allowlist
- [ ] Device identity is end-to-end to host
- [ ] Pairing is host-authorized only
- [ ] Cloud cannot observe or control sessions
- [ ] Cloud may authenticate accounts and route devices, but host authenticates session participants
- [ ] Hub never becomes a second authority for session access
- [ ] Push payloads remain minimal and non-sensitive

### Protocol

- [ ] Stable device identity format
- [ ] Stable host identity format
- [ ] Connection handshake format
- [ ] Resume / replay request format
- [ ] Explicit controller request / release format

## Suggested MVP Cuts

### MVP-A: Usable but narrow

- [ ] Presence
- [ ] Rendezvous
- [ ] Relay only
- [ ] Existing session replay / attach semantics
- [ ] Host-owned pairing and auth
- [ ] Mobile push notifications
- [ ] Minimal web host/session listing only

This skips direct peer-to-peer initially but proves the architecture.

### MVP-B: Full intended internet mode

- [ ] Presence
- [ ] Direct connect first
- [ ] Relay fallback
- [ ] Host-owned pairing and auth
- [ ] Session resume and replay
- [ ] Explicit control ownership
- [ ] Push-driven mobile intervention

## Recommendation

Use MVP-A first if speed matters:
- relay-first internet mode
- no session semantics in cloud
- same host-owned replay / control model
- mobile-first intervention loop
- minimal web dashboard only

Then add direct-connect as an optimization, not as the core semantic path.

That keeps the architecture honest:
- cloud is still dumb
- host stays authoritative
- reconnect and replay semantics stay exactly where they belong
- current Sentrits session model remains largely intact
- Hub is an external service around Core, not a replacement for Core runtime behavior

## Recommended Development Path

### Product pathway

Build in this order:

1. Hub control plane + mobile notification loop
2. Mobile attach / replay / brief control
3. Relay-backed internet attach
4. Direct connection optimization
5. Desktop complexity later
6. Rich web later only if justified

Do not build in this order:

1. rich web terminal
2. browser feature parity
3. desktop orchestration
4. mobile later

That would optimize the wrong surface first.

### Technical pathway

Given this product direction, the most practical stack is:

- `Sentrits-Hub`: Go
- control-plane API: HTTP/JSON
- signaling / live coordination: WebSocket
- database: PostgreSQL
- push notifications: APNs integration from Hub
- transport:
  - MVP-A: relay-first
  - MVP-B: direct-first with relay fallback

Why this fits:

- Go is a strong fit for a thin high-concurrency control-plane and relay service
- PostgreSQL is enough for accounts, devices, hosts, presence, rendezvous, and push metadata
- Hub does not need a heavy framework
- relay-first keeps required Core changes smaller
- mobile-first means push and session inventory matter earlier than browser transport sophistication

### Suggested stack

- language: Go
- HTTP framework: standard `net/http` or only a very small router
- database: PostgreSQL
- auth:
  - account auth in Hub
  - device identity bound to account in Hub
  - host trust decision remains in Core
- push:
  - APNs first
- relay:
  - simple opaque byte relay managed by Hub

### What not to do first

- do not build Hub in C++ unless there is a very specific low-level requirement
- do not choose Python for the long-lived relay/control-plane core as the default path
- do not force WebRTC into MVP if relay-first proves the product loop faster
- do not expand the web client into a full terminal/control surface for MVP

## Definition of Done

Internet mode is done when all of the following are true:

- [ ] A paired client can discover a host through the cloud
- [ ] Client can attach to an existing host session over the internet
- [ ] Disconnect does not kill the session
- [ ] Reconnect resumes by `session_id`
- [ ] Replay catch-up works from a prior revision / sequence
- [ ] Host alone decides who is trusted
- [ ] Host alone decides who controls a session
- [ ] Cloud stores no session payload and cannot read replay data
- [ ] Direct connect is attempted first
- [ ] Relay fallback works when direct connect fails
- [ ] Hub can push attention notifications with minimal metadata only
- [ ] Mobile can inspect and intervene without requiring a desk-bound workflow
