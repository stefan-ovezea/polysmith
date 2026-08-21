TopoDS_Shape build_feature_shape(const FeatureEntry& feature) {
  if (feature.kind == "box" && feature.box_parameters.has_value()) {
    return build_box_shape(feature.box_parameters.value());
  }
  if (feature.kind == "cylinder" && feature.cylinder_parameters.has_value()) {
    return build_cylinder_shape(feature.cylinder_parameters.value());
  }
  if (feature.kind == "extrude" && feature.extrude_parameters.has_value()) {
    return build_extrude_shape(feature.extrude_parameters.value());
  }
  if (feature.kind == "loft" && feature.loft_parameters.has_value()) {
    return build_loft_shape(feature.loft_parameters.value());
  }
  if (feature.kind == "revolve" && feature.revolve_parameters.has_value()) {
    return build_revolve_shape(feature.revolve_parameters.value());
  }
  if (feature.kind == "sweep" && feature.sweep_parameters.has_value()) {
    return build_sweep_shape(feature.sweep_parameters.value());
  }
  if (feature.kind == "fastener" && feature.fastener_parameters.has_value()) {
    return build_fastener_shape(feature.fastener_parameters.value());
  }
  if (feature.kind == "plugin_feature" &&
      feature.plugin_parameters.has_value()) {
    return build_plugin_feature_shape(feature.plugin_parameters.value());
  }
  if (feature.kind == "mesh_import" &&
      feature.mesh_import_parameters.has_value()) {
    return build_mesh_import_shape(feature.mesh_import_parameters.value());
  }
  if (feature.kind == "step_import" &&
      feature.step_import_parameters.has_value()) {
    return build_step_import_shape(feature.step_import_parameters.value());
  }
  if (feature.kind == "iges_import" &&
      feature.iges_import_parameters.has_value()) {
    return build_iges_import_shape(feature.iges_import_parameters.value());
  }
  return TopoDS_Shape{};
}
