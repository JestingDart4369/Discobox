#!/usr/bin/env python3
"""
make_viewer.py — builds ONE self-contained HTML viewer (WebGL, no internet needed) with several model versions.

Run: python3 cad/make_viewer.py     -> doc/viewer/discobox_viewer.html
Open the HTML file in a browser. Left side: version tabs, view buttons, shell see-through slider, and a tree to show
or hide groups and single parts (floor, side table, electronics, wires, record, ...).

Add a version: add one line to VERSIONS below (a 3MF from cad/placeholders/), run the script again.
"""
import sys, os, io, json, base64, zipfile
import numpy as np
import trimesh
from lxml import etree

here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
PH = os.path.join(here, "placeholders")
OUT = os.path.join(here, "..", "doc", "viewer", "discobox_viewer.html")

# newest first. dist / tz = start camera distance and target height.
VERSIONS = [
    dict(id="v6", label="v6 record player (current)", file="discobox_full.3mf", dist=950, tz=140, table=True, interactive=True, addons=True,
         note="The full model. Settings: interactive disco button, add-ons (cables with the big red button, 3 m LED strip, motor disco ball), the side table and a drawings-only view are separate switches. The plugs in the back pocket are always there. The side table has two sizes: round, and a big one that is used when cables, strip or ball are on."),
    dict(id="v4", label="v4 shell + frame", file="shell_frame_assembly.3mf", dist=800, tz=60, table=False,
         note="Before the record-player look: lift-off shell on a frame, mains compartment with lid, module block-outs."),
    dict(id="v3", label="v3 block-out", file="blockout_assembly.3mf", dist=800, tz=60, table=False,
         note="First block-out: housing with speakers, port pocket, mains compartment, parts, straight pipe and flat platter."),
    dict(id="v2", label="v2 starter design (Schalplatte)", file="../starter/Shalplatte_Discobox.3mf", scale=1000.0, dist=700, tz=-60, table=False,
         note="Your starter model: 315 mm platter with LED ring and diffuser plate, angled 100 mm stem and the Arduino holder below. The LED ring top is reused 1:1 in v5. Requirements got better from here: Arduino moved into the housing, flat platter plus tilted record, removable hollow pipe, mains compartment, recessed ports."),
]

NS = "{http://schemas.microsoft.com/3dmanufacturing/core/2015/02}"
FALLBACK = [("led holder", (25, 25, 28)), ("test top", (225, 225, 232)), ("led ring", (30, 160, 200)), ("abwinklung", (150, 150, 155)),
            ("holder arduino", (120, 130, 150)), ("small parts", (0, 130, 140)), ("shell", (200, 25, 30)), ("housing", (200, 25, 30)), ("frame", (70, 70, 75)), ("mains_lid", (230, 120, 20)),
            ("mains_compartment", (230, 120, 20)), ("speaker", (60, 60, 60)), ("arduino", (0, 130, 140)), ("dfplayer", (60, 60, 190)),
            ("pam", (40, 150, 70)), ("buck", (190, 40, 150)), ("relay", (30, 90, 200)), ("psu", (90, 90, 95)), ("inlet", (230, 200, 40)),
            ("platter", (25, 25, 25)), ("pipe", (190, 190, 195))]


def group_of(n):
    n = n.lower()
    if n.startswith("rp_") and any(k in n for k in ("knob", "slider", "control_panel", "tonearm", "hinge", "platter")): return "Shell (PLA)"   # printed with the shell
    # add-ons: every add-on carries ONLY its own cable
    if n.startswith("ball_") or n.startswith("cable_outlet_B"): return "Disco ball + cord (not printed)"
    if n.startswith("cable_led_strip") or n.startswith("cable_strip_cable"): return "LED strip + cable (not printed)"
    if "big_red" in n or n.startswith("cable_button_cable"): return "Big red button + cable (not printed)"
    if "mains_cable" in n: return "Mains cable 5 m (not printed)"
    if n.startswith("plug_"): return "Plugs (not printed)"      # the plugs in the back pocket: always there
    if n in ("led holder", "test top"): return "Record head"
    if n == "led ring": return "Record head"
    if n.startswith("abwinklung"): return "Pipe"
    if n == "holder arduino" or n.startswith("small parts"): return "Electronics"
    if n.startswith("table_wide"): return "Side table, extended (for cables)"
    if n.startswith("table"): return "Side table"
    if any(k in n for k in ("knob", "slider", "control_panel", "tonearm", "hinge", "foot", "platter", "grooves", "label", "collar")) and "block_" not in n:
        return "Cosmetic (looks only)"
    if n.startswith("shell") or n == "housing": return "Shell (PLA)"
    if n.startswith("frame"): return "Frame (PETG)"
    if "mains_lid" in n or "mains_compartment" in n: return "Mains compartment"
    if n.startswith("wire_"): return "Wires"
    if "speaker" in n: return "Speakers"
    if "pipe" in n: return "Pipe"
    if "tilted" in n: return "Record head"
    if n.startswith("block_") or any(k in n for k in ("arduino", "dfplayer", "pam", "buck", "relay", "psu", "inlet")): return "Electronics"
    return "Other"


