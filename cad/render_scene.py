#!/usr/bin/env python3
"""render_scene.py — the record player as a product picture (light blue background) and on a side table.
Run: python3 cad/render_scene.py -> doc/img/recordplayer_product.png, recordplayer_scene.png
Uses zrender.py (software z-buffer, anti-aliased)."""
import os, numpy as np, trimesh, importlib.util
here = os.path.dirname(os.path.abspath(__file__))
def load(n):
    sp = importlib.util.spec_from_file_location(n, os.path.join(here, n + ".py"))
    m = importlib.util.module_from_spec(sp); sp.loader.exec_module(m); return m
f, zr = load("make_full_assembly"), load("zrender")
cyl = f.sf.cyl

def col(m, rgb):
    m = m.copy(); m.visual.face_colors = list(rgb) + [255]; return m

def parts(table=False):
    p = {"shell": col(f.sf.shell(), f.C["shell"])}
    p.update({"rp_" + n: m for n, m in f.rp.all_parts().items()})
    if table:
        T = -10.0
        p["table_top"] = col(cyl(0, 0, T - 28, 520, 28, 128), (150, 105, 65))
        p["table_edge"] = col(cyl(0, 0, T - 32, 500, 4, 128), (120, 82, 50))
        for a in (45, 135, 225, 315):
            x, y = 190 * np.cos(np.radians(a)), 190 * np.sin(np.radians(a))
            p[f"table_leg_{a}"] = col(trimesh.creation.cylinder(radius=14, segment=[(x, y, T - 32), (x * 1.25, y * 1.25, T - 400)], sections=32), (95, 65, 40))
    return p

if __name__ == "__main__":
    out = os.path.join(here, "..", "doc", "img"); os.makedirs(out, exist_ok=True)
    zr.render(parts(False), el=20, az=-30, width=1400, background="blue", shadow=True, out=os.path.join(out, "recordplayer_product.png"))
    zr.render(parts(True), el=17, az=-32, width=1100, background="room", shadow=False, out=os.path.join(out, "recordplayer_scene.png"))
