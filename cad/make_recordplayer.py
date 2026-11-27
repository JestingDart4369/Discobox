#!/usr/bin/env python3
"""
make_recordplayer.py — record-player look.

* FLAT platter + slip mat lie on the deck (where a record normally lies); the pipe passes through its centre.
* ONE hollow pipe (OD 60, ID 48): vertical out of a socket collar in the frame, bends 40 degrees and ends in a
  100 mm hub on the back of the record. Cables run through it. It slides out of the top (2 M3 set screws).
* The RECORD (300 mm, printed) is TILTED 40 degrees like your starter model, label 100 mm = hub size, the LED ring
  block-out from your model (OD 70, ID 50, 10 mm) sits in a pocket in the back, red button in the centre.
* Knobs, slider, tonearm, hinges, feet.  Tonearm pivot position follows a normal turntable: pivot at the right rear,
  arm reaching the platter (real turntables: 222 mm pivot-to-spindle; scaled down here because the housing is 340 mm).

Used by make_full_assembly.py; run alone to print the numbers:  python3 cad/make_recordplayer.py
Coordinates as in make_blockout.py: origin = centre of the housing floor, Z up, FRONT = -Y.
"""
import numpy as np
import trimesh
from shapely.geometry import Point
import importlib.util, os
here = os.path.dirname(os.path.abspath(__file__))
sp = importlib.util.spec_from_file_location("make_shell_frame", os.path.join(here, "make_shell_frame.py"))
sf = importlib.util.module_from_spec(sp); sp.loader.exec_module(sf)
mb = sf.mb
box, cyl, diff, union = mb.box, mb.cyl, mb.diff, mb.union
H, W, D, WALL = mb.H, mb.W, mb.D, mb.WALL

TILT = 40.0                          # degrees (starter model), record faces the front
S, Cc = np.sin(np.radians(TILT)), np.cos(np.radians(TILT))
PX, PY = mb.STEM_X, mb.STEM_Y        # pipe axis
PIPE_OD, PIPE_ID = 60.0, 48.0
REC_D, REC_T = 300.0, 22.0           # record (12 inch) = starter holder 17 mm + diffuser plate 5 mm
LABEL_D = 100.0                      # label = hub
HUB_T = 14.0
RING_OD, RING_ID, RING_T = 70.0, 50.0, 10.0   # LED ring block-out from the starter model (OD 70, ID about 50)
PLAT_D, PLAT_T, MAT_D, MAT_T = 270.0, 8.0, 262.0, 3.0   # flat platter + mat on the deck
ANG_LEN = 62.0                       # length of the angled pipe section behind the record
DECK_TOP = H + PLAT_T + MAT_T
RIM_CLEAR = 30.0                     # lowest rim above the mat
# record centre P0 so that the lowest rim clears the mat, and the angled section meets the vertical pipe at y = PY
Z0 = DECK_TOP + RIM_CLEAR + (REC_D / 2) * S + REC_T * Cc
TOTAL = ANG_LEN + REC_T                           # from the record front plane back to the bend
Y0 = PY - TOTAL * S
ZB = Z0 - TOTAL * Cc                              # bend height
NORMAL = np.array([0, -S, Cc])


def C(m, rgb):
    m = m.copy(); m.visual.face_colors = list(rgb) + [255]; return m


def annulus(ro, ri, h, z0=0.0):
    a = trimesh.creation.annulus(r_min=ri, r_max=ro, height=h, sections=128)
    a.apply_translation((0, 0, z0 + h / 2)); return a


def disc(d, h, z0=0.0):
    c = trimesh.creation.cylinder(radius=d / 2, height=h, sections=128)
    c.apply_translation((0, 0, z0 + h / 2)); return c


def tilt_place(m):
    """Local record frame (origin = centre of the record front face, +Z = face normal) -> world."""
    m = m.copy()
    m.apply_transform(trimesh.transformations.rotation_matrix(np.radians(TILT), (1, 0, 0)))
    m.apply_translation((0, Y0, Z0))
    return m


STARTER = os.path.join(here, "starter")      # parts of YOUR starter model (Shalplatte_Discobox.3mf), exported 1:1 in mm


def _starter(name, dz):
    m = trimesh.load(os.path.join(STARTER, name + ".stl"))
    m.apply_translation((0, 0, dz))
    return m


