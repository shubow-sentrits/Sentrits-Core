# Session Attention Inference V1

This document freezes the first attention model for `sentrits`.

It answers a different question than lifecycle:

> Should a human look at this session now?

The model is conservative, provider-agnostic, and intended to support host UI badges, remote supervision UI, watch mode, and later notifications.

## Core Alignment

Lifecycle and attention remain separate layers.

Lifecycle answers:

- is the process running
- is it awaiting input
- did it exit
- did it fail

Attention answers:

- should a human look now
- how urgent is that
- why

The runtime should expose both.

## Attention States

Recommended v1 attention states:

- `none`
- `info`
- `action_required`
- `intervention`

These are intentionally small and stable.

## Attention Reasons

Recommended v1 reasons:

- `none`
- `awaiting_input`
- `session_error`
- `session_disconnected`
- `workspace_changed`
- `git_state_changed`
- `controller_changed`
- `session_exited_cleanly`
- `idle_too_long`
- `stalled_no_progress`
- `unstable_connection`

The reason should be machine-readable and suitable for client sorting, badges, and future notifications.

## Required Signals

Attention inference should prefer structured signals:

- lifecycle status
- process liveness
- controller ownership and controller changes
- attach/detach churn
- output timestamps
- activity timestamps
- file-change timestamps and counts
- git state transitions

Avoid v1 dependence on fragile PTY text parsing.

## Time and Decay

Attention should include time-aware behavior from the start:

- `attentionSince`
- short decay for info-level states
- notification cooldowns for alert-worthy states

Recommended initial decay windows:

- `workspace_changed` -> 30s
- `git_state_changed` -> 30s
- `controller_changed` -> 20s
- `session_exited_cleanly` -> 120s

## Initial Rules

Immediate:

- `Error` -> `intervention / session_error`
- `AwaitingInput` -> `action_required / awaiting_input`

Short-lived info:

- recent file changes -> `info / workspace_changed`
- recent git transition -> `info / git_state_changed`
- recent controller change -> `info / controller_changed`
- clean exit -> `info / session_exited_cleanly`

Later conservative rules:

- long quiet alive session with no output/files -> `intervention / stalled_no_progress`
- repeated attach/detach churn -> `intervention / unstable_connection`

## Important Nuance

`Disconnected` is useful as a supervision-facing condition, but it should not be conflated too early with the current internal PTY process lifecycle enum.

For this codebase today:

- process lifecycle remains the base truth for the session runtime
- connection loss and client churn should be modeled as separate supervisory signals
- a future client-facing `lifecycleStatus` or `availabilityStatus` may include `Disconnected` without forcing the internal runtime state machine to collapse transport and process concerns together

This keeps the architecture honest while still allowing the product to express disconnected or unstable supervision conditions clearly.

## JSON Direction

Client-facing session/event payloads should eventually expose:

- `lifecycleStatus`
- `attentionState`
- `attentionReason`
- `attentionSince`

And supporting time fields such as:

- `lastOutputAt`
- `lastActivityAt`
- `lastFileChangeAt`
- `lastLifecycleTransitionAt`
- `lastControllerChangeAt`
- `lastAttentionChangeAt`

## Notification Scope

Initial watch notifications should be limited to:

- `awaiting_input`
- `session_error`
- `session_disconnected`
- `stalled_no_progress`

Info-only reasons should affect UI state but not notify.

## Rollout Plan

Phase 1:

- attention state separation
- `attentionState`
- `attentionReason`
- `attentionSince`
- rules for `session_error`, `awaiting_input`, `workspace_changed`, `session_exited_cleanly`

Phase 2:

- time-aware info decay
- `git_state_changed`
- `controller_changed`
- notification cooldown plumbing

Phase 3:

- `stalled_no_progress`
- `unstable_connection`

Phase 4:

- optional provider-specific enrichments
- optional resource overlays

## Review Standard

A new attention rule should only ship if it is:

- based on structured signals
- explainable to users
- conservative
- stable over time
- equipped with a clear reason code
- easy for clients to render

## State Consolidation Plan

This is the follow-on plan to reduce client-visible state sprawl while keeping richer terminal and session evidence in Core.

Target principle:

- keep rich evidence internal
- derive one strengthened assessment model in Core
- expose a smaller, more coherent client-facing state surface

### Current Problems

- `supervisionState`, `activityState`, `isActive`, and `interactionKind` partially overlap
- `attentionState`, `attentionReason`, and `semanticPreview` are one concept split across multiple fields
- new terminal semantics such as `terminalSemanticChange` are evidence, not product state, and should not become another top-level client axis
- `SessionSummary`, `SessionSignals`, and `SessionNodeSummary` duplicate meaning in slightly different shapes

### Target Layers

1. Evidence layer
- terminal screen/buffer diff
- raw vs meaningful vs cosmetic output timing
- stdin activity / future interaction segmentation
- controller changes
- file and git changes

2. Assessment layer
- one authoritative Core-owned model derived from evidence
- includes:
  - lifecycle mode
  - interaction mode
  - activity class
  - attention level
  - attention cause
  - user-facing summary

3. Presentation layer
- thin client-facing API
- clients mainly consume:
  - mode
  - attention
  - summary
- low-level evidence should be debug-only or optional

### Proposed Merge Mapping

- `status` -> lifecycle mode
- `interactionKind` -> interaction mode
- `supervisionState` + output semantics -> activity class
- `attentionState` -> attention level
- `attentionReason` -> attention cause
- `semanticPreview` -> user-facing summary
- `terminalSemanticChange` -> evidence only

### Checklist

- [x] Add low-level terminal semantic change detection in `TerminalMultiplexer`
- [x] Thread terminal semantic change through Core snapshot signals
- [ ] Define an internal session assessment model in Core
- [ ] Refactor `SessionManager` to build current legacy fields from the assessment model
- [ ] Separate raw output time from meaningful output time in Core evidence
- [ ] Add stdin-driven interaction partitioning to Core evidence
- [ ] Introduce consolidated API fields alongside legacy fields
- [ ] Move low-level evidence to debug/diagnostic payloads
- [ ] Deprecate redundant legacy fields after client migration

### Phase 1 Scope

Phase 1 should be internal-only:

- build the assessment model in Core
- keep current API fields unchanged
- make all current legacy state derive from the new assessment path
- add tests for merged attention/activity behavior

### Phase 2 Scope

Phase 2 should add new client-facing fields:

- `mode`
- `attention`
- `summary`

while keeping current legacy fields for compatibility.
