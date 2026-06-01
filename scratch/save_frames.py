from PIL import Image

img = Image.open('boss/assets/effects/laser/spritesheet.png')
fw, fh = 300, 1309
cols = 4

for i in range(8):
    col = i % cols
    row = i // cols
    x = col * fw
    y = row * fh
    frame = img.crop((x, y, x + fw, y + fh))
    frame.save(f'scratch/frame_{i}.png')
    bbox = frame.split()[-1].getbbox()
    print(f"Frame {i} (col {col}, row {row}): bbox = {bbox}")
