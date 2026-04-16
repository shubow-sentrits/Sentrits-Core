# Provider, Session Setup, and Host Identity Alignment

This document is a code-truth implementation plan for aligning session creation inputs and host identity management across the Sentrits runtime, web client, and iOS client. If there's conflict with this doc and code, use code as ground truth.

Scope:

- runtime repo: `Sentrits-Core`
- web repo: `Sentrits-Web`
- iOS repo: `Sentrits-IOS`

Non-goals:

- broad UX redesign
- session-network-v2 planning
- replacing existing create-session or host-info APIs with a breaking rewrite

## Current-State Audit

### Runtime

Runtime facts from code:

- Host identity is persisted in `host_identity.json` via `FileHostConfigStore` in [src/store/file_stores.cpp](/Users/shubow/dev/Sentrits-Core-provider-host-alignment/src/store/file_stores.cpp).
- `HostIdentity` currently persists:
  - `hostId`
  - `displayName`
  - admin listener host/port
  - remote listener host/port
  - TLS certificate/key paths
  - per-provider command overrides for `codex` and `claude`
  - source: [include/vibe/store/host_config_store.h](/Users/shubow/dev/Sentrits-Core-provider-host-alignment/include/vibe/store/host_config_store.h)
- A stable `hostId` is generated lazily by `EnsureHostIdentity()` if one does not exist. `displayName` falls back to `Sentrits Host`.
  - source: [src/store/host_config_store.cpp](/Users/shubow/dev/Sentrits-Core-provider-host-alignment/src/store/host_config_store.cpp)
- `CreateLocalAuthServices()` calls `EnsureHostIdentity()` during daemon auth/bootstrap, so host identity exists by the time the daemon is running.
  - source: [src/net/local_auth.cpp](/Users/shubow/dev/Sentrits-Core-provider-host-alignment/src/net/local_auth.cpp)
- `POST /host/config` is the only mutation path for host-owned display name and provider command overrides. It updates persisted host identity and returns `host/info`.
  - source: [src/net/http_shared.cpp](/Users/shubow/dev/Sentrits-Core-provider-host-alignment/src/net/http_shared.cpp)
- `GET /host/info` returns `displayName`, listener addresses/ports, TLS info, and `providerCommands`.
  - source: [src/net/json.cpp](/Users/shubow/dev/Sentrits-Core-provider-host-alignment/src/net/json.cpp)
- Session create requests currently accept:
  - `provider`
  - `workspaceRoot`
  - `title`
  - optional `conversationId`
  - optional explicit `command` argv array
  - optional `groupTags`
  - source: [src/net/request_parsing.cpp](/Users/shubow/dev/Sentrits-Core-provider-host-alignment/src/net/request_parsing.cpp)
- If create-session omits `command`, runtime applies the host-level provider override from `HostIdentity`. If create-session includes `command`, the request wins.
  - source: [src/net/http_shared.cpp](/Users/shubow/dev/Sentrits-Core-provider-host-alignment/src/net/http_shared.cpp)
- The runtime launch model still has no persisted reusable session setup object. `SessionManager::CreateSession()` builds launch metadata from request inputs only, then optionally swaps the executable/args from `command_argv`.
  - source: [src/service/session_manager.cpp](/Users/shubow/dev/Sentrits-Core-provider-host-alignment/src/service/session_manager.cpp)
- `ProviderConfig` is only the hard-coded provider default executable (`codex` / `claude`) plus default args/env. It does not model reusable named setups or host-owned launch presets.
  - source: [src/session/provider_config.cpp](/Users/shubow/dev/Sentrits-Core-provider-host-alignment/src/session/provider_config.cpp)
- `BuildLaunchSpec()` only combines session metadata, provider config, extra args, and terminal size. It does not perform shell expansion.
  - source: [src/session/launch_spec.cpp](/Users/shubow/dev/Sentrits-Core-provider-host-alignment/src/session/launch_spec.cpp)

Runtime CLI facts:

- `sentrits session start` and the compatibility alias `session-start` are hard-coded to:
  - provider `codex`
  - workspace root = current working directory
  - title from `--title` or positional/default fallback
- CLI create flow does not expose:
  - `--provider`
  - `--workspace-root`
  - `--conversation-id`
  - `--command`
  - setup reuse
  - host-name mutation
- CLI host awareness is read-only through `sentrits host status`, which prints `hostId` and `displayName`.
  - sources: [src/main.cpp](/Users/shubow/dev/Sentrits-Core-provider-host-alignment/src/main.cpp), [src/cli/daemon_client.cpp](/Users/shubow/dev/Sentrits-Core-provider-host-alignment/src/cli/daemon_client.cpp)

Implications:

