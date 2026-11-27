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
# Garsent 3 inch full-range (measured by you): 9.7 cm across the mounting flange, 3.5 cm deep, 4 screw holes in the corners on a
# 7.7 cm circle (opposite holes 7.7 cm apart, aligned and centred on the frame), diaphragm 7.5 cm. Basket diameter (70) is a guess.
SPK_D, SPK_DEPTH, SPK_FLANGE, SPK_HOLES, SPK_CONE = 70.0, 35.0, 97.0, 77.0, 75.0
SPK_HOLE_D = 3.4                                   # M3 clearance (guess)
BAFFLE_T, BAFFLE_GAP = 6.0, 0.4                    # frame baffle plate behind the shell front wall holds the speaker; speaker flange sits behind the baffle
SPK_Y0 = -170.0 + 3.0 + BAFFLE_GAP + BAFFLE_T      # y of the speaker flange front (= back face of the baffle)
SPK_HOLE_OFF = SPK_HOLES / 2 * np.sqrt(0.5)        # x and z offset of each hole from the speaker centre (corners at 45 degrees)
SPK_X, SPK_Z = 85.0, 50.0            # speaker centre: left/right offset, height
BRICK_L, BRICK_W, BRICK_H = 220.0, 60.0, 40.0     # 12 V brick (measured by you: 22 cm long, 6 cm wide, 4 cm high)
GRILLE_D = 70.0                      # visible grille hole in the front wall
# Tool-less pipe joint: bayonet (3 lugs, ramped slots, one way only). The 9 wires meet in a plug connector inside the pipe, reached through a window in the pipe wall.
BAY_ANG = (80.0, 180.0, 310.0)        # lug angles in the LOCKED position (deg from +X, counter-clockwise seen from above); gaps 100/130/130 deg = fits one way only
BAY_TWIST, BAY_DROP = 35.0, 4.0       # turn the pipe 35 deg; it drops 4 mm along the ramp and the pads land on the spring pins
BAY_LUG_Z, BAY_LUG_H, BAY_LUG_W = 44.0, 6.0, 6.0   # lug: bottom height (locked), height, width (mm)
BAY_R_LUG, BAY_R_SLOT, BAY_R_IN = 33.5, 34.5, 30.0  # lug tip radius, slot bottom radius, collar bore radius = 30.5 (pipe radius 30)
WIN_Z0, WIN_Z1, WIN_W = 118.0, 150.0, 36.0     # window in the back (+Y) of the straight pipe, above the platter (top at z 111); a snap-on cover closes it
STEM_D = 66.0                        # round opening in the top for the hollow pipe (OD 60)
STEM_X, STEM_Y = 0.0, 0.0           # pipe position (x, y)
PLATTER_D, PLATTER_T, PLATTER_GAP = 315.0, 17.0, 5.0   # from the starter 3MF
MAINS_X0, MAINS_Y0 = 40.0, -90.0     # mains compartment inner corner (front-left); long enough for the 22 cm brick
MAINS_W, MAINS_D, MAINS_H = 122.0, 230.0, 70.0        # inside size; ends at y = 140, in front of the port pocket
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


def grille_cuts(sx):
    """Hex-perforated speaker grille (about 150 hexagonal holes, 4.6 mm across, 5.6 mm pitch) inside a 72 mm circle, with a solid centre
    boss and a 1 mm solid rim. Printable (every bar is >= 1 mm), keeps fingers, bugs and splashes out, looks like a metal grille.
    Also a 0.8 mm deep round bezel recess around it."""
    cuts, pitch, r_hole, R = [], 5.6, 2.3, 34.0
    row_h = pitch * np.sqrt(3) / 2
    holes = []
    for j in range(-8, 9):
        for i in range(-9, 10):
            x = i * pitch + (pitch / 2 if j % 2 else 0.0); z = j * row_h
            if np.hypot(x, z) > R - r_hole - 0.6 or np.hypot(x, z) < 6.5:        # keep the rim and a centre boss solid
                continue
            h = trimesh.creation.cylinder(radius=r_hole, height=WALL + 4, sections=6)
            h.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, (1, 0, 0)))
            h.apply_translation((sx + x, -D / 2 + WALL / 2, SPK_Z + z))
            holes.append(h)
    cuts.append(trimesh.util.concatenate(holes))
    bez = cyl(0, 0, -0.4, 86.0, 1.2)                                       # 0.8 mm deep, flush with the outside face after rotation
    bez.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, (1, 0, 0)))
    bez.apply_translation((sx, -D / 2 + 0.4, SPK_Z))
    cuts.append(bez)
    return cuts


def pipe_connector():
    """Mated 9-pin plug connector (guess: 2.5 mm pitch housing pair, about 28 x 14 x 8 mm) inside the pipe, right behind the window."""
    return box(-14, 6, 126, 28, 14, 8)


def housing():
    hw, hd = W / 2, D / 2
    shell = box(-hw, -hd, 0, W, D, H)
    inner = box(-hw + WALL, -hd + WALL, WALL, W - 2 * WALL, D - 2 * WALL, H - 2 * WALL)
    cuts = [inner,
            cyl(STEM_X, STEM_Y, H - WALL - 1, STEM_D, WALL + 2)]                       # stem opening in the top
    for sx in (-SPK_X, SPK_X):                                               # speaker grille holes, front wall
        for c in grille_cuts(sx):                                                                  # printed grille: concentric ring slots
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
        c = cyl(0, 0, -(WALL + 4) / 2, d, WALL + 4)
        c.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, (1, 0, 0)))
        c.apply_translation((x, hd - POCKET_D - WALL / 2, pz0 + POCKET_H / 2))
        port_cuts.append(c)
    return diff(body, port_cuts)


