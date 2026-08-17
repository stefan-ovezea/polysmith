# CLAUDE.md

This is the **single authoritative agent instruction file** for this repository.
All AI coding agents (Claude Code, Codex, DeepSeek, etc.) should read this file
first at the start of every session. It replaces what was previously scattered
across AGENTS.md, CONTRIBUTING.md, and `.deepseek/instructions.md`.

The other files still exist for human contributors, but agents should treat
this file as the binding instruction set.

## Project Overview

PolySmith is a local-first desktop CAD application for hobbyist 3D-printing workflows — single-part parametric modeling with a strict UI/CAD boundary. Licensed AGPL-3.0-or-later.

## Session Onboarding

At the start of every session, check the current branch and working tree state.
Then read these wiki pages to understand the system. They are the canonical
documentation.

### First session — read in this order

1. **[Core-UI Design Principles](wiki/Core-UI-Design-Principles.md)** — **READ FIRST.** What belongs in core vs UI.
2. **[Architecture Overview](wiki/Architecture-Overview.md)** — UI / Tauri / C++ core layout
3. **[Contextual Modeling Workflow](wiki/Contextual-Modeling-Workflow.md)** — the binding UX pattern every feature follows
4. **[IPC Protocol](wiki/IPC-Protocol.md)** — how UI and core communicate
5. **[Topological Naming Problem](wiki/Topological-Naming-Problem.md)** — the project's mantra

### Return-session quick-ref

- **[Repository Map](wiki/Repository-Map.md)** — directory layout
- **[AI CAD Command Language](wiki/AI-CAD-Command-Language.md)** — IPC command reference for agents
- **[Implementation Log](wiki/Implementation-Log.md)** — what's shipped, including platform-specific build fixes
- **[V1 Roadmap](wiki/V1-Roadmap.md)** — current priorities

### Active Work

Check [`.deepseek/active-task.md`](.deepseek/active-task.md) for the current
implementation status, completed phases, key files changed, and the next-session
checklist. This is the living task tracker — read it before starting any code
work to avoid redoing or breaking completed features.

## Build & Development Commands

```bash
# First-time bootstrap (submodules → JS deps → OCCT → CAD core)
pnpm bootstrap

# Run the desktop app (Vite + Tauri + CAD core)
pnpm dev

# Run UI only (no Tauri, no CAD core — browser dev)
pnpm ui:dev

# Rebuild the C++ CAD core after changes
pnpm core:rebuild                    # configure + build
pnpm core:build                      # build only (if CMake cache is current)

# Run every C++ test suite (the regression safety net — run before committing)
pnpm test:core

# Rebuild OpenCascade (rarely needed)
pnpm occt:rebuild

# Type-check the UI
pnpm --filter desktop-ui exec tsc --noEmit

# Release build (produces .app / .exe)
pnpm build:release
```

**Prerequisites:** pnpm 9.x, Node 20.x, Rust stable, CMake 3.20+, C++20 compiler. Linux also needs GTK3/WebKit2GTK dev headers. Windows needs Visual Studio 2022 with C++ workload and vcpkg for Boost + Eigen3.

## Architecture

Three layers with strict ownership boundaries:

| Layer | Tech | Owns |
|---|---|---|
| **UI** | React + TypeScript (Vite) | Presentation, user input, command dispatch |
| **Shell** | Tauri (Rust) | Window management, file dialogs, spawning the C++ core process |
| **CAD Core** | C++20 + OpenCascade 7.8 | All geometry, feature history, document state, modeling operations |

The CAD core runs as a **separate process** spawned by Tauri. Communication is via newline-delimited JSON over `stdin`/`stdout`. The core is the single source of truth; React never owns CAD state.

```
React UI  ──IPC (JSON)──>  Tauri (Rust)  ──stdin/stdout──>  C++ CAD Core
  (TS)       <──          spawns/manages      <──            (OCCT)
```

### Key Architectural Rules

