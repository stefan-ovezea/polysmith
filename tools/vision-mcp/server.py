#!/usr/bin/env python3
"""polysmith-vision — local MCP server that describes screenshots.

The Claude Code session model never receives image blocks (the DeepSeek
gateway substitutes "[Unsupported Image]" before the model sees them), so
pasted screenshots are read out of the session transcript and sent to
`deepseek-flash` — the only vision-capable model on the user's gateway —
through the user's OWN gateway endpoint. The description comes back as
tool-result text the session model can read.

Stdlib only: no pip install, no venv, no model pulls.
Credentials are read at runtime from ~/.polysmith
(JSON keys: deepseek_api_key, deepseek_base_url) — never inline them.

MCP transport: newline-delimited JSON-RPC over stdin/stdout.
"""

import base64
import json
import os
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path

MODEL = "deepseek-flash"  # only vision-capable model on the user's gateway
MAX_TOKENS = 1200
DEFAULT_GATEWAY = "https://api.deepseek.com/anthropic/v1"

DEFAULT_QUESTION = (
    "This screenshot was pasted into a CAD app development session (PolySmith). "
    "Describe it precisely and concretely: overall layout; every visible label, "
    "value, and text (quote error messages exactly); UI element positions in "
    "approximate pixels or relative terms; visual states (selected, highlighted, "
    "disabled); anything that looks wrong or unfinished. End with the three most "
    "likely things the user wants to know or fix based on this screenshot."
)

TOOLS = [
    {
        "name": "describe_latest_screenshot",
        "description": (
            "Describe the most recent screenshot the user pasted into this Claude "
            "Code session. Extracts the image from the session transcript and sends "
            "it to deepseek-flash through the user's own DeepSeek gateway. Use this "
            "whenever the user pastes a screenshot and the model cannot see it."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "question": {
                    "type": "string",
                    "description": "Optional: a specific question to ask about the "
                    "screenshot instead of the default full description.",
                },
                "index": {
                    "type": "integer",
                    "description": "Which pasted image to use: 0 = newest (default), "
                    "1 = second newest, ...",
                    "default": 0,
                },
            },
        },
    },
    {
        "name": "describe_image_file",
        "description": (
            "Describe an image file on disk (png/jpg/webp/gif) by sending it to "
            "deepseek-flash through the user's own DeepSeek gateway."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {
                    "type": "string",
                    "description": "Absolute or ~-relative path to the image file.",
                },
                "question": {
                    "type": "string",
                    "description": "Optional: a specific question to ask instead of "
                    "the default full description.",
                },
            },
            "required": ["path"],
        },
    },
]

MEDIA_TYPES = {
    ".png": "image/png",
    ".jpg": "image/jpeg",
    ".jpeg": "image/jpeg",
    ".webp": "image/webp",
    ".gif": "image/gif",
}


def read_gateway_config():
    """deepseek_api_key + deepseek_base_url from ~/.polysmith (JSON)."""
    cfg_path = Path.home() / ".polysmith"
    try:
        cfg = json.loads(cfg_path.read_text(encoding="utf-8"))
    except Exception as e:
        raise RuntimeError(f"cannot read ~/.polysmith ({e})")
    key = cfg.get("deepseek_api_key")
    if not key:
        raise RuntimeError("~/.polysmith has no deepseek_api_key")
    return key, cfg.get("deepseek_base_url", DEFAULT_GATEWAY).rstrip("/")


