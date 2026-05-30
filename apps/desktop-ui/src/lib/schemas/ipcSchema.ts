import { z } from "zod";

const planeFrameSchema = z.object({
  origin: z.object({ x: z.number(), y: z.number(), z: z.number() }),
  x_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
  y_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
  normal: z.object({ x: z.number(), y: z.number(), z: z.number() }),
});

const documentStateSchema = z.object({
  document_id: z.string(),
  name: z.string(),
  units: z.string(),
  revision: z.number(),
  selected_feature_id: z.string().nullable(),
  selected_reference_id: z.string().nullable(),
  selected_face_id: z.string().nullable(),
  // Multi-edge selection (Phase C). Older `.polysmith` saves used a
  // single `selected_edge_id`; the C++ loader migrates them to the
  // new array shape, so by the time the schema runs we always see an
  // array. Default `[]` keeps the schema lenient for tests that
  // hand-craft document payloads without selection state.
  selected_edge_ids: z.array(z.string()).default([]),
  // Multi-vertex selection: same shape and rationale as
  // `selected_edge_ids`. The C++ loader migrates legacy single-id
  // saves to the array form, so by the time we run we always see an
  // array. Default `[]` keeps the schema lenient.
  selected_vertex_ids: z.array(z.string()).default([]),
  active_sketch_plane_id: z.string().nullable(),
  active_sketch_face_id: z.string().nullable(),
  active_sketch_feature_id: z.string().nullable(),
  active_sketch_tool: z.string().nullable(),
  selected_sketch_point_id: z.string().nullable(),
  selected_sketch_entity_id: z.string().nullable(),
  selected_sketch_point_ids: z.array(z.string()).default([]),
  selected_sketch_entity_ids: z.array(z.string()).default([]),
  selected_sketch_dimension_id: z.string().nullable(),
  selected_sketch_profile_id: z.string().nullable(),
  selected_sketch_profile_ids: z.array(z.string()).default([]),
  timeline_cursor: z.number().int().nullable().default(null),
  feature_history: z.array(
    z.object({
      feature_id: z.string(),
      kind: z.string(),
      name: z.string(),
      status: z.string(),
      suppressed: z.boolean().default(false),
      // Set by the core when this feature references upstream
      // geometry that no longer exists (e.g. its sketch plane was a
      // face that got fillet'd away). The timeline shows a yellow
      // warning button when true; the message is the tooltip.
      dependency_broken: z.boolean().default(false),
      dependency_warning: z.string().default(""),
      parameters_summary: z.string(),
      box_parameters: z
        .object({
          width: z.number(),
          height: z.number(),
          depth: z.number(),
        })
        .nullable(),
      cylinder_parameters: z
        .object({
          radius: z.number(),
          height: z.number(),
        })
        .nullable(),
      extrude_parameters: z
        .object({
          sketch_feature_id: z.string(),
          profile_id: z.string(),
          profile_ids: z.array(z.string()).default([]),
          open_entity_ids: z.array(z.string()).default([]),
          plane_id: z.string(),
          plane_frame: z
            .object({
              origin: z.object({ x: z.number(), y: z.number(), z: z.number() }),
              x_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
              y_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
              normal: z.object({ x: z.number(), y: z.number(), z: z.number() }),
            })
            .nullable(),
          profile_kind: z.enum(["rectangle", "circle", "polygon", "open_chain"]),
          start_x: z.number(),
          start_y: z.number(),
          width: z.number(),
          height: z.number(),
          radius: z.number(),
          profile_points: z.array(
            z.object({
              x: z.number(),
              y: z.number(),
            }),
          ),
          inner_loops: z
            .array(z.array(z.object({ x: z.number(), y: z.number() })))
            .default([]),
          additional_profile_points: z
            .array(z.array(z.object({ x: z.number(), y: z.number() })))
            .default([]),
          additional_inner_loops: z
            .array(
              z.array(z.array(z.object({ x: z.number(), y: z.number() }))),
            )
            .default([]),
          depth: z.number(),
          extent_mode: z
            .enum(["one_side", "symmetric", "two_sides"])
            .default("one_side"),
          side1: z
            .object({
              extent_type: z
                .enum(["distance", "through_all", "to_object", "to_next"])
                .default("distance"),
              distance: z.number().default(10),
              start_offset: z.number().default(0),
              taper_angle_degrees: z.number().default(0),
              target_reference_id: z.string().nullable().default(null),
            })
            .default({
              extent_type: "distance",
              distance: 10,
              start_offset: 0,
              taper_angle_degrees: 0,
              target_reference_id: null,
            }),
          side2: z
            .object({
              extent_type: z
                .enum(["distance", "through_all", "to_object", "to_next"])
                .default("distance"),
              distance: z.number().default(10),
              start_offset: z.number().default(0),
              taper_angle_degrees: z.number().default(0),
              target_reference_id: z.string().nullable().default(null),
            })
            .nullable()
            .default(null),
          thin: z
            .object({
              enabled: z.boolean().default(false),
              thickness: z.number().default(1),
              placement: z.enum(["center", "inside", "outside"]).default("center"),
            })
            .default({ enabled: false, thickness: 1, placement: "center" }),
          mode: z
            .enum(["new_body", "join", "cut", "intersect"])
            .default("new_body"),
          operation: z
            .enum(["auto", "new_body", "join", "cut", "intersect"])
            .default("new_body"),
          intersect_result: z
            .enum(["replace_target", "new_body"])
            .default("replace_target"),
          target_body_id: z.string().nullable().default(null),
        })
        .nullable(),
      loft_parameters: z
        .object({
          ruled: z.boolean().default(false),
          sections: z.array(
            z.object({
              sketch_feature_id: z.string(),
              profile_id: z.string(),
              plane_id: z.string(),
              plane_frame: planeFrameSchema.nullable(),
              profile_points: z.array(
                z.object({
                  x: z.number(),
                  y: z.number(),
                }),
              ),
            }),
          ),
        })
        .nullable()
        .default(null),
      revolve_parameters: z
        .object({
          sketch_feature_id: z.string(),
          profile_id: z.string(),
          plane_id: z.string(),
          plane_frame: planeFrameSchema.nullable(),
          profile_kind: z.string(),
          profile_points: z.array(z.object({ x: z.number(), y: z.number() })),
          inner_loops: z
            .array(z.array(z.object({ x: z.number(), y: z.number() })))
            .default([]),
          axis_sketch_feature_id: z.string(),
          axis_entity_id: z.string(),
          axis_start_x: z.number(),
          axis_start_y: z.number(),
          axis_start_z: z.number(),
          axis_end_x: z.number(),
          axis_end_y: z.number(),
          axis_end_z: z.number(),
          angle_degrees: z.number(),
        })
        .nullable()
        .default(null),
      sweep_parameters: z
        .object({
          sketch_feature_id: z.string(),
          profile_id: z.string(),
          plane_id: z.string(),
          plane_frame: planeFrameSchema.nullable(),
          profile_kind: z.string(),
          profile_points: z.array(z.object({ x: z.number(), y: z.number() })),
          inner_loops: z
            .array(z.array(z.object({ x: z.number(), y: z.number() })))
            .default([]),
          path_sketch_feature_id: z.string(),
          path_entity_id: z.string(),
          path_start_x: z.number(),
          path_start_y: z.number(),
          path_start_z: z.number(),
          path_end_x: z.number(),
          path_end_y: z.number(),
          path_end_z: z.number(),
          path_segments: z
            .array(
              z.object({
                entity_id: z.string(),
                kind: z.enum(["line", "arc"]),
                start_x: z.number(),
                start_y: z.number(),
                start_z: z.number(),
                end_x: z.number(),
                end_y: z.number(),
                end_z: z.number(),
                center_x: z.number(),
                center_y: z.number(),
                center_z: z.number(),
                mid_x: z.number(),
                mid_y: z.number(),
                mid_z: z.number(),
                radius: z.number(),
                ccw: z.boolean(),
              }),
            )
            .default([]),
        })
        .nullable()
        .default(null),
      fillet_parameters: z
        .object({
          target_body_id: z.string(),
          edge_ids: z.array(z.string()),
          radius: z.number(),
          is_pending: z.boolean().default(false),
        })
        .nullable()
        .default(null),
      chamfer_parameters: z
        .object({
          target_body_id: z.string(),
          edge_ids: z.array(z.string()),
          distance: z.number(),
          is_pending: z.boolean().default(false),
        })
        .nullable()
        .default(null),
      shell_parameters: z
        .object({
          target_body_id: z.string(),
          removed_face_ids: z.array(z.string()),
          thickness: z.number(),
          is_pending: z.boolean().default(false),
        })
        .nullable()
        .default(null),
      // Parametric offset construction plane parameters. Defaulted
      // to null so older `.polysmith` saves (and messages from a
      // pre-construction-plane core) round-trip cleanly.
      construction_plane_parameters: z
        .object({
          plane_type: z
            .enum(["offset", "midplane", "tangent", "angle"])
            .default("offset"),
          source_plane_id: z.string(),
          source_plane_ids: z.array(z.string()).default([]),
          source_axis_id: z.string().default(""),
          offset: z.number(),
          angle_degrees: z.number().default(0),
          plane_frame: z.object({
            origin: z.object({ x: z.number(), y: z.number(), z: z.number() }),
            x_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
            y_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
            normal: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          }),
        })
        .nullable()
        .default(null),
      construction_axis_parameters: z
        .object({
          source_id: z.string(),
          source_kind: z.enum(["edge", "sketch_line", "construction_axis"]),
          start: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          end: z.object({ x: z.number(), y: z.number(), z: z.number() }),
        })
        .nullable()
        .default(null),
      construction_point_parameters: z
        .object({
          source_id: z.string(),
          source_kind: z.enum(["vertex", "sketch_point"]),
          position: z.object({ x: z.number(), y: z.number(), z: z.number() }),
        })
        .nullable()
        .default(null),
      hole_parameters: z
        .object({
          target_body_id: z.string(),
          source_face_id: z.string(),
          plane_frame: planeFrameSchema,
          center_x: z.number(),
          center_y: z.number(),
          hole_type: z
            .enum(["simple", "counterbore", "countersink", "spotface"])
            .default("simple"),
          extent_type: z.enum(["blind", "through_all"]).default("blind"),
          diameter: z.number(),
          depth: z.number(),
          counterbore_diameter: z.number(),
          counterbore_depth: z.number(),
          countersink_diameter: z.number(),
          countersink_angle_degrees: z.number(),
          standard: z.enum(["custom", "metric", "imperial"]).default("custom"),
          standard_size: z.string().default(""),
          hole_fit: z
            .enum(["clearance", "tap_drill", "threaded"])
            .default("clearance"),
          thread_enabled: z.boolean().default(false),
          thread_spec: z.string().default(""),
          thread_pitch: z.number().default(0),
          major_diameter: z.number().default(0),
          minor_diameter: z.number().default(0),
          thread_depth: z.number(),
          thread_representation: z.enum(["cosmetic", "modeled"]).default("cosmetic"),
          is_pending: z.boolean().default(false),
        })
        .nullable()
        .default(null),
      helix_parameters: z
        .object({
          axis_source_id: z.string(),
          axis_start: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          axis_end: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          radius: z.number(),
          pitch: z.number(),
          height: z.number(),
          turns: z.number(),
          handedness: z.enum(["left", "right"]).default("right"),
          start_angle_degrees: z.number(),
          points: z.array(z.number()).default([]),
        })
        .nullable()
        .default(null),
      thread_parameters: z
        .object({
          target_body_id: z.string(),
          axis_source_id: z.string(),
          mode: z.enum(["external", "internal"]).default("external"),
          standard: z.enum(["custom", "metric", "imperial"]).default("custom"),
          size: z.string().default(""),
          major_diameter: z.number(),
          minor_diameter: z.number(),
          pitch: z.number(),
          length: z.number(),
          thread_angle_degrees: z.number(),
          start_offset: z.number(),
          handedness: z.enum(["left", "right"]).default("right"),
          representation: z.enum(["cosmetic", "modeled"]).default("cosmetic"),
          is_pending: z.boolean().default(false),
        })
        .nullable()
        .default(null),
      fastener_parameters: z
        .object({
          standard: z.enum(["metric", "imperial", "custom"]).default("metric"),
          size: z.string(),
          diameter: z.number(),
          minor_diameter: z.number().default(4.2),
          pitch: z.number().default(0.8),
          length: z.number(),
          thread_length: z.number(),
          head_type: z
            .enum(["socket_head", "button_head", "flat", "hex_bolt"])
            .default("socket_head"),
          drive_type: z.enum(["none", "hex_socket", "phillips"]).default("hex_socket"),
          thread_representation: z.enum(["cosmetic", "modeled"]).default("cosmetic"),
        })
        .nullable()
        .default(null),
      move_parameters: z
        .object({
          target_body_id: z.string(),
          translation_x: z.number().default(0),
          translation_y: z.number().default(0),
          translation_z: z.number().default(0),
          rotation_x_degrees: z.number().default(0),
          rotation_y_degrees: z.number().default(0),
          rotation_z_degrees: z.number().default(0),
          is_pending: z.boolean().default(false),
        })
        .nullable()
        .default(null),
      body_copy_parameters: z
        .object({
          source_body_id: z.string(),
          copy_mode: z.enum(["linked", "standalone"]).default("linked"),
          source_body_name: z.string().default(""),
          serialized_shape: z.string().default(""),
          local_x_axis_x: z.number().default(1),
          local_x_axis_y: z.number().default(0),
          local_x_axis_z: z.number().default(0),
          local_y_axis_x: z.number().default(0),
          local_y_axis_y: z.number().default(1),
          local_y_axis_z: z.number().default(0),
          local_z_axis_x: z.number().default(0),
          local_z_axis_y: z.number().default(0),
          local_z_axis_z: z.number().default(1),
        })
        .nullable()
        .default(null),
      sketch_parameters: z
        .object({
          plane_id: z.string(),
          plane_frame: z
            .object({
              origin: z.object({ x: z.number(), y: z.number(), z: z.number() }),
              x_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
              y_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
              normal: z.object({ x: z.number(), y: z.number(), z: z.number() }),
            })
            .nullable(),
          active_tool: z.string(),
          lines: z.array(
            z.object({
              line_id: z.string(),
              start_point_id: z.string(),
              end_point_id: z.string(),
              start_x: z.number(),
              start_y: z.number(),
              end_x: z.number(),
              end_y: z.number(),
              constraint: z.enum(["horizontal", "vertical"]).nullable(),
              // Reference-only construction lines render dashed and
              // are excluded from profile detection. Optional /
              // defaulted for back-compat with older saves.
              is_construction: z.boolean().default(false),
            }),
          ),
          // Midpoint anchors bind a sketch point (typically an
          // endpoint of some other line) to the midpoint of a host
          // line. The solver re-pulls the point on every edit so the
          // relation persists. Defaulted to empty for older saves.
          midpoint_anchors: z
            .array(
              z.object({
                anchor_id: z.string(),
                point_id: z.string(),
                line_id: z.string(),
              }),
            )
            .default([]),
          // Point-line anchors are the parametric generalization of
          // midpoint anchors: the bound point sits at fraction `t`
          // along the host line and rides with edits. Used by the
          // sub-segment midpoint snap and the line-body snap.
          // Defaulted to empty for older saves.
          point_line_anchors: z
            .array(
              z.object({
                anchor_id: z.string(),
                point_id: z.string(),
                line_id: z.string(),
                t: z.number(),
              }),
            )
            .default([]),
          // Standalone projected points placed by the Project tool.
          // Defaulted to [] so older saves / pre-Project cores keep
          // parsing cleanly.
          projected_points: z
            .array(
              z.object({
                point_id: z.string(),
                source_id: z.string(),
                x: z.number(),
                y: z.number(),
              }),
            )
            .default([]),
          // Body face / edge ids that have been projected onto this
          // sketch. Used for idempotency. Defaulted to [].
          projected_sources: z.array(z.string()).default([]),
          // Live-link records — see SketchProjectionEntry. Defaulted
          // to [] so older saves / pre-live-link cores parse cleanly.
          projections: z
            .array(
              z.object({
                projection_id: z.string(),
                source_id: z.string(),
                source_kind: z.enum(["face", "edge", "vertex", "profile"]),
                generated_line_ids: z.array(z.string()).default([]),
                generated_circle_ids: z.array(z.string()).default([]),
                generated_arc_ids: z.array(z.string()).default([]),
                generated_point_id: z.string().default(""),
                dependency_broken: z.boolean().default(false),
                dependency_warning: z.string().default(""),
              }),
            )
            .default([]),
          circles: z.array(
            z.object({
              circle_id: z.string(),
              center_x: z.number(),
              center_y: z.number(),
              radius: z.number(),
              is_construction: z.boolean().default(false),
            }),
          ),
          // Sketch arcs in the document state. Defaulted to `[]`
          // so older saves (or messages from a core that pre-dates
          // arc support) don't fail validation.
          arcs: z
            .array(
              z.object({
                arc_id: z.string(),
                start_point_id: z.string(),
                end_point_id: z.string(),
                center_x: z.number(),
                center_y: z.number(),
                radius: z.number(),
                start_x: z.number(),
                start_y: z.number(),
                end_x: z.number(),
                end_y: z.number(),
                ccw: z.boolean(),
                is_construction: z.boolean().default(false),
              }),
            )
            .default([]),
          // Parametric corner fillets. Defaulted to `[]` so older
          // saves (or messages from a core that pre-dates fillet
          // support) keep parsing cleanly.
          fillets: z
            .array(
              z.object({
                fillet_id: z.string(),
                corner_point_id: z.string(),
                corner_x: z.number(),
                corner_y: z.number(),
                line_a_id: z.string(),
                line_b_id: z.string(),
                trim_a_point_id: z.string(),
                trim_b_point_id: z.string(),
                arc_id: z.string(),
                radius: z.number(),
              }),
            )
            .default([]),
          points: z.array(
            z.object({
              point_id: z.string(),
              kind: z.enum(["endpoint", "center", "projected", "quadrant"]),
              x: z.number(),
              y: z.number(),
              is_fixed: z.boolean(),
            }),
          ),
          dimensions: z.array(
            z.object({
              dimension_id: z.string(),
              kind: z.enum([
                "line_length",
                "circle_radius",
                "polygon_radius",
                "angle",
                "line_angle",
                "line_line_distance",
                "circle_center_distance",
                "circle_line_distance",
                "point_distance",
              ]),
              entity_id: z.string(),
              // Empty string for unary dims; second line id for angle.
              secondary_entity_id: z.string().default(""),
              value: z.number(),
              expression: z.string().default(""),
              driven: z.boolean().default(false),
              display_as: z.string().default(""),
              label_x: z.number().nullable().optional(),
              label_y: z.number().nullable().optional(),
            }),
          ),
          line_relations: z.array(
            z.object({
              relation_id: z.string(),
              kind: z.enum([
                "equal_length",
                "perpendicular",
                "parallel",
                "tangent_line_circle",
              ]),
              first_line_id: z.string(),
              second_line_id: z.string(),
            }),
          ),
          profiles: z.array(
            z.object({
              profile_id: z.string(),
              kind: z.enum(["polygon", "circle"]),
              point_ids: z.array(z.string()),
              line_ids: z.array(z.string()),
              points: z.array(
                z.object({
                  x: z.number(),
                  y: z.number(),
                }),
              ),
              inner_loops: z
                .array(z.array(z.object({ x: z.number(), y: z.number() })))
                .default([]),
              source_circle_id: z.string().nullable(),
              center_x: z.number(),
              center_y: z.number(),
              radius: z.number(),
            }),
          ),
          // Optional pending mirror tool state. Present only while
          // the user has the mirror tool open. The UI uses the
          // presence of this object to mount the floating panel.
          pending_mirror: z
            .object({
              axis_line_id: z.string().nullable(),
              object_ids: z.array(z.string()),
              generated_lines: z.array(
                z.object({
                  line_id: z.string(),
                  start_point_id: z.string(),
                  end_point_id: z.string(),
                  start_x: z.number(),
                  start_y: z.number(),
                  end_x: z.number(),
                  end_y: z.number(),
                  is_construction: z.boolean(),
                }),
              ),
              generated_circles: z.array(
                z.object({
                  circle_id: z.string(),
                  center_x: z.number(),
                  center_y: z.number(),
                  radius: z.number(),
                  is_construction: z.boolean().default(false),
                }),
              ),
            })
            .nullable()
            .default(null),
        })
        .nullable(),
    }),
  ),
  appearance: z
    .object({
      body_colors: z
        .array(
          z.object({
            body_id: z.string(),
            color: z.string(),
          }),
        )
        .default([]),
      face_colors: z
        .array(
          z.object({
            face_id: z.string(),
            owner_body_id: z.string(),
            signature: z.string(),
            color: z.string(),
          }),
        )
        .default([]),
    })
    .default({ body_colors: [], face_colors: [] }),
  parameters: z.array(
    z.object({
      name: z.string(),
      kind: z.string().default("length"),
      expression: z.string(),
      resolved_value: z.number(),
      has_error: z.boolean(),
      error_message: z.string().default(""),
    }),
  ).default([]),
});

