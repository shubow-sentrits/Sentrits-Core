# Sentrits-Hub Internet Mode MVP Plan

## Goal

Define the MVP for a new central service, `Sentrits-Hub`, that enables internet
connectivity for Sentrits hosts and clients without taking ownership of session
state, replay, authorization, or control flow away from `Sentrits-Core`.

## Current Status Snapshot (2026-04-28)

This plan is the architectural source of truth, but current implementation
status matters for sequencing.

### MVP 1 complete

- `Sentrits-Hub` Phase 1 control-plane surfaces exist and are tested:
  - account auth
  - device registration
  - host registration
  - heartbeat / presence
  - host listing
- `Sentrits-Core` outbound Hub client:
  - `hub_url` / `hub_token` in `HostIdentity`
  - outbound heartbeat thread
  - session snapshot collected on the owning `io_context` via `asio::post`
  - HTTPS path with peer verification and hostname verification
- Hub DB-backed integration coverage: host ownership, token rotation,
  heartbeat replacement, online/offline computation

### MVP 2A in progress (host-side relay complete)

What is done as of 2026-04-28:

- Core `HubControlChannel`: persistent control WS to Hub `/api/v1/hosts/stream`
  - fully async (Beast `async_read` + `ioc.run()`); `Stop()` is instantaneous
  - sends `session.inventory` on connect; Hub persists it via `UpdateHostHeartbeat`
  - spawns relay bridge threads on `relay.requested`; bridges are also async
    and stop cleanly with `Stop()`
  - local TLS flag correctly selects WSS/verify-none for loopback session side
- Hub control stream (`handleHostStream`): reads `session.inventory`, updates DB
- Hub relay host endpoint (`/api/v1/relay/host/{channel_id}`): accepts host bridge
- Hub relay token store: single-use, 30-second TTL, session-path-scoped tokens
- All of the above are covered by unit and integration tests

What is not done yet (remaining for MVP 2A):

- Hub client-facing relay request endpoint: client calls Hub to request relay for
  a session → Hub sends `relay.requested` to host over control WS → host bridge
  connects → bytes flow end-to-end
- End-to-end relay integration test (client ↔ Hub relay ↔ host session)
- Any internet pairing flow
- Any mobile Hub client/service layer

Most important planning correction from current code and iOS review:

- the existing iOS client still uses HTTP REST for session list, snapshot, and
  pairing, and WebSocket for live session streaming/input
- therefore a WebSocket-only relay is a valid MVP 2 transport proof, but it is
  not yet a complete internet replay/attach solution by itself
- the plan must distinguish:
  - `MVP 2A`: relay transport proof over WebSocket attach
  - `MVP 2B`: full replay-capable internet attach, which requires the HTTP path
    needed by the current clients

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
  - [`include/vibe/session/session_output_buffer.h`](/home/shubow/dev/Sentrits-Core/include/vibe/session/session_output_buffer.h)
  - [`src/session/session_output_buffer.cpp`](/home/shubow/dev/Sentrits-Core/src/session/session_output_buffer.cpp)
- Remote API and websocket lanes:
  - [`src/net/http_shared.cpp`](/home/shubow/dev/Sentrits-Core/src/net/http_shared.cpp)
  - [`src/net/http_server.cpp`](/home/shubow/dev/Sentrits-Core/src/net/http_server.cpp)
- Pairing / trusted devices:
  - [`src/net/http_shared.cpp`](/home/shubow/dev/Sentrits-Core/src/net/http_shared.cpp)
  - [`src/auth/default_pairing_service.cpp`](/home/shubow/dev/Sentrits-Core/src/auth/default_pairing_service.cpp)
  - [`include/vibe/store/pairing_store.h`](/home/shubow/dev/Sentrits-Core/include/vibe/store/pairing_store.h)
