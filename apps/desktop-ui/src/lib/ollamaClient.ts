import type { AiConfig } from "@/config";

interface OllamaTagsResponse {
  models?: Array<{ name?: string }>;
}

interface OllamaChatResponse {
  message?: {
    content?: string;
  };
  response?: string;
}

function trimBaseUrl(baseUrl: string) {
  return baseUrl.replace(/\/+$/, "");
}

export async function listOllamaModels(baseUrl: string): Promise<string[]> {
  const response = await fetch(`${trimBaseUrl(baseUrl)}/api/tags`);
  if (!response.ok) {
    throw new Error(`Ollama returned HTTP ${response.status}`);
  }
  const payload = (await response.json()) as OllamaTagsResponse;
  return (payload.models ?? [])
    .map((model) => model.name)
    .filter((name): name is string => Boolean(name));
}

export async function requestOllamaChat(
  config: AiConfig,
  messages: Array<{ role: "system" | "user" | "assistant"; content: string }>,
): Promise<string> {
  const response = await fetch(`${trimBaseUrl(config.baseUrl)}/api/chat`, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify({
      model: config.model.trim(),
      stream: false,
      format: "json",
      // Disable thinking on capable models (e.g. gemma4). With thinking on,
      // gemma4 emits everything into message.thinking and returns an empty
      // content field before the JSON envelope is produced. Verified accepted
      // by both thinking-capable (gemma4) and non-thinking (gemma3) models on
      // Ollama 0.33+.
      think: false,
      options: {
        // Ollama defaults to a 4096-token context. The system prompt plus the
        // CAD state summary plus multi-turn history exceeds that by the third
        // agent step, which truncates the reply (done_reason: "length").
        // gemma models support far larger windows; 16384 covers realistic
        // multi-batch sketches with headroom.
        num_ctx: 16384,
      },
      messages,
    }),
  });

  if (!response.ok) {
    throw new Error(`Ollama returned HTTP ${response.status}`);
  }

  const payload = (await response.json()) as OllamaChatResponse;
  const content = payload.message?.content ?? payload.response;
  if (!content) {
    throw new Error("Ollama response did not include message content.");
  }
  return content;
}
