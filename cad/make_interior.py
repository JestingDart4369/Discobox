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
       "gnd": (20, 20, 20), "cable": (40, 150, 90)}


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
BUCK_OUT, BUCK_IN = (-95, 72, 14), (-120, 72, 14)
JACK = (-105, 110, 14)       # barrel-jack terminals (guess)
GNDB = (-105, 125, 14)       # GND block (guess)
SHIFT = (-70, 125, 12)       # 74AHCT125 (guess)
BRICK_L = (MX + 5, 95, 20)   # brick low-voltage lead leaves on the left
RELAY = (MX + 30, 50, 18)
INLET = (MX + 85, 138, 40)
OUTLET_IN = (P_OUT, PORT_Y, PZ)

wires = {
    # mains (only inside the mains compartment)
    "mains_live_inlet_to_relay_COM": ("230", 3, [(P_MAINS, PORT_Y - 2, PZ), (MX + 85, 140, 45), (MX + 85, 140, 25), (MX + 50, 60, 24)]),
    "mains_live_relay_NO_to_outlet": ("230", 3, [(MX + 40, 58, 24), (MX + 40, 125, 52), (P_OUT, PORT_Y - 2, PZ)]),
    "mains_neutral_straight_through": ("230", 3, [(P_MAINS, PORT_Y - 2, PZ - 6), (MX + 50, 140, 40), (P_OUT, PORT_Y - 2, PZ - 6)]),
    # 12 V
    "12V_brick_to_barrel_jack": ("12", 3, [BRICK_L, (WALLX, 95, 20), (WALLX - 20, 100, 14), JACK]),
    "12V_jack_to_strip_port_via_fuse": ("12", 3, [JACK, (-105, 140, 14), (P_STRIP + 3, PORT_Y - 2, PZ)]),
    "12V_jack_to_buck_in": ("12", 2, [JACK, (-110, 90, 14), BUCK_IN]),
    # 5 V
    "5V_buck_to_arduino": ("5", 2, [BUCK_OUT, (-70, 85, 14), (-50, 100, 14)]),
    "5V_buck_to_relay_coil": ("5", 2, [BUCK_OUT, (-70, 60, 10), (WALLX - 10, 50, 10), (WALLX, 50, 12), RELAY]),
    "5V_buck_to_dfplayer_amp": ("5", 2, [BUCK_OUT, (-90, 20, 10), (-100, -90, 12)]),
    # data and button
    "data_D11_via_74AHCT125": ("data", 1.5, [(-30, 105, 14), SHIFT]),
    "data_74AHCT125_to_strip_port": ("data", 1.5, [SHIFT, (-70, 140, 14), (P_STRIP + 6, PORT_Y - 2, PZ + 4)]),
    "button_D2_to_pipe": ("data", 1.5, [(-30, 85, 14), (-10, 40, 40), (0, 0, 90)]),
    "button_GND_pipe_to_port": ("gnd", 1.5, [(0, 0, 90), (-5, 40, 60), (P_BTN, PORT_Y - 2, PZ)]),
    "button_signal_to_port": ("data", 1.5, [(-30, 85, 14), (-60, 120, 30), (P_BTN + 3, PORT_Y - 2, PZ + 3)]),
    # GND star block
    "GND_jack_to_gnd_block": ("gnd", 2.5, [JACK, GNDB]),
    "GND_block_to_buck": ("gnd", 2, [GNDB, (-115, 95, 10), (-120, 72, 10)]),
    "GND_block_to_arduino": ("gnd", 2, [GNDB, (-70, 115, 8), (-48, 105, 14)]),
    "GND_block_to_strip_port": ("gnd", 2.5, [GNDB, (-110, 140, 10), (P_STRIP - 3, PORT_Y - 2, PZ - 3)]),
    # speakers
    "speaker_left_pair": ("5", 1.5, [(-100, -90, 12), (-95, -120, 20), (-mb.SPK_X, -130, mb.SPK_Z)]),
    "speaker_right_pair": ("5", 1.5, [(-85, -90, 12), (0, -110, 12), (mb.SPK_X, -130, mb.SPK_Z)]),
}


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
