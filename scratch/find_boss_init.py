import sys
sys.stdout.reconfigure(encoding='utf-8')
with open("boss/src/boss.c", "r", encoding="utf-8") as f:
    for i, line in enumerate(f):
        if "state" in line and "boss->" in line and i < 200:
            print(f"boss.c:{i+1}: {line.strip()}")
