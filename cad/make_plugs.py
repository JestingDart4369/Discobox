#!/usr/bin/env python3
"""
make_plugs.py — plugs in the back ports and the external cables at their REAL length (for the 3D model, not printed).

parts() -> {name: coloured trimesh}.  Used by make_full_assembly.py. Run directly to print the cable list.
Each external cable is modelled as a smooth lead out of its plug that then lies on the floor as a flat coil.
The coil is sized so that the centre line of the cable is exactly the real length (see CABLES).
Sizes of plugs and cables are typical values (guess); measure your parts.
"""
import os, sys, importlib.util
import numpy as np
import trimesh
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
import make_blockout as mb
import make_interior as mi

HD = mb.D / 2                       # back wall outside, y
Y_FACE = mi.PORT_Y + 1.0            # inner face of the port pocket (plug flange sits on it)
Y_OPEN = HD                         # pocket opening
PZ = mi.PZ
FLOOR = -10.0                       # underside of the feet

BALL_CORD_MM = [0.0]
# name: (port x, cable length mm, cable OD mm, colour, coil centre (x, y), coil outer radius, far end)
CABLES = {
    "strip_cable_GX16":  dict(x=mb.PORTS["strip connector GX16"][0], L=2200.0, od=9.0, rgb=(35, 35, 38), centre=(-480, 520), r_out=150.0, far="bare"),
    "button_cable_GX12": dict(x=mb.PORTS["button connector GX12"][0], L=2200.0, od=9.0, rgb=(35, 35, 38), centre=(-130, 560), r_out=150.0, far="bare"),
    "mains_cable_5m":    dict(x=mb.PORTS["mains inlet (Swiss plug cable)"][0], L=5000.0, od=10.0, rgb=(235, 235, 235), centre=(450, 640), r_out=240.0, far="swiss"),
}


def C(m, rgb):
    m = m.copy(); m.visual.face_colors = list(rgb) + [255]; return m


def ycyl(x, y0, y1, z, d, sections=40):
    return trimesh.creation.cylinder(radius=d / 2, segment=[(x, y0, z), (x, y1, z)], sections=sections)


def ybox(x, y0, y1, z, w, h):
    m = trimesh.creation.box(extents=(w, abs(y1 - y0), h)); m.apply_translation((x, (y0 + y1) / 2, z)); return m


def cat(*ms):
    return trimesh.util.concatenate(list(ms))


def spiral_length(r_out, r_in, pitch):
    return np.pi * (r_out ** 2 - r_in ** 2) / pitch


def coil_path(cx, cy, r_out, L, od, start_angle):
    """Flat spiral on the floor, from r_out inwards, centre line exactly L long."""
    pitch = od + 1.0
    r_in = np.sqrt(max(r_out ** 2 - L * pitch / np.pi, 1.0))
    pts, s, r, a, z = [], 0.0, r_out, start_angle, FLOOR + od / 2
    prev = None
    while s < L:
        p = np.array([cx + r * np.cos(a), cy + r * np.sin(a), z])
        if prev is not None:
            s += np.linalg.norm(p - prev)
        pts.append(p); prev = p
        da = 2.0 / max(r, 20.0)                 # about 2 mm of cable per step
        a += da; r -= pitch * da / (2 * np.pi)
    return pts, r_in


def path_length(pts):
    return float(sum(np.linalg.norm(np.array(b) - np.array(a)) for a, b in zip(pts[:-1], pts[1:])))


def lead(x, start_z, od, end):
    """Smooth lead from the plug (straight out of the pocket) down to the floor and to 'end' (coil start)."""
    p0 = np.array([x, Y_OPEN + 4.0, start_z])
    p3 = np.array(end)
    p1 = p0 + np.array([0, 60.0, 0]); p2 = p3 + np.array([0, -60.0, 120.0 if p3[2] < start_z else 0])
    t = np.linspace(0, 1, 40)[:, None]
    return [((1 - u) ** 3 * p0 + 3 * (1 - u) ** 2 * u * p1 + 3 * (1 - u) * u ** 2 * p2 + u ** 3 * p3) for (u,) in t]