- Host identity (already generates and persists stable `host_id`):
  - [`src/store/host_config_store.cpp`](/home/shubow/dev/Sentrits-Core/src/store/host_config_store.cpp) — `GenerateHostId()`, `EnsureHostIdentity()`
  - [`src/net/local_auth.cpp`](/home/shubow/dev/Sentrits-Core/src/net/local_auth.cpp) — calls `EnsureHostIdentity()` at init

### Core-Change Guardrail

`Sentrits-Core` should only be changed for these specific reasons during Hub
integration. Any proposed Core change that does not fit one of these categories
should be rejected and reconsidered at the Hub or protocol layer instead.

Permitted Core changes:
- Generate and expose Hub registration credential (store Hub-issued host token
  alongside `HostIdentity`)
- Add minimal outbound Hub client (heartbeat, session metadata push, attention
  event push) — Core is currently server-only; this is required for NAT
  traversal
- Add transport adapter layer to route Hub relay/direct streams through
  existing session attach semantics
- Export attention event signals to Hub over the outbound channel
- Support end-to-end host/device authentication handshake over new transport
  paths
- Extend `PairingRecord` (`include/vibe/auth/pairing.h`) with an optional
  `hub_device_id` field; update serialization in `src/store/file_stores.cpp`
  to persist it. This is the only pairing schema change required. All existing
  pairing logic, host-generated `device_id`, and `bearer_token` remain
  unchanged.

Not permitted without explicit justification:
- Redesigning session persistence, replay, or control semantics
- Moving any trust or session-state logic into Hub-visible form
- Adding Hub-specific session fields to `SessionManager` or `SessionSummary`
- Replacing or redesigning host-generated pairing credentials (`device_id`,
  `bearer_token`) — the only permitted pairing change is adding an optional
  `hub_device_id` field to `PairingRecord`

Source of truth for session/replay/control model:
- `src/service/session_manager.cpp`
- `include/vibe/session/session_output_buffer.h`
- `src/session/session_output_buffer.cpp`
- `src/net/http_server.cpp`

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

### Graceful Degradation Requirement

This is a hard requirement, not a nice-to-have.

If `Sentrits-Hub` is unreachable, `Sentrits-Core` must continue to operate over
localhost and LAN with zero session degradation. No session should be paused,
lost, or degraded because Hub is down. Hub is an additive internet transport
layer. The local runtime is always self-sufficient.

### Identity and Connection Model

Two distinct identity concepts must not be conflated:

- `device_id` (Hub-issued) — stable principal, persisted on the device and
  stored as an optional `hub_device_id` field on the host's `PairingRecord`
  after internet pairing. Does not replace the host-generated `device_id` or
  `bearer_token` in that record. Lets the host recognize a returning
  Hub-mediated device on future connections.
- `client_id` — per-connection handle, Core-generated, ephemeral. Identifies a
  single attached connection. One device may have multiple concurrent
  connections.

Hub `device_id` is carried as authenticated metadata on a connection. It does
not replace `client_id`. Controller semantics (one controller per session)
continue to apply at the `client_id` level, unchanged.

## Gaps To Be Solved By Sentrits-Hub

- No Hub-backed host discovery over the public internet
- No Hub rendezvous service for direct connection setup
- No Hub relay fallback transport
- Core has no outbound HTTP client; heartbeats, session metadata push, and
  attention event export all require adding a minimal Hub client to Core
- No Hub-issued host credential for host→Hub authentication; Hub must issue a
  long-lived host token at registration, stored alongside `HostIdentity` in
  Core's local store
- Pairing is host-local today; for internet mode, Hub forwards pairing messages
  between device and host over relay — Hub does not approve pairings
- No direct device identity handshake over a Hub-forwarded path; Hub
  `device_id` must map to a host-local pairing record — Hub authenticates the
  device to Hub, host authenticates the device to the session
- No NAT traversal strategy
- No Hub push-notification path for attention events; Core will push coarse
  attention/session metadata to Hub over the same outbound heartbeat channel
  (Hub polling is not viable once Core is behind NAT)