- **React owns presentation and user intent only.** CAD state, geometry, feature history, and modeling logic live ONLY in the native C++ core.
- **All communication goes through the IPC protocol.** Never bypass it. Protocol schemas live in `protocol/schema/`.
- **Core sends DOCUMENT STATE, not INTERACTION STATE.** If it moves with the mouse, it's UI. If it saves to a file, it's core. Snap candidates, drag previews, and hover highlights are UI-side concerns.
- **TNP (Topological Naming Problem) is the project's mantra:** Never store a naked OCCT topology index and trust it across recomputes. Every feature referencing 3D geometry must re-resolve against live body shapes on every recompute. On failure, degrade with `dependency_broken` + warning — never crash.
- **Contextual modeling workflow** is the binding UX pattern for all features: select inputs → invoke action → floating context panel with real geometry preview → confirm (Enter) or cancel (Escape, with undo).

### Diagnostics Rule

- **ALL diagnostic output MUST go through the structured logger.** This is a Tauri desktop app — there is no terminal. Never use `fprintf(stderr, ...)`, `printf`, `std::cerr`, `std::cout`, or `console.log`. Always use `polysmith::core::log_info/log_warn/log_error/log_debug("tag", "message")` in C++, or `addMessage` / `addLogEntry` in TypeScript. These appear in the in-app Logs panel the user can actually see. This is non-negotiable — the user cannot see stderr.

### No Untested Commits

- **Never commit code that has not been exercised.** Diagnostic logging, speculative fixes, debug scaffolding — all of it stays uncommitted until the user has run the app and confirmed the change works correctly. A successful compile is not enough; the change must be observed doing its job at runtime. Commit only after the user confirms the fix resolves the issue.

### UI Copy Rules

- **Never expose internal ids in the UI.** Entity ids, feature ids, point ids, etc. are implementation details. User-visible copy describes things by their kind ("Line", "Circle"), their count ("3 selected"), or by user-meaningful labels ("Sketch on XY"). Ids are allowed in debug overlays gated behind a flag, never in default UI.
- When adding or changing user-visible UI labels, put the English string in `apps/desktop-ui/src/i18n/en.json` and render it through the translation layer. Do not hardcode new labels in React components. You do not need to translate every other locale in the same change; make the label translatable and let missing locales fall back to English.

### UI Theme Rules

- Do not hardcode colors in React components or viewport utilities.
- Use existing CSS/theme variables, or add a new token to every theme JSON file under `apps/desktop-ui/src/config/themes/` before consuming it.
- Keep theme-specific palette values, including Catppuccin colors, inside the theme JSON files. Components should remain theme-agnostic.
- When adding a third-party palette theme, preserve clear attribution in `wiki/Design-System.md` and keep user-visible theme names properly credited.

### Workflow Expectations

When implementing a task:
1. Explain the plan before writing code
2. Show which files will be changed
3. Keep diffs small and reviewable
4. Avoid unrelated refactors
5. Add comments where intent is not obvious
6. Prefer clarity over cleverness — write explicit, readable code
7. Do not introduce unnecessary abstractions or new dependencies without justification

### Forbidden Behaviors

- No large "vibe-coded" rewrites
- No silent refactoring across modules
- No mixing UI logic with CAD logic
- No bypassing architecture for speed
- No architectural changes without explicit approval

## Repository Layout

