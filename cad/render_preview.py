#!/usr/bin/env python3
"""render_preview.py — front / side / top preview PNGs of the record player (z-buffer renderer, see zrender.py).
Run: python3 cad/render_preview.py   -> doc/img/recordplayer_front.png, recordplayer_side.png, recordplayer_top.png"""
import os, importlib.util
here = os.path.dirname(os.path.abspath(__file__))
def load(n):
    sp = importlib.util.spec_from_file_location(n, os.path.join(here, n + ".py"))
    m = importlib.util.module_from_spec(sp); sp.loader.exec_module(m); return m
rs, zr = load("render_scene"), load("zrender")

if __name__ == "__main__":
    out = os.path.join(here, "..", "doc", "img"); os.makedirs(out, exist_ok=True)
    p = rs.parts(False)
    for name, (el, az) in {"front": (25, -35), "side": (0, -90), "top": (90, 0)}.items():
        zr.render(p, el=el, az=az, width=1000, background="blue", shadow=False, out=os.path.join(out, f"recordplayer_{name}.png"))