def record_parts():
    """Top of the starter model, positions 1:1 (so the ring pocket, ring width and ring-to-diffuser distance are exact).
    Starter z: ring -8..2, holder -7..10, diffuser plate ("Test Top") 10..15. Here z = 0 is the front face, so shift -15."""
    p = {}
    holder = _starter("led_holder", -15.0)                              # 315 mm disc with the ring pocket
    holder = trimesh.boolean.intersection([holder, cyl(0, 0, -40, REC_D, 60, 128)], engine="manifold")   # trimmed to 300 mm
    p["record_body"] = C(holder, (20, 10, 34))
    top = _starter("test_top", -15.0)                                   # 120 mm diffuser plate, hole 16 mm for the button
    p["diffuser_plate"] = C(top, (238, 224, 255))
    p["led_ring_block"] = C(_starter("led_ring", -15.0), (47, 243, 255))
    zt = -5.0                                                           # holder front face
    g = [annulus(r + 1.0, r, 0.3, zt) for r in np.arange(66.0, REC_D / 2 - 4, 6.0)]
    p["record_grooves"] = C(trimesh.util.concatenate(g), (80, 44, 125))
    p["record_label"] = C(annulus(LABEL_D / 2, 8.3, 0.8, 0), (255, 140, 40))          # label stuck on the diffuser plate
    # 16 mm push button (PM161H style): metal bezel ring, plastic cap with a domed top that moves a little when pressed
    p["button_collar"] = C(annulus(9.0, 7.9, 2.8, 0.8), (205, 205, 210))
    stem = disc(15.4, 3.2, 1.0)
    dome = trimesh.creation.uv_sphere(radius=14.0, count=[48, 96]); dome.apply_translation((0, 0, 4.2 + 2.2 - 14.0))
    dome = dome.slice_plane((0, 0, 4.2), (0, 0, 1), cap=True)
    p["button_cap"] = C(trimesh.util.concatenate([stem, dome]), (255, 47, 163))
    return p


BEND_R = 70.0                        # centre-line radius of the smooth bend
HUB_R = LABEL_D / 2                  # hub radius (like the starter model: 100 mm)
HUB_LEN = 24.0                       # straight hub behind the record
CONE_LEN = 12.0                      # smooth taper from hub to pipe


def _smooth(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3 - 2 * t)


def _tube(centres, radii, sections=64):
    """Closed tube mesh along a polyline with varying radius (parallel-transport frames)."""
    centres = np.asarray(centres, float); radii = np.asarray(radii, float)
    keep = [0] + [i for i in range(1, len(centres)) if np.linalg.norm(centres[i] - centres[i - 1]) > 0.2]   # drop (almost) duplicate rings
    centres, radii = centres[keep], radii[keep]
    tang = np.gradient(centres, axis=0)
    tang /= np.linalg.norm(tang, axis=1)[:, None]
    ref = np.array([1.0, 0, 0]) if abs(tang[0][0]) < 0.9 else np.array([0, 1.0, 0])
    nrm = np.cross(tang[0], ref); nrm /= np.linalg.norm(nrm)
    ang = np.linspace(0, 2 * np.pi, sections, endpoint=False)
    verts = []
    for i, (c, t, r) in enumerate(zip(centres, tang, radii)):
        if i:
            nrm = nrm - np.dot(nrm, t) * t                   # keep the frame perpendicular to the new tangent
            nrm /= np.linalg.norm(nrm)
        b = np.cross(t, nrm)
        verts.append(c + r * (np.outer(np.cos(ang), nrm) + np.outer(np.sin(ang), b)))
    verts = np.concatenate(verts)
    n = len(centres); faces = []
    for i in range(n - 1):
        for j in range(sections):
            a_, b_ = i * sections + j, i * sections + (j + 1) % sections
            c_, d_ = (i + 1) * sections + j, (i + 1) * sections + (j + 1) % sections
            faces += [[a_, b_, d_], [a_, d_, c_]]
    vc = len(verts)
    verts = np.vstack([verts, centres[0], centres[-1]])
    for j in range(sections):
        faces.append([vc, (j + 1) % sections, j])
        k = (n - 1) * sections
        faces.append([vc + 1, k + j, k + (j + 1) % sections])
    m = trimesh.Trimesh(verts, faces, process=True)
    m.fix_normals()
    return m


def pipe_path(extra_start=0.0, extra_end=0.0, r_out=True):
    """Centre line + radii: straight up, smooth bend, straight along the record axis, taper, hub."""
    B = np.array([PX, PY, ZB]); up = np.array([0, 0, 1.0])
    th = np.radians(TILT)
    t = BEND_R * np.tan(th / 2)
    A, E = B - t * up, B + t * NORMAL
    pts, rad = [], []
    ro = PIPE_OD / 2 if r_out else PIPE_ID / 2
    hub = HUB_R if r_out else PIPE_ID / 2
    def add(p, r): pts.append(np.asarray(p, float)); rad.append(r)
    add(np.array([PX, PY, 40.0 - extra_start]), ro); add(A - up * 10, ro); add(A, ro)
    centre = A + BEND_R * (NORMAL - up) / np.linalg.norm(NORMAL - up) * 0  # (arc centre computed below)
    # circular arc from A to E
    cen = A + BEND_R * np.array([0, -1.0, 0])      # bend turns toward -Y (front)
    for k in range(1, 25):
        a_ = th * k / 24
        p = cen + BEND_R * np.array([0, np.cos(a_), np.sin(a_)])
        add(p, ro)
    P0 = np.array([0, Y0, Z0])
    def axis(sd): return P0 - NORMAL * sd
    s_e = TOTAL - t
    s_cone0 = REC_T + HUB_LEN + CONE_LEN
    for k in range(0, 13):                         # smooth taper (hub radius at the record side)
        sd = s_cone0 - CONE_LEN * k / 12
        add(axis(sd), ro + (hub - ro) * _smooth(k / 12))
    add(axis(REC_T - 2.0 - extra_end), hub)     # ends 2 mm INSIDE the record, so hub and record are joined
    return np.array(pts), np.array(rad)