```
apps/desktop-ui/           React + TypeScript + Tauri desktop app
  src/
    app/                   Feature actions, lifecycle, tool logic, panel components
    layout/                Layout components (panels, toolbars, viewport)
    layout/viewport/       Viewport rendering (Three.js), draft previews, snap, selection
    state/                 Zustand store (cadCoreStore.ts)
    hooks/                 React hooks for core bridge, event handling
    lib/                   IPC protocol helpers, schema parsing, core client
    types/                 TypeScript types (IPC, geometry, viewport, scene)
    config/                App config, theme JSON files
    i18n/                  Translation strings (en.json)
  src-tauri/
    src/                   Rust: core process management, protocol bridge, OrcaSlicer integration
    Cargo.toml
native/cad-core/           C++ CAD core (CMake project)
  src/
    app.cpp/h              Main event loop, command dispatch
    protocol/              IPC JSON parsing, event construction
    core/
      document/            Document model, feature tree, undo/redo
      sketch/              Sketch geometry, constraints (planegcs solver), profiles, inference
      geometry/            Body compilation, edge/face geometry, feature shapes
      extrude/             Extrude feature
      revolve/             Revolve feature
      loft/ / sweep/       Loft and sweep features
      primitive/           Box and cylinder primitives
      construction/        Construction planes
      cam/                 CAM operations (face milling)
      viewport/            Viewport state generation (mesh data, primitives)
      export/              STEP/STL export
      diagnostics/         Structured logging
  tests/                   C++ test executables
protocol/schema/           IPC JSON schemas (commands, events)
wiki/                      Canonical documentation (mirrored to polysmith.wiki git submodule)
third_party/
  occt/                    Vendored OpenCascade 7.8 source (git submodule)
  freetype/                FreeType (git submodule)
  planegcs/                2D geometric constraint solver from FreeCAD (git submodule)
  planegcs-validation/     Planegcs test suite (git submodule)
scripts/                   Build scripts (configure-occt.mjs, build-release.mjs)
```

## IPC Protocol

Commands (UI → Core) and events (Core → UI) are newline-delimited JSON messages:

```json
{ "id": "<uuid>", "type": "command_name", "payload": { ... } }
```

- **Commands** arrive on core's stdin, dispatched by `app.cpp` to the appropriate handler.
- **Events** are written to core's stdout, picked up by Tauri's stdout reader thread, and emitted as Tauri events (`cad-core-event`).
- **Structured logs** from the core go to stderr; Tauri forwards them as `cad-core-log` events.
- Every command gets a response event (success/error/state update). Commands are async — the UI sends and waits for the matching event by `id`.
- Protocol schemas live in `protocol/schema/commands.schema.json` and `protocol/schema/events.schema.json`.
- When changing IPC messages, also update `wiki/IPC-Protocol.md` and `wiki/AI-CAD-Command-Language.md`.

## Testing

**Rule: Never commit untested code.** Before committing any C++ or TypeScript
change, verify it compiles. For C++ changes, run `pnpm core:rebuild` (or at
minimum `pnpm core:build` if CMake cache is current). For TypeScript changes,
run `pnpm --filter desktop-ui exec tsc --noEmit`. If a build fails, fix the
error before committing — never commit code known to be broken.

For logic changes, run the relevant test suite when one exists. If no test
covers the changed code path, manually verify the fix in the running
application before committing. A commit message should describe what was
tested and how.

### C++ Tests

Tests are standalone executables built by CMake (sketch profile, multi-profile
extrude, extrude quality, CAM face reference, plugin feature). Run ALL of them
with one command — it handles the OCCT DLL path:

```bash
pnpm test:core
```

Rebuild tests with `pnpm core:rebuild` (they link against the full CAD core + OCCT).

### Regression Prevention (binding)

Profile detection and sketch geometry are the most regression-prone code in
the project — bugs there resurface as "Sketch profile not found", missing
surfaces, or wrong extrude volumes, and manual app testing is slow. These
rules exist to keep regressions from reaching the user:

- **Test before fix.** Every bug fix to sketch/profile/geometry logic must
  ship with a C++ regression test that reproduces the bug — it must fail
  before the fix and pass after. Add it to the matching suite in
  `native/cad-core/tests/`, following the existing test style.
- **Assert the complete region set.** Profile-detection tests must assert
  the FULL expected profile set (exact entity-id set + kind per region)
  using `polysmith::test::profiles_match` from
  `native/cad-core/tests/sketch_test_utils.h` — never just the presence of
  one profile. Precedent: a 2026-08 face-walk change silently removed a
  full-circle profile and the suite stayed green because the trim test only
  asserted the outer polygon.
