#!/usr/bin/env python3
"""
make_shell_frame.py — two-part enclosure: inner FRAME (PETG, carries all parts) + lift-off SHELL (PLA, cosmetic).

Run: python3 cad/make_shell_frame.py   (same packages as make_blockout.py)
Output (cad/placeholders/):
  frame.stl, shell.stl, mains_lid.stl            whole parts (for CAD / checking)
  shell_q1..q4.stl, frame_q1..q4.stl             quarters that fit the P1S bed (print, then join with screws / glue)
  shell_frame_assembly.3mf                       frame + shell + mains lid + parts, one object each
The shell has no floor: it lifts straight up off the frame after removing 4 M3 screws (from below, into heat-set
inserts in the shell's corner columns). Sizes marked (guess) are not from a datasheet. Edit numbers, rerun.
"""
import os
import numpy as np
import trimesh
import importlib.util
here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("mb", os.path.join(here, "make_blockout.py"))
mb = importlib.util.module_from_spec(spec); spec.loader.exec_module(mb)
box, cyl, union, diff = mb.box, mb.cyl, mb.union, mb.diff
W, D, H, WALL = mb.W, mb.D, mb.H, mb.WALL
hw, hd = W / 2, D / 2
PLATE_T = 3.0
FIT = 0.4                       # clearance between frame plate and shell inner wall
COL_OFF = 7.0                   # corner column centre distance from the inner wall
COL = 12.0                      # corner column size
HOLE_INSERT, HOLE_SCREW = 4.0, 3.4


from shapely.geometry import Polygon

# ── module mounts (snap-fit cradles) and cable clips ────────────────────────
PAD_H = 5.0     # PCB height above the plate (matches make_blockout.py)
# name: (x0, y0, size x, size y, PCB thickness, pad height). x0,y0 = lower-left corner. (guess) = measure your part.
MODULES = {
    "dfplayer":      (-120, -100, 20.8, 20.4, 1.0, PAD_H),
    "pam8403_amp":   (-90, -100, 21.0, 18.0, 1.6, PAD_H),       # (guess)
    "buck_converter": (-120, 60, 50.0, 25.0, 1.6, PAD_H),       # (guess)
    "level_shifter_74AHCT125": (-100, 100, 30.0, 20.0, 1.6, PAD_H),   # perfboard (guess)
    "barrel_jack_terminals": (-160, 95, 25.0, 15.0, 1.6, PAD_H),      # (guess)
    "gnd_block":     (-160, 115, 25.0, 12.0, 1.6, PAD_H),             # (guess)
    "relay_module":  (mb.MAINS_X0 + 5, mb.MAINS_Y0 + 5, 50.0, 26.0, 1.6, PAD_H),
}
# cable clips: (x, y, rotation deg about Z). Channel runs along Y at 0 deg, along X at 90.
CLIPS = [(-135, 135, 0), (-105, 135, 0), (-95, 30, 0), (-100, -60, 0), (-100, -128, 0),
         (-3, -118, 90), (-10, 30, 0)]


def _hook_profile(sign, thick):
    """Snap hook profile in the X/Z plane, PCB edge at x = 0, hook reaches over the PCB (x > 0)."""
    return None


def snap_arm(pcb_t, pad_h, thick, width):
    """One hook arm along +Y, PCB edge at x=0, arm outside (x<0), hook over the PCB. Y from 0..width."""
    hl = pad_h + pcb_t + 0.2
    pts = [(-0.3 - thick, 0), (-0.3, 0), (-0.3, hl), (1.2, hl), (1.2, hl + 0.5), (-0.3, hl + 2.0), (-0.3 - thick, hl + 2.0)]
    m = trimesh.creation.extrude_polygon(Polygon(pts), height=width)
    m.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, (1, 0, 0)))   # profile y -> z, extrude -> -y
    m.apply_translation((0, width, 0))
    return m


def rotz_about(m, deg, cx, cy):
    m = m.copy()
    m.apply_transform(trimesh.transformations.rotation_matrix(np.radians(deg), (0, 0, 1), point=(cx, cy, 0)))
    return m


