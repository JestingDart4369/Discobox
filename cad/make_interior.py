#!/usr/bin/env python3
"""
make_interior.py — block-out WITH wires: adds coloured wire tubes to the block-out and renders previews.

Run: python3 cad/make_interior.py   (needs trimesh, numpy, matplotlib + the packages of make_blockout.py)
Output: cad/placeholders/interior_with_wires.3mf  (one object per wire / part, coloured)
        cad/placeholders/interior_top.png, interior_iso.png  (previews, top lid and platter hidden)
Wire paths are ROUGH planning routes (not exact lengths). Colours: red 230 V, orange 12 V, magenta 5 V,
blue data, black GND, green = cable to the ports.
"""
import os
import numpy as np
import trimesh
import importlib.util
here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("mb", os.path.join(here, "make_blockout.py"))
mb = importlib.util.module_from_spec(spec); spec.loader.exec_module(mb)

hd = mb.D / 2
PORT_Y = hd - mb.POCKET_D - mb.WALL        # inner face of the port pocket
PZ = mb.POCKET_Z0 + mb.POCKET_H / 2        # port height
PX = {k: v[0] for k, v in mb.PORTS.items()}
P_MAINS, P_OUT = PX["mains inlet (Swiss plug cable)"], PX["outlet B Type 13 (disco ball)"]
P_STRIP, P_BTN = PX["strip connector GX16"], PX["button connector GX12"]
MX = mb.MAINS_X0
WALLX = MX - mb.WALL / 2                   # wall between mains compartment and low-voltage side

COL = {"230": (210, 0, 0), "12": (230, 140, 0), "5": (230, 30, 120), "data": (30, 80, 220),
       "gnd": (20, 20, 20), "cable": (40, 150, 90), "audio": (40, 190, 110), "led_r": (230, 40, 40), "led_g": (40, 190, 70), "led_b": (50, 110, 255)}


def tube(pts, d, rgb):
    segs = []
    for a, b in zip(pts[:-1], pts[1:]):
        a, b = np.array(a, float), np.array(b, float)
        if np.linalg.norm(b - a) < 1e-6:
            continue
        segs.append(trimesh.creation.cylinder(radius=d / 2, segment=[a, b], sections=10))
        segs.append(trimesh.creation.icosphere(subdivisions=1, radius=d / 2).apply_translation(b))
    m = trimesh.util.concatenate(segs)
    m.visual.face_colors = list(rgb) + [255]
    return m


# parts positions (see make_blockout.py)
ARD = (-45, 95, 14)          # middle of the Arduino
BUCK_OUT, BUCK_IN = (-73, 75, 18), (-137, 75, 18)     # terminals at the two short ends of the 6 cm buck: 12 V in (left), 5 V out (right)
GNDB = (-105, 125, 14)       # GND block (guess)
SHIFT = (-70, 125, 12)       # 74AHCT125 (guess)
BRICK_L = (MX + 32, -57, 22)   # 12 V lead leaves the DC end of the brick (front) through the compartment front wall
RELAY = (MX + 91, 108, 18)
INLET = (MX + 85, 138, 40)
OUTLET_IN = (P_OUT, PORT_Y, PZ)