def mains_box():
    """Walls of the separate mains compartment (open top: it gets its own screw-on lid)."""
    x0, y0, z0 = MAINS_X0, MAINS_Y0, WALL
    return diff(box(x0 - WALL, y0 - WALL, z0, MAINS_W + 2 * WALL, MAINS_D + 2 * WALL, MAINS_H),
                [box(x0, y0, z0 + 1, MAINS_W, MAINS_D, MAINS_H)])


def _rev(profile, sections=64):
    """Revolve an (r, z) profile around the z axis (looks only, not printed)."""
    m = trimesh.creation.revolve(np.array(profile, float), sections=sections); m.merge_vertices(); return m


def speaker_parts(sx):
    """Garsent 3 inch full-range speaker, coloured pieces: flange with 4 screw holes, rubber surround, cone, dust cap, basket, magnet.
    Local frame: z = 0 front plane, +z towards the back (35 mm deep). Placed with its flange on the inner face of the front wall."""
    y0 = SPK_Y0
    flange = cyl(0, 0, 0, SPK_FLANGE, 3.0)
    flange = diff(flange, [cyl(ox, oy, -1, SPK_HOLE_D, 5) for ox in (-SPK_HOLE_OFF, SPK_HOLE_OFF) for oy in (-SPK_HOLE_OFF, SPK_HOLE_OFF)] +
                  [cyl(0, 0, -1, SPK_CONE + 2.0, 5)])                                          # open centre for the cone
    flange = trimesh.boolean.intersection([flange, box(-60, -46.0, -1, 120, 92.0, 6)], engine="manifold")          # flats top and bottom: 92 mm fits the 94 mm inside height
    surround = trimesh.creation.torus(major_radius=SPK_CONE / 2 - 1.5, minor_radius=2.2, major_sections=96, minor_sections=14); surround.apply_translation((0, 0, 3.2))
    cone = _rev([(SPK_CONE / 2 - 3.0, 3.6), (SPK_CONE / 2 - 2.2, 3.6), (15.2, 15.0), (14.0, 15.0), (SPK_CONE / 2 - 3.0, 3.6)])
    cap = trimesh.creation.uv_sphere(radius=17.0, count=[24, 64]); cap.apply_translation((0, 0, 12.0 + 17.0 - 5.0)); cap = cap.slice_plane((0, 0, 12.0), (0, 0, -1), cap=True)
    basket = _rev([(SPK_D / 2, 3.0), (SPK_D / 2 - 1.5, 3.0), (26.0, 17.0), (27.5, 17.0), (SPK_D / 2, 3.0)])
    magnet = trimesh.util.concatenate([cyl(0, 0, 17.0, 54.0, 5.0), cyl(0, 0, 22.0, 46.0, SPK_DEPTH - 22.0)])
    parts = {"flange": (flange, (30, 30, 34)), "surround": (surround, (20, 20, 22)), "cone": (cone, (205, 205, 210)),
             "dustcap": (cap, (60, 60, 66)), "basket": (basket, (50, 50, 56)), "magnet": (magnet, (90, 90, 96))}
    out = {}
    for k, (m, rgb) in parts.items():
        m = m.copy()
        m.apply_transform(trimesh.transformations.rotation_matrix(-np.pi / 2, (1, 0, 0)))      # axis -> +Y
        m.apply_translation((sx, y0, SPK_Z))
        m.visual.face_colors = list(rgb) + [255]
        out[k] = m
    return out


def speaker(sx):
    return trimesh.util.concatenate(list(speaker_parts(sx).values()))


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
        "dfplayer": union([box(-160, -120, 8, 90, 55, 17), place(ph.dfplayer(), -100, -100, 25)]),     # DFPlayer Mini on a breadboard 9 x 5.5 x 2 cm (measured by you)
        "pam8403_amp": union([box(-60, -110, 8, 30, 20, 5), box(-45, -110, 8, 20, 20, 15)]),     # amp board 3 x 2 cm, 0.5 cm high; volume dial block 2 x 2 x 1.5 cm sticks out 0.5 cm further (3.5 cm in all)
        "buck_converter": union([box(-125, 55, 8, 65, 39, 15), box(-165, 69, 9, 40, 12, 12)]),        # board 6.5 x 3.9 x 1.5 cm; the ~4 cm DC plug sits on the left end (12 V in), 5 V out at the right end
        "relay_module": place(ph.relay_module(), MAINS_X0 + 66, 95.0, WALL + 5),
        "psu_brick_12V": box(MAINS_X0 + 2, MAINS_Y0 + 3, WALL + 2, BRICK_W, BRICK_L, BRICK_H),        # 22 x 6 x 4 cm (DC plug end at the front, AC end at the back)
        "mains_inlet_and_fuse (guess)": box(MAINS_X0 + 66, MAINS_Y0 + MAINS_D - 30, WALL + 36, 55, 25, 28),
        "pipe_to_button": place(ph.button_pipe(), 0, 0, H - WALL),
        "pipe_connector_9pin": pipe_connector(),
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