- Current web scope is broader than the intended MVP dashboard role
- The Hub/Core trust boundary is not yet formalized in code or docs:
  - account auth lives in Hub
  - session participant auth must remain host-side
  - relay must stay transport-only
- No client-side SDK/library scoped for Hub discovery and transport negotiation
  (mobile-first; required for any client to reach a Hub-mediated session)

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

- [x] Account authentication in Hub
- [x] Device registration API
- [x] Host registration API
- [x] Hub issues long-lived host credential (host token) at registration; Core
      stores it alongside `HostIdentity`
- [x] Host heartbeat / online presence (Core pushes over outbound channel using
      existing `host_id` from `EnsureHostIdentity()` — no new generation needed)
- [x] Core adds minimal outbound Hub client for heartbeat and metadata push
- [x] Client can list its visible hosts from Hub
- [x] Hub stores only routing metadata and presence metadata
- [x] Host identity exposed to Hub uses stable `host_id` (already generated by
      `GenerateHostId()` / `EnsureHostIdentity()` in Core)
- [x] Device identity exposed to Hub uses stable `device_id`
- [x] Core pushes coarse session metadata to Hub over heartbeat channel:
  - session id
  - title
  - coarse state
  - attention state / reason
  - (source: `SessionSummary` in `session_manager.h`)
- [x] If Hub is unreachable, Core continues over localhost/LAN with no
      degradation

Deliverable:
- Client can see "which of my hosts are online" without yet attaching

Definition of done:
- [x] Host registers with Hub on startup using Hub-issued token
- [x] Host sends periodic heartbeats; Hub reflects online/offline state
- [x] Client can list hosts associated with its account
- [x] Core works normally if Hub endpoint is unreachable

### MVP 2A: Hub Relay Transport Proof

Relay-first is still the MVP-A path, but the first relay slice should be framed
honestly: it proves the Hub/Core/client transport seam before claiming complete
internet replay parity.

- [x] Relay channels created by Hub for host/client pair
- [x] Relay forwards opaque bytes only
- [x] Relay does not decode session protocol
- [x] Relay does not inspect or store session payload
- [x] Core opens host-side relay connection in response to Hub relay request
- [ ] Existing host WebSocket attach path can be reached through relay
      _(host bridge is complete; missing: Hub client-facing relay request endpoint)_
- [ ] Existing live session stream can traverse relay unchanged
      _(blocked by same missing client-facing endpoint)_

Deliverable:
- Internet WebSocket session attach works end-to-end over relay without
  changing current `Sentrits-Core` session semantics

Definition of done:
- [x] Relay forwards bytes opaquely between host and client
- [x] Core host-side relay bridge can attach to an existing session by `session_id`
- [ ] Existing session WebSocket protocol works over relay unchanged
      _(end-to-end test pending; requires client-facing relay endpoint)_

### MVP 2B: Replay-Capable Internet Attach

This is the first slice that truly delivers replay/reconnect parity for the
current clients. It requires the HTTP path used today for session inventory,
snapshot, and pairing-related flows, not just the WebSocket path.

- [ ] Client can query host session inventory over internet path
- [ ] Client can fetch session snapshot over internet path
- [ ] Client can fetch replay since sequence / revision over internet path
- [ ] Reconnect over internet path resumes existing host session
- [ ] Replay remains host-owned and sequence-based:
  - snapshot fetch
  - `GetOutputSince(...)`
  - reconnect by `session_id` + last known sequence
- [ ] Hub remains transport/control-plane only; replay stays entirely on host

Deliverable:
- Internet attach works with the same replay/reconnect semantics as LAN for the
  currently supported clients

Definition of done:
- Client can fetch session inventory, snapshot, and replay over internet path
- Reconnect resumes existing host session by `session_id`
- Replay catch-up works using the existing Core sequence model

### MVP 3: End-to-End Trust and Pairing Over Internet

Host remains the sole trust authority.

