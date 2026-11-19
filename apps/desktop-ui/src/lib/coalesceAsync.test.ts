import { describe, expect, it, vi } from "vitest";

import { coalesceAsync } from "./coalesceAsync";

describe("coalesceAsync", () => {
  it("runs the operation once for concurrent callers", async () => {
    let resolveRun: (value: string) => void = () => {};
    const operation = vi.fn(
      () =>
        new Promise<string>((resolve) => {
          resolveRun = resolve;
        }),
    );
    const startOnce = coalesceAsync(operation);

    const first = startOnce();
    const second = startOnce();
    expect(operation).toHaveBeenCalledTimes(1);
    expect(second).toBe(first);

    resolveRun("connected");
    await expect(first).resolves.toBe("connected");
    await expect(second).resolves.toBe("connected");
  });

  it("starts again once the run has finished", async () => {
    const operation = vi.fn(async () => "ok");
    const startOnce = coalesceAsync(operation);

    await startOnce();
    await startOnce();

    expect(operation).toHaveBeenCalledTimes(2);
  });

  it("lets a failed run be retried", async () => {
    const operation = vi
      .fn<() => Promise<string>>()
      .mockRejectedValueOnce(new Error("spawn failed"))
      .mockResolvedValueOnce("connected");
    const startOnce = coalesceAsync(operation);

    await expect(startOnce()).rejects.toThrow("spawn failed");
    await expect(startOnce()).resolves.toBe("connected");
    expect(operation).toHaveBeenCalledTimes(2);
  });
});