- Runtime already treats provider command override as host-owned state.
- Runtime already treats display name as host-owned state.
- Runtime does not yet treat reusable session setup as host-owned state.
- CLI is the least aligned surface.

### Web

Web facts from code:

- Web create-session payload already supports:
  - `provider`
  - `workspaceRoot`
  - `title`
  - optional `conversationId`
  - optional `command`
  - optional `groupTags`
  - source: [src/lib/runtime-api.ts](</Users/shubow/dev/Sentrits-Web/src/lib/runtime-api.ts>)
- Web UI persists last-used create inputs in browser local storage, not on the host:
  - provider
  - title
  - workspace root
  - conversation id
  - command string
  - source: [src/App.tsx](</Users/shubow/dev/Sentrits-Web/src/App.tsx>)
- Web tokenizes the command input by whitespace using `split(/\s+/)`.
  - source: [src/App.tsx](</Users/shubow/dev/Sentrits-Web/src/App.tsx>)
- That tokenization cannot represent shell expressions correctly, so examples like `codex "$(cat prompt.md)"` are not preserved as intended.
- Web does not call `POST /host/config`, does not expose host-owned provider overrides, and does not expose runtime host display-name editing.
- Web stores paired/discovered hosts locally and treats `displayName` as host-provided read-only data merged into local records.
  - source: [src/lib/host-store.ts](</Users/shubow/dev/Sentrits-Web/src/lib/host-store.ts>)

Implications:

- Web is ahead of CLI on raw create-session coverage.
- Web is behind runtime on host-owned config management.
- Web currently implements session setup memory on the client instead of on the host.

### iOS

iOS facts from code:

- iOS `HostClient.createSession()` only sends:
  - `provider`
  - `workspaceRoot`
  - `title`
  - `groupTags`
  - source: [SentritsIOS/Services/HostClient.swift](</Users/shubow/dev/Sentrits-IOS/SentritsIOS/Services/HostClient.swift>)
- iOS create flow does not send:
  - `conversationId`
  - `command`
- iOS create UI exposes:
  - workspace root
  - title
  - provider picker
  - group tags
  - source: [SentritsIOS/Views/InventoryView.swift](</Users/shubow/dev/Sentrits-IOS/SentritsIOS/Views/InventoryView.swift>)
- `CreateSessionInput` likewise contains no command or conversation fields.
  - source: [SentritsIOS/Services/InventoryStore.swift](</Users/shubow/dev/Sentrits-IOS/SentritsIOS/Services/InventoryStore.swift>)
- iOS consumes `hostId` and `displayName` from discovery and `host/info`, and it verifies host identity consistency when refreshing inventory.
  - sources: [SentritsIOS/Models/HostModels.swift](</Users/shubow/dev/Sentrits-IOS/SentritsIOS/Models/HostModels.swift), [SentritsIOS/Services/InventoryStore.swift](</Users/shubow/dev/Sentrits-IOS/SentritsIOS/Services/InventoryStore.swift)
- iOS has a local per-device `alias` for saved hosts. That alias is not the runtime host display name.
- iOS does not call `POST /host/config`, does not expose provider command overrides, and does not expose host display-name editing.

Implications:

- iOS is the most limited create-session surface.
- iOS already distinguishes remote host identity from local saved-host presentation.
- iOS should keep local aliases, but host display name remains runtime-owned.

### Where Surfaces Differ Today

- Provider selection:
  - runtime API: supported
  - web: supported
  - iOS: supported
  - runtime CLI: not supported
- Custom command override at create time:
  - runtime API: supported as explicit argv array
  - web: supported, but parsed unsafely from a whitespace-split string
  - iOS: not supported
  - runtime CLI: not supported
- Host-owned provider default command override:
  - runtime: supported and persisted
  - web: visible only indirectly via `host/info`, not editable or reused
  - iOS: not surfaced
  - CLI: not surfaced
- Reusable setup/preset:
  - runtime: not implemented
  - web: only client-local sticky form state
  - iOS: not implemented
  - CLI: not implemented
- Host display-name editing:
  - runtime local admin API: supported
  - CLI: read-only
  - web: not supported
  - iOS: not supported

## Proposed Source-of-Truth Model

### Why "setup" is the wrong concept

The original implementation built a named `sessionSetups` model — CRUD-managed, named presets, explicit save/delete. That is the wrong design.

The correct model is one host-owned bounded launch-record store. The key distinctions:

- **Do not** build a separate "saved setups" model and a separate "recent history" model.
- **Do** converge on one bounded store of recent launches, auto-populated when sessions are created.
- The host trims the store to `maxLaunchRecords` (default 50). Oldest entries fall off automatically.
- If a client wants to pin or favorite a record, that is client-side behavior, not a second host-owned model.

The word "setup" is replaced by "launch record" in all new surfaces. Where existing API compatibility requires a temporary bridge, it is called out explicitly. There is no remaining use of `sessionSetups` in the runtime.

