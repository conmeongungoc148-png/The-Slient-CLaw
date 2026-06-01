import sys
sys.stdout.reconfigure(encoding='utf-8')
with open("boss/src/boss.c", "r", encoding="utf-8") as f:
    for i, line in enumerate(f):
        if "DrawUI" in line:
            print(f"boss.c:{i+1}: {line.strip()}")
with open("src/main.c", "r", encoding="utf-8") as f:
    for i, line in enumerate(f):
        if "DrawUI" in line:
            print(f"main.c:{i+1}: {line.strip()}")
