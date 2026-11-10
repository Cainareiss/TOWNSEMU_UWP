"""Tsugaru (FM Towns) branding art for the frontend, from the Marty logo (uwp/package/src/marty_icon.png).
  assets/flamenco-emblem.png   512x512 RGBA: the Marty mascot + "FM TOWNS MARTY" word mark, background keyed to alpha
  assets/nocover-flamenco.png  500x500 RGBA: "TSUGARU / FM TOWNS EMULATOR" card in the Marty palette
File names are unchanged (coverflow.c refers to them). Run: python make_marty_art.py  (Pillow)."""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
SW = os.path.normpath(os.path.join(HERE, ".."))
ICON = os.path.normpath(os.path.join(SW, "..", "package", "src", "marty_icon.png"))
FONT_BOLD = os.path.join(SW, "assets", "fonts", "ChakraPetch-700.ttf")
FONT_REG = os.path.join(SW, "assets", "fonts", "Inter-600.ttf")

BLUE = (0x3B, 0x87, 0xD1)
BLUE_DEEP = (0x2F, 0x6F, 0xBF)
MINT = (0x7F, 0xCF, 0xB0)
SALMON = (0xF0, 0x8A, 0x8A)
STAR = (0xF5, 0xD1, 0x42)
CREAM = (0xF6, 0xF6, 0xF6)
NAVY = (0x1C, 0x2A, 0x3C)


def keyed(src):
    """The logo with its flat #F6F6F6 backdrop turned into alpha (soft edge: distance to the backdrop colour)."""
    im = src.convert("RGB")
    w, h = im.size
    out = Image.new("RGBA", (w, h))
    sp, dp = im.load(), out.load()
    for y in range(h):
        for x in range(w):
            r, g, b = sp[x, y]
            d = max(abs(r - CREAM[0]), abs(g - CREAM[1]), abs(b - CREAM[2]))
            a = 0 if d < 8 else 255 if d > 40 else int((d - 8) * 255 / 32)
            dp[x, y] = (r, g, b, a)
    return out


def bbox_of(im):
    a = im.split()[3]
    return a.getbbox()


def emblem():
    src = Image.open(ICON)
    k = keyed(src)
    box = bbox_of(k)
    k = k.crop(box)
    canvas = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
    scale = min(470 / k.width, 470 / k.height)
    k = k.resize((max(1, round(k.width * scale)), max(1, round(k.height * scale))), Image.LANCZOS)
    canvas.paste(k, ((512 - k.width) // 2, (512 - k.height) // 2), k)
    path = os.path.join(SW, "assets", "flamenco-emblem.png")
    canvas.save(path, optimize=True)
    print(path, canvas.size)


def nocover():
    W = H = 500
    im = Image.new("RGBA", (W, H), CREAM + (255,))
    d = ImageDraw.Draw(im)
    # a blue frame and a salmon/mint/star stripe, in the spirit of the Marty's striped shirt
    d.rounded_rectangle((14, 14, W - 15, H - 15), radius=28, outline=BLUE, width=8)
    stripes = [BLUE, SALMON, MINT, STAR, BLUE]
    y0 = 300
    for i, c in enumerate(stripes):
        d.rectangle((40, y0 + i * 14, W - 41, y0 + i * 14 + 10), fill=c)
    # the mascot, small, above the title
    try:
        k = keyed(Image.open(ICON))
        box = bbox_of(k)
        # mascot only: the top part above the word mark (the word mark starts around 55% of the height)
        mascot = k.crop((box[0], box[1], box[2], box[1] + int((box[3] - box[1]) * 0.58)))
        mb = bbox_of(mascot)
        mascot = mascot.crop(mb)
        s = 150 / mascot.height
        mascot = mascot.resize((round(mascot.width * s), 150), Image.LANCZOS)
        im.paste(mascot, ((W - mascot.width) // 2, 48), mascot)
    except Exception as e:
        print("mascot skipped:", e)
    try:
        f1 = ImageFont.truetype(FONT_BOLD, 78)
        f2 = ImageFont.truetype(FONT_REG, 24)
    except Exception:
        f1 = f2 = ImageFont.load_default()
    d = ImageDraw.Draw(im)
    t = "TSUGARU"
    tw = d.textlength(t, font=f1)
    d.text(((W - tw) / 2, 206), t, font=f1, fill=BLUE_DEEP)
    t2 = "FM TOWNS EMULATOR"
    tw2 = d.textlength(t2, font=f2)
    d.text(((W - tw2) / 2, 392), t2, font=f2, fill=NAVY)
    t3 = "NO COVER"
    tw3 = d.textlength(t3, font=f2)
    d.text(((W - tw3) / 2, 430), t3, font=f2, fill=(0x56, 0x68, 0x7E))
    path = os.path.join(SW, "assets", "nocover-flamenco.png")
    im.save(path, optimize=True)
    print(path, im.size)


if __name__ == "__main__":
    emblem()
    nocover()
