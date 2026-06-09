import json

def inspect_map(path):
    print(f"=== Inspecting {path} ===")
    try:
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        print("Map properties:", list(data.keys())[:10])
        if "tilesets" in data:
            print("Tilesets:")
            for ts in data["tilesets"]:
                print(f"  - source: {ts.get('source', 'N/A')}, name: {ts.get('name', 'N/A')}, image: {ts.get('image', 'N/A')}")
        if "layers" in data:
            print("Layers:", [l.get("name") for l in data["layers"]])
    except Exception as e:
        print("Error:", e)

inspect_map("d:/1AG]/Theforest/assets/thefirstmap.tmj")
inspect_map("d:/1AG]/Theforest/assets/thesecondmap.tmj")
inspect_map("d:/1AG]/Theforest/assets/nightcity.tmj")
inspect_map("d:/1AG]/Theforest/assets/forestmap.tmj")
