# Sentrits Session Signal Map

Status: current implementation plus a small next-step expansion list.

This document now reflects the signal model that is actually present in code. It is not a broad future taxonomy.

## Implemented Signal Inputs

The current runtime derives supervision and node-summary state from four concrete sources:

### 1. Session lifecycle and controller state

Used in `SessionManager` to infer:

- supervision state
- attention state and reason
- interaction kind

Current derived cases:

- `Error` -> attention `intervention` / `session_error`
- `AwaitingInput` -> attention `action_required` / `awaiting_input`
- controller present while running -> `interactive_line_mode`
- exited or error after output -> `completed_quickly`

### 2. PTY activity and sequence state

Used today for:

- recent output timing
- current sequence watermark
- `running_non_interactive` fallback when the session is running and has produced output
- stopped/active/quiet supervision inference

What exists:

- `lastOutputAtUnixMs`
- `currentSequence`
- canonical `terminalScreen`
- canonical `terminalViewport` when requested

What does not exist yet:

- reliable fullscreen inference
- semantic parsing of raw PTY chunks as a required layer

### 3. Workspace file-change signals

Used today for:

- recent file-change count
- recent file-change list on snapshots
- attention `info` / `workspace_changed`
- semantic preview `Workspace changed`

### 4. Git state signals

Used today for:

- `gitDirty`
- git branch
- modified/staged/untracked counters
- attention `info` / `git_state_changed`
- semantic preview `Git state changed`
- fallback semantic preview `Workspace dirty`

## Implemented Derived Outputs

The current code derives the following runtime-owned outputs:

### Summary-level outputs

- `supervisionState`
- `attentionState`
- `attentionReason`
- `interactionKind`
- `semanticPreview`
- `recentFileChangeCount`
- git branch and dirty counters
- `nodeSummary`

### Snapshot-level outputs

- all of the above through `signals` and `nodeSummary`
- `terminalScreen`
- additive `terminalViewport`
- recent file-change list
- git file lists

### Trace outputs

- `core.node:summary.transition`
- optional file logging through `SENTRITS_SESSION_SIGNAL_TRACE_PATH`

## Current Gaps

The current signal model is intentionally conservative.

Missing or sparse areas:

- no fullscreen or alternate-screen interaction classification
- no prompt or approval detection
- no progress-loop or spinner classification
- no explicit visible-error detection
- no dedicated observation payload that packages these fields for agent consumption

## Small Next-Step Expansion List

The next signal additions should stay low-risk and screen-based.

Good candidates:

### Terminal-state cues

- bottom visible lines
- whether the cursor is near the bottom visible region
- whether one line is repeatedly rewritten
- whether alternate-screen activity was seen recently

### Derived observation hints

- likely awaiting typed input
- likely confirmation or approval prompt visible
- likely progress loop active
- likely visible error region

These should feed:

- better `semanticPreview`
- additive observation payloads

They should not become lifecycle truth.
