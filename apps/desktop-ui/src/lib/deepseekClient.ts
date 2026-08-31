// DeepSeek cloud client. api.deepseek.com serves two compatible API shapes:
//
// - "anthropic": POST {baseUrl}/v1/messages with x-api-key + anthropic-version
//   headers (the /anthropic endpoint; e.g. model deepseek-v4-pro[1m]).
// - "openai":    POST {baseUrl}/chat/completions with Bearer auth and
//   response_format json_object (the official platform endpoint; e.g. model
//   deepseek-chat).
//
// Both endpoints echo permissive CORS headers, so the Tauri renderer can call
// them directly — no Rust proxy.
import type { AiConfig } from "@/config";

import type { AiChatMessage } from "./aiClient";

function trimBaseUrl(baseUrl: string) {
  return baseUrl.replace(/\/+$/, "");
}

interface AnthropicChatResponse {
  // Thinking-capable models (deepseek-v4-pro) prepend a "thinking" block to
  // the content list — the text answer is NOT always content[0].
  content?: Array<{ type?: string; text?: string; thinking?: string }>;
}

interface OpenAiChatResponse {
  choices?: Array<{ message?: { content?: string } }>;
}

async function requestAnthropicStyle(
  config: AiConfig,
  messages: AiChatMessage[],
): Promise<string> {
  const response = await fetch(`${trimBaseUrl(config.baseUrl)}/v1/messages`, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      "x-api-key": config.apiKey,
      "anthropic-version": "2023-06-01",
    },
    body: JSON.stringify({
      model: config.model.trim(),
      max_tokens: 4096,
      // The Anthropic shape takes system separately from the turn list.
      system: messages
        .filter((message) => message.role === "system")
        .map((message) => message.content)
        .join("\n\n"),
      messages: messages.filter((message) => message.role !== "system"),
    }),
  });
  if (!response.ok) {
    throw new Error(`DeepSeek returned HTTP ${response.status}`);
  }
  const payload = (await response.json()) as AnthropicChatResponse;
  // Join every text block; skip thinking blocks entirely.
  const text = (payload.content ?? [])
    .filter((block) => block.type === "text" && Boolean(block.text))
    .map((block) => block.text as string)
    .join("\n")
    .trim();
  if (!text) {
    throw new Error("DeepSeek response did not include message content.");
  }
  return text;
}

async function requestOpenAiStyle(
  config: AiConfig,
  messages: AiChatMessage[],
): Promise<string> {
  const response = await fetch(`${trimBaseUrl(config.baseUrl)}/chat/completions`, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      Authorization: `Bearer ${config.apiKey}`,
    },
    body: JSON.stringify({
      model: config.model.trim(),
      stream: false,
      // Envelope compliance on the OpenAI shape: force a JSON object reply.
      response_format: { type: "json_object" },
      messages,
    }),
  });
  if (!response.ok) {
    throw new Error(`DeepSeek returned HTTP ${response.status}`);
  }
  const payload = (await response.json()) as OpenAiChatResponse;
  const text = payload.choices?.[0]?.message?.content;
  if (!text) {
    throw new Error("DeepSeek response did not include message content.");
  }
  return text;
}

export async function requestDeepseekChat(
  config: AiConfig,
  messages: AiChatMessage[],
): Promise<string> {
  if (!config.apiKey) {
    throw new Error(
      'DeepSeek API key not found — add "deepseek_api_key" to ~/.polysmith.',
    );
  }
  if (config.apiStyle === "openai") {
    return requestOpenAiStyle(config, messages);
  }
  return requestAnthropicStyle(config, messages);
}

// Best-effort model listing. Not every endpoint deployment exposes a models
// route — fall back to the configured model so the Settings dropdown still
// shows a usable entry.
export async function listDeepseekModels(config: AiConfig): Promise<string[]> {
  const configured = config.model.trim();
  const fallback = configured ? [configured] : [];
  try {
    const url =
      config.apiStyle === "openai"
        ? `${trimBaseUrl(config.baseUrl)}/models`
        : `${trimBaseUrl(config.baseUrl)}/v1/models`;
    const headers =
      config.apiStyle === "openai"
        ? { Authorization: `Bearer ${config.apiKey}` }
        : { "x-api-key": config.apiKey };
    const response = await fetch(url, { headers });
    if (!response.ok) {
      return fallback;
    }
    const payload = (await response.json()) as {
      data?: Array<{ id?: string }>;
      models?: Array<{ name?: string }>;
    };
    const names = (payload.data ?? payload.models ?? [])
      .map((entry) => entry.id ?? (entry as { name?: string }).name)
      .filter((name): name is string => Boolean(name));
    return names.length > 0 ? names : fallback;
  } catch {
    return fallback;
  }
}
