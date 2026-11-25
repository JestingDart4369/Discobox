#!/usr/bin/env python3
"""
make_viewer.py — builds ONE self-contained HTML viewer (WebGL, no internet needed) with several model versions.

Run: python3 cad/make_viewer.py     -> doc/viewer/discobox_viewer.html
Open the HTML file in a browser. Left side: version tabs, view buttons, shell see-through slider, and a tree to show
or hide groups and single parts (floor, side table, electronics, wires, record, ...).

Add a version: add one line to VERSIONS below (a 3MF from cad/placeholders/), run the script again.
"""
import os, io, json, base64, zipfile
import numpy as np
import trimesh
from lxml import etree

here = os.path.dirname(os.path.abspath(__file__))
PH = os.path.join(here, "placeholders")
OUT = os.path.join(here, "..", "doc", "viewer", "discobox_viewer.html")

# newest first. dist / tz = start camera distance and target height.
VERSIONS = [
    dict(id="v5", label="v5 record player (current)", file="discobox_full.3mf", dist=950, tz=140, table=True,
         note="Tilted 300 mm record with the LED ring top of the starter model, flat platter, one hollow bent pipe (removable from the top), knobs, tonearm, PETG frame with module mounts and clips, PLA shell, wires. Side table hidden (switch it on at the bottom); looks-only parts are grouped as Cosmetic."),
    dict(id="v4", label="v4 shell + frame", file="shell_frame_assembly.3mf", dist=800, tz=60, table=False,
         note="Before the record-player look: lift-off shell on a frame, mains compartment with lid, module block-outs."),
    dict(id="v3", label="v3 block-out", file="blockout_assembly.3mf", dist=800, tz=60, table=False,
         note="First block-out: housing with speakers, port pocket, mains compartment, parts, straight pipe and flat platter."),
]

NS = "{http://schemas.microsoft.com/3dmanufacturing/core/2015/02}"
FALLBACK = [("shell", (200, 25, 30)), ("housing", (200, 25, 30)), ("frame", (70, 70, 75)), ("mains_lid", (230, 120, 20)),
            ("mains_compartment", (230, 120, 20)), ("speaker", (60, 60, 60)), ("arduino", (0, 130, 140)), ("dfplayer", (60, 60, 190)),
            ("pam", (40, 150, 70)), ("buck", (190, 40, 150)), ("relay", (30, 90, 200)), ("psu", (90, 90, 95)), ("inlet", (230, 200, 40)),
            ("platter", (25, 25, 25)), ("pipe", (190, 190, 195))]


def group_of(n):
    n = n.lower()
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
    return ex


def build():
    versions = []
    for v in VERSIONS:
        path = os.path.join(PH, v["file"])
        known = colours_from_3mf(path)
        sc = trimesh.load(path, force="scene")
        parts, zmin = [], 1e9
        for name, g in sc.geometry.items():
            T, _ = sc.graph[name]
            m = g.copy(); m.apply_transform(T)
            zmin = min(zmin, m.bounds[0][2])
            parts.append((name, m, colour_for(name, known)))
        zmin = float(zmin)
        for n, (m, c) in extras(zmin, v["table"]).items():
            parts.append((n, m, c))
        data = []
        for name, m, c in parts:
            data.append(dict(n=name, g=group_of(name), c=list(c), v=enc(m.vertices.astype(np.float32)), i=enc(m.faces.astype(np.uint32))))
        hidden = ["Side table"] if v["table"] else []
        versions.append(dict(id=v["id"], label=v["label"], note=v["note"], dist=v["dist"], tz=v["tz"], hiddenGroups=hidden, parts=data))
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