const sessionStateSchema = z.object({
  document_count: z.number(),
  has_active_document: z.boolean(),
  active_document_id: z.string().nullable(),
  can_undo: z.boolean(),
  can_redo: z.boolean(),
});

const viewportStateSchema = z.object({
  has_active_document: z.boolean(),
  boxes: z.array(
    z.object({
      primitive_id: z.string(),
      label: z.string(),
      width: z.number(),
      height: z.number(),
      depth: z.number(),
      x_offset: z.number(),
      center: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      is_selected: z.boolean(),
      appearance_color: z.string().nullable().default(null),
    }),
  ),
  cylinders: z.array(
    z.object({
      primitive_id: z.string(),
      label: z.string(),
      radius: z.number(),
      height: z.number(),
      x_offset: z.number(),
      center: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      is_selected: z.boolean(),
      appearance_color: z.string().nullable().default(null),
    }),
  ),
  polygon_extrudes: z.array(
    z.object({
      primitive_id: z.string(),
      label: z.string(),
      plane_id: z.string(),
      plane_frame: z
        .object({
          origin: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          x_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          y_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          normal: z.object({ x: z.number(), y: z.number(), z: z.number() }),
        })
        .nullable(),
      profile_points: z.array(
        z.object({
          x: z.number(),
          y: z.number(),
        }),
      ),
      inner_loops: z
        .array(z.array(z.object({ x: z.number(), y: z.number() })))
        .default([]),
      depth: z.number(),
      is_selected: z.boolean(),
      appearance_color: z.string().nullable().default(null),
    }),
  ),
  solid_faces: z.array(
    z.object({
      face_id: z.string(),
      owner_id: z.string(),
      owner_kind: z.string(),
      label: z.string(),
      sketchability: z.string(),
      center: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      normal: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      plane_frame: z.object({
        origin: z.object({ x: z.number(), y: z.number(), z: z.number() }),
        x_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
        y_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
        normal: z.object({ x: z.number(), y: z.number(), z: z.number() }),
      }),
      size: z.object({
        width: z.number(),
        height: z.number(),
        radius: z.number(),
      }),
      // Body-derived faces carry per-face triangulation; legacy
      // analytical faces leave these empty and rely on (size, plane_frame)
      // for the UI's pick mesh.
      triangle_positions: z.array(z.number()).default([]),
      triangle_indices: z.array(z.number()).default([]),
      is_selected: z.boolean(),
      appearance_color: z.string().nullable().default(null),
    }),
  ),
  reference_planes: z.array(
    z.object({
      reference_id: z.string(),
      label: z.string(),
      // Origin planes use one of "xy" / "yz" / "xz"; construction
      // planes use "custom" and ship a real `plane_frame`.
      orientation: z.enum(["xy", "yz", "xz", "custom"]),
      center: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      size: z.object({
        width: z.number(),
        height: z.number(),
      }),
      is_selected: z.boolean(),
      is_active_sketch_plane: z.boolean(),
      // Defaulted to null so older snapshots without the field
      // (origin-only planes from a pre-construction-plane core)
      // still validate.
      plane_frame: z
        .object({
          origin: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          x_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          y_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          normal: z.object({ x: z.number(), y: z.number(), z: z.number() }),
        })
        .nullable()
        .default(null),
    }),
  ),
  reference_axes: z.array(
    z.object({
      reference_id: z.string(),
      label: z.string(),
      axis: z.enum(["x", "y", "z", "custom"]),
      start: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      end: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
    }),
  ),
  reference_points: z
    .array(
      z.object({
        reference_id: z.string(),
        label: z.string(),
        position: z.object({
          x: z.number(),
          y: z.number(),
          z: z.number(),
        }),
        is_selected: z.boolean(),
      }),
    )
    .default([]),
  helices: z
    .array(
      z.object({
        helix_id: z.string(),
        label: z.string(),
        points: z.array(z.number()).default([]),
        is_selected: z.boolean(),
      }),
    )
    .default([]),
  sketch_lines: z.array(
    z.object({
      line_id: z.string(),
      start_point_id: z.string(),
      end_point_id: z.string(),
      is_construction: z.boolean().default(false),
      plane_id: z.string(),
      start: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      end: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      is_selected: z.boolean(),
      constraint: z.enum(["horizontal", "vertical"]).nullable(),
      // True for transient lines generated by the in-progress
      // Mirror tool. Rendered as a dashed translucent preview;
      // not selectable. Defaulted for back-compat with the few
      // call sites that may not yet emit it.
      is_preview: z.boolean().default(false),
    }),
  ),
  sketch_circles: z.array(
    z.object({
      circle_id: z.string(),
      plane_id: z.string(),
      plane_frame: planeFrameSchema.nullable().default(null),
      center: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      radius: z.number(),
      is_selected: z.boolean(),
      is_construction: z.boolean().default(false),
      // See `sketch_lines.is_preview`.
      is_preview: z.boolean().default(false),
    }),
  ),
  // Sketch polygons — regular N-sided polygons on the sketch plane.
  // Defaulted to `[]` so clients running against an older core
  // don't crash.
  sketch_polygons: z
    .array(
      z.object({
        polygon_id: z.string(),
        plane_id: z.string(),
        plane_frame: planeFrameSchema.nullable().default(null),
        corner_x: z.array(z.number()),
        corner_y: z.array(z.number()),
        corner_z: z.array(z.number()),
        sides: z.number(),
        mode: z.string(),
        center: z.object({
          x: z.number(),
          y: z.number(),
          z: z.number(),
        }),
        radius: z.number(),
        is_selected: z.boolean(),
        is_construction: z.boolean().default(false),
        is_preview: z.boolean().default(false),
      }),
    )
    .default([]),
  // Sketch arcs share the same visual treatment as sketch circles
  // (selectable, preview-aware) but carry both endpoints + center
  // separately because the renderer samples between the two
  // endpoints around the center along the stored sweep direction.
  // Defaulted to `[]` so clients running against an older core
  // (no `sketch_arcs` key in the payload) don't crash on the
  // downstream `.filter()` call in `viewportScene.ts`.
  sketch_arcs: z
    .array(
      z.object({
        arc_id: z.string(),
        start_point_id: z.string(),
        end_point_id: z.string(),
        plane_id: z.string(),
        plane_frame: planeFrameSchema.nullable().default(null),
        center: z.object({
          x: z.number(),
          y: z.number(),
          z: z.number(),
        }),
        radius: z.number(),
        start: z.object({
          x: z.number(),
          y: z.number(),
          z: z.number(),
        }),
        end: z.object({
          x: z.number(),
          y: z.number(),
          z: z.number(),
        }),
        ccw: z.boolean(),
        is_selected: z.boolean(),
        is_construction: z.boolean().default(false),
        is_preview: z.boolean().default(false),
      }),
    )
    .default([]),
  sketch_points: z.array(
    z.object({
      point_id: z.string(),
      plane_id: z.string(),
      kind: z.enum(["endpoint", "center", "projected", "quadrant"]),
      position: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      is_fixed: z.boolean(),
      is_selected: z.boolean(),
    }),
  ),
  sketch_dimensions: z.array(
    z.object({
      dimension_id: z.string(),
      plane_id: z.string(),
      kind: z.enum([
        "line_length",
        "circle_radius",
        "polygon_radius",
        "angle",
        "line_angle",
        "line_line_distance",
        "circle_center_distance",
        "circle_line_distance",
        "point_distance",
      ]),
      entity_id: z.string(),
      label: z.string(),
      is_selected: z.boolean(),
      anchor_start: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      anchor_end: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      dimension_start: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      dimension_end: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      label_position: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      // Angle arc geometry (from C++ core, optional)
      arc_center: z
        .object({ x: z.number(), y: z.number(), z: z.number() })
        .optional(),
      arc_radius: z.number().optional(),
      arc_start_angle: z.number().optional(),
      arc_end_angle: z.number().optional(),
      arc_ccw: z.boolean().optional(),
      // Reference line (from C++ core, optional)
      ref_line_start: z
        .object({ x: z.number(), y: z.number(), z: z.number() })
        .optional(),
      ref_line_end: z
        .object({ x: z.number(), y: z.number(), z: z.number() })
        .optional(),
    }),
  ),
  sketch_constraints: z.array(
    z.object({
      constraint_id: z.string(),
      plane_id: z.string(),
      kind: z.enum([
        "horizontal",
        "vertical",
        "equal_length",
        "perpendicular",
        "parallel",
        "fixed",
        "midpoint",
        "on_line",
        "tangent_line_circle",
      ]),
      entity_id: z.string(),
      related_entity_id: z.string().nullable(),
      label: z.string(),
      is_selected: z.boolean(),
      position: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
    }),
  ),
  sketch_profiles: z.array(
    z.object({
      profile_id: z.string(),
      plane_id: z.string(),
      plane_frame: z
        .object({
          origin: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          x_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          y_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          normal: z.object({ x: z.number(), y: z.number(), z: z.number() }),
        })
        .nullable(),
      profile_kind: z.enum(["polygon", "circle"]),
      profile_points: z.array(
        z.object({
          x: z.number(),
          y: z.number(),
        }),
      ),
      start_x: z.number(),
      start_y: z.number(),
      width: z.number(),
      height: z.number(),
      radius: z.number(),
      is_selected: z.boolean(),
    }),
  ),
  meshes: z
    .array(
      z.object({
        primitive_id: z.string(),
        positions: z.array(z.number()),
        normals: z.array(z.number()),
        indices: z.array(z.number()),
        is_selected: z.boolean(),
        appearance_color: z.string().nullable().default(null),
      }),
    )
    .default([]),
  cut_previews: z
    .array(
      z.object({
        id: z.string(),
        positions: z.array(z.number()),
        normals: z.array(z.number()),
        indices: z.array(z.number()),
      }),
    )
    .default([]),
  bodies: z
    .array(
      z.object({
        id: z.string(),
        label: z.string(),
        center: z
          .object({ x: z.number(), y: z.number(), z: z.number() })
          .default({ x: 0, y: 0, z: 0 }),
        size: z
          .object({ x: z.number(), y: z.number(), z: z.number() })
          .default({ x: 0, y: 0, z: 0 }),
        local_frame: z
          .object({
            x_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
            y_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
            z_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          })
          .default({
            x_axis: { x: 1, y: 0, z: 0 },
            y_axis: { x: 0, y: 1, z: 0 },
            z_axis: { x: 0, y: 0, z: 1 },
          }),
      }),
    )
    .default([]),
  edges: z
    .array(
      z.object({
        id: z.string(),
        owner_body_id: z.string(),
        kind: z.string(),
        points: z.array(z.number()),
        // Default 0 so older snapshots without the field still validate;
        // new core builds always populate it.
        length: z.number().default(0),
        is_selected: z.boolean(),
      }),
    )
    .default([]),
  vertices: z
    .array(
      z.object({
        id: z.string(),
        owner_body_id: z.string(),
        position: z.object({ x: z.number(), y: z.number(), z: z.number() }),
        is_selected: z.boolean(),
      }),
    )
    .default([]),
  scene_width: z.number(),
  scene_height: z.number(),
  scene_depth: z.number(),
  scene_bounds: z.object({
    center: z.object({
      x: z.number(),
      y: z.number(),
      z: z.number(),
    }),
    size: z.object({
      x: z.number(),
      y: z.number(),
      z: z.number(),
    }),
    max_dimension: z.number(),
  }),
  dof_statuses: z.array(
    z.object({
      entity_id: z.string(),
      entity_kind: z.string(),
      total_dof: z.number(),
      consumed_dof: z.number(),
      status: z.enum(["under", "full", "over"]),
    }),
  ),
  snap_candidates: z.array(
    z.object({
      kind: z.string(),
      entity_id: z.string(),
      point_id: z.string(),
      local_x: z.number(),
      local_y: z.number(),
      label: z.string(),
    }),
  ),
  selection_filter: z.object({
    select_curves: z.boolean(),
    select_points: z.boolean(),
    select_construction: z.boolean(),
    select_constraints: z.boolean(),
    snap_endpoint: z.boolean(),
    snap_midpoint: z.boolean(),
    snap_center: z.boolean(),
    snap_intersection: z.boolean(),
    snap_nearest: z.boolean(),
    snap_quadrant: z.boolean(),
    snap_perpendicular: z.boolean(),
    snap_parallel: z.boolean(),
    snap_tangent: z.boolean(),
    snap_grid: z.boolean(),
    snap_grid_line: z.boolean(),
    snap_polar: z.boolean(),
    polar_angle_degrees: z.number(),
    magnetic_pull: z.boolean(),
    tolerance_px: z.number(),
  }),
});

