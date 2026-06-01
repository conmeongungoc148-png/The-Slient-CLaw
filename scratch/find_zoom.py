import sys
sys.stdout.reconfigure(encoding='utf-8')
with open("src/main.c", "r", encoding="utf-8") as f:
    for i, line in enumerate(f):
        if "zoom" in line.lower():
            print(f"{i+1}: {line.strip()}")
