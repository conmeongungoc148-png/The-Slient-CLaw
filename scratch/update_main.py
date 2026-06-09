import os

filepath = r'd:\1AG]\Theforest\src\main.c'
with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace('"boss/assets/forestmap.tmj",\n      ', '')
content = content.replace('int totalMaps = 3;', 'int totalMaps = 2;')
content = content.replace('currentMapIndex == 2', 'currentMapIndex == 1')
content = content.replace('currentMapIndex != 2', 'currentMapIndex != 1')
content = content.replace('myCam.zoom = 1.0f;', 'myCam.zoom = 1.42f;')

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(content)
