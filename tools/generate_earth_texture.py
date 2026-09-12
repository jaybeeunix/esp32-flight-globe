#!/usr/bin/env python3
import json
import math
import os
from PIL import Image, ImageDraw

PROJ_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), 
GEOJSON_LAND = PROJ_PATH, "../../ne_50m_land.geojson"
GEOJSON_COAST = PROJ_PATH, "../../ne_50m_coastline.geojson"
TEXTURE_HEADER = PROJ_PATH, "../src/earth_texture.h"))

W_SS, H_SS = 1440, 720
W, H = 720, 360

img_ss = Image.new("RGB", (W_SS, H_SS), (10, 18, 36))
draw_ss = ImageDraw.Draw(img_ss)

print("Loading 50m Natural Earth land...")
with open(GEOJSON_LAND, "r", encoding="utf-8") as f:
    land_data = json.load(f)

for feat in land_data["features"]:
    geom = feat["geometry"]
    gtype = geom["type"]
    polys = [geom["coordinates"]] if gtype == "Polygon" else geom["coordinates"]
    for poly in polys:
        ring = poly[0]
        pts = [(int((lon + 180.0) * (W_SS / 360.0)) % W_SS, int((90.0 - lat) * (H_SS / 180.0))) for lon, lat in ring]
        if len(pts) > 2:
            draw_ss.polygon(pts, fill=(28, 62, 46), outline=(48, 106, 76))

print("Loading 50m Natural Earth coastlines...")
with open(GEOJSON_COAST, "r", encoding="utf-8") as f:
    coast_data = json.load(f)

for feat in coast_data["features"]:
    geom = feat["geometry"]
    coords_list = [geom["coordinates"]] if geom["type"] == "LineString" else geom["coordinates"]
    for coords in coords_list:
        if len(coords) < 2: continue
        pts = [(int((lon + 180.0) * (W_SS / 360.0)) % W_SS, int((90.0 - lat) * (H_SS / 180.0))) for lon, lat in coords]
        seg = []
        for p in pts:
            if not seg:
                seg.append(p)
            else:
                if abs(p[0] - seg[-1][0]) > W_SS // 2:
                    if len(seg) > 1:
                        draw_ss.line(seg, fill=(58, 130, 94), width=2)
                    seg = [p]
                else:
                    seg.append(p)
        if len(seg) > 1:
            draw_ss.line(seg, fill=(58, 130, 94), width=2)

draw_ss.line([(0, H_SS // 2), (W_SS, H_SS // 2)], fill=(16, 28, 52), width=2)
draw_ss.line([(0, int((90.0 - 23.44) * H_SS / 180.0)), (W_SS, int((90.0 - 23.44) * H_SS / 180.0))], fill=(14, 24, 44), width=2)
draw_ss.line([(0, int((90.0 + 23.44) * H_SS / 180.0)), (W_SS, int((90.0 + 23.44) * H_SS / 180.0))], fill=(14, 24, 44), width=2)

print("Resampling to 720x360 with Lanczos filter...")
img = img_ss.resize((W, H), Image.Resampling.LANCZOS)

def rgb_to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

print(f"Writing {TEXTURE_HEADER}...")
with open(TEXTURE_HEADER, "w", encoding="utf-8") as out:
    out.write("// Auto-generated 720x360 High-Resolution RGB565 Earth Texture
")
    out.write("#pragma once
")
    out.write("#include <Arduino.h>

")
    out.write("#define EARTH_TEX_WIDTH  720
")
    out.write("#define EARTH_TEX_HEIGHT 360

")
    out.write("static const uint16_t EARTH_TEXTURE[EARTH_TEX_WIDTH * EARTH_TEX_HEIGHT] PROGMEM = {
")
    pixels = img.load()
    row = []
    for y in range(H):
        for x in range(W):
            r, g, b = pixels[x, y]
            c565 = rgb_to_rgb565(r, g, b)
            row.append(f"0x{c565:04X}")
            if len(row) >= 16:
                out.write("    " + ", ".join(row) + ",
")
                row = []
    if row:
        out.write("    " + ", ".join(row) + "
")
    out.write("};
")

print(f"Generated {TEXTURE_HEADER} ({os.path.getsize(TEXTURE_HEADER):,} bytes)")