const helloEventSchema = z.object({
  type: z.literal("hello"),
  payload: z.object({
    service: z.string(),
    version: z.string(),
  }),
});

const pongEventSchema = z.object({
  id: z.string(),
  type: z.literal("pong"),
  payload: z.object({
    version: z.string(),
  }),
});

const documentCreatedEventSchema = z.object({
  id: z.string(),
  type: z.literal("document_created"),
  payload: documentStateSchema,
});

const documentStateEventSchema = z.object({
  id: z.string(),
  type: z.literal("document_state"),
  payload: documentStateSchema,
});

const sessionStateEventSchema = z.object({
  id: z.string(),
  type: z.literal("session_state"),
  payload: sessionStateSchema,
});

const viewportStateEventSchema = z.object({
  id: z.string(),
  type: z.literal("viewport_state"),
  payload: viewportStateSchema,
});

const documentExportedEventSchema = z.object({
  id: z.string(),
  type: z.literal("document_exported"),
  payload: z.object({
    file_path: z.string(),
    format: z.enum(["step", "stl"]),
    exported_feature_count: z.number(),
  }),
});

const documentSavedEventSchema = z.object({
  id: z.string(),
  type: z.literal("document_saved"),
  payload: z.object({
    file_path: z.string(),
  }),
});

const logEventSchema = z.object({
  type: z.literal("log"),
  payload: z.object({
    level: z.enum(["debug", "info", "warn", "error"]),
    source: z.string(),
    message: z.string(),
    timestamp: z.string(),
  }),
});

const errorEventSchema = z.object({
  id: z.string().optional(),
  type: z.literal("error"),
  payload: z.object({
    code: z.string(),
    message: z.string(),
  }),
});

const draftSnapResolvedEventSchema = z.object({
  id: z.string(),
  type: z.literal("draft_snap_resolved"),
  payload: z.union([
    z.object({
      snap_x: z.number(),
      snap_y: z.number(),
      snap_kind: z.string(),
      snap_label: z.string(),
      host_entity_id: z.string(),
      host_point_id: z.string(),
      host_param_t: z.number().optional(),
    }),
    z.null(),
  ]),
});

export const coreMessageSchema = z.union([
  helloEventSchema,
  pongEventSchema,
  documentCreatedEventSchema,
  documentStateEventSchema,
  sessionStateEventSchema,
  viewportStateEventSchema,
  documentExportedEventSchema,
  documentSavedEventSchema,
  logEventSchema,
  draftSnapResolvedEventSchema,
  errorEventSchema,
]);