def colours_from_3mf(path):
    out = {}
    with zipfile.ZipFile(path) as z:
        root = etree.fromstring(z.read("3D/3dmodel.model"))
    for bm in root.iter(NS + "basematerials"):
        for b in bm.findall(NS + "base"):
            c = b.get("displaycolor", "#999999FF").lstrip("#")
            out[b.get("name")] = tuple(int(c[i:i + 2], 16) for i in (0, 2, 4))
    return out


def colour_for(name, known):
    if name in known: return known[name]
    for k, c in FALLBACK:
        if k in name.lower(): return c
    return (150, 150, 155)


def enc(a): return base64.b64encode(np.ascontiguousarray(a).tobytes()).decode()


def extras(zmin, table):
    """Optional round side table under the model (no floor)."""
    ex = {}
    if table:
        top = trimesh.creation.cylinder(radius=260, height=28, sections=96); top.apply_translation((0, 0, zmin - 14))
        ex["table_top"] = (top, (150, 105, 65))
        for a in (45, 135, 225, 315):
            x, y = 190 * np.cos(np.radians(a)), 190 * np.sin(np.radians(a))
            leg = trimesh.creation.cylinder(radius=14, segment=[(x, y, zmin - 28), (x * 1.25, y * 1.25, zmin - 330)], sections=24)
            ex[f"table_leg_{a}"] = (leg, (95, 65, 40))
        # extended table: big enough for the cables, the strip and the ball behind the box (about 1.5 m x 1.15 m)
        w = trimesh.creation.box(extents=(1500, 1150, 28)); w.apply_translation((50, 325, zmin - 14))
        ex["table_wide_top"] = (w, (150, 105, 65))
        for i, (lx, ly) in enumerate(((-640, -190), (740, -190), (-640, 840), (740, 840))):
            leg = trimesh.creation.cylinder(radius=18, segment=[(lx, ly, zmin - 28), (lx, ly, zmin - 700)], sections=24)
            ex[f"table_wide_leg_{i}"] = (leg, (95, 65, 40))
    return ex


def build():
    versions = []
    for v in VERSIONS:
        if v.get("like"):
            versions.append(dict(id=v["id"], label=v["label"], note=v["note"], like=v["like"], hiddenGroups=["Side table", "Side table, extended (for cables)"] + v["hide"]))
            continue
        path = os.path.join(PH, v["file"])
        known = colours_from_3mf(path)
        sc = trimesh.load(path, force="scene")
        parts, zmin, small = [], 1e9, []
        k = v.get("scale", 1.0)
        for name, g in sc.geometry.items():
            T, _ = sc.graph[name]
            m = g.copy(); m.apply_transform(T); m.apply_scale(k)
            if k != 1.0 and name.startswith("Part "):      # the starter has ~300 tiny parts: merge them
                small.append(m); continue
            zmin = min(zmin, m.bounds[0][2])
            parts.append((name, m, colour_for(name, {} if k != 1.0 else known)))
        if small:
            sm = trimesh.util.concatenate(small); zmin = min(zmin, sm.bounds[0][2])
            parts.append(("small parts (Arduino and others)", sm, colour_for("small parts", {})))
        zmin = float(zmin)
        for n, (m, c) in extras(zmin, v["table"]).items():
            parts.append((n, m, c))
        data = []
        for name, m, c in parts:
            data.append(dict(n=name, g=group_of(name), c=list(c), v=enc(m.vertices.astype(np.float32)), i=enc(m.faces.astype(np.uint32))))
        hidden = ["Side table", "Side table, extended (for cables)"] if v["table"] else []
        versions.append(dict(id=v["id"], label=v["label"], note=v["note"], dist=v["dist"], tz=v["tz"], hiddenGroups=hidden, parts=data, interactive=bool(v.get("interactive")), addons=bool(v.get("addons")), spin=([dict(name="ball_disco_ball_mirror", pivot=list(__import__("make_plugs").BALL_XY))] if v.get("addons") else []), normal=[0, -float(np.sin(np.radians(40))), float(np.cos(np.radians(40)))]))
    return versions


def main():
    versions = build()
    tpl = open(os.path.join(here, "viewer_template.html"), encoding="utf-8").read()
    html = tpl.replace("/*__DATA__*/", "const VERSIONS = " + json.dumps(versions, separators=(",", ":")) + ";")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f: f.write(html)
    print(f"{len(versions)} versions, {os.path.getsize(OUT) / 1e6:.1f} MB -> {os.path.normpath(OUT)}")


if __name__ == "__main__":
    main()
