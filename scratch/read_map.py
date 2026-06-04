import json

with open("boss/assets/boss map 1v1.tmj", "r", encoding="utf-8") as f:
    data = json.load(f)

print(f"Map size: {data.get('width')}x{data.get('height')} tiles")
print(f"Tile size: {data.get('tilewidth')}x{data.get('tileheight')}")

for layer in data.get("layers", []):
    print(f"\nLayer Name: '{layer.get('name')}', Type: '{layer.get('type')}', Visible: {layer.get('visible')}")
    if layer.get("type") == "objectgroup":
        for obj in layer.get("objects", []):
            print(f"  Object ID {obj.get('id')}: '{obj.get('name')}', position: ({obj.get('x')}, {obj.get('y')}), size: {obj.get('width')}x{obj.get('height')}, gid: {obj.get('gid')}")