wires = {
    # mains (only inside the mains compartment)
    "mains_live_inlet_to_relay_COM": ("230", 3, [(P_MAINS, PORT_Y - 2, PZ), (MX + 95, 140, 45), (MX + 95, 128, 30), (MX + 88, 112, 26)]),
    "mains_live_relay_NO_to_outlet": ("230", 3, [(MX + 100, 112, 26), (MX + 70, 130, 52), (P_OUT, PORT_Y - 2, PZ)]),
    "mains_neutral_straight_through": ("230", 3, [(P_MAINS, PORT_Y - 2, PZ - 6), (MX + 50, 140, 40), (P_OUT, PORT_Y - 2, PZ - 6)]),
    # mains to the 12 V brick (brick lies along Y in the mains compartment, AC end at the back, DC plug end at the front)
    "mains_live_inlet_to_brick": ("230", 3, [(MX + 93, 128, 50), (MX + 60, 131, 50), (MX + 34, 137, 32)]),
    "mains_neutral_inlet_to_brick": ("230", 3, [(MX + 93, 126, 46), (MX + 60, 129, 46), (MX + 30, 137, 28)]),
    # 12 V
    "12V_brick_to_buck_barrel_jack": ("12", 3, [BRICK_L, (MX + 32, -66, 22), (-45, -66, 24), (-70, 10, 26), (-100, 50, 28), (-143, 50, 24), (-143, 75, 18), BUCK_IN]),   # barrel jack is on the buck converter
    "12V_buck_in_to_strip_port_via_fuse": ("12", 3, [BUCK_IN, (-143, 100, 16), (-125, 128, 14), (-105, 140, 14), (P_STRIP + 3, PORT_Y - 2, PZ)]),
    # 5 V
    "5V_buck_to_arduino": ("5", 2, [BUCK_OUT, (-70, 85, 14), (-50, 100, 14)]),
    "5V_buck_to_relay_coil": ("5", 2, [BUCK_OUT, (-70, 60, 10), (WALLX - 10, 50, 10), (WALLX, 50, 12), RELAY]),
    "5V_buck_to_amp_pair": ("5", 2, [BUCK_OUT, (-70, 55, 12), (-90, 20, 10), (-85, -90, 12)]),                       # the pair 5 V + GND for the amp ...
    "GND_buck_to_amp_pair": ("gnd", 2, [(BUCK_OUT[0], BUCK_OUT[1] - 5, BUCK_OUT[2] - 2), (-86, 18, 8), (-80, -90, 10)]),
    "5V_amp_to_dfplayer": ("5", 1.5, [(-85, -90, 12), (-100, -96, 14), (-112, -92, 14)]),            # ... and on to the DFPlayer Mini
    "GND_amp_to_dfplayer": ("gnd", 1.5, [(-80, -90, 10), (-100, -100, 12), (-112, -96, 12)]),
    "audio_dfplayer_to_amp_L_R": ("audio", 1.5, [(-110, -86, 14), (-100, -80, 16), (-88, -84, 14)]),
    # piezo buzzer (D9) with GND
    "buzzer_D9": ("data", 1.5, [(-30, 105, 14), (-50, 60, 12), (-80, 20, 12), (-125, -10, 24), (-125, -20, 24)]),
    "buzzer_GND": ("gnd", 1.5, [(-105, 125, 14), (-143, 100, 10), (-150, 40, 10), (-148, -15, 24), (-142, -18, 24)]),
    # data and button
    "data_D11_via_74AHCT125": ("data", 1.5, [(-30, 105, 14), SHIFT]),
    "data_74AHCT125_to_strip_port": ("data", 1.5, [SHIFT, (-70, 140, 14), (P_STRIP + 6, PORT_Y - 2, PZ + 4)]),
    "serial_arduino_to_dfplayer_RX_TX": ("data", 1.5, [(-45, 95, 14), (-60, 20, 10), (-114, -86, 12)]),
    # separate big red button on the 2.2 m cable (GX12 port): 2 wires, pin not chosen yet (?)
    "bigbutton_signal_to_port": ("data", 1.5, [(-30, 105, 14), (-70, 128, 24), (P_BTN + 3, PORT_Y - 2, PZ + 3)]),
    "bigbutton_GND_to_port": ("gnd", 1.5, [(-105, 128, 14), (-108, 135, 20), (P_BTN - 3, PORT_Y - 2, PZ - 3)]),
    # GND star block
    "GND_buck_to_gnd_block": ("gnd", 2.5, [BUCK_IN, (-143, 95, 12), GNDB]),
    "GND_block_to_arduino": ("gnd", 2, [GNDB, (-70, 115, 8), (-48, 105, 14)]),
    "GND_block_to_strip_port": ("gnd", 2.5, [GNDB, (-110, 140, 10), (P_STRIP - 3, PORT_Y - 2, PZ - 3)]),
    # speakers
    "speaker_left_pair": ("audio", 1.5, [(-100, -90, 12), (-95, -120, 20), (-mb.SPK_X, -130, mb.SPK_Z)]),
    "speaker_right_pair": ("audio", 1.5, [(-85, -90, 12), (0, -110, 12), (mb.SPK_X, -130, mb.SPK_Z)]),
}


def _pipe_route(start_pts, end_axis_sd, off, side=0.0):
    """Wire from the box up through the hollow pipe (x offset keeps wires apart) to a point on the record head."""
    import make_recordplayer as rp
    c, r = rp.pipe_path()
    pts = [np.array(q, float) for q in start_pts]
    for q in c[::3]:
        pts.append(q + np.array([off, 0, 0]))
    P0 = np.array([0, rp.Y0, rp.Z0])
    end = P0 - rp.NORMAL * end_axis_sd + np.array([off + side, 0, 0])
    pts.append(end)
    return [tuple(p) for p in pts]


