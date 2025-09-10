from PIL import Image, ImageOps
import numpy as np

# === Eingabedatei ===
input_path = "audi-logo-png_seeklogo-13445.png"

# === Bild laden und auf RGBA setzen (für Transparenz) ===
img_rgba = Image.open(input_path).convert("RGBA")

# === Weißen Hintergrund unterlegen (Transparenz entfernen) ===
background = Image.new("RGBA", img_rgba.size, (255, 255, 255, 255))
img_combined = Image.alpha_composite(background, img_rgba).convert("L")

# === Invertieren, damit Logo schwarz wird ===
img_inverted = ImageOps.invert(img_combined)

# === In 1-Bit umwandeln (monochrom) ===
img_bw = img_inverted.point(lambda x: 0 if x < 128 else 255, '1')

# === Größe auf TFT beschränken (optional) ===
try:
    resample_method = Image.Resampling.LANCZOS
except AttributeError:
    resample_method = Image.ANTIALIAS

img_bw.thumbnail((240, 320), resample_method)

# === Vorschau speichern ===
img_bw.save("audi_monochrome_preview.png")

# === Breite und Höhe ermitteln ===
width, height = img_bw.size

# === In NumPy-Array und dann zu Bytes umwandeln ===
bitmap_array = np.array(img_bw, dtype=np.uint8)
bitmap_bytes = np.packbits(bitmap_array, axis=1).flatten()

# === C-Array generieren ===
c_array_lines = []
for i, byte in enumerate(bitmap_bytes):
    if i % 12 == 0:
        c_array_lines.append("\n  ")
    c_array_lines.append(f"0x{byte:02X},")

c_array_str = ''.join(c_array_lines)

# === Header-Datei schreiben ===
c_header = f"""// Monochrome bitmap for Audi logo
// Size: {width}x{height}

const unsigned int audi_logo_width = {width};
const unsigned int audi_logo_height = {height};

const uint8_t audi_logo_bits[] = {{{c_array_str}
}};
"""

with open("audi_logo.h", "w") as f:
    f.write(c_header)

print("✅ Fertig! Header-Datei 'audi_logo.h' und Vorschau 'audi_monochrome_preview.png' wurden erstellt.")
