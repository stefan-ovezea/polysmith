// @vitest-environment node
// Pins the /api/chat wire format. stream:false + format:"json" keep the
// response a single JSON blob, and think:false is load-bearing for
// thinking-capable models (gemma4) — with thinking enabled they emit the
// envelope into message.thinking and return an empty content field.
import { afterEach, describe, expect, it, vi } from "vitest";

import type { AiConfig } from "@/config";
import { listOllamaModels, requestOllamaChat } from "./ollamaClient";

const config: AiConfig = {
  enabled: true,
  provider: "ollama",
  baseUrl: "http://localhost:11434",
  model: "gemma4:12b",
  apiKey: "",
  apiStyle: "anthropic",
  previewBeforeRun: true,
  maxAgentSteps: 8,
};

function jsonResponse(body: unknown, ok = true) {
  return {
    ok,
    status: ok ? 200 : 500,
    json: async () => body,
  } as unknown as Response;
}

afterEach(() => {
  vi.unstubAllGlobals();
});

describe("requestOllamaChat", () => {
  it("posts the required wire format and returns the message content", async () => {
    const fetchMock = vi.fn(async (_url: string, init?: RequestInit) =>
      jsonResponse({ message: { content: '{"message":"ok"}' } }),
    );
    vi.stubGlobal("fetch", fetchMock);

    const content = await requestOllamaChat(config, [
      { role: "system", content: "system prompt" },
      { role: "user", content: "user prompt" },
    ]);

    expect(content).toBe('{"message":"ok"}');
    expect(fetchMock).toHaveBeenCalledTimes(1);
    const [url, init] = fetchMock.mock.calls[0] as [string, RequestInit];
    expect(url).toBe("http://localhost:11434/api/chat");
    const body = JSON.parse(String(init.body));
    expect(body).toMatchObject({
      model: "gemma4:12b",
      stream: false,
      format: "json",
      think: false,
      options: { num_ctx: 16384 },
      messages: [
        { role: "system", content: "system prompt" },
        { role: "user", content: "user prompt" },
      ],
    });
  });

  it("throws on HTTP errors", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse({}, false)));
    await expect(
      requestOllamaChat(config, [{ role: "user", content: "hi" }]),
    ).rejects.toThrow(/HTTP 500/);
  });

  it("throws when the response has no message content", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn(async () => jsonResponse({ message: { thinking: "..." } })),
    );
    await expect(
      requestOllamaChat(config, [{ role: "user", content: "hi" }]),
    ).rejects.toThrow(/did not include message content/);
  });
});

describe("listOllamaModels", () => {
  it("maps /api/tags model names", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn(async () =>
        jsonResponse({
          models: [{ name: "gemma4:12b" }, { name: "gemma3:4b" }],
        }),
      ),
    );
    const models = await listOllamaModels(config.baseUrl);
    expect(models).toEqual(["gemma4:12b", "gemma3:4b"]);
  });

  it("throws on HTTP errors", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse({}, false)));
    await expect(listOllamaModels(config.baseUrl)).rejects.toThrow(/HTTP 500/);
  });
});