def module_mount(x0, y0, sx, sy, t, pad_h):
    parts = []
    for px, py in [(0.5, 0.5), (sx - 5.5, 0.5), (0.5, sy - 5.5), (sx - 5.5, sy - 5.5)]:   # 4 corner pads
        parts.append(box(x0 + px, y0 + py, PLATE_T - 0.5, 5, 5, pad_h + 0.5))
    aw = min(8.0, sy - 6)
    # flexible arms on the two X ends (thin, 1.6 mm), fixed lips on the two Y sides (thick, 3 mm)
    left = snap_arm(t, pad_h, 1.6, aw); left.apply_translation((x0, y0 + (sy - aw) / 2, PLATE_T))
    right = snap_arm(t, pad_h, 1.6, aw); right.apply_transform(trimesh.transformations.rotation_matrix(np.pi, (0, 0, 1)))
    right.apply_translation((x0 + sx, y0 + (sy + aw) / 2, PLATE_T))
    fw = min(8.0, sx - 6)
    front = snap_arm(t, pad_h, 3.0, fw); front.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, (0, 0, 1)))
    front.apply_translation((x0 + (sx + fw) / 2, y0, PLATE_T))
    back = snap_arm(t, pad_h, 3.0, fw); back.apply_transform(trimesh.transformations.rotation_matrix(-np.pi / 2, (0, 0, 1)))
    back.apply_translation((x0 + (sx - fw) / 2, y0 + sy, PLATE_T))
    parts += [left, right, front, back]
    # a little base under each hook, so it is fused to the plate
    return parts


def brick_stops():
    """Corner stops and two cable-tie slots for the 12 V brick (110 x 50 guess) in the mains compartment."""
    x0, y0 = mb.MAINS_X0 + 5, mb.MAINS_Y0 + 35
    parts = []
    for cx, cy in [(x0 - 3, y0 - 3), (x0 + 110, y0 - 3), (x0 - 3, y0 + 50), (x0 + 110, y0 + 50)]:
        parts.append(box(cx, cy, PLATE_T, 3, 3, 12))
    return parts, [(x0 + 30, y0 - 8), (x0 + 80, y0 - 8)]


CLIP_D = 6.0        # wire bundle diameter the clip holds (mm); tune it
CLIP_W = 6.0        # clip width along the cable (mm)


def small_clip(d=CLIP_D, w=CLIP_W):
    """Small J/C-shaped hook: a post, an arm that folds over the cable and a lip with a slanted entry slit.
    Opening faces +X, cable runs along Y. Origin = centre of the footprint, Z=0 = top of the plate."""
    L = d + 4.6                       # overall length
    h = d + 2.6                       # overall height
    lipz = 0.6 * d                    # slit height above the plate
    post = Polygon([(0, 0), (2, 0), (2, h), (0, h)])
    arm = Polygon([(0, h - 2), (L, h - 2), (L, h), (0, h)])
    lip = Polygon([(L - 2, h), (L, h), (L, lipz), (L - 2, lipz + 2.0)])
    prof = post.union(arm).union(lip)
    m = trimesh.creation.extrude_polygon(prof, height=w)
    m.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, (1, 0, 0)))
    m.apply_translation((-L / 2, w / 2, 0))
    return m


def clip_mesh(x, y, rot):
    c = rotz_about(small_clip(), rot, 0, 0)
    c.apply_translation((x, y, PLATE_T - 0.5))
    c = trimesh.util.concatenate([c, box(x - 7, y - 4, PLATE_T - 0.5, 14, 8, 1.5)]) if False else c
    return c


def rx90(m, sign=1):
    m.apply_transform(trimesh.transformations.rotation_matrix(sign * np.pi / 2, (1, 0, 0)))
    return m


def col_pos():
    cx, cy = hw - WALL - COL_OFF, hd - WALL - COL_OFF
    return [(sx * cx, sy * cy) for sx in (-1, 1) for sy in (-1, 1)]


def shell():
    h = mb.housing()
    h = diff(h, [box(-hw - 1, -hd - 1, -1, W + 2, D + 2, WALL + 1)])        # remove the floor: the frame is the floor
    cols = []
    for x, y in col_pos():
        c = box(x - COL / 2, y - COL / 2, 0, COL, COL, H - WALL)
        cols.append(c)
    h = union([h] + cols)
    cuts = []
    for x, y in col_pos():
        cuts.append(cyl(x, y, -1, HOLE_INSERT, 11))                         # insert hole from below
        cuts.append(cyl(x, y, 9, HOLE_SCREW, 30))
    # vent slots in both side walls (above the 12 V brick / buck converter)
    for sx in (-1, 1):
        for i in range(6):
            s = box(sx * hw - 2, -10 + i * 14, 40, 4, 8, 30)
            cuts.append(s)
    return diff(h, cuts)


