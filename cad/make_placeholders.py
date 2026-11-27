#!/usr/bin/env python3
"""
make_placeholders.py — generates simple STL placeholder models of the Discobox parts.

Run:   python3 cad/make_placeholders.py
Needs: pip install trimesh manifold3d numpy

Units are millimetres. Every part has its own origin (see cad/README.md):
  - boards: origin = lower-left corner of the PCB, Z = 0 is the BOTTOM of the PCB
  - ring:   origin = centre of the ring, Z = 0 is the bottom of the PCB
  - button: origin = centre of the button, Z = 0 is the TOP face of the panel
These are placeholders for planning the enclosure (space, holes, standoffs),
not exact copies of the real parts. Values marked "typical" were NOT taken
from a datasheet in the project, check them with a caliper.
"""
import os
import numpy as np
import trimesh

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "placeholders")
os.makedirs(OUT, exist_ok=True)


def box(x0, y0, z0, dx, dy, dz):
    m = trimesh.creation.box(extents=(dx, dy, dz))
    m.apply_translation((x0 + dx / 2, y0 + dy / 2, z0 + dz / 2))
    return m


def cyl(cx, cy, z0, d, h, sections=64):
    m = trimesh.creation.cylinder(radius=d / 2, height=h, sections=sections)
    m.apply_translation((cx, cy, z0 + h / 2))
    return m


def union(parts):
    return trimesh.boolean.union(parts, engine="manifold")


def diff(a, cutters):
    return trimesh.boolean.difference([a] + cutters, engine="manifold")


def save(mesh, name):
    path = os.path.join(OUT, name)
    mesh.export(path)
    b = mesh.bounds
    size = b[1] - b[0]
    print(f"{name:34s} {size[0]:6.1f} x {size[1]:6.1f} x {size[2]:6.1f} mm  watertight={mesh.is_watertight}")


# ── Arduino Uno R4 WiFi ─────────────────────────────────────────────────────
# Datasheet ABX00087, section 13: board 68.58 x 53.34 mm, holes at
# (13.97, 2.54) (15.24, 50.80) (66.04, 7.62) (66.04, 35.56), tallest part 8.5 mm.
# Headers and connectors are typical Uno positions (approximate).
def arduino():
    W, H, T = 68.58, 53.34, 1.6
    pcb = box(0, 0, 0, W, H, T)
    holes = [cyl(x, y, -1, 3.2, T + 2, 32) for x, y in
             [(13.97, 2.54), (15.24, 50.80), (66.04, 7.62), (66.04, 35.56)]]
    pcb = diff(pcb, holes)
    parts = [pcb]
    parts.append(box(17.8, 49.5, T, 48.3, 2.54, 8.5 - T))      # top header row
    parts.append(box(26.7, 1.3, T, 39.4, 2.54, 8.5 - T))       # bottom header row
    parts.append(box(-1.5, 38.0, T, 10.0, 9.0, 3.3))           # USB-C (approx.)
    parts.append(box(-2.5, 4.0, T, 14.0, 9.0, 11.0))           # barrel jack (approx.)
    return union(parts)


# ── DFR0888 WS2812 ring, 12 LEDs ────────────────────────────────────────────
# Drawing DFR0888-12 dimension: outer 47 mm, inner 26 mm. LEDs are 5050 (5 x 5 x 1.6).
def ring():
    T = 1.6
    pcb = cyl(0, 0, 0, 47, T, 128)
    pcb = diff(pcb, [cyl(0, 0, -1, 26, T + 2, 128)])
    parts = [pcb]
    r = (47 + 26) / 4
    for i in range(12):
        a = np.radians(i * 30)
        led = box(-2.5, -2.5, T, 5, 5, 1.6)
        led.apply_transform(trimesh.transformations.rotation_matrix(a, (0, 0, 1)))
        led.apply_translation((r * np.cos(a), r * np.sin(a), 0))
        parts.append(led)
    return union(parts)


# ── DFPlayer Mini ───────────────────────────────────────────────────────────
# Manual gives no dimensions. Typical module: about 20.8 x 20.4 mm, microSD slot on top.
def dfplayer():
    pcb = box(0, 0, 0, 20.8, 20.4, 1.0)
    slot = box(3.0, 2.0, 1.0, 15.0, 14.5, 1.8)
    return union([pcb, slot])


