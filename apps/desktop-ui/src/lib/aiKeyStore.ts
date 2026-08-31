// DeepSeek connection settings. The key AND the base URL live ONLY in the
// user-owned `~/.polysmith` file — never in the repo, never in the persisted
// app config. The Rust shell reads the file (src-tauri/src/ai_key.rs); the
// renderer asks it for the settings and caches the answer for the session.
import { invoke } from "@tauri-apps/api/core";

export interface AiHomeSettings {
  apiKey: string;
  baseUrl: string;
  sourcePath: string;
  // True when the Rust command itself was unavailable (stale build running,
  // or browser-only dev) — distinguishable from "file read but no key".
  commandError: boolean;
}

let cachedSettings: AiHomeSettings | null = null;

export async function getAiSettings(): Promise<AiHomeSettings> {
  if (cachedSettings !== null) {
    return cachedSettings;
  }
  try {
    const result = (await invoke("read_ai_settings")) as Omit<
      AiHomeSettings,
      "commandError"
    >;
    cachedSettings = { ...result, commandError: false };
  } catch {
    // Browser-only dev (`pnpm ui:dev`) has no Tauri runtime, and a stale
    // desktop build predating the command throws here too.
    cachedSettings = { apiKey: "", baseUrl: "", sourcePath: "", commandError: true };
  }
  return cachedSettings;
}
