#!/usr/bin/env python3
"""Generate all derived brand assets from assets/brand/ masters.

Masters (repo-committed originals, never edited):
  app-icon.png      1254x1254 glossy neon play glyph + crescent + orbit
  hero-wide.png     1983x793  hero banner (GitHub social, itch, store headers)
  banner-tall.png   1024x1536 vertical banner (store caps)
  logo-mark.png     1254x1254 navy line mark, transparent (light backgrounds)
  logo-mark-light.png same mark tinted lavender (dark backgrounds)

Outputs (all regenerated, never hand-edited):
  app/Jochona.icns                          macOS bundle icon
  app/Jochona.ico                           Windows exe/msi icon (RC_ICONS)
  app/res/jochona-512.png                   Linux high-res icon
  app/res/com.jochona.client.png            desktop-file icon (128)
  app/deploy/steamlink/jochona.png          Steam Link native icon (116)
  docs/assets/github-social.png             1200x630 repo social preview

Requires Pillow. Run from repo root: python3 scripts/generate-brand-assets.py
"""
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRAND = os.path.join(ROOT, "assets", "brand")


def master(name):
    return Image.open(os.path.join(BRAND, name)).convert("RGBA")


def save(img, rel, **kw):
    path = os.path.join(ROOT, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path, **kw)
    print(f"wrote {rel} {img.width}x{img.height}")


def main():
    icon = master("app-icon.png")
    if icon.width != icon.height:
        sys.exit("app-icon.png must be square")

    # --- macOS .icns via iconutil (Apple-quality downsampling) ---
    iconset = tempfile.mkdtemp(prefix="jochona_", suffix=".iconset")
    # filename -> source pixels, per Apple iconset spec (1x nominal, @2x doubled)
    matrix = {
        "icon_16x16.png": 16, "icon_16x16@2x.png": 32,
        "icon_32x32.png": 32, "icon_32x32@2x.png": 64,
        "icon_128x128.png": 128, "icon_128x128@2x.png": 256,
        "icon_256x256.png": 256, "icon_256x256@2x.png": 512,
        "icon_512x512.png": 512, "icon_512x512@2x.png": 1024,
    }
    for fname, px in matrix.items():
        icon.resize((px, px), Image.LANCZOS).save(os.path.join(iconset, fname))
    icns = os.path.join(ROOT, "app", "Jochona.icns")
    subprocess.run(["iconutil", "-c", "icns", "-o", icns, iconset], check=True)
    print("wrote app/Jochona.icns")

    # --- Windows multi-size .ico ---
    icon.resize((256, 256), Image.LANCZOS).save(
        os.path.join(ROOT, "app", "Jochona.ico"),
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    print("wrote app/Jochona.ico")

    # --- Linux ---
    save(icon.resize((512, 512), Image.LANCZOS), "app/res/jochona-512.png")
    save(icon.resize((128, 128), Image.LANCZOS), "app/res/com.jochona.client.png")
    # Freedesktop hicolor layout: linuxdeploy and the desktop file's
    # Icon=com.jochona.client require this exact filename in a sized directory.
    save(icon.resize((128, 128), Image.LANCZOS),
         "app/res/hicolor/128x128/apps/com.jochona.client.png")
    save(icon.resize((512, 512), Image.LANCZOS),
         "app/res/hicolor/512x512/apps/com.jochona.client.png")

    # --- Steam Link native icon (spec: 116x116) ---
    save(icon.resize((116, 116), Image.LANCZOS), "app/deploy/steamlink/jochona.png")

    # --- WiX bootstrap logo (square, burned into installer UI) ---
    save(icon.resize((64, 64), Image.LANCZOS), "app/jochona-wix.png")

    # --- GitHub social preview: crop hero to 1200x630, typeset brand lockup ---
    # The hero master stays text-free; the wordmark + tagline are composed here
    # with the brand fonts so they can change without touching the artwork.
    hero = master("hero-wide.png").convert("RGB")
    target_ratio = 1200 / 630
    w, h = hero.size
    if w / h > target_ratio:
        new_w = int(h * target_ratio)
        hero = hero.crop(((w - new_w) // 2, 0, (w + new_w) // 2, h))
    else:
        new_h = int(w / target_ratio)
        hero = hero.crop((0, (h - new_h) // 2, w, (h + new_h) // 2))
    hero = hero.resize((1200, 630), Image.LANCZOS)

    def font(weight, size):
        path = os.path.join(ROOT, "app", "fonts", f"SpaceGrotesk-{weight}.ttf")
        return ImageFont.truetype(path, size)

    title_font, tag_font = font("Bold", 104), font("Medium", 36)
    title, tagline = "Jochona", "Stream from anywhere."
    tx, ty = 84, 84
    # neon glow: blurred white text layers composited under the crisp glyphs
    glow = Image.new("L", hero.size, 0)
    ImageDraw.Draw(glow).text((tx, ty), title, font=title_font, fill=255)
    ImageDraw.Draw(glow).text((tx + 2, ty + 122), tagline, font=tag_font, fill=200)
    for radius, strength in ((24, 0.9), (10, 0.8)):
        halo = glow.filter(ImageFilter.GaussianBlur(radius))
        tint = Image.new("RGB", hero.size, (90, 170, 255))
        hero = Image.composite(tint, hero, halo.point(lambda p: int(p * strength)))
    draw = ImageDraw.Draw(hero)
    draw.text((tx, ty), title, font=title_font, fill=(235, 244, 255))
    draw.text((tx + 2, ty + 122), tagline, font=tag_font, fill=(199, 205, 242))
    save(hero, "docs/assets/github-social.png")


if __name__ == "__main__":
    main()
