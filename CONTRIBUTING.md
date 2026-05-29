# Contributing to PolySmith

Thanks for contributing to PolySmith.

This project is intentionally built with strong architectural boundaries so that it remains understandable and maintainable over time.

## Core Principles

- Keep changes small and focused
- Prefer clarity over cleverness
- Preserve architecture boundaries
- Do not move CAD logic into the UI
- Do not bypass the IPC contract

## Before You Change Code

Please read:

- `README.md`
- `AGENTS.md`
- `.deepseek/instructions.md`
- `wiki/Architecture-Overview.md`
- `wiki/Contextual-Modeling-Workflow.md`
- `wiki/IPC-Protocol.md`
- `wiki/Topological-Naming-Problem.md`

## Branch Workflow

- `dev` is the default development branch and the base for feature work.
- `main` is production/stable release code. Do not target feature PRs at
  `main`.
- Before starting work, sync the latest `dev`.
- Create a dedicated feature branch from `dev` for each implementation or fix.
- Keep branches narrow and delete them after their PR is merged.

## Development Workflow

### Run the desktop app

```bash
pnpm dev
```

### Build the CAD core

```bash
pnpm core:rebuild
```

## Change Scope

Good changes:

- one focused feature
- one bug fix
- one protocol improvement
- one documentation improvement

Avoid mixing:

- protocol changes
- UI cleanup
- architecture refactors
- unrelated formatting
- dependency changes

into one PR or one AI task.

## Coding Guidelines

- Write explicit code
- Keep functions readable
- Avoid hidden state
- Keep files focused
- Prefer simple data flow
- Add comments where intent is not obvious

## Architecture Rules

React owns:

- presentation
- user intent
- view state

The CAD core owns:

- CAD state
- document state
- geometry state
- feature history
- modeling behavior

Communication must go through the protocol layer.

## Protocol Changes

If you change message shapes or behavior, also update:

- `wiki/IPC-Protocol.md`
- `wiki/AI-CAD-Command-Language.md`
- `protocol/schema/*`

## AI-Assisted Development

If using Codex or other AI tools:

- follow `AGENTS.md`
- keep tasks narrow
- avoid broad autonomous refactors
- check the branch and working tree state before editing

## Testing

For non-trivial changes:

- add or update tests
- verify behavior manually if automated tests do not exist yet

## Pull Requests

- Use GitHub pull requests to merge feature branches into `dev`.
- Prefer the GitHub CLI (`gh`) when it is available and authenticated.
- Open implementation PRs as draft until the change has been tested.
- Include a concise summary, test notes, and known risks or follow-up work.
- Sync with the latest `dev` before review or merge.
- Use squash-merge after approval and passing checks.
- Delete the remote and local feature branch after merge.

## Philosophy

PolySmith should remain understandable by a human maintainer at all times.

AI is a tool to accelerate development, not a substitute for ownership.
