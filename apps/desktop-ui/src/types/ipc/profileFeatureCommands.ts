import type { ExtrudeFeatureParameters } from "../geometry/3d";

export type ExtrudeMode = "new_body" | "join" | "cut" | "intersect";
export type ExtrudeOperation = ExtrudeMode;
export type ExtrudeExtentMode = "one_side" | "symmetric" | "two_sides";
export type ExtrudeExtentType =
  | "distance"
  | "through_all"
  | "to_object"
  | "to_next";
export type ExtrudeThinPlacement = "center" | "inside" | "outside";

export interface ExtrudeSideParameters {
  extent_type: ExtrudeExtentType;
  distance: number;
  start_offset: number;
  taper_angle_degrees: number;
  target_reference_id: string | null;
}

export interface ExtrudeThinParameters {
  enabled: boolean;
  thickness: number;
  placement: ExtrudeThinPlacement;
}

export interface ExtrudeAdvancedParameters {
  extent_mode: ExtrudeExtentMode;
  side1: ExtrudeSideParameters;
  side2: ExtrudeSideParameters | null;
  thin: ExtrudeThinParameters;
  operation: ExtrudeOperation;
  intersect_result: "replace_target" | "new_body";
}

export interface ExtrudeProfileCommand {
  id: string;
  type: "extrude_profile";
  payload: {
    profile_id?: string;
    profile_ids?: string[];
    open_entity_ids?: string[];
    depth: number;
    mode?: ExtrudeMode;
    target_body_id?: string;
    parameters?: Partial<ExtrudeAdvancedParameters>;
  };
}

export interface ExtrudeFaceCommand {
  id: string;
  type: "extrude_face";
  payload: {
    face_id: string;
    depth: number;
    mode?: ExtrudeMode;
    target_body_id?: string;
    parameters?: Partial<ExtrudeAdvancedParameters>;
  };
}

export interface UpdateExtrudeModeCommand {
  id: string;
  type: "update_extrude_mode";
  payload: {
    feature_id: string;
    mode: ExtrudeMode;
  };
}

export interface UpdateExtrudeTargetBodyCommand {
  id: string;
  type: "update_extrude_target_body";
  payload: {
    feature_id: string;
    // Omit (or set undefined) to clear the explicit target and fall back
    // to the most recent body.
    target_body_id?: string;
  };
}

export interface UpdateExtrudeParametersCommand {
  id: string;
  type: "update_extrude_parameters";
  payload: {
    feature_id: string;
    parameters: ExtrudeFeatureParameters;
  };
}

export interface UpdateExtrudeProfilesCommand {
  id: string;
  type: "update_extrude_profiles";
  payload: {
    feature_id: string;
    profile_ids: string[];
  };
}

export interface LoftProfilesCommand {
  id: string;
  type: "loft_profiles";
  payload: {
    profile_ids: string[];
    ruled?: boolean;
  };
}

export interface UpdateLoftProfilesCommand {
  id: string;
  type: "update_loft_profiles";
  payload: {
    feature_id: string;
    profile_ids: string[];
  };
}

export interface UpdateLoftRuledCommand {
  id: string;
  type: "update_loft_ruled";
  payload: {
    feature_id: string;
    ruled: boolean;
  };
}

export interface RevolveProfileCommand {
  id: string;
  type: "revolve_profile";
  payload: {
    profile_id: string;
    axis_entity_id: string;
    angle_degrees?: number;
  };
}

export interface UpdateRevolveProfileCommand {
  id: string;
  type: "update_revolve_profile";
  payload: {
    feature_id: string;
    profile_id: string;
  };
}

export interface UpdateRevolveAxisCommand {
  id: string;
  type: "update_revolve_axis";
  payload: {
    feature_id: string;
    axis_entity_id: string;
  };
}

export interface UpdateRevolveAngleCommand {
  id: string;
  type: "update_revolve_angle";
  payload: {
    feature_id: string;
    angle_degrees: number;
  };
}

export interface SweepProfileCommand {
  id: string;
  type: "sweep_profile";
  payload: {
    profile_id: string;
    path_entity_id: string;
  };
}

export interface UpdateSweepProfileCommand {
  id: string;
  type: "update_sweep_profile";
  payload: {
    feature_id: string;
    profile_id: string;
  };
}

export interface UpdateSweepPathCommand {
  id: string;
  type: "update_sweep_path";
  payload: {
    feature_id: string;
    path_entity_id: string;
  };
}
