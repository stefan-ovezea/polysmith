# Codex Rules for PolySmith

Use these rules when asking AI to help with development.

## General Instructions

- Always propose a plan before coding
- Show changed files first
- Keep changes minimal and scoped
- Do not refactor unrelated code
- Do not introduce new dependencies unless necessary

## Architecture Rules

- UI (React) handles presentation and user intent only
- CAD core owns all geometry and modeling logic
- Communication must go through IPC protocol
- Do not mix responsibilities between layers

## Code Guidelines

- Prefer simple, readable code
- Avoid premature abstractions
- Keep functions small
- Use explicit types

## When Adding Features

- Start with the smallest possible implementation
- Validate assumptions early
- Avoid over-engineering
- Build incrementally

## When Modifying Code

- Explain why the change is needed
- Avoid breaking existing behavior
- Keep diffs easy to review

## Verification Preference

- By default, verify changes with `pnpm core:rebuild` so the native C++ code is rebuilt
- Do not run frontend or Tauri checks by default after each change
- Only run additional checks when explicitly requested by the user

## Output Format

When responding:

1. Short explanation of approach
2. List of files to modify
3. Code changes
4. Notes (if needed)

## Important Reminder

Do not optimize for speed of coding.

Optimize for:
- clarity
- maintainability
- correctness