- **Face-walk changes require all suites.** Any change to
  `sketch_profile_exact.inc` (arrangement, face walk, tangency/epsilon
  rules) must pass `pnpm test:core` in full — a walk-rule tweak that fixes
  one face commonly breaks another.
- **No tangency heuristic without both epsilon signs.** Tangency,
  epsilon, and tie-break changes are floating-point traps; a regression
  test must cover both signs of the deviation (e.g., the tangent line
  drawn slightly to either side of exact tangency).
- **Use the built-in face-walk trace.** The face walk has a permanent
  env-gated diagnostic: `PS_TRACE_FACES=1` dumps every walk step to the
  structured log under tag `exact_profiles`. Use it to diagnose face
  detection issues before adding one-off debug scaffolding.
- **Name the suites in the commit message.** State which suites ran and
  what was verified, per the never-commit-untested-code rule above.

### TypeScript

No formal test runner is configured yet. Type-check with:
```bash
pnpm --filter desktop-ui exec tsc --noEmit
```

## Key Dependencies

- **OpenCascade 7.8** — geometry kernel, vendored as a git submodule at `third_party/occt/`
- **planegcs** — 2D geometric constraint solver (ported from FreeCAD), built as a static library linked into the CAD core
- **Eigen3** — linear algebra (used by planegcs)
- **Boost** — graph and math libraries (used by planegcs constraint solving)
- **nlohmann/json** — JSON parsing in the C++ core (header-only, vendored)
- **Three.js** — WebGL viewport rendering in the UI
- **Zustand** — React state management
- **Tauri v2** — desktop shell

## Cross-Platform

Must compile on Windows (MSVC) and POSIX (Linux/macOS, GCC/Clang).

C++ notes:
- `M_PI` is not standard — define `_USE_MATH_DEFINES` before `<cmath>` on MSVC.
- MSVC treats `const char*` ↔ `unsigned char*` as an error. Match types exactly.
- OCCT DLLs on Windows live at `third_party/occt-install/win64/vc14/bin`. Tauri prepends this to `PATH` when spawning the core.

## Debugging & Logging

This is a **Tauri desktop app** — there is no F12 DevTools console and no terminal
output visible to the user. All diagnostic output must go through the in-app
**Logs panel** (toolbar icon, expandable sidebar). Never use `console.log`,
`printf`, `fprintf(stderr, ...)`, `std::cerr`, or `std::cout` for permanent
diagnostics — they are either discarded or surfaced as raw stderr events the
user cannot filter.

### Logging from TypeScript (UI layer)

Two functions available from `useCadCoreStore`:

```ts
// Simple text message — appears in Logs panel immediately.
const addMessage = useCadCoreStore((state) => state.addMessage);
addMessage("export started");

// Structured log entry — level-filterable, carries source + timestamp.
// Use makeUiLogEntry from @/lib for correct structure.
import { makeUiLogEntry } from "@/lib";
const addLogEntry = useCadCoreStore((state) => state.addLogEntry);
addLogEntry(makeUiLogEntry("info", "desktop_ui", "export started"));
```

`LogLevel`: `"debug"` | `"info"` | `"warn"` | `"error"`.
`LogEntry`: `{ level, source, message, timestamp }` (defined in `src/types/ipc.ts`).

### Logging from C++ (CAD core)

Use the project's structured logger (`core/diagnostics/logger.h`), **not** raw
`fprintf` or `std::cerr`:

```cpp
#include "core/diagnostics/logger.h"

polysmith::core::log_info("source_tag", "message");
polysmith::core::log_warn("source_tag", "message");
polysmith::core::log_error("source_tag", "message");
polysmith::core::log_debug("source_tag", "message");
```

