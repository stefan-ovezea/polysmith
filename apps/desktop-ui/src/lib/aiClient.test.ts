// @vitest-environment node
// Pins the provider dispatch: ollama calls go straight through; deepseek
// calls get their key filled from the ~/.polysmith store when the config
// carries none (the app never persists the key).
import { afterEach, describe, expect, it, vi } from "vitest";

import type { AiConfig } from "@/config";
import { requestAiChat } from "./aiClient";

function makeConfig(overrides: Partial<AiConfig>): AiConfig {
  return {
    enabled: true,
    provider: "ollama",
    baseUrl: "http://localhost:11434",
    model: "gemma4:12b",
    apiKey: "",
    apiStyle: "anthropic",
    previewBeforeRun: true,
    maxAgentSteps: 8,
    ...overrides,
  };
}

afterEach(() => {
  vi.unstubAllGlobals();
  vi.resetModules();
});

describe("requestAiChat dispatch", () => {
  it("routes ollama providers to /api/chat", async () => {
    const fetchMock = vi.fn(async () =>
      ({
        ok: true,
        status: 200,
        json: async () => ({ message: { content: "ok" } }),
      }) as unknown as Response,
    );
    vi.stubGlobal("fetch", fetchMock);

    const content = await requestAiChat(makeConfig({}), [
      { role: "user", content: "hi" },
    ]);
    expect(content).toBe("ok");
    const [url] = fetchMock.mock.calls[0] as unknown as [string];
    expect(String(url)).toContain("/api/chat");
  });

  it("fills the deepseek key from the key store when config has none", async () => {
    vi.doMock("./aiKeyStore", () => ({
      getAiSettings: async () => ({
        apiKey: "sk-from-home",
        baseUrl: "https://api.deepseek.com/anthropic",
      }),
    }));
    const fetchMock = vi.fn(async () =>
      ({
        ok: true,
        status: 200,
        json: async () => ({
          content: [{ type: "text", text: "ok" }],
        }),
      }) as unknown as Response,
    );
    vi.stubGlobal("fetch", fetchMock);

    // Import after the mock so the dispatcher picks up the stub.
    const { requestAiChat: mockedRequest } = await import("./aiClient");
    await mockedRequest(
      makeConfig({
        provider: "deepseek",
        baseUrl: "https://api.deepseek.com/anthropic",
        model: "deepseek-v4-pro[1m]",
      }),
      [{ role: "user", content: "hi" }],
    );
    const [url, init] = fetchMock.mock.calls[0] as unknown as [string, RequestInit];
    expect(String(url)).toBe("https://api.deepseek.com/anthropic/v1/messages");
    expect(init.headers).toMatchObject({ "x-api-key": "sk-from-home" });
  });
});
