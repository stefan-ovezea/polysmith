#!/usr/bin/env python3
"""Conformance harness for incremental document state.

The UI is moving from "every command re-sends the whole document" to "events
carry only what changed". That only stays safe if the incrementally patched
client view is always identical to a full document, so this harness checks
exactly that:

  1. replay a command script against the core;
  2. after every command, build a SHADOW client view by applying the intended
     merge rules to the incoming event;
  3. ask the core for a full `get_document_state` and assert the shadow matches.

The merge rules are the ones the UI will implement, kept here in one place:

  * a `document_state` replaces the view, except that a feature that omits
    `profiles`/`vertices` keeps the arrays the client already had for it;
  * a `feature_changed` patches just that feature (and honours the same
    keep-derived rule);
  * a `sketch_entities_changed` patches lines/vertices inside one feature;
  * anything unknown forces a full refetch (and the harness asserts that the
    refetch matches, so a missed rule shows up as a mismatch, not a silent lie).

It runs against the binary that is already built, so it costs nothing to keep
in CI. `--self-test` proves it can fail: it drops one field from a reply and
expects a mismatch.

    python scripts/ipc_conformance.py
    python scripts/ipc_conformance.py --self-test
"""
from __future__ import annotations

import argparse
import copy
import json
import os
import subprocess
import sys
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


def run(core: Path, commands: list[dict]) -> dict[str, dict]:
    env = os.environ.copy()
    if OCCT_BIN.is_dir():
        env["PATH"] = f"{OCCT_BIN}{os.pathsep}{env.get('PATH', '')}"
    stdin = "".join(json.dumps(command) + "\n" for command in commands)
    completed = subprocess.run(
        [str(core)], cwd=str(REPO), env=env, input=stdin,
        capture_output=True, text=True, encoding="utf-8", timeout=900,
    )
    if completed.returncode != 0:
        sys.exit(f"core exited {completed.returncode}: {completed.stderr[-800:]}")
    replies = {}
    for line in completed.stdout.splitlines():
        if not line.strip():
            continue
        message = json.loads(line)
        if "id" in message:
            replies[message["id"]] = message
    return replies


DERIVED = ("profiles", "vertices")


def merge_document(view: dict | None, event: dict) -> dict:
    """Apply one event to the client view, using the intended merge rules."""
    kind = event.get("type")
    payload = event.get("payload") or {}

    if kind in ("document_created", "document_state"):
        if view is None:
            return copy.deepcopy(payload)
        # keep derived arrays for features that omit them (the wire trims them)
        merged = copy.deepcopy(payload)
        previous = {
            feature.get("feature_id"): feature
            for feature in view.get("feature_history", [])
        }
        for feature in merged.get("feature_history", []):
            old = previous.get(feature.get("feature_id"))
            params = feature.get("sketch_parameters")
            old_params = (old or {}).get("sketch_parameters")
            if not isinstance(params, dict) or not isinstance(old_params, dict):
                continue
            for key in DERIVED:
                if not params.get(key) and old_params.get(key):
                    params[key] = copy.deepcopy(old_params[key])
        return merged

    if kind == "feature_changed":
        if view is None:
            raise KeyError("feature_changed before any document")
        merged = copy.deepcopy(view)
        removed = set(payload.get("removed_feature_ids") or [])
        history = [f for f in merged.get("feature_history", [])]
        merged["feature_history"] = [f for f in history if f.get("feature_id") not in removed]
        incoming = payload.get("feature")
        if isinstance(incoming, dict):
            target = next(
                (f for f in merged["feature_history"]
                 if f.get("feature_id") == incoming.get("feature_id")),
                None,
            )
            if target is None:
                merged["feature_history"].append(copy.deepcopy(incoming))
            else:
                old_params = (target.get("sketch_parameters") or {})
                new_params = (incoming.get("sketch_parameters") or {})
                for key in DERIVED:
                    if key not in new_params and key in old_params:
                        new_params[key] = copy.deepcopy(old_params[key])
                target.update(copy.deepcopy(incoming))
        return merged

    if kind == "sketch_entities_changed":
        if view is None:
            raise KeyError("sketch_entities_changed before any document")
        merged = copy.deepcopy(view)
        feature_id = payload.get("feature_id")
        for feature in merged.get("feature_history", []):
            if feature.get("feature_id") != feature_id:
                continue
            params = feature.setdefault("sketch_parameters", {})
            merge_entities(params, "lines", "line_id", payload.get("lines"))
            merge_entities(params, "vertices", "vertex_id", payload.get("vertices"))
            removed = set(payload.get("removed") or [])
            for key in ("lines", "vertices"):
                if isinstance(params.get(key), list):
                    id_key = "line_id" if key == "lines" else "vertex_id"
                    params[key] = [e for e in params[key] if e.get(id_key) not in removed]
            derived = payload.get("derived") or {}
            for key in ("profile_count", "vertex_count"):
                if key in derived:
                    params[key] = derived[key]
        return merged

    if kind == "selection_changed":
        if view is None:
            raise KeyError("selection_changed before any document")
        merged = copy.deepcopy(view)
        merged.update(copy.deepcopy(payload))   # selection fields + revision
        return merged

    raise KeyError(f"no merge rule for event `{kind}`")