def frame():
    iw, idp = W - 2 * WALL - 2 * FIT, D - 2 * WALL - 2 * FIT
    plate = box(-iw / 2, -idp / 2, 0, iw, idp, PLATE_T)
    add = [plate, mb.mains_box()]
    # speaker saddles
    for sx in (-mb.SPK_X, mb.SPK_X):
        s = box(sx - 38, -hd + WALL + 2, PLATE_T, 76, 34, mb.SPK_Z - 40 + 3)
        cr = cyl(0, 0, -50, mb.SPK_D + 0.4, 100)
        rx90(cr, -1).apply_translation((sx, -hd + WALL, mb.SPK_Z))
        add.append(diff(s, [cr]))
    # Arduino standoffs at the 4 datasheet holes (board rotated -90 deg, USB edge to the back)
    for hx, hy in [(13.97, 2.54), (15.24, 50.80), (66.04, 7.62), (66.04, 35.56)]:
        x, y = hy - 50, -hx + 60 + 68.6
        add.append(cyl(x, y, PLATE_T, 6.5, 5))
    for name, (x0, y0, sx, sy, t, ph_) in MODULES.items():
        add += module_mount(x0, y0, sx, sy, t, ph_)
    stops, ties = brick_stops()
    add += stops
    for x, y, r in CLIPS:
        add.append(clip_mesh(x, y, r))
    # socket collar for the removable pipe: the 60 mm pipe slides in from the top, 2 M3 set screws hold it
    collar = diff(cyl(mb.STEM_X, mb.STEM_Y, PLATE_T, 72, 85), [cyl(mb.STEM_X, mb.STEM_Y, PLATE_T - 1, 61, 90)])
    add.append(collar)
    body = union(add)
    cuts = []
    for ang in (0, 90):
        sc_ = cyl(0, 0, -40, 3.0, 80)                                         # set-screw holes through the collar wall
        sc_.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, (0, 1, 0)))
        sc_.apply_transform(trimesh.transformations.rotation_matrix(np.radians(ang), (0, 0, 1)))
        sc_.apply_translation((mb.STEM_X, mb.STEM_Y, 75))
        cuts.append(sc_)
    for tx, ty in ties:                                                      # 3 x 6 mm slots for cable ties around the brick
        cuts.append(box(tx, ty, -1, 3, 6, PLATE_T + 2))
    for x, y in col_pos():
        cuts.append(cyl(x, y, -1, HOLE_SCREW, PLATE_T + 2))                 # screw from below
    for hx, hy in [(13.97, 2.54), (15.24, 50.80), (66.04, 7.62), (66.04, 35.56)]:
        x, y = hy - 50, -hx + 60 + 68.6
        cuts.append(cyl(x, y, -1, 2.6, PLATE_T + 7))                        # M3 self-tap
    return diff(body, cuts)


def mains_lid():
    x0, y0 = mb.MAINS_X0 - WALL, mb.MAINS_Y0 - WALL
    w, d = mb.MAINS_W + 2 * WALL, mb.MAINS_D + 2 * WALL
    lid = box(x0, y0, WALL + mb.MAINS_H, w, d, 3)
    cuts = [cyl(x0 + 5, y0 + 5, 0, 3.4, 200), cyl(x0 + w - 5, y0 + 5, 0, 3.4, 200),
            cyl(x0 + 5, y0 + d - 5, 0, 3.4, 200), cyl(x0 + w - 5, y0 + d - 5, 0, 3.4, 200)]
    return diff(lid, cuts)


def quarters(m):
    out = []
    for i, (sx, sy) in enumerate([(-1, 1), (1, 1), (-1, -1), (1, -1)], 1):
        x0 = 0 if sx > 0 else -W
        y0 = 0 if sy > 0 else -D
        q = trimesh.boolean.intersection([m, box(x0, y0, -5, W, D, 400)], engine="manifold")
        out.append((i, q))
    return out


def main():
    os.makedirs(mb.OUT, exist_ok=True)
    parts = {"shell (PLA)": shell(), "frame (PETG)": frame(), "mains_lid (PETG)": mains_lid()}
    for n, m in parts.items():
        m.export(os.path.join(mb.OUT, n.split()[0] + ".stl"))
    for n in ("shell", "frame"):
        m = parts[n + " (PLA)" if n == "shell" else n + " (PETG)"]
        for i, q in quarters(m):
            if len(q.faces):
                q.export(os.path.join(mb.OUT, f"{n}_q{i}.stl"))
                s = q.bounds[1] - q.bounds[0]
                print(f"{n}_q{i}: {s[0]:.0f} x {s[1]:.0f} x {s[2]:.0f}  P1S: {'ok' if max(s[:2]) <= 252 else 'TOO BIG'}  watertight={q.is_watertight}")
    sc = trimesh.Scene()
    for n, m in parts.items():
        sc.add_geometry(m, node_name=n, geom_name=n)
    for n, g in trimesh.load(os.path.join(mb.OUT, "blockout_assembly.3mf"), force="scene").geometry.items():
        if n in ("arduino", "dfplayer", "relay_module", "speaker_left", "speaker_right", "pipe_to_button") or "guess" in n:
            sc.add_geometry(g, node_name=n, geom_name=n)
    sc.export(os.path.join(mb.OUT, "shell_frame_assembly.3mf"))
    for n, m in parts.items():
        print(f"{n}: watertight={m.is_watertight}, volume={m.volume/1000:.0f} cm3")


if __name__ == "__main__":
    mb.main = mb.main
    main()
