# Active task: GRBL workspace polish — SPRINT COMPLETE (2026-09-12)

> **Branch:** `feature/grbl-polish` (from `dev` @ d0db60d)
> **Plan:** approved plan at `.claude/plans/jaunty-sleeping-moth.md`
> **Commits:** a67b26d (P1) · fb7ea0b (P2–P6) · 38306b1 (P7)
> P8 ("test patterns both") was covered by P1 (CAM test-pattern
> panels ride the internal handoff) + P4 (GRBL-workspace quick
> utilities) — no extra code needed.

## Shipped in this sprint

1. **P1 internal handoff** — CAM posts G-code to memory, no file
   dialog/disk (cam_export_gcode_text ↔ grbl_parse_text ↔
   grbl_send_program; byte-identical to the file export, pinned by
   cam_export_test).
2. **P2 machine picker** — disk machine library dropdown in the GRBL
   workspace (localStorage `polysmith.grbl.machineName`); bed
   fallback machine → document → default.
3. **P3 red laser pointer** — crosshair at MPos + machine pointer
   offset (--cad-pointer-dot), toggle M3 S13/M5, resets on
   disconnect.
4. **P4 laser utilities** — test fire (hold-to-fire), focus pulse,
   framing box; Rust-generated (grbl_utilities.rs), streamed +
   rendered as an overlay with independent progress; serde
   rename_all_fields fix + regression test.
5. **P5 live overrides + stats** — feed 10–200 % / power 10–100 %
   sliders via allowlisted real-time bytes (grbl_write_byte);
   elapsed/ETA row.
6. **P6 bed check + alarm decode** — program-exceeds-bed banner;
   ALARM:n descriptions (codes 1–9) in error events.
7. **P7 console + $$ settings** — up-arrow command recall; $$ dump
   collection with deadline fallback (FluidNC may skip ok); settings
   dialog with ~35 key descriptions, per-row Apply.

**Verification:** user in-app rounds on the FluidNC machine for
P1–P6; P7 committed on the user's explicit instruction with
automated gates (cargo test 19/19, tsc --noEmit); the settings
dialog can still be exercised in the running app.

## Next session checklist

1. Open the PR from `feature/grbl-polish` → `dev` (squash-merge per
   project workflow; requires user approval — no git mutations
   without it). After merge, delete the remote + local branch.
2. Deferred items the user may pick up later: WebSocket (port 81)
   transport, machine-definition JSON extensions (homing/jog
   prefs), GRBL-side $ settings persistence.
