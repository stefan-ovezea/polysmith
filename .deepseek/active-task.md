# Active task: GRBL transport sprint — CODE COMPLETE, verification pending (2026-09-12)

> **Branch:** `feature/grbl-transport` (from `dev` @ 5ca9550 — the
> squash-merged PR #79 that closed the polish sprint)
> **Plan:** `.claude/plans/jaunty-sleeping-moth.md` (P1–P8) + the
> follow-up scope approved by the user: WebSocket (port 81)
> transport, richer machine-definition JSON (homing/jog prefs),
> persisting $ settings.
> **Commits:** none yet — no commit without user verification +
> explicit approval (CLAUDE.md).

## Shipped in this sprint (all three items code-complete)

### A — WebSocket (port 81) transport

- **Rust** (`src-tauri`): `tungstenite = "0.26"` dependency;
  `GrblLink::Ws(WsLink)` — a framing adapter because tungstenite 0.26
  dropped the `std::io::Read/Write` impls (pending-bytes buffer, one
  message per read; write = Text if valid UTF-8 else Binary; Close
  frame → EOF). `connect_ws`: TCP connect 5 s → nodelay → 5 s read
  timeout → `ClientHandshake::start(stream, url.into_client_request(),
  None)?.handshake()` → 50 ms timeout. New `grbl_connect_ws` command
  registered in main.rs.
- **TS**: `grblConnectWs` client wrapper; CamGrblPanel third
  transport option "Network (WebSocket)", port input default 81
  persisted as `polysmith.grbl.wsPort`; i18n `cam.grbl.wsTransport`.

### B — Richer machine-definition JSON (homing/jog prefs)

- **Core**: `MachineDefinition` grew `jog_step_mm` (10),
  `jog_feed_mm_per_min` (1000), `homing_enabled` (true),
  `pointer_power_percent` (5). machine_library.cpp: parse with
  fallbacks (old JSON files load unchanged — pinned by the legacy
  test), to_json, validation (jog > 0, pointer power 0–100), laser
  seeds extended. Protocol: to_payload + from_payload in both .inc
  files (touch serialization.cpp — .inc trap).
- **TS**: MachineDefinition type + zod `cam_machine_list_result`
  event schema; CamSetupPanel GRBL-prefs fieldset (jog step/feed,
  pointer power, homing checkbox; seeded from a picked machine,
  included in findMatchingMachine + saved with the definition).
- **Workspace**: CamGrblPanel `machinePrefs` prop seeds the jog
  inputs and hides Home when `homing_enabled` is false; the pointer
  toggle now fires `M3 S{clamp(255·pct/100, 1, 255)}` from the
  machine instead of the hardcoded S13.

### C — Persist $ settings per machine + restore

- `polysmith.grbl.settings.<machineName>` snapshot written whenever a
  $$ dump arrives while a machine is selected (workspace passes
  `settingsMachineName`). GrblSettingsDialog gained
  `storedSettings` + Restore ("Restore saved (N)") — re-applies every
  persisted `$k=v`, then re-fetches. i18n keys added.

## Gates run (all green)

- `cargo test` — **19/19** (exit code un-masked this time; see the
  trap note below)
- `pnpm --filter desktop-ui exec tsc --noEmit` — clean
- C++ rebuild via the VS2022 cmake wrapper + `pnpm test:core` —
  **46/46 suites**, including the extended cam_machine_library_test
  (new-field round trip, legacy defaults, validation rejections,
  seed prefs)

**Traps hit this sprint (recorded in memory):** piping gates through
`| tail` masks the exit code — two masked cargo failures were
reported as green before the real errors surfaced; run gates with no
pipe. tungstenite 0.26 has no io::Read/Write, no
`handshake::client::client()`, no `Error::WriteZero`,
`write_message`→`send`, `Text(Utf8Bytes)`/`Binary(Bytes)` payload
types — full notes in `cam-rebuild-build-env.md`.

## Next session checklist

1. **User verification on the real FluidNC** (binding per CLAUDE.md):
   - Transport dropdown → "Network (WebSocket)", host + port 81:
     connects, status polls, jog/pointer work (FigUI on the same
     port proves the endpoint).
   - Machine picker → machine prefs: jog step/feed seed the inputs,
     pointer power fires at the machine's %, Home hides when the
     machine says no homing.
   - Settings dialog: fetch, edit, then Restore after a FluidNC
     reboot (runtime $ writes are dropped on reboot — the point of
     the feature).
2. Commit with explicit user approval (no Co-Authored-By trailer),
   then PR `feature/grbl-transport` → `dev` (squash), delete the
   branch.
