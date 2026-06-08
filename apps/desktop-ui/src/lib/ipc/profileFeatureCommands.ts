import type {
  CoreCommand,
  ExtrudeAdvancedParameters,
  ExtrudeFeatureParameters,
  ExtrudeMode,
} from "@/types";

export function makeExtrudeProfileCommand(
  profileIds: string | readonly string[],
  depth: number,
  mode: ExtrudeMode | null = null,
  targetBodyId: string | null = null,
  parameters: Partial<ExtrudeAdvancedParameters> | null = null,
): CoreCommand {
  const ids = Array.isArray(profileIds) ? [...profileIds] : [profileIds];
  return {
    id: crypto.randomUUID(),
    type: "extrude_profile",
    payload: {
      profile_id: ids[0],
      profile_ids: ids,
      depth,
      ...(mode ? { mode } : {}),
      ...(targetBodyId ? { target_body_id: targetBodyId } : {}),
      ...(parameters ? { parameters } : {}),
    },
  };
}

export function makeExtrudeOpenEntitiesCommand(
  entityIds: readonly string[],
  depth: number,
  mode: ExtrudeMode | null = null,
  targetBodyId: string | null = null,
  parameters: Partial<ExtrudeAdvancedParameters> | null = null,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "extrude_profile",
    payload: {
      open_entity_ids: [...entityIds],
      depth,
      ...(mode ? { mode } : {}),
      ...(targetBodyId ? { target_body_id: targetBodyId } : {}),
      ...(parameters ? { parameters } : {}),
    },
  };
}

export function makeExtrudeFaceCommand(
  faceId: string,
  depth: number,
  mode: ExtrudeMode | null = null,
  targetBodyId: string | null = null,
  parameters: Partial<ExtrudeAdvancedParameters> | null = null,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "extrude_face",
    payload: {
      face_id: faceId,
      depth,
      ...(mode ? { mode } : {}),
      ...(targetBodyId ? { target_body_id: targetBodyId } : {}),
      ...(parameters ? { parameters } : {}),
    },
  };
}

export function makeUpdateExtrudeModeCommand(
  featureId: string,
  mode: ExtrudeMode,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_extrude_mode",
    payload: {
      feature_id: featureId,
      mode,
    },
  };
}

export function makeUpdateExtrudeTargetBodyCommand(
  featureId: string,
  targetBodyId: string | null,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_extrude_target_body",
    payload: {
      feature_id: featureId,
      ...(targetBodyId ? { target_body_id: targetBodyId } : {}),
    },
  };
}

export function makeUpdateExtrudeParametersCommand(
  featureId: string,
  parameters: ExtrudeFeatureParameters,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_extrude_parameters",
    payload: {
      feature_id: featureId,
      parameters,
    },
  };
}

export function makeUpdateExtrudeProfilesCommand(
  featureId: string,
  profileIds: readonly string[],
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_extrude_profiles",
    payload: {
      feature_id: featureId,
      profile_ids: [...profileIds],
    },
  };
}

export function makeLoftProfilesCommand(
  profileIds: readonly string[],
  ruled = false,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "loft_profiles",
    payload: {
      profile_ids: [...profileIds],
      ruled,
    },
  };
}

export function makeUpdateLoftProfilesCommand(
  featureId: string,
  profileIds: readonly string[],
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_loft_profiles",
    payload: {
      feature_id: featureId,
      profile_ids: [...profileIds],
    },
  };
}

export function makeUpdateLoftRuledCommand(
  featureId: string,
  ruled: boolean,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_loft_ruled",
    payload: {
      feature_id: featureId,
      ruled,
    },
  };
}

export function makeRevolveProfileCommand(
  profileId: string,
  axisEntityId: string,
  angleDegrees = 360,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "revolve_profile",
    payload: {
      profile_id: profileId,
      axis_entity_id: axisEntityId,
      angle_degrees: angleDegrees,
    },
  };
}

export function makeUpdateRevolveProfileCommand(
  featureId: string,
  profileId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_revolve_profile",
    payload: {
      feature_id: featureId,
      profile_id: profileId,
    },
  };
}

export function makeUpdateRevolveAxisCommand(
  featureId: string,
  axisEntityId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_revolve_axis",
    payload: {
      feature_id: featureId,
      axis_entity_id: axisEntityId,
    },
  };
}

export function makeUpdateRevolveAngleCommand(
  featureId: string,
  angleDegrees: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_revolve_angle",
    payload: {
      feature_id: featureId,
      angle_degrees: angleDegrees,
    },
  };
}

export function makeSweepProfileCommand(
  profileId: string,
  pathEntityId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "sweep_profile",
    payload: {
      profile_id: profileId,
      path_entity_id: pathEntityId,
    },
  };
}

export function makeUpdateSweepProfileCommand(
  featureId: string,
  profileId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sweep_profile",
    payload: {
      feature_id: featureId,
      profile_id: profileId,
    },
  };
}

export function makeUpdateSweepPathCommand(
  featureId: string,
  pathEntityId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sweep_path",
    payload: {
      feature_id: featureId,
      path_entity_id: pathEntityId,
    },
  };
}
