// Thin orchestrator: all profile detection lives in the exact-curve
// arrangement module (sketch_profile_exact.inc).  Every bounded closed
// region of the arrangement becomes one profile region.
std::vector<SketchProfileRegion> build_sketch_profile_regions(
    const SketchFeatureParameters& parameters) {
  return build_exact_profiles(parameters);
}