- [ ] Client presents stable device identity directly to host
- [ ] Host checks local paired-device allowlist
- [ ] Host accepts or rejects connection
- [ ] Hub cannot approve, impersonate, or inject trust
- [ ] Hub may assert account identity, but host still evaluates device trust locally
- [ ] Pairing flow works through forwarded messages when host/client are remote;
      Hub forwards pairing messages as transport only — the existing Core
      `StartPairing → ApprovePairing → ClaimApprovedPairing` flow is reused
      with one schema extension: `PairingRecord` gains an optional
      `hub_device_id` field to carry the Hub-issued device identity alongside
      the host-generated credentials
- [ ] Hub never approves a pairing on behalf of the host
- [ ] Hub `device_id` is stored as an optional `hub_device_id` field on the host's `PairingRecord` after pairing completes — it is attached metadata alongside the host-generated `device_id` and `bearer_token`, not a replacement
- [ ] Pairing result is stored only in host local trusted-device store

Deliverable:
- Internet-capable pairing without Hub-owned permissions

Definition of done:
- Device can initiate pairing with host through Hub-forwarded relay messages
- Host approves/rejects using existing local pairing logic
- Hub `device_id` is associated with host pairing record post-approval
- No pairing state is stored in Hub

### MVP 4: Remote Session Control Parity

Reuse current session model over the new transport after replay-capable internet
attach exists.

- [ ] Client attaches as observer by default
- [ ] Client can explicitly request controller ownership
- [ ] Host arbitrates control transfer
- [ ] Disconnect + reconnect resumes from replay gap
- [ ] Mobile attach is optimized for inspect / decide / disconnect

Deliverable:
- Internet client behavior matches current LAN / localhost semantics

Definition of done:
- Client can attach and control over internet with the same host-owned rules as LAN
- Disconnect does not kill the session; reconnect resumes by `session_id`
- `device_id` (stable principal) and `client_id` (per-connection handle) remain
  distinct; controller semantics operate at `client_id` level, unchanged

### MVP 5: Notification-driven intervention

This is part of the core product value, not an optional add-on.

- [ ] Host emits lightweight attention events suitable for Hub delivery
- [ ] Hub stores only minimal notification metadata
- [ ] Hub pushes notifications to iOS via APNs
- [ ] Mobile can deep-link into host/session attach flow
- [ ] Notification content avoids session-payload leakage

Deliverable:
- user can stay in the loop away from desk

Definition of done:
- Host pushes attention events to Hub over the outbound heartbeat channel
- Hub delivers push notification to mobile via APNs
- Notification contains only minimal metadata (session id, title, attention reason)
- Mobile deep-link lands on correct host/session attach flow

### MVP 6: Hub Rendezvous for Direct Connection

This is a post-MVP-A optimization. It should reuse the same host auth and
session semantics already proven on relay.

- [ ] Host asks Hub for rendezvous session
- [ ] Client asks Hub to connect to host
- [ ] Hub exchanges candidate addresses between host and client
- [ ] Attempt direct peer-to-peer transport first
- [ ] Host and client complete direct host-auth handshake without trusting Hub
- [ ] Hub does not authorize session attach
- [ ] Hub-issued `device_id` is presented to host as identity metadata; host
      maps it to a local pairing record and makes the access decision itself
- [ ] Hub-issued account/device identity is only input to host policy, never the decision itself

Deliverable:
- Best-effort direct connection path over the public internet

Definition of done:
- Host and client can negotiate a direct connection through Hub rendezvous
- Host completes auth handshake directly with device, independent of Hub
- Hub `device_id` maps to host's local paired device record

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
  - host registration (including Hub-issued host token)
  - device registration
  - presence / heartbeat
  - rendezvous session
  - relay allocation
  - push notification subscription
- [ ] Implement Hub host registration and host token issuance
- [ ] Implement Hub host heartbeat receiver
- [ ] Add minimal outbound Hub client to Core (heartbeat + coarse session
      metadata push); uses existing `host_id` from `EnsureHostIdentity()` —
      no new generation needed
