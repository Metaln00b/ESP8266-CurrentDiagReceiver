from PIL import Image, ImageFont, ImageDraw
import os

# Pfad zur Eurostile TTF-Font
FONT_PATH = "_Eurostile.ttf"  # <-- deine TTF-Datei hier
OUTPUT_DIR = "../data/font"
CHAR_SIZE = 18  # Pixelgröße pro Zeichen

# Zeichenliste
CHARS = "%+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"

SAFE_NAMES = {'/': 'slash', '\\': 'backslash', '.': 'dot', ',': 'comma'}

os.makedirs(OUTPUT_DIR, exist_ok=True)
font = ImageFont.truetype(FONT_PATH, CHAR_SIZE)
ascent, descent = font.getmetrics()

for c in CHARS:
    img = Image.new("1", (CHAR_SIZE, CHAR_SIZE), 0)
    draw = ImageDraw.Draw(img)

    bbox = draw.textbbox((0,0), c, font=font)
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    x = (CHAR_SIZE - w) // 2
    y = ((CHAR_SIZE) - (ascent + descent)) // 2 + (ascent - bbox[3])

    draw.text((x, y), c, fill=1, font=font)

    # Pixel in 1D Array
    pixels = [img.getpixel((col, row)) for row in range(CHAR_SIZE) for col in range(CHAR_SIZE)]

    fname = SAFE_NAMES.get(c, c)
    bin_path = os.path.join(OUTPUT_DIR, f"{fname}.bin")

    with open(bin_path, "wb") as f:
        f.write(bytearray(pixels))
    print(f"Erstellt: {bin_path}")