def tube_mesh(pts, od, rgb):
    segs = []
    for a, b in zip(pts[:-1], pts[1:]):
        a, b = np.asarray(a, float), np.asarray(b, float)
        if np.linalg.norm(b - a) > 1e-6:
            segs.append(trimesh.creation.cylinder(radius=od / 2, segment=[a, b], sections=10))
    return C(trimesh.util.concatenate(segs), rgb)


def ribbon(pts, width, thick):
    """Flat tape lying on the floor along a path (width in the floor plane, thickness in z)."""
    P = np.array(pts, float); n = len(P)
    t = np.gradient(P, axis=0); t[:, 2] = 0; t /= np.linalg.norm(t, axis=1)[:, None]
    side = np.stack([-t[:, 1], t[:, 0], np.zeros(n)], axis=1) * (width / 2)
    z0 = np.array([0, 0, -thick / 2]); z1 = np.array([0, 0, thick / 2])
    V = np.concatenate([P + side + z1, P - side + z1, P - side + z0, P + side + z0])   # 4 rings: TL TR BR BL
    F = []
    for i in range(n - 1):
        for a, b in ((0, 1), (1, 2), (2, 3), (3, 0)):
            a0, a1, b0, b1 = a * n + i, a * n + i + 1, b * n + i, b * n + i + 1
            F += [[a0, b0, b1], [a0, b1, a1]]
    F += [[0, 1, 2], [0, 2, 3], [n - 1, 3 * n + n - 1, 2 * n + n - 1], [n - 1, 2 * n + n - 1, n + n - 1]]
    m = trimesh.Trimesh(V, F, process=True); m.fix_normals(); return m


def strip_parts(cx, cy, end, L=3000.0, width=12.0, thick=3.0, leds_per_m=60):
    """3 m LED strip (WS2805, 60 LED/m) coiled flat, continuing from the end of the strip cable."""
    a0 = np.arctan2(end[1] - cy, end[0] - cx); r0 = np.hypot(end[0] - cx, end[1] - cy)
    pts, r_in = coil_path(cx, cy, r0 - width - 1.0, L, width, a0)
    # trim to L exactly
    keep, s = [pts[0]], 0.0
    for a, b in zip(pts[:-1], pts[1:]):
        st = np.linalg.norm(b - a)
        if s + st >= L: keep.append(a + (b - a) * ((L - s) / st)); s = L; break
        keep.append(b); s += st
    for q in keep: q[2] = FLOOR + thick / 2
    tape = C(ribbon(keep, width, thick), (240, 240, 240))
    # LEDs every 1000/60 mm along the centre line
    cum = np.concatenate([[0], np.cumsum([np.linalg.norm(b - a) for a, b in zip(keep[:-1], keep[1:])])])
    leds = []
    for d in np.arange(8.0, cum[-1], 1000.0 / leds_per_m):
        i = min(np.searchsorted(cum, d), len(keep) - 1); b = trimesh.creation.box(extents=(5, 5, 1.6)); b.apply_translation(keep[i] + np.array([0, 0, thick / 2 + 0.7])); leds.append(b)
    return tape, C(trimesh.util.concatenate(leds), (255, 236, 170)), float(cum[-1]), len(leds)


def big_red_button(end):
    """Separate big red push button, e-stop / quiz-buzzer style: 90 mm base, 70 mm dome, about 45 mm tall. Lies on the floor at the cable end."""
    p = np.array([end[0], end[1], FLOOR])
    base = trimesh.creation.cylinder(radius=45, segment=[p, p + [0, 0, 16]], sections=72)
    ring = trimesh.creation.cylinder(radius=39, segment=[p + [0, 0, 16], p + [0, 0, 22]], sections=72)
    R = 55.0; h = 24.0
    dome = trimesh.creation.uv_sphere(radius=R, count=[48, 96]); dome.apply_translation(p + [0, 0, 22 + h - R])
    dome = dome.slice_plane(p + [0, 0, 22], (0, 0, 1), cap=True)
    return C(trimesh.util.concatenate([base, ring]), (30, 30, 34)), C(dome, (225, 25, 35))


BALL_XY = (130.0, 330.0)             # position of the motor disco ball behind the box (viewer spins it in disco mode)