def pipe():
    """ONE hollow part: pipe + smooth bend + hub (like the 100 mm Abwinklung of the starter model)."""
    c, r = pipe_path()
    outer = _tube(c, r)
    ci, ri = pipe_path(extra_start=1.0, extra_end=1.0, r_out=False)
    inner = _tube(ci, ri)
    notch = box(PX - 10, PY + 15, 39.0, 20, 20, 14)        # cutout at the foot (back) so the wire bundle is not pinched where the pipe stands on the lip
    return C(diff(outer, [inner, notch]), (90, 90, 95))


def hull_parts():
    p = {}
    for sx in (-1, 1):
        for sy in (-1, 1):
            p[f"foot_{'L' if sx < 0 else 'R'}{'F' if sy < 0 else 'B'}"] = C(cyl(sx * 140, sy * 140, -10, 32, 10), (25, 25, 25))
    # flat platter + slip mat on the deck, centred on the pipe, with a hole for the pipe
    p["platter_flat"] = C(diff(disc(PLAT_D, PLAT_T, H), [disc(PIPE_OD + 1, PLAT_T + 2, H - 1)]), (52, 22, 100))
    p["platter_mat"] = C(diff(disc(MAT_D, MAT_T, H + PLAT_T), [disc(PIPE_OD + 1, MAT_T + 2, H + PLAT_T - 1)]), (22, 8, 48))
    # (control panel with the three deco knobs and the slider are gone; so are the two hinges)
    # tonearm: pivot at the right rear (as on a normal turntable), arm reaches over the platter
    bx, by = 135.0, 150.0
    arm_end = (128.0, 45.0)
    p["tonearm_base"] = C(cyl(bx, by, H, 32, 16), (200, 200, 205))
    p["tonearm_tube"] = C(trimesh.creation.cylinder(radius=4, segment=[(bx, by, H + 20), (arm_end[0], arm_end[1], H + PLAT_T + MAT_T + 12)], sections=24), (200, 200, 205))
    p["tonearm_counterweight"] = C(trimesh.creation.cylinder(radius=9, segment=[(bx, by + 4, H + 20), (bx + 3, by + 30, H + 20)], sections=32), (122, 47, 255))
    # SIMPLE printable headshell: one block and one cartridge block, both straight along the arm (no angle, no tiny parts),
    # plus a post under the arm end so the 108 mm arm does not hang free while printing.
    ex, ey, ez = arm_end[0], arm_end[1], H + PLAT_T + MAT_T + 12
    th = np.arctan2(arm_end[1] - by, arm_end[0] - bx)
    def at_arm(m, rgb):
        m = m.copy()
        m.apply_transform(trimesh.transformations.rotation_matrix(th, (0, 0, 1)))
        m.apply_translation((ex, ey, ez))
        return C(m, rgb)
    p["tonearm_headshell"] = at_arm(box(-4, -8, -6, 36, 16, 6), (32, 32, 36))               # 36 x 16 x 6 mm
    p["tonearm_cartridge"] = at_arm(box(6, -6, -12, 22, 12, 6), (255, 47, 163))             # 22 x 12 x 6 mm, hangs under the headshell
    p["tonearm_post"] = C(cyl(ex, ey, H, 8, ez - 4 - H), (200, 200, 205))                  # 8 mm support post under the arm end
    p["tonearm_tube_collar"] = C(cyl(bx, by, H + 14, 12, 12), (170, 170, 175))          # gimbal ring at the pivot
    p["tonearm_rest"] = C(box(bx - 40, by - 30, H, 8, 20, 18), (60, 60, 65))
    return p


def all_parts():
    p = hull_parts()
    for n, m in record_parts().items():
        p["tilted_" + n] = tilt_place(m)
    p["pipe_hollow_removable"] = pipe()
    return p


if __name__ == "__main__":
    print(f"record centre ({0:.0f}, {Y0:.0f}, {Z0:.0f}), bend at z={ZB:.0f}, pipe axis ({PX:.0f},{PY:.0f})")
    lowest = Z0 - (REC_D / 2) * S - REC_T * Cc
    print(f"lowest rim z = {lowest:.0f}, mat top {DECK_TOP:.0f}")
    for n, m in all_parts().items():
        print(f"{n:30s} {np.round(m.bounds[1]-m.bounds[0],0)} watertight={m.is_watertight}")
