# Sentrits-Core Docs Realignment Prompt

Work in `Sentrits-Core`.

Goal:

Realign the `Sentrits-Core` documentation set to the current product and codebase. Use the code as the source of truth. This is a documentation review, cleanup, and MVP realignment pass, not a speculative planning exercise.

## Primary Objectives

1. Update the full-picture architecture docs
- Document the current end-to-end architecture across:
  - runtime / daemon
  - CLI
  - web client
  - iOS client
- Explain the real interaction model between runtime and clients:
  - pairing/auth
  - host discovery
  - session inventory
  - observer flow
  - controller flow
  - snapshot/bootstrap flow
  - local attach/control vs remote control
- Keep the runtime itself as the main focus, but clearly show where clients fit.

2. Remove stale docs
- Identify outdated docs, rewrite-era docs, abandoned plans, stale prompts, and obsolete checklists.
- Remove docs that no longer reflect the current product direction or current code.
- If a doc still has value but is stale, update it instead of removing it.

3. Update existing docs using code as ground truth
- Audit existing docs against the current implementation.
- Correct architectural claims, API descriptions, packaging notes, session model descriptions, and client/runtime interaction notes.
- Prefer concise and accurate descriptions over aspirational or speculative language.

4. Update the MVP checklist
- Rewrite the current MVP checklist to match the actual current product state.
- Clearly separate:
  - current working surface
  - near-term next-release MVP
  - explicitly non-goals / later work

## Additional Instructions

1. Client memos in `development_memo/`
- If client-specific memo docs are duplicated by better/current docs in the client repos, remove them from `Sentrits-Core/development_memo/`.
- `Sentrits-Core` should not keep stale shadow documentation for maintained clients if the source docs now live in:
  - `https://github.com/shubow-sentrits/Sentrits-Web`
  - `https://github.com/shubow-sentrits/Sentrits-IOS`

2. Keep `get_started.md` clean
- `get_started.md` should stay focused on:
  - build
  - test
  - basic runtime usage
  - CLI usage
- Do not overload it with product strategy, architecture deep-dives, or client-specific detail.
- Treat it as the clean onboarding and operator quickstart doc.

3. Treat `README.md` as the GitHub landing page
- `README.md` should be a real landing page for the project.
- It should focus on:
  - product vision
  - future vision where appropriate
    - see `development_memo/future/` for input
  - high-level architecture and design introduction
  - current feature set
  - future design direction at a high level
  - links to deeper docs
- Include references to key docs such as:
  - vibe coding doc
  - architecture docs
  - API docs
  - packaging docs
  - getting started docs
- Keep it attractive, high-level, and useful for GitHub readers.
- Do not turn `README.md` into a deep engineering memo.

## Documentation Rules

### Source of truth
- Code wins over docs.
- If docs conflict with implementation, update docs to match implementation.
- Do not preserve stale wording just because it existed already.

### Cross-repo references
- For references to files or repos outside this repo, use GitHub URLs only.
- Repo reference example:
  - `https://github.com/shubow-sentrits/Sentrits-Core`
- File reference example:
  - `https://github.com/shubow-sentrits/Sentrits-Core/blob/main/src/main.cpp`

### In-repo references
- For files inside `Sentrits-Core`, use relative paths only.
- Do not use absolute local filesystem paths.
- Do not embed local machine-specific paths.

## Scope

Focus on these areas first:

- repo root docs
- `development_memo/`
- `prompts/`
- `get_started.md`
- `README.md`
- any architecture, API, packaging, runtime-shape, and MVP docs
- any stale rewrite or migration notes

## Expected Deliverables

1. Updated architecture docs
- Current runtime/client full-picture architecture
- Runtime responsibilities vs client responsibilities
- Current packaging/deployment direction
- Current discovery/pairing/session/control model

2. Updated MVP docs
- current working surface
- next-release MVP
- known gaps / non-goals

3. Updated landing and onboarding docs
- `README.md` as landing page
- `get_started.md` as clean build/test/CLI onboarding

4. Stale-doc cleanup
- remove or rewrite outdated docs/prompts/checklists
- remove duplicated client memos when better client-repo docs already exist

5. Short change summary
- list:
  - docs updated
  - docs removed
  - any remaining ambiguous areas that need later follow-up

## Writing Style

- Prefer high-signal, factual, current-state documentation.
- Avoid rewrite-era language, roadmap fluff, or speculative architecture unless clearly labeled as future work.
- Keep docs readable for engineers onboarding to the runtime and its client integration surface.

## Important Constraint

Do not redesign the system in docs. Document the current system first.
Where future direction is needed, keep it short and clearly separated from current-state documentation.