def disco_ball(cx, cy, r=100.0):
    """Motor disco ball: round base with motor housing and a shaft; the 20 cm mirror ball sits on the shaft and turns."""
    z0 = FLOOR
    base = trimesh.util.concatenate([
        trimesh.creation.cylinder(radius=60, segment=[(cx, cy, z0), (cx, cy, z0 + 12)], sections=64),          # foot plate
        trimesh.creation.cylinder(radius=34, segment=[(cx, cy, z0 + 12), (cx, cy, z0 + 60)], sections=64),     # motor housing
        trimesh.creation.cylinder(radius=40, segment=[(cx, cy, z0 + 60), (cx, cy, z0 + 66)], sections=64),     # turntable collar
        trimesh.creation.cylinder(radius=5, segment=[(cx, cy, z0 + 66), (cx, cy, z0 + 80)], sections=24),      # shaft
        trimesh.creation.cylinder(radius=6, segment=[(cx - 34, cy - 8, z0 + 30), (cx - 40, cy - 8, z0 + 30)], sections=20),   # cord entry
    ])
    ball = trimesh.creation.icosphere(subdivisions=3, radius=r); ball.apply_translation((cx, cy, z0 + 80 + r))
    rng = np.random.default_rng(5)
    g = 190 + rng.integers(0, 60, len(ball.faces))
    ball.visual.face_colors = np.stack([g, g, np.minimum(g + 15, 255), np.full(len(g), 255)], axis=1).astype(np.uint8)
    return ball, C(base, (40, 40, 48)), np.array([cx - 42, cy - 8, z0 + 30])


def swiss_plug(x, y, z0, along):
    """Type 12/13 style plug lying on the floor at (x, y), pins pointing along +/-X."""
    body = trimesh.creation.box(extents=(34, 50, 24)); body.apply_translation((x, y, z0 + 12))
    pins = [trimesh.creation.cylinder(radius=2.0, segment=[(x - 17, y + dy, z0 + 12), (x - 17 - 18, y + dy, z0 + 12)], sections=12) for dy in (-9.5, 0, 9.5)]
    return cat(C(body, (235, 235, 235)), *[C(p, (190, 190, 195)) for p in pins])


