import sys
sys.stdout.reconfigure(encoding='utf-8')

print("=== SRC/MAIN.C RESOLUTION LINES ===")
with open("src/main.c", "r", encoding="utf-8") as f:
    for i, line in enumerate(f):
        if "width" in line.lower() or "height" in line.lower() or "window" in line.lower() or "screen" in line.lower():
            if any(term in line for term in ["#define", "InitWindow", "VIRTUAL"]):
                print(f"{i+1}: {line.strip()}")

print("\n=== BOSS/SRC/MAIN.C RESOLUTION LINES ===")
with open("boss/src/main.c", "r", encoding="utf-8") as f:
    for i, line in enumerate(f):
        if "width" in line.lower() or "height" in line.lower() or "window" in line.lower() or "screen" in line.lower():
            if any(term in line for term in ["#define", "InitWindow", "VIRTUAL"]):
                print(f"{i+1}: {line.strip()}")
