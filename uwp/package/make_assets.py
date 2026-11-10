"""Generates the MSIX tile/logo assets from the Marty icon (src/marty_icon.png).
The icon has no alpha channel and a #F6F6F6 backdrop, so every asset is that colour plus the icon fitted inside.
Run: python make_assets.py   (needs Pillow)"""
from PIL import Image
import os

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = Image.open(os.path.join(HERE, "src", "marty_icon.png")).convert("RGB")
BG = SRC.getpixel((2, 2))  # (246, 246, 246)
OUT = os.path.join(HERE, "Assets")
os.makedirs(OUT, exist_ok=True)

# name -> (width, height, fill) ; sizes are the scale-200 sizes (no .pri in the package, so files are unqualified)
ASSETS = {
    "StoreLogo.png": (100, 100, 0.92),
    "Square44x44Logo.png": (88, 88, 0.96),
    "Square71x71Logo.png": (142, 142, 0.94),
    "Square150x150Logo.png": (300, 300, 0.94),
    "Wide310x150Logo.png": (620, 300, 0.92),
    "Square310x310Logo.png": (620, 620, 0.94),
    "SplashScreen.png": (1240, 600, 0.80),
}

for name, (w, h, fill) in ASSETS.items():
    canvas = Image.new("RGB", (w, h), BG)
    box_w, box_h = int(w * fill), int(h * fill)
    scale = min(box_w / SRC.width, box_h / SRC.height)
    nw, nh = max(1, round(SRC.width * scale)), max(1, round(SRC.height * scale))
    icon = SRC.resize((nw, nh), Image.LANCZOS)
    canvas.paste(icon, ((w - nw) // 2, (h - nh) // 2))
    canvas.save(os.path.join(OUT, name), optimize=True)
    print(name, w, h)

print("background", "#%02X%02X%02X" % BG[:3])