### Host Config

Host config remains the runtime-owned source of truth for data that belongs to the host.

Contents:

- `hostId`
- `displayName`
- listener config and TLS paths
- per-provider default command override (`providerCommands`)
- `launchRecords`: bounded array of recent `LaunchRecord` entries (most-recent-first)
- `maxLaunchRecords`: configurable cap, default 50

Each `LaunchRecord` contains:

- `recordId` (auto-generated `rec_<hex>`)
- `provider`
- `workspaceRoot`
- `title`
- `launchedAtUnixMs` (unix epoch ms, set at create time)
- optional `conversationId`
- optional `groupTags`
- optional `commandArgv` (exact argv override)
- optional `commandShell` (shell-expanded command, executed via `/bin/sh -lc`)

No `name` field. Records are auto-managed history, not named presets.

Backward compatibility: existing `host_identity.json` files with the old `sessionSetups` key load as empty `launchRecords`. The `maxLaunchRecords` field loads with default 50 when absent.

### Session Create Request

The create request stays additive and request-scoped.

Accepted fields:

- `provider`
- `workspaceRoot`
- `title`
- optional `conversationId`
- optional `groupTags`
- optional `commandArgv` (exact argv; `command` array is a compatibility alias)
- optional `commandShell` (shell-expanded command string)
- optional `recordId` (reference a previously saved launch record as defaults)

Behavior:

- if `recordId` is provided, runtime resolves that `LaunchRecord` first
- any explicitly provided create-request fields override the referenced record for that launch
- after a successful session create, the runtime auto-saves a new `LaunchRecord` and trims to `maxLaunchRecords`

There is no `saveSetup`, `setupName`, or `setupId`. Records are auto-saved; there is no manual save flow.

### Relationship Between Provider, Custom Command, Title, and Workspace

- `provider` is a first-class field in both `LaunchRecord` and create-session requests.
- Host-level provider command override remains the default executable/argv baseline when no per-session override is supplied.
- A `LaunchRecord` may contain no command override (use provider default), an exact argv override, or a shell command override.
- `title` and `workspaceRoot` are stored in `LaunchRecord` as part of repeated session intent.
- `conversationId` is stored in `LaunchRecord` only when provided at create time. Clients should be cautious about replaying stale conversation identifiers.

### Host Display Name and Host Identity

Source of truth:

- runtime host config owns `displayName`
- discovery and `host/info` publish it
- clients consume it as host-owned identity metadata
- iOS local alias stays a client-local annotation layered on top

The important distinction:

- `displayName`: runtime-owned, shared across all clients
- `alias`: client-local convenience label

## Concrete Implementation Plan

### Runtime Changes

#### 1. Host config schema — `LaunchRecord` replacing `SessionSetupRecord`

`HostIdentity` now contains:

- `std::vector<LaunchRecord> launch_records`
- `std::size_t max_launch_records` (default 50)

`LaunchRecord` fields: `record_id`, `provider`, `workspace_root`, `title`, `launched_at_unix_ms`, `conversation_id`, `group_tags`, `command_argv`, `command_shell`.

No `name` field. No manual save semantics.

JSON persistence: `launchRecords` array, `maxLaunchRecords` integer. Old `sessionSetups` key silently ignored on load (backward compat).

#### 2. Create-session request parsing

`CreateSessionRequestPayload` uses `record_id` (not `setup_id`). No `save_setup` or `setup_name`.

`command` array remains a compatibility alias for `commandArgv`.

`ParseHostSessionSetupRequest` has been removed.

#### 3. Auto-save on session create

`HandleCreateSessionRequest` calls `AutoSaveLaunchRecord` after a successful `CreateSession`. This:

1. Builds a `LaunchRecord` from the resolved request inputs
2. Prepends it to `launch_records` (most-recent-first)
3. Trims to `max_launch_records`
4. Saves the updated identity

No manual POST endpoint for creating records.

#### 4. Shell-command launch support

`commandShell` is already accepted in the create-session request and stored in `LaunchRecord`. The runtime executes `/bin/sh -lc <command>` for shell-mode launches (wired through `LaunchSpec`).

#### 5. Host launch-record APIs

- `GET /host/records` — returns the bounded launch-record list
- `DELETE /host/records/{recordId}` — removes a specific record

No `POST /host/records`. Records are created only via session create.

Legacy paths `GET /host/setups` and `POST /host/setups` are removed. Clients must use `/host/records`.

#### 6. CLI host-config commands

New commands added:

- `sentrits host set-name <display-name>`
- `sentrits host set-provider-command --provider codex|claude -- <command> [args...]`
- `sentrits host clear-provider-command --provider codex|claude`
- `sentrits records list [--json]`
- `sentrits records show [--json] <record-id>`

