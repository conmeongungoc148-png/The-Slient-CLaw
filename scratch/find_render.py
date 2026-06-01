import sys
sys.stdout.reconfigure(encoding='utf-8')
with open("src/main.c", "r", encoding="utf-8") as f:
    for i, line in enumerate(f):
        if "target.texture" in line or "DrawTexture" in line:
            print(f"{i+1}: {line.strip()}")
