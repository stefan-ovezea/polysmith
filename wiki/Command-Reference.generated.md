# Command Reference (generated)

> Auto-generated from the app command handlers by
> `node scripts/command-registry.mjs --write <path>`.
> Do not edit by hand — edits would drift from the code.

| command | keys |
|---|---|
| `add_box_feature` | width (dimension), height (dimension), depth (dimension) |
| `add_cylinder_feature` | radius (dimension), height (dimension) |
| `add_parameter` | name (string), expression (string) |
| `add_sketch_angle_dimension` | first_line_id (string), second_line_id (string) |
| `add_sketch_arc` | start_x (dimension), start_y (dimension), end_x (dimension), end_y (dimension), anchor_x (dimension), anchor_y (dimension), mode (string) |
| `add_sketch_arc_angle_dimension` | arc_id (string) |
| `add_sketch_arc_length_dimension` | arc_id (string) |
| `add_sketch_arc_radius_dimension` | arc_id (string) |
| `add_sketch_chamfer` | mode (optional_string), angle_degrees (optional_dimension), distance_b (optional_dimension), corner_vertex_id (string), line_a_id (string), line_b_id (string), distance_a (dimension) |
| `add_sketch_circle` | center_x (dimension), center_y (dimension), radius (dimension) |
| `add_sketch_circle_radius_dimension` | circle_id (string) |
| `add_sketch_distance_dimension` | first_entity_id (string), second_entity_id (string) |
| `add_sketch_ellipse` | center_x (dimension), center_y (dimension), axis_a_x (dimension), axis_a_y (dimension), axis_b_x (dimension), axis_b_y (dimension) |
| `add_sketch_fillet` | corner_vertex_id (string), line_a_id (string), line_b_id (string), radius (dimension), arc_a_id (optional_string), arc_b_id (optional_string) |
| `add_sketch_line` | start_x (dimension), start_y (dimension), end_x (dimension), end_y (dimension), line_type (optional_string) |
| `add_sketch_line_angle_dimension` | line_id (string) |
| `add_sketch_line_length_dimension` | line_id (string) |
| `add_sketch_point` | x (dimension), y (dimension) |
| `add_sketch_polygon` | sides (dimension), mode (string), start_x (dimension), start_y (dimension), end_x (dimension), end_y (dimension) |
| `add_sketch_polygon_radius_dimension` | polygon_id (string) |
| `add_sketch_rectangle` | start_x (dimension), start_y (dimension), end_x (dimension), end_y (dimension) |
| `add_sketch_rounded_rectangle` | start_x (dimension), start_y (dimension), end_x (dimension), end_y (dimension), radius (dimension) |
| `add_sketch_slot` | center_x (dimension), center_y (dimension), length (dimension), radius (dimension), rotation (dimension), mode (optional_string_value), arc_radius (optional_dimension), start_angle (optional_dimension), sweep_angle (optional_dimension) |
| `add_sketch_spline` | degree (optional_dimension), mode (optional_string_value) |
| `add_sketch_text` | font_path (optional_string), anchor_x (dimension), anchor_y (dimension), path_entity_id (optional_string) |
| `add_sketch_vertex_distance_dimension` | vertex_a_id (string), vertex_b_id (string) |
| `cam_capture_edge_reference` |  |
| `cam_capture_face_reference` |  |
| `cam_capture_point` |  |
| `cam_export_gcode` | file_path (string) |
| `cam_export_gcode_text` |  |
| `cam_machine_list` |  |
| `cam_machine_save` | from_payload |
| `cam_machine_settings_set` | from_payload |
| `cam_operation_create` | from_payload |
| `cam_operation_delete` | op_id (string) |
| `cam_operation_generate` | op_id (string) |
| `cam_operation_set_scope` | op_id (string), feature_id (string) |
| `cam_operation_update` | op_id (string) |
| `cam_post_import` | source_path (string) |
| `cam_post_list` |  |
| `cam_post_processor_set` | from_payload |
| `cam_setup_create` | from_payload |
| `cam_setup_delete` | setup_id (string) |
| `cam_setup_get` |  |
| `cam_setup_update` | from_payload |
| `cam_stock_get` |  |
| `cam_stock_set` | from_payload |
| `cam_tool_add` | from_payload |
| `cam_tool_delete` | tool_id (string) |
| `cam_tool_export_file` | file_path (string) |
| `cam_tool_export_text` | format (string) |
| `cam_tool_import_file` | source_path (string), mode (string) |
| `cam_tool_library_list` |  |
| `cam_tool_library_save` | from_payload |
| `cam_tool_list` |  |
| `cam_tool_parse_file` | source_path (string) |
| `cam_tool_parse_text` | format (string), text (string) |
| `cam_tool_update` | tool_id (string) + from_payload |
| `cam_wcs_set_face` |  |
| `cancel_mirror_preview` |  |
| `clear_appearance_overrides` |  |
| `clear_body_color` | body_id (string) |
| `clear_face_color` | face_id (string) |
| `clear_selection` |  |
| `clear_sketch_line_constraints` | line_id (string) |
| `commit_mirror_preview` |  |
| `confirm_chamfer` | feature_id (string) |
| `confirm_fillet` | feature_id (string) |
| `confirm_hole` | feature_id (string) |
| `confirm_move` | feature_id (string) |
| `confirm_plugin_feature` | feature_id (string) |
| `confirm_shell` | feature_id (string) |
| `confirm_thread` | feature_id (string) |
| `convert_mesh_to_body` | body_id (string) |
| `corner_trim_preview` | entity_a_id (string), entity_b_id (string), cursor_x (dimension), cursor_y (dimension) |
| `corner_trim_sketch_entities` | entity_a_id (string), entity_b_id (string), click_x (dimension), click_y (dimension) |
| `create_angle_plane` | source_plane_id (string), source_axis_id (string), angle_degrees (dimension) |
| `create_body_copy` | source_body_id (string), copy_mode (optional_string) |
| `create_chamfer` | distance (dimension) |
| `create_construction_axis` | source_id (string) |
| `create_construction_point` | source_id (string) |
| `create_document` |  |
| `create_fastener` |  |
| `create_fillet` | radius (dimension) |
| `create_helix` | axis_source_id (string) |
| `create_hole` | face_id (string), center_x (dimension), center_y (dimension), center_z (dimension) |
| `create_linear_array` | dx (dimension), dy (dimension), count (int), center_x (dimension), center_y (dimension), total_angle_deg (dimension) |
| `create_midplane` |  |
| `create_move` | target_body_id (string) |
| `create_offset_plane` | source_plane_id (string), offset (dimension) |
| `create_plugin_feature` |  |
| `create_shell` | face_id (string), thickness (dimension) |
| `create_tangent_plane` | source_face_id (string) |
| `create_thread` |  |
| `delete_feature` | feature_id (string) |
| `delete_parameter` | name (string) |
| `delete_sketch_chamfer` | chamfer_id (string) |
| `delete_sketch_coincident_constraint` | constraint_id (string) |
| `delete_sketch_dimension` | dimension_id (string) |
| `delete_sketch_fillet` | fillet_id (string) |
| `delete_sketch_selection` |  |
| `delete_sketch_text` | text_id (string) |
| `detach_body_projections` | body_id (string) |
| `drawing_annotation_create` | drawing_id (string), view_id (string), kind (string), placement (optional_position) |
| `drawing_annotation_delete` | drawing_id (string), annotation_id (string) |
| `drawing_annotation_preview` | drawing_id (string), view_id (string), kind (string) |
| `drawing_annotation_update` | drawing_id (string), annotation_id (string), text_offset (optional_position) |
| `drawing_create` | from_payload |
| `drawing_delete` | drawing_id (string) |
| `drawing_dimension_create` | drawing_id (string), view_id (string), dim_type (string) |
| `drawing_dimension_delete` | drawing_id (string), annotation_id (string) |
| `drawing_dimension_preview` | drawing_id (string), view_id (string), dim_type (string) |
| `drawing_dimension_update` | drawing_id (string), annotation_id (string), text_offset (optional_position) |
| `drawing_export` | drawing_id (string), sheet_id (string), format (string), file_path (string), dxf_mode (string) |
| `drawing_note_create` | drawing_id (string), sheet_id (string), text (string) |
| `drawing_note_delete` | drawing_id (string), note_id (string) |
| `drawing_note_update` | drawing_id (string), note_id (string), position (optional_position) |
| `drawing_section_update` | drawing_id (string), view_id (string) |
| `drawing_set_active` | drawing_id (string) |
| `drawing_sheet_create` | drawing_id (string) |
| `drawing_sheet_delete` | drawing_id (string), sheet_id (string) |
| `drawing_sheet_update` | drawing_id (string), sheet_id (string), paper_size (string), orientation (string), projection_angle (string), name (string) |
| `drawing_template_load` | file_path (string) |
| `drawing_template_save` | file_path (string) |
| `drawing_title_block_update` | drawing_id (string), sheet_id (string) |
| `drawing_view_create` | drawing_id (string), sheet_id (string) |
| `drawing_view_delete` | drawing_id (string), view_id (string) |
| `drawing_view_move` | drawing_id (string), view_id (string) |
| `drawing_view_preview` | drawing_id (string), sheet_id (string) |
| `drawing_view_update` | drawing_id (string) |
| `export_body_step` | file_path (string), body_id (string) |
| `export_body_stl` | file_path (string), body_id (string) |
| `export_document` | file_path (string) |
| `export_document_dxf` | file_path (string) |
| `export_document_iges` | file_path (string) |
| `export_document_stl` | file_path (string) |
| `extend_sketch_entity` | entity_id (string), click_x (dimension), click_y (dimension) |
| `extrude_face` | depth (dimension), face_id (string) |
| `extrude_profile` | profile_id (string), depth (dimension) |
| `finish_sketch` |  |
| `get_document_state` |  |
| `get_session_state` |  |
| `get_sketch_diagnostics` | feature_id (string) |
| `get_sketch_profiles` | feature_id (string) |
| `get_viewport_state` |  |
| `import_dxf` | file_path (string), plane_id (optional_string) |
| `import_iges` | file_path (string) |
| `import_step` | file_path (string) |
| `import_stl` | file_path (string), scale (optional_dimension) |
| `load_document` | file_path (string) |
| `loft_profiles` |  |
| `merge_coincident_sketch_points` | feature_id (string) |
| `move_sketch_entities` |  |
| `offset_sketch_entity` | entity_id (string), distance (dimension) |
| `pcb_capture_layer_reference` |  |
| `pcb_import_layer` | file_path (string) |
| `pcb_remove_layer` | feature_id (string) |
| `pcb_update_layer` | feature_id (string) |
| `ping` |  |
| `project_body_into_sketch` | body_id (string), mode (string) |
| `project_edge_into_sketch` | edge_id (string) |
| `project_face_into_sketch` | face_id (string) |
| `project_profile_into_sketch` | profile_id (string) |
| `project_vertex_into_sketch` | vertex_id (string) |
| `redefine_sketch_plane` | feature_id (string), plane_id (string) |
| `redo` |  |
| `reenter_sketch` | feature_id (string) |
| `remove_sketch_projections` | feature_id (string), keep_geometry (bool) |
| `rename_feature` | feature_id (string), name (string) |
| `revolve_profile` | profile_id (string), axis_entity_id (string) |
| `save_document` | file_path (string) |
| `select_edge` | edge_id (string) |
| `select_face` | face_id (string) |
| `select_feature` | feature_id (string) |
| `select_reference` | reference_id (string) |
| `select_sketch_dimension` | dimension_id (string) |
| `select_sketch_entities` |  |
| `select_sketch_entity` | entity_id (string) |
| `select_sketch_profile` | entity_id (string), profile_id (string) |
| `select_sketch_profiles` |  |
| `select_sketch_profiles_rect` | x1 (dimension), y1 (dimension), x2 (dimension), y2 (dimension) |
| `select_sketch_rect` | x1 (dimension), y1 (dimension), x2 (dimension), y2 (dimension) |
| `select_sketch_vertex` | vertex_id (string) |
| `select_vertex` | vertex_id (string) |
| `set_body_color` | body_id (string), color (string) |
| `set_face_color` | face_id (string), color (string) |
| `set_feature_suppressed` | feature_id (string) |
| `set_sketch_coincident_constraint` | vertex_id (string), other_vertex_id (string) |
| `set_sketch_collinear_constraint` | line_id (string), other_line_id (string) |
| `set_sketch_equal_length_constraint` | other_line_id (string), line_id (string) |
| `set_sketch_line_constraint` | constraint (string), line_id (string) |
| `set_sketch_line_construction` | line_id (string) |
| `set_sketch_midpoint_anchor` | vertex_id (string) |
| `set_sketch_midpoint_constraint` | vertex_id (string), line_id (string) |
| `set_sketch_parallel_constraint` | other_line_id (string), line_id (string) |
| `set_sketch_perpendicular_constraint` | other_line_id (string), line_id (string) |
| `set_sketch_symmetric_constraint` | point_id (string), other_point_id (string), axis_line_id (string) |
| `set_sketch_tangent_constraint` | line_id (string), circle_id (string) |
| `set_sketch_tangent_pair_constraint` | first_id (string), second_id (string) |
| `set_sketch_tool` | tool (string) |
| `set_sketch_vertex_fixed` | vertex_id (string) |
| `set_sketch_vertex_line_anchor` | vertex_id (string) |
| `set_timeline_cursor` | included_action_count (int) |
| `set_undo_limit` | limit (int) |
| `split_sketch_entity` | entity_id (string), click_x (dimension), click_y (dimension), split2_x (dimension), split2_y (dimension) |
| `start_mirror_preview` |  |
| `start_sketch_on_face` | face_id (string), plane_frame (plane_frame) |
| `start_sketch_on_plane` | reference_id (string) |
| `sweep_profile` | profile_id (string), path_entity_id (string) |
| `toggle_sketch_dimension_driven` | dimension_id (string) |
| `transform_sketch_entities` | dx (dimension), dy (dimension), center_x (dimension), center_y (dimension), angle_deg (dimension), scale (dimension) |
| `trim_sketch_entity` | entity_id (string), click_x (dimension), click_y (dimension) |
| `trim_sketch_stroke` |  |
| `undo` |  |
| `undo_abort_all_groups` |  |
| `undo_abort_group` |  |
| `undo_begin_group` | name (string) |
| `undo_end_group` |  |
| `undo_many` | count (int) |
| `unlink_body_copy` | feature_id (string) |
| `update_angle_plane` | feature_id (string), angle_degrees (dimension) |
| `update_box_feature` | feature_id (string), width (dimension), height (dimension), depth (dimension) |
| `update_chamfer_distance` | feature_id (string), distance (dimension) |
| `update_chamfer_edges` | feature_id (string) |
| `update_cylinder_feature` | feature_id (string), radius (dimension), height (dimension) |
| `update_extrude_depth` | feature_id (string), depth (dimension) |
| `update_extrude_mode` | feature_id (string), mode (string) |
| `update_extrude_parameters` | feature_id (string) |
| `update_extrude_profiles` | feature_id (string) |
| `update_extrude_target_body` | feature_id (string) |
| `update_fastener_parameters` | feature_id (string) |
| `update_fillet_edges` | feature_id (string) |
| `update_fillet_radius` | feature_id (string), radius (dimension) |
| `update_helix_parameters` | feature_id (string) |
| `update_hole_parameters` | feature_id (string) |
| `update_loft_profiles` | feature_id (string) |
| `update_loft_ruled` | feature_id (string) |
| `update_mirror_preview_axis` | axis_line_id (string) |
| `update_mirror_preview_objects` |  |
| `update_move_parameters` | feature_id (string) |
| `update_offset_plane` | feature_id (string), offset (dimension) |
| `update_parameter` | name (string), expression (string) |
| `update_plugin_feature` | feature_id (string) |
| `update_revolve_angle` | feature_id (string), angle_degrees (dimension) |
| `update_revolve_axis` | feature_id (string), axis_entity_id (string) |
| `update_revolve_profile` | feature_id (string), profile_id (string) |
| `update_selection_filter` |  |
| `update_shell_thickness` | feature_id (string), thickness (dimension) |
| `update_sketch_chamfer` | mode (optional_string), angle_degrees (optional_dimension), chamfer_id (string), distance_a (dimension), distance_b (optional_dimension) |
| `update_sketch_circle` | circle_id (string), center_x (dimension), center_y (dimension), radius (dimension) |
| `update_sketch_dimension` | dimension_id (string) |
| `update_sketch_dimension_display` | dimension_id (string), display_as (string) |
| `update_sketch_dimension_label_position` | dimension_id (string), label_x (dimension), label_y (dimension) |
| `update_sketch_fillet_radius` | fillet_id (string), radius (dimension) |
| `update_sketch_line` | line_id (string), start_x (dimension), start_y (dimension), end_x (dimension), end_y (dimension) |
| `update_sketch_slot` | slot_id (string), center_x (dimension), center_y (dimension), length (dimension), radius (dimension), rotation (dimension), arc_radius (optional_dimension), start_angle (optional_dimension), sweep_angle (optional_dimension) |
| `update_sketch_text` | text_id (string), font_path (optional_string), path_entity_id (optional_string) |
| `update_sketch_vertex` | vertex_id (string), x (dimension), y (dimension), bind_to_vertex_id (optional_string_value) |
| `update_sweep_path` | feature_id (string), path_entity_id (string) |
| `update_sweep_profile` | feature_id (string), profile_id (string) |
| `update_thread_parameters` | feature_id (string) |
