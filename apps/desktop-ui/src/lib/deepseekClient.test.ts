// @vitest-environment node
// Pins both DeepSeek API shapes. The "anthropic" style is the proven path for
// deepseek-v4-pro[1m] keys (same endpoint this repo's agent tooling uses);
// the "openai" style covers the official platform deepseek-chat model.
import { afterEach, describe, expect, it, vi } from "vitest";

import type { AiConfig } from "@/config";
import { listDeepseekModels, requestDeepseekChat } from "./deepseekClient";

function makeConfig(overrides: Partial<AiConfig>): AiConfig {
  return {
    enabled: true,
    provider: "deepseek",
    baseUrl: "https://api.deepseek.com/anthropic",
    model: "deepseek-v4-pro[1m]",
    apiKey: "sk-test",
    apiStyle: "anthropic",
    previewBeforeRun: true,
    maxAgentSteps: 8,
    ...overrides,
  };
}

function jsonResponse(body: unknown, ok = true) {
  return {
    ok,
    status: ok ? 200 : 401,
    json: async () => body,
  } as unknown as Response;
}

afterEach(() => {
  vi.unstubAllGlobals();
});

describe("requestDeepseekChat (anthropic style)", () => {
  it("posts to /v1/messages with x-api-key and separates system", async () => {
    const fetchMock = vi.fn(async () =>
      jsonResponse({ content: [{ type: "text", text: '{"message":"ok"}' }] }),
    );
    vi.stubGlobal("fetch", fetchMock);

    const content = await requestDeepseekChat(makeConfig({}), [
      { role: "system", content: "system prompt" },
      { role: "user", content: "user prompt" },
    ]);

    expect(content).toBe('{"message":"ok"}');
    const [url, init] = fetchMock.mock.calls[0] as unknown as [string, RequestInit];
    expect(url).toBe("https://api.deepseek.com/anthropic/v1/messages");
    expect(init.headers).toMatchObject({
      "x-api-key": "sk-test",
      "anthropic-version": "2023-06-01",
    });
    const body = JSON.parse(String(init.body));
    expect(body).toMatchObject({
      model: "deepseek-v4-pro[1m]",
      max_tokens: 4096,
      // Thinking is off: the envelope doesn't need reasoning and it costs
      // ~6x the latency on flash when left on.
      thinking: { type: "disabled" },
      system: "system prompt",
      messages: [{ role: "user", content: "user prompt" }],
    });
  });

  it("extracts the text block when a thinking block comes first", async () => {
    // deepseek-v4-pro prepends a thinking block — content[0] has no text.
    vi.stubGlobal(
      "fetch",
      vi.fn(async () =>
        jsonResponse({
          content: [
            { type: "thinking", thinking: "reasoning..." },
            { type: "text", text: '{"message":"ok"}' },
          ],
        }),
      ),
    );
    const content = await requestDeepseekChat(makeConfig({}), [
      { role: "user", content: "hi" },
    ]);
    expect(content).toBe('{"message":"ok"}');
  });

  it("throws a clear error when no API key is configured", async () => {
    await expect(
      requestDeepseekChat(makeConfig({ apiKey: "" }), [
        { role: "user", content: "hi" },
      ]),
    ).rejects.toThrow(/~\/\.polysmith/);
  });

  it("throws on HTTP errors and empty content", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse({}, false)));
    await expect(
      requestDeepseekChat(makeConfig({}), [{ role: "user", content: "hi" }]),
    ).rejects.toThrow(/HTTP 401/);

    vi.stubGlobal(
      "fetch",
      vi.fn(async () => jsonResponse({ content: [] })),
    );
    await expect(
      requestDeepseekChat(makeConfig({}), [{ role: "user", content: "hi" }]),
    ).rejects.toThrow(/did not include message content/);
  });
});

describe("requestDeepseekChat (openai style)", () => {
  it("posts to /chat/completions with Bearer and json_object format", async () => {
    const fetchMock = vi.fn(async () =>
      jsonResponse({ choices: [{ message: { content: '{"message":"ok"}' } }] }),
    );
    vi.stubGlobal("fetch", fetchMock);

    const config = makeConfig({
      baseUrl: "https://api.deepseek.com",
      model: "deepseek-chat",
      apiStyle: "openai",
    });
    const content = await requestDeepseekChat(config, [
      { role: "system", content: "system prompt" },
      { role: "user", content: "user prompt" },
    ]);

    expect(content).toBe('{"message":"ok"}');
    const [url, init] = fetchMock.mock.calls[0] as unknown as [string, RequestInit];
    expect(url).toBe("https://api.deepseek.com/chat/completions");
    expect(init.headers).toMatchObject({ Authorization: "Bearer sk-test" });
    const body = JSON.parse(String(init.body));
    expect(body).toMatchObject({
      model: "deepseek-chat",
      stream: false,
      thinking: { type: "disabled" },
      response_format: { type: "json_object" },
      messages: [
        { role: "system", content: "system prompt" },
        { role: "user", content: "user prompt" },
      ],
    });
  });
});

describe("listDeepseekModels", () => {
  it("maps the models list from the root endpoint", async () => {
    const fetchMock = vi.fn(async () =>
      jsonResponse({
        data: [
          { id: "deepseek-v4-flash" },
          { id: "deepseek-v4-pro" },
          { id: "deepseek-v4-flash-vision-exp" },
        ],
      }),
    );
    vi.stubGlobal("fetch", fetchMock);

    const models = await listDeepseekModels(
      makeConfig({ baseUrl: "https://api.deepseek.com", apiStyle: "openai" }),
    );
    expect(models).toEqual([
      "deepseek-v4-flash",
      "deepseek-v4-pro",
      "deepseek-v4-flash-vision-exp",
    ]);
  });

  it("strips the /anthropic suffix and hits the root /models route", async () => {
    const fetchMock = vi.fn(async () =>
      jsonResponse({ data: [{ id: "deepseek-v4-flash" }] }),
    );
    vi.stubGlobal("fetch", fetchMock);

    const models = await listDeepseekModels(makeConfig({}));
    expect(models).toEqual(["deepseek-v4-flash"]);
    const [url] = fetchMock.mock.calls[0] as unknown as [string];
    expect(String(url)).toBe("https://api.deepseek.com/models");
  });

  it("falls back to the configured model when listing fails", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse({}, false)));
    const models = await listDeepseekModels(makeConfig({}));
    expect(models).toEqual(["deepseek-v4-pro[1m]"]);
  });

  it("falls back to the documented v4 models when nothing is configured", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse({}, false)));
    const models = await listDeepseekModels(makeConfig({ model: "" }));
    expect(models).toEqual([
      "deepseek-v4-flash",
      "deepseek-v4-pro",
      "deepseek-v4-flash-vision-exp",
    ]);
  });
});
