# AGENTS.md

Guidelines for AI-assisted development (Codex, ChatGPT, etc.)

## Documentation

All project documentation lives in `wiki/`.

New sessions should read the onboarding guide at `.deepseek/instructions.md`
for a recommended reading order. At minimum, read these pages first to
understand the system:

- `wiki/Architecture-Overview.md` — system layout
- `wiki/Contextual-Modeling-Workflow.md` — binding UX pattern
- `wiki/IPC-Protocol.md` — communication contract
- `wiki/Topological-Naming-Problem.md` — the project's mantra

When adding new documentation, create the file in `wiki/` and
add a link from `wiki/Home.md`.

## Cross-Platform

PolySmith must compile and run on **Windows (MSVC)** and **POSIX (Linux / macOS, GCC / Clang)**.
Every change must work on both families or be guarded with `#ifdef` / `#[cfg]`.

When writing C++:
- Prefer standard C++ over platform-specific APIs. Use `<cmath>` not `<math.h>`.
- `M_PI` is not standard — define `_USE_MATH_DEFINES` before including `<cmath>` on MSVC,
  or use a project-wide constant.
- MSVC treats `const char*` ↔ `unsigned char*` as an error (not a warning). Match types exactly.

When writing Rust:
- Test with `cargo check` (not just `cargo build`) — it catches platform-gated issues faster.
- `windows-sys` crate types may move between minor versions; prefer `core::` paths when available.

When touching the build:
- OCCT DLLs live at `third_party/occt-install/win64/vc14/bin` on Windows. The Tauri spawn code
  prepends this to `PATH` automatically. If you change how the core is launched, preserve this.
- FreeType is a git submodule at `third_party/freetype`. The configure script builds it.
  Do not add freetype sources inside the OCCT tree.
- `CMAKE_RUNTIME_OUTPUT_DIRECTORY` behaves differently with multi-config generators (MSBuild).
  Prefer per-config output paths or keep the default and update `build.rs` to match.

Reference: `wiki/Implementation-Log.md` (see "Windows Build Fixes" under 2026-05-28).

## Core Principles

- Do not make architectural changes without explicit approval.
- Keep changes minimal and scoped.
- Prefer clarity over cleverness.
- Do not introduce unnecessary abstractions.
- Do not introduce new dependencies without justification.

## Code Style

- Write explicit, readable code.
- Avoid magic behavior and hidden state.
- Prefer simple functions over complex class hierarchies.
- Keep files focused and small.

## Project Boundaries

- React (UI) must NOT own CAD state.
- CAD state, geometry, and modeling logic live ONLY in the native core.
- Communication between UI and core must go through the IPC protocol.

## IPC Rules

- Do not bypass the protocol layer.
- All communication must follow the defined schema.
- Always update protocol docs when changing messages.
- When IPC commands, payloads, responses, or CAD-agent workflows change, also
  update `wiki/AI-CAD-Command-Language.md` so coding agents keep
  the app's CAD command language current.

## Workflow Expectations

When implementing a task:

1. Explain the plan before writing code
2. Show which files will be changed
3. Keep diffs small and reviewable
4. Avoid unrelated refactors
5. Add comments where intent is not obvious

## Branch Workflow

PolySmith uses a feature-branch workflow:

- `dev` is the default development branch and the base for everyday work.
- `main` is the production/stable release branch. Do not use it for feature
  work unless explicitly asked to prepare a release or hotfix.
- At the start of every prompt that may change files, check the current branch
  and working tree state before editing.
- Start each implementation from the latest `dev`: fetch/sync `dev`, then
  create a new feature branch from it.
- Keep feature branches scoped to one implementation or fix.
- Merge feature branches back into `dev` through a pull request.
- Use squash-merge for feature PRs so `dev` keeps a readable history.
- After a PR is approved and merged, delete the merged feature branch locally
  and on the remote.

### GitHub PR Flow

- Prefer the GitHub CLI (`gh`) for pull request work when it is available and
  authenticated.
- Before opening a PR, push the feature branch and confirm the PR base is
  `dev`, not `main`.
- Open implementation PRs as draft until the change has been tested and is
  ready for review.
- Include a concise PR summary, test notes, and any known risks or follow-up
  work in the PR description.
- Before review or merge, sync the feature branch with the latest `dev` and
  resolve conflicts on the feature branch.
- After approval and passing checks, squash-merge the PR into `dev`.
- Confirm GitHub deleted the remote feature branch after merge, or delete it
  with `gh pr merge --delete-branch` / `git push origin --delete <branch>`.
- Delete the local feature branch after returning to an updated `dev`.

## Topological Naming Problem (TNP)

**This is the project's mantra.** Never introduce a feature that stores a
naked OCCT topology index and trusts it across recomputes. Every new
feature kind that references 3D geometry must re-resolve its references
against live body shapes on every recompute. When resolution fails,
degrade gracefully with `dependency_broken` + a warning — never crash or
produce garbage.

Full strategy: `wiki/Topological-Naming-Problem.md`

## Testing

- Add tests for non-trivial logic
- Do not break existing behavior without explanation

## Forbidden Behaviors

- No large "vibe-coded" rewrites
- No silent refactoring across modules
- No mixing UI logic with CAD logic
- No bypassing architecture for speed

## UX Pattern

PolySmith follows a **contextual modeling workflow** for all modeling
features: select inputs → invoke action → floating context panel with real
geometry preview → confirm or cancel.

This pattern is documented in `wiki/Contextual-Modeling-Workflow.md`
and is binding for new features.

## UI Copy Rules

- **Never expose internal ids in the UI.** Entity ids, feature ids,
  point ids, etc. are implementation details. User-visible copy
  describes things by their kind ("Line", "Circle"), their count
  ("3 selected"), or by user-meaningful labels ("Sketch on XY").
  Ids are allowed in debug overlays gated behind a flag, never in
  default UI.
- When adding or changing user-visible UI labels, put the English string in
  `apps/desktop-ui/src/i18n/en.json` and render it through the translation
  layer. Do not hardcode new labels in React components. You do not need to
  translate every other locale in the same change; make the label translatable
  and let missing locales fall back to English.

## UI Theme Rules

- Do not hardcode colors in React components or viewport utilities.
- Use existing CSS/theme variables, or add a new token to every theme JSON file
  under `apps/desktop-ui/src/config/themes/` before consuming it.
- Keep theme-specific palette values, including Catppuccin colors, inside the
  theme JSON files. Components should remain theme-agnostic.
- When adding a third-party palette theme, preserve clear attribution in
  `wiki/Design-System.md` and keep user-visible theme names properly credited.

## Philosophy

PolySmith is built to be **understandable and maintainable by humans first**.

AI is a tool, not the owner of the codebase.
