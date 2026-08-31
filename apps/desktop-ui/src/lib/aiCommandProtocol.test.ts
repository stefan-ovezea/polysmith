// @vitest-environment node
// Unit tests for the AI envelope safety layer. These pin the repair /
// deferral behaviors the panel relies on so prompt or schema drift cannot
// silently change how model output is validated and dispatched.
import { describe, expect, it } from "vitest";
import { z } from "zod";

import type { CoreCommand, DocumentState, ViewportState } from "@/types";
import {
  formatAiCommandError,
  parseAiCommandEnvelope,
  prepareAiCommandBatchForState,
  type AiExecutableCommand,
} from "./aiCommandProtocol";

// Fixtures are deliberately minimal fakes: overrides are untyped so callers
// can build partial state without satisfying every nested interface.
function makeDocument(overrides: Record<string, unknown> = {}): DocumentState {
  const document: unknown = {
    document_id: "doc-1",
    name: "Untitled",
    units: "mm",
    revision: 1,
    selected_feature_id: null,
    selected_reference_id: null,
    selected_face_id: null,
    selected_edge_ids: [],
    selected_vertex_ids: [],
    active_sketch_plane_id: null,
    active_sketch_face_id: null,
    active_sketch_feature_id: null,
    active_sketch_tool: null,
    selected_sketch_vertex_id: null,
    selected_sketch_entity_id: null,
    selected_sketch_vertex_ids: [],
    selected_sketch_entity_ids: [],
    selected_sketch_dimension_id: null,
    selected_sketch_profile_id: null,
    selected_sketch_profile_ids: [],
    timeline_cursor: null,
    feature_history: [],
    parameters: [],
    appearance: { body_colors: [], face_colors: [] },
    cam: {},
    ...overrides,
  };
  return document as DocumentState;
}

const documentWithActiveSketch = makeDocument({
  active_sketch_feature_id: "sketch-1",
  feature_history: [
    {
      feature_id: "sketch-1",
      kind: "sketch",
      name: "Sketch 1",
      status: "ok",
      parameters_summary: "",
      sketch_parameters: {
        profiles: [
          {
            profile_id: "profile-1",
            kind: "polygon",
            vertex_ids: [],
            line_ids: ["line-1"],
            points: [],
            inner_loops: [],
            source_circle_id: null,
            center_x: 0,
            center_y: 0,
            radius: 0,
          },
        ],
      },
    },
  ],
});

const emptyViewport = {} as unknown as ViewportState;

const viewportWithBody = {
  bodies: [{ id: "feature-3" }],
  solid_faces: [
    {
      face_id: "feature-3:face:5",
      owner_id: "feature-3",
      sketchability: "planar",
    },
  ],
} as unknown as ViewportState;

function rectangleCommand(): AiExecutableCommand {
  return {
    id: "test-id",
    type: "add_sketch_rectangle",
    payload: {
      start_x: 0,
      start_y: 0,
      end_x: 10,
      end_y: 10,
      is_construction: false,
    },
  } as AiExecutableCommand;
}

describe("parseAiCommandEnvelope", () => {
  it("accepts a valid minimal envelope and assigns command ids", () => {
    const envelope = parseAiCommandEnvelope(
      JSON.stringify({
        message: "Drawing a rectangle.",
        commands: [
          {
            type: "add_sketch_rectangle",
            payload: {
              start_x: 0,
              start_y: 0,
              end_x: 60,
              end_y: 40,
              is_construction: false,
            },
          },
        ],
        continue: true,
      }),
    );
    expect(envelope.message).toBe("Drawing a rectangle.");
    expect(envelope.continue).toBe(true);
    expect(envelope.commands).toHaveLength(1);
    expect(envelope.commands[0].type).toBe("add_sketch_rectangle");
    expect(envelope.commands[0].id).toBeTruthy();
  });

  it("accepts a markdown-fenced JSON envelope", () => {
    const envelope = parseAiCommandEnvelope(
      '```json\n{"message":"ok","commands":[],"continue":false}\n```',
    );
    expect(envelope.message).toBe("ok");
    expect(envelope.commands).toHaveLength(0);
  });

  it("rejects non-JSON content", () => {
    expect(() => parseAiCommandEnvelope("sorry, here is the plan...")).toThrow(
      /not valid JSON/,
    );
  });

  it("rejects unknown top-level envelope keys", () => {
    expect(() =>
      parseAiCommandEnvelope(
        JSON.stringify({
          message: "ok",
          commands: [],
          continue: false,
          extra_field: true,
        }),
      ),
    ).toThrow();
  });

  it("rejects commands that include an id", () => {
    expect(() =>
      parseAiCommandEnvelope(
        JSON.stringify({
          message: "ok",
          commands: [
            {
              id: "model-made-up-id",
              type: "create_document",
              payload: {},
            },
          ],
          continue: false,
        }),
      ),
    ).toThrow(/ids|Unrecognized key/);
  });

  it("rejects unknown command types", () => {
    expect(() =>
      parseAiCommandEnvelope(
        JSON.stringify({
          message: "ok",
          commands: [{ type: "draw_a_cat", payload: {} }],
          continue: false,
        }),
      ),
    ).toThrow(/Unknown or unsupported AI command: draw_a_cat/);
  });

  it("rejects malformed payloads", () => {
    expect(() =>
      parseAiCommandEnvelope(
        JSON.stringify({
          message: "ok",
          commands: [
            {
              type: "add_sketch_rectangle",
              payload: { start_x: "zero", start_y: 0, end_x: 10, end_y: 10 },
            },
          ],
          continue: false,
        }),
      ),
    ).toThrow();
  });
});

