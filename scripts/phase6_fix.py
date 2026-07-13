import pathlib
root = pathlib.Path('c:/SRC/polysmith/native/cad-core/src')
fixes = [
    ('params.points', 'params.vertices'),
    ('sketch.points', 'sketch.vertices'),
    ('parameters->points', 'parameters->vertices'),
]
for ext in ['*.inc', '*.cpp', '*.h']:
    for f in root.glob(f'**/{ext}'):
        c = f.read_text('utf-8')
        changed = False
        for old, new in fixes:
            if old in c:
                c = c.replace(old, new)
                changed = True
        if changed:
            f.write_text(c, 'utf-8')
            print(f'  {f.relative_to(root)}')
print('Done')
