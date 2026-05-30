import re

path = "apps/desktop-ui/src/layout/ViewportPanel.tsx"
with open(path, "r") as f:
    lines = f.readlines()

begin = None
end = None
for i, line in enumerate(lines):
    if "// REMOVED_BLOCK_END" in line:
        begin = i
    if "// -- END REMOVED --" in line:
        end = i

if begin is not None and end is not None and begin < end:
    del lines[begin:end+1]
    print(f"Removed lines {begin+1} to {end+1} ({end-begin+1} lines)")
    with open(path, "w") as f:
        f.writelines(lines)
else:
    print(f"Markers not found: begin={begin}, end={end}")