This writes both to stderr (for Tauri's stderr bridge) **and** emits a
structured `log` IPC event that the UI routes to the Logs panel with proper
level/source/timestamp metadata.

Raw stderr output (from `fprintf(stderr, ...)` or uncaught exception messages)
is captured by Tauri and forwarded as `cad-core-log` events, but without
structured metadata — level defaults to `"info"` and source is unknown.
Prefer the logger API.

## Build Troubleshooting

### Boost find_package fails on CMake 3.30+

CMake 3.30 removed the built-in `FindBoost` module. Use a fallback pattern:
```cmake
find_package(Boost CONFIG QUIET)    # vcpkg provides BoostConfig.cmake
if(NOT Boost_FOUND)
  find_package(Boost MODULE REQUIRED)  # fallback for old distros
endif()
```
This is already applied in `native/cad-core/CMakeLists.txt` — preserve this
pattern if modifying CMake files.

### Eigen3 not found (Windows)

```bash
vcpkg install eigen3 --triplet x64-windows
```
Eigen3 is header-only but must be present in the vcpkg tree.

### planegcs submodule empty

```bash
git submodule update --init third_party/planegcs
```
The submodule is registered in `.gitmodules` but may not be cloned.

### npm package missing

```bash
pnpm install
```
The `@salusoft89/planegcs` WASM package must be materialized in `node_modules`.

### Submodules not initialized after clone

The bootstrap script handles this, but if skipping bootstrap:
```bash
git submodule update --init --recursive
```

## Wiki Documentation

All project documentation lives in `wiki/` and is mirrored to the GitHub wiki submodule at `polysmith.wiki/`. Edits go to `wiki/` first, then mirrored.

**After any edit to `wiki/`, sync the mirror:**
```bash
rsync -av --delete wiki/ polysmith.wiki/ --exclude .git
```
Then verify with:
```bash
diff -r wiki/ polysmith.wiki/ --exclude='.git' && echo "IDENTICAL"
```
Both directories must stay identical. Adding a new page to `wiki/` means also adding it to the mirror. Deleting from `wiki/` means also deleting from the mirror.

Key pages for understanding the system:
- `wiki/Architecture-Overview.md` — system layout
- `wiki/Core-UI-Design-Principles.md` — what belongs in core vs. UI (READ FIRST)
- `wiki/Contextual-Modeling-Workflow.md` — binding UX pattern
- `wiki/IPC-Protocol.md` — communication contract
- `wiki/Topological-Naming-Problem.md` — TNP strategy
- `wiki/Repository-Map.md` — directory layout
- `wiki/Implementation-Log.md` — shipped features and build fixes

## Branch Workflow

- `dev` is the default development branch; `main` is production/stable.
- Feature branches from latest `dev`, squash-merge back via PR.
- **No git mutations without approval.** `git commit`, `git checkout`, `git rm`, `git reset`, `git stash`, `git revert`, `git cherry-pick`, `git add`, and any other command that changes the working tree, index, or branch state must be approved by the user before execution. Read-only commands (`git status`, `git log`, `git diff`, `git show`) are exempt.
- Prefer `gh` CLI for PR operations when available — also requires approval.
- **Never commit without asking first.** Every `git commit` must be approved by the user before execution.
- **Never commit untested code.** Code must be verified by at least a successful build (`pnpm core:rebuild` or equivalent) before it can be committed. If the build environment is unavailable, state that clearly instead of committing blind.
- At the start of every prompt that may change files, check the current branch and working tree state before editing.
- Keep feature branches scoped to one implementation or fix.
- Open implementation PRs as draft until tested and ready for review.
- After merge, delete the remote and local feature branch.

## Philosophy

PolySmith is built to be **understandable and maintainable by humans first**.
AI is a tool to accelerate development, not a substitute for ownership.

- Keep changes small and focused.
- Prefer clarity over cleverness.
- Preserve architecture boundaries.
- Do not move CAD logic into the UI.
- Do not bypass the IPC contract.
