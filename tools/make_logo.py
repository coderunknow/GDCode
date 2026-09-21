#!/usr/bin/env python3
"""Generate logo.png (336x336, the size the Geode index expects) from code.

Kept in the repo so the logo is reproducible and obviously original work:
a rounded green tile with a "</>" glyph drawn from simple strokes.
"""
from PIL import Image, ImageDraw

SIZE = 336
img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
d = ImageDraw.Draw(img)

# tile
d.rounded_rectangle((8, 8, SIZE - 8, SIZE - 8), radius=64, fill=(38, 110, 62, 255),
                    outline=(20, 60, 34, 255), width=10)
d.rounded_rectangle((26, 26, SIZE - 26, SIZE - 26), radius=52, fill=(56, 160, 88, 255))

# "</>" strokes
W = 26
col = (250, 250, 250, 255)
# left chevron
d.line([(118, 108), (56, 168), (118, 228)], fill=col, width=W, joint="curve")
# right chevron
d.line([(218, 108), (280, 168), (218, 228)], fill=col, width=W, joint="curve")
# slash
d.line([(196, 84), (140, 252)], fill=(255, 224, 96, 255), width=W)

img.save("logo.png")
print("wrote logo.png", img.size)