Removed: `sentrits setup list`, `sentrits setup show`.

#### 7. `sentrits session start` flags

Available:

- `--provider codex|claude`
- `--workspace PATH`
- `--title TITLE`
- `--record RECORD_ID` (relaunch from a previous record as defaults)
- `--shell-command COMMAND`
- `--attach`

No `--save-setup` or `--setup-name`. Records are auto-saved.

### API Changes

`POST /sessions`:

- continue accepting: `provider`, `workspaceRoot`, `title`, `conversationId`, `command` (compat alias), `groupTags`, `commandArgv`, `commandShell`
- add: `recordId` (optional relaunch reference)
- remove: `setupId`, `saveSetup`, `setupName`

`GET /host/info`:

- `sessionSetupCount` replaced by `launchRecordCount`
- `capabilities` includes `launchRecords` instead of `sessionSetups`

New record endpoints:

- `GET /host/records`
- `DELETE /host/records/{recordId}`

Removed endpoints: `GET /host/setups`, `POST /host/setups`, `DELETE /host/setups/{setupId}`.

### Web Changes

#### 1. Stop treating setup persistence as browser-only truth

Retain local sticky draft state for convenience, but move reusable setup persistence to the runtime.

Add web flows for:

- loading host-owned setups
- creating a session from a setup
- saving the current form as a setup

#### 2. Replace whitespace-split command parsing

Current `parseCommand()` is insufficient for shell expressions.

Web should expose an explicit launch mode picker:

- `Provider default`
- `Exact argv`
- `Shell command`

Behavior:

- `Provider default`: omit per-session command override
- `Exact argv`: collect structured argv tokens, or keep a simple advanced editor that serializes to `commandArgv`
- `Shell command`: send `commandShell` as raw string

Do not keep tokenizing shell input by whitespace.

#### 3. Expose host-owned config where appropriate

For local-admin or authorized host-config workflows, add:

- host display-name editing
- provider default command override editing
- setup list management

Remote paired web clients should only expose these controls when the runtime authorizes host-configuration access.

### iOS Changes

#### 1. Extend create-session model

Add to `CreateSessionInput`:

- optional `conversationId`
- launch mode
- optional `commandArgvText` or structured argv editor
- optional `commandShell`
- optional setup reference / setup-save intent

Update `HostClient.CreateSessionPayload` accordingly.

#### 2. Add setup-aware create UI

In the create-session sheet:

- allow choosing an existing host-owned setup
- allow editing derived fields before launch
- allow saving current fields as a reusable setup

#### 3. Preserve local alias semantics

Do not replace local `alias`.

iOS should display:

- host-owned display name from runtime
- optional local alias layered on top

#### 4. Add host config management

Add authorized host-config UI for:

- display name
- provider default command overrides
- setup management

This should remain separate from local alias editing.

### Migration and Compatibility Notes

- Existing `host_identity.json` files remain valid. New setup storage loads as empty when absent.
- Existing `POST /sessions` clients remain valid.
- Existing `command: string[]` remains supported as compatibility alias for argv mode.
- Existing provider default override behavior remains unchanged when no setup or per-session override is used.
- Existing iOS aliases remain client-local and should not be migrated into runtime display name.
- Existing web local form persistence remains useful as draft state but should no longer be the reusable-setup truth.

## Recommended Delivery Sequence

1. Runtime storage and create-session resolution seams.
2. Runtime shell-command support.
3. Runtime host setup APIs.
4. Runtime CLI parity for provider, command, setup, and host display-name mutation.
5. Web setup and launch-mode migration.
6. iOS setup and launch-mode migration.

This order keeps the runtime as source of truth and lets clients adopt incrementally.

## Explicit Open Questions

These are the remaining questions after code inspection, and they are narrow implementation questions rather than product-shape uncertainty.

- Should reusable setup records store `conversationId` by default, or should clients opt into saving it explicitly to avoid accidental conversation reuse?
- Should remote paired clients be allowed to call host-config mutation endpoints directly, or should host-config mutation remain local-admin-only in the first phase and remote clients only consume setup/display-name reads?
- For exact argv entry in web and iOS, is a plain newline-delimited token editor sufficient for v1, or is a richer structured token editor required immediately?

## Summary Recommendation

The clean source-of-truth model is:

- runtime-owned host config for `displayName`, provider default commands, and reusable session setups
- additive session-create requests that can either reference a saved setup or provide direct launch inputs
- explicit launch modes so both exact argv and shell-expanded commands are supported
- client-local aliases and sticky drafts remain convenience layers, not authority

That keeps current runtime behavior intact, fixes CLI lag, avoids breaking existing clients, and gives web and iOS a consistent path to "new session from previous setup" without inventing separate client-side preset systems.
