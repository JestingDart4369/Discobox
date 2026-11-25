#!/usr/bin/env python3
"""
make_blockout.py — rough block-out of the Discobox record-player housing.

Run:   python3 cad/make_blockout.py     (needs: pip install trimesh manifold3d numpy networkx lxml)
Output: cad/placeholders/blockout_assembly.3mf   one object per part (toggle them in your CAD / slicer)
        cad/placeholders/blockout_assembly.stl   everything merged into one mesh

Coordinates (mm): origin = centre of the housing floor, Z up, FRONT of the record player = -Y,
back = +Y (USB / mains / cable plugs), left = -X. Everything here is a ROUGH BLOCK for planning
space; sizes marked (guess) are not from a datasheet. Edit the numbers below and rerun.
"""
import os
import numpy as np
import trimesh

import importlib.util
_spec = importlib.util.spec_from_file_location(
    "ph", os.path.join(os.path.dirname(os.path.abspath(__file__)), "make_placeholders.py"))
ph = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ph)
box, cyl, union, diff = ph.box, ph.cyl, ph.union, ph.diff

OUT = ph.OUT

# ── numbers to tune ─────────────────────────────────────────────────────────
W, D, H = 340.0, 340.0, 100.0        # housing outside: width, depth, height (guess)
WALL = 3.0                           # wall / floor / top thickness
SPK_D, SPK_DEPTH = 80.0, 40.0        # speaker frame dia + depth (guess: check the datasheet)
SPK_X, SPK_Z = 85.0, 50.0            # speaker centre: left/right offset, height
GRILLE_D = 70.0                      # visible grille hole in the front wall
STEM_D = 66.0                        # round opening in the top for the hollow pipe (OD 60)
STEM_X, STEM_Y = 0.0, 0.0           # pipe position (x, y)
PLATTER_D, PLATTER_T, PLATTER_GAP = 315.0, 17.0, 5.0   # from the starter 3MF
MAINS_X0, MAINS_Y0 = 40.0, 35.0      # mains compartment inner corner (front-left)
MAINS_W, MAINS_D, MAINS_H = 122.0, 105.0, 70.0        # inside size (guess); ends at y = 140, in front of the port pocket
# recessed port pocket in the back wall (width, depth, height, bottom height)
POCKET_W, POCKET_D, POCKET_H, POCKET_Z0 = 300.0, 22.0, 50.0, 15.0
# port holes in the pocket's inner face: name -> (x position, hole diameter). Right side = over the mains compartment.
PORTS = {
    "mains inlet (Swiss plug cable)": (135.0, 22.0),
    "outlet B Type 13 (disco ball)": (85.0, 35.0),
    "strip connector GX16": (-135.0, 16.0),
    "button connector GX12": (-105.0, 12.0),
    "spare (e.g. second button)": (-75.0, 12.0),
}


def place(m, x, y, z):
    m = m.copy()
    m.apply_translation((x, y, z))
    return m


def rotz(m, deg):
    m = m.copy()
    m.apply_transform(trimesh.transformations.rotation_matrix(np.radians(deg), (0, 0, 1)))
    return m


def housing():
    hw, hd = W / 2, D / 2
    shell = box(-hw, -hd, 0, W, D, H)
    inner = box(-hw + WALL, -hd + WALL, WALL, W - 2 * WALL, D - 2 * WALL, H - 2 * WALL)
    cuts = [inner,
            cyl(STEM_X, STEM_Y, H - WALL - 1, STEM_D, WALL + 2)]                       # stem opening in the top
    for sx in (-SPK_X, SPK_X):                                               # speaker grille holes, front wall
        c = cyl(0, 0, 0, GRILLE_D, WALL + 2)
        c.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, (1, 0, 0)))
        c.apply_translation((sx, -hd + WALL / 2, SPK_Z))
        cuts.append(c)
    body = diff(shell, cuts)

    # Recessed port pocket in the back wall: the plugs sit set back so cables do not stick out.
    # A 3 mm liner box closes the pocket towards the inside; the ports are holes in its inner face.
    px0, pz0 = -POCKET_W / 2, POCKET_Z0
    liner = box(px0 - WALL, hd - POCKET_D - WALL, pz0 - WALL, POCKET_W + 2 * WALL, POCKET_D + WALL, POCKET_H + 2 * WALL)
    body = union([body, liner])
    pocket = box(px0, hd - POCKET_D, pz0, POCKET_W, POCKET_D + 1, POCKET_H)
    port_cuts = [pocket]
    for x, d in PORTS.values():
        c = cyl(0, 0, 0, d, WALL + 4)
        c.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, (1, 0, 0)))
        c.apply_translation((x, hd - POCKET_D - WALL / 2, pz0 + POCKET_H / 2))
        port_cuts.append(c)
    return diff(body, port_cuts)


