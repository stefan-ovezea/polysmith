// @ts-nocheck
import { spawn } from "node:child_process";
import { existsSync } from "node:fs";
import path from "node:path";

import { describe, expect, it } from "vitest";

import { makeCreateGridfinityFeatureCommand } from "./commands";
import { defaultGridfinityConfig, configToFeatureParameters } from "./defaultConfig";
import type { PluginGeometryOperation } from "../sdk";

function gridfinityOperations() {
  const command = makeCreateGridfinityFeatureCommand(
    configToFeatureParameters(defaultGridfinityConfig),
  );
  return (command.payload as { geometry: PluginGeometryOperation[] }).geometry;
}

function closeTo(value: number | undefined, expected: number) {
  return value !== undefined && Math.abs(value - expected) < 1e-9;
}

describe("Gridfinity geometry recipe", () => {
  it("emits spec-sized repeated base feet for each grid cell", () => {
    const geometry = gridfinityOperations();
    const lowerFoot = geometry.find(
      (operation) =>
        operation.primitive === "tapered_rounded_box" &&
        closeTo(operation.x, 3.2) &&
        closeTo(operation.y, 3.2) &&
        closeTo(operation.z, 0),
    );
    const upperFoot = geometry.find(
      (operation) =>
        operation.primitive === "tapered_rounded_box" &&
        closeTo(operation.x, 2.4) &&
        closeTo(operation.y, 2.4) &&
        closeTo(operation.z, 2.6),
    );

    expect(lowerFoot).toMatchObject({
      width: 35.6,
      depth: 35.6,
      height: 0.8,
      top_width: 37.2,
      top_depth: 37.2,
    });
    expect(upperFoot).toMatchObject({
      width: 37.2,
      depth: 37.2,
      height: 2.15,
      top_width: 41.5,
      top_depth: 41.5,
    });
  });

  it("starts the container cavity above the 4.75 mm base profile", () => {
    const cavityCuts = gridfinityOperations().filter(
      (operation) =>
        operation.operation === "subtract" &&
        operation.primitive === "rounded_box" &&
        operation.z > 4.75,
    );

    expect(cavityCuts.length).toBeGreaterThan(0);
    expect(Math.min(...cavityCuts.map((operation) => operation.z))).toBeCloseTo(
      7.15,
    );
  });

  it("places magnet holes in all four corners of every grid unit", () => {
    const magnetHoles = gridfinityOperations().filter(
      (operation) =>
        operation.operation === "subtract" &&
        operation.primitive === "cylinder" &&
        operation.radius === 3.25 &&
        operation.height === 2.4,
    );

    expect(magnetHoles).toHaveLength(16);
    expect(magnetHoles).toEqual(
      expect.arrayContaining([
        expect.objectContaining({ x: 10.5, y: 10.5 }),
        expect.objectContaining({ x: 31.5, y: 31.5 }),
        expect.objectContaining({ x: 52.5, y: 52.5 }),
        expect.objectContaining({ x: 73.5, y: 73.5 }),
      ]),
    );
  });

  const corePath = path.resolve(
    process.cwd(),
    "../../native/cad-core/build/cad_core",
  );
  const itWithCore = existsSync(corePath) ? it : it.skip;

  itWithCore(
    "builds the generated bin as a native core body",
    async () => {
      const core = spawn(corePath);
      const pending = new Map<string, (event: Record<string, unknown>) => void>();
      let stdoutBuffer = "";
      let stderrBuffer = "";

      core.stdout.on("data", (chunk: Buffer) => {
        stdoutBuffer += chunk.toString("utf8");
        const lines = stdoutBuffer.split("\n");
        stdoutBuffer = lines.pop() ?? "";
        for (const line of lines) {
          if (!line.trim()) {
            continue;
          }
          const event = JSON.parse(line) as Record<string, unknown>;
          const id = event.id;
          if (typeof id === "string" && pending.has(id)) {
            pending.get(id)?.(event);
            pending.delete(id);
          }
        }
      });
      core.stderr.on("data", (chunk: Buffer) => {
        stderrBuffer += chunk.toString("utf8");
      });

      function send(command: Record<string, unknown>) {
        core.stdin.write(`${JSON.stringify(command)}\n`);
      }

      function waitFor(id: string) {
        return new Promise<Record<string, unknown>>((resolve, reject) => {
          const timeout = setTimeout(() => {
            pending.delete(id);
            reject(new Error(`Timed out waiting for ${id}: ${stderrBuffer}`));
          }, 10_000);
          pending.set(id, (event) => {
            clearTimeout(timeout);
            resolve(event);
          });
        });
      }

      try {
        send({ id: "create-document", type: "create_document", payload: {} });
        await waitFor("create-document");

        const createCommand = makeCreateGridfinityFeatureCommand(
          configToFeatureParameters(defaultGridfinityConfig),
        );
        createCommand.id = "create-gridfinity";
        send(createCommand as unknown as Record<string, unknown>);
        const created = await waitFor("create-gridfinity");
        expect(created.type).toBe("document_state");

        send({ id: "viewport", type: "get_viewport_state", payload: {} });
        const viewport = await waitFor("viewport");
        const payload = viewport.payload as {
          bodies: unknown[];
          meshes: Array<{ positions: number[]; indices: number[] }>;
        };

        expect(payload.bodies.length).toBeGreaterThan(0);
        expect(payload.meshes.length).toBeGreaterThan(0);
        expect(payload.meshes[0].positions.length).toBeGreaterThan(0);
        expect(payload.meshes[0].indices.length).toBeGreaterThan(0);
      } finally {
        send({ type: "shutdown", payload: {} });
        core.kill();
      }
    },
    15_000,
  );
});