def parts():
    out = {}
    # ---- fixed plugs / sockets in the pocket (back) ----
    # GX16-4 for the strip: socket flange on the inner face, plug body to the opening
    for key, d_body, d_flange, rgb_sock in (("strip", 20.0, 26.0, (150, 150, 155)), ("button", 14.0, 19.0, (150, 150, 155))):
        x = mb.PORTS["strip connector GX16" if key == "strip" else "button connector GX12"][0]
        sock = cat(ycyl(x, Y_FACE, Y_FACE + 3, PZ, d_flange, 6), ycyl(x, Y_FACE + 3, Y_FACE + 9, PZ, d_body, 40))
        plug = cat(ycyl(x, Y_FACE + 9, Y_OPEN - 3, PZ, d_body + 2.0, 40), ycyl(x, Y_OPEN - 3, Y_OPEN + 4, PZ, d_body * 0.55, 30))
        out[f"plug_{key}_socket"] = C(sock, rgb_sock)
        out[f"plug_{key}_GX_plug"] = C(plug, (40, 40, 44))
    out["plug_spare_cap"] = C(cat(ycyl(mb.PORTS["spare (e.g. second button)"][0], Y_FACE, Y_FACE + 3, PZ, 19, 6), ycyl(mb.PORTS["spare (e.g. second button)"][0], Y_FACE + 3, Y_FACE + 8, PZ, 12.5, 30)), (90, 90, 95))
    # mains: cable gland on the back wall of the mains compartment with the cable going through
    xm = mb.PORTS["mains inlet (Swiss plug cable)"][0]
    out["plug_mains_gland"] = C(cat(ycyl(xm, Y_FACE, Y_FACE + 5, PZ, 28, 6), ycyl(xm, Y_FACE + 5, Y_OPEN - 4, PZ, 22, 40), ycyl(xm, Y_OPEN - 4, Y_OPEN + 4, PZ, 16, 30)), (30, 30, 34))
    # outlet B: Type 13 panel socket with a plug (disco ball cable) pushed in
    xo = mb.PORTS["outlet B Type 13 (disco ball)"][0]
    out["plug_outlet_B_socket"] = C(cat(ycyl(xo, Y_FACE, Y_FACE + 4, PZ, 44, 6), ycyl(xo, Y_FACE + 4, Y_FACE + 14, PZ, 34, 48)), (235, 235, 235))
    ball = cat(ycyl(xo, Y_FACE + 14, Y_OPEN + 2, PZ, 36, 48), ycyl(xo, Y_OPEN + 2, Y_OPEN + 22, PZ, 18, 30))
    out["plug_outlet_B_ball_plug"] = C(ball, (20, 20, 22))
    # disco ball: plugs into outlet B on the back; its own cord runs to the motor cap on top of the ball
    BX, BY = BALL_XY
    ball, bbase, centry = disco_ball(BX, BY)
    out["ball_disco_ball_mirror"] = ball; out["ball_disco_ball_base_motor"] = bbase
    p0 = np.array([xo, Y_OPEN + 22, PZ]); p3 = np.array(centry)
    p1 = p0 + [0, 60, 0]; p2 = p3 + [-70, -20, 40]
    cord = [((1 - u) ** 3 * p0 + 3 * (1 - u) ** 2 * u * p1 + 3 * (1 - u) * u ** 2 * p2 + u ** 3 * p3) for u in np.linspace(0, 1, 50)]
    out["cable_outlet_B_disco_ball_cord"] = tube_mesh(cord, 8.0, (20, 20, 22))
    BALL_CORD_MM[0] = path_length(cord)
    # ---- external cables at real length ----
    for name, c in CABLES.items():
        L, od = c["L"], c["od"]
        if name == "button_cable_GX12":
            # the big red button sits beside the box (front left, on the table); the cable goes over a small coil at the left side
            # (lead -> coil centre -> spiral outwards -> straight tail to the button); total length is exactly L
            cc = np.array([-300.0, 185.0]); pitch = od + 1.0; r_in = 14.0
            p0 = np.array([c["x"], Y_OPEN + 4.0, PZ]); top = np.array([cc[0], cc[1], FLOOR + od * 2.6])
            ld = [((1 - u) ** 3 * p0 + 3 * (1 - u) ** 2 * u * (p0 + [0, 70, 0]) + 3 * (1 - u) * u ** 2 * (top + [90, 0, 60]) + u ** 3 * top) for u in np.linspace(0, 1, 40)]
            L_s = L - path_length(ld) - 250.0
            r_out = float(np.sqrt(r_in ** 2 + L_s * pitch / np.pi))
            spiral, _ = coil_path(cc[0], cc[1], r_out, L_s, od, -np.pi / 2)
            spiral = spiral[::-1]                                       # centre -> outside, ends at angle -90 degrees (towards the front)
            drop = [top + (spiral[0] - top) * t for t in np.linspace(0, 1, 6)[1:]]
            body = ld + drop + spiral[1:]
            tail = L - path_length(body)
            endp = spiral[-1] + np.array([0.0, -tail, 0.0])
            pts = body + [endp]
            out["cable_" + name] = tube_mesh(pts, od, c["rgb"])
            c["_len_model"] = path_length(pts)
            bb, bc = big_red_button(endp)
            out["plug_big_red_button_base"] = bb; out["plug_big_red_button_dome"] = bc
            continue
        cx, cy = c["centre"]
        ang0 = np.arctan2(cy - (Y_OPEN + 30), 0)           # choose the coil start towards the plug side
        # spiral start point: nearest point of the outer ring to the plug side
        start_angle = -np.pi / 2 + 0.0
        spiral, r_in = coil_path(cx, cy, c["r_out"], L - 0.0, od, start_angle)
        start = spiral[0]
        ld = lead(c["x"], PZ, od, start)
        total = path_length(ld) + path_length(spiral)
        # trim the spiral so lead + coil = L exactly
        keep, s = [spiral[0]], path_length(ld)
        for a, b in zip(spiral[:-1], spiral[1:]):
            step = np.linalg.norm(b - a)
            if s + step >= L:
                f = (L - s) / step; keep.append(a + (b - a) * f); s = L; break
            keep.append(b); s += step
        pts = ld + keep
        out["cable_" + name] = tube_mesh(pts, od, c["rgb"])
        c["_len_model"] = path_length(pts)
        end = pts[-1]
        if name == "strip_cable_GX16":
            tape, leds, ln, nl = strip_parts(cx, cy, end)
            out["cable_led_strip_3m_tape"] = tape; out["cable_led_strip_3m_leds"] = leds
            c["_strip"] = (ln, nl)
        if name == "button_cable_GX12":
            bb, bc = big_red_button(end)
            out["plug_big_red_button_base"] = bb; out["plug_big_red_button_dome"] = bc
        if name == "strip_cable_GX16":
            continue
        if name == "button_cable_GX12":
            continue
        if c["far"] == "swiss":
            sp = swiss_plug(end[0], end[1], FLOOR, 1)
            out["plug_" + name + "_swiss_type13"] = sp
        else:
            out["plug_" + name + "_cut_end"] = C(trimesh.creation.cylinder(radius=od / 2 + 0.5, segment=[end, end + np.array([0, 0, 0.1])], sections=12), (200, 120, 40))
    return out