def call_flash(image_b64, media_type, question):
    """POST the image + question to deepseek-flash (Anthropic Messages format)."""
    api_key, base_url = read_gateway_config()
    prompt = (question or "").strip() or DEFAULT_QUESTION
    body = {
        "model": MODEL,
        "max_tokens": MAX_TOKENS,
        # Without this, flash spends the whole budget in a thinking block
        # and returns no text at all.
        "thinking": {"type": "disabled"},
        "messages": [
            {
                "role": "user",
                "content": [
                    {
                        "type": "image",
                        "source": {
                            "type": "base64",
                            "media_type": media_type,
                            "data": image_b64,
                        },
                    },
                    {"type": "text", "text": prompt},
                ],
            }
        ],
    }
    req = urllib.request.Request(
        f"{base_url}/messages",
        data=json.dumps(body).encode("utf-8"),
        headers={
            "x-api-key": api_key,
            "anthropic-version": "2023-06-01",
            "content-type": "application/json",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=180) as resp:
            payload = json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        detail = e.read().decode("utf-8", errors="replace")[:400]
        raise RuntimeError(f"gateway returned HTTP {e.code}: {detail}")
    text = "".join(
        b.get("text", "")
        for b in payload.get("content", [])
        if isinstance(b, dict) and b.get("type") == "text"
    ).strip()
    if not text:
        raise RuntimeError("gateway returned no text (thinking likely not disabled?)")
    return text


def transcript_dir():
    """~/.claude/projects/<cwd-slug> — e.g. C:\\SRC\\polysmith → C--SRC-polysmith."""
    override = os.environ.get("POLYSMITH_SESSION_DIR")
    if override:
        return Path(override).expanduser()
    slug = re.sub(r"[^A-Za-z0-9]", "-", os.getcwd())
    return Path.home() / ".claude" / "projects" / slug


def find_images(session_dir, index):
    """All base64 image blocks from user messages in the NEWEST transcript."""
    files = sorted(session_dir.glob("*.jsonl"), key=lambda p: p.stat().st_mtime)
    if not files:
        raise RuntimeError(
            f"no .jsonl transcripts under {session_dir} (the transcript files "
            "live flat in the projects dir, e.g. "
            "~/.claude/projects/C--SRC-polysmith/<session-id>.jsonl)"
        )
    candidates = []
    with open(files[-1], encoding="utf-8", errors="replace") as f:
        for line in f:
            try:
                msg = json.loads(line)
            except json.JSONDecodeError:
                continue
            if msg.get("type") != "user":
                continue
            content = msg.get("message", {}).get("content")
            if isinstance(content, str):
                continue
            for block in content or []:
                if not isinstance(block, dict) or block.get("type") != "image":
                    continue
                src = block.get("source") or {}
                if src.get("type") == "base64" and src.get("data"):
                    candidates.append(
                        (src["data"], src.get("media_type") or "image/png")
                    )
    if not candidates:
        raise RuntimeError(
            "no image found in the current session transcript — the attachment "
            "never arrived; ask the user to re-paste the image"
        )
    if index >= len(candidates):
        raise RuntimeError(
            f"only {len(candidates)} pasted image(s) in this session "
            f"(index {index} out of range)"
        )
    return candidates[-(index + 1)]


def tool_latest_screenshot(args):
    index = int(args.get("index") or 0)
    data, media = find_images(transcript_dir(), index)
    return call_flash(data, media, args.get("question"))


def tool_image_file(args):
    p = Path(args["path"]).expanduser()
    if not p.is_file():
        raise RuntimeError(f"no such file: {p}")
    media = MEDIA_TYPES.get(p.suffix.lower())
    if not media:
        raise RuntimeError(
            f"unsupported image type {p.suffix!r} (use png/jpg/webp/gif)"
        )
    data = base64.b64encode(p.read_bytes()).decode("ascii")
    return call_flash(data, media, args.get("question"))


def send(payload):
    sys.stdout.write(json.dumps(payload, ensure_ascii=False) + "\n")
    sys.stdout.flush()


def main():
    # Windows console default is cp1252 — gateway replies contain ≈/°/— etc.
    sys.stdout.reconfigure(encoding="utf-8")
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            continue
        method, msg_id = msg.get("method"), msg.get("id")
        if method == "initialize":
            send(
                {
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "result": {
                        "protocolVersion": msg.get("params", {}).get(
                            "protocolVersion", "2024-11-05"
                        ),
                        "capabilities": {"tools": {}},
                        "serverInfo": {"name": "polysmith-vision", "version": "1.0.0"},
                    },
                }
            )
        elif method == "notifications/initialized":
            pass  # notifications get no response
        elif method == "ping":
            send({"jsonrpc": "2.0", "id": msg_id, "result": {}})
        elif method == "tools/list":
            send({"jsonrpc": "2.0", "id": msg_id, "result": {"tools": TOOLS}})
        elif method == "tools/call":
            params = msg.get("params", {})
            name = params.get("name", "")
            args = params.get("arguments") or {}
            try:
                if name == "describe_latest_screenshot":
                    text = tool_latest_screenshot(args)
                elif name == "describe_image_file":
                    text = tool_image_file(args)
                else:
                    raise RuntimeError(f"unknown tool {name!r}")
                send(
                    {
                        "jsonrpc": "2.0",
                        "id": msg_id,
                        "result": {"content": [{"type": "text", "text": text}]},
                    }
                )
            except Exception as e:
                send(
                    {
                        "jsonrpc": "2.0",
                        "id": msg_id,
                        "result": {
                            "content": [{"type": "text", "text": f"ERROR: {e}"}],
                            "isError": True,
                        },
                    }
                )
        # other notifications (initialized handled above, everything else) — ignore


if __name__ == "__main__":
    main()