describe("prepareAiCommandBatchForState", () => {
  it("inserts create_document and an XY sketch start when no document exists", () => {
    const batch = prepareAiCommandBatchForState(
      [rectangleCommand()],
      false,
      null,
      null,
    );
    expect(batch.commands.map((command) => command.type)).toEqual([
      "create_document",
      "start_sketch_on_plane",
      "add_sketch_rectangle",
    ]);
    expect(batch.notices).toHaveLength(1);
    expect(batch.notices[0]).toMatch(/sketch/);
  });

  it("inserts only the sketch start when a document exists without an active sketch", () => {
    const document = makeDocument();
    const batch = prepareAiCommandBatchForState(
      [rectangleCommand()],
      false,
      document,
      emptyViewport,
    );
    expect(batch.commands.map((command) => command.type)).toEqual([
      "start_sketch_on_plane",
      "add_sketch_rectangle",
    ]);
    expect(batch.notices).toHaveLength(1);
  });

  it("passes commands through when a sketch is already active", () => {
    const batch = prepareAiCommandBatchForState(
      [rectangleCommand()],
      false,
      documentWithActiveSketch,
      emptyViewport,
    );
    expect(batch.commands.map((command) => command.type)).toEqual([
      "add_sketch_rectangle",
    ]);
    expect(batch.notices).toEqual([]);
  });

  it("defers an extrude with an unknown profile id and sets continue", () => {
    const batch = prepareAiCommandBatchForState(
      [
        rectangleCommand(),
        {
          id: "test-id-2",
          type: "extrude_profile",
          payload: { profile_ids: ["profile-not-yet-known"], depth: 5 },
        },
      ],
      false,
      null,
      null,
    );
    expect(batch.commands.map((command) => command.type)).toEqual([
      "create_document",
      "start_sketch_on_plane",
      "add_sketch_rectangle",
    ]);
    expect(batch.continue).toBe(true);
    expect(batch.notices.some((notice) => /Deferred/.test(notice))).toBe(true);
  });

  it("rejects a batch that creates only construction geometry", () => {
    expect(() =>
      prepareAiCommandBatchForState(
        [
          {
            id: "test-id",
            type: "add_sketch_rectangle",
            payload: {
              start_x: 0,
              start_y: 0,
              end_x: 10,
              end_y: 10,
              is_construction: true,
            },
          },
        ],
        false,
        documentWithActiveSketch,
        emptyViewport,
      ),
    ).toThrow(/is_construction: false/);
  });

  it("accepts an extrude referencing a known profile id", () => {
    const batch = prepareAiCommandBatchForState(
      [
        {
          id: "test-id",
          type: "extrude_profile",
          payload: { profile_ids: ["profile-1"], depth: 5 },
        },
      ],
      false,
      documentWithActiveSketch,
      emptyViewport,
    );
    expect(batch.commands).toHaveLength(1);
    expect(batch.continue).toBe(false);
    expect(batch.notices).toEqual([]);
  });

  it("rejects extrude_face with new_body on a face of an existing body", () => {
    expect(() =>
      prepareAiCommandBatchForState(
        [
          {
            id: "test-id",
            type: "extrude_face",
            payload: {
              face_id: "feature-3:face:5",
              depth: 10,
              mode: "new_body",
            },
          },
        ],
        false,
        documentWithActiveSketch,
        viewportWithBody,
      ),
    ).toThrow(/second overlapping body.*mode "join"/);
  });

  it("accepts extrude_face with join and target_body_id on an owned face", () => {
    const batch = prepareAiCommandBatchForState(
      [
        {
          id: "test-id",
          type: "extrude_face",
          payload: {
            face_id: "feature-3:face:5",
            depth: 10,
            mode: "join",
            target_body_id: "feature-3",
          },
        },
      ],
      false,
      documentWithActiveSketch,
      viewportWithBody,
    );
    expect(batch.commands).toHaveLength(1);
    expect(batch.notices).toEqual([]);
  });

  it("accepts extrude_face without an explicit mode (core auto-joins)", () => {
    const batch = prepareAiCommandBatchForState(
      [
        {
          id: "test-id",
          type: "extrude_face",
          payload: { face_id: "feature-3:face:5", depth: 10 },
        },
      ],
      false,
      documentWithActiveSketch,
      viewportWithBody,
    );
    expect(batch.commands).toHaveLength(1);
    expect(batch.notices).toEqual([]);
  });
});

describe("formatAiCommandError", () => {
  it("maps zod issues to compact path: message lines", () => {
    const result = z
      .object({ depth: z.number() })
      .strict()
      .safeParse({ depth: "five" });
    expect(result.success).toBe(false);
    const formatted = formatAiCommandError(result.error);
    expect(formatted).toMatch(/depth:/);
    expect(formatted).not.toContain("ZodError");
  });

  it("passes plain errors through as their message", () => {
    expect(formatAiCommandError(new Error("boom"))).toBe("Error: boom");
  });
});
