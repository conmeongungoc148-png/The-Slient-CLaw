import sys
import os

query = sys.argv[1]
search_path = sys.argv[2]

print(f"Searching for '{query}' in '{search_path}'...")

if os.path.isfile(search_path):
    files = [search_path]
else:
    files = []
    for root, dirs, filenames in os.walk(search_path):
        for f in filenames:
            if f.endswith(".c") or f.endswith(".h"):
                files.append(os.path.join(root, f))

for file_path in files:
    try:
        with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()
        for idx, line in enumerate(lines):
            if query.lower() in line.lower():
                print(f"{file_path}:{idx+1}: {line.strip()}")
    except Exception as e:
        print(f"Error reading {file_path}: {e}")
