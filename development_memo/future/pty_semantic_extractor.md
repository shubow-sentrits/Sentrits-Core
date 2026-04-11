# PTY Semantic Extractor

Status: narrow backlog note for the next semantic-preview pass.

This document no longer describes a large standalone subsystem. The current codebase already has the right foundation for a conservative extractor:

- canonical terminal screen snapshots
- bounded scrollback
- per-view viewport snapshots
- supervision, attention, and interaction summaries

The next extractor work should stay small and additive.

## Purpose

Improve observation surfaces with weak but useful hints derived from terminal state.

Primary use cases:

- human supervision
- session inventory previews
- read-only agent observation

Not a use case for this phase:

- lifecycle ownership
- control-path decisions
- provider-specific orchestration

## Recommended Inputs

Use the runtime surfaces that already exist:

- `terminalScreen.visibleLines`
- `terminalScreen.scrollbackLines`
- `terminalScreen.cursorRow`
- `terminalScreen.cursorColumn`
- render revision changes over time
- existing attention and interaction state
- recent file and git signals

Avoid making raw PTY chunk parsing the main product interface.

## Highest-Value Hints

### 1. Awaiting-input hints

Detect likely cases where the terminal is visibly ready for typed input.

Signals:

- stable bottom line
- cursor near bottom
- line-mode interaction already inferred
- prompt-like suffix or approval text on visible lines

### 2. Approval or confirmation hints

Detect simple generic confirmation moments.

Examples:

- `[y/n]`
- `(y/n)`
- `press enter`
- `continue?`
- `approve`

This is the most useful early semantic upgrade for supervision.

### 3. Progress-loop hints

Detect that the terminal is active but repeatedly refreshing a small status area.

Signals:

- frequent render revision changes with low visible text diversity
- repeated bottom-line changes
- repeated overwrite behavior already hinted by terminal traces

### 4. Visible-error hints

Detect likely error-heavy terminal states.

Examples:

- `error:`
- `failed`
- `exception`
- `traceback`

Keep this generic and low-confidence.

## Output Shape

Prefer a compact additive object rather than a large semantic schema.

Reasonable fields:

- `promptVisible`
- `confirmationPromptVisible`
- `progressActive`
- `errorVisible`
- `bottomLinePreview`
- `confidence`

These outputs should feed:

- better `semanticPreview`
- any future observation-specific payload

## Guardrails

- semantic hints are not execution truth
- do not block PTY control or output processing on semantic extraction
- do not overfit to one coding CLI
- do not expand this into session-network v2 work