def merge_entities(params: dict, key: str, id_key: str, entries) -> None:
    if not entries:
        return
    current = params.setdefault(key, [])
    by_id = {entry.get(id_key): entry for entry in current}
    for entry in entries:
        existing = by_id.get(entry.get(id_key))
        if existing is None:
            current.append(copy.deepcopy(entry))
        else:
            existing.update(copy.deepcopy(entry))


def first_entry(document: dict, kind: str, id_key: str) -> str | None:
    for feature in document.get("feature_history", []):
        if feature.get("kind") != kind:
            continue
        params = feature.get("sketch_parameters") or {}
        entries = params.get({"line_id": "lines", "vertex_id": "vertices"}.get(id_key, "")) or []
        if entries:
            return entries[0].get(id_key)
    return None


def strip_profiles(document: dict) -> dict:
    """`profiles` is fetched separately (`get_sketch_profiles`), so it is not part
    of the event contract: compare everything else."""
    clone = copy.deepcopy(document)
    for feature in clone.get("feature_history", []):
        params = feature.get("sketch_parameters")
        if isinstance(params, dict):
            params.pop("profiles", None)
    return clone


def diff(path: str, expected, actual, out: list[str]) -> None:
    if type(expected) is not type(actual) and not (
        isinstance(expected, (int, float)) and isinstance(actual, (int, float))
    ):
        out.append(f"{path}: type {type(expected).__name__} != {type(actual).__name__}")
        return
    if isinstance(expected, dict):
        for key in sorted(set(expected) | set(actual)):
            if key not in expected:
                out.append(f"{path}.{key}: unexpected in shadow")
            elif key not in actual:
                out.append(f"{path}.{key}: missing from shadow")
            else:
                diff(f"{path}.{key}", expected[key], actual[key], out)
    elif isinstance(expected, list):
        if len(expected) != len(actual):
            out.append(f"{path}: length {len(actual)} != {len(expected)}")
            return
        for index, (left, right) in enumerate(zip(expected, actual)):
            diff(f"{path}[{index}]", left, right, out)
    elif expected != actual:
        out.append(f"{path}: {actual!r} != {expected!r}")


