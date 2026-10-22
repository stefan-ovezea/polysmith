#!/usr/bin/env python3
"""Measure what the CAD core actually puts on the IPC wire.

The core is a newline-delimited JSON process: every command is
`{"id", "type", "payload"}` on stdin, every reply is `{"id", "type", "payload"}`
on stdout (logs interleave as `{"type":"log",...}` events). This script feeds a
fixed command sequence to the **already built** `cad_core.exe`, then reports the
serialised size of each reply and a breakdown of the biggest one, so the IPC
payload plan (wiki/IPC-Payload-Plan.md) has real numbers to move.

Feeding the whole sequence at once and closing stdin keeps the harness immune to
pipe-buffer deadlocks; per-command latency is not measured here (the app's own
suite measures that).

Usage (from the repo root):

    python scripts/measure_payload.py
    python scripts/measure_payload.py --document projects/Sketch3.polysmith --json out.json
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OCCT_BIN = REPO / "third_party/occt8-install/win64/vc14/bin"
CANDIDATES = (
    "native/cad-core/build/Release/cad_core.exe",
    "native/cad-core/build/cad_core.exe",
    "native/cad-core/build-release/Release/cad_core.exe",
)


def find_core(explicit: str | None) -> Path:
    if explicit:
        path = Path(explicit)
        if not path.is_file():
            sys.exit(f"core not found: {path}")
        return path
    for candidate in CANDIDATES:
        path = REPO / candidate
        if path.is_file():
            return path
    sys.exit("cad_core.exe not found - build the core first (pnpm core:build)")


def first_sketch_ids(document: dict) -> tuple[str | None, str | None, str | None]:
    """The sketch feature plus a vertex and a line to poke.

    `vertices` is derived and only shipped for the open sketch (Phase A), so the
    ids come from the line entry; `reenter_sketch` needs the feature id.
    """
    for feature in document.get("feature_history", []):
        if feature.get("kind") != "sketch":
            continue
        params = feature.get("sketch_parameters") or {}
        lines = params.get("lines") or []
        if not lines:
            continue
        line = lines[0]
        return feature.get("feature_id"), line.get("start_vertex_id"), line.get("line_id")
    return None, None, None


def send_batch(exe: Path, commands: list[dict]) -> tuple[list[dict], float]:
    env = os.environ.copy()
    if OCCT_BIN.is_dir():
        env["PATH"] = f"{OCCT_BIN}{os.pathsep}{env.get('PATH', '')}"
    stdin = "".join(json.dumps(command) + "\n" for command in commands)
    started = time.perf_counter()
    completed = subprocess.run(
        [str(exe)],
        cwd=str(REPO),
        env=env,
        input=stdin,
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=600,
    )
    elapsed = (time.perf_counter() - started) * 1000
    if completed.returncode != 0:
        sys.exit(f"core exited {completed.returncode}: {completed.stderr[-800:]}")
    replies = []
    for line in completed.stdout.splitlines():
        if not line.strip():
            continue
        message = json.loads(line)
        if "id" in message:            # replies echo the command id; logs do not
            replies.append(message)
    return replies, elapsed


def breakdown(document: dict) -> list[tuple[str, int]]:
    rows: list[tuple[str, int]] = []
    for key, value in document.items():
        if key == "feature_history" and isinstance(value, list):
            rows.append(("feature_history (all features)", len(json.dumps(value))))
            for feature in value[:8]:
                kind = feature.get("kind", "?")
                rows.append((f"  feature[{kind}]", len(json.dumps(feature))))
                params = feature.get("sketch_parameters")
                if isinstance(params, dict):
                    for pkey, pvalue in params.items():
                        size = len(json.dumps(pvalue))
                        if size > 400:
                            rows.append((f"    sketch_parameters.{pkey}", size))
                # features other than sketches keep their payload elsewhere
                for pkey, pvalue in feature.items():
                    if pkey in ("sketch_parameters", "feature_id", "kind", "name"):
                        continue
                    size = len(json.dumps(pvalue))
                    if size > 1000:
                        rows.append((f"    {pkey}", size))
        else:
            rows.append((key, len(json.dumps(value))))
    return sorted(rows, key=lambda row: -row[1])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core")
    parser.add_argument("--document", default="projects/Sketch3.polysmith")
    parser.add_argument("--json", help="write the table as JSON to this path")
    args = parser.parse_args()

    core = find_core(args.core)
    document_path = REPO / args.document
    if not document_path.is_file():
        sys.exit(f"document not found: {document_path}")

    # 1. open the design and read the ids we will act on
    replies, _ = send_batch(core, [
        {"id": "b1", "type": "create_document", "payload": {}},
        {"id": "b2", "type": "load_document", "payload": {"file_path": str(document_path)}},
    ])
    loaded = next((r for r in replies if r["id"] == "b2"), None)
    if loaded is None:
        sys.exit("load_document produced no reply")
    document = loaded.get("payload", {})
    sketch_id, point_id, entity_id = first_sketch_ids(document)

    # 2. one process, the representative sequence a click or an edit runs
    commands = [
        {"id": "c1", "type": "create_document", "payload": {}},
        {"id": "c2", "type": "load_document", "payload": {"file_path": str(document_path)}},
        {"id": "c3", "type": "get_document_state", "payload": {}},
        {"id": "c4", "type": "get_viewport_state", "payload": {}},
    ]
    labels = {
        "c1": "create_document",
        "c2": "load_document",
        "c3": "get_document_state (full)",
        "c4": "get_viewport_state",
    }
    if point_id and entity_id and sketch_id:
        # selection and point edits need an open sketch ("No active sketch"
        # otherwise), which is also the state in which the derived caches ship
        commands.append({"id": "c5", "type": "reenter_sketch",
                         "payload": {"feature_id": sketch_id}})
        labels["c5"] = "reenter_sketch (open)"
        commands += [
            {"id": "c6", "type": "select_sketch_entity", "payload": {"entity_id": entity_id}},
            {"id": "c7", "type": "select_sketch_vertex", "payload": {"vertex_id": point_id}},
            {"id": "c8", "type": "update_sketch_vertex",
             "payload": {"vertex_id": point_id, "x": 12.34, "y": 5.67}},
            {"id": "c9", "type": "get_document_state", "payload": {}},
        ]
        labels.update({
            "c6": "select_sketch_entity",
            "c7": "select_sketch_vertex",
            "c8": "update_sketch_vertex (edit)",
            "c9": "get_document_state (after edit)",
        })
    else:
        print("! no sketch with a line found; skipping the sketch commands")

    replies, elapsed = send_batch(core, commands)
    by_id = {reply["id"]: reply for reply in replies}

    rows = []
    print(f"core:     {core}")
    print(f"document: {document_path.name}  ({document_path.stat().st_size:,} bytes on disk)")
    print(f"sequence: {len(commands)} commands in {elapsed:,.0f} ms (one process, incl. load)\n")
    print(f"{'command':32} {'reply':22} {'bytes':>10}")
    print("-" * 68)
    for command in commands:
        reply = by_id.get(command["id"])
        label = labels[command["id"]]
        if reply is None:
            print(f"{label:32} {'NO REPLY':22}")
            continue
        size = len(json.dumps(reply, separators=(",", ":")).encode("utf-8"))
        rows.append({"command": label, "reply": reply["type"], "bytes": size})
        print(f"{label:32} {reply['type']:22} {size:>10,}")

    state = by_id.get("c8") or by_id.get("c3")
    if state is not None:
        print("\nlargest document_state reply, by key:")
        for key, size in breakdown(state.get("payload", {}))[:18]:
            print(f"  {key:46} {size:>9,} bytes")

    viewport = by_id.get("c4")
    if viewport is not None:
        total = rows[-1]["bytes"] if rows else 0
        print(f"\nviewport_state payload keys:")
        for key, size in sorted(
            ((k, len(json.dumps(v))) for k, v in viewport.get("payload", {}).items()),
            key=lambda kv: -kv[1],
        )[:10]:
            print(f"  {key:46} {size:>9,} bytes")

    if args.json:
        Path(args.json).write_text(json.dumps(rows, indent=2) + "\n", encoding="utf-8")
        print(f"\nwrote {args.json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
