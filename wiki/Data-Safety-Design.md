# Stage 2 — Data safety design

Concrete design for the deferred Stage 2 of
[Refactoring-Plan.md](Refactoring-Plan.md): atomic save, recovery journal, and
file-format versioning. Decisions confirmed 2026-10-23: **ASCII JSON** file
format, a versioned envelope, one `.bak`, and journaling all mutating commands.

## Current state

- `DocumentManager::save_document_to_path` writes `to_payload(document,
  include_opaque=true).dump(2)` straight over the destination — no temp, no
  flush/fsync, no `.bak`, no version field.
- `load_document_from_path` reads + parses + `document_from_payload`, with no
  version check or migration.
- Config paths already follow a convention: Tauri computes
  `app.path().app_data_dir()` and passes dirs to the core via `POLYSMITH_*_DIR`
  env vars, read with `std::getenv`.
- `document_->id` already keys the runtime caches — it is the document identity.

## A. File-format version

### A.1 The versioned envelope (v1)

```json
{
  "format": "polysmith/1",
  "document": { …DocumentState payload… }
}
```

`"document"` is exactly what `to_payload(document, include_opaque=true)` produces
today, so v1 is a pure envelope change with zero document-schema migration.
Absent `"format"` ⇒ treated as v0 (today's unversioned files).

### A.2 Migration ladder

```
native/cad-core/src/core/document/format/
  format_version.h   // constexpr int kCurrentFormatVersion = 1;
  migrations.h/.cpp  // one step per version, never edited after ship
```

Load flow: parse → read version (default 0) → apply pending steps in order →
`document_from_payload(payload["document"])`. The only step shipped now is
`migrate_0_to_1` (wrap the raw payload in the envelope).

### A.3 Corpus

```
native/cad-core/tests/corpus/
  box_v0.polysmith           # unversioned (raw DocumentState)
  box_v1.polysmith           # v1 envelope
  box_truncated.polysmith    # cut mid-object
  box_hostile_numbers.polysmith # NaN/Inf in a numeric field
```

A `cad_core_format_version_test` asserts: v0 loads and evaluates; v1 loads;
damaged variants error (never crash).

### A.4 (follow-up, not v1) — stop persisting derived caches

SolidCraft's container rule is "no kernel internals / memory layouts / opaque
blobs". PolySmith's `to_payload` persists derived caches (profiles, vertices,
viewport snapshots) the core recomputes. Flagged, not implemented in Stage 2 —
it overlaps the IPC-payload plan and is a separate change.

## B. Atomic save

Replace the raw write with:

1. serialize to the v1 envelope;
2. write `<path>.tmp` in the same directory;
3. `flush()` + `fsync()` (Windows `_commit`, POSIX `fsync`);
4. if `<path>` exists, rename it to `<path>.bak`;
5. rename `<path>.tmp` → `<path>`.

A crash at any instant leaves the old file, the new file, or `.bak` + `.tmp`,
never a truncated `<path>`. Kill-loop test (25 mid-save kills) asserts the file
always loads to the old or new state.

## C. Recovery journal

- Log every mutating command, as received, to `<config>/journal/<doc-id>.jsonl`
  — one raw command object per line (the same envelope the dispatcher consumes).
- `<config>` = `app_data_dir`; add `POLYSMITH_JOURNAL_DIR` (set by `cad_core.rs`
  like the existing `POLYSMITH_*_DIR`), read by the core, temp fallback when unset.
- `<doc-id>` = `document_->id`.
- A successful save truncates the journal (the saved file is the checkpoint).
- Replay: list `*.jsonl`, pick one → `create_document()` + re-dispatch each line,
  skipping (and logging) failures.
- v1 excludes autosave timing and journal GC — follow-up increments.

## Implementation order

1. A.1 + A.2 + A.3 (version, migration, corpus).
2. B (atomic save + kill test).
3. C (journal + replay test).
