# Vision MCP

Local AI vision for understanding screenshots and images. The MCP server sends
images to a local [Ollama](https://ollama.com/) multimodal model — nothing
leaves your machine.

**Default model**: `qwen3.5:2b` (configurable via `VISION_MODEL` env var).

## Installation

The server lives at `third_party/deepseek-vision-mcp/`.

```bash
cd third_party/deepseek-vision-mcp
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

### Prerequisites

- **Python 3.10+**
- **Ollama** running locally (`ollama serve`)
- A vision-capable model pulled:
  ```bash
  ollama pull qwen3.5:2b
  ```

## How it works

1. Claude Code invokes the `deepseek-vision` MCP server
2. The server receives an image path (screenshot, exported image, etc.)
3. The image is sent to the local Ollama model for analysis
4. The model's description is returned to Claude Code

## Configuration

`.mcp.json` (project root):

```json
{
  "mcpServers": {
    "deepseek-vision": {
      "command": "./third_party/deepseek-vision-mcp/.venv/bin/python3",
      "args": ["./third_party/deepseek-vision-mcp/server.py"]
    }
  }
}
```

### Switching models

Set the `VISION_MODEL` environment variable before launching Claude Code:

```bash
export VISION_MODEL=qwen3-vl:2b
```

Any Ollama vision model works (`gemma3:4b`, `llava:13b`, etc.).

## Teammate setup

One-time setup after clone:

```bash
cd third_party/deepseek-vision-mcp
python3 -m venv .venv && source .venv/bin/activate && pip install -r requirements.txt
ollama pull qwen3.5:2b
```

Then restart Claude Code — the vision tool is available immediately.
