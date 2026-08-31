// Provider dispatch for the AI assistant: one entry point for the panel and
// the headless harness, routing to the Ollama or DeepSeek clients by
// AiConfig.provider.
import type { AiConfig } from "@/config";

import { getAiSettings } from "./aiKeyStore";
import { listDeepseekModels, requestDeepseekChat } from "./deepseekClient";
import { listOllamaModels, requestOllamaChat } from "./ollamaClient";

export type AiChatMessage = {
  role: "system" | "user" | "assistant";
  content: string;
};

export async function listAiModels(config: AiConfig): Promise<string[]> {
  if (config.provider === "deepseek") {
    // The app never persists the key or the cloud URL, so the config's
    // apiKey is always empty here — the /models endpoint would 401 and the
    // lister would fall back to the single configured model. Merge the
    // ~/.polysmith settings first, exactly like requestAiChat does.
    const settings = await getAiSettings();
    const apiKey = config.apiKey || settings.apiKey;
    const baseUrl = settings.baseUrl || config.baseUrl;
    return listDeepseekModels({ ...config, apiKey, baseUrl });
  }
  return listOllamaModels(config.baseUrl);
}

export async function requestAiChat(
  config: AiConfig,
  messages: AiChatMessage[],
): Promise<string> {
  if (config.provider === "deepseek") {
    // The app never persists the key or the cloud URL; when the caller has
    // not supplied them (harness env), fetch them from ~/.polysmith via the
    // Rust shell. The file URL overrides the config URL when present.
    const settings = await getAiSettings();
    const apiKey = config.apiKey || settings.apiKey;
    const baseUrl = settings.baseUrl || config.baseUrl;
    return requestDeepseekChat({ ...config, apiKey, baseUrl }, messages);
  }
  return requestOllamaChat(config, messages);
}
