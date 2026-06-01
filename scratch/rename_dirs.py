import os
import shutil

# 1. Rename boss/assets/characters to boss/assets/boss
src1 = r"c:\Users\HugoDo\Desktop\Theforest-main\boss\assets\characters"
dst1 = r"c:\Users\HugoDo\Desktop\Theforest-main\boss\assets\boss"
if os.path.exists(src1):
    if os.path.exists(dst1):
        shutil.rmtree(dst1)
    os.rename(src1, dst1)
    print(f"Renamed {src1} -> {dst1}")
else:
    print(f"Source not found: {src1}")

# 2. Move assets/characters/sprites/cat to assets/cat
src2 = r"c:\Users\HugoDo\Desktop\Theforest-main\assets\characters\sprites\cat"
dst2 = r"c:\Users\HugoDo\Desktop\Theforest-main\assets\cat"
if os.path.exists(src2):
    if os.path.exists(dst2):
        shutil.rmtree(dst2)
    os.rename(src2, dst2)
    print(f"Renamed {src2} -> {dst2}")
else:
    print(f"Source not found: {src2}")

# Clean up empty parent folders: assets/characters/sprites and assets/characters
p1 = r"c:\Users\HugoDo\Desktop\Theforest-main\assets\characters\sprites"
p2 = r"c:\Users\HugoDo\Desktop\Theforest-main\assets\characters"
if os.path.exists(p1) and not os.listdir(p1):
    os.rmdir(p1)
    print(f"Removed empty dir {p1}")
if os.path.exists(p2) and not os.listdir(p2):
    os.rmdir(p2)
    print(f"Removed empty dir {p2}")
