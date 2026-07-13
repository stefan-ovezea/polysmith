import pathlib

root = pathlib.Path('c:/SRC/polysmith')

# Step 1: Struct renames
struct_renames = {
    'SketchPoint': 'SketchVertex',
    'ViewportSketchPointPrimitive': 'ViewportSketchVertexPrimitive',
}
for ext in ['*.h', '*.inc', '*.cpp']:
    for f in root.glob(f'native/cad-core/src/**/{ext}'):
        content = f.read_text(encoding='utf-8')
        changed = False
        for old, new in struct_renames.items():
            if old in content:
                content = content.replace(old, new)
                changed = True
        if changed:
            f.write_text(content, encoding='utf-8')
            print(f'  C++ struct: {f.relative_to(root)}')

# Step 2: Field renames (simple string replace)
field_renames = [
    ('parameters.points', 'parameters.vertices'),
    ('sketch_points', 'sketch_vertices'),
    ('selected_sketch_point_id', 'selected_sketch_vertex_id'),
    ('selected_sketch_point_ids', 'selected_sketch_vertex_ids'),
]
for ext in ['*.h', '*.inc', '*.cpp']:
    for f in root.glob(f'native/cad-core/src/**/{ext}'):
        content = f.read_text(encoding='utf-8')
        changed = False
        for old, new in field_renames:
            if old in content:
                content = content.replace(old, new)
                changed = True
        if changed:
            f.write_text(content, encoding='utf-8')
            print(f'  C++ field: {f.relative_to(root)}')

# Step 3: TS renames
ts_renames = {
    'SketchPointEntry': 'SketchVertexEntry',
    'SketchPointScene': 'SketchVertexScene',
    'ViewportSketchPoint': 'ViewportSketchVertex',
}
for ext in ['*.ts', '*.tsx']:
    for f in root.glob(f'apps/desktop-ui/src/**/{ext}'):
        content = f.read_text(encoding='utf-8')
        changed = False
        for old, new in ts_renames.items():
            if old in content:
                content = content.replace(old, new)
                changed = True
        if changed:
            f.write_text(content, encoding='utf-8')
            print(f'  TS: {f.relative_to(root)}')

print('Done.')
