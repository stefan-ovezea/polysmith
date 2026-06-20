# AGENTS.md

Guidelines for AI-assisted development (Codex, ChatGPT, etc.)

**All agent instructions have been consolidated into [CLAUDE.md](CLAUDE.md).**
Read that file at the start of every session — it is the single authoritative
agent instruction set for this repository. The content below is kept for human
contributors and as a quick reference; if anything conflicts, CLAUDE.md wins.

## Quick Reference

### Documentation

All project documentation lives in `wiki/`. When adding new documentation,
create the file in `wiki/` and add a link from `wiki/Home.md`.

### Core Principles

- Do not make architectural changes without explicit approval.
- Keep changes minimal and scoped.
- Prefer clarity over cleverness.
- React (UI) must NOT own CAD state — CAD state, geometry, and modeling logic live ONLY in the native core.
- Communication between UI and core must go through the IPC protocol.

### Topological Naming Problem (TNP)

**This is the project's mantra.** Never store a naked OCCT topology index and
trust it across recomputes. Re-resolve against live body shapes on every
recompute. On failure, degrade with `dependency_broken` + warning — never crash.

Full strategy: `wiki/Topological-Naming-Problem.md`

### Testing

- Add tests for non-trivial logic
- Do not break existing behavior without explanation