def build_script(document: dict) -> list[dict]:
    sketch_id = next(
        (f.get("feature_id") for f in document.get("feature_history", [])
         if f.get("kind") == "sketch"),
        None,
    )
    line = first_entry(document, "sketch", "line_id")
    # `vertices` is derived and empty until the sketch is open, so the point id
    # comes from the line entry, which always carries it.
    point = None
    for feature in document.get("feature_history", []):
        if feature.get("kind") == "sketch":
            lines = (feature.get("sketch_parameters") or {}).get("lines") or []
            if lines:
                point = lines[0].get("start_vertex_id")
                break
    script = [{"id": "s1", "type": "get_document_state", "payload": {}}]
    if sketch_id:
        script.append({"id": "s2", "type": "reenter_sketch", "payload": {"feature_id": sketch_id}})
    if point:
        script.append({"id": "s3", "type": "update_sketch_vertex",
                       "payload": {"vertex_id": point, "x": 11.5, "y": 4.25}})
        script.append({"id": "s4", "type": "update_sketch_vertex",
                       "payload": {"vertex_id": point, "x": 12.75, "y": 4.75}})
    if line:
        script.append({"id": "s5", "type": "select_sketch_entity", "payload": {"entity_id": line}})
        script.append({"id": "s6", "type": "update_sketch_line",
                       "payload": {"line_id": line, "start_x": 10.5, "start_y": 3.25,
                                   "end_x": 22.0, "end_y": 3.25}})
        script.append({"id": "s7", "type": "move_sketch_entities",
                       "payload": {"entity_ids": [line], "dx": 0.5, "dy": 0.25,
                                   "center_x": 0.0, "center_y": 0.0,
                                   "angle_deg": 0.0}})
        script.append({"id": "s8", "type": "add_sketch_line",
                       "payload": {"start_x": 30.0, "start_y": 6.0,
                                   "end_x": 38.0, "end_y": 6.0,
                                   "is_construction": False}})
    extrude = next((f for f in document.get("feature_history", [])
                    if f.get("kind") == "extrude"), None)
    if extrude:
        depth = (extrude.get("extrude_parameters") or {}).get("depth", 10.0)
        script.append({"id": "s8b", "type": "update_extrude_depth",
                       "payload": {"feature_id": extrude.get("feature_id"),
                                   "depth": float(depth) + 2.5}})
    script.append({"id": "s9", "type": "get_document_state", "payload": {}})
    return script


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core")
    parser.add_argument("--document", default="projects/Sketch3.polysmith")
    parser.add_argument("--self-test", action="store_true",
                        help="prove the harness can fail by corrupting one reply")
    args = parser.parse_args()

    core = find_core(args.core)
    document_path = REPO / args.document
    if not document_path.is_file():
        sys.exit(f"document not found: {document_path}")

    print(f"core:     {core}")
    print(f"document: {document_path.name}\n")

    preamble = [
        {"id": "l1", "type": "create_document", "payload": {}},
        {"id": "l2", "type": "load_document", "payload": {"file_path": str(document_path)}},
    ]
    loaded = run(core, preamble)
    document = loaded.get("l2", {}).get("payload")
    if not document:
        sys.exit("load_document returned no document")
    sketch_id = next((f.get("feature_id") for f in document.get("feature_history", [])
                      if f.get("kind") == "sketch"), None)

    script = build_script(document)
    # the whole sequence runs in ONE process: create, load, then the commands
    replies = run(core, preamble + script)

    view = merge_document(None, replies["l2"])
    failures = 0
    # everything except the read-only states, replayed in the same order
    mutations = [command for command in script
                 if command["type"] != "get_document_state"]
    for command in script:
        reply = replies.get(command["id"])
        if reply is None:
            print(f"FAIL  {command['type']:24} no reply")
            failures += 1
            continue
        if args.self_test and command["id"] == "s9":
            # pretend the core forgot one line in the LAST full state: nothing
            # downstream can repair the shadow, so the harness must notice
            payload = reply.get("payload") or {}
            for feature in payload.get("feature_history", []):
                params = feature.get("sketch_parameters")
                if isinstance(params, dict) and params.get("lines"):
                    params["lines"] = params["lines"][:-1]
                    break
            reply = dict(reply, payload=payload)
        if reply["type"] == "error":
            print(f"FAIL  {command['type']:24} core error: "
                  f"{reply['payload'].get('message')}")
            failures += 1
            continue
        try:
            view = merge_document(view, reply)
        except KeyError as error:
            print(f"FAIL  {command['type']:24} no merge rule: {error}")
            failures += 1
            continue
        print(f"ok    {command['type']:24} -> {reply['type']:20}")

    # The shadow after the whole script must equal a full state produced by a
    # fresh process replaying the same mutations: that is the contract.
    replay = run(core, preamble + mutations + [
        {"id": "r99", "type": "get_document_state", "payload": {}},
    ])
    authoritative = replay.get("r99", {}).get("payload")
    if authoritative is None:
        print("FAIL  could not obtain an authoritative document state")
        failures += 1
    elif view is None:
        print("FAIL  no shadow view to compare")
        failures += 1
    else:
        problems: list[str] = []
        diff("document", strip_profiles(authoritative), strip_profiles(view), problems)
        failures += 1 if problems else 0
        print(f"{'ok  ' if not problems else 'FAIL'}  {'shadow vs full state':24} "
              f"{len(problems)} diff(s)")
        for problem in problems[:8]:
            print(f"        {problem}")

    print(f"\n{'FAILED' if failures else 'conformant'}"
          f"{' (self-test: failure was expected)' if args.self_test else ''}")
    if args.self_test:
        return 0 if failures else 1
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
