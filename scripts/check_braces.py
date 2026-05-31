path = "apps/desktop-ui/src/layout/ViewportPanel.tsx"
with open(path, "r") as f:
    lines = f.readlines()

depth = 0
for i, line in enumerate(lines, 1):
    for ch in line:
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
    if depth < 0:
        print("Line " + str(i) + ": depth went negative (extra })")
        break

print(f"Final depth: {depth}")
if depth != 0:
    print("BRACES ARE UNBALANCED")
