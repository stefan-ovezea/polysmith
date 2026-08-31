// TS port of the C++ profile oracle (native/cad-core/tests/sketch_test_utils.h
// profiles_match) for the serialized document_state, plus small scenario
// assertions for the AI generation tests.
//
// Profile-detection tests must assert the COMPLETE expected region set, not
// just the presence of one profile — the C++ suite learned this the hard way
// (a face-walk change silently removed a full-circle profile and the suite
// stayed green). Keep the same discipline here.
import type { DocumentState, ViewportState } from "@/types";

export type ProfileKind = "polygon" | "circle" | "ellipse" | "spline";

export interface ExpectedProfile {
  // Either one kind or a set of acceptable kinds — circle regions serialize
  // as "polygon" (circle id in the boundary set + source_circle_id) in some
  // arrangements and as "circle" (no source_circle_id) in others.
  kind: ProfileKind | ProfileKind[];
  // Exact boundary entity-id set (line ids; circle regions carry their
  // circle id in the boundary set). Omit to only check boundary_count.
  entity_ids?: string[];
  // Exact number of boundary entity ids. Checked when entity_ids is omitted.
  boundary_count?: number;
  // When provided, asserts whether the region carries a source_circle_id.
  has_source_circle_id?: boolean;
}

interface SerializedProfile {
  profile_id: string;
  kind: "polygon" | "circle" | "ellipse" | "spline";
  line_ids: string[];
  source_circle_id: string | null;
}

function collectProfiles(document: DocumentState): SerializedProfile[] {
  const profiles: SerializedProfile[] = [];
  for (const feature of document.feature_history ?? []) {
    if (feature.kind !== "sketch" || !feature.sketch_parameters) {
      continue;
    }
    for (const profile of feature.sketch_parameters.profiles ?? []) {
      profiles.push({
        profile_id: profile.profile_id,
        kind: profile.kind,
        line_ids: profile.line_ids ?? [],
        source_circle_id: profile.source_circle_id ?? null,
      });
    }
  }
  return profiles;
}

function boundaryIds(profile: SerializedProfile): Set<string> {
  // Circle regions may carry their circle id inside line_ids (core layout)
  // or only via source_circle_id (serialization); the union covers both.
  return new Set([
    ...profile.line_ids,
    ...(profile.source_circle_id ? [profile.source_circle_id] : []),
  ]);
}

// Asserts the document's sketch profile regions match `expected` exactly:
// same total count, and every expected entry (kind + boundary entity set or
// count + source-circle flag) is present. Throws with the first discrepancy.
export function assertProfilesMatch(
  document: DocumentState,
  expected: ExpectedProfile[],
): void {
  const profiles = collectProfiles(document);
  if (profiles.length !== expected.length) {
    throw new Error(
      `expected ${expected.length} profiles, got ${profiles.length}`,
    );
  }

  const matched = new Array(expected.length).fill(false) as boolean[];
  for (const profile of profiles) {
    let found = false;
    for (let index = 0; index < expected.length; index++) {
      const acceptableKinds = Array.isArray(expected[index].kind)
        ? expected[index].kind
        : [expected[index].kind];
      if (matched[index] || !acceptableKinds.includes(profile.kind)) {
        continue;
      }
      const candidate = expected[index];
      if (candidate.entity_ids) {
        const ids = boundaryIds(profile);
        const want = new Set(candidate.entity_ids);
        if (ids.size !== want.size || ![...want].every((id) => ids.has(id))) {
          continue;
        }
      } else if (candidate.boundary_count !== undefined) {
        if (boundaryIds(profile).size !== candidate.boundary_count) {
          continue;
        }
      }
      if (
        candidate.has_source_circle_id !== undefined &&
        Boolean(profile.source_circle_id) !== candidate.has_source_circle_id
      ) {
        continue;
      }
      matched[index] = true;
      found = true;
      break;
    }
    if (!found) {
      throw new Error(
        `profile kind=${profile.kind} ids=[${[...boundaryIds(profile)].join(" ")}] ` +
          `src=${profile.source_circle_id ?? "none"} matches no expected profile`,
      );
    }
  }
}

export function featureKinds(document: DocumentState): string[] {
  return (document.feature_history ?? []).map((feature) => feature.kind);
}

// Reads the depth of the first extrude feature with the given kind filter
// (defaults to any extrude). Cut extrudes are extrude features too.
export function extrudeFeatures(
  document: DocumentState,
  kindFilter?: "extrude",
): Array<NonNullable<DocumentState["feature_history"][number]["extrude_parameters"]>> {
  return (document.feature_history ?? [])
    .filter(
      (feature) =>
        (!kindFilter || feature.kind === kindFilter) && feature.extrude_parameters,
    )
    .map((feature) => feature.extrude_parameters as NonNullable<typeof feature.extrude_parameters>);
}

export function expectExtrudeDepth(
  document: DocumentState,
  depth: number,
  tolerance = 0.01,
): void {
  const extrudes = extrudeFeatures(document);
  if (extrudes.length === 0) {
    throw new Error("expected at least one extrude feature, got none");
  }
  const depths = extrudes.map((parameters) => parameters.depth);
  if (!depths.some((value) => Math.abs(value - depth) <= tolerance)) {
    throw new Error(
      `expected an extrude with depth ${depth}, got depths [${depths.join(", ")}]`,
    );
  }
}

export function expectBodyCount(viewport: ViewportState, count: number): void {
  const bodies = viewport?.bodies ?? [];
  if (bodies.length !== count) {
    throw new Error(`expected ${count} bodies, got ${bodies.length}`);
  }
}

export function expectRevisionAtLeast(document: DocumentState, revision: number): void {
  if ((document.revision ?? 0) < revision) {
    throw new Error(
      `expected document revision >= ${revision}, got ${document.revision}`,
    );
  }
}
