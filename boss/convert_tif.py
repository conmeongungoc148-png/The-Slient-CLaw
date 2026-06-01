from PIL import Image
import os

# Convert texture_laser.tif → .png
src = "assets/effects/laser/texture_laser.tif"
dst = "assets/effects/laser/texture_laser.png"
if os.path.exists(src):
    img = Image.open(src)
    img.save(dst)
    print(f"Converted {src} -> {dst}, size={img.size}")

# Pack shipfire06_001..005 into single horizontal spritesheet
shipfire_dir = "assets/effects/shipfire"
frames = []
for i in range(1, 6):
    fname = f"shipfire06_00{i}.png"
    fpath = os.path.join(shipfire_dir, fname)
    if os.path.exists(fpath):
        frames.append(Image.open(fpath))
    else:
        print(f"Missing: {fpath}")

if len(frames) == 5:
    fw, fh = frames[0].size
    sheet = Image.new("RGBA", (fw * 5, fh), (0, 0, 0, 0))
    for i, fr in enumerate(frames):
        sheet.paste(fr, (i * fw, 0))
    sheet_path = "assets/effects/shipfire/orb_fire_sheet.png"
    sheet.save(sheet_path)
    print(f"Packed shipfire06 -> {sheet_path}, size={sheet.size}")