ARD_PIN = lambda dy: (-30, 85 + dy, 14)
_up = [(-40, 60, 20), (-15, 25, 35)]
HEAD = {}
for _k, (_cls, _d, _start, _sd, _off, _side) in {
    # button on the record: 2 wires for the switch, one wire per colour, one common ground for the three LEDs
    "button_switch_D2":            ("data", 1.5, ARD_PIN(0), 2.0, -10.0, 0.0),
    "button_switch_GND":           ("gnd", 1.5, (-105, 125, 14), 2.0, -7.0, 0.0),
    "button_LED_R_D3":             ("led_r", 1.5, ARD_PIN(4), 2.0, -4.0, 0.0),
    "button_LED_G_D5":             ("led_g", 1.5, ARD_PIN(8), 2.0, -1.0, 0.0),
    "button_LED_B_D6":             ("led_b", 1.5, ARD_PIN(12), 2.0, 2.0, 0.0),
    "button_LED_common_GND":       ("gnd", 1.5, (-105, 128, 14), 2.0, 5.0, 0.0),
    # RGB ring (16 x WS2812) on the record: 5 V, GND, data via 330 Ohm
    "ring_5V":                     ("5", 2, BUCK_OUT, 12.0, 8.0, 24.0),
    "ring_GND":                    ("gnd", 2, (-105, 122, 14), 12.0, 10.5, 24.0),
    "ring_data_D4_330ohm":         ("data", 1.5, ARD_PIN(16), 12.0, 13.0, 24.0),
}.items():
    wires[_k] = (_cls, _d, _pipe_route([_start] + _up, _sd, _off, _side))


def build():
    parts = {}
    for k, (c, d, p) in wires.items():
        parts["wire_" + k] = tube(p, d, COL[c])
    return parts


def render(parts_all, out_top, out_iso):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection
    for out, (elev, azim) in ((out_top, (90, -90)), (out_iso, (35, -60))):
        fig = plt.figure(figsize=(11, 11), dpi=110)
        ax = fig.add_subplot(111, projection="3d", proj_type="ortho")
        for name, m in parts_all.items():
            if name == "platter" or name == "pipe_to_button" and False:
                continue
            tri = m.triangles
            f = m.faces
            if name == "housing":      # hide the top lid
                keep = ~(m.triangles[:, :, 2] > mb.H - mb.WALL - 0.01).all(axis=1)
                tri = tri[keep]
                col, al = (0.6, 0.6, 0.6, 0.07), 0.07
            elif name.startswith("wire_"):
                col, al = np.array(m.visual.face_colors[0]) / 255.0, 1.0
            elif name.startswith("speaker"):
                col, al = (0.3, 0.3, 0.3, 0.8), 0.8
            elif name == "mains_compartment_walls":
                col, al = (0.9, 0.3, 0.3, 0.10), 0.10
            else:
                col, al = (0.5, 0.6, 0.75, 0.55), 0.55
            pc = Poly3DCollection(tri, facecolor=col, edgecolor=(0, 0, 0, 0.04) if not name.startswith("wire_") else "none", linewidths=0.2)
            ax.add_collection3d(pc)
        L = 180
        ax.set_xlim(-L, L); ax.set_ylim(-L, L); ax.set_zlim(0, 2 * L)
        ax.set_box_aspect((1, 1, 1))
        ax.view_init(elev=elev, azim=azim)
        ax.set_axis_off()
        ax.text(-L, L + 8, 0, "BACK (ports)", fontsize=11)
        ax.text(-L, -L - 15, 0, "FRONT (speakers)", fontsize=11)
        from matplotlib.lines import Line2D
        lab = [("230 V mains", "#d20000"), ("12 V", "#e68c00"), ("5 V", "#e61e78"), ("data / button", "#1e50dc"), ("GND", "#141414")]
        ax.legend(handles=[Line2D([0], [0], color=c, lw=3, label=l) for l, c in lab], loc="upper right")
        fig.savefig(out, bbox_inches="tight", facecolor="white")
        plt.close(fig)


def main():
    base = {}
    # reuse the parts from make_blockout by calling its main pieces
    import io, contextlib
    with contextlib.redirect_stdout(io.StringIO()):
        mb.main()
    sc = trimesh.load(os.path.join(mb.OUT, "blockout_assembly.3mf"), force="scene")
    for n, g in sc.geometry.items():
        base[n] = g
    wp = build()
    allp = dict(base); allp.update(wp)
    scene = trimesh.Scene()
    for n, m in allp.items():
        scene.add_geometry(m, node_name=n, geom_name=n)
    scene.export(os.path.join(mb.OUT, "interior_with_wires.3mf"))
    render(allp, os.path.join(mb.OUT, "interior_top.png"), os.path.join(mb.OUT, "interior_iso.png"))
    print("wires:", len(wp), "-> interior_with_wires.3mf, interior_top.png, interior_iso.png")


if __name__ == "__main__":
    main()
