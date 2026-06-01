import sys, os
sys.stdout.reconfigure(encoding='utf-8')
for root, dirs, files in os.walk("src"):
    for file in files:
        if file.endswith(".c") or file.endswith(".h"):
            path = os.path.join(root, file)
            with open(path, "r", encoding="utf-8", errors="ignore") as f:
                for i, line in enumerate(f):
                    if "SCREEN_WIDTH" in line or "SCREEN_HEIGHT" in line:
                        print(f"{path}:{i+1}: {line.strip()}")
