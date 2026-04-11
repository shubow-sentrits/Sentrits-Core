# Provider, Session Setup, and Host Identity Alignment

This document is a code-truth implementation plan for aligning session creation inputs and host identity management across the Sentrits runtime, web client, and iOS client.

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

### Host Config

Host config should remain the runtime-owned source of truth for data that belongs to the host rather than to any one session.

Keep in host config:

- `hostId`
- `displayName`
- listener config and TLS paths
- per-provider default command override
- reusable session setup records

Add a host-owned reusable setup record collection, for example:

- `sessionSetups` or `savedSessionSetups`

Each setup record should include:

- `setupId`
- `name`
- `provider`
- `workspaceRoot`
- `title`
- launch mode
- optional `conversationId`
- optional `groupTags`
- command representation
- timestamps such as `createdAt` / `lastUsedAt`

Recommended command representation:

- preserve the existing explicit argv form for exact launches
- add a shell-command form for shell-expanded launches

That yields a runtime-owned model that can represent both:

- exact argv launches
- shell-expanded launches like `codex "$(cat prompt.md)"`

Suggested shape:

- `launchMode: "provider_default" | "argv" | "shell"`
- `commandArgv?: string[]`
- `commandShell?: string`

Rationale:

- Existing `command: string[]` is safe and already wired through the runtime.
- Shell form is needed to preserve expansion patterns from CLI and clients.
- A launch-mode discriminator avoids overloading a single field with ambiguous semantics.

### Session Create Request

Session create should stay additive and request-scoped.

The create request should continue to support direct launch inputs:

- `provider`
- `workspaceRoot`
- `title`
- optional `conversationId`
- optional `groupTags`
- optional explicit launch override

Add optional setup-based fields:

- `setupId`
- optional `saveSetup`
- optional `setupName`

Behavior:

- if `setupId` is provided, runtime resolves the host-owned setup first
- any explicitly provided create-request fields override the referenced setup for that launch
- if `saveSetup` is true, runtime stores the resolved launch inputs back into host-owned setup storage
- if `setupName` is present with `saveSetup`, runtime creates or updates a named reusable setup

This preserves a simple create-session entry point while moving reusable setup ownership to the host.

### Relationship Between Provider, Custom Command, Title, and Workspace

Recommended rules:

- `provider` remains a first-class field in both host-owned setups and create-session requests.
- Host-level provider command override remains the default executable/argv baseline for a provider when no per-session override is supplied.
- A reusable setup may include either:
  - no explicit command override, meaning "use provider default / host provider override"
  - an explicit argv override
  - a shell command override
- `title` and `workspaceRoot` belong in reusable setup because they are part of repeated session bootstrap intent, not just ad hoc request metadata.
- `conversationId` stays optional and request-friendly; if reused today, it can be stored in setup records, but clients should treat reuse carefully because some providers may not want stale conversation identifiers replayed implicitly.

### Host Display Name and Host Identity

Source of truth:

- runtime host config owns `displayName`
- discovery and `host/info` publish it
- clients consume it as host-owned identity metadata
- iOS local alias stays a client-local annotation layered on top

Recommended model:

- allow runtime host display name to be set before daemon enable/bootstrap through CLI config commands
- allow runtime host display name to be changed later through:
  - runtime CLI
  - local host web/admin surface
  - maintained remote clients when authorized against the local-admin/configure-host capability path

The important distinction is:

- `displayName`: remote host-owned, shared identity
- `alias`: local client-owned convenience label

## Concrete Implementation Plan

### Runtime Changes

#### 1. Extend host config schema for reusable session setups

Add to `HostIdentity`:

- `std::vector<SessionSetupRecord> session_setups`

Add a new model in `vibe/store` or `vibe/session`:

- `SessionSetupRecord`
- `SessionLaunchOverride`

Update:

- JSON parsing and persistence in `file_stores.cpp`
- tests for round-trip and backward-compatible loading

