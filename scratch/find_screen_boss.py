import sys
sys.stdout.reconfigure(encoding='utf-8')
with open("boss/src/main.c", "r", encoding="utf-8") as f:
    for i, line in enumerate(f):
        if "SCREEN_WIDTH" in line or "SCREEN_HEIGHT" in line:
            print(f"{i+1}: {line.strip()}")
