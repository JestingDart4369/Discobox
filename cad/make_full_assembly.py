#!/usr/bin/env python3
"""
make_full_assembly.py — everything in one coloured file: shell, frame, mains lid, platter, pipe, all module block-outs, wires.

Run: python3 cad/make_full_assembly.py   (needs the packages of make_blockout.py + shapely, mapbox_earcut)
Output (cad/placeholders/): discobox_full.3mf (one coloured object per part), discobox_full.stl (one merged mesh, no colour)
"""
import os, io, contextlib
import numpy as np
import trimesh
import importlib.util
here = os.path.dirname(os.path.abspath(__file__))
def load(n):
    sp = importlib.util.spec_from_file_location(n, os.path.join(here, n + ".py"))
    m = importlib.util.module_from_spec(sp); sp.loader.exec_module(m); return m
sf, wi, rp = load("make_shell_frame"), load("make_interior"), load("make_recordplayer")
mb = sf.mb

C = {  # RGB
    "shell": (200, 25, 30), "frame": (70, 70, 75), "mains_lid": (230, 120, 20), "platter": (25, 25, 25),
    "pipe": (190, 190, 195), "speaker": (60, 60, 60), "arduino": (0, 130, 140), "dfplayer": (60, 60, 190),
    "pam": (40, 150, 70), "buck": (190, 40, 150), "relay": (30, 90, 200), "psu": (90, 90, 95),
    "inlet": (230, 200, 40), "shifter": (30, 160, 200), "jack": (140, 90, 40), "gnd": (30, 30, 30),
}

def colour(m, rgb):
    m = m.copy(); m.visual.face_colors = list(rgb) + [255]; return m

def pick(n):
    for k, v in (("speaker", "speaker"), ("arduino", "arduino"), ("dfplayer", "dfplayer"), ("pam", "pam"),
                 ("buck", "buck"), ("relay", "relay"), ("psu", "psu"), ("inlet", "inlet"),
                 ("pipe", "pipe"), ("platter", "platter")):
        if k in n: return C[v]
    return (150, 150, 150)

def add_colours(path, cols):
    """trimesh writes no colours into 3MF: add a basematerials block and point every object at its colour."""
    import zipfile, shutil
    from lxml import etree
    ns = "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"
    tmp = path + ".tmp"
    with zipfile.ZipFile(path) as zin, zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as zout:
        for item in zin.infolist():
            data = zin.read(item.filename)
            if item.filename == "3D/3dmodel.model":
                root = etree.fromstring(data)
                res = root.find(f"{{{ns}}}resources")
                objs = res.findall(f"{{{ns}}}object")
                ids = [int(o.get("id")) for o in objs]
                mid = max(ids) + 1
                bm = etree.Element(f"{{{ns}}}basematerials", id=str(mid))
                for i, o in enumerate(objs):
                    r, g, b = cols.get(o.get("name"), (150, 150, 150))
                    etree.SubElement(bm, f"{{{ns}}}base", name=o.get("name"), displaycolor="#%02X%02X%02XFF" % (r, g, b))
                    o.set("pid", str(mid)); o.set("pindex", str(i))
                res.insert(0, bm)
                data = etree.tostring(root, xml_declaration=True, encoding="UTF-8")
            zout.writestr(item, data)
    shutil.move(tmp, path)


def main():
    parts = {}
    parts["shell_PLA"] = colour(sf.shell(), C["shell"])
    parts["frame_PETG"] = colour(sf.frame(), C["frame"])
    parts["mains_lid_PETG"] = colour(sf.mains_lid(), C["mains_lid"])
    with contextlib.redirect_stdout(io.StringIO()):
        mb.main()
    base = trimesh.load(os.path.join(mb.OUT, "blockout_assembly.3mf"), force="scene").geometry
    skip = ("housing", "mains_compartment_walls", "relay_module", "platter", "pipe_to_button")      # shell/frame replace these; relay added below
    for n, g in base.items():
        if n in skip: continue
        parts["block_" + n.replace(" ", "_")] = colour(g, pick(n))
    # modules that make_blockout does not have (guess sizes, same numbers as the mounts)
    for n, (x0, y0, sx, sy, t, ph) in sf.MODULES.items():
        if n in ("dfplayer", "pam8403_amp", "buck_converter"):
            continue
        key = "relay" if n == "relay_module" else "shifter" if "shifter" in n else "jack" if "jack" in n else "gnd"
        parts["block_" + n] = colour(sf.box(x0, y0, sf.PLATE_T + ph, sx, sy, 6 if key != "relay" else 17), C[key])
    for n, m in rp.all_parts().items():          # record-player look: tilted platter + record, knobs, tonearm, feet
        parts["rp_" + n] = m
    for n, m in wi.build().items():
        parts[n] = m
    sc = trimesh.Scene()
    for n, m in parts.items():
        sc.add_geometry(m, node_name=n, geom_name=n)
    path = os.path.join(mb.OUT, "discobox_full.3mf")
    sc.export(path)
    add_colours(path, {n: tuple(int(v) for v in m.visual.face_colors[0][:3]) for n, m in parts.items()})
    trimesh.util.concatenate(list(parts.values())).export(os.path.join(mb.OUT, "discobox_full.stl"))
    # overlap check: clip / mount parts vs module blocks (bounding boxes), just a warning
    boxes = {n: m.bounds for n, m in parts.items() if n.startswith("block_") and "speaker" not in n and "pipe" not in n and "platter" not in n}
    for x, y, r in sf.CLIPS:
        c = sf.clip_mesh(x, y, r).bounds
        for n, b in boxes.items():
            if (c[0][:2] < b[1][:2]).all() and (c[1][:2] > b[0][:2]).all() and c[1][2] > b[0][2]:
                print(f"WARNING clip at ({x},{y}) overlaps {n}")
    print(len(parts), "objects -> discobox_full.3mf / discobox_full.stl")

if __name__ == "__main__":
    main()
