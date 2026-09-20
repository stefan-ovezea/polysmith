# polysmith-vision MCP server

Local MCP server that lets the Claude Code session model read screenshots.

**Why it exists:** the session model (deepseek-v4-pro, via the user's
DeepSeek gateway) never receives image blocks — the gateway substitutes
`[Unsupported Image]` before the model sees them. `deepseek-flash` is the
only vision-capable model on that gateway. This server extracts pasted
images from the session transcript (or reads image files from disk) and
sends them to `deepseek-flash` through the user's own gateway, returning
the text description as a tool result.

This replaces the old Ollama-based `deepseek-vision` MCP (see
`wiki/Vision-MCP.md`) — same job, but no local Ollama, no venv, no model
pulls. **Stdlib only** (Python 3.10+, no pip install).

## Tools

| Tool | What it does |
|---|---|
| `describe_latest_screenshot` | Finds the newest pasted image in the current session transcript and describes it. Optional `question` for a specific ask, `index` to reach older images (0 = newest). |
| `describe_image_file` | Describes an image file on disk (`path` + optional `question`). Supports png/jpg/webp/gif. |

## Configuration

- Registered in the project's `.mcp.json` as `polysmith-vision`
  (`python tools/vision-mcp/server.py`, relative to the repo root).
- Credentials: `~/.polysmith` JSON with `deepseek_api_key` and
  `deepseek_base_url` (default `https://api.deepseek.com/anthropic/v1`).
  The key is read at runtime, never stored in the repo.
- `POLYSMITH_SESSION_DIR` env var overrides the transcript directory
  (default: `~/.claude/projects/<cwd-slug>`).

## How the transcript extraction works

Claude Code stores pasted screenshots as base64 image blocks inside user
messages of the session `.jsonl`. The server takes the newest `.jsonl`
in the project's session directory, walks the lines for `user` messages
containing `content` blocks of `type: "image"`, and uses the last one
(`index` walks backwards from there).

If no image block exists in the transcript at all, the attachment never
arrived — the tool returns an error telling Claude to ask the user to
re-paste the image (rather than "vision is broken").
