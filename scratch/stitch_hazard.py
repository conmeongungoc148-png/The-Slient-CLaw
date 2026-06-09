from PIL import Image
import os

source_dir = r"d:\1AG]\Theforest\asset_sources\skills\pixel_simulations\Flame1"
output_file = r"d:\1AG]\Theforest\boss\assets\effects\hazard_sheet.png"

# 80 frames, size 64x64. We can make a 10x8 grid.
cols = 10
rows = 8
frame_w = 64
frame_h = 64

sheet_w = cols * frame_w
sheet_h = rows * frame_h

sheet = Image.new("RGBA", (sheet_w, sheet_h))

for i in range(1, 81):
    filename = f"{i:04d}.png"
    filepath = os.path.join(source_dir, filename)
    if os.path.exists(filepath):
        img = Image.open(filepath).convert("RGBA")
        idx = i - 1
        x = (idx % cols) * frame_w
        y = (idx // cols) * frame_h
        sheet.paste(img, (x, y))

sheet.save(output_file)
print(f"Successfully saved spritesheet to {output_file}")