Compatibility:

- missing `sessionSetups` must load as empty
- existing `host_identity.json` remains valid

#### 2. Extend create-session request parsing additively

Keep current fields working.

Add support for:

- `setupId`
- `setupName`
- `saveSetup`
- additive launch fields:
  - `commandArgv`
  - `commandShell`
  - or a backward-compatible interpretation of current `command` as `commandArgv`

Recommended request parser behavior:

- preserve current `command` array as compatibility alias for `commandArgv`
- reject requests that specify conflicting launch forms at once
- normalize reused tags the same way current request parsing does

#### 3. Add runtime session-setup resolution

Create a runtime seam that resolves final launch inputs in this order:

1. start from direct create request or referenced setup
2. apply explicit request overrides
3. if no explicit command override remains, apply host provider command override
4. build final `LaunchSpec`

This logic should live outside `http_shared.cpp` so it can be reused by CLI and tests cleanly.

Likely seam:

- `ResolveSessionLaunchInputs(create_request, host_identity)`

#### 4. Add shell-command launch support

Add a launch mode for shell expansion, implemented explicitly by runtime rather than by clients.

Suggested implementation:

- for `commandShell`, launch `/bin/sh -lc <command>` on POSIX
- set `LaunchSpec.provider` from requested provider even when executable becomes `/bin/sh`
- treat shell mode as an explicit override distinct from provider-default/argv mode

This is the minimal additive way to support:

- `codex "$(cat prompt.md)"`

without requiring every client to implement shell parsing/escaping.

#### 5. Add host setup APIs

Additive admin/config routes:

- `GET /host/setups`
- `POST /host/setups`
- `PATCH /host/setups/{setupId}` or `PUT`
- `DELETE /host/setups/{setupId}`

Keep `POST /sessions` as the only session creation route.

Optional fast-follow:

- `POST /sessions` with `saveSetup`
- `POST /sessions/from-setup` is not required and would be redundant

#### 6. Add CLI host-config commands

Add runtime CLI commands for host-owned config that currently has no CLI mutation path.

Suggested commands:

- `sentrits host status`
- `sentrits host set-name <display-name>`
- `sentrits host set-provider-command --provider <provider> -- <command...>`
- `sentrits host clear-provider-command --provider <provider>`
- `sentrits host setup list`
- `sentrits host setup save ...`
- `sentrits host setup delete <setup-id>`

These should use existing local-admin/config APIs rather than bypassing the daemon store.

#### 7. Upgrade `sentrits session start`

Add:

- `--provider`
- `--workspace-root`
- `--conversation-id`
- `--group-tag`
- `--command-argv ...` or `--` passthrough pattern
- `--shell-command "<string>"`
- `--from-setup <setup-id>`
- `--save-setup`
- `--setup-name <name>`

Minimum compatibility rule:

- existing `sentrits session start [--title TITLE] [--attach]` behavior must still work

#### 8. Tests

Add or update tests for:

- host config round-trip with setup records
- create request parsing for setup and launch-mode fields
- precedence rules between setup, provider default override, explicit argv, and explicit shell command
- `commandShell` launching via `/bin/sh -lc`
- compatibility with existing `command` array requests
- host name mutation through CLI/API

### API Changes

Keep existing fields valid and additive.

Recommended `POST /sessions` evolution:

- continue accepting:
  - `provider`
  - `workspaceRoot`
  - `title`
  - `conversationId`
  - `command` as compatibility alias
  - `groupTags`
- add:
  - `setupId`
  - `setupName`
  - `saveSetup`
  - `commandArgv`
  - `commandShell`

Recommended `GET /host/info` evolution:

- keep current fields
- optionally add summarized setup capabilities rather than full setup list

Example additions:

- `capabilities` includes `sessionSetups`
- `sessionSetupCount`

Full setup content should come from dedicated host setup endpoints, not `host/info`.

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