- [ ] Store Hub-issued host token alongside `HostIdentity` in Core's local store
- [ ] Implement Hub-backed client host listing
- [ ] Implement APNs push plumbing
- [ ] Verify: Core operates normally when Hub endpoint is unreachable
- [ ] Finish minimum Phase 1 test gate before relay work:
  - Hub ownership conflict integration test
  - Hub heartbeat replacement integration test
  - Core HTTPS verification test
  - Core outbound heartbeat integration test
  - one Core test covering `io_context` snapshot ownership model

### Phase 2: Minimal host-facing transport seam

Do not start with a large Core refactor. Only introduce the smallest seam
needed to let the current runtime speak over Hub-mediated transports.

- [ ] Define a minimal authenticated byte/message channel interface
- [ ] Keep current `SessionManager` and replay model unchanged
- [ ] Reuse current remote HTTP / WS attach semantics where practical
- [ ] Add Hub direct/relay stream implementations around the existing host APIs
- [ ] Document each required `Sentrits-Core` change and justify it against the
      Core-change guardrail (see above)
- [ ] Attention events export via the outbound Hub client added in Phase 1 —
      Core pushes on state change; Hub polling is explicitly not used (host may
      be behind NAT)
- [ ] First transport target is relay, not direct
- [ ] First relay validation target is WebSocket attach path, not full protocol parity

### Phase 3: Relay transport proof (MVP 2A)

- [x] Add relay byte channel
- [x] Add host-side control channel from Core to Hub
- [x] Session inventory pushed over control channel on connect
- [ ] Add Hub client-facing relay request endpoint (remaining blocker)
- [ ] Reuse existing session WebSocket protocol over relay (end-to-end test)
- [ ] Validate live attach over relay
- [x] Keep Hub transport-only; no Hub protocol awareness above byte forwarding

### Phase 4: Replay-capable internet attach (MVP 2B)

- [ ] Add the HTTP path needed by current clients for session inventory,
      snapshot, and replay fetch
- [ ] Validate reconnect and replay catch-up over the internet path
- [ ] Keep replay entirely host-owned; Hub remains control-plane / transport only

### Phase 5: Internet pairing and trust completion

- [ ] Add host-mediated remote pairing handshake
- [ ] Persist approved device locally on host
- [ ] Carry Hub `device_id` as metadata mapped to local pairing record

### Phase 6: Notification-driven mobile flow

- [ ] Emit host-side attention events
- [ ] Push to mobile through Hub
- [ ] Mobile deep-link to session inspect/attach

### Phase 7: Reconnect and recovery polish

- [ ] Resume by session id
- [ ] Replay from last revision
- [ ] Controller transfer and reconnect edge cases

### Phase 6: Direct connect optimization

- [ ] Add rendezvous candidate exchange
- [ ] Add direct connect attempt
- [ ] Add host-auth handshake over direct stream

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
- [ ] Connect via relay (MVP-A); direct connect is added as a post-MVP-A optimization
- [ ] Fall back to relay automatically when direct connect is attempted (MVP-B)
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

### Client SDK / Transport Library

This is a required MVP workstream. Without it there is no delivery surface for
mobile or web clients.

- [ ] Hub discovery and authentication (account login, device registration)
- [ ] Host listing from Hub
- [ ] Relay-first session attach over Hub-mediated transport
- [ ] Replay and reconnect using `session_id` + last known sequence
- [ ] Pairing flow over forwarded relay messages
- [ ] Push notification registration (APNs device token delivery to Hub)
- [ ] Mobile-first: optimize for quick attach, inspect, disconnect
- [ ] Minimal web equivalent for control-plane dashboard (host/session listing)
- [ ] Direct-connect negotiation added after relay-first path is proven

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
- [ ] Relay transport works end-to-end (direct connect is a post-MVP-A optimization)
- [ ] Relay fallback works when direct connect fails (MVP-B)
- [ ] Hub can push attention notifications with minimal metadata only
- [ ] Mobile can inspect and intervene without requiring a desk-bound workflow