def write_doc():
    docp = os.path.join(here, "..", "doc", "CABLES.md")
    L = ["# Cables and plugs (real amounts)", "", "Generated by `python3 cad/make_plugs.py`. Internal lengths come from the wire routes of the 3D model plus 15 % slack, rounded up to 5 mm. Routes are planning routes: cut one wire to length and check it before you make the rest.", "",
         "## External cables (coiled behind the model)", "", "| Cable | Length | Outer diameter | Plug at the box | Far end |", "|---|---|---|---|---|",
         "| Strip cable, 4-core 1.0 mm2 (H05VV-F 4G1.0) | 2.20 m | about 9 mm | GX16-4 plug | bare, to the LED strip |",
         "| Cable of the separate big red button, same type (2 of the 4 cores used) | 2.20 m | about 9 mm | GX12-2 plug | big red buzzer-style button (90 mm base, 70 mm dome), beside the box |",
         "| Mains cable (Swiss extension, type 13) | 5.00 m | about 10 mm | through the cable gland into the mains compartment | Swiss plug into the wall |",
         "| Disco ball cord (motor ball: 20 cm mirror ball on a motor base) | about 0.5 m in the model (the real cord may differ) | about 8 mm | Type 13 plug in outlet B | motor of the ball |", "",
         "The two 2.2 m cables use 4.4 m of the 5 m roll in `BOM_enclosure.csv`.", "", "## Plugs and sockets in the back pocket", "", "| Position x (mm) | Part | Hole |", "|---|---|---|"]
    for n, (x, d) in mb.PORTS.items():
        L.append(f"| {x:.0f} | {n} | {d:.0f} mm |")
    L += ["", "## Internal wires", "", "| Wire | Net | Wire diameter in model | Cut length with slack |", "|---|---|---|---|"]
    tot = {}
    for k, (cls, d, pts) in mi.wires.items():
        ln = sum(np.linalg.norm(np.array(b, float) - np.array(a, float)) for a, b in zip(pts[:-1], pts[1:]))
        cut = int(np.ceil(ln * 1.15 / 5.0) * 5)
        tot[cls] = tot.get(cls, 0) + cut
        L.append(f"| {k.replace('_', ' ')} | {cls} | {d} mm | {cut} mm |")
    L += ["", "Total internal wire per net: " + ", ".join(f"{c} {v / 1000:.2f} m" for c, v in tot.items()) + ". Buy at least this much, plus spare."]
    open(docp, "w").write("\n".join(L) + "\n")


if __name__ == "__main__":
    write_doc()
    p = parts()
    print("name                      real mm   model mm")
    for n, c in CABLES.items():
        print(f"{n:24s} {c['L']:8.0f} {c['_len_model']:10.1f}")
    print("LED strip", CABLES["strip_cable_GX16"]["_strip"], "(mm, LEDs)")
    print(len(p), "parts")
