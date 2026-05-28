# PolySmith — Project Onboarding

When starting a new session in this project, read these wiki pages first to
understand the system. They are the canonical documentation.

## Reading order (first session)

1. **[Architecture Overview](wiki/Architecture-Overview.md)** — UI / Tauri / C++ core layout
2. **[Contextual Modeling Workflow](wiki/Contextual-Modeling-Workflow.md)** — the binding UX pattern every feature follows
3. **[IPC Protocol](wiki/IPC-Protocol.md)** — how UI and core communicate
4. **[Topological Naming Problem](wiki/Topological-Naming-Problem.md)** — the project's mantra

## Return-session quick-ref

- **[Repository Map](wiki/Repository-Map.md)** — directory layout
- **[AI CAD Command Language](wiki/AI-CAD-Command-Language.md)** — IPC command reference for agents
- **[V1 Roadmap](wiki/V1-Roadmap.md)** — current priorities
- **[Implementation Log](wiki/Implementation-Log.md)** — what's shipped, including platform-specific build fixes

## Wiki Mirroring

The PolySmith wiki is mirrored across two locations that must stay in sync
with the canonical source:

| Location | Description |
|---|---|
| `wiki/` | Canonical source — edit here first |
| `felix/polysmith.wiki/` | felix's personal mirror (separate git repo) |
| `stefan/polysmith.wiki/` | stefan's personal mirror (separate git repo) |

The normal flow is **canonical → mirrors**: changes originate in `wiki/` and
are propagated outward. Seldom, changes may originate in a mirror and need to
flow back to canonical. In either case, all three locations must stay consistent.

### When editing wiki documentation

1. Make the change in `wiki/<file>.md` first.
2. Mirror the identical change to both mirrors — if a mirror directory
   (`felix/polysmith.wiki/` or `stefan/polysmith.wiki/`) does not exist,
   print a warning and skip it; do not fail.
3. Verify the copies that exist match after the edit.

### When adding new wiki pages

1. Create the file in `wiki/<New-Page>.md`.
2. Add a link from `wiki/Home.md`.
3. Copy the new file and mirror the `Home.md` link update to both mirrors —
   if a mirror directory does not exist, warn and skip it.

## Rules

- All documentation lives in `wiki/`.
- AGENTS.md at repo root is the binding instruction set — read it at session start.
- When adding new docs, create the file in `wiki/` and add a link from `wiki/Home.md`.
- Cross-reference wiki pages by their title-cased name without extension (e.g., `Architecture-Overview`).