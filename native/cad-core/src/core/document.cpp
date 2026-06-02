#include "core/document.h"
#include "core/sketch_feature.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRep_Builder.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepTools.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "core/body_compiler.h"
#include "core/construction_plane_feature.h"
#include "core/edge_geometry.h"
#include "core/face_geometry.h"
#include "core/feature_shape.h"
#include "core/formula_eval.h"
#include "core/logger.h"
#include "core/refresh_dependents.h"
#include "core/svg_import.h"
#include "protocol/serialization.h"

namespace polysmith::core {
namespace {

constexpr double kPi = 3.14159265358979323846;

namespace fs = std::filesystem;

std::string feature_id_from_index(int index) {
  return "feature-" + std::to_string(index);
}

std::string import_asset_id_from_index(int index) {
  return "asset-" + std::to_string(index);
}

std::string file_name_from_path(const std::string& path) {
  return fs::path(path).filename().string();
}

std::string extension_from_path(const std::string& path) {
  std::string ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return ext.empty() ? ".dat" : ext;
}

std::string media_type_for_extension(const std::string& extension,
                                     const std::string& fallback) {
  if (extension == ".png") return "image/png";
  if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
  if (extension == ".webp") return "image/webp";
  if (extension == ".gif") return "image/gif";
  if (extension == ".svg") return "image/svg+xml";
  return fallback;
}

fs::path sidecar_assets_dir_for_document(const std::string& file_path) {
  fs::path document_path(file_path);
  return document_path.parent_path() /
         (document_path.stem().string() + ".assets") / "imports";
}

fs::path temp_assets_dir_for_document(const std::string& document_id) {
  return fs::temp_directory_path() / "polysmith-imports" / document_id /
         "imports";
}

std::string relative_import_asset_path(const std::string& asset_id,
                                       const std::string& extension) {
  return (fs::path("imports") / (asset_id + extension)).generic_string();
}

fs::path resolve_asset_path(const std::string& document_file_path,
                            const std::string& relative_asset_path) {
  fs::path document_path(document_file_path);
  return document_path.parent_path() /
         (document_path.stem().string() + ".assets") / relative_asset_path;
}

std::string copy_import_asset(const std::string& source_path,
                              const std::string& destination_dir,
                              const std::string& asset_id) {
  const std::string extension = extension_from_path(source_path);
  fs::create_directories(destination_dir);
  const fs::path destination = fs::path(destination_dir) / (asset_id + extension);
  fs::copy_file(source_path,
                destination,
                fs::copy_options::overwrite_existing);
  return destination.string();
}

std::string import_summary(const ImportPlacementParameters& params) {
  std::ostringstream stream;
  stream << params.file_name << " · " << params.width_mm << " x "
         << params.height_mm << " mm";
  return stream.str();
}

FeatureEntry* find_feature(DocumentState& document, const std::string& id) {
  const auto it = std::find_if(document.feature_history.begin(),
                               document.feature_history.end(),
                               [&](const FeatureEntry& feature) {
                                 return feature.id == id;
                               });
  return it == document.feature_history.end() ? nullptr : &(*it);
}

const FeatureEntry* find_feature(const DocumentState& document,
                                 const std::string& id) {
  const auto it = std::find_if(document.feature_history.begin(),
                               document.feature_history.end(),
                               [&](const FeatureEntry& feature) {
                                 return feature.id == id;
                               });
  return it == document.feature_history.end() ? nullptr : &(*it);
}

void clear_import_selection(DocumentState& document) {
  document.selected_reference_id = std::nullopt;
  document.selected_face_id = std::nullopt;
  document.selected_edge_ids.clear();
  document.selected_vertex_ids.clear();
  document.selected_sketch_point_id = std::nullopt;
  document.selected_sketch_entity_id = std::nullopt;
  document.selected_sketch_dimension_id = std::nullopt;
  document.selected_sketch_profile_id = std::nullopt;
  document.selected_sketch_profile_ids.clear();
  document.selected_sketch_point_ids.clear();
  document.selected_sketch_entity_ids.clear();
}

void remove_feature_by_id(DocumentState& document, const std::string& feature_id) {
  document.feature_history.erase(
      std::remove_if(document.feature_history.begin(),
                     document.feature_history.end(),
                     [&](const FeatureEntry& feature) { return feature.id == feature_id; }),
      document.feature_history.end());
  if (document.selected_feature_id == feature_id) {
    document.selected_feature_id = std::nullopt;
  }
}

void remove_import_asset_if_unreferenced(
    const DocumentState& document,
    const ImportPlacementParameters& params) {
  if (params.asset_path.empty() || params.asset_id.empty()) {
    return;
  }
  for (const auto& feature : document.feature_history) {
    const ImportPlacementParameters* other = nullptr;
    if (feature.image_import_parameters.has_value()) {
      other = &feature.image_import_parameters.value();
    } else if (feature.svg_import_parameters.has_value()) {
      other = &feature.svg_import_parameters.value();
    }
    if (other != nullptr && other->asset_id == params.asset_id) {
      return;
    }
  }
  std::error_code ignored;
  fs::remove(params.asset_path, ignored);
}

std::pair<double, double> transform_svg_point(
    const SvgImportResult& svg,
    const SvgImportFeatureParameters& params,
    double x,
    double y) {
  const double scale_x = params.width_mm / std::max(svg.width, 0.001);
  const double scale_y = params.height_mm / std::max(svg.height, 0.001);
  double u = (x - svg.width * 0.5) * scale_x;
  double v = (svg.height * 0.5 - y) * scale_y;
  const double angle = params.rotation_degrees * kPi / 180.0;
  const double ca = std::cos(angle);
  const double sa = std::sin(angle);
  return {params.offset_u_mm + u * ca - v * sa,
          params.offset_v_mm + u * sa + v * ca};
}

bool is_origin_plane_reference(const std::string& reference_id) {
  return reference_id == "ref-plane-xy" || reference_id == "ref-plane-yz" ||
         reference_id == "ref-plane-xz";
}

int action_count(const DocumentState& document) {
  int count = 0;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "root_part") {
      ++count;
    }
  }
  return count;
}

bool includes_feature_at_cursor(const DocumentState& document,
                                const std::string& feature_id) {
  int remaining_actions =
      document.timeline_cursor.value_or(action_count(document));
  for (const auto& feature : document.feature_history) {
    if (feature.id != feature_id) {
      if (feature.kind != "root_part" && remaining_actions > 0) {
        --remaining_actions;
      }
      continue;
    }
    return feature.kind == "root_part" || remaining_actions > 0;
  }
  return false;
}

void mark_feature_healthy(FeatureEntry& feature) {
  feature.status = "healthy";
  feature.dependency_broken = false;
  feature.dependency_warning.clear();
}

std::string make_move_parameters_summary(const MoveFeatureParameters& params) {
  std::ostringstream stream;
  stream << "Move " << params.translation_x << ", " << params.translation_y
         << ", " << params.translation_z << " mm · rotate "
         << params.rotation_x_degrees << ", " << params.rotation_y_degrees
         << ", " << params.rotation_z_degrees << " deg";
  return stream.str();
}