# ── 16 mm RGB push button (PM161H-10E, "Little Button" note) ───────────────
# From the datasheet doc/datasheets/PM161-10E_RGB_12V_twin_datasheet.pdf (PM161-10E/J/42RGB/12V/S, the 12 V twin of our 5 V button, same housing):
# panel hole 16 +0.2 mm, panel max 6 mm, flange dia 18 x 1.5 on the front, button dia 11.8, body M16x1 thread,
# 20 mm behind the flange, hex nut 21.8 across the corners (behind the panel), pin terminals 2 x 0.5 mm.
def button():
    flange = cyl(0, 0, 0, 18.0, 1.5)
    cap = cyl(0, 0, 1.5, 11.8, 1.0)
    body = cyl(0, 0, -20.0, 16.0, 20.0)
    nut = cyl(0, 0, -7.5, 21.8, 3.0)
    return union([flange, cap, body, nut])


# ── 1-channel 5 V relay module (MAINS PART) ────────────────────────────────
# Typical SRD-05VDC module: 50 x 26 mm PCB, relay 19 x 15.5 x 15.3 mm, screw terminals on one side.
def relay_module():
    pcb = box(0, 0, 0, 50, 26, 1.6)
    relay = box(15, 5, 1.6, 19, 15.5, 15.3)
    terminals = box(36, 4, 1.6, 14, 18, 10)
    return union([pcb, relay, terminals])


# ── Button pipe (rises from the platter centre, button on top) ─────────────
# Origin = centre of the pipe, Z = 0 is where it meets the platter. The button
# (button_16mm_pm161h.stl) sits in the 16 mm hole of the cap; its panel plane is the
# top of the cap. Values are my guesses: change PIPE_H / PIPE_OD / PIPE_ID to taste.
PIPE_H, PIPE_OD, PIPE_ID, CAP_T = 100.0, 26.0, 20.0, 3.0

def button_pipe():
    tube = diff(cyl(0, 0, 0, PIPE_OD, PIPE_H), [cyl(0, 0, -1, PIPE_ID, PIPE_H + 2)])
    cap = diff(cyl(0, 0, PIPE_H, PIPE_OD, CAP_T), [cyl(0, 0, PIPE_H - 1, 16.0, CAP_T + 2)])
    return union([tube, cap])


# ── One-way cable clip (test print) ────────────────────────────────────────
# Two ribs rise from a base with a round channel between them. The slit to the
# channel is ANGLED: a cable can be pressed in along the slope, but it cannot be
# pulled straight out. Print in PETG; tune CLIP_SLIT (a bit smaller than the cable
# diameter = needs a push; bigger = falls out). 4G1.0 cable is about 8-9 mm thick.
CLIP_CABLE_D, CLIP_SLIT, CLIP_ANGLE = 9.0, 6.0, 40.0

def cable_clip(length=14.0):
    w, base_h, h = 22.0, 3.0, 14.0
    blk = box(-w / 2, 0, 0, w, length, base_h + h)
    channel = cyl(0, -1, 0, CLIP_CABLE_D, length + 2)
    channel.apply_transform(trimesh.transformations.rotation_matrix(-np.pi / 2, (1, 0, 0)))
    channel.apply_translation((0, 0, base_h + CLIP_CABLE_D / 2 + 0.5))
    slit = box(-CLIP_SLIT / 2, -1, 0, CLIP_SLIT, length + 2, h + 2)          # vertical slit up from the channel
    slit.apply_translation((0, 0, base_h + CLIP_CABLE_D / 2))
    slit.apply_transform(trimesh.transformations.rotation_matrix(
        np.radians(CLIP_ANGLE), (0, 1, 0), point=(0, 0, base_h + CLIP_CABLE_D / 2)))   # tilt: the angled entry
    return diff(blk, [channel, slit])


PARTS = {
    "cable_clip_test.stl": cable_clip,
    "button_pipe.stl": button_pipe,
    "arduino_uno_r4_wifi.stl": arduino,
    "led_ring_dfr0888_12led.stl": ring,
    "dfplayer_mini.stl": dfplayer,
    "button_16mm_pm161h.stl": button,
    "relay_module_1ch_5v.stl": relay_module,
}

if __name__ == "__main__":
    for name, fn in PARTS.items():
        save(fn(), name)