def mains_box():
    """Walls of the separate mains compartment (open top: it gets its own screw-on lid)."""
    x0, y0, z0 = MAINS_X0, MAINS_Y0, WALL
    return diff(box(x0 - WALL, y0 - WALL, z0, MAINS_W + 2 * WALL, MAINS_D + 2 * WALL, MAINS_H),
                [box(x0, y0, z0 + 1, MAINS_W, MAINS_D, MAINS_H)])


def speaker(sx):
    c = cyl(0, 0, 0, SPK_D, SPK_DEPTH)
    c.apply_transform(trimesh.transformations.rotation_matrix(-np.pi / 2, (1, 0, 0)))  # axis -> +Y
    c.apply_translation((sx, -D / 2 + WALL, SPK_Z))
    return c


def main():
    arduino = ph.arduino()
    arduino = rotz(arduino, -90)                       # USB / jack edge now points to the back (+Y)
    b = arduino.bounds
    arduino = place(arduino, -50 - b[0][0], 60 - b[0][1], 8)      # on 5 mm standoffs; USB edge faces the back wall, no hole: open the housing to reach it

    parts = {
        "housing": housing(),
        "mains_compartment_walls": mains_box(),
        "speaker_left": speaker(-SPK_X),
        "speaker_right": speaker(SPK_X),
        "arduino": arduino,
        "dfplayer": place(ph.dfplayer(), -120, -100, 8),
        "pam8403_amp (guess)": box(-90, -100, 8, 21, 18, 6),
        "buck_converter (guess)": box(-120, 60, 8, 50, 25, 15),
        "relay_module": place(ph.relay_module(), MAINS_X0 + 5, MAINS_Y0 + 5, WALL + 5),
        "psu_brick_12V (guess)": box(MAINS_X0 + 5, MAINS_Y0 + 35, WALL + 2, 110, 50, 32),
        "mains_inlet_and_fuse (guess)": box(MAINS_X0 + 60, MAINS_Y0 + MAINS_D - 30, WALL + 36, 55, 25, 28),
        "pipe_to_button": place(ph.button_pipe(), 0, 0, H - WALL),
        "platter": box(-1, -1, 0, 2, 2, 1),   # replaced below
    }
    platter = cyl(0, 0, H + PLATTER_GAP, PLATTER_D, PLATTER_T, 128)
    parts["platter"] = platter

    scene = trimesh.Scene()
    for name, m in parts.items():
        scene.add_geometry(m, node_name=name, geom_name=name)
    os.makedirs(OUT, exist_ok=True)
    scene.export(os.path.join(OUT, "blockout_assembly.3mf"))
    trimesh.util.concatenate(list(parts.values())).export(os.path.join(OUT, "blockout_assembly.stl"))

    # Bambu Lab P1S: 256 x 256 x 256 mm build volume. Keep ~4 mm margin for the skirt / purge line.
    BED = 252.0
    for name, m in parts.items():
        s = m.bounds[1] - m.bounds[0]
        fits = sorted(s[:2])[1] <= BED and s[2] <= BED
        print(f"{name:32s} {s[0]:6.1f} x {s[1]:6.1f} x {s[2]:6.1f}   P1S: {'ok' if fits else 'TOO BIG, split it'}")


if __name__ == "__main__":
    main()
