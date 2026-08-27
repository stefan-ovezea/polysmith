      {"sketch_parameters",
       feature.sketch_parameters.has_value()
           ? json{
                 {"plane_id", feature.sketch_parameters->plane_id},
                 {"next_vertex_index",
                  feature.sketch_parameters->next_vertex_index},
                 {"plane_frame",
                  feature.sketch_parameters->plane_frame.has_value()
                      ? json{
                            {"origin",
                             {
                                 {"x", feature.sketch_parameters->plane_frame->origin_x},
                                 {"y", feature.sketch_parameters->plane_frame->origin_y},
                                 {"z", feature.sketch_parameters->plane_frame->origin_z},
                             }},
                            {"x_axis",
                             {
                                 {"x", feature.sketch_parameters->plane_frame->x_axis_x},
                                 {"y", feature.sketch_parameters->plane_frame->x_axis_y},
                                 {"z", feature.sketch_parameters->plane_frame->x_axis_z},
                             }},
                            {"y_axis",
                             {
                                 {"x", feature.sketch_parameters->plane_frame->y_axis_x},
                                 {"y", feature.sketch_parameters->plane_frame->y_axis_y},
                                 {"z", feature.sketch_parameters->plane_frame->y_axis_z},
                             }},
                            {"normal",
                             {
                                 {"x", feature.sketch_parameters->plane_frame->normal_x},
                                 {"y", feature.sketch_parameters->plane_frame->normal_y},
                                 {"z", feature.sketch_parameters->plane_frame->normal_z},
                             }},
                        }
                      : json(nullptr)},
#include "protocol/impl/feature_to_payload_sketch_geometry_entries.inc"
#include "protocol/impl/feature_to_payload_sketch_chamfer_entries.inc"
#include "protocol/impl/feature_to_payload_sketch_slot_entries.inc"
#include "protocol/impl/feature_to_payload_sketch_text_entries.inc"
#include "protocol/impl/feature_to_payload_sketch_dimension_entries.inc"
#include "protocol/impl/feature_to_payload_sketch_relation_entries.inc"
#include "protocol/impl/feature_to_payload_sketch_projection_entries.inc"
#include "protocol/impl/feature_to_payload_sketch_profile_entries.inc"
#include "protocol/impl/feature_to_payload_sketch_pending_entries.inc"
             }
           : json(nullptr)},
