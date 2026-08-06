# Vision MCP

Local AI vision for understanding screenshots and images. The MCP server sends
images to a local [Ollama](https://ollama.com/) multimodal model — nothing
leaves your machine.

**Default model**: `qwen3.5:2b` with full GPU offload, tagged as
`qwen3.5-vision:latest`. Configurable via the `OLLAMA_VISION_MODEL` env var
in `.mcp.json`.

## Installation

The server is a git submodule at `third_party/deepseek-vision-mcp/`.

```bash
git submodule update --init third_party/deepseek-vision-mcp
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

### GPU optimisation (recommended)

The default model may not fully load onto GPU. Create an optimised variant
that forces all layers to GPU and uses a smaller context window:

```bash
cat > /tmp/qwen-vision.Modelfile << 'EOF'
FROM qwen3.5:2b
PARAMETER num_gpu 99
PARAMETER num_ctx 2048
EOF
ollama create qwen3.5-vision:latest -f /tmp/qwen-vision.Modelfile
```

The `.mcp.json` config already points to `qwen3.5-vision:latest`.

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
      "args": ["./third_party/deepseek-vision-mcp/server.py"],
      "env": {
        "OLLAMA_VISION_MODEL": "qwen3.5-vision:latest"
      }
    }
  }
}
```

### Switching models

Change the `OLLAMA_VISION_MODEL` env var in `.mcp.json`. Any Ollama vision
model works (`minicpm-v`, `llava:7b`, `gemma3:4b`, etc.).

## Teammate setup

One-time setup after clone:

```bash
git submodule update --init third_party/deepseek-vision-mcp
cd third_party/deepseek-vision-mcp
python3 -m venv .venv && source .venv/bin/activate && pip install -r requirements.txt
ollama pull qwen3.5:2b
ollama create qwen3.5-vision:latest -f /tmp/qwen-vision.Modelfile
```

Then restart Claude Code — the vision tool is available immediately.
