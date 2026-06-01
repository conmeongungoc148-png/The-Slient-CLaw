import sys
sys.stdout.reconfigure(encoding='utf-8')
with open("boss/src/boss.c", "r", encoding="utf-8") as f:
    lines = f.readlines()
for i, line in enumerate(lines):
    if "laser" in line.lower():
        print(f"{i+1}: {line.strip()}")
