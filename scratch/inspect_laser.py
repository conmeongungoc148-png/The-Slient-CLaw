from PIL import Image

# Open the spritesheet
img = Image.open('boss/assets/effects/laser/spritesheet.png')
# Crop the first frame (column 0, row 0)
frame = img.crop((0, 0, 300, 1309))

# Find the bounding box of non-transparent pixels in the top 300px vs bottom 300px
top_part = frame.crop((0, 0, 300, 300))
bottom_part = frame.crop((0, 1009, 300, 1309))

# Check alpha channel of top vs bottom
top_alpha = top_part.split()[-1]
bottom_alpha = bottom_part.split()[-1]

print("Top part non-zero alpha pixels count:", len([p for p in top_alpha.getdata() if p > 0]))
print("Bottom part non-zero alpha pixels count:", len([p for p in bottom_alpha.getdata() if p > 0]))

# Let's save them as separate files to inspect
top_part.save('scratch/laser_top.png')
bottom_part.save('scratch/laser_bottom.png')
frame.save('scratch/laser_frame0.png')
print("Saved inspect images to scratch/")