std::string feature_name_by_id(const DocumentState& document,
                               const std::string& feature_id) {
  const auto it = std::find_if(
      document.feature_history.begin(),
      document.feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  return it == document.feature_history.end() ? std::string{} : it->name;
}

void write_copy_frame(BodyCopyFeatureParameters& params,
                      const BodyLocalFrame& frame) {
  params.local_x_axis_x = frame.x_axis_x;
  params.local_x_axis_y = frame.x_axis_y;
  params.local_x_axis_z = frame.x_axis_z;
  params.local_y_axis_x = frame.y_axis_x;
  params.local_y_axis_y = frame.y_axis_y;
  params.local_y_axis_z = frame.y_axis_z;
  params.local_z_axis_x = frame.z_axis_x;
  params.local_z_axis_y = frame.z_axis_y;
  params.local_z_axis_z = frame.z_axis_z;
}

std::string serialize_shape_snapshot(const TopoDS_Shape& shape) {
  std::ostringstream stream;
  try {
    BRepTools::Write(shape, stream);
  } catch (const Standard_Failure&) {
    throw std::runtime_error("Could not serialize copied body shape");
  }
  return stream.str();
}

void mark_extrude_preview_warning(FeatureEntry& feature,
                                  const std::string& message) {
  feature.status = "warning";
  feature.dependency_broken = true;
  feature.dependency_warning = message;
  feature.parameters_summary = "Preview unavailable";
}

void apply_extrude_parameters_with_preview_validation(
    FeatureEntry& feature,
    const ExtrudeFeatureParameters& parameters) {
  feature.extrude_parameters = parameters;
  try {
    const TopoDS_Shape preview_shape = build_extrude_shape(parameters);
    if (preview_shape.IsNull()) {
      throw std::runtime_error("Extrude preview produced an empty shape");
    }
    mark_feature_healthy(feature);
    feature.parameters_summary =
        parameters.profile_id + " · " + std::to_string(parameters.depth) + " mm";
  } catch (const std::exception& error) {
    mark_extrude_preview_warning(feature, error.what());
  }
}

void normalize_timeline_cursor(DocumentState& document) {
  if (!document.timeline_cursor.has_value()) {
    return;
  }
  const int max_actions = action_count(document);
  if (document.timeline_cursor.value() >= max_actions) {
    document.timeline_cursor = std::nullopt;
  } else if (document.timeline_cursor.value() < 0) {
    document.timeline_cursor = 0;
  }
}

// True for any plane the user can pick from the viewport's reference
// hierarchy: the three origin planes plus every construction plane
// currently in `document.feature_history`. Face picks go through
// `select_face` instead and aren't covered here.
bool is_selectable_plane_reference(const DocumentState& document,
                                   const std::string& reference_id) {
  if (is_origin_plane_reference(reference_id)) {
    return true;
  }
  for (const auto& feature : document.feature_history) {
    if (feature.id == reference_id && feature.kind == "construction_plane") {
      return true;
    }
  }
  return false;
}

// Convert a `PlaneFrame` (the core-wide form used by the dependency
// resolver) into the sketch-specific copy stored on
// `SketchFeatureParameters`. Same field-by-field layout.
SketchFeatureParameters::SketchPlaneFrame to_sketch_plane_frame(
    const PlaneFrame& frame) {
  return SketchFeatureParameters::SketchPlaneFrame{
      .origin_x = frame.origin_x,
      .origin_y = frame.origin_y,
      .origin_z = frame.origin_z,
      .x_axis_x = frame.x_axis_x,
      .x_axis_y = frame.x_axis_y,
      .x_axis_z = frame.x_axis_z,
      .y_axis_x = frame.y_axis_x,
      .y_axis_y = frame.y_axis_y,
      .y_axis_z = frame.y_axis_z,
      .normal_x = frame.normal_x,
      .normal_y = frame.normal_y,
      .normal_z = frame.normal_z,
  };
}

std::string imported_svg_sketch_name(const SvgImportFeatureParameters& params,
                                     const SvgLayer& layer,
                                     bool multiple_layers) {
  const std::string base =
      params.file_name.empty() ? "Imported SVG" : params.file_name;
  if (!multiple_layers) {
    return params.file_name.empty() ? "Imported SVG Sketch" : params.file_name;
  }
  return base + " - " + (layer.name.empty() ? "Layer" : layer.name);
}

std::vector<std::string> append_svg_sketches_to_document(
    DocumentState& document,
    int& next_feature_id,
    int& next_sketch_line_id,
    const SvgImportFeatureParameters& params,
    const SvgImportResult& svg) {
  std::vector<const SvgLayer*> layers;
  for (const auto& layer : svg.layers) {
    if (!layer.segments.empty()) {
      layers.push_back(&layer);
    }
  }
  if (layers.empty()) {
    throw std::runtime_error("SVG did not contain usable vector geometry");
  }

  std::vector<std::string> created_ids;
  const bool multiple_layers = layers.size() > 1;
  for (const auto* layer : layers) {
    FeatureEntry sketch;
    sketch.id = feature_id_from_index(next_feature_id++);
    sketch.kind = "sketch";
    sketch.name = imported_svg_sketch_name(params, *layer, multiple_layers);
    sketch.status = "ok";
    SketchFeatureParameters sketch_params;
    sketch_params.plane_id = params.plane_id;
    if (params.plane_frame.has_value()) {
      sketch_params.plane_frame = to_sketch_plane_frame(params.plane_frame.value());
    }
    sketch_params.active_tool = "select";
    for (const auto& segment : layer->segments) {
      const auto [sx, sy] =
          transform_svg_point(svg, params, segment.start_x, segment.start_y);
      const auto [ex, ey] =
          transform_svg_point(svg, params, segment.end_x, segment.end_y);
      const int line_index = next_sketch_line_id++;
      sketch_params.lines.push_back(SketchLine{
          .id = "line-" + std::to_string(line_index),
          .start_point_id = "point-line-" + std::to_string(line_index) + "-start",
          .end_point_id = "point-line-" + std::to_string(line_index) + "-end",
          .start_x = sx,
          .start_y = sy,
          .end_x = ex,
          .end_y = ey,
          .constraint = std::nullopt,
          .is_construction = false,
      });
    }
    sketch.sketch_parameters = sketch_params;
    refresh_sketch_derived_state(sketch);
    sketch.parameters_summary =
        sketch_params.plane_id + " · " +
        std::to_string(sketch.sketch_parameters->lines.size()) +
        " imported lines";
    created_ids.push_back(sketch.id);
    document.feature_history.push_back(std::move(sketch));
  }
  return created_ids;
}

// Test whether two solid shapes intersect with non-zero volume. Used by
// the auto-cut detector at extrude-creation time so a new_body extrude
// that overlaps an existing body is silently promoted to a cut.
bool shapes_intersect_with_volume(const TopoDS_Shape& a,
                                  const TopoDS_Shape& b) {
  if (a.IsNull() || b.IsNull()) {
    return false;
  }
  try {
    BRepAlgoAPI_Common common(a, b);
    common.Build();
    if (!common.IsDone()) {
      return false;
    }
    const TopoDS_Shape result = common.Shape();
    if (result.IsNull()) {
      return false;
    }
    // Any solid in the result -> the inputs share volume. We don't
    // dilute to TopAbs_FACE because two adjacent (touching but not
    // overlapping) solids share faces without sharing volume, and
    // promoting that to a cut would surprise the user.
    TopExp_Explorer explorer(result, TopAbs_SOLID);
    return explorer.More();
  } catch (const std::exception&) {
    return false;
  }
}

bool shapes_touch_without_volume(const TopoDS_Shape& a,
                                 const TopoDS_Shape& b) {
  if (a.IsNull() || b.IsNull()) {
    return false;
  }
  try {
    BRepAlgoAPI_Common common(a, b);
    common.Build();
    if (!common.IsDone()) {
      return false;
    }
    const TopoDS_Shape result = common.Shape();
    if (result.IsNull()) {
      return false;
    }
    if (TopExp_Explorer(result, TopAbs_SOLID).More()) {
      return false;
    }
    if (TopExp_Explorer(result, TopAbs_FACE).More() ||
        TopExp_Explorer(result, TopAbs_EDGE).More() ||
        TopExp_Explorer(result, TopAbs_VERTEX).More()) {
      return true;
    }
  } catch (const std::exception&) {
  }
  try {
    BRepExtrema_DistShapeShape distance(a, b);
    distance.Perform();
    return distance.IsDone() && distance.Value() <= 1.0e-6;
  } catch (const std::exception&) {
    return false;
  }
}

// Check whether a candidate extrude (built from `parameters`) would
// overlap any existing body in `document`. Returns the body id of the
// first such body, or nullopt when no intersection is found. Bodies
// derived from features after the candidate (in feature_history order)
// are excluded — we only want to detect overlap with bodies that exist
// "now" from the user's point of view.
std::optional<std::string> find_intersecting_body_for_extrude(
    const DocumentState& document,
    const ExtrudeFeatureParameters& parameters) {
  TopoDS_Shape candidate;
  try {
    candidate = build_extrude_shape(parameters);
  } catch (const std::exception&) {
    return std::nullopt;
  }
  if (candidate.IsNull()) {
    return std::nullopt;
  }

  const CompiledBodies compiled = compile_bodies(document);
  for (const auto& body : compiled.bodies) {
    if (body.shape.IsNull()) {
      continue;
    }
    if (shapes_intersect_with_volume(body.shape, candidate)) {
      return body.id;
    }
  }
  return std::nullopt;
}

std::optional<std::string> find_touching_body_for_extrude(
    const DocumentState& document,
    const ExtrudeFeatureParameters& parameters) {
  TopoDS_Shape candidate;
  try {
    candidate = build_extrude_shape(parameters);
  } catch (const std::exception&) {
    return std::nullopt;
  }
  if (candidate.IsNull()) {
    return std::nullopt;
  }

  const CompiledBodies compiled = compile_bodies(document);
  for (const auto& body : compiled.bodies) {
    if (body.shape.IsNull()) {
      continue;
    }
    if (shapes_touch_without_volume(body.shape, candidate)) {
      return body.id;
    }
  }
  return std::nullopt;
}

std::string face_owner_id(const std::string& face_id) {
  const auto separator = face_id.find(":face:");
  if (separator == std::string::npos) {
    return "";
  }

  return face_id.substr(0, separator);
}

bool is_supported_sketch_tool(const std::string& tool) {
  return tool == "select" || tool == "line" || tool == "rectangle" ||
         tool == "circle" || tool == "polygon" || tool == "arc" || tool == "fillet" ||
         tool == "trim" || tool == "project" || tool == "dimension";
}

std::string plane_id_from_frame(
    const SketchFeatureParameters::SketchPlaneFrame& plane_frame) {
  const double abs_x = std::abs(plane_frame.normal_x);
  const double abs_y = std::abs(plane_frame.normal_y);
  const double abs_z = std::abs(plane_frame.normal_z);

  if (abs_x >= abs_y && abs_x >= abs_z) {
    return "ref-plane-yz";
  }

  if (abs_y >= abs_x && abs_y >= abs_z) {
    return "ref-plane-xy";
  }

  return "ref-plane-xz";
}

std::string plane_id_from_frame(const PlaneFrame& plane_frame) {
  const double abs_x = std::abs(plane_frame.normal_x);
  const double abs_y = std::abs(plane_frame.normal_y);
  const double abs_z = std::abs(plane_frame.normal_z);

  if (abs_x >= abs_y && abs_x >= abs_z) {
    return "ref-plane-yz";
  }

  if (abs_y >= abs_x && abs_y >= abs_z) {
    return "ref-plane-xy";
  }

  return "ref-plane-xz";
}

PlaneFrame make_plane_frame(
    const SketchFeatureParameters::SketchPlaneFrame& plane_frame) {
  return PlaneFrame{
      .origin_x = plane_frame.origin_x,
      .origin_y = plane_frame.origin_y,
      .origin_z = plane_frame.origin_z,
      .x_axis_x = plane_frame.x_axis_x,
      .x_axis_y = plane_frame.x_axis_y,
      .x_axis_z = plane_frame.x_axis_z,
      .y_axis_x = plane_frame.y_axis_x,
      .y_axis_y = plane_frame.y_axis_y,
      .y_axis_z = plane_frame.y_axis_z,
      .normal_x = plane_frame.normal_x,
      .normal_y = plane_frame.normal_y,
      .normal_z = plane_frame.normal_z,
  };
}

gp_Vec extrude_normal_vector(const ExtrudeFeatureParameters& params,
                             double side_sign) {
  if (params.plane_frame.has_value()) {
    return gp_Vec(params.plane_frame->normal_x * side_sign,
                  params.plane_frame->normal_y * side_sign,
                  params.plane_frame->normal_z * side_sign);
  }
  if (params.plane_id == "ref-plane-xy") {
    return gp_Vec(0.0, side_sign, 0.0);
  }
  if (params.plane_id == "ref-plane-yz") {
    return gp_Vec(side_sign, 0.0, 0.0);
  }
  return gp_Vec(0.0, 0.0, side_sign);
}

gp_Pnt extrude_plane_origin(const ExtrudeFeatureParameters& params) {
  if (params.plane_frame.has_value()) {
    return gp_Pnt(params.plane_frame->origin_x,
                  params.plane_frame->origin_y,
                  params.plane_frame->origin_z);
  }
  return gp_Pnt(0.0, 0.0, 0.0);
}

std::optional<std::pair<double, double>> body_projection_range(
    const TopoDS_Shape& shape,
    const gp_Pnt& origin,
    const gp_Vec& direction) {
  if (shape.IsNull()) {
    return std::nullopt;
  }
  Bnd_Box box;
  BRepBndLib::Add(shape, box);
  if (box.IsVoid()) {
    return std::nullopt;
  }
  double xmin = 0.0;
  double ymin = 0.0;
  double zmin = 0.0;
  double xmax = 0.0;
  double ymax = 0.0;
  double zmax = 0.0;
  box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
  const std::array<gp_Pnt, 8> corners = {
      gp_Pnt(xmin, ymin, zmin),
      gp_Pnt(xmin, ymin, zmax),
      gp_Pnt(xmin, ymax, zmin),
      gp_Pnt(xmin, ymax, zmax),
      gp_Pnt(xmax, ymin, zmin),
      gp_Pnt(xmax, ymin, zmax),
      gp_Pnt(xmax, ymax, zmin),
      gp_Pnt(xmax, ymax, zmax),
  };
  double min_projection = std::numeric_limits<double>::infinity();
  double max_projection = -std::numeric_limits<double>::infinity();
  for (const auto& corner : corners) {
    const gp_Vec delta(origin, corner);
    const double projection = delta.Dot(direction);
    min_projection = std::min(min_projection, projection);
    max_projection = std::max(max_projection, projection);
  }
  return std::make_pair(min_projection, max_projection);
}

std::optional<double> distance_to_body_boundary(
    const CompiledBodies& compiled,
    const std::optional<std::string>& body_id,
    const gp_Pnt& origin,
    const gp_Vec& direction,
    double start_offset,
    bool through_all) {
  std::optional<double> best;
  for (const auto& body : compiled.bodies) {
    if (body_id.has_value() && body.id != body_id.value()) {
      continue;
    }
    const auto range = body_projection_range(body.shape, origin, direction);
    if (!range.has_value()) {
      continue;
    }
    const double target =
        through_all ? range->second + std::max(1.0, range->second - range->first) * 0.05
                    : (range->first > start_offset ? range->first : range->second);
    const double distance = target - start_offset;
    if (distance <= 1.0e-6) {
      continue;
    }
    if (!best.has_value() || distance < best.value()) {
      best = distance;
    }
  }
  return best;
}

std::optional<double> distance_to_face_plane(
    const DocumentState& document,
    const std::string& face_id,
    const gp_Pnt& origin,
    const gp_Vec& direction,
    double start_offset) {
  const auto profile = compute_planar_face_profile(document, face_id);
  if (!profile.has_value()) {
    return std::nullopt;
  }
  const gp_Pnt target_origin(profile->plane_frame.origin_x,
                             profile->plane_frame.origin_y,
                             profile->plane_frame.origin_z);
  const gp_Vec target_normal(profile->plane_frame.normal_x,
                             profile->plane_frame.normal_y,
                             profile->plane_frame.normal_z);
  const double denom = direction.Dot(target_normal);
  if (std::abs(denom) <= 1.0e-8) {
    return std::nullopt;
  }
  const double t = gp_Vec(origin, target_origin).Dot(target_normal) / denom;
  const double distance = t - start_offset;
  return distance > 1.0e-6 ? std::optional<double>(distance) : std::nullopt;
}

void resolve_extrude_side_extent(
    const DocumentState& document,
    ExtrudeFeatureParameters& params,
    ExtrudeFeatureParameters::SideParameters& side,
    double side_sign) {
  if (side.extent_type == "distance") {
    return;
  }

  const CompiledBodies compiled = compile_bodies(document);
  const gp_Pnt origin = extrude_plane_origin(params);
  const gp_Vec direction = extrude_normal_vector(params, side_sign);
  std::optional<double> distance;

  if (side.extent_type == "to_object" && side.target_reference_id.has_value() &&
      side.target_reference_id->find(":face:") != std::string::npos) {
    distance = distance_to_face_plane(document,
                                      side.target_reference_id.value(),
                                      origin,
                                      direction,
                                      side.start_offset);
  } else if (side.extent_type == "through_all" ||
             side.extent_type == "to_object") {
    const std::optional<std::string> body_id =
        side.target_reference_id.has_value()
            ? side.target_reference_id
            : params.target_body_id;
    if (side.extent_type == "through_all" && !body_id.has_value()) {
      throw std::runtime_error("Through All extrude requires a target body");
    }
    distance = distance_to_body_boundary(compiled,
                                         body_id,
                                         origin,
                                         direction,
                                         side.start_offset,
                                         side.extent_type == "through_all");
  } else if (side.extent_type == "to_next") {
    distance = distance_to_body_boundary(compiled,
                                         std::nullopt,
                                         origin,
                                         direction,
                                         side.start_offset,
                                         false);
  }

  if (!distance.has_value()) {
    throw std::runtime_error("Unable to resolve extrude extent: " +
                             side.extent_type);
  }
  side.distance = distance.value();
}

void normalize_extrude_parameters(const DocumentState& document,
                                  ExtrudeFeatureParameters& params) {
  const double abs_depth = std::abs(params.depth);
  const bool default_side =
      params.side1.extent_type == "distance" &&
      params.side1.distance == 10.0 &&
      params.side1.start_offset == 0.0 &&
      params.side1.taper_angle_degrees == 0.0 &&
      !params.side1.target_reference_id.has_value();
  if (default_side) {
    params.side1.distance = abs_depth > 0.0 ? abs_depth : 10.0;
  }
  if (params.operation.empty()) {
    params.operation = params.mode.empty() ? "new_body" : params.mode;
  }
  if (params.operation == "join" && !params.target_body_id.has_value()) {
    params.mode = "new_body";
  } else {
    params.mode = params.operation;
  }

  if (params.extent_mode != "one_side" && params.extent_mode != "symmetric" &&
      params.extent_mode != "two_sides") {
    params.extent_mode = "one_side";
  }
  if (params.extent_mode == "symmetric" && !params.side2.has_value()) {
    params.side2 = params.side1;
  }
  if (params.extent_mode == "two_sides" && !params.side2.has_value()) {
    params.side2 = params.side1;
  }

  resolve_extrude_side_extent(document, params, params.side1, params.depth < 0 ? -1.0 : 1.0);
  if (params.side2.has_value()) {
    resolve_extrude_side_extent(document, params, params.side2.value(), params.depth < 0 ? 1.0 : -1.0);
  }

  if (params.extent_mode == "symmetric") {
    params.depth = (params.depth < 0.0 ? -1.0 : 1.0) * params.side1.distance;
  } else {
    params.depth = (params.depth < 0.0 ? -1.0 : 1.0) * params.side1.distance;
  }

}

std::vector<SketchProfilePoint> sample_circle_profile_points(
    const SketchProfileRegion& profile) {
  std::vector<SketchProfilePoint> points;
  constexpr int kCircleSegments = 64;
  points.reserve(kCircleSegments);
  for (int index = 0; index < kCircleSegments; ++index) {
    const double angle =
        (static_cast<double>(index) / static_cast<double>(kCircleSegments)) *
        2.0 * 3.14159265358979323846;
    points.push_back(SketchProfilePoint{
        .x = profile.center_x + profile.radius * std::cos(angle),
        .y = profile.center_y + profile.radius * std::sin(angle),
    });
  }
  return points;
}

std::optional<ExtrudeFeatureParameters> make_extrude_parameters_for_profile(
    const FeatureEntry& sketch_feature,
    const SketchProfileRegion& profile,
    double depth) {
  if (!sketch_feature.sketch_parameters.has_value()) {
    return std::nullopt;
  }

  const auto& sketch = sketch_feature.sketch_parameters.value();
  const std::string plane_id = sketch.plane_frame.has_value()
                                   ? plane_id_from_frame(sketch.plane_frame.value())
                                   : sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(make_plane_frame(sketch.plane_frame.value()))
          : std::nullopt;

  if (profile.kind == "polygon") {
    return ExtrudeFeatureParameters{
        .sketch_feature_id = sketch_feature.id,
        .profile_id = profile.id,
        .profile_ids = {profile.id},
        .plane_id = plane_id,
        .plane_frame = plane_frame,
        .profile_kind = "polygon",
        .start_x = 0.0,
        .start_y = 0.0,
        .width = 0.0,
        .height = 0.0,
        .radius = 0.0,
        .profile_points = profile.points,
        .inner_loops = profile.inner_loops,
        .depth = depth,
    };
  }

  if (profile.kind == "circle") {
    return ExtrudeFeatureParameters{
        .sketch_feature_id = sketch_feature.id,
        .profile_id = profile.id,
        .profile_ids = {profile.id},
        .plane_id = plane_id,
        .plane_frame = plane_frame,
        .profile_kind = "circle",
        .start_x = profile.center_x,
        .start_y = profile.center_y,
        .width = 0.0,
        .height = 0.0,
        .radius = profile.radius,
        .profile_points = {},
        .depth = depth,
    };
  }

  return std::nullopt;
}

std::vector<SketchProfilePoint> profile_boundary_points(
    const SketchProfileRegion& profile) {
  return profile.kind == "circle" ? sample_circle_profile_points(profile)
                                  : profile.points;
}

double squared_distance(const SketchProfilePoint& a,
                        const SketchProfilePoint& b) {
  const double dx = a.x - b.x;
  const double dy = a.y - b.y;
  return dx * dx + dy * dy;
}

double cross(const SketchProfilePoint& a,
             const SketchProfilePoint& b,
             const SketchProfilePoint& c) {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool point_on_segment(const SketchProfilePoint& point,
                      const SketchProfilePoint& a,
                      const SketchProfilePoint& b,
                      double tolerance) {
  if (std::abs(cross(a, b, point)) > tolerance) {
    return false;
  }
  return point.x >= std::min(a.x, b.x) - tolerance &&
         point.x <= std::max(a.x, b.x) + tolerance &&
         point.y >= std::min(a.y, b.y) - tolerance &&
         point.y <= std::max(a.y, b.y) + tolerance;
}

bool segments_touch(const SketchProfilePoint& a1,
                    const SketchProfilePoint& a2,
                    const SketchProfilePoint& b1,
                    const SketchProfilePoint& b2,
                    double tolerance) {
  if (point_on_segment(a1, b1, b2, tolerance) ||
      point_on_segment(a2, b1, b2, tolerance) ||
      point_on_segment(b1, a1, a2, tolerance) ||
      point_on_segment(b2, a1, a2, tolerance)) {
    return true;
  }

  const double c1 = cross(a1, a2, b1);
  const double c2 = cross(a1, a2, b2);
  const double c3 = cross(b1, b2, a1);
  const double c4 = cross(b1, b2, a2);
  return ((c1 > tolerance && c2 < -tolerance) ||
          (c1 < -tolerance && c2 > tolerance)) &&
         ((c3 > tolerance && c4 < -tolerance) ||
          (c3 < -tolerance && c4 > tolerance));
}

bool profile_regions_touch(const SketchProfileRegion& a,
                           const SketchProfileRegion& b) {
  constexpr double kTolerance = 1.0e-6;
  constexpr double kToleranceSquared = kTolerance * kTolerance;
  const std::vector<SketchProfilePoint> a_points = profile_boundary_points(a);
  const std::vector<SketchProfilePoint> b_points = profile_boundary_points(b);
  if (a_points.empty() || b_points.empty()) {
    return false;
  }

  for (const auto& a_point : a_points) {
    for (const auto& b_point : b_points) {
      if (squared_distance(a_point, b_point) <= kToleranceSquared) {
        return true;
      }
    }
  }

  for (size_t a_index = 0; a_index < a_points.size(); ++a_index) {
    const auto& a1 = a_points[a_index];
    const auto& a2 = a_points[(a_index + 1) % a_points.size()];
    for (size_t b_index = 0; b_index < b_points.size(); ++b_index) {
      const auto& b1 = b_points[b_index];
      const auto& b2 = b_points[(b_index + 1) % b_points.size()];
      if (segments_touch(a1, a2, b1, b2, kTolerance)) {
        return true;
      }
    }
  }

  return false;
}

std::vector<std::vector<SketchProfileRegion>> group_touching_profiles(
    const std::vector<SketchProfileRegion>& profiles) {
  std::vector<std::vector<SketchProfileRegion>> groups;
  std::vector<bool> visited(profiles.size(), false);

  for (size_t start = 0; start < profiles.size(); ++start) {
    if (visited[start]) {
      continue;
    }

    std::vector<size_t> stack{start};
    visited[start] = true;
    std::vector<SketchProfileRegion> group;

    while (!stack.empty()) {
      const size_t current = stack.back();
      stack.pop_back();
      group.push_back(profiles[current]);

      for (size_t candidate = 0; candidate < profiles.size(); ++candidate) {
        if (visited[candidate]) {
          continue;
        }
        if (!profile_regions_touch(profiles[current], profiles[candidate])) {
          continue;
        }
        visited[candidate] = true;
        stack.push_back(candidate);
      }
    }

    groups.push_back(std::move(group));
  }

  return groups;
}

std::optional<ExtrudeFeatureParameters> make_extrude_parameters_for_profiles(
    const FeatureEntry& sketch_feature,
    const std::vector<SketchProfileRegion>& profiles,
    double depth) {
  if (profiles.empty() || !sketch_feature.sketch_parameters.has_value()) {
    return std::nullopt;
  }
  if (profiles.size() == 1) {
    return make_extrude_parameters_for_profile(sketch_feature,
                                               profiles.front(),
                                               depth);
  }

  const auto& sketch = sketch_feature.sketch_parameters.value();
  const auto& first = profiles.front();
  const std::string plane_id = sketch.plane_frame.has_value()
                                   ? plane_id_from_frame(sketch.plane_frame.value())
                                   : sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(make_plane_frame(sketch.plane_frame.value()))
          : std::nullopt;
  std::vector<std::string> ids;
  for (const auto& profile : profiles) {
    ids.push_back(profile.id);
  }

  ExtrudeFeatureParameters parameters{
      .sketch_feature_id = sketch_feature.id,
      .profile_id = ids.front(),
      .profile_ids = ids,
      .plane_id = plane_id,
      .plane_frame = plane_frame,
      .profile_kind = "polygon",
      .start_x = 0.0,
      .start_y = 0.0,
      .width = 0.0,
      .height = 0.0,
      .radius = 0.0,
      .profile_points = profile_boundary_points(first),
      .inner_loops = first.inner_loops,
      .depth = depth,
  };

  for (size_t index = 1; index < profiles.size(); ++index) {
    const auto& profile = profiles[index];
    parameters.additional_profile_points.push_back(
        profile_boundary_points(profile));
    parameters.additional_inner_loops.push_back(profile.inner_loops);
  }

  return parameters;
}

void preserve_extrude_source_geometry(ExtrudeFeatureParameters& target,
                                      const ExtrudeFeatureParameters& source) {
  target.sketch_feature_id = source.sketch_feature_id;
  target.profile_id = source.profile_id;
  target.profile_ids = source.profile_ids;
  target.open_entity_ids = source.open_entity_ids;
  target.plane_id = source.plane_id;
  target.plane_frame = source.plane_frame;
  target.profile_kind = source.profile_kind;
  target.start_x = source.start_x;
  target.start_y = source.start_y;
  target.width = source.width;
  target.height = source.height;
  target.radius = source.radius;
  target.profile_points = source.profile_points;
  target.inner_loops = source.inner_loops;
  target.additional_profile_points = source.additional_profile_points;
  target.additional_inner_loops = source.additional_inner_loops;
}

void normalize_extrude_operation_mode(ExtrudeFeatureParameters& parameters) {
  if (parameters.operation.empty()) {
    parameters.operation =
        parameters.mode.empty() ? std::string("new_body") : parameters.mode;
  }
  if (parameters.operation == "join" && !parameters.target_body_id.has_value()) {
    parameters.mode = "new_body";
    return;
  }
  parameters.mode = parameters.operation;
}

void apply_automatic_extrude_mode(const DocumentState& document,
                                  ExtrudeFeatureParameters& parameters,
                                  bool grouped_touching_profiles) {
  parameters.operation = "new_body";
  parameters.mode = "new_body";
  parameters.target_body_id = std::nullopt;

  if (const auto touching =
          find_touching_body_for_extrude(document, parameters);
      touching.has_value()) {
    parameters.operation = "join";
    parameters.mode = "join";
    parameters.target_body_id = touching;
    return;
  }

  if (const auto intersected =
          find_intersecting_body_for_extrude(document, parameters);
      intersected.has_value()) {
    parameters.operation = "cut";
    parameters.mode = "cut";
    parameters.target_body_id = intersected;
    return;
  }

  if (grouped_touching_profiles) {
    parameters.operation = "join";
    parameters.mode = "new_body";
  }
}

std::optional<LoftSectionParameters> make_loft_section_for_profile(
    const FeatureEntry& sketch_feature,
    const SketchProfileRegion& profile) {
  if (!sketch_feature.sketch_parameters.has_value()) {
    return std::nullopt;
  }
  if (!profile.inner_loops.empty()) {
    return std::nullopt;
  }

  const auto& sketch = sketch_feature.sketch_parameters.value();
  const std::string plane_id = sketch.plane_frame.has_value()
                                   ? plane_id_from_frame(sketch.plane_frame.value())
                                   : sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(make_plane_frame(sketch.plane_frame.value()))
          : std::nullopt;

  return LoftSectionParameters{
      .sketch_feature_id = sketch_feature.id,
      .profile_id = profile.id,
      .plane_id = plane_id,
      .plane_frame = plane_frame,
      .profile_points = profile.kind == "circle"
                            ? sample_circle_profile_points(profile)
                            : profile.points,
  };
}

std::array<double, 3> revolve_sketch_local_to_world(
    const SketchFeatureParameters& sketch,
    double local_x,
    double local_y) {
  if (sketch.plane_frame.has_value()) {
    const auto& frame = sketch.plane_frame.value();
    return {
        frame.origin_x + frame.x_axis_x * local_x + frame.y_axis_x * local_y,
        frame.origin_y + frame.x_axis_y * local_x + frame.y_axis_y * local_y,
        frame.origin_z + frame.x_axis_z * local_x + frame.y_axis_z * local_y,
    };
  }
  if (sketch.plane_id == "ref-plane-xy") {
    return {local_x, 0.0, local_y};
  }
  if (sketch.plane_id == "ref-plane-yz") {
    return {0.0, local_x, local_y};
  }
  if (sketch.plane_id == "ref-plane-xz") {
    return {local_x, local_y, 0.0};
  }
  throw std::runtime_error("Unsupported sketch plane: " + sketch.plane_id);
}

SketchLine* find_line_by_id(SketchFeatureParameters& sketch,
                            const std::string& line_id);
const SketchLine* find_line_by_id(const SketchFeatureParameters& sketch,
                                  const std::string& line_id);

std::optional<RevolveFeatureParameters> make_revolve_parameters_for_profile(
    const FeatureEntry& profile_sketch_feature,
    const SketchProfileRegion& profile,
    const FeatureEntry& axis_sketch_feature,
    const SketchLine& axis_line,
    double angle_degrees) {
  if (!profile_sketch_feature.sketch_parameters.has_value() ||
      !axis_sketch_feature.sketch_parameters.has_value()) {
    return std::nullopt;
  }

  const auto& profile_sketch = profile_sketch_feature.sketch_parameters.value();
  const auto& axis_sketch = axis_sketch_feature.sketch_parameters.value();
  const std::string plane_id =
      profile_sketch.plane_frame.has_value()
          ? plane_id_from_frame(profile_sketch.plane_frame.value())
          : profile_sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      profile_sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(
                make_plane_frame(profile_sketch.plane_frame.value()))
          : std::nullopt;
  const auto axis_start = revolve_sketch_local_to_world(
      axis_sketch, axis_line.start_x, axis_line.start_y);
  const auto axis_end = revolve_sketch_local_to_world(
      axis_sketch, axis_line.end_x, axis_line.end_y);

  return RevolveFeatureParameters{
      .sketch_feature_id = profile_sketch_feature.id,
      .profile_id = profile.id,
      .plane_id = plane_id,
      .plane_frame = plane_frame,
      .profile_kind = profile.kind,
      .profile_points = profile.kind == "circle"
                            ? sample_circle_profile_points(profile)
                            : profile.points,
      .inner_loops = profile.inner_loops,
      .axis_sketch_feature_id = axis_sketch_feature.id,
      .axis_entity_id = axis_line.id,
      .axis_start_x = axis_start[0],
      .axis_start_y = axis_start[1],
      .axis_start_z = axis_start[2],
      .axis_end_x = axis_end[0],
      .axis_end_y = axis_end[1],
      .axis_end_z = axis_end[2],
      .angle_degrees = angle_degrees,
  };
}

bool profile_id_mentions_entity(const std::string& profile_id,
                                const std::string& entity_id) {
  const std::string needle = "-" + entity_id;
  size_t position = profile_id.find(needle);
  while (position != std::string::npos) {
    const size_t after = position + needle.size();
    if (after == profile_id.size() || profile_id[after] == '-') {
      return true;
    }
    position = profile_id.find(needle, position + 1);
  }
  return false;
}

const SketchProfileRegion* find_profile_by_id(
    const std::vector<SketchProfileRegion>& profiles,
    const std::string& profile_id) {
  const auto profile_it = std::find_if(
      profiles.begin(),
      profiles.end(),
      [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
  return profile_it == profiles.end() ? nullptr : &(*profile_it);
}

const SketchProfileRegion* find_equivalent_profile(
    const std::vector<SketchProfileRegion>& profiles,
    const std::string& source_profile_id) {
  if (const SketchProfileRegion* exact =
          find_profile_by_id(profiles, source_profile_id)) {
    return exact;
  }

  std::vector<const SketchProfileRegion*> candidates;
  for (const auto& profile : profiles) {
    if (profile.kind == "circle" && profile.source_circle_id.has_value() &&
        source_profile_id.find("profile-circle-" +
                               profile.source_circle_id.value()) == 0) {
      candidates.push_back(&profile);
      continue;
    }

    if (profile.kind == "polygon" && !profile.line_ids.empty()) {
      const bool same_entities = std::all_of(
          profile.line_ids.begin(),
          profile.line_ids.end(),
          [&](const std::string& entity_id) {
            return profile_id_mentions_entity(source_profile_id, entity_id);
          });
      if (same_entities) {
        candidates.push_back(&profile);
      }
    }
  }

  return candidates.size() == 1 ? candidates.front() : nullptr;
}

SweepFeatureParameters make_sweep_parameters(
    std::vector<FeatureEntry>& features,
    const std::string& profile_id,
    const std::string& path_entity_id);

void refresh_linked_extrudes(DocumentState& document,
                             const FeatureEntry& sketch_feature) {
  if (!sketch_feature.sketch_parameters.has_value()) {
    return;
  }
  const auto mark_source_profile_warning = [](FeatureEntry& feature) {
    feature.status = "warning";
    feature.parameters_summary = "Source profile unavailable";
    feature.dependency_broken = true;
    feature.dependency_warning =
        "Extrude source profile no longer exists. Edit the source sketch "
        "or recreate the extrude.";
  };

  for (auto& feature : document.feature_history) {
    if (feature.kind != "extrude" || !feature.extrude_parameters.has_value() ||
        feature.extrude_parameters->sketch_feature_id != sketch_feature.id) {
      continue;
    }

    const auto& sketch = sketch_feature.sketch_parameters.value();
    const std::vector<std::string> source_profile_ids =
        feature.extrude_parameters->profile_ids.empty()
            ? std::vector<std::string>{feature.extrude_parameters->profile_id}
            : feature.extrude_parameters->profile_ids;
    std::vector<SketchProfileRegion> source_profiles;
    for (const auto& profile_id : source_profile_ids) {
      const SketchProfileRegion* profile =
          find_equivalent_profile(sketch.profiles, profile_id);
      if (profile == nullptr) {
        mark_source_profile_warning(feature);
        source_profiles.clear();
        break;
      }
      source_profiles.push_back(*profile);
    }
    if (source_profiles.empty()) {
      continue;
    }

    const double depth = feature.extrude_parameters->depth;
    const std::string mode = feature.extrude_parameters->mode;
    const auto target_body_id = feature.extrude_parameters->target_body_id;
    std::optional<ExtrudeFeatureParameters> next_parameters;
    if (source_profiles.size() == 1 && source_profiles.front().kind == "circle") {
      next_parameters =
          make_extrude_parameters_for_profile(sketch_feature,
                                              source_profiles.front(),
                                              depth);
    } else {
      const auto& first = source_profiles.front();
      const std::string plane_id =
          sketch.plane_frame.has_value() ? plane_id_from_frame(sketch.plane_frame.value())
                                         : sketch.plane_id;
      const std::optional<PlaneFrame> plane_frame =
          sketch.plane_frame.has_value()
              ? std::optional<PlaneFrame>(make_plane_frame(sketch.plane_frame.value()))
              : std::nullopt;
      std::vector<std::string> ids;
      for (const auto& profile : source_profiles) {
        ids.push_back(profile.id);
      }
      next_parameters = ExtrudeFeatureParameters{
          .sketch_feature_id = sketch_feature.id,
          .profile_id = ids.front(),
          .profile_ids = ids,
          .plane_id = plane_id,
          .plane_frame = plane_frame,
          .profile_kind = "polygon",
          .start_x = 0.0,
          .start_y = 0.0,
          .width = 0.0,
          .height = 0.0,
          .radius = 0.0,
          .profile_points = first.kind == "circle"
                                ? sample_circle_profile_points(first)
                                : first.points,
          .inner_loops = first.inner_loops,
          .depth = depth,
      };
      for (size_t index = 1; index < source_profiles.size(); ++index) {
        const auto& profile = source_profiles[index];
        next_parameters->additional_profile_points.push_back(
            profile.kind == "circle" ? sample_circle_profile_points(profile)
                                     : profile.points);
        next_parameters->additional_inner_loops.push_back(profile.inner_loops);
      }
    }
    if (!next_parameters.has_value()) {
      feature.status = "warning";
      feature.parameters_summary = "Source profile unsupported";
      feature.dependency_broken = true;
      feature.dependency_warning =
          "Extrude source profile is no longer supported after the sketch edit.";
      continue;
    }

    feature.extrude_parameters = next_parameters.value();
    feature.extrude_parameters->mode = mode;
    feature.extrude_parameters->target_body_id = target_body_id;
    feature.status = "healthy";
    feature.dependency_broken = false;
    feature.dependency_warning.clear();
    feature.parameters_summary =
        feature.extrude_parameters->profile_id + " · " +
        std::to_string(feature.extrude_parameters->depth) + " mm";
  }

  const auto mark_loft_profile_warning = [](FeatureEntry& feature,
                                            const std::string& message) {
    feature.status = "warning";
    feature.parameters_summary = "Source profile unavailable";
    feature.dependency_broken = true;
    feature.dependency_warning = message;
  };

  for (auto& feature : document.feature_history) {
    if (feature.kind != "loft" || !feature.loft_parameters.has_value()) {
      continue;
    }

    bool references_sketch = false;
    for (const auto& section : feature.loft_parameters->sections) {
      if (section.sketch_feature_id == sketch_feature.id) {
        references_sketch = true;
        break;
      }
    }
    if (!references_sketch) {
      continue;
    }

    const auto& sketch = sketch_feature.sketch_parameters.value();
    LoftFeatureParameters next_parameters = feature.loft_parameters.value();
    bool failed = false;
    for (auto& section : next_parameters.sections) {
      if (section.sketch_feature_id != sketch_feature.id) {
        continue;
      }
      const SketchProfileRegion* profile =
          find_equivalent_profile(sketch.profiles, section.profile_id);
      if (profile == nullptr) {
        mark_loft_profile_warning(
            feature,
            "Loft source profile no longer exists. Edit the source sketch "
            "or recreate the loft.");
        failed = true;
        break;
      }
      const auto next_section =
          make_loft_section_for_profile(sketch_feature, *profile);
      if (!next_section.has_value()) {
        mark_loft_profile_warning(
            feature,
            "Loft source profile now contains unsupported holes. Edit the "
            "source sketch or recreate the loft.");
        failed = true;
        break;
      }
      section = next_section.value();
    }
    if (failed) {
      continue;
    }

    try {
      const TopoDS_Shape next_shape = build_loft_shape(next_parameters);
      if (next_shape.IsNull()) {
        throw std::runtime_error("Loft rebuild produced an empty shape");
      }
    } catch (const std::exception&) {
      mark_loft_profile_warning(
          feature,
          "Loft could not rebuild from the updated profile shape. Edit the "
          "source sketch or recreate the loft.");
      continue;
    }

    feature.loft_parameters = next_parameters;
    feature.status = "healthy";
    feature.dependency_broken = false;
    feature.dependency_warning.clear();
    feature.parameters_summary =
        std::to_string(feature.loft_parameters->sections.size()) +
        " sections" + (feature.loft_parameters->ruled ? " · ruled" : "");
  }

  const auto mark_revolve_warning = [](FeatureEntry& feature,
                                       const std::string& message) {
    feature.status = "warning";
    feature.parameters_summary = "Source profile or axis unavailable";
    feature.dependency_broken = true;
    feature.dependency_warning = message;
  };

  for (auto& feature : document.feature_history) {
    if (feature.kind != "revolve" ||
        !feature.revolve_parameters.has_value()) {
      continue;
    }
    const bool references_profile_sketch =
        feature.revolve_parameters->sketch_feature_id == sketch_feature.id;
    const bool references_axis_sketch =
        feature.revolve_parameters->axis_sketch_feature_id == sketch_feature.id;
    if (!references_profile_sketch && !references_axis_sketch) {
      continue;
    }

    RevolveFeatureParameters next_parameters =
        feature.revolve_parameters.value();
    const FeatureEntry* profile_sketch_feature = nullptr;
    const FeatureEntry* axis_sketch_feature = nullptr;
    const SketchProfileRegion* profile = nullptr;
    const SketchLine* axis_line = nullptr;

    for (const auto& candidate : document.feature_history) {
      if (candidate.id == next_parameters.sketch_feature_id) {
        profile_sketch_feature = &candidate;
      }
      if (candidate.id == next_parameters.axis_sketch_feature_id) {
        axis_sketch_feature = &candidate;
      }
    }
    if (profile_sketch_feature == nullptr ||
        !profile_sketch_feature->sketch_parameters.has_value()) {
      mark_revolve_warning(
          feature,
          "Revolve source profile sketch no longer exists. Edit the source "
          "sketch or recreate the revolve.");
      continue;
    }
    if (axis_sketch_feature == nullptr ||
        !axis_sketch_feature->sketch_parameters.has_value()) {
      mark_revolve_warning(
          feature,
          "Revolve axis sketch no longer exists. Edit the source sketch "
          "or recreate the revolve.");
      continue;
    }

    profile = find_equivalent_profile(
        profile_sketch_feature->sketch_parameters->profiles,
        next_parameters.profile_id);
    axis_line = find_line_by_id(axis_sketch_feature->sketch_parameters.value(),
                                next_parameters.axis_entity_id);
    if (profile == nullptr) {
      mark_revolve_warning(
          feature,
          "Revolve source profile no longer exists. Edit the source sketch "
          "or recreate the revolve.");
      continue;
    }
    if (axis_line == nullptr) {
      mark_revolve_warning(
          feature,
          "Revolve axis line no longer exists. Edit the source sketch "
          "or recreate the revolve.");
      continue;
    }

    const auto rebuilt =
        make_revolve_parameters_for_profile(*profile_sketch_feature,
                                            *profile,
                                            *axis_sketch_feature,
                                            *axis_line,
                                            next_parameters.angle_degrees);
    if (!rebuilt.has_value()) {
      mark_revolve_warning(
          feature,
          "Revolve source profile or axis is no longer supported after "
          "the sketch edit.");
      continue;
    }

    try {
      const TopoDS_Shape next_shape = build_revolve_shape(rebuilt.value());
      if (next_shape.IsNull()) {
        throw std::runtime_error("Revolve rebuild produced an empty shape");
      }
    } catch (const std::exception&) {
      mark_revolve_warning(
          feature,
          "Revolve could not rebuild from the updated profile or axis. "
          "Edit the source sketch or recreate the revolve.");
      continue;
    }

    feature.revolve_parameters = rebuilt.value();
    feature.status = "healthy";
    feature.dependency_broken = false;
    feature.dependency_warning.clear();
    feature.parameters_summary =
        feature.revolve_parameters->profile_id + " · " +
        std::to_string(feature.revolve_parameters->angle_degrees) + " deg";
  }

  const auto mark_sweep_warning = [](FeatureEntry& feature,
                                     const std::string& message) {
    feature.status = "warning";
    feature.parameters_summary = "Source profile or path unavailable";
    feature.dependency_broken = true;
    feature.dependency_warning = message;
  };

  for (auto& feature : document.feature_history) {
    if (feature.kind != "sweep" || !feature.sweep_parameters.has_value()) {
      continue;
    }
    const bool references_profile_sketch =
        feature.sweep_parameters->sketch_feature_id == sketch_feature.id;
    const bool references_path_sketch =
        feature.sweep_parameters->path_sketch_feature_id == sketch_feature.id;
    if (!references_profile_sketch && !references_path_sketch) {
      continue;
    }

    try {
      const SweepFeatureParameters rebuilt =
          make_sweep_parameters(document.feature_history,
                                feature.sweep_parameters->profile_id,
                                feature.sweep_parameters->path_entity_id);
      const TopoDS_Shape next_shape = build_sweep_shape(rebuilt);
      if (next_shape.IsNull()) {
        throw std::runtime_error("Sweep rebuild produced an empty shape");
      }
      feature.sweep_parameters = rebuilt;
      feature.status = "healthy";
      feature.dependency_broken = false;
      feature.dependency_warning.clear();
      feature.parameters_summary = "Profile · path";
    } catch (const std::exception&) {
      mark_sweep_warning(
          feature,
          "Sweep could not rebuild from the updated profile or path. Edit the "
          "source sketch or recreate the sweep.");
    }
  }
}

bool id_in_set(const std::unordered_set<std::string>& ids,
               const std::string& id) {
  return ids.find(id) != ids.end();
}

void add_id(std::unordered_set<std::string>& ids, const std::string& id) {
  if (!id.empty()) {
    ids.insert(id);
  }
}

void add_ids(std::unordered_set<std::string>& ids,
             const std::vector<std::string>& source) {
  for (const auto& id : source) {
    add_id(ids, id);
  }
}

void remove_ids_from_vector(std::vector<std::string>& ids,
                            const std::unordered_set<std::string>& deleted_ids) {
  ids.erase(std::remove_if(ids.begin(),
                           ids.end(),
                           [&](const std::string& id) {
                             return id_in_set(deleted_ids, id);
                           }),
            ids.end());
}

bool projection_has_generated_entities(const SketchProjection& projection) {
  return !projection.generated_line_ids.empty() ||
         !projection.generated_circle_ids.empty() ||
         !projection.generated_arc_ids.empty() ||
         !projection.generated_point_id.empty();
}

bool projection_generates_entity(const SketchProjection& projection,
                                 const std::string& entity_id) {
  return std::find(projection.generated_line_ids.begin(),
                   projection.generated_line_ids.end(),
                   entity_id) != projection.generated_line_ids.end() ||
         std::find(projection.generated_circle_ids.begin(),
                   projection.generated_circle_ids.end(),
                   entity_id) != projection.generated_circle_ids.end() ||
         std::find(projection.generated_arc_ids.begin(),
                   projection.generated_arc_ids.end(),
                   entity_id) != projection.generated_arc_ids.end();
}

bool compiled_body_exists(const DocumentState& document,
                          const std::string& body_id) {
  const CompiledBodies compiled = compile_bodies(document);
  return std::any_of(compiled.bodies.begin(),
                     compiled.bodies.end(),
                     [&](const CompiledBody& body) {
                       return body.id == body_id;
                     });
}

std::optional<TopoDS_Face> resolve_face_for_appearance(
    const DocumentState& document,
    const std::string& face_id) {
  const auto separator = face_id.find(":face:");
  if (separator == std::string::npos) {
    return std::nullopt;
  }
  const std::string owner_id = face_id.substr(0, separator);
  int index = -1;
  try {
    index = std::stoi(face_id.substr(separator + 6));
  } catch (const std::exception&) {
    return std::nullopt;
  }
  if (index < 0) {
    return std::nullopt;
  }

  const CompiledBodies compiled = compile_bodies(document);
  const auto body_it =
      std::find_if(compiled.bodies.begin(),
                   compiled.bodies.end(),
                   [&](const CompiledBody& body) { return body.id == owner_id; });
  if (body_it == compiled.bodies.end() || body_it->shape.IsNull()) {
    return std::nullopt;
  }

  TopTools_IndexedMapOfShape face_map;
  TopExp::MapShapes(body_it->shape, TopAbs_FACE, face_map);
  const int one_based = index + 1;
  if (one_based < 1 || one_based > face_map.Extent()) {
    return std::nullopt;
  }
  const TopoDS_Face face = TopoDS::Face(face_map(one_based));
  if (face.IsNull()) {
    return std::nullopt;
  }
  return face;
}

}  // namespace

void DocumentManager::require_document() const {
  if (!document_.has_value()) {
    throw std::runtime_error("No active document");
  }
}

void DocumentManager::push_undo_state() {
  require_document();
  undo_stack_.push_back(document_.value());
}

void DocumentManager::bump_geometry_revision() {
  require_document();
  refresh_history_dependencies(*document_);
  normalize_timeline_cursor(*document_);
  // Increment the revision via a local before assigning back so the
  // literal `document_->revision += 1;` doesn't appear here — that
  // way mass-renaming call sites won't accidentally rewrite this
  // implementation.
  const int next = document_->revision + 1;
  document_->revision = next;
}

void DocumentManager::clear_redo_stack() {
  redo_stack_.clear();
}

FeatureEntry DocumentManager::make_root_feature() {
  return FeatureEntry{
      .id = "feature-" + std::to_string(next_feature_id_++),
      .kind = "root_part",
      .name = "Base Part",
      .status = "healthy",
      .parameters_summary = "Document root",
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = std::nullopt,
      .sketch_parameters = std::nullopt,
  };
}

DocumentState DocumentManager::create_document() {
  DocumentState document{
      .id = "doc-" + std::to_string(next_document_id_++),
      .name = "Untitled Part",
      .units = "mm",
      .revision = 1,
      .selected_feature_id = std::nullopt,
      .selected_reference_id = std::nullopt,
      .selected_face_id = std::nullopt,
      .selected_edge_ids = {},
      .selected_vertex_ids = {},
      .active_sketch_plane_id = std::nullopt,
      .active_sketch_face_id = std::nullopt,
      .active_sketch_feature_id = std::nullopt,
      .active_sketch_tool = std::nullopt,
      .selected_sketch_point_id = std::nullopt,
      .selected_sketch_entity_id = std::nullopt,
      .selected_sketch_dimension_id = std::nullopt,
      .selected_sketch_profile_id = std::nullopt,
      .selected_sketch_profile_ids = {},
      .timeline_cursor = std::nullopt,
      .feature_history = {make_root_feature()},
      .parameters = {},
      .appearance = {},
      .selection_filter = SelectionFilter{},
  };

  document_ = document;
  document_file_path_ = std::nullopt;
  document_count_ = 1;
  undo_stack_.clear();
  redo_stack_.clear();
  return document;
}

DocumentState DocumentManager::add_box_feature(
    const BoxFeatureParameters& parameters) {
  require_document();
  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(
      create_box_feature(next_feature_id_++, parameters));
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_cylinder_feature(
    const CylinderFeatureParameters& parameters) {
  require_document();
  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(
      create_cylinder_feature(next_feature_id_++, parameters));
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_box_feature(
    const std::string& feature_id, const BoxFeatureParameters& parameters) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_box_feature(*feature_it, parameters);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_cylinder_feature(
    const std::string& feature_id,
    const CylinderFeatureParameters& parameters) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_cylinder_feature(*feature_it, parameters);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_extrude_depth(
    const std::string& feature_id, double depth) {
  require_document();
  if (depth == 0.0) {
    return document_.value();
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_extrude_depth(*feature_it, depth);
  if (feature_it->extrude_parameters.has_value()) {
    feature_it->extrude_parameters->extent_mode = "one_side";
    feature_it->extrude_parameters->side1.extent_type = "distance";
    feature_it->extrude_parameters->side1.distance = std::abs(depth);
    feature_it->extrude_parameters->side2 = std::nullopt;
    normalize_extrude_parameters(*document_, feature_it->extrude_parameters.value());
    apply_extrude_parameters_with_preview_validation(
        *feature_it, feature_it->extrude_parameters.value());
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_extrude_mode(
    const std::string& feature_id, const std::string& mode) {
  require_document();

  if (mode != "new_body" && mode != "join" && mode != "cut" &&
      mode != "intersect") {
    throw std::runtime_error("Unsupported extrude mode: " + mode);
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  if (feature_it->kind != "extrude" ||
      !feature_it->extrude_parameters.has_value()) {
    throw std::runtime_error(
        "update_extrude_mode requires an extrude feature: " + feature_id);
  }

  push_undo_state();
  clear_redo_stack();
  feature_it->extrude_parameters->mode = mode;
  feature_it->extrude_parameters->operation = mode;
  normalize_extrude_operation_mode(feature_it->extrude_parameters.value());
  normalize_extrude_parameters(*document_, feature_it->extrude_parameters.value());
  apply_extrude_parameters_with_preview_validation(
      *feature_it, feature_it->extrude_parameters.value());
  if (feature_it->name == "Extrude" || feature_it->name == "Body") {
    feature_it->name = mode == "new_body" ? "Body" : "Extrude";
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_extrude_target_body(
    const std::string& feature_id,
    const std::optional<std::string>& target_body_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  if (feature_it->kind != "extrude" ||
      !feature_it->extrude_parameters.has_value()) {
    throw std::runtime_error(
        "update_extrude_target_body requires an extrude feature: " +
        feature_id);
  }

  // Target ids that point at the extrude itself (or at a feature that
  // does not exist) are silently coerced to nullopt so the body compiler
  // simply falls back to "most recent body".
  std::optional<std::string> resolved = target_body_id;
  if (resolved.has_value()) {
    if (resolved.value() == feature_id) {
      resolved = std::nullopt;
    } else {
      bool exists = false;
      for (const auto& other : document_->feature_history) {
        if (other.id == resolved.value()) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        resolved = std::nullopt;
      }
    }
  }

  push_undo_state();
  clear_redo_stack();
  feature_it->extrude_parameters->target_body_id = resolved;
  normalize_extrude_operation_mode(feature_it->extrude_parameters.value());
  normalize_extrude_parameters(*document_, feature_it->extrude_parameters.value());
  apply_extrude_parameters_with_preview_validation(
      *feature_it, feature_it->extrude_parameters.value());
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_extrude_parameters(
    const std::string& feature_id,
    const ExtrudeFeatureParameters& parameters) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "extrude" ||
      !feature_it->extrude_parameters.has_value()) {
    throw std::runtime_error(
        "update_extrude_parameters requires an extrude feature: " + feature_id);
  }

  ExtrudeFeatureParameters next = parameters;
  preserve_extrude_source_geometry(next, feature_it->extrude_parameters.value());
  if (next.depth == 0.0 || next.side1.distance == 0.0 ||
      (next.side2.has_value() && next.side2->distance == 0.0)) {
    return document_.value();
  }
  normalize_extrude_operation_mode(next);
  normalize_extrude_parameters(*document_, next);

  push_undo_state();
  clear_redo_stack();
  apply_extrude_parameters_with_preview_validation(*feature_it, next);
  if (feature_it->name == "Extrude" || feature_it->name == "Body") {
    feature_it->name = next.mode == "new_body" ? "Body" : "Extrude";
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::rename_feature(const std::string& feature_id,
                                              const std::string& name) {
  require_document();

  if (name.empty()) {
    throw std::runtime_error("Feature name cannot be empty");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  push_undo_state();
  clear_redo_stack();
  feature_it->name = name;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_feature_suppressed(
    const std::string& feature_id, bool suppressed) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  if (feature_it->kind == "root_part") {
    throw std::runtime_error("The root feature cannot be suppressed");
  }

  // No-op early exit so we don't pollute undo with a redundant entry
  // when the UI's "Suppress" button is double-clicked.
  if (feature_it->suppressed == suppressed) {
    return document_.value();
  }

  push_undo_state();
  clear_redo_stack();
  feature_it->suppressed = suppressed;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::delete_feature(const std::string& feature_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  if (feature_it->kind == "root_part") {
    throw std::runtime_error("The root feature cannot be deleted");
  }

  if (feature_it->kind == "sketch" &&
      document_->active_sketch_feature_id.has_value() &&
      document_->active_sketch_feature_id.value() == feature_id) {
    throw std::runtime_error("Finish the active sketch before deleting it");
  }

  push_undo_state();
  clear_redo_stack();

  const bool deleted_selected =
      document_->selected_feature_id.has_value() &&
      document_->selected_feature_id.value() == feature_id;

  document_->feature_history.erase(feature_it);
  if (deleted_selected) {
    document_->selected_feature_id = std::nullopt;
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::undo() {
  require_document();

  if (undo_stack_.empty()) {
    throw std::runtime_error("Nothing to undo");
  }

  redo_stack_.push_back(document_.value());
  document_ = undo_stack_.back();
  undo_stack_.pop_back();
  return document_.value();
}

DocumentState DocumentManager::redo() {
  require_document();

  if (redo_stack_.empty()) {
    throw std::runtime_error("Nothing to redo");
  }

  undo_stack_.push_back(document_.value());
  document_ = redo_stack_.back();
  redo_stack_.pop_back();
  return document_.value();
}

DocumentState DocumentManager::set_timeline_cursor(int included_action_count) {
  require_document();

  const int max_actions = action_count(document_.value());
  const int clamped =
      std::max(0, std::min(included_action_count, max_actions));

  if (clamped >= max_actions) {
    document_->timeline_cursor = std::nullopt;
  } else {
    document_->timeline_cursor = clamped;
  }

  if (document_->selected_feature_id.has_value() &&
      !includes_feature_at_cursor(document_.value(),
                                  document_->selected_feature_id.value())) {
    document_->selected_feature_id = std::nullopt;
  }
  if (document_->selected_reference_id.has_value() &&
      !is_origin_plane_reference(document_->selected_reference_id.value()) &&
      !includes_feature_at_cursor(document_.value(),
                                  document_->selected_reference_id.value())) {
    document_->selected_reference_id = std::nullopt;
  }
  if (document_->active_sketch_feature_id.has_value() &&
      !includes_feature_at_cursor(document_.value(),
                                  document_->active_sketch_feature_id.value())) {
    document_->active_sketch_feature_id = std::nullopt;
    document_->active_sketch_plane_id = std::nullopt;
    document_->active_sketch_face_id = std::nullopt;
    document_->active_sketch_tool = std::nullopt;
  }
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->selected_sketch_profile_ids.clear();

  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::select_feature(const std::string& feature_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

DocumentState DocumentManager::select_reference(const std::string& reference_id) {
  require_document();

  if (!is_selectable_plane_reference(*document_, reference_id)) {
    throw std::runtime_error("Reference not found: " + reference_id);
  }

  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = reference_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

DocumentState DocumentManager::start_sketch_on_plane(
    const std::string& reference_id) {
  require_document();

  if (!is_selectable_plane_reference(*document_, reference_id)) {
    throw std::runtime_error("Sketch plane not found: " + reference_id);
  }

  push_undo_state();
  clear_redo_stack();

  // Origin reference planes have hardcoded mappings throughout the
  // existing code paths, so we omit `plane_frame` and let the
  // legacy fallbacks take over (matching the historical behaviour).
  // Construction-plane sketches must carry a `plane_frame` because
  // there are no hardcoded mappings for arbitrary frames; we read
  // the cached frame off the source feature.
  std::optional<SketchFeatureParameters::SketchPlaneFrame> sketch_frame;
  if (!is_origin_plane_reference(reference_id)) {
    const auto frame = resolve_plane_source_frame(*document_, reference_id);
    if (!frame.has_value()) {
      throw std::runtime_error(
          "Sketch plane source could not be resolved: " + reference_id);
    }
    sketch_frame = to_sketch_plane_frame(frame.value());
  }

  document_->feature_history.push_back(create_sketch_feature(
      next_feature_id_++, reference_id, sketch_frame));
  const std::string sketch_feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = reference_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->active_sketch_plane_id = reference_id;
  document_->active_sketch_face_id = std::nullopt;
  document_->active_sketch_feature_id = sketch_feature_id;
  document_->active_sketch_tool = "select";
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::select_face(const std::string& face_id) {
  require_document();

  const std::string owner_id = face_owner_id(face_id);
  if (owner_id.empty()) {
    throw std::runtime_error("Face not found: " + face_id);
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Face owner not found: " + face_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = face_id;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

DocumentState DocumentManager::select_edge(const std::string& edge_id,
                                            bool additive) {
  require_document();

  // Edge ids are minted by the viewport as "<owner_body_id>:edge:<index>".
  // We don't validate the index — the body's edge enumeration may shift
  // by topology so a stale id from the UI just becomes a no-op highlight.
  // The owner body id, however, must point at a real feature so we can
  // also bring the body into focus (selected_feature_id) for the
  // hierarchy panel and downstream actions like fillet/chamfer.
  const auto separator = edge_id.find(":edge:");
  if (separator == std::string::npos || separator == 0) {
    throw std::runtime_error("Malformed edge id: " + edge_id);
  }
  const std::string owner_id = edge_id.substr(0, separator);

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Edge owner not found: " + edge_id);
  }

  // Multi-select semantics: shift-click toggles, plain click replaces.
  // Toggling preserves the rest of the selection so users can build up
  // a multi-edge set incrementally for fillet / chamfer. Adding an
  // edge from a different body is allowed at the storage layer; the
  // fillet / chamfer creators are the ones that reject mixed-body
  // selections, since OCCT expects a single target body per feature.
  if (additive) {
    const auto existing = std::find(document_->selected_edge_ids.begin(),
                                    document_->selected_edge_ids.end(),
                                    edge_id);
    if (existing != document_->selected_edge_ids.end()) {
      document_->selected_edge_ids.erase(existing);
    } else {
      document_->selected_edge_ids.push_back(edge_id);
    }
  } else {
    document_->selected_edge_ids = {edge_id};
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

DocumentState DocumentManager::select_vertex(const std::string& vertex_id,
                                              bool additive) {
  require_document();

  // Vertex ids are minted by the viewport as
  // "<owner_body_id>:vertex:<index>". Same lenience as select_edge:
  // we don't validate the index since topology can shift, but the
  // owner body id must point at a real feature so the hierarchy
  // panel highlights the correct body and downstream actions can
  // resolve the body via selected_feature_id.
  const auto separator = vertex_id.find(":vertex:");
  if (separator == std::string::npos || separator == 0) {
    throw std::runtime_error("Malformed vertex id: " + vertex_id);
  }
  const std::string owner_id = vertex_id.substr(0, separator);

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Vertex owner not found: " + vertex_id);
  }

  // Toggle / replace mirror of select_edge: shift-click adds or
  // removes; plain click replaces. Two-vertex selections drive the
  // distance readout in the UI's Selection panel.
  if (additive) {
    const auto existing = std::find(document_->selected_vertex_ids.begin(),
                                    document_->selected_vertex_ids.end(),
                                    vertex_id);
    if (existing != document_->selected_vertex_ids.end()) {
      document_->selected_vertex_ids.erase(existing);
    } else {
      document_->selected_vertex_ids.push_back(vertex_id);
    }
  } else {
    document_->selected_vertex_ids = {vertex_id};
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

namespace {

std::string edge_owner_id(const std::string& edge_id) {
  const auto separator = edge_id.find(":edge:");
  if (separator == std::string::npos) {
    return "";
  }
  return edge_id.substr(0, separator);
}

}  // namespace

DocumentState DocumentManager::create_fillet(
    const std::vector<std::string>& edge_ids, double radius) {
  require_document();

  if (radius <= 0.0) {
    throw std::runtime_error("Fillet radius must be greater than zero");
  }
  if (edge_ids.empty()) {
    throw std::runtime_error("Fillet requires at least one edge");
  }

  // Validate every edge is well-formed and that all share the same
  // owner body — OCCT's fillet builder operates on a single shape, so
  // mixing edges from multiple bodies has no well-defined semantics.
  // We surface this as an error rather than silently slicing the
  // selection.
  const std::string owner_id = edge_owner_id(edge_ids.front());
  if (owner_id.empty()) {
    throw std::runtime_error("Malformed edge id: " + edge_ids.front());
  }
  for (const auto& edge_id : edge_ids) {
    const std::string id_owner = edge_owner_id(edge_id);
    if (id_owner.empty()) {
      throw std::runtime_error("Malformed edge id: " + edge_id);
    }
    if (id_owner != owner_id) {
      throw std::runtime_error(
          "Fillet edges must all belong to the same body");
    }
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Edge owner not found: " + owner_id);
  }

  push_undo_state();
  clear_redo_stack();

  FilletFeatureParameters params{};
  params.target_body_id = owner_id;
  params.edge_ids = edge_ids;
  params.radius = radius;
  // Born pending so body_compiler retains the pre-fillet shape for
  // edge picking until the UI calls `confirm_fillet`. See
  // FilletFeatureParameters::is_pending.
  params.is_pending = true;

  std::ostringstream summary;
  summary << edge_ids.size() << " edge"
          << (edge_ids.size() == 1 ? "" : "s") << " · " << radius
          << " mm";

  FeatureEntry feature{
      .id = "feature-" + std::to_string(next_feature_id_++),
      .kind = "fillet",
      .name = "Fillet",
      .status = "healthy",
      .parameters_summary = summary.str(),
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = std::nullopt,
      .sketch_parameters = std::nullopt,
      .fillet_parameters = params,
      .chamfer_parameters = std::nullopt,
  };
  document_->feature_history.push_back(std::move(feature));
  document_->selected_feature_id = document_->feature_history.back().id;
  // Keep the picked edges highlighted while the floating panel is
  // open: the body_compiler highlights edges that are in
  // selected_edge_ids, and the panel's "edit edges" interaction
  // (update_fillet_edges) keeps this set in sync. The UI's Confirm
  // path is responsible for clearing the selection once the user is
  // done; Cancel goes through undo() which restores the prior set.
  document_->selected_edge_ids = edge_ids;
  document_->selected_vertex_ids.clear();
  document_->selected_face_id = std::nullopt;
  document_->selected_reference_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_fillet_edges(
    const std::string& feature_id,
    const std::vector<std::string>& edge_ids) {
  require_document();

  if (edge_ids.empty()) {
    throw std::runtime_error("Fillet must keep at least one edge");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "fillet" ||
      !feature_it->fillet_parameters.has_value()) {
    throw std::runtime_error(
        "update_fillet_edges requires a fillet feature: " + feature_id);
  }

  // Every new edge must belong to the feature's existing target body
  // — see create_fillet for why mixed-body sets aren't representable.
  // We compare against the stored target_body_id rather than parsing
  // the first edge's owner so the feature's body identity stays
  // authoritative across edge swaps.
  const std::string& target = feature_it->fillet_parameters->target_body_id;
  for (const auto& edge_id : edge_ids) {
    const auto separator = edge_id.find(":edge:");
    if (separator == std::string::npos || separator == 0) {
      throw std::runtime_error("Malformed edge id: " + edge_id);
    }
    if (edge_id.substr(0, separator) != target) {
      throw std::runtime_error(
          "Fillet edges must all belong to the same body");
    }
  }

  // No push_undo_state here: this is a live-preview update on the
  // freshly-created fillet whose own create_fillet already pushed an
  // undo step. Pushing here would make a panel session of N edge
  // toggles and M radius edits collapse to a single user-visible
  // feature but produce N+M+1 undo steps, so Cancel → undo() would
  // only revert the last edit instead of the whole session. The
  // create_* push covers the entire session.
  feature_it->fillet_parameters->edge_ids = edge_ids;
  std::ostringstream summary;
  summary << edge_ids.size() << " edge"
          << (edge_ids.size() == 1 ? "" : "s") << " · "
          << feature_it->fillet_parameters->radius << " mm";
  feature_it->parameters_summary = summary.str();
  // Keep the highlight in sync with the live edge set so the user
  // sees what's being filleted while picking.
  document_->selected_edge_ids = edge_ids;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::confirm_fillet(const std::string& feature_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "fillet" ||
      !feature_it->fillet_parameters.has_value()) {
    throw std::runtime_error(
        "confirm_fillet requires a fillet feature: " + feature_id);
  }

  // Idempotent: confirming an already-confirmed fillet is a no-op so
  // the UI doesn't have to track whether it has called this before.
  if (!feature_it->fillet_parameters->is_pending) {
    return document_.value();
  }

  // No push_undo here — the matching create_fillet's undo step covers
  // the entire pending session, so a single undo() rolls back create
  // + all edits + this confirm in one step.
  feature_it->fillet_parameters->is_pending = false;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_fillet_radius(
    const std::string& feature_id, double radius) {
  require_document();

  if (radius <= 0.0) {
    throw std::runtime_error("Fillet radius must be greater than zero");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "fillet" || !feature_it->fillet_parameters.has_value()) {
    throw std::runtime_error(
        "update_fillet_radius requires a fillet feature: " + feature_id);
  }

  // See update_fillet_edges for why this command intentionally does
  // not push an undo step.
  feature_it->fillet_parameters->radius = radius;
  std::ostringstream summary;
  summary << feature_it->fillet_parameters->edge_ids.size() << " edge"
          << (feature_it->fillet_parameters->edge_ids.size() == 1 ? "" : "s")
          << " · " << radius << " mm";
  feature_it->parameters_summary = summary.str();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_chamfer(
    const std::vector<std::string>& edge_ids, double distance) {
  require_document();

  if (distance <= 0.0) {
    throw std::runtime_error("Chamfer distance must be greater than zero");
  }
  if (edge_ids.empty()) {
    throw std::runtime_error("Chamfer requires at least one edge");
  }

  // Same single-body validation as create_fillet — see comment there
  // for why mixed-body selections aren't representable.
  const std::string owner_id = edge_owner_id(edge_ids.front());
  if (owner_id.empty()) {
    throw std::runtime_error("Malformed edge id: " + edge_ids.front());
  }
  for (const auto& edge_id : edge_ids) {
    const std::string id_owner = edge_owner_id(edge_id);
    if (id_owner.empty()) {
      throw std::runtime_error("Malformed edge id: " + edge_id);
    }
    if (id_owner != owner_id) {
      throw std::runtime_error(
          "Chamfer edges must all belong to the same body");
    }
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Edge owner not found: " + owner_id);
  }

  push_undo_state();
  clear_redo_stack();

  ChamferFeatureParameters params{};
  params.target_body_id = owner_id;
  params.edge_ids = edge_ids;
  params.distance = distance;
  // Same rationale as create_fillet — see comment there.
  params.is_pending = true;

  std::ostringstream summary;
  summary << edge_ids.size() << " edge"
          << (edge_ids.size() == 1 ? "" : "s") << " · " << distance
          << " mm";

  FeatureEntry feature{
      .id = "feature-" + std::to_string(next_feature_id_++),
      .kind = "chamfer",
      .name = "Chamfer",
      .status = "healthy",
      .parameters_summary = summary.str(),
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = std::nullopt,
      .sketch_parameters = std::nullopt,
      .fillet_parameters = std::nullopt,
      .chamfer_parameters = params,
  };
  document_->feature_history.push_back(std::move(feature));
  document_->selected_feature_id = document_->feature_history.back().id;
  // Same rationale as create_fillet: keep the chamfered edges
  // highlighted while the floating panel is open. See comment there.
  document_->selected_edge_ids = edge_ids;
  document_->selected_vertex_ids.clear();
  document_->selected_face_id = std::nullopt;
  document_->selected_reference_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_chamfer_edges(
    const std::string& feature_id,
    const std::vector<std::string>& edge_ids) {
  require_document();

  if (edge_ids.empty()) {
    throw std::runtime_error("Chamfer must keep at least one edge");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "chamfer" ||
      !feature_it->chamfer_parameters.has_value()) {
    throw std::runtime_error(
        "update_chamfer_edges requires a chamfer feature: " + feature_id);
  }

  // See update_fillet_edges for the same-body invariant rationale.
  const std::string& target = feature_it->chamfer_parameters->target_body_id;
  for (const auto& edge_id : edge_ids) {
    const auto separator = edge_id.find(":edge:");
    if (separator == std::string::npos || separator == 0) {
      throw std::runtime_error("Malformed edge id: " + edge_id);
    }
    if (edge_id.substr(0, separator) != target) {
      throw std::runtime_error(
          "Chamfer edges must all belong to the same body");
    }
  }

  // See update_fillet_edges for why this command intentionally does
  // not push an undo step.
  feature_it->chamfer_parameters->edge_ids = edge_ids;
  std::ostringstream summary;
  summary << edge_ids.size() << " edge"
          << (edge_ids.size() == 1 ? "" : "s") << " · "
          << feature_it->chamfer_parameters->distance << " mm";
  feature_it->parameters_summary = summary.str();
  document_->selected_edge_ids = edge_ids;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_chamfer_distance(
    const std::string& feature_id, double distance) {
  require_document();

  if (distance <= 0.0) {
    throw std::runtime_error("Chamfer distance must be greater than zero");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "chamfer" ||
      !feature_it->chamfer_parameters.has_value()) {
    throw std::runtime_error(
        "update_chamfer_distance requires a chamfer feature: " + feature_id);
  }

  // See update_fillet_edges for why this command intentionally does
  // not push an undo step.
  feature_it->chamfer_parameters->distance = distance;
  std::ostringstream summary;
  summary << feature_it->chamfer_parameters->edge_ids.size() << " edge"
          << (feature_it->chamfer_parameters->edge_ids.size() == 1 ? "" : "s")
          << " · " << distance << " mm";
  feature_it->parameters_summary = summary.str();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::confirm_chamfer(const std::string& feature_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "chamfer" ||
      !feature_it->chamfer_parameters.has_value()) {
    throw std::runtime_error(
        "confirm_chamfer requires a chamfer feature: " + feature_id);
  }

  if (!feature_it->chamfer_parameters->is_pending) {
    return document_.value();
  }

  // See `confirm_fillet` for why this does not push an undo step.
  feature_it->chamfer_parameters->is_pending = false;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_shell(const std::string& face_id,
                                            double thickness) {
  require_document();

  if (thickness <= 0.0) {
    throw std::runtime_error("Shell thickness must be greater than zero");
  }

  const std::string owner_id = face_owner_id(face_id);
  if (owner_id.empty()) {
    throw std::runtime_error("Malformed face id: " + face_id);
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Shell face owner not found: " + owner_id);
  }

  push_undo_state();
  clear_redo_stack();

  ShellFeatureParameters params{};
  params.target_body_id = owner_id;
  params.removed_face_ids = {face_id};
  params.thickness = thickness;
  params.is_pending = true;

  std::ostringstream summary;
  summary << "1 face · " << thickness << " mm";

  FeatureEntry feature{
      .id = "feature-" + std::to_string(next_feature_id_++),
      .kind = "shell",
      .name = "Shell",
      .status = "healthy",
      .parameters_summary = summary.str(),
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = std::nullopt,
      .sketch_parameters = std::nullopt,
      .fillet_parameters = std::nullopt,
      .chamfer_parameters = std::nullopt,
      .shell_parameters = params,
  };
  document_->feature_history.push_back(std::move(feature));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_face_id = face_id;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_reference_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_shell_thickness(
    const std::string& feature_id,
    double thickness) {
  require_document();

  if (thickness <= 0.0) {
    throw std::runtime_error("Shell thickness must be greater than zero");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "shell" ||
      !feature_it->shell_parameters.has_value()) {
    throw std::runtime_error(
        "update_shell_thickness requires a shell feature: " + feature_id);
  }

  feature_it->shell_parameters->thickness = thickness;
  std::ostringstream summary;
  summary << feature_it->shell_parameters->removed_face_ids.size() << " face"
          << (feature_it->shell_parameters->removed_face_ids.size() == 1 ? ""
                                                                         : "s")
          << " · " << thickness << " mm";
  feature_it->parameters_summary = summary.str();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::confirm_shell(const std::string& feature_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "shell" ||
      !feature_it->shell_parameters.has_value()) {
    throw std::runtime_error(
        "confirm_shell requires a shell feature: " + feature_id);
  }

  if (!feature_it->shell_parameters->is_pending) {
    return document_.value();
  }

  feature_it->shell_parameters->is_pending = false;
  document_->selected_face_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::start_sketch_on_face(
    const std::string& face_id,
    const SketchFeatureParameters::SketchPlaneFrame& plane_frame) {
  require_document();

  const std::string owner_id = face_owner_id(face_id);
  if (owner_id.empty()) {
    throw std::runtime_error("Sketch face not found: " + face_id);
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch face owner not found: " + face_id);
  }

  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(
      create_sketch_feature(next_feature_id_++, face_id, plane_frame));
  const std::string sketch_feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = face_id;
  document_->active_sketch_plane_id = face_id;
  document_->active_sketch_face_id = face_id;
  document_->active_sketch_feature_id = sketch_feature_id;
  document_->active_sketch_tool = "select";
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_tool(const std::string& tool) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  if (!is_supported_sketch_tool(tool)) {
    throw std::runtime_error("Unsupported sketch tool: " + tool);
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  polysmith::core::set_sketch_tool(*feature_it, tool);
  document_->active_sketch_tool = tool;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_line(const std::string& line_id,
                                                  double start_x,
                                                  double start_y,
                                                  double end_x,
                                                  double end_y) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_sketch_line(
      *feature_it, line_id, start_x, start_y, end_x, end_y);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = line_id;
  document_->selected_sketch_dimension_id = "dim-line-" + line_id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_point(const std::string& point_id,
                                                   double x,
                                                   double y) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_sketch_point(*feature_it, point_id, x, y);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = point_id;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_line_constraint(
    const std::string& line_id,
    const std::optional<std::string>& constraint) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_line_constraint(*feature_it, line_id, constraint);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::clear_sketch_line_constraints(
    const std::string& line_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::clear_sketch_line_constraints(*feature_it, line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_line_construction(
    const std::string& line_id,
    bool is_construction) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_line_construction(
      *feature_it, line_id, is_construction);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_midpoint_anchor(
    const std::string& point_id,
    const std::string& host_line_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_midpoint_anchor(
      *feature_it, point_id, host_line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_point_line_anchor(
    const std::string& point_id,
    const std::string& host_line_id,
    double t) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_point_line_anchor(
      *feature_it, point_id, host_line_id, t);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_equal_length_constraint(
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_equal_length_constraint(
      *feature_it, line_id, other_line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_perpendicular_constraint(
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_perpendicular_constraint(
      *feature_it, line_id, other_line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

// Tiny helper used by all five mirror_preview wrappers — they
// all need to find the active sketch feature and bail out if
// there isn't one. Returns a non-const iterator into
// `feature_history` so the caller can mutate it.
namespace {
std::vector<FeatureEntry>::iterator require_active_sketch(
    DocumentState& document) {
  if (!document.active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }
  const auto& sketch_id = *document.active_sketch_feature_id;
  const auto feature_it = std::find_if(
      document.feature_history.begin(),
      document.feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == sketch_id; });
  if (feature_it == document.feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }
  return feature_it;
}
}  // namespace

DocumentState DocumentManager::start_mirror_preview() {
  require_document();
  const auto feature_it = require_active_sketch(*document_);
  // No undo push — preview state is transient and we don't want
  // it to clutter the undo history.
  polysmith::core::start_mirror_preview(*feature_it);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_mirror_preview_axis(
    const std::string& axis_line_id) {
  require_document();
  const auto feature_it = require_active_sketch(*document_);
  polysmith::core::update_mirror_preview_axis(*feature_it, axis_line_id);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_mirror_preview_objects(
    const std::vector<std::string>& object_ids) {
  require_document();
  const auto feature_it = require_active_sketch(*document_);
  polysmith::core::update_mirror_preview_objects(*feature_it, object_ids);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::commit_mirror_preview() {
  require_document();
  const auto feature_it = require_active_sketch(*document_);
  // Commit is the only mirror_preview op that actually changes
  // permanent geometry, so it's the one that pushes an undo
  // state. Cancel/start/update don't write anything that needs
  // rolling back beyond the in-memory pending struct.
  push_undo_state();
  clear_redo_stack();
  polysmith::core::commit_mirror_preview(
      *feature_it, next_sketch_line_id_, next_sketch_circle_id_);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::cancel_mirror_preview() {
  require_document();
  const auto feature_it = require_active_sketch(*document_);
  polysmith::core::cancel_mirror_preview(*feature_it);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_tangent_constraint(
    const std::string& line_id, const std::string& circle_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_tangent_constraint(
      *feature_it, line_id, circle_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_parallel_constraint(
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_parallel_constraint(
      *feature_it, line_id, other_line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_coincident_constraint(
    const std::string& point_id, const std::string& other_point_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_coincident_constraint(
      *feature_it, point_id, other_point_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = other_point_id;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_point_fixed(const std::string& point_id,
                                                      bool is_fixed) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::set_sketch_point_fixed(*feature_it, point_id, is_fixed);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = point_id;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_circle(const std::string& circle_id,
                                                    double center_x,
                                                    double center_y,
                                                    double radius) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_sketch_circle(
      *feature_it, circle_id, center_x, center_y, radius);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = circle_id;
  document_->selected_sketch_dimension_id = "dim-circle-" + circle_id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_angle_dimension(
    const std::string& first_line_id,
    const std::string& second_line_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::add_sketch_angle_dimension(
      *feature_it, first_line_id, second_line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.kind == "angle" &&
               ((dimension.entity_id == first_line_id &&
                 dimension.secondary_entity_id == second_line_id) ||
                (dimension.entity_id == second_line_id &&
                 dimension.secondary_entity_id == first_line_id));
      });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_point_id = std::nullopt;
    document_->selected_sketch_point_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_distance_dimension(
    const std::string& first_entity_id,
    const std::string& second_entity_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::add_sketch_distance_dimension(
      *feature_it, first_entity_id, second_entity_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dimension) {
        if (dimension.kind != "line_line_distance" &&
            dimension.kind != "circle_center_distance" &&
            dimension.kind != "circle_line_distance") {
          return false;
        }
        return (dimension.entity_id == first_entity_id &&
                dimension.secondary_entity_id == second_entity_id) ||
               (dimension.entity_id == second_entity_id &&
                dimension.secondary_entity_id == first_entity_id);
      });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_point_id = std::nullopt;
    document_->selected_sketch_point_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_line_length_dimension(
    const std::string& line_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::add_sketch_line_length_dimension(*feature_it, line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;

  const std::string dimension_id = "dim-line-" + line_id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_point_id = std::nullopt;
    document_->selected_sketch_point_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_circle_radius_dimension(
    const std::string& circle_id,
    std::optional<std::string> display_as) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::add_sketch_circle_radius_dimension(
      *feature_it, circle_id, display_as);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;

  const std::string dimension_id = "dim-circle-" + circle_id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_point_id = std::nullopt;
    document_->selected_sketch_point_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_polygon_radius_dimension(
    const std::string& polygon_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::add_sketch_polygon_radius_dimension(*feature_it,
                                                        polygon_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;

  const std::string dimension_id = "dim-polygon-" + polygon_id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_point_id = std::nullopt;
    document_->selected_sketch_point_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::trim_sketch_entity(
    const std::string& entity_id,
    double click_x,
    double click_y) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::trim_sketch_entity(
      *feature_it, entity_id, click_x, click_y);

  // Refresh derived state: rebuild points, recompute profiles, reify
  // dimensions, and re-sync projection/dependency state.
  polysmith::core::refresh_sketch_derived_state(*feature_it);

  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = entity_id;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_point_distance_dimension(
    const std::string& point_a_id,
    const std::string& point_b_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::add_sketch_point_distance_dimension(
      *feature_it, point_a_id, point_b_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;

  // Select the newly created dimension
  const std::string dimension_id =
      "dim-point-distance-" + point_a_id + "-" + point_b_id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_point_id = std::nullopt;
    document_->selected_sketch_point_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_dimension(
    const std::string& dimension_id,
    double value,
    std::optional<std::string> expression) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dimension) { return dimension.id == dimension_id; });
  if (dimension_it == feature_it->sketch_parameters->dimensions.end()) {
    throw std::runtime_error("Sketch dimension not found: " + dimension_id);
  }
  if (dimension_it->kind == "circle_radius") {
    for (const auto& projection : feature_it->sketch_parameters->projections) {
      if (std::find(projection.generated_circle_ids.begin(),
                    projection.generated_circle_ids.end(),
                    dimension_it->entity_id) !=
          projection.generated_circle_ids.end()) {
        throw std::runtime_error(
            "Projected circle dimensions are driven by their source geometry");
      }
    }
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_sketch_dimension(*feature_it, dimension_id, value, expression);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  if (feature_it->sketch_parameters.has_value()) {
    const auto updated_dimension_it = std::find_if(
        feature_it->sketch_parameters->dimensions.begin(),
        feature_it->sketch_parameters->dimensions.end(),
        [&](const SketchDimension& dimension) {
          return dimension.id == dimension_id;
        });

    if (updated_dimension_it != feature_it->sketch_parameters->dimensions.end()) {
      document_->selected_sketch_entity_id = updated_dimension_it->entity_id;
      document_->selected_sketch_dimension_id = updated_dimension_it->id;
    }
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_dimension_label_position(
    const std::string& dimension_id,
    double label_x,
    double label_y) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  auto& dimensions = feature_it->sketch_parameters->dimensions;
  const auto dimension_it = std::find_if(
      dimensions.begin(),
      dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.id == dimension_id;
      });
  if (dimension_it == dimensions.end()) {
    throw std::runtime_error("Sketch dimension not found: " + dimension_id);
  }

  push_undo_state();
  clear_redo_stack();
  dimension_it->label_x = label_x;
  dimension_it->label_y = label_y;
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = dimension_it->entity_id;
  document_->selected_sketch_dimension_id = dimension_it->id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

namespace {

std::vector<FeatureEntry>::iterator find_sketch_feature_owning_profile(
    std::vector<FeatureEntry>& features, const std::string& profile_id) {
  return std::find_if(
      features.begin(), features.end(), [&](const FeatureEntry& feature) {
        if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
          return false;
        }
        const auto& profiles = feature.sketch_parameters->profiles;
        return std::any_of(
            profiles.begin(), profiles.end(),
            [&](const SketchProfileRegion& profile) {
              return profile.id == profile_id;
            });
      });
}

std::vector<FeatureEntry>::iterator find_sketch_feature_owning_line(
    std::vector<FeatureEntry>& features, const std::string& line_id) {
  return std::find_if(
      features.begin(), features.end(), [&](const FeatureEntry& feature) {
        if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
          return false;
        }
        const auto& lines = feature.sketch_parameters->lines;
        return std::any_of(
            lines.begin(), lines.end(),
            [&](const SketchLine& line) { return line.id == line_id; });
      });
}

SketchLine* find_line_by_id(SketchFeatureParameters& sketch,
                            const std::string& line_id) {
  const auto line_it = std::find_if(
      sketch.lines.begin(),
      sketch.lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });
  return line_it == sketch.lines.end() ? nullptr : &(*line_it);
}

const SketchLine* find_line_by_id(const SketchFeatureParameters& sketch,
                                  const std::string& line_id) {
  const auto line_it = std::find_if(
      sketch.lines.begin(),
      sketch.lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });
  return line_it == sketch.lines.end() ? nullptr : &(*line_it);
}

std::vector<FeatureEntry>::iterator find_sketch_feature_owning_path_entity(
    std::vector<FeatureEntry>& features, const std::string& entity_id) {
  return std::find_if(
      features.begin(), features.end(), [&](const FeatureEntry& feature) {
        if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
          return false;
        }
        const auto& sketch = feature.sketch_parameters.value();
        const bool owns_line = std::any_of(
            sketch.lines.begin(), sketch.lines.end(),
            [&](const SketchLine& line) { return line.id == entity_id; });
        const bool owns_arc = std::any_of(
            sketch.arcs.begin(), sketch.arcs.end(),
            [&](const SketchArc& arc) { return arc.id == entity_id; });
        return owns_line || owns_arc;
      });
}

struct SweepPathEntity {
  std::string id;
  std::string kind;
  std::string start_point_id;
  std::string end_point_id;
  const SketchLine* line = nullptr;
  const SketchArc* arc = nullptr;
};

double normalize_arc_sweep(double start_angle, double end_angle, bool ccw) {
  double sweep = end_angle - start_angle;
  if (ccw) {
    while (sweep <= 0.0) {
      sweep += 2.0 * kPi;
    }
  } else {
    while (sweep >= 0.0) {
      sweep -= 2.0 * kPi;
    }
  }
  return sweep;
}

std::vector<SweepPathEntity> order_sweep_path_entities(
    const SketchFeatureParameters& sketch,
    const std::string& seed_entity_id) {
  std::vector<SweepPathEntity> entities;
  for (const auto& line : sketch.lines) {
    if (line.is_construction) {
      continue;
    }
    entities.push_back(SweepPathEntity{.id = line.id,
                                       .kind = "line",
                                       .start_point_id = line.start_point_id,
                                       .end_point_id = line.end_point_id,
                                       .line = &line});
  }
  for (const auto& arc : sketch.arcs) {
    if (arc.is_construction) {
      continue;
    }
    entities.push_back(SweepPathEntity{.id = arc.id,
                                       .kind = "arc",
                                       .start_point_id = arc.start_point_id,
                                       .end_point_id = arc.end_point_id,
                                       .arc = &arc});
  }

  auto seed_it = std::find_if(
      entities.begin(), entities.end(),
      [&](const SweepPathEntity& entity) { return entity.id == seed_entity_id; });
  if (seed_it == entities.end()) {
    throw std::runtime_error("Sweep path entity not found: " + seed_entity_id);
  }
  const size_t seed_index = static_cast<size_t>(
      std::distance(entities.begin(), seed_it));

  std::unordered_map<std::string, std::vector<size_t>> by_point;
  for (size_t index = 0; index < entities.size(); ++index) {
    by_point[entities[index].start_point_id].push_back(index);
    by_point[entities[index].end_point_id].push_back(index);
  }

  std::vector<size_t> stack{seed_index};
  std::unordered_set<size_t> component;
  while (!stack.empty()) {
    const size_t index = stack.back();
    stack.pop_back();
    if (!component.insert(index).second) {
      continue;
    }
    for (const auto& point_id :
         {entities[index].start_point_id, entities[index].end_point_id}) {
      for (const size_t connected : by_point[point_id]) {
        if (!component.count(connected)) {
          stack.push_back(connected);
        }
      }
    }
  }

  for (const size_t index : component) {
    const auto& entity = entities[index];
    if (by_point[entity.start_point_id].size() > 2 ||
        by_point[entity.end_point_id].size() > 2) {
      throw std::runtime_error(
          "Sweep path cannot contain branches. Select a simple connected path.");
    }
  }

  size_t start_index = seed_index;
  std::string current_point = entities[seed_index].start_point_id;
  for (const size_t index : component) {
    const auto& entity = entities[index];
    if (by_point[entity.start_point_id].size() == 1) {
      start_index = index;
      current_point = entity.start_point_id;
      break;
    }
    if (by_point[entity.end_point_id].size() == 1) {
      start_index = index;
      current_point = entity.end_point_id;
      break;
    }
  }

  std::vector<SweepPathEntity> ordered;
  std::unordered_set<size_t> used;
  size_t current_index = start_index;
  while (used.size() < component.size()) {
    const auto& entity = entities[current_index];
    SweepPathEntity oriented = entity;
    if (entity.end_point_id == current_point) {
      std::swap(oriented.start_point_id, oriented.end_point_id);
    }
    ordered.push_back(oriented);
    used.insert(current_index);
    current_point = oriented.end_point_id;

    std::optional<size_t> next_index;
    for (const size_t connected : by_point[current_point]) {
      if (component.count(connected) && !used.count(connected)) {
        next_index = connected;
        break;
      }
    }
    if (!next_index.has_value()) {
      break;
    }
    current_index = next_index.value();
  }

  if (ordered.size() != component.size()) {
    throw std::runtime_error("Sweep path must be a single connected chain.");
  }
  return ordered;
}

std::vector<SweepFeatureParameters::PathSegment> make_sweep_path_segments(
    const SketchFeatureParameters& sketch,
    const std::string& seed_entity_id) {
  const auto ordered = order_sweep_path_entities(sketch, seed_entity_id);
  std::vector<SweepFeatureParameters::PathSegment> segments;
  for (const auto& entity : ordered) {
    if (entity.kind == "line" && entity.line != nullptr) {
      const bool reversed = entity.start_point_id != entity.line->start_point_id;
      const auto start = revolve_sketch_local_to_world(
          sketch, reversed ? entity.line->end_x : entity.line->start_x,
          reversed ? entity.line->end_y : entity.line->start_y);
      const auto end = revolve_sketch_local_to_world(
          sketch, reversed ? entity.line->start_x : entity.line->end_x,
          reversed ? entity.line->start_y : entity.line->end_y);
      segments.push_back(SweepFeatureParameters::PathSegment{
          .entity_id = entity.id,
          .kind = "line",
          .start_x = start[0],
          .start_y = start[1],
          .start_z = start[2],
          .end_x = end[0],
          .end_y = end[1],
          .end_z = end[2],
      });
      continue;
    }
    if (entity.kind == "arc" && entity.arc != nullptr) {
      const bool reversed = entity.start_point_id != entity.arc->start_point_id;
      const bool ccw = reversed ? !entity.arc->ccw : entity.arc->ccw;
      const double start_x = reversed ? entity.arc->end_x : entity.arc->start_x;
      const double start_y = reversed ? entity.arc->end_y : entity.arc->start_y;
      const double end_x = reversed ? entity.arc->start_x : entity.arc->end_x;
      const double end_y = reversed ? entity.arc->start_y : entity.arc->end_y;
      const double start_angle =
          std::atan2(start_y - entity.arc->center_y,
                     start_x - entity.arc->center_x);
      const double end_angle =
          std::atan2(end_y - entity.arc->center_y,
                     end_x - entity.arc->center_x);
      const double mid_angle =
          start_angle + normalize_arc_sweep(start_angle, end_angle, ccw) * 0.5;
      const double mid_x = entity.arc->center_x +
                           entity.arc->radius * std::cos(mid_angle);
      const double mid_y = entity.arc->center_y +
                           entity.arc->radius * std::sin(mid_angle);
      const auto start = revolve_sketch_local_to_world(sketch, start_x, start_y);
      const auto end = revolve_sketch_local_to_world(sketch, end_x, end_y);
      const auto center = revolve_sketch_local_to_world(
          sketch, entity.arc->center_x, entity.arc->center_y);
      const auto mid = revolve_sketch_local_to_world(sketch, mid_x, mid_y);
      segments.push_back(SweepFeatureParameters::PathSegment{
          .entity_id = entity.id,
          .kind = "arc",
          .start_x = start[0],
          .start_y = start[1],
          .start_z = start[2],
          .end_x = end[0],
          .end_y = end[1],
          .end_z = end[2],
          .center_x = center[0],
          .center_y = center[1],
          .center_z = center[2],
          .mid_x = mid[0],
          .mid_y = mid[1],
          .mid_z = mid[2],
          .radius = entity.arc->radius,
          .ccw = ccw,
      });
    }
  }
  if (segments.empty()) {
    throw std::runtime_error("Sweep path must contain at least one segment.");
  }
  return segments;
}

LoftFeatureParameters make_loft_parameters_for_profiles(
    std::vector<FeatureEntry>& features,
    const std::vector<std::string>& profile_ids,
    bool ruled) {
  if (profile_ids.size() < 2) {
    throw std::runtime_error("Loft requires at least two sketch profiles");
  }

  LoftFeatureParameters parameters{};
  parameters.ruled = ruled;
  for (const auto& profile_id : profile_ids) {
    const auto sketch_it = find_sketch_feature_owning_profile(features, profile_id);
    if (sketch_it == features.end()) {
      throw std::runtime_error("Sketch profile not found: " + profile_id);
    }
    const auto& sketch = sketch_it->sketch_parameters.value();
    const auto profile_it = std::find_if(
        sketch.profiles.begin(), sketch.profiles.end(),
        [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
    if (profile_it == sketch.profiles.end()) {
      throw std::runtime_error("Sketch profile not found: " + profile_id);
    }
    if (!profile_it->inner_loops.empty()) {
      throw std::runtime_error("Loft does not support profiles with holes yet");
    }
    const auto section = make_loft_section_for_profile(*sketch_it, *profile_it);
    if (!section.has_value()) {
      throw std::runtime_error("Selected profile is not supported for loft");
    }
    parameters.sections.push_back(section.value());
  }

  return parameters;
}

RevolveFeatureParameters make_revolve_parameters(
    std::vector<FeatureEntry>& features,
    const std::string& profile_id,
    const std::string& axis_entity_id,
    double angle_degrees) {
  const auto profile_sketch_it =
      find_sketch_feature_owning_profile(features, profile_id);
  if (profile_sketch_it == features.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }
  auto& profile_sketch = profile_sketch_it->sketch_parameters.value();
  const auto profile_it = std::find_if(
      profile_sketch.profiles.begin(),
      profile_sketch.profiles.end(),
      [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
  if (profile_it == profile_sketch.profiles.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  const auto axis_sketch_it =
      find_sketch_feature_owning_line(features, axis_entity_id);
  if (axis_sketch_it == features.end()) {
    throw std::runtime_error("Revolve axis line not found: " + axis_entity_id);
  }
  const SketchLine* axis_line =
      find_line_by_id(axis_sketch_it->sketch_parameters.value(), axis_entity_id);
  if (axis_line == nullptr) {
    throw std::runtime_error("Revolve axis line not found: " + axis_entity_id);
  }

  const auto parameters =
      make_revolve_parameters_for_profile(*profile_sketch_it,
                                          *profile_it,
                                          *axis_sketch_it,
                                          *axis_line,
                                          angle_degrees);
  if (!parameters.has_value()) {
    throw std::runtime_error("Selected profile or axis is not supported for revolve");
  }
  return parameters.value();
}

SweepFeatureParameters make_sweep_parameters(
    std::vector<FeatureEntry>& features,
    const std::string& profile_id,
    const std::string& path_entity_id) {
  const auto profile_sketch_it =
      find_sketch_feature_owning_profile(features, profile_id);
  if (profile_sketch_it == features.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }
  const auto& profile_sketch = profile_sketch_it->sketch_parameters.value();
  const auto profile_it = std::find_if(
      profile_sketch.profiles.begin(),
      profile_sketch.profiles.end(),
      [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
  if (profile_it == profile_sketch.profiles.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  const auto path_sketch_it =
      find_sketch_feature_owning_path_entity(features, path_entity_id);
  if (path_sketch_it == features.end()) {
    throw std::runtime_error("Sweep path entity not found: " + path_entity_id);
  }

  const std::string plane_id =
      profile_sketch.plane_frame.has_value()
          ? plane_id_from_frame(profile_sketch.plane_frame.value())
          : profile_sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      profile_sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(
                make_plane_frame(profile_sketch.plane_frame.value()))
          : std::nullopt;
  const auto path_segments = make_sweep_path_segments(
      path_sketch_it->sketch_parameters.value(), path_entity_id);
  const auto& first_segment = path_segments.front();
  const auto& last_segment = path_segments.back();

  return SweepFeatureParameters{
      .sketch_feature_id = profile_sketch_it->id,
      .profile_id = profile_it->id,
      .plane_id = plane_id,
      .plane_frame = plane_frame,
      .profile_kind = profile_it->kind,
      .profile_points = profile_it->kind == "circle"
                            ? sample_circle_profile_points(*profile_it)
                            : profile_it->points,
      .inner_loops = profile_it->inner_loops,
      .path_sketch_feature_id = path_sketch_it->id,
      .path_entity_id = path_entity_id,
      .path_start_x = first_segment.start_x,
      .path_start_y = first_segment.start_y,
      .path_start_z = first_segment.start_z,
      .path_end_x = last_segment.end_x,
      .path_end_y = last_segment.end_y,
      .path_end_z = last_segment.end_z,
      .path_segments = path_segments,
  };
}

}  // namespace

DocumentState DocumentManager::select_sketch_profile(const std::string& profile_id,
                                                     bool additive) {
  require_document();

  // Selection of profiles is allowed both inside and outside an active sketch.
  // The owning sketch feature is located by scanning the feature history.
  const auto feature_it = find_sketch_feature_owning_profile(
      document_->feature_history, profile_id);

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  const auto profile_it = std::find_if(
      feature_it->sketch_parameters->profiles.begin(),
      feature_it->sketch_parameters->profiles.end(),
      [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });

  if (profile_it == feature_it->sketch_parameters->profiles.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  if (additive) {
    auto& selected = document_->selected_sketch_profile_ids;
    const auto existing = std::find(selected.begin(), selected.end(), profile_id);
    if (existing == selected.end()) {
      selected.push_back(profile_id);
    } else {
      selected.erase(existing);
    }
  } else {
    document_->selected_sketch_profile_ids = {profile_id};
  }
  document_->selected_sketch_profile_id =
      document_->selected_sketch_profile_ids.empty()
          ? std::optional<std::string>{}
          : std::optional<std::string>{document_->selected_sketch_profile_ids.back()};
  return document_.value();
}

DocumentState DocumentManager::extrude_profile(
    const std::string& profile_id,
    double depth,
    const std::string& mode,
    const std::optional<std::string>& target_body_id,
    const std::optional<ExtrudeFeatureParameters>& parameters) {
  return extrude_profiles({profile_id}, depth, mode, target_body_id, parameters);
}

DocumentState DocumentManager::extrude_profiles(
    const std::vector<std::string>& profile_ids,
    double depth,
    const std::string& mode,
    const std::optional<std::string>& target_body_id,
    const std::optional<ExtrudeFeatureParameters>& parameters) {
  require_document();
  if (profile_ids.empty()) {
    throw std::runtime_error("No sketch profiles selected");
  }

  // Extrusion runs on any sketch profile in the document, even if its parent
  // sketch is finished (i.e. not the active sketch).
  const auto feature_it = find_sketch_feature_owning_profile(
      document_->feature_history, profile_ids.front());

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_ids.front());
  }

  const auto& sketch = feature_it->sketch_parameters.value();
  std::vector<SketchProfileRegion> selected_profiles;
  for (const auto& profile_id : profile_ids) {
    const auto owner_it = find_sketch_feature_owning_profile(
        document_->feature_history, profile_id);
    if (owner_it == document_->feature_history.end() ||
        owner_it->id != feature_it->id) {
      throw std::runtime_error(
          "Selected profiles must belong to the same sketch plane");
    }
    const auto profile_it = std::find_if(
        sketch.profiles.begin(), sketch.profiles.end(),
        [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
    if (profile_it == sketch.profiles.end()) {
      throw std::runtime_error("Sketch profile not found: " + profile_id);
    }
    selected_profiles.push_back(*profile_it);
  }

  const bool automatic_mode = mode.empty();
  std::vector<std::vector<SketchProfileRegion>> profile_groups;
  if (selected_profiles.size() == 1 || mode == "cut" || mode == "intersect" ||
      (mode == "join" && target_body_id.has_value())) {
    profile_groups.push_back(selected_profiles);
  } else if (automatic_mode || mode == "join") {
    profile_groups = group_touching_profiles(selected_profiles);
  } else {
    for (const auto& profile : selected_profiles) {
      profile_groups.push_back({profile});
    }
  }

  std::vector<ExtrudeFeatureParameters> extrude_parameter_groups;
  extrude_parameter_groups.reserve(profile_groups.size());
  for (const auto& group : profile_groups) {
    std::optional<ExtrudeFeatureParameters> extrude_parameters =
        make_extrude_parameters_for_profiles(*feature_it, group, depth);
    if (!extrude_parameters.has_value()) {
      throw std::runtime_error("Sketch profile not found");
    }
    if (parameters.has_value()) {
      const auto source = *extrude_parameters;
      *extrude_parameters = parameters.value();
      preserve_extrude_source_geometry(*extrude_parameters, source);
    }
    extrude_parameters->depth = depth;
    extrude_parameters->target_body_id = target_body_id;
    if (automatic_mode) {
      apply_automatic_extrude_mode(*document_,
                                   extrude_parameters.value(),
                                   group.size() > 1);
    } else {
      const bool split_join_without_target =
          mode == "join" && !target_body_id.has_value();
      extrude_parameters->mode = split_join_without_target ? "new_body" : mode;
      if (extrude_parameters->operation == "new_body" &&
          mode != "new_body") {
        extrude_parameters->operation = mode;
      }
      normalize_extrude_operation_mode(extrude_parameters.value());
    }
    normalize_extrude_parameters(*document_, extrude_parameters.value());
    extrude_parameter_groups.push_back(extrude_parameters.value());
  }

  push_undo_state();
  clear_redo_stack();
  for (const auto& extrude_parameters : extrude_parameter_groups) {
    document_->feature_history.push_back(
        create_extrude_feature(next_feature_id_++, extrude_parameters));
  }
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::extrude_open_entities(
    const std::vector<std::string>& entity_ids,
    double depth,
    const std::string& mode,
    const std::optional<std::string>& target_body_id,
    const std::optional<ExtrudeFeatureParameters>& parameters) {
  require_document();
  if (entity_ids.empty()) {
    throw std::runtime_error("No sketch entities selected");
  }

  FeatureEntry* sketch_feature = nullptr;
  std::vector<SketchProfilePoint> chain;
  for (const auto& entity_id : entity_ids) {
    FeatureEntry* owner = nullptr;
    const SketchLine* line = nullptr;
    const SketchArc* arc = nullptr;
    for (auto& feature : document_->feature_history) {
      if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
        continue;
      }
      const auto& sketch = feature.sketch_parameters.value();
      const auto line_it = std::find_if(
          sketch.lines.begin(),
          sketch.lines.end(),
          [&](const SketchLine& candidate) { return candidate.id == entity_id; });
      if (line_it != sketch.lines.end()) {
        owner = &feature;
        line = &(*line_it);
        break;
      }
      const auto arc_it = std::find_if(
          sketch.arcs.begin(),
          sketch.arcs.end(),
          [&](const SketchArc& candidate) { return candidate.id == entity_id; });
      if (arc_it != sketch.arcs.end()) {
        owner = &feature;
        arc = &(*arc_it);
        break;
      }
    }
    if (owner == nullptr) {
      throw std::runtime_error("Sketch entity not found: " + entity_id);
    }
    if (sketch_feature != nullptr && sketch_feature->id != owner->id) {
      throw std::runtime_error(
          "Open thin extrude entities must belong to the same sketch");
    }
    sketch_feature = owner;

    auto append_point = [&](double x, double y) {
      if (!chain.empty()) {
        const auto& last = chain.back();
        const double dx = last.x - x;
        const double dy = last.y - y;
        if (std::sqrt(dx * dx + dy * dy) <= 1.0e-6) {
          return;
        }
      }
      chain.push_back({.x = x, .y = y});
    };

    if (line != nullptr) {
      append_point(line->start_x, line->start_y);
      append_point(line->end_x, line->end_y);
    } else if (arc != nullptr) {
      const double start_angle =
          std::atan2(arc->start_y - arc->center_y, arc->start_x - arc->center_x);
      double end_angle =
          std::atan2(arc->end_y - arc->center_y, arc->end_x - arc->center_x);
      if (arc->ccw && end_angle < start_angle) {
        end_angle += 2.0 * 3.14159265358979323846;
      } else if (!arc->ccw && end_angle > start_angle) {
        end_angle -= 2.0 * 3.14159265358979323846;
      }
      constexpr int kArcSegments = 16;
      for (int index = 0; index <= kArcSegments; ++index) {
        const double t = static_cast<double>(index) / kArcSegments;
        const double angle = start_angle + (end_angle - start_angle) * t;
        append_point(arc->center_x + std::cos(angle) * arc->radius,
                     arc->center_y + std::sin(angle) * arc->radius);
      }
    }
  }

  if (sketch_feature == nullptr || !sketch_feature->sketch_parameters.has_value()) {
    throw std::runtime_error("Open thin extrude source sketch not found");
  }
  if (chain.size() < 2) {
    throw std::runtime_error("Open thin extrude requires a connected chain");
  }

  const auto& sketch = sketch_feature->sketch_parameters.value();
  const std::string plane_id =
      sketch.plane_frame.has_value() ? plane_id_from_frame(sketch.plane_frame.value())
                                     : sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(make_plane_frame(sketch.plane_frame.value()))
          : std::nullopt;

  ExtrudeFeatureParameters extrude_parameters =
      parameters.value_or(ExtrudeFeatureParameters{});
  extrude_parameters.sketch_feature_id = sketch_feature->id;
  extrude_parameters.profile_id = "open-chain";
  extrude_parameters.profile_ids = {};
  extrude_parameters.open_entity_ids = entity_ids;
  extrude_parameters.plane_id = plane_id;
  extrude_parameters.plane_frame = plane_frame;
  extrude_parameters.profile_kind = "open_chain";
  extrude_parameters.start_x = 0.0;
  extrude_parameters.start_y = 0.0;
  extrude_parameters.width = 0.0;
  extrude_parameters.height = 0.0;
  extrude_parameters.radius = 0.0;
  extrude_parameters.profile_points = chain;
  extrude_parameters.inner_loops.clear();
  extrude_parameters.additional_profile_points.clear();
  extrude_parameters.additional_inner_loops.clear();
  extrude_parameters.depth = depth;
  extrude_parameters.mode = mode;
  if (mode.empty()) {
    extrude_parameters.mode = "new_body";
    extrude_parameters.operation = "new_body";
  } else if (extrude_parameters.operation == "new_body" && mode != "new_body") {
    extrude_parameters.operation = mode;
  }
  extrude_parameters.target_body_id = target_body_id;
  extrude_parameters.thin.enabled = true;
  normalize_extrude_parameters(*document_, extrude_parameters);

  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_extrude_feature(next_feature_id_++, extrude_parameters));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_sketch_entity_ids.clear();
  document_->selected_sketch_entity_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::extrude_face(
    const std::string& face_id,
    double depth,
    const std::string& mode,
    const std::optional<std::string>& target_body_id,
    const std::optional<ExtrudeFeatureParameters>& parameters) {
  require_document();

  const auto profile = compute_planar_face_profile(*document_, face_id);
  if (!profile.has_value()) {
    throw std::runtime_error("Selected face is not a supported planar face");
  }

  ExtrudeFeatureParameters extrude_parameters{
      .sketch_feature_id = "",
      .profile_id = "face:" + face_id,
      .profile_ids = {"face:" + face_id},
      .plane_id = plane_id_from_frame(profile->plane_frame),
      .plane_frame = profile->plane_frame,
      .profile_kind = "polygon",
      .start_x = 0.0,
      .start_y = 0.0,
      .width = 0.0,
      .height = 0.0,
      .radius = 0.0,
      .profile_points = profile->outer_points,
      .inner_loops = profile->inner_loops,
      .depth = depth,
      .mode = mode,
      .target_body_id = target_body_id,
  };

  if (parameters.has_value()) {
    const auto source = extrude_parameters;
    extrude_parameters = parameters.value();
    extrude_parameters.sketch_feature_id = source.sketch_feature_id;
    extrude_parameters.profile_id = source.profile_id;
    extrude_parameters.profile_ids = source.profile_ids;
    extrude_parameters.plane_id = source.plane_id;
    extrude_parameters.plane_frame = source.plane_frame;
    extrude_parameters.profile_kind = source.profile_kind;
    extrude_parameters.start_x = source.start_x;
    extrude_parameters.start_y = source.start_y;
    extrude_parameters.width = source.width;
    extrude_parameters.height = source.height;
    extrude_parameters.radius = source.radius;
    extrude_parameters.profile_points = source.profile_points;
    extrude_parameters.inner_loops = source.inner_loops;
    extrude_parameters.additional_profile_points =
        source.additional_profile_points;
    extrude_parameters.additional_inner_loops = source.additional_inner_loops;
  }
  extrude_parameters.depth = depth;
  extrude_parameters.mode = mode;
  extrude_parameters.target_body_id = target_body_id;
  if (mode.empty()) {
    apply_automatic_extrude_mode(*document_, extrude_parameters, false);
  } else {
    if (extrude_parameters.operation == "new_body" && mode != "new_body") {
      extrude_parameters.operation = mode;
    }
    normalize_extrude_operation_mode(extrude_parameters);
  }
  normalize_extrude_parameters(*document_, extrude_parameters);

  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_extrude_feature(next_feature_id_++, extrude_parameters));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_extrude_profiles(
    const std::string& feature_id,
    const std::vector<std::string>& profile_ids) {
  require_document();
  if (profile_ids.empty()) {
    throw std::runtime_error("No sketch profiles selected");
  }

  const auto extrude_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (extrude_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (extrude_it->kind != "extrude" ||
      !extrude_it->extrude_parameters.has_value()) {
    throw std::runtime_error(
        "update_extrude_profiles requires an extrude feature: " + feature_id);
  }

  const auto sketch_it = find_sketch_feature_owning_profile(
      document_->feature_history, profile_ids.front());
  if (sketch_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_ids.front());
  }

  const auto& sketch = sketch_it->sketch_parameters.value();
  std::vector<SketchProfileRegion> selected_profiles;
  for (const auto& profile_id : profile_ids) {
    const auto owner_it = find_sketch_feature_owning_profile(
        document_->feature_history, profile_id);
    if (owner_it == document_->feature_history.end() ||
        owner_it->id != sketch_it->id) {
      throw std::runtime_error(
          "Selected profiles must belong to the same sketch plane");
    }
    const auto profile_it = std::find_if(
        sketch.profiles.begin(), sketch.profiles.end(),
        [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
    if (profile_it == sketch.profiles.end()) {
      throw std::runtime_error("Sketch profile not found: " + profile_id);
    }
    selected_profiles.push_back(*profile_it);
  }

  const double depth = extrude_it->extrude_parameters->depth;
  const std::string mode = extrude_it->extrude_parameters->mode;
  const auto target_body_id = extrude_it->extrude_parameters->target_body_id;
  std::optional<ExtrudeFeatureParameters> next_parameters;
  if (selected_profiles.size() == 1 && selected_profiles.front().kind == "circle") {
    next_parameters =
        make_extrude_parameters_for_profile(*sketch_it,
                                            selected_profiles.front(),
                                            depth);
  } else {
    const auto& first = selected_profiles.front();
    const std::string plane_id =
        sketch.plane_frame.has_value() ? plane_id_from_frame(sketch.plane_frame.value())
                                       : sketch.plane_id;
    const std::optional<PlaneFrame> plane_frame =
        sketch.plane_frame.has_value()
            ? std::optional<PlaneFrame>(make_plane_frame(sketch.plane_frame.value()))
            : std::nullopt;
    std::vector<std::string> ids;
    for (const auto& profile : selected_profiles) {
      ids.push_back(profile.id);
    }
    next_parameters = ExtrudeFeatureParameters{
        .sketch_feature_id = sketch_it->id,
        .profile_id = ids.front(),
        .profile_ids = ids,
        .plane_id = plane_id,
        .plane_frame = plane_frame,
        .profile_kind = "polygon",
        .start_x = 0.0,
        .start_y = 0.0,
        .width = 0.0,
        .height = 0.0,
        .radius = 0.0,
        .profile_points = first.kind == "circle"
                              ? sample_circle_profile_points(first)
                              : first.points,
        .inner_loops = first.inner_loops,
        .depth = depth,
    };
    for (size_t index = 1; index < selected_profiles.size(); ++index) {
      const auto& profile = selected_profiles[index];
      next_parameters->additional_profile_points.push_back(
          profile.kind == "circle" ? sample_circle_profile_points(profile)
                                   : profile.points);
      next_parameters->additional_inner_loops.push_back(profile.inner_loops);
    }
  }
  if (!next_parameters.has_value()) {
    throw std::runtime_error("Sketch profile not found");
  }
  next_parameters->mode = mode;
  next_parameters->target_body_id = target_body_id;
  extrude_it->extrude_parameters = next_parameters.value();
  extrude_it->parameters_summary =
      extrude_it->extrude_parameters->profile_id + " · " +
      std::to_string(extrude_it->extrude_parameters->depth) + " mm";
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::loft_profiles(
    const std::vector<std::string>& profile_ids,
    bool ruled) {
  require_document();
  const LoftFeatureParameters parameters =
      make_loft_parameters_for_profiles(document_->feature_history,
                                        profile_ids,
                                        ruled);

  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_loft_feature(next_feature_id_++, parameters));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_loft_profiles(
    const std::string& feature_id,
    const std::vector<std::string>& profile_ids) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "loft" || !feature_it->loft_parameters.has_value()) {
    throw std::runtime_error(
        "update_loft_profiles requires a loft feature: " + feature_id);
  }

  const bool ruled = feature_it->loft_parameters->ruled;
  const LoftFeatureParameters next_parameters =
      make_loft_parameters_for_profiles(document_->feature_history,
                                        profile_ids,
                                        ruled);
  const TopoDS_Shape next_shape = build_loft_shape(next_parameters);
  if (next_shape.IsNull()) {
    throw std::runtime_error("Loft update produced an empty shape");
  }
  feature_it->loft_parameters = next_parameters;
  feature_it->parameters_summary =
      std::to_string(feature_it->loft_parameters->sections.size()) +
      " sections" + (feature_it->loft_parameters->ruled ? " · ruled" : "");
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_loft_ruled(const std::string& feature_id,
                                                 bool ruled) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  polysmith::core::update_loft_ruled(*feature_it, ruled);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::revolve_profile(
    const std::string& profile_id,
    const std::string& axis_entity_id,
    double angle_degrees) {
  require_document();
  const RevolveFeatureParameters parameters =
      make_revolve_parameters(document_->feature_history,
                              profile_id,
                              axis_entity_id,
                              angle_degrees);

  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_revolve_feature(next_feature_id_++, parameters));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_revolve_profile(
    const std::string& feature_id,
    const std::string& profile_id) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "revolve" ||
      !feature_it->revolve_parameters.has_value()) {
    throw std::runtime_error(
        "update_revolve_profile requires a revolve feature: " + feature_id);
  }

  const RevolveFeatureParameters next_parameters =
      make_revolve_parameters(document_->feature_history,
                              profile_id,
                              feature_it->revolve_parameters->axis_entity_id,
                              feature_it->revolve_parameters->angle_degrees);
  const TopoDS_Shape next_shape = build_revolve_shape(next_parameters);
  if (next_shape.IsNull()) {
    throw std::runtime_error("Revolve update produced an empty shape");
  }
  feature_it->revolve_parameters = next_parameters;
  feature_it->parameters_summary =
      next_parameters.profile_id + " · " +
      std::to_string(next_parameters.angle_degrees) + " deg";
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_revolve_axis(
    const std::string& feature_id,
    const std::string& axis_entity_id) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "revolve" ||
      !feature_it->revolve_parameters.has_value()) {
    throw std::runtime_error(
        "update_revolve_axis requires a revolve feature: " + feature_id);
  }

  const RevolveFeatureParameters next_parameters =
      make_revolve_parameters(document_->feature_history,
                              feature_it->revolve_parameters->profile_id,
                              axis_entity_id,
                              feature_it->revolve_parameters->angle_degrees);
  const TopoDS_Shape next_shape = build_revolve_shape(next_parameters);
  if (next_shape.IsNull()) {
    throw std::runtime_error("Revolve update produced an empty shape");
  }
  feature_it->revolve_parameters = next_parameters;
  feature_it->parameters_summary =
      next_parameters.profile_id + " · " +
      std::to_string(next_parameters.angle_degrees) + " deg";
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_revolve_angle(
    const std::string& feature_id,
    double angle_degrees) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  polysmith::core::update_revolve_angle(*feature_it, angle_degrees);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::sweep_profile(
    const std::string& profile_id,
    const std::string& path_entity_id) {
  require_document();
  const SweepFeatureParameters parameters =
      make_sweep_parameters(document_->feature_history, profile_id, path_entity_id);

  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_sweep_feature(next_feature_id_++, parameters));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sweep_profile(
    const std::string& feature_id,
    const std::string& profile_id) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "sweep" || !feature_it->sweep_parameters.has_value()) {
    throw std::runtime_error(
        "update_sweep_profile requires a sweep feature: " + feature_id);
  }

  const SweepFeatureParameters next_parameters =
      make_sweep_parameters(document_->feature_history,
                            profile_id,
                            feature_it->sweep_parameters->path_entity_id);
  const TopoDS_Shape next_shape = build_sweep_shape(next_parameters);
  if (next_shape.IsNull()) {
    throw std::runtime_error("Sweep update produced an empty shape");
  }
  feature_it->sweep_parameters = next_parameters;
  feature_it->parameters_summary = "Profile · path";
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sweep_path(
    const std::string& feature_id,
    const std::string& path_entity_id) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "sweep" || !feature_it->sweep_parameters.has_value()) {
    throw std::runtime_error(
        "update_sweep_path requires a sweep feature: " + feature_id);
  }

  const SweepFeatureParameters next_parameters =
      make_sweep_parameters(document_->feature_history,
                            feature_it->sweep_parameters->profile_id,
                            path_entity_id);
  const TopoDS_Shape next_shape = build_sweep_shape(next_parameters);
  if (next_shape.IsNull()) {
    throw std::runtime_error("Sweep update produced an empty shape");
  }
  feature_it->sweep_parameters = next_parameters;
  feature_it->parameters_summary = "Profile · path";
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_line(double start_x,
                                               double start_y,
                                               double end_x,
                                               double end_y,
                                               bool is_construction) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::add_sketch_line(*feature_it,
                                   next_sketch_line_id_++,
                                   start_x,
                                   start_y,
                                   end_x,
                                   end_y,
                                   is_construction,
                                   /*snap_start=*/false);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->lines.back().id;
  // Construction lines don't get an auto length dimension, so the
  // dimension list may be unchanged. Pick the dimension only if one
  // was actually created for this line; otherwise clear the
  // selection so the editor doesn't open on an unrelated dimension.
  const std::string expected_dim_id =
      "dim-line-" + feature_it->sketch_parameters->lines.back().id;
  const auto& dims = feature_it->sketch_parameters->dimensions;
  const auto dim_it = std::find_if(
      dims.begin(), dims.end(),
      [&](const SketchDimension& dim) { return dim.id == expected_dim_id; });
  document_->selected_sketch_dimension_id =
      dim_it != dims.end() ? std::optional<std::string>{dim_it->id}
                           : std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->active_sketch_tool = "line";
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_rectangle(double start_x,
                                                    double start_y,
                                                    double end_x,
                                                    double end_y,
                                                    bool is_construction) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::add_sketch_rectangle(
      *feature_it, next_sketch_line_id_, start_x, start_y, end_x, end_y,
      is_construction);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->lines.back().id;
  document_->selected_sketch_dimension_id =
      is_construction ? std::nullopt
                      : std::make_optional(
                            feature_it->sketch_parameters->dimensions.back().id);
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->active_sketch_tool = "rectangle";
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_arc(double start_x,
                                              double start_y,
                                              double end_x,
                                              double end_y,
                                              double anchor_x,
                                              double anchor_y,
                                              const std::string& mode,
                                              bool is_construction) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  // Resolve (center, radius, ccw) up-front from whichever creation
  // mode the UI used. Doing it here keeps `polysmith::core::add_sketch_arc`
  // a pure constructor — it just stores the cached params we hand it.
  double center_x = 0.0;
  double center_y = 0.0;
  double radius = 0.0;
  double final_end_x = end_x;
  double final_end_y = end_y;
  bool ccw = true;

  if (mode == "three_point") {
    // Circumcenter of (start, end, anchor). Solved via the standard
    // perpendicular-bisector intersection:
    //   d = 2 * ((sx)(ey - ay) + (ex)(ay - sy) + (ax)(sy - ey))
    // Falls through to a degenerate-arc error when the three points
    // are colinear (d == 0) so the UI can surface the failure
    // instead of producing a malformed arc.
    const double sx = start_x;
    const double sy = start_y;
    const double ex = end_x;
    const double ey = end_y;
    const double ax = anchor_x;
    const double ay = anchor_y;
    const double d =
        2.0 * (sx * (ey - ay) + ex * (ay - sy) + ax * (sy - ey));
    if (std::abs(d) < 1e-9) {
      throw std::runtime_error(
          "Three-point arc: start, end, and anchor are colinear");
    }
    const double s2 = sx * sx + sy * sy;
    const double e2 = ex * ex + ey * ey;
    const double a2 = ax * ax + ay * ay;
    center_x =
        (s2 * (ey - ay) + e2 * (ay - sy) + a2 * (sy - ey)) / d;
    center_y =
        (s2 * (ax - ex) + e2 * (sx - ax) + a2 * (ex - sx)) / d;
    radius = std::hypot(start_x - center_x, start_y - center_y);
    // Sweep direction: CCW iff the signed area (start, anchor, end)
    // is positive. Matches "the anchor lies on the CCW arc from
    // start to end".
    const double cross =
        (anchor_x - start_x) * (end_y - start_y) -
        (anchor_y - start_y) * (end_x - start_x);
    ccw = cross > 0.0;
  } else if (mode == "center_start_end") {
    // anchor is the center; radius = |center - start|. We then
    // *snap* the user-supplied end point onto the resulting circle
    // by walking the angle direction from center→end and
    // re-projecting at radius distance — keeps the arc closed even
    // if the user's third click wasn't pixel-perfect.
    center_x = anchor_x;
    center_y = anchor_y;
    radius = std::hypot(start_x - center_x, start_y - center_y);
    // Mirrors the dimension-floor used elsewhere in sketch validation
    // (see `kMinimumSketchDimensionValue` in sketch_feature.cpp). Kept
    // as an inline literal here because the constant lives in a TU-
    // local anonymous namespace in sketch_feature.cpp; promoting it
    // to a header just for this one call site isn't worth the churn.
    constexpr double kArcMinimumDimension = 0.001;
    if (radius <= kArcMinimumDimension) {
      throw std::runtime_error("Center+start+end arc: radius is zero");
    }
    const double end_dx = end_x - center_x;
    const double end_dy = end_y - center_y;
    const double end_len = std::hypot(end_dx, end_dy);
    if (end_len <= kArcMinimumDimension) {
      throw std::runtime_error(
          "Center+start+end arc: end point coincides with center");
    }
    final_end_x = center_x + end_dx * radius / end_len;
    final_end_y = center_y + end_dy * radius / end_len;
    // CCW when going from start_angle to end_angle increases angle
    // (i.e. the cross product of the two radii is positive).
    const double cross =
        (start_x - center_x) * (final_end_y - center_y) -
        (start_y - center_y) * (final_end_x - center_x);
    ccw = cross > 0.0;
  } else {
    throw std::runtime_error("Unknown arc creation mode: " + mode);
  }

  push_undo_state();
  clear_redo_stack();

  // Endpoint points share the line counter so they live in a single
  // id space; arc id has its own counter.
  const int start_point_index = next_sketch_line_id_++;
  const int end_point_index = next_sketch_line_id_++;
  polysmith::core::add_sketch_arc(*feature_it,
                                  next_sketch_arc_id_++,
                                  start_point_index,
                                  end_point_index,
                                  start_x,
                                  start_y,
                                  final_end_x,
                                  final_end_y,
                                  center_x,
                                  center_y,
                                  radius,
                                  ccw,
                                  is_construction);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->arcs.back().id;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_fillet(
    const std::string& corner_point_id,
    const std::string& line_a_id,
    const std::string& line_b_id,
    double radius) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();

  // Trim points share the line endpoint counter so the points table
  // stays uniform; arc and fillet have independent counters because
  // they're separate id namespaces.
  const int trim_a_point_index = next_sketch_line_id_++;
  const int trim_b_point_index = next_sketch_line_id_++;
  polysmith::core::add_sketch_fillet(*feature_it,
                                     next_sketch_fillet_id_++,
                                     trim_a_point_index,
                                     trim_b_point_index,
                                     next_sketch_arc_id_++,
                                     corner_point_id,
                                     line_a_id,
                                     line_b_id,
                                     radius);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->fillets.back().id;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_fillet_radius(
    const std::string& fillet_id,
    double radius) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_sketch_fillet_radius(*feature_it, fillet_id, radius);
  refresh_linked_extrudes(*document_, *feature_it);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::delete_sketch_fillet(
    const std::string& fillet_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::delete_sketch_fillet(*feature_it, fillet_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_sketch_entity_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::delete_sketch_dimension(
    const std::string& dimension_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::delete_sketch_dimension(*feature_it, dimension_id);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_dimension_display(
    const std::string& dimension_id,
    const std::string& display_as) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  if (!feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Sketch feature has no parameters");
  }

  auto& dimensions = feature_it->sketch_parameters->dimensions;
  const auto dim_it = std::find_if(
      dimensions.begin(), dimensions.end(),
      [&](const SketchDimension& d) { return d.id == dimension_id; });
  if (dim_it == dimensions.end()) {
    throw std::runtime_error("Sketch dimension not found: " + dimension_id);
  }
  // Only circle_radius dimensions support display_as toggling
  if (dim_it->kind != "circle_radius") {
    throw std::runtime_error(
        "display_as can only be changed on circle radius dimensions");
  }

  push_undo_state();
  clear_redo_stack();
  dim_it->display_as = display_as;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::delete_sketch_selection(
    const std::vector<std::string>& entity_ids,
    const std::vector<std::string>& point_ids,
    const std::vector<std::string>& profile_ids) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });
  if (feature_it == document_->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  auto& parameters = *feature_it->sketch_parameters;
  std::unordered_set<std::string> line_ids;
  std::unordered_set<std::string> circle_ids;
  std::unordered_set<std::string> arc_ids;
  std::unordered_set<std::string> projected_point_ids;
  std::unordered_set<std::string> selected_point_ids;
  std::unordered_set<std::string> fillet_ids;

  add_ids(selected_point_ids, point_ids);

  for (const auto& profile_id : profile_ids) {
    const auto profile_it = std::find_if(
        parameters.profiles.begin(),
        parameters.profiles.end(),
        [&](const SketchProfileRegion& profile) {
          return profile.id == profile_id;
        });
    if (profile_it == parameters.profiles.end()) {
      continue;
    }
    add_ids(line_ids, profile_it->line_ids);
    if (profile_it->source_circle_id.has_value()) {
      add_id(circle_ids, profile_it->source_circle_id.value());
    }
  }

  for (const auto& entity_id : entity_ids) {
    add_id(line_ids, entity_id);
    add_id(circle_ids, entity_id);
    add_id(arc_ids, entity_id);
    add_id(fillet_ids, entity_id);
  }

  for (const auto& point_id : selected_point_ids) {
    for (const auto& line : parameters.lines) {
      if (line.start_point_id == point_id || line.end_point_id == point_id) {
        add_id(line_ids, line.id);
      }
    }
    for (const auto& arc : parameters.arcs) {
      if (arc.start_point_id == point_id || arc.end_point_id == point_id) {
        add_id(arc_ids, arc.id);
      }
    }
    for (const auto& circle : parameters.circles) {
      if ("point-circle-" + circle.id + "-center" == point_id) {
        add_id(circle_ids, circle.id);
      }
    }
    for (const auto& projected : parameters.projected_points) {
      if (projected.id == point_id) {
        add_id(projected_point_ids, projected.id);
      }
    }
  }

  for (const auto& fillet : parameters.fillets) {
    if (id_in_set(line_ids, fillet.line_a_id) ||
        id_in_set(line_ids, fillet.line_b_id) ||
        id_in_set(arc_ids, fillet.arc_id) ||
        id_in_set(selected_point_ids, fillet.corner_point_id) ||
        id_in_set(selected_point_ids, fillet.trim_a_point_id) ||
        id_in_set(selected_point_ids, fillet.trim_b_point_id)) {
      add_id(fillet_ids, fillet.id);
    }
  }

  const auto has_selected_line = [&](const SketchLine& line) {
    return id_in_set(line_ids, line.id);
  };
  const auto has_selected_circle = [&](const SketchCircle& circle) {
    return id_in_set(circle_ids, circle.id);
  };
  const auto has_selected_arc = [&](const SketchArc& arc) {
    return id_in_set(arc_ids, arc.id);
  };
  const auto has_selected_projected_point =
      [&](const SketchProjectedPoint& projected) {
        return id_in_set(projected_point_ids, projected.id);
      };

  const bool will_delete =
      std::any_of(parameters.lines.begin(), parameters.lines.end(), has_selected_line) ||
      std::any_of(parameters.circles.begin(),
                  parameters.circles.end(),
                  has_selected_circle) ||
      std::any_of(parameters.arcs.begin(), parameters.arcs.end(), has_selected_arc) ||
      std::any_of(parameters.projected_points.begin(),
                  parameters.projected_points.end(),
                  has_selected_projected_point) ||
      std::any_of(parameters.fillets.begin(),
                  parameters.fillets.end(),
                  [&](const SketchFillet& fillet) {
                    return id_in_set(fillet_ids, fillet.id);
                  });
  if (!will_delete) {
    return document_.value();
  }

  push_undo_state();
  clear_redo_stack();

  std::vector<std::string> fillets_to_delete;
  for (const auto& fillet : parameters.fillets) {
    if (id_in_set(fillet_ids, fillet.id)) {
      fillets_to_delete.push_back(fillet.id);
    }
  }
  for (const auto& fillet_id : fillets_to_delete) {
    polysmith::core::delete_sketch_fillet(*feature_it, fillet_id);
  }

  parameters.lines.erase(
      std::remove_if(parameters.lines.begin(), parameters.lines.end(), has_selected_line),
      parameters.lines.end());
  parameters.circles.erase(
      std::remove_if(parameters.circles.begin(),
                     parameters.circles.end(),
                     has_selected_circle),
      parameters.circles.end());
  parameters.arcs.erase(
      std::remove_if(parameters.arcs.begin(), parameters.arcs.end(), has_selected_arc),
      parameters.arcs.end());
  parameters.projected_points.erase(
      std::remove_if(parameters.projected_points.begin(),
                     parameters.projected_points.end(),
                     has_selected_projected_point),
      parameters.projected_points.end());

  const auto deleted_entity = [&](const std::string& id) {
    return id_in_set(line_ids, id) || id_in_set(circle_ids, id) ||
           id_in_set(arc_ids, id);
  };
  parameters.dimensions.erase(
      std::remove_if(parameters.dimensions.begin(),
                     parameters.dimensions.end(),
                     [&](const SketchDimension& dimension) {
                       return deleted_entity(dimension.entity_id) ||
                              deleted_entity(dimension.secondary_entity_id);
                     }),
      parameters.dimensions.end());
  parameters.line_relations.erase(
      std::remove_if(parameters.line_relations.begin(),
                     parameters.line_relations.end(),
                     [&](const SketchLineRelation& relation) {
                       return deleted_entity(relation.first_line_id) ||
                              deleted_entity(relation.second_line_id);
                     }),
      parameters.line_relations.end());
  parameters.midpoint_anchors.erase(
      std::remove_if(parameters.midpoint_anchors.begin(),
                     parameters.midpoint_anchors.end(),
                     [&](const SketchMidpointAnchor& anchor) {
                       return id_in_set(selected_point_ids, anchor.point_id) ||
                              id_in_set(line_ids, anchor.line_id);
                     }),
      parameters.midpoint_anchors.end());
  parameters.point_line_anchors.erase(
      std::remove_if(parameters.point_line_anchors.begin(),
                     parameters.point_line_anchors.end(),
                     [&](const SketchPointLineAnchor& anchor) {
                       return id_in_set(selected_point_ids, anchor.point_id) ||
                              id_in_set(line_ids, anchor.line_id);
                     }),
      parameters.point_line_anchors.end());
  for (auto& projection : parameters.projections) {
    remove_ids_from_vector(projection.generated_line_ids, line_ids);
    remove_ids_from_vector(projection.generated_circle_ids, circle_ids);
    remove_ids_from_vector(projection.generated_arc_ids, arc_ids);
    if (!projection.generated_point_id.empty() &&
        id_in_set(projected_point_ids, projection.generated_point_id)) {
      projection.generated_point_id.clear();
    }
  }
  parameters.projections.erase(
      std::remove_if(parameters.projections.begin(),
                     parameters.projections.end(),
                     [](const SketchProjection& projection) {
                       return !projection_has_generated_entities(projection);
                     }),
      parameters.projections.end());

  refresh_sketch_derived_state(*feature_it);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_circle(double center_x,
                                                 double center_y,
                                                 double radius,
                                                 bool is_construction) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::add_sketch_circle(
      *feature_it, next_sketch_circle_id_++, center_x, center_y, radius,
      is_construction);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->circles.back().id;
  document_->selected_sketch_dimension_id =
      is_construction ? std::nullopt
                      : std::make_optional(
                            feature_it->sketch_parameters->dimensions.back().id);
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->active_sketch_tool = "circle";
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_polygon(int sides,
                                                  const std::string& mode,
                                                  double start_x,
                                                  double start_y,
                                                  double end_x,
                                                  double end_y,
                                                  bool is_construction) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::add_sketch_polygon(
      *feature_it, next_sketch_polygon_id_++, sides, mode, start_x, start_y,
      end_x, end_y, is_construction);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->polygons.back().id;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->active_sketch_tool = "polygon";
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::select_sketch_point(const std::string& point_id,
                                                   bool additive) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  const auto point_it = std::find_if(
      feature_it->sketch_parameters->points.begin(),
      feature_it->sketch_parameters->points.end(),
      [&](const SketchPoint& point) { return point.id == point_id; });
  if (point_it == feature_it->sketch_parameters->points.end()) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_ids.clear();
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  if (additive) {
    auto& point_ids = document_->selected_sketch_point_ids;
    const auto existing =
        std::find(point_ids.begin(), point_ids.end(), point_id);
    if (existing == point_ids.end()) {
      point_ids.push_back(point_id);
    } else {
      point_ids.erase(existing);
    }
  } else {
    document_->selected_sketch_point_ids = {point_id};
  }
  document_->selected_sketch_point_id =
      document_->selected_sketch_point_ids.empty()
          ? std::nullopt
          : std::make_optional(document_->selected_sketch_point_ids.back());
  return document_.value();
}

DocumentState DocumentManager::select_sketch_entity(
    const std::string& entity_id, bool additive) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  const bool has_line = std::any_of(
      feature_it->sketch_parameters->lines.begin(),
      feature_it->sketch_parameters->lines.end(),
      [&](const SketchLine& line) { return line.id == entity_id; });
  const bool has_circle = std::any_of(
      feature_it->sketch_parameters->circles.begin(),
      feature_it->sketch_parameters->circles.end(),
      [&](const SketchCircle& circle) { return circle.id == entity_id; });
  const bool has_arc = std::any_of(
      feature_it->sketch_parameters->arcs.begin(),
      feature_it->sketch_parameters->arcs.end(),
      [&](const SketchArc& arc) { return arc.id == entity_id; });

  if (!has_line && !has_circle && !has_arc) {
    throw std::runtime_error("Sketch entity not found: " + entity_id);
  }

  const bool is_projected_entity = std::any_of(
      feature_it->sketch_parameters->projections.begin(),
      feature_it->sketch_parameters->projections.end(),
      [&](const SketchProjection& projection) {
        return projection_generates_entity(projection, entity_id);
      });

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  if (additive) {
    auto& entity_ids = document_->selected_sketch_entity_ids;
    const auto existing =
        std::find(entity_ids.begin(), entity_ids.end(), entity_id);
    if (existing == entity_ids.end()) {
      entity_ids.push_back(entity_id);
    } else {
      entity_ids.erase(existing);
    }
  } else {
    document_->selected_sketch_entity_ids = {entity_id};
  }
  document_->selected_sketch_entity_id =
      document_->selected_sketch_entity_ids.empty()
          ? std::nullopt
          : std::make_optional(document_->selected_sketch_entity_ids.back());
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dimension) {
        return !is_projected_entity &&
               document_->selected_sketch_entity_ids.size() == 1 &&
               dimension.entity_id == document_->selected_sketch_entity_ids.front();
      });
  document_->selected_sketch_dimension_id =
      dimension_it != feature_it->sketch_parameters->dimensions.end()
          ? std::make_optional(dimension_it->id)
          : std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  return document_.value();
}

DocumentState DocumentManager::select_sketch_dimension(
    const std::string& dimension_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dimension) { return dimension.id == dimension_id; });

  if (dimension_it == feature_it->sketch_parameters->dimensions.end()) {
    throw std::runtime_error("Sketch dimension not found: " + dimension_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = dimension_it->entity_id;
  document_->selected_sketch_dimension_id = dimension_it->id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

DocumentState DocumentManager::finish_sketch() {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch to finish");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  feature_it->status = "healthy";
  document_->selected_feature_id = feature_it->id;
  document_->selected_reference_id = std::nullopt;
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::reenter_sketch(const std::string& feature_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch feature not found: " + feature_id);
  }

  if (feature_it->kind != "sketch" || !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Feature is not a sketch: " + feature_id);
  }

  // Re-entering does not push to undo: it only flips active flags, so undo
  // continues to refer to real geometry edits.
  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->active_sketch_plane_id = feature_it->sketch_parameters->plane_id;
  // Face id information is not preserved separately in sketch_parameters; the
  // plane_id matches the face id when the sketch was created on a face, which
  // is sufficient for downstream consumers (viewport hides the sketch face,
  // raycasting is plane-frame based).
  document_->active_sketch_face_id =
      feature_it->sketch_parameters->plane_frame.has_value()
          ? std::optional<std::string>(feature_it->sketch_parameters->plane_id)
          : std::nullopt;
  document_->active_sketch_feature_id = feature_it->id;
  document_->active_sketch_tool = "select";
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::project_face_into_sketch(
    const std::string& face_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto sketch_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (sketch_it == document_->feature_history.end() ||
      !sketch_it->sketch_parameters.has_value() ||
      !sketch_it->sketch_parameters->plane_frame.has_value()) {
    throw std::runtime_error(
        "Active sketch does not have a plane frame for projection");
  }

  // Don't project a face onto its own owning sketch — the sketch is on the
  // base plane of the extrude, so projecting the base back onto it would
  // overlay the original profile.
  const std::string separator = ":face:";
  const auto sep_pos = face_id.find(separator);
  if (sep_pos == std::string::npos) {
    throw std::runtime_error("Invalid face id: " + face_id);
  }
  const std::string face_owner_id = face_id.substr(0, sep_pos);

  // Walk extrude features that point at this sketch — projecting one of
  // their faces back onto the sketch is almost always a user mistake.
  for (const auto& feature : document_->feature_history) {
    if (feature.kind != "extrude" || !feature.extrude_parameters.has_value()) {
      continue;
    }
    if (feature.extrude_parameters->sketch_feature_id == sketch_it->id &&
        feature.id == face_owner_id) {
      throw std::runtime_error(
          "Cannot project a face from an extrude back onto its own source "
          "sketch");
    }
  }

  // Idempotency: if the user already projected this face onto this
  // sketch (typical when the modal Project tool stays active and they
  // accidentally click the same face twice), skip the rebuild and
  // return the current document state unchanged. Matches common CAD workflow's
  // "second click is a no-op" behaviour. Vertex / edge projections
  // share this dedup index so all three project_* methods walk a
  // single `projections` list.
  for (const auto& projection :
       sketch_it->sketch_parameters->projections) {
    if (projection.source_id == face_id) {
      return document_.value();
    }
  }

  const auto outline = compute_face_outline(*document_, face_id);
  if (!outline.has_value()) {
    throw std::runtime_error(
        "Face is not supported by the projection helper: " + face_id);
  }

  const auto& sketch_frame = sketch_it->sketch_parameters->plane_frame.value();

  auto project_to_sketch_local =
      [&sketch_frame](const FaceOutlinePoint& world) -> std::pair<double, double> {
    const double dx = world.x - sketch_frame.origin_x;
    const double dy = world.y - sketch_frame.origin_y;
    const double dz = world.z - sketch_frame.origin_z;
    const double sx = dx * sketch_frame.x_axis_x + dy * sketch_frame.x_axis_y +
                      dz * sketch_frame.x_axis_z;
    const double sy = dx * sketch_frame.y_axis_x + dy * sketch_frame.y_axis_y +
                      dz * sketch_frame.y_axis_z;
    return {sx, sy};
  };

  push_undo_state();
  clear_redo_stack();

  // We capture the ids of every sketch entity this projection emits
  // so `refresh_sketch_projections` can later patch their cached
  // coords when the upstream face moves. Faces produce lines
  // (rectangle / polygon outline) or circles for circular loops.
  SketchProjection record;
  record.id =
      "projection-" + std::to_string(next_sketch_projection_id_++);
  record.source_id = face_id;
  record.source_kind = "face";

  if (outline->kind == "rectangle") {
    if (outline->rectangle_corners.size() != 4) {
      throw std::runtime_error("Rectangle outline must have four corners");
    }

    std::array<std::pair<double, double>, 4> local{};
    for (size_t i = 0; i < 4; ++i) {
      local[i] = project_to_sketch_local(outline->rectangle_corners[i]);
    }

    const size_t lines_before = sketch_it->sketch_parameters->lines.size();
    for (size_t i = 0; i < 4; ++i) {
      const auto& a = local[i];
      const auto& b = local[(i + 1) % 4];
      polysmith::core::add_sketch_line(*sketch_it,
                                       next_sketch_line_id_++,
                                       a.first,
                                       a.second,
                                       b.first,
                                       b.second);
    }

    // Lock every endpoint of the four projected lines so the user cannot
    // drag them away from their projected location. This mirrors common CAD workflow's
    // "Project" behaviour where projected entities are derived geometry.
    for (size_t i = lines_before; i < sketch_it->sketch_parameters->lines.size();
         ++i) {
      const auto& line = sketch_it->sketch_parameters->lines[i];
      polysmith::core::set_sketch_point_fixed(*sketch_it,
                                              line.start_point_id,
                                              true);
      polysmith::core::set_sketch_point_fixed(*sketch_it,
                                              line.end_point_id,
                                              true);
      record.generated_line_ids.push_back(line.id);
    }
  } else if (outline->kind == "circle") {
    auto add_projected_circle =
        [&](const FaceOutlinePoint& center, double radius) {
      const auto center_local = project_to_sketch_local(center);
      const size_t circles_before =
          sketch_it->sketch_parameters->circles.size();
      polysmith::core::add_sketch_circle(*sketch_it,
                                         next_sketch_circle_id_++,
                                         center_local.first,
                                         center_local.second,
                                         radius);
      for (size_t i = circles_before;
           i < sketch_it->sketch_parameters->circles.size(); ++i) {
        const auto& circle = sketch_it->sketch_parameters->circles[i];
        polysmith::core::set_sketch_point_fixed(
            *sketch_it, "point-circle-" + circle.id + "-center", true);
        record.generated_circle_ids.push_back(
            circle.id);
      }
    };

    add_projected_circle(outline->circle_center, outline->circle_radius);
    for (const auto& inner_circle : outline->inner_circles) {
      add_projected_circle(inner_circle.center, inner_circle.radius);
    }
  } else if (outline->kind == "polygon") {
    if (outline->polygon_corners.size() < 3) {
      throw std::runtime_error("Polygon outline must have at least 3 corners");
    }

    // Project every corner first, then add closed-loop sketch lines.
    std::vector<std::pair<double, double>> local;
    local.reserve(outline->polygon_corners.size());
    for (const auto& corner : outline->polygon_corners) {
      local.push_back(project_to_sketch_local(corner));
    }

    const size_t lines_before = sketch_it->sketch_parameters->lines.size();
    for (size_t i = 0; i < local.size(); ++i) {
      const auto& a = local[i];
      const auto& b = local[(i + 1) % local.size()];
      polysmith::core::add_sketch_line(*sketch_it,
                                       next_sketch_line_id_++,
                                       a.first,
                                       a.second,
                                       b.first,
                                       b.second);
    }
    for (const auto& inner_loop : outline->inner_loops) {
      if (inner_loop.size() < 3) {
        continue;
      }
      std::vector<std::pair<double, double>> inner_local;
      inner_local.reserve(inner_loop.size());
      for (const auto& corner : inner_loop) {
        inner_local.push_back(project_to_sketch_local(corner));
      }
      for (size_t i = 0; i < inner_local.size(); ++i) {
        const auto& a = inner_local[i];
        const auto& b = inner_local[(i + 1) % inner_local.size()];
        polysmith::core::add_sketch_line(*sketch_it,
                                         next_sketch_line_id_++,
                                         a.first,
                                         a.second,
                                         b.first,
                                         b.second);
      }
    }

    // Lock every endpoint of the projected lines (same CAD-style
    // "derived geometry" lock as the rectangle path).
    for (size_t i = lines_before; i < sketch_it->sketch_parameters->lines.size();
         ++i) {
      const auto& line = sketch_it->sketch_parameters->lines[i];
      polysmith::core::set_sketch_point_fixed(*sketch_it,
                                              line.start_point_id,
                                              true);
      polysmith::core::set_sketch_point_fixed(*sketch_it,
                                              line.end_point_id,
                                              true);
      record.generated_line_ids.push_back(line.id);
    }
  } else {
    throw std::runtime_error("Unsupported projected face kind: " + outline->kind);
  }

  // Register the live link so any future upstream edit of `face_id`
  // can re-derive these generated entities (see
  // `refresh_sketch_projections`).
  sketch_it->sketch_parameters->projections.push_back(std::move(record));

  refresh_linked_extrudes(*document_, *sketch_it);
  document_->selected_feature_id = sketch_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();

  return document_.value();
}

namespace {

// Validate that the active sketch in `document` is set up to receive
// projected geometry: there must be an active sketch with a plane
// frame. Returns the matching FeatureEntry iterator on success and
// throws a runtime_error on every failure path so all three
// project_*_into_sketch methods share the same diagnostics.
std::vector<FeatureEntry>::iterator require_projection_target(
    DocumentState& document) {
  if (!document.active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }
  const auto sketch_it = std::find_if(
      document.feature_history.begin(),
      document.feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document.active_sketch_feature_id.value();
      });
  if (sketch_it == document.feature_history.end() ||
      !sketch_it->sketch_parameters.has_value() ||
      !sketch_it->sketch_parameters->plane_frame.has_value()) {
    throw std::runtime_error(
        "Active sketch does not have a plane frame for projection");
  }
  return sketch_it;
}

// Flatten a world-space point onto the sketch's local (u, v) frame.
// Identical maths to the lambda inside `project_face_into_sketch` —
// pulled out so edge / vertex projection can reuse it without
// duplicating the dot-product expansion.
std::pair<double, double> world_to_sketch_local(
    const SketchFeatureParameters::SketchPlaneFrame& frame,
    double wx, double wy, double wz) {
  const double dx = wx - frame.origin_x;
  const double dy = wy - frame.origin_y;
  const double dz = wz - frame.origin_z;
  const double sx =
      dx * frame.x_axis_x + dy * frame.x_axis_y + dz * frame.x_axis_z;
  const double sy =
      dx * frame.y_axis_x + dy * frame.y_axis_y + dz * frame.y_axis_z;
  return {sx, sy};
}

// Returns true when `axis` is parallel (or anti-parallel) to the
// sketch frame's normal. A circular edge whose plane is parallel to
// the sketch plane projects cleanly to a circle / arc; non-parallel
// circular edges would project to ellipses, which we don't support
// in v1 (see the user-facing decision in the implementation log).
bool circle_axis_parallel_to_sketch(
    const SketchFeatureParameters::SketchPlaneFrame& frame,
    const EdgePoint& axis,
    double tolerance = 1e-6) {
  // dot product magnitude == 1 (within tolerance) iff parallel /
  // anti-parallel given both are unit vectors.
  const double dot = axis.x * frame.normal_x +
                     axis.y * frame.normal_y +
                     axis.z * frame.normal_z;
  return std::abs(std::abs(dot) - 1.0) <= tolerance;
}

FaceOutlinePoint sketch_local_to_world(const SketchFeatureParameters& sketch,
                                       double local_x,
                                       double local_y) {
  if (sketch.plane_frame.has_value()) {
    const auto& frame = sketch.plane_frame.value();
    return FaceOutlinePoint{
        .x = frame.origin_x + frame.x_axis_x * local_x +
             frame.y_axis_x * local_y,
        .y = frame.origin_y + frame.x_axis_y * local_x +
             frame.y_axis_y * local_y,
        .z = frame.origin_z + frame.x_axis_z * local_x +
             frame.y_axis_z * local_y,
    };
  }
  if (sketch.plane_id == "ref-plane-xy") {
    return FaceOutlinePoint{.x = local_x, .y = 0.0, .z = local_y};
  }
  if (sketch.plane_id == "ref-plane-yz") {
    return FaceOutlinePoint{.x = 0.0, .y = local_x, .z = local_y};
  }
  return FaceOutlinePoint{.x = local_x, .y = local_y, .z = 0.0};
}

FaceOutlinePoint sketch_normal(const SketchFeatureParameters& sketch) {
  if (sketch.plane_frame.has_value()) {
    const auto& frame = sketch.plane_frame.value();
    return FaceOutlinePoint{
        .x = frame.normal_x, .y = frame.normal_y, .z = frame.normal_z};
  }
  if (sketch.plane_id == "ref-plane-yz") {
    return FaceOutlinePoint{.x = 1.0, .y = 0.0, .z = 0.0};
  }
  if (sketch.plane_id == "ref-plane-xz") {
    return FaceOutlinePoint{.x = 0.0, .y = 0.0, .z = 1.0};
  }
  return FaceOutlinePoint{.x = 0.0, .y = 1.0, .z = 0.0};
}

bool normals_parallel(const FaceOutlinePoint& left,
                      const SketchFeatureParameters::SketchPlaneFrame& right,
                      double tolerance = 1e-6) {
  const double dot = left.x * right.normal_x + left.y * right.normal_y +
                     left.z * right.normal_z;
  return std::abs(std::abs(dot) - 1.0) <= tolerance;
}

bool circle_contains_circle_for_projection(const SketchCircle& outer,
                                           const SketchCircle& inner) {
  if (outer.id == inner.id || outer.radius <= inner.radius) {
    return false;
  }
  const double dx = inner.center_x - outer.center_x;
  const double dy = inner.center_y - outer.center_y;
  const double center_distance = std::sqrt(dx * dx + dy * dy);
  return center_distance + inner.radius <= outer.radius + 1e-6;
}

}  // namespace

DocumentState DocumentManager::project_profile_into_sketch(
    const std::string& profile_id) {
  require_document();
  const auto sketch_it = require_projection_target(*document_);

  for (const auto& projection : sketch_it->sketch_parameters->projections) {
    if (projection.source_id == profile_id) {
      return document_.value();
    }
  }

  const auto source_it = find_sketch_feature_owning_profile(
      document_->feature_history, profile_id);
  if (source_it == document_->feature_history.end() ||
      !source_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  const SketchFeatureParameters source_sketch = source_it->sketch_parameters.value();
  const auto profile_it = std::find_if(
      source_sketch.profiles.begin(), source_sketch.profiles.end(),
      [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
  if (profile_it == source_sketch.profiles.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }
  const SketchProfileRegion source_profile = *profile_it;
  const auto target_frame = sketch_it->sketch_parameters->plane_frame.value();

  auto project_source_point =
      [&](const SketchProfilePoint& point) -> std::pair<double, double> {
    const auto world = sketch_local_to_world(source_sketch, point.x, point.y);
    return world_to_sketch_local(target_frame, world.x, world.y, world.z);
  };

  push_undo_state();
  clear_redo_stack();

  SketchProjection record;
  record.id =
      "projection-" + std::to_string(next_sketch_projection_id_++);
  record.source_id = profile_id;
  record.source_kind = "profile";

  auto add_projected_loop =
      [&](const std::vector<SketchProfilePoint>& loop) {
    if (loop.size() < 3) {
      return;
    }
    std::vector<std::pair<double, double>> local;
    local.reserve(loop.size());
    for (const auto& point : loop) {
      local.push_back(project_source_point(point));
    }
    const size_t lines_before = sketch_it->sketch_parameters->lines.size();
    for (size_t index = 0; index < local.size(); ++index) {
      const auto& a = local[index];
      const auto& b = local[(index + 1) % local.size()];
      polysmith::core::add_sketch_line(*sketch_it,
                                       next_sketch_line_id_++,
                                       a.first,
                                       a.second,
                                       b.first,
                                       b.second);
    }
    for (size_t index = lines_before;
         index < sketch_it->sketch_parameters->lines.size(); ++index) {
      const auto& line = sketch_it->sketch_parameters->lines[index];
      polysmith::core::set_sketch_point_fixed(
          *sketch_it, line.start_point_id, true);
      polysmith::core::set_sketch_point_fixed(
          *sketch_it, line.end_point_id, true);
      record.generated_line_ids.push_back(line.id);
    }
  };

  const auto add_projected_circle =
      [&](const SketchCircle& circle) {
    const auto center_world = sketch_local_to_world(
        source_sketch, circle.center_x, circle.center_y);
    const auto center_local = world_to_sketch_local(
        target_frame, center_world.x, center_world.y, center_world.z);
    const size_t circles_before = sketch_it->sketch_parameters->circles.size();
    polysmith::core::add_sketch_circle(*sketch_it,
                                       next_sketch_circle_id_++,
                                       center_local.first,
                                       center_local.second,
                                       circle.radius);
    for (size_t index = circles_before;
         index < sketch_it->sketch_parameters->circles.size(); ++index) {
      const auto& circle = sketch_it->sketch_parameters->circles[index];
      polysmith::core::set_sketch_point_fixed(
          *sketch_it, "point-circle-" + circle.id + "-center", true);
      record.generated_circle_ids.push_back(
          circle.id);
    }
  };

  if (source_profile.kind == "circle" ||
      source_profile.source_circle_id.has_value()) {
    if (!normals_parallel(sketch_normal(source_sketch), target_frame)) {
      throw std::runtime_error(
          "Circular profile projects to an ellipse (its sketch plane is not "
          "parallel to the active sketch). Not supported in v1.");
    }
    const auto source_circle_it = source_profile.source_circle_id.has_value()
        ? std::find_if(
              source_sketch.circles.begin(), source_sketch.circles.end(),
              [&](const SketchCircle& circle) {
                return circle.id == source_profile.source_circle_id.value();
              })
        : std::find_if(
              source_sketch.circles.begin(), source_sketch.circles.end(),
              [&](const SketchCircle& circle) {
                return circle.id == source_profile.source_circle_id.value_or("");
              });
    if (source_profile.kind == "circle") {
      SketchCircle circle{};
      circle.id = source_profile.source_circle_id.value_or("");
      circle.center_x = source_profile.center_x;
      circle.center_y = source_profile.center_y;
      circle.radius = source_profile.radius;
      add_projected_circle(circle);
    } else if (source_circle_it != source_sketch.circles.end()) {
      add_projected_circle(*source_circle_it);
      for (const auto& candidate : source_sketch.circles) {
        if (candidate.is_construction ||
            !circle_contains_circle_for_projection(*source_circle_it,
                                                   candidate)) {
          continue;
        }
        add_projected_circle(candidate);
      }
    } else {
      add_projected_loop(source_profile.points);
    }
  } else {
    add_projected_loop(source_profile.points);
  }

  if (record.generated_circle_ids.empty()) {
    for (const auto& inner_loop : source_profile.inner_loops) {
      add_projected_loop(inner_loop);
    }
  }

  sketch_it->sketch_parameters->projections.push_back(std::move(record));

  refresh_linked_extrudes(*document_, *sketch_it);
  document_->selected_feature_id = sketch_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();

  return document_.value();
}

DocumentState DocumentManager::project_edge_into_sketch(
    const std::string& edge_id) {
  require_document();
  const auto sketch_it = require_projection_target(*document_);

  // Reject body edges that belong to an extrude built from this same
  // sketch — same rationale as the face guard above.
  const auto separator = edge_id.find(":edge:");
  if (separator == std::string::npos || separator == 0) {
    throw std::runtime_error("Malformed edge id: " + edge_id);
  }
  const std::string owner_id = edge_id.substr(0, separator);
  for (const auto& feature : document_->feature_history) {
    if (feature.kind != "extrude" || !feature.extrude_parameters.has_value()) {
      continue;
    }
    if (feature.extrude_parameters->sketch_feature_id == sketch_it->id &&
        feature.id == owner_id) {
      throw std::runtime_error(
          "Cannot project an edge from an extrude back onto its own source "
          "sketch");
    }
  }

  // Idempotency: same dedup index that face projection uses.
  for (const auto& projection :
       sketch_it->sketch_parameters->projections) {
    if (projection.source_id == edge_id) {
      return document_.value();
    }
  }

  const auto edge_geometry = compute_edge_geometry(*document_, edge_id);
  if (!edge_geometry.has_value()) {
    throw std::runtime_error(
        "Edge could not be resolved against the current document: " +
        edge_id);
  }

  const auto& frame = sketch_it->sketch_parameters->plane_frame.value();

  push_undo_state();
  clear_redo_stack();

  // See `project_face_into_sketch` for the rationale: we capture the
  // ids of every emitted entity so `refresh_sketch_projections` can
  // re-derive them from the upstream edge on every recompute.
  SketchProjection record;
  record.id =
      "projection-" + std::to_string(next_sketch_projection_id_++);
  record.source_id = edge_id;
  record.source_kind = "edge";

  if (edge_geometry->kind == "line") {
    const auto a = world_to_sketch_local(frame,
                                         edge_geometry->start.x,
                                         edge_geometry->start.y,
                                         edge_geometry->start.z);
    const auto b = world_to_sketch_local(frame,
                                         edge_geometry->end.x,
                                         edge_geometry->end.y,
                                         edge_geometry->end.z);
    // Reject zero-length projections — happens when the edge runs
    // perpendicular to the sketch plane and collapses to a point.
    // v1 leaves these alone rather than generating a degenerate
    // line. Vertex projection covers the "I want a point at the
    // collapsed location" intent.
    const double dx = b.first - a.first;
    const double dy = b.second - a.second;
    if (dx * dx + dy * dy < 1e-12) {
      throw std::runtime_error(
          "Edge projects to a zero-length segment (perpendicular to "
          "sketch plane). Project the endpoint as a vertex instead.");
    }

    const size_t lines_before = sketch_it->sketch_parameters->lines.size();
    polysmith::core::add_sketch_line(*sketch_it,
                                     next_sketch_line_id_++,
                                     a.first,
                                     a.second,
                                     b.first,
                                     b.second);
    // Lock both endpoints — projected geometry is derived.
    for (size_t i = lines_before;
         i < sketch_it->sketch_parameters->lines.size(); ++i) {
      const auto& line = sketch_it->sketch_parameters->lines[i];
      polysmith::core::set_sketch_point_fixed(
          *sketch_it, line.start_point_id, true);
      polysmith::core::set_sketch_point_fixed(
          *sketch_it, line.end_point_id, true);
      record.generated_line_ids.push_back(line.id);
    }
  } else if (edge_geometry->kind == "circle" || edge_geometry->kind == "arc") {
    if (!circle_axis_parallel_to_sketch(frame, edge_geometry->axis)) {
      throw std::runtime_error(
          "Curved edge projects to an ellipse (its plane isn't parallel "
          "to the sketch). Not supported in v1.");
    }

    const auto center =
        world_to_sketch_local(frame,
                              edge_geometry->center.x,
                              edge_geometry->center.y,
                              edge_geometry->center.z);

    if (edge_geometry->kind == "circle") {
      const size_t circles_before =
          sketch_it->sketch_parameters->circles.size();
      polysmith::core::add_sketch_circle(*sketch_it,
                                         next_sketch_circle_id_++,
                                         center.first,
                                         center.second,
                                         edge_geometry->radius);
      for (size_t i = circles_before;
           i < sketch_it->sketch_parameters->circles.size(); ++i) {
        record.generated_circle_ids.push_back(
            sketch_it->sketch_parameters->circles[i].id);
      }
    } else {
      const auto start = world_to_sketch_local(frame,
                                               edge_geometry->start.x,
                                               edge_geometry->start.y,
                                               edge_geometry->start.z);
      const auto end = world_to_sketch_local(frame,
                                             edge_geometry->end.x,
                                             edge_geometry->end.y,
                                             edge_geometry->end.z);
      // Determine winding by checking whether the body axis points
      // in the same direction as the sketch normal. When parallel,
      // the body's natural CCW direction matches the sketch's CCW
      // direction; when anti-parallel, it's flipped.
      const double dot = edge_geometry->axis.x * frame.normal_x +
                         edge_geometry->axis.y * frame.normal_y +
                         edge_geometry->axis.z * frame.normal_z;
      const bool ccw = dot > 0.0;
      const int start_point_index = next_sketch_line_id_++;
      const int end_point_index = next_sketch_line_id_++;
      const size_t arcs_before = sketch_it->sketch_parameters->arcs.size();
      polysmith::core::add_sketch_arc(*sketch_it,
                                      next_sketch_arc_id_++,
                                      start_point_index,
                                      end_point_index,
                                      start.first,
                                      start.second,
                                      end.first,
                                      end.second,
                                      center.first,
                                      center.second,
                                      edge_geometry->radius,
                                      ccw);
      for (size_t i = arcs_before;
           i < sketch_it->sketch_parameters->arcs.size(); ++i) {
        record.generated_arc_ids.push_back(
            sketch_it->sketch_parameters->arcs[i].id);
      }
    }
  } else {
    // "unsupported" — propagate as a controlled error. Caller (the
    // UI) surfaces it as a transient toast.
    throw std::runtime_error(
        "Edge type is not supported by the Project tool yet: " + edge_id);
  }

  sketch_it->sketch_parameters->projections.push_back(std::move(record));

  refresh_linked_extrudes(*document_, *sketch_it);
  document_->selected_feature_id = sketch_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();

  return document_.value();
}

DocumentState DocumentManager::project_vertex_into_sketch(
    const std::string& vertex_id) {
  require_document();
  const auto sketch_it = require_projection_target(*document_);

  // Idempotency: same dedup index that face / edge projection uses.
  for (const auto& projection :
       sketch_it->sketch_parameters->projections) {
    if (projection.source_id == vertex_id) {
      return document_.value();
    }
  }

  const auto position = compute_vertex_position(*document_, vertex_id);
  if (!position.has_value()) {
    throw std::runtime_error(
        "Vertex could not be resolved against the current document: " +
        vertex_id);
  }

  const auto& frame = sketch_it->sketch_parameters->plane_frame.value();
  const auto local = world_to_sketch_local(frame,
                                           position->x,
                                           position->y,
                                           position->z);

  push_undo_state();
  clear_redo_stack();

  const std::string point_id = "projected-point-" +
                               std::to_string(next_sketch_projected_point_id_++);
  sketch_it->sketch_parameters->projected_points.push_back(
      SketchProjectedPoint{
          .id = point_id,
          .source_id = vertex_id,
          .x = local.first,
          .y = local.second,
      });

  // Live-link record. Same shape as face / edge but only points to a
  // single `SketchProjectedPoint`; `refresh_sketch_projections` uses
  // `generated_point_id` to find and patch the cached (x, y).
  SketchProjection projection_record;
  projection_record.id =
      "projection-" + std::to_string(next_sketch_projection_id_++);
  projection_record.source_id = vertex_id;
  projection_record.source_kind = "vertex";
  projection_record.generated_point_id = point_id;
  sketch_it->sketch_parameters->projections.push_back(
      std::move(projection_record));

  // Force the points list to pick up the new projected point in this
  // frame. (`refresh_linked_extrudes` won't run sketch derived state
  // for us — it only refreshes downstream features.)
  refresh_sketch_derived_state(*sketch_it);
  refresh_linked_extrudes(*document_, *sketch_it);
  document_->selected_feature_id = sketch_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();

  return document_.value();
}

DocumentState DocumentManager::create_offset_plane(
    const std::string& source_plane_id, double offset) {
  require_document();

  // The source must be a real plane that exists in the current
  // document. We allow the three origin planes, any earlier
  // construction plane, any sketch profile id, and any planar body
  // face id (face ids of the form "<body_id>:face:<index>"). The
  // validity check is intentionally lighter than `select_face`'s —
  // `resolve_plane_source_frame` returns nullopt for unknown /
  // non-planar / missing sources, so the `frame.has_value()` guard
  // below catches every failure mode in one place.
  const auto frame =
      resolve_plane_source_frame(*document_, source_plane_id);
  if (!frame.has_value()) {
    throw std::runtime_error(
        "Offset plane source not found: " + source_plane_id);
  }

  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(create_construction_plane_feature(
      next_feature_id_++, source_plane_id, offset, frame.value()));
  const std::string feature_id = document_->feature_history.back().id;
  // Select the new plane both as a feature (for the hierarchy /
  // timeline) and as a reference (for the viewport quad highlight
  // and the Sketch button gate). Origin-plane selection only sets
  // selected_reference_id, but construction planes are *both* a
  // feature and a reference, so we keep both in sync.
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = feature_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_midplane(
    const std::string& first_source_id,
    const std::string& second_source_id) {
  require_document();
  const auto first_frame =
      resolve_plane_source_frame(*document_, first_source_id);
  const auto second_frame =
      resolve_plane_source_frame(*document_, second_source_id);
  if (!first_frame.has_value() || !second_frame.has_value()) {
    throw std::runtime_error("Midplane source not found");
  }

  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(create_midplane_feature(
      next_feature_id_++,
      first_source_id,
      second_source_id,
      first_frame.value(),
      second_frame.value()));
  const std::string feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = feature_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_tangent_plane(
    const std::string& source_face_id) {
  require_document();
  const auto tangent_frame =
      resolve_tangent_plane_source_frame(*document_, source_face_id);
  if (!tangent_frame.has_value()) {
    throw std::runtime_error("Tangent plane source face not found");
  }

  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(create_tangent_plane_feature(
      next_feature_id_++, source_face_id, tangent_frame.value()));
  const std::string feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = feature_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_angle_plane(
    const std::string& source_plane_id,
    const std::string& source_axis_id,
    double angle_degrees) {
  require_document();
  const auto source_frame =
      resolve_plane_source_frame(*document_, source_plane_id);
  const auto axis =
      resolve_angle_plane_axis(*document_, source_axis_id);
  if (!source_frame.has_value()) {
    throw std::runtime_error("Angle plane source not found");
  }
  if (!axis.has_value()) {
    throw std::runtime_error("Angle plane axis not found or not linear");
  }

  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(create_angle_plane_feature(
      next_feature_id_++,
      source_plane_id,
      source_axis_id,
      angle_degrees,
      source_frame.value(),
      axis.value()));
  const std::string feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = feature_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_construction_axis(
    const std::string& source_id) {
  require_document();
  const auto axis = resolve_construction_axis_source(*document_, source_id);
  if (!axis.has_value()) {
    throw std::runtime_error(
        "Construction axis source not found or not linear: " + source_id);
  }

  push_undo_state();
  clear_redo_stack();

  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "construction_axis";
  feature.name = "Axis";
  feature.status = "healthy";
  feature.parameters_summary =
      axis->source_kind == "edge" ? "From edge" : "From sketch line";
  feature.construction_axis_parameters = axis.value();
  document_->feature_history.push_back(feature);

  const std::string feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_construction_point(
    const std::string& source_id) {
  require_document();
  const auto point = resolve_construction_point_source(*document_, source_id);
  if (!point.has_value()) {
    throw std::runtime_error(
        "Construction point source not found: " + source_id);
  }

  push_undo_state();
  clear_redo_stack();

  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "construction_point";
  feature.name = "Point";
  feature.status = "healthy";
  feature.parameters_summary =
      point->source_kind == "vertex" ? "From vertex" : "From sketch point";
  feature.construction_point_parameters = point.value();
  document_->feature_history.push_back(feature);

  const std::string feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_hole(
    const std::string& face_id,
    double center_x,
    double center_y,
    double center_z,
    const HoleFeatureParameters& parameters) {
  require_document();
  const auto frame = resolve_plane_source_frame(*document_, face_id);
  if (!frame.has_value()) {
    throw std::runtime_error("Hole requires a planar face source");
  }
  const auto face_marker = face_id.find(":face:");
  if (face_marker == std::string::npos || face_marker == 0) {
    throw std::runtime_error("Hole requires a body face id");
  }

  HoleFeatureParameters next = parameters;
  next.target_body_id = face_id.substr(0, face_marker);
  next.source_face_id = face_id;
  next.plane_frame = frame.value();
  const double dx = center_x - frame->origin_x;
  const double dy = center_y - frame->origin_y;
  const double dz = center_z - frame->origin_z;
  next.center_x = dx * frame->x_axis_x + dy * frame->x_axis_y + dz * frame->x_axis_z;
  next.center_y = dx * frame->y_axis_x + dy * frame->y_axis_y + dz * frame->y_axis_z;
  next.is_pending = true;
  if (next.thread_enabled && next.thread_depth <= 0.0) {
    next.thread_depth = next.depth;
  }

  push_undo_state();
  clear_redo_stack();

  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "hole";
  feature.name = "Hole";
  feature.status = "healthy";
  feature.parameters_summary = next.hole_type + " · " +
                               std::to_string(next.diameter) + " mm";
  feature.hole_parameters = next;
  document_->feature_history.push_back(feature);
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_hole_parameters(
    const std::string& feature_id,
    const HoleFeatureParameters& parameters) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "hole" ||
      !feature_it->hole_parameters.has_value()) {
    throw std::runtime_error("update_hole_parameters requires a hole feature");
  }
  HoleFeatureParameters next = parameters;
  next.target_body_id = feature_it->hole_parameters->target_body_id;
  next.source_face_id = feature_it->hole_parameters->source_face_id;
  next.plane_frame = feature_it->hole_parameters->plane_frame;
  next.center_x = feature_it->hole_parameters->center_x;
  next.center_y = feature_it->hole_parameters->center_y;
  next.is_pending = feature_it->hole_parameters->is_pending;
  feature_it->hole_parameters = next;
  feature_it->parameters_summary = next.hole_type + " · " +
                                   std::to_string(next.diameter) + " mm";
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::confirm_hole(const std::string& feature_id) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "hole" ||
      !feature_it->hole_parameters.has_value()) {
    throw std::runtime_error("confirm_hole requires a hole feature");
  }
  feature_it->hole_parameters->is_pending = false;
  bump_geometry_revision();
  return document_.value();
}

namespace {

std::vector<double> make_helix_points(const ConstructionAxisFeatureParameters& axis,
                                      const HelixFeatureParameters& params) {
  const double ax = axis.end_x - axis.start_x;
  const double ay = axis.end_y - axis.start_y;
  const double az = axis.end_z - axis.start_z;
  const double length = std::sqrt(ax * ax + ay * ay + az * az);
  if (length <= 1.0e-9 || params.radius <= 0.0 || params.pitch <= 0.0 ||
      params.height <= 0.0) {
    return {};
  }
  const std::array<double, 3> dir{ax / length, ay / length, az / length};
  std::array<double, 3> seed =
      std::abs(dir[0]) < 0.9 ? std::array<double, 3>{1.0, 0.0, 0.0}
                             : std::array<double, 3>{0.0, 1.0, 0.0};
  std::array<double, 3> u{
      dir[1] * seed[2] - dir[2] * seed[1],
      dir[2] * seed[0] - dir[0] * seed[2],
      dir[0] * seed[1] - dir[1] * seed[0],
  };
  const double u_len = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
  if (u_len <= 1.0e-9) {
    return {};
  }
  u = {u[0] / u_len, u[1] / u_len, u[2] / u_len};
  const std::array<double, 3> v{
      dir[1] * u[2] - dir[2] * u[1],
      dir[2] * u[0] - dir[0] * u[2],
      dir[0] * u[1] - dir[1] * u[0],
  };
  const double turns = params.height / params.pitch;
  const int samples = std::max(24, static_cast<int>(std::ceil(turns * 48.0)));
  const double handed = params.handedness == "left" ? -1.0 : 1.0;
  const double start = params.start_angle_degrees * kPi / 180.0;
  std::vector<double> points;
  points.reserve(static_cast<size_t>(samples + 1) * 3);
  for (int i = 0; i <= samples; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(samples);
    const double along = params.height * t;
    const double angle = start + handed * 2.0 * kPi * turns * t;
    const double radial_u = std::cos(angle) * params.radius;
    const double radial_v = std::sin(angle) * params.radius;
    points.push_back(axis.start_x + dir[0] * along + u[0] * radial_u + v[0] * radial_v);
    points.push_back(axis.start_y + dir[1] * along + u[1] * radial_u + v[1] * radial_v);
    points.push_back(axis.start_z + dir[2] * along + u[2] * radial_u + v[2] * radial_v);
  }
  return points;
}

}  // namespace

DocumentState DocumentManager::create_helix(
    const std::string& axis_source_id,
    const HelixFeatureParameters& parameters) {
  require_document();
  const auto axis = resolve_construction_axis_source(*document_, axis_source_id);
  if (!axis.has_value()) {
    throw std::runtime_error("Helix axis source not found or not linear");
  }
  HelixFeatureParameters next = parameters;
  next.axis_source_id = axis_source_id;
  next.axis_start_x = axis->start_x;
  next.axis_start_y = axis->start_y;
  next.axis_start_z = axis->start_z;
  next.axis_end_x = axis->end_x;
  next.axis_end_y = axis->end_y;
  next.axis_end_z = axis->end_z;
  next.turns = next.pitch > 0.0 ? next.height / next.pitch : 0.0;
  next.points = make_helix_points(axis.value(), next);
  if (next.points.empty()) {
    throw std::runtime_error("Helix parameters are invalid");
  }

  push_undo_state();
  clear_redo_stack();
  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "helix";
  feature.name = "Helix";
  feature.status = "healthy";
  feature.parameters_summary = std::to_string(next.pitch) + " mm pitch";
  feature.helix_parameters = next;
  document_->feature_history.push_back(feature);
  document_->selected_feature_id = document_->feature_history.back().id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_helix_parameters(
    const std::string& feature_id,
    const HelixFeatureParameters& parameters) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "helix" ||
      !feature_it->helix_parameters.has_value()) {
    throw std::runtime_error("update_helix_parameters requires a helix feature");
  }
  const auto axis = resolve_construction_axis_source(
      *document_, feature_it->helix_parameters->axis_source_id);
  if (!axis.has_value()) {
    feature_it->dependency_broken = true;
    feature_it->dependency_warning = "Helix axis source is no longer available.";
    return document_.value();
  }
  HelixFeatureParameters next = parameters;
  next.axis_source_id = feature_it->helix_parameters->axis_source_id;
  next.axis_start_x = axis->start_x;
  next.axis_start_y = axis->start_y;
  next.axis_start_z = axis->start_z;
  next.axis_end_x = axis->end_x;
  next.axis_end_y = axis->end_y;
  next.axis_end_z = axis->end_z;
  next.turns = next.pitch > 0.0 ? next.height / next.pitch : 0.0;
  next.points = make_helix_points(axis.value(), next);
  feature_it->helix_parameters = next;
  feature_it->dependency_broken = next.points.empty();
  feature_it->dependency_warning =
      next.points.empty() ? "Helix parameters are invalid." : "";
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_thread(
    const ThreadFeatureParameters& parameters) {
  require_document();
  push_undo_state();
  clear_redo_stack();
  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "thread";
  feature.name = "Thread";
  feature.status = "healthy";
  feature.parameters_summary = parameters.size.empty() ? "Custom thread" : parameters.size;
  feature.thread_parameters = parameters;
  feature.thread_parameters->is_pending = true;
  document_->feature_history.push_back(feature);
  document_->selected_feature_id = document_->feature_history.back().id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_thread_parameters(
    const std::string& feature_id,
    const ThreadFeatureParameters& parameters) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "thread" ||
      !feature_it->thread_parameters.has_value()) {
    throw std::runtime_error("update_thread_parameters requires a thread feature");
  }
  feature_it->thread_parameters = parameters;
  feature_it->parameters_summary = parameters.size.empty() ? "Custom thread" : parameters.size;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::confirm_thread(const std::string& feature_id) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "thread" ||
      !feature_it->thread_parameters.has_value()) {
    throw std::runtime_error("confirm_thread requires a thread feature");
  }
  feature_it->thread_parameters->is_pending = false;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_fastener(
    const FastenerFeatureParameters& parameters) {
  require_document();
  push_undo_state();
  clear_redo_stack();
  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "fastener";
  feature.name = "Fastener";
  feature.status = "healthy";
  feature.parameters_summary = parameters.size + " · " + std::to_string(parameters.length) + " mm";
  feature.fastener_parameters = parameters;
  document_->feature_history.push_back(feature);
  document_->selected_feature_id = document_->feature_history.back().id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_fastener_parameters(
    const std::string& feature_id,
    const FastenerFeatureParameters& parameters) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "fastener" ||
      !feature_it->fastener_parameters.has_value()) {
    throw std::runtime_error("update_fastener_parameters requires a fastener feature");
  }
  feature_it->fastener_parameters = parameters;
  feature_it->parameters_summary = parameters.size + " · " + std::to_string(parameters.length) + " mm";
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_move(
    const std::string& target_body_id,
    const MoveFeatureParameters& parameters) {
  require_document();
  if (!compiled_body_exists(*document_, target_body_id)) {
    throw std::runtime_error("Move target body not found: " + target_body_id);
  }

  MoveFeatureParameters next = parameters;
  next.target_body_id = target_body_id;
  next.is_pending = true;

  push_undo_state();
  clear_redo_stack();

  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "move";
  feature.name = "Move";
  feature.status = "healthy";
  feature.parameters_summary = make_move_parameters_summary(next);
  feature.move_parameters = next;
  document_->feature_history.push_back(feature);
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_move_parameters(
    const std::string& feature_id,
    const MoveFeatureParameters& parameters) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "move" ||
      !feature_it->move_parameters.has_value()) {
    throw std::runtime_error("update_move_parameters requires a move feature");
  }

  MoveFeatureParameters next = parameters;
  next.target_body_id = feature_it->move_parameters->target_body_id;
  next.is_pending = feature_it->move_parameters->is_pending;
  feature_it->move_parameters = next;
  feature_it->parameters_summary = make_move_parameters_summary(next);
  mark_feature_healthy(*feature_it);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::confirm_move(const std::string& feature_id) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "move" ||
      !feature_it->move_parameters.has_value()) {
    throw std::runtime_error("confirm_move requires a move feature");
  }
  feature_it->move_parameters->is_pending = false;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_body_copy(
    const std::string& source_body_id,
    const std::string& copy_mode) {
  require_document();

  const std::string normalized_mode =
      copy_mode == "standalone" ? "standalone" : "linked";
  const CompiledBodies compiled = compile_bodies(*document_);
  const auto body_it = std::find_if(
      compiled.bodies.begin(),
      compiled.bodies.end(),
      [&](const CompiledBody& body) { return body.id == source_body_id; });
  if (body_it == compiled.bodies.end()) {
    throw std::runtime_error("Copy source body not found: " + source_body_id);
  }

  push_undo_state();
  clear_redo_stack();

  BodyCopyFeatureParameters params{};
  params.source_body_id = source_body_id;
  params.copy_mode = normalized_mode;
  params.source_body_name = feature_name_by_id(*document_, source_body_id);
  write_copy_frame(params, body_it->local_frame);
  if (normalized_mode == "standalone") {
    params.serialized_shape = serialize_shape_snapshot(body_it->shape);
  }

  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "body_copy";
  feature.name = normalized_mode == "standalone" ? "Independent Copy" : "Linked Copy";
  feature.status = "healthy";
  feature.parameters_summary =
      normalized_mode == "standalone" ? "Independent body copy" : "Linked body copy";
  feature.body_copy_parameters = params;
  document_->feature_history.push_back(feature);
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::unlink_body_copy(const std::string& feature_id) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "body_copy" ||
      !feature_it->body_copy_parameters.has_value()) {
    throw std::runtime_error("unlink_body_copy requires a body copy feature");
  }
  if (feature_it->body_copy_parameters->copy_mode != "linked") {
    throw std::runtime_error("Body copy is already independent");
  }

  DocumentState prefix = *document_;
  const auto feature_index =
      static_cast<std::size_t>(std::distance(document_->feature_history.begin(),
                                             feature_it));
  prefix.feature_history.resize(feature_index + 1);
  const CompiledBodies compiled = compile_bodies(prefix);
  const auto body_it = std::find_if(
      compiled.bodies.begin(),
      compiled.bodies.end(),
      [&](const CompiledBody& body) { return body.id == feature_id; });
  if (body_it == compiled.bodies.end() || body_it->shape.IsNull()) {
    throw std::runtime_error("Could not resolve linked copy body for unlink");
  }

  BodyCopyFeatureParameters next = feature_it->body_copy_parameters.value();
  next.copy_mode = "standalone";
  next.serialized_shape = serialize_shape_snapshot(body_it->shape);
  write_copy_frame(next, body_it->local_frame);

  push_undo_state();
  clear_redo_stack();
  feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  feature_it->body_copy_parameters = next;
  feature_it->name = "Independent Copy";
  feature_it->parameters_summary = "Independent body copy";
  mark_feature_healthy(*feature_it);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_body_color(const std::string& body_id,
                                              const std::string& color) {
  require_document();
  if (!is_valid_appearance_color(color)) {
    throw std::runtime_error("Body color must be an opaque #RRGGBB value");
  }
  if (!compiled_body_exists(*document_, body_id)) {
    throw std::runtime_error("Body not found: " + body_id);
  }

  push_undo_state();
  clear_redo_stack();
  auto& overrides = document_->appearance.body_colors;
  const auto existing = std::find_if(
      overrides.begin(),
      overrides.end(),
      [&](const BodyAppearanceOverride& entry) {
        return entry.body_id == body_id;
      });
  if (existing == overrides.end()) {
    overrides.push_back(BodyAppearanceOverride{.body_id = body_id,
                                               .color = color});
  } else {
    existing->color = color;
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_face_color(const std::string& face_id,
                                              const std::string& color) {
  require_document();
  if (!is_valid_appearance_color(color)) {
    throw std::runtime_error("Face color must be an opaque #RRGGBB value");
  }
  const auto owner_id = owner_body_id_from_face_id(face_id);
  const auto face = resolve_face_for_appearance(*document_, face_id);
  if (!owner_id.has_value() || !compiled_body_exists(*document_, owner_id.value())) {
    throw std::runtime_error("Face not found: " + face_id);
  }
  const std::string signature = face.has_value()
                                    ? appearance_face_signature(face.value())
                                    : "semantic:" + face_id;
  if (signature.empty()) {
    throw std::runtime_error("Face appearance signature could not be resolved");
  }

  push_undo_state();
  clear_redo_stack();
  auto& overrides = document_->appearance.face_colors;
  const auto existing = std::find_if(
      overrides.begin(),
      overrides.end(),
      [&](const FaceAppearanceOverride& entry) {
        return entry.face_id == face_id;
      });
  if (existing == overrides.end()) {
    overrides.push_back(FaceAppearanceOverride{
        .face_id = face_id,
        .owner_body_id = owner_id.value(),
        .signature = signature,
        .color = color,
    });
  } else {
    existing->owner_body_id = owner_id.value();
    existing->signature = signature;
    existing->color = color;
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::clear_body_color(const std::string& body_id) {
  require_document();
  auto& overrides = document_->appearance.body_colors;
  const auto existing = std::find_if(
      overrides.begin(),
      overrides.end(),
      [&](const BodyAppearanceOverride& entry) {
        return entry.body_id == body_id;
      });
  if (existing == overrides.end()) {
    return document_.value();
  }
  push_undo_state();
  clear_redo_stack();
  overrides.erase(std::remove_if(overrides.begin(),
                                 overrides.end(),
                                 [&](const BodyAppearanceOverride& entry) {
                                   return entry.body_id == body_id;
                                 }),
                  overrides.end());
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::clear_face_color(const std::string& face_id) {
  require_document();
  auto& overrides = document_->appearance.face_colors;
  const auto existing = std::find_if(
      overrides.begin(),
      overrides.end(),
      [&](const FaceAppearanceOverride& entry) {
        return entry.face_id == face_id;
      });
  if (existing == overrides.end()) {
    return document_.value();
  }
  push_undo_state();
  clear_redo_stack();
  overrides.erase(std::remove_if(overrides.begin(),
                                 overrides.end(),
                                 [&](const FaceAppearanceOverride& entry) {
                                   return entry.face_id == face_id;
                                 }),
                  overrides.end());
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::clear_appearance_overrides() {
  require_document();
  if (document_->appearance.body_colors.empty() &&
      document_->appearance.face_colors.empty()) {
    return document_.value();
  }
  push_undo_state();
  clear_redo_stack();
  document_->appearance.body_colors.clear();
  document_->appearance.face_colors.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_offset_plane(
    const std::string& feature_id, double offset) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "construction_plane" ||
      !feature_it->construction_plane_parameters.has_value()) {
    throw std::runtime_error(
        "update_offset_plane requires a construction_plane feature: " +
        feature_id);
  }
  if (feature_it->construction_plane_parameters->plane_type != "offset") {
    throw std::runtime_error(
        "update_offset_plane requires an offset construction plane: " +
        feature_id);
  }

  // Resolve the source frame from the rest of the document (everything
  // before this feature in feature_history is already up to date by
  // the time we get here, and the source must come from earlier in
  // history per `is_selectable_plane_reference`).
  const auto& params = feature_it->construction_plane_parameters.value();
  const auto frame =
      resolve_plane_source_frame(*document_, params.source_plane_id);
  if (!frame.has_value()) {
    throw std::runtime_error(
        "Offset plane source could not be resolved: " + params.source_plane_id);
  }

  // No push_undo_state here: this is a live-preview update on the
  // freshly-created construction plane whose own create_offset_plane
  // already pushed an undo step. Pushing here would make a panel
  // session of N debounced offset edits collapse to N user-visible
  // undo steps when the user really wants Cancel to revert the
  // entire session. Mirrors the same comment + rationale in
  // `update_fillet_edges` / `update_fillet_radius`.
  clear_redo_stack();
  polysmith::core::update_construction_plane(*feature_it, offset, frame.value());
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_angle_plane(
    const std::string& feature_id, double angle_degrees) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "construction_plane" ||
      !feature_it->construction_plane_parameters.has_value()) {
    throw std::runtime_error(
        "update_angle_plane requires a construction_plane feature: " +
        feature_id);
  }
  if (feature_it->construction_plane_parameters->plane_type != "angle") {
    throw std::runtime_error(
        "update_angle_plane requires an angle construction plane: " +
        feature_id);
  }

  const auto& params = feature_it->construction_plane_parameters.value();
  const auto source_frame =
      resolve_plane_source_frame(*document_, params.source_plane_id);
  const auto axis =
      resolve_angle_plane_axis(*document_, params.source_axis_id);
  if (!source_frame.has_value()) {
    throw std::runtime_error(
        "Angle plane source could not be resolved: " + params.source_plane_id);
  }
  if (!axis.has_value()) {
    throw std::runtime_error(
        "Angle plane axis could not be resolved: " + params.source_axis_id);
  }

  clear_redo_stack();
  polysmith::core::update_angle_plane(
      *feature_it, angle_degrees, source_frame.value(), axis.value());
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_parameter(const std::string& name,
                                            const std::string& expression,
                                            const std::string& kind) {
  require_document();

  if (name.empty()) {
    throw std::runtime_error("Parameter name cannot be empty");
  }

  // Reject duplicates
  for (const auto& p : document_->parameters) {
    if (p.name == name) {
      throw std::runtime_error("Parameter '" + name + "' already exists");
    }
  }

  push_undo_state();
  clear_redo_stack();

  ParameterEntry entry;
  entry.name = name;
  entry.expression = expression;
  entry.kind = kind;
  document_->parameters.push_back(entry);

  // Re-evaluate all parameters and refresh dimension expressions
  reify_parameters(document_->parameters);
  for (auto& feat : document_->feature_history) {
    reify_dimension_expressions(feat, document_->parameters);
  }
  refresh_history_dependencies(document_.value());
  bump_geometry_revision();

  return document_.value();
}

DocumentState DocumentManager::update_parameter(const std::string& name,
                                                 const std::string& expression,
                                                 const std::string& kind) {
  require_document();

  if (name.empty()) {
    throw std::runtime_error("Parameter name cannot be empty");
  }

  ParameterEntry* target = nullptr;
  for (auto& p : document_->parameters) {
    if (p.name == name) {
      target = &p;
      break;
    }
  }

  if (!target) {
    throw std::runtime_error("Parameter '" + name + "' not found");
  }

  push_undo_state();
  clear_redo_stack();

  target->expression = expression;
  target->kind = kind;

  // Re-evaluate all parameters and refresh dimension expressions
  reify_parameters(document_->parameters);
  for (auto& feat : document_->feature_history) {
    reify_dimension_expressions(feat, document_->parameters);
  }
  refresh_history_dependencies(document_.value());
  bump_geometry_revision();

  return document_.value();
}

DocumentState DocumentManager::delete_parameter(const std::string& name) {
  require_document();

  if (name.empty()) {
    throw std::runtime_error("Parameter name cannot be empty");
  }

  auto it = document_->parameters.begin();
  for (; it != document_->parameters.end(); ++it) {
    if (it->name == name) {
      break;
    }
  }

  if (it == document_->parameters.end()) {
    throw std::runtime_error("Parameter '" + name + "' not found");
  }

  push_undo_state();
  clear_redo_stack();

  document_->parameters.erase(it);

  // Re-evaluate remaining parameters (those referencing the deleted
  // one will now have errors) and refresh dimension expressions
  reify_parameters(document_->parameters);
  for (auto& feat : document_->feature_history) {
    reify_dimension_expressions(feat, document_->parameters);
  }
  refresh_history_dependencies(document_.value());
  bump_geometry_revision();

  return document_.value();
}

DocumentState DocumentManager::update_selection_filter(
    const SelectionFilter& filter) {
  require_document();

  push_undo_state();
  clear_redo_stack();

  document_->selection_filter = filter;

  return document_.value();
}

DocumentState DocumentManager::clear_selection() {
  require_document();

  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

DocumentState DocumentManager::create_image_import(
    const ImageImportFeatureParameters& parameters,
    const std::string& source_path) {
  require_document();
  if (source_path.empty()) {
    throw std::runtime_error("Image import source path cannot be empty");
  }
  push_undo_state();
  clear_redo_stack();

  const std::string asset_id = import_asset_id_from_index(next_import_asset_id_++);
  const std::string extension = extension_from_path(source_path);
  const std::string asset_dir = document_file_path_.has_value()
                                    ? sidecar_assets_dir_for_document(
                                          document_file_path_.value()).string()
                                    : temp_assets_dir_for_document(document_->id).string();
  ImageImportFeatureParameters params = parameters;
  params.asset_id = asset_id;
  params.file_name = file_name_from_path(source_path);
  params.relative_asset_path = relative_import_asset_path(asset_id, extension);
  params.asset_path = copy_import_asset(source_path, asset_dir, asset_id);
  params.media_type = media_type_for_extension(extension, "image/*");
  params.is_pending = true;

  FeatureEntry feature;
  feature.id = feature_id_from_index(next_feature_id_++);
  feature.kind = "image_import";
  feature.name = params.file_name.empty() ? "Reference Image" : params.file_name;
  feature.status = "ok";
  feature.parameters_summary = import_summary(params);
  feature.image_import_parameters = params;
  document_->feature_history.push_back(feature);
  document_->selected_feature_id = feature.id;
  clear_import_selection(*document_);
  document_->revision++;
  return document_.value();
}

DocumentState DocumentManager::update_image_import(
    const std::string& feature_id,
    const ImageImportFeatureParameters& parameters) {
  require_document();
  FeatureEntry* feature = find_feature(*document_, feature_id);
  if (feature == nullptr || feature->kind != "image_import" ||
      !feature->image_import_parameters.has_value()) {
    throw std::runtime_error("Image import feature not found: " + feature_id);
  }
  auto next = parameters;
  next.asset_id = feature->image_import_parameters->asset_id;
  next.asset_path = feature->image_import_parameters->asset_path;
  next.relative_asset_path = feature->image_import_parameters->relative_asset_path;
  next.file_name = feature->image_import_parameters->file_name;
  next.media_type = feature->image_import_parameters->media_type;
  next.source_width = feature->image_import_parameters->source_width;
  next.source_height = feature->image_import_parameters->source_height;
  next.warnings = feature->image_import_parameters->warnings;
  feature->image_import_parameters = next;
  feature->parameters_summary = import_summary(next);
  document_->selected_feature_id = feature_id;
  document_->revision++;
  return document_.value();
}

DocumentState DocumentManager::confirm_image_import(
    const std::string& feature_id) {
  require_document();
  FeatureEntry* feature = find_feature(*document_, feature_id);
  if (feature == nullptr || feature->kind != "image_import" ||
      !feature->image_import_parameters.has_value()) {
    throw std::runtime_error("Image import feature not found: " + feature_id);
  }
  feature->image_import_parameters->is_pending = false;
  feature->parameters_summary = import_summary(*feature->image_import_parameters);
  document_->selected_feature_id = feature_id;
  document_->revision++;
  return document_.value();
}

DocumentState DocumentManager::cancel_image_import(
    const std::string& feature_id) {
  require_document();
  const FeatureEntry* feature = find_feature(*document_, feature_id);
  std::optional<ImageImportFeatureParameters> params;
  if (feature != nullptr && feature->image_import_parameters.has_value()) {
    params = feature->image_import_parameters.value();
  }
  remove_feature_by_id(*document_, feature_id);
  if (params.has_value()) {
    remove_import_asset_if_unreferenced(*document_, params.value());
  }
  document_->revision++;
  return document_.value();
}

DocumentState DocumentManager::create_svg_import(
    const SvgImportFeatureParameters& parameters,
    const std::string& source_path) {
  require_document();
  if (source_path.empty()) {
    throw std::runtime_error("SVG import source path cannot be empty");
  }

  SvgImportFeatureParameters params = parameters;
  params.file_name = file_name_from_path(source_path);
  params.asset_path = source_path;
  params.media_type = "image/svg+xml";
  params.is_pending = false;
  log_info("svg_import", "Starting SVG import: " + params.file_name);
  const auto svg = flatten_svg_file(source_path);
  log_info("svg_import",
           "Parsed SVG import: " + std::to_string(svg.layers.size()) +
               " layer(s), " + std::to_string(svg.segments.size()) +
               " segment(s)");
  for (const auto& warning : svg.warnings) {
    log_warn("svg_import", warning);
  }
  if (svg.segments.empty()) {
    throw std::runtime_error("SVG did not contain usable vector geometry");
  }
  push_undo_state();
  clear_redo_stack();

  log_info("svg_import", "Creating sketches for SVG import");
  const auto created_ids =
      append_svg_sketches_to_document(*document_,
                                      next_feature_id_,
                                      next_sketch_line_id_,
                                      params,
                                      svg);
  log_info("svg_import",
           "Created " + std::to_string(created_ids.size()) +
               " sketch(es) from SVG import");
  document_->selected_feature_id = created_ids.back();
  clear_import_selection(*document_);
  document_->revision++;
  return document_.value();
}

DocumentState DocumentManager::update_svg_import(
    const std::string& feature_id,
    const SvgImportFeatureParameters& parameters) {
  require_document();
  FeatureEntry* feature = find_feature(*document_, feature_id);
  if (feature == nullptr || feature->kind != "svg_import" ||
      !feature->svg_import_parameters.has_value()) {
    throw std::runtime_error("SVG import feature not found: " + feature_id);
  }
  auto next = parameters;
  next.asset_id = feature->svg_import_parameters->asset_id;
  next.asset_path = feature->svg_import_parameters->asset_path;
  next.relative_asset_path = feature->svg_import_parameters->relative_asset_path;
  next.file_name = feature->svg_import_parameters->file_name;
  next.media_type = feature->svg_import_parameters->media_type;
  next.source_width = feature->svg_import_parameters->source_width;
  next.source_height = feature->svg_import_parameters->source_height;
  next.warnings = feature->svg_import_parameters->warnings;
  feature->svg_import_parameters = next;
  feature->parameters_summary = import_summary(next);
  document_->selected_feature_id = feature_id;
  document_->revision++;
  return document_.value();
}

DocumentState DocumentManager::confirm_svg_import(
    const std::string& feature_id) {
  require_document();
  FeatureEntry* feature = find_feature(*document_, feature_id);
  if (feature == nullptr || feature->kind != "svg_import" ||
      !feature->svg_import_parameters.has_value()) {
    throw std::runtime_error("SVG import feature not found: " + feature_id);
  }
  const SvgImportFeatureParameters params = *feature->svg_import_parameters;
  const auto svg = flatten_svg_file(params.asset_path);
  if (svg.segments.empty()) {
    throw std::runtime_error("SVG did not contain usable vector geometry");
  }

  remove_feature_by_id(*document_, feature_id);
  const auto created_ids =
      append_svg_sketches_to_document(*document_,
                                      next_feature_id_,
                                      next_sketch_line_id_,
                                      params,
                                      svg);
  document_->selected_feature_id = created_ids.back();
  document_->revision++;
  return document_.value();
}

DocumentState DocumentManager::cancel_svg_import(
    const std::string& feature_id) {
  require_document();
  const FeatureEntry* feature = find_feature(*document_, feature_id);
  std::optional<SvgImportFeatureParameters> params;
  if (feature != nullptr && feature->svg_import_parameters.has_value()) {
    params = feature->svg_import_parameters.value();
  }
  remove_feature_by_id(*document_, feature_id);
  if (params.has_value()) {
    remove_import_asset_if_unreferenced(*document_, params.value());
  }
  document_->revision++;
  return document_.value();
}

ExportResult DocumentManager::export_document_as_step(
    const std::string& file_path) const {
  require_document();
  return polysmith::core::export_document_as_step(document_.value(), file_path);
}

ExportResult DocumentManager::export_document_as_stl(
    const std::string& file_path) const {
  require_document();
  return polysmith::core::export_document_as_stl(document_.value(), file_path);
}

ExportResult DocumentManager::export_body_as_stl(
    const std::string& file_path,
    const std::string& body_id) const {
  require_document();
  return polysmith::core::export_body_as_stl(document_.value(),
                                             file_path,
                                             body_id);
}

namespace {

// Extract the trailing positive integer from id strings like "feature-12",
// "doc-3", "line-7". Returns 0 if no trailing integer is present.
int trailing_integer(const std::string& id) {
  size_t i = id.size();
  while (i > 0 && id[i - 1] >= '0' && id[i - 1] <= '9') {
    --i;
  }
  if (i == id.size()) {
    return 0;
  }
  try {
    return std::stoi(id.substr(i));
  } catch (...) {
    return 0;
  }
}

}  // namespace

void DocumentManager::save_document_to_path(const std::string& file_path) {
  require_document();
  if (file_path.empty()) {
    throw std::runtime_error("Save path cannot be empty");
  }

  const fs::path asset_dir = sidecar_assets_dir_for_document(file_path);
  fs::create_directories(asset_dir);
  for (auto& feature : document_->feature_history) {
    ImportPlacementParameters* params = nullptr;
    if (feature.image_import_parameters.has_value()) {
      params = &feature.image_import_parameters.value();
    } else if (feature.svg_import_parameters.has_value()) {
      params = &feature.svg_import_parameters.value();
    }
    if (params == nullptr || params->asset_path.empty()) {
      continue;
    }
    const std::string extension = extension_from_path(params->asset_path);
    if (params->relative_asset_path.empty()) {
      params->relative_asset_path =
          relative_import_asset_path(params->asset_id, extension);
    }
    const fs::path target = resolve_asset_path(file_path, params->relative_asset_path);
    fs::create_directories(target.parent_path());
    if (fs::exists(params->asset_path) &&
        fs::weakly_canonical(fs::path(params->asset_path)) !=
            fs::weakly_canonical(target)) {
      fs::copy_file(params->asset_path,
                    target,
                    fs::copy_options::overwrite_existing);
    }
    params->asset_path = target.string();
  }
  document_file_path_ = file_path;

  const nlohmann::json payload = polysmith::protocol::to_payload(document_.value());

  std::ofstream stream(file_path);
  if (!stream.is_open()) {
    throw std::runtime_error("Failed to open file for writing: " + file_path);
  }
  stream << payload.dump(2);
  if (!stream.good()) {
    throw std::runtime_error("Failed to write document to: " + file_path);
  }
}

DocumentState DocumentManager::load_document_from_path(
    const std::string& file_path) {
  if (file_path.empty()) {
    throw std::runtime_error("Load path cannot be empty");
  }

  std::ifstream stream(file_path);
  if (!stream.is_open()) {
    throw std::runtime_error("Failed to open file for reading: " + file_path);
  }
  std::stringstream buffer;
  buffer << stream.rdbuf();
  if (!stream.good() && !stream.eof()) {
    throw std::runtime_error("Failed to read document from: " + file_path);
  }

  nlohmann::json payload;
  try {
    payload = nlohmann::json::parse(buffer.str());
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("Document parse error: ") +
                             error.what());
  }

  DocumentState loaded = polysmith::protocol::document_from_payload(payload);
  document_file_path_ = file_path;
  for (auto& feature : loaded.feature_history) {
    ImportPlacementParameters* params = nullptr;
    if (feature.image_import_parameters.has_value()) {
      params = &feature.image_import_parameters.value();
    } else if (feature.svg_import_parameters.has_value()) {
      params = &feature.svg_import_parameters.value();
    }
    if (params == nullptr) {
      continue;
    }
    next_import_asset_id_ =
        std::max(next_import_asset_id_, trailing_integer(params->asset_id) + 1);
    if (!params->relative_asset_path.empty()) {
      params->asset_path =
          resolve_asset_path(file_path, params->relative_asset_path).string();
    }
    params->warnings.erase(
        std::remove(params->warnings.begin(),
                    params->warnings.end(),
                    "Imported asset file is missing"),
        params->warnings.end());
    if (params->asset_path.empty() || !fs::exists(params->asset_path)) {
      params->warnings.push_back("Imported asset file is missing");
    }
  }

  // Replace the live document with the loaded one. Clear undo/redo, since
  // their previous contents reference a different document timeline.
  document_ = loaded;
  undo_stack_.clear();
  redo_stack_.clear();
  if (document_count_ == 0) {
    document_count_ = 1;
  }

  // Restore id counters so subsequent feature/sketch additions don't
  // collide with ids already present in the loaded document. Walk the
  // feature history and bump every counter to one past the highest seen
  // value.
  next_document_id_ = std::max(next_document_id_, trailing_integer(loaded.id) + 1);

  for (const auto& feature : loaded.feature_history) {
    next_feature_id_ = std::max(next_feature_id_, trailing_integer(feature.id) + 1);

    if (!feature.sketch_parameters.has_value()) {
      continue;
    }
    const auto& sketch = feature.sketch_parameters.value();
    for (const auto& line : sketch.lines) {
      next_sketch_line_id_ =
          std::max(next_sketch_line_id_, trailing_integer(line.id) + 1);
    }
    for (const auto& circle : sketch.circles) {
      next_sketch_circle_id_ =
          std::max(next_sketch_circle_id_, trailing_integer(circle.id) + 1);
    }
    for (const auto& arc : sketch.arcs) {
      next_sketch_arc_id_ =
          std::max(next_sketch_arc_id_, trailing_integer(arc.id) + 1);
    }
    for (const auto& fillet : sketch.fillets) {
      next_sketch_fillet_id_ =
          std::max(next_sketch_fillet_id_, trailing_integer(fillet.id) + 1);
    }
    for (const auto& projected : sketch.projected_points) {
      next_sketch_projected_point_id_ = std::max(
          next_sketch_projected_point_id_,
          trailing_integer(projected.id) + 1);
    }
    for (const auto& projection : sketch.projections) {
      next_sketch_projection_id_ = std::max(
          next_sketch_projection_id_,
          trailing_integer(projection.id) + 1);
    }
  }

  if (document_) {
    bump_geometry_revision();
  }

  return document_.value();
}

std::optional<DocumentState> DocumentManager::get_document() const {
  return document_;
}

SessionState DocumentManager::get_session_state() const {
  return SessionState{
      .document_count = document_count_,
      .active_document_id =
          document_.has_value() ? std::make_optional(document_->id) : std::nullopt,
      .can_undo = !undo_stack_.empty(),
      .can_redo = !redo_stack_.empty(),
  };
}

}  // namespace polysmith::core
