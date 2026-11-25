#!/usr/bin/env python3
"""
make_wiring_interactive.py — builds doc/wiring/interactive.html: a clickable wiring diagram linked to the 3D viewer.

Run: python3 cad/make_wiring_interactive.py
Wire names, voltage class and diameter come from cad/make_interior.py (the same wires that are in the 3D model),
so diagram and model cannot drift apart. Click a wire or a part: its details show on the right and the 3D viewer
(embedded) lights up the same wire(s). Picking a wire in the viewer's "Highlight" list selects it here too.
To change the schematic layout edit NODES / ROUTES below.
"""
import os, json, sys
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
import make_interior as mi

OUT = os.path.join(here, "..", "doc", "wiring", "interactive.html")
NET = {"230": ("230 V mains", "#c00000"), "12": ("12 V", "#d98200"), "5": ("5 V", "#e11d48"), "data": ("Data / signal", "#1d4ed8"), "gnd": ("GND", "#111111"), "audio": ("Audio (speaker level)", "#16a34a"), "led_r": ("Button LED red", "#d92b2b"), "led_g": ("Button LED green", "#28a745"), "led_b": ("Button LED blue", "#2f6fff")}

# id: (x, y, w, h, label, sub)
NODES = {
    "INLET":  (40, 90, 120, 52, "C14 inlet + fuse", "230 V in, Swiss plug cable"),
    "RELAY":  (40, 220, 120, 60, "Relay module", "1 channel, own mains compartment"),
    "OUTLET": (150, 330, 130, 52, "Outlet B, Type 13", "disco ball"),
    "BRICK":  (175, 150, 125, 52, "12 V brick", "19 x 6 x 4 cm, AC in"),
    "BUCK":   (370, 210, 130, 62, "Buck converter", "12 V to 5 V, barrel jack on board"),
    "GNDB":   (370, 340, 130, 52, "GND star block", ""),
    "SHIFT":  (590, 90, 130, 52, "74AHCT125", "level shifter"),
    "ARD":    (590, 210, 130, 70, "Arduino Uno R4 WiFi", ""),
    "STRIP":  (790, 90, 120, 52, "Strip port", "GX connector, LED strip"),
    "BTN":    (790, 300, 130, 74, "RGB button", "16 mm, on the record"),
    "BIGBTN": (800, 530, 120, 60, "Big red button", "separate, 2.2 m cable"),
    "RING":   (790, 440, 130, 60, "RGB LED ring", "16 x WS2812, on the record"),
    "BUZZ":   (40, 450, 130, 56, "Piezo buzzer", "3 x 3 x 1.5 cm, pin D9"),
    "DFP":    (370, 450, 130, 62, "DFPlayer Mini", "MP3 from SD card"),
    "AMP":    (590, 450, 130, 62, "PAM8403 amp", "stereo, 5 V"),
    "SPKL":   (530, 540, 110, 42, "Speaker left", ""),
    "SPKR":   (670, 540, 110, 42, "Speaker right", ""),
}
# wire name (as in make_interior.wires) -> (from, to, note)
ROUTES = {
    "mains_live_inlet_to_relay_COM": ("INLET", "RELAY", "Live from the inlet (after the fuse) to the relay COM contact. Mains compartment only."),
    "mains_live_relay_NO_to_outlet": ("RELAY", "OUTLET", "Switched live from the relay NO contact to the outlet."),
    "mains_neutral_straight_through": ("INLET", "OUTLET", "Neutral goes straight through, never switched."),
    "mains_live_inlet_to_brick": ("INLET", "BRICK", "Live from the inlet (after the fuse) into the 12 V brick. The mains goes through the brick first."),
    "mains_neutral_inlet_to_brick": ("INLET", "BRICK", "Neutral from the inlet into the brick."),
    "12V_brick_to_buck_barrel_jack": ("BRICK", "BUCK", "12 V out of the brick (mains compartment) through the wall into the barrel jack on the buck converter."),
    "12V_buck_in_to_strip_port_via_fuse": ("BUCK", "STRIP", "12 V taken from the buck input terminals, through a fuse, to the strip port."),
    "5V_buck_to_arduino": ("BUCK", "ARD", "5 V to the Arduino."),
    "5V_buck_to_relay_coil": ("BUCK", "RELAY", "5 V relay coil supply. Low-voltage wire crosses into the mains compartment through its own hole."),
    "5V_buck_to_amp_pair": ("BUCK", "AMP", "The pair to the amp: +5 V (this wire) and GND (next wire)."),
    "GND_buck_to_amp_pair": ("BUCK", "AMP", "The pair to the amp: GND, runs next to the +5 V wire."),
    "5V_amp_to_dfplayer": ("AMP", "DFP", "+5 V on to the DFPlayer Mini (its VCC)."),
    "GND_amp_to_dfplayer": ("AMP", "DFP", "GND on to the DFPlayer Mini."),
    "audio_dfplayer_to_amp_L_R": ("DFP", "AMP", "DFPlayer audio out (DAC_L, DAC_R) to the amp inputs."),
    "serial_arduino_to_dfplayer_RX_TX": ("ARD", "DFP", "Serial1 (D0/D1) to the DFPlayer; 1 kOhm in series on its RX."),
    "data_D11_via_74AHCT125": ("ARD", "SHIFT", "Pin D11 (strip data) to the level shifter input."),
    "data_74AHCT125_to_strip_port": ("SHIFT", "STRIP", "Level-shifted 5 V data to the strip port."),
    "button_switch_D2": ("ARD", "BTN", "Button switch, contact 1: pin D2 (internal pull-up, pressed = LOW). Runs up through the hollow pipe."),
    "button_switch_GND": ("GNDB", "BTN", "Button switch, contact 2: GND."),
    "button_LED_R_D3": ("ARD", "BTN", "Button LED, red: pin D3. Resistor is built into the button."),
    "button_LED_G_D5": ("ARD", "BTN", "Button LED, green: pin D5."),
    "button_LED_B_D6": ("ARD", "BTN", "Button LED, blue: pin D6."),
    "button_LED_common_GND": ("GNDB", "BTN", "Common ground (cathode) of the three button LEDs."),
    "ring_5V": ("BUCK", "RING", "5 V for the LED ring, through the pipe."),
    "ring_GND": ("GNDB", "RING", "GND for the LED ring."),
    "ring_data_D4_330ohm": ("ARD", "RING", "Ring data from pin D4 through a 330 Ohm resistor."),
    "bigbutton_signal_to_port": ("ARD", "BIGBTN", "Big red button signal to a free pin (not chosen yet (?), for example D8), through the GX12 port and the 2.2 m cable. Pressed = LOW with internal pull-up. Planned use: straight to disco mode (needs firmware)."),
    "bigbutton_GND_to_port": ("GNDB", "BIGBTN", "Big red button GND, through the GX12 port and the cable."),
    "buzzer_D9": ("ARD", "BUZZ", "Piezo buzzer signal from pin D9 (tone). The buzzer is 3 x 3 x 1.5 cm, sits on a 4 mm platform on the frame; its side lug has a screw hole."),
    "buzzer_GND": ("GNDB", "BUZZ", "Piezo buzzer GND."),
    "GND_buck_to_gnd_block": ("BUCK", "GNDB", "Common ground: buck GND to the star block."),
    "GND_block_to_arduino": ("GNDB", "ARD", "Star block to Arduino GND."),
    "GND_block_to_strip_port": ("GNDB", "STRIP", "Star block to strip port GND."),
    "speaker_left_pair": ("AMP", "SPKL", "Left speaker, pair of wires (+ and -) from the amp output."),
    "speaker_right_pair": ("AMP", "SPKR", "Right speaker, pair of wires (+ and -) from the amp output."),
}

missing = [k for k in mi.wires if k not in ROUTES] + [k for k in ROUTES if k not in mi.wires]
if missing:
    raise SystemExit("wires out of sync with make_interior.py: " + ", ".join(missing))

wires = []
for k, (cls, d, pts) in mi.wires.items():
    a, b, note = ROUTES[k]
    wires.append(dict(id="wire_" + k, label=k.replace("_", " "), net=cls, dia=d, a=a, b=b, note=note))
nodes = {k: dict(x=v[0], y=v[1], w=v[2], h=v[3], label=v[4], sub=v[5]) for k, v in NODES.items()}

HTML = r"""<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Discobox interactive wiring</title>
<style>
:root { color-scheme: only light; --main:#5E282C; --text:#190607; --accent:#EEE3C9; --bg:#f6f7fb; }
* { box-sizing:border-box; } body { margin:0; font-family:Roboto, system-ui, Arial, sans-serif; background:var(--bg); color:var(--main); }
header { padding:.7rem 1.2rem; border-bottom:2px solid var(--main); display:flex; flex-wrap:wrap; gap:.8rem; align-items:center; }
h1 { font-size:1.1rem; margin:0; color:var(--text); } .filters button { border:2px solid var(--main); background:#fff; border-radius:999px; padding:.2rem .8rem; cursor:pointer; font-size:.85rem; color:var(--main); }
.filters button[aria-pressed=true] { background:var(--accent); } .filters .sw { display:inline-block; width:.7rem; height:.7rem; border-radius:50%; margin-right:.35rem; vertical-align:-1px; }
main { display:grid; grid-template-columns:minmax(0,1.5fr) minmax(280px,1fr); gap:1rem; padding:1rem; }
@media (max-width:900px) { main { grid-template-columns:1fr; } }
@media (max-width:600px) { header { padding:.6rem .8rem; } main { padding:.5rem; gap:.6rem; } iframe { height:320px; } .filters button { padding:.4rem .9rem; min-height:40px; } .card { border-radius:18px; } }
@media (pointer:coarse) { .wire path.hit { stroke-width:26px; } .node, .wire { -webkit-tap-highlight-color:transparent; } }
.card { background:#fff; border:2px solid var(--accent); border-radius:26px; padding:.6rem; }
svg { width:100%; height:auto; display:block; } .node rect { fill:#fff; stroke:var(--main); stroke-width:2; rx:12; } .node text { fill:var(--text); font-size:13px; pointer-events:none; } .node .sub { font-size:10.5px; fill:#7e6e56; }
.node { cursor:pointer; } .node:hover rect, .node:focus rect, .node.sel rect { fill:var(--accent); } .node:focus { outline:none; } .node:focus rect { stroke-width:4; }
.wire path.line { fill:none; stroke-width:3; stroke-linecap:round; } .wire path.hit { fill:none; stroke:transparent; stroke-width:16; cursor:pointer; } .wire:focus { outline:none; }
.wire:focus path.line { stroke-width:6; } .wire.sel path.line { stroke-width:7; } .wire.dim { opacity:.15; } .node.dim { opacity:.35; }
.zone { fill:#fff5f5; stroke:#c00000; stroke-width:2; stroke-dasharray:8 5; rx:10; } .zonelabel { fill:#c00000; font-weight:700; font-size:12px; }
#info h2 { font-size:1rem; margin:.2rem 0 .3rem; color:var(--text); } #info p { margin:.2rem 0; font-size:.9rem; } #info .tag { display:inline-block; padding:.05rem .6rem; border-radius:999px; color:#fff; font-size:.8rem; }
#info button { margin:.4rem .4rem 0 0; border:2px solid var(--main); border-radius:999px; background:var(--accent); padding:.25rem .9rem; cursor:pointer; color:var(--main); }
iframe { width:100%; height:430px; border:2px solid var(--accent); border-radius:18px; margin-top:.6rem; background:#dcebf8; }
.warn { font-size:.8rem; color:#7e6e56; margin-top:.5rem; }
.sr { position:absolute; left:-9999px; }
/* Novaroma + 80s */
body { background:linear-gradient(#f6f7fb 0 55%, #efe3ff 100%) fixed; }
header { box-shadow:0 4px 0 #ff2fa3, 0 8px 0 #2ff3ff, 0 12px 0 #ffe12f; margin-bottom:12px; }
h1 { font-style:italic; text-transform:uppercase; letter-spacing:.08em; text-shadow:2px 2px 0 #2ff3ff99; }
.node.sel rect, .node:hover rect { fill:#ffd2ee; stroke:#ff2fa3; } button:focus-visible, .wire:focus-visible, .node:focus-visible { outline:3px solid #2ff3ff; outline-offset:2px; }
.filters button[aria-pressed=true] { background:#ff2fa3; color:#fff; border-color:#ff2fa3; }
.card { box-shadow:0 0 0 2px #2ff3ff55; }

</style></head><body>
<header><h1>Discobox wiring, interactive</h1><div class="filters" id="filters" role="group" aria-label="Show nets"></div></header>
<main>
<div class="card"><svg id="svg" viewBox="0 0 940 600" role="group" aria-label="Wiring schematic. Tab to move between wires and parts, Enter to select."></svg></div>
<div>
<div class="card" id="info" aria-live="polite"><h2>Pick a wire or a part</h2><p>Click a wire to see where it goes and to light it up in the 3D model. Click a part to light up every wire on it.</p></div>
<iframe id="v" title="3D viewer" src="__VIEWER__#v5"></iframe>
<p class="warn">The schematic is a plan, not an approved electrical drawing. Check every value against your parts; mains work needs a qualified person.</p>
</div></main>
<script>
const NET = __NET__, NODES = __NODES__, WIRES = __WIRES__;
const svg = document.getElementById("svg"), info = document.getElementById("info"), frame = document.getElementById("v");
const NS = "http://www.w3.org/2000/svg"; const el = (n, a, p) => { const e = document.createElementNS(NS, n); for (const k in a) e.setAttribute(k, a[k]); if (p) p.append(e); return e; };
el("rect", { class: "zone", x: 20, y: 60, width: 290, height: 350 }, svg); const zl = el("text", { class: "zonelabel", x: 34, y: 80 }, svg); zl.textContent = "MAINS COMPARTMENT (230 V)";
const C = n => [NODES[n].x + NODES[n].w / 2, NODES[n].y + NODES[n].h / 2];
const gW = el("g", {}, svg), gN = el("g", {}, svg);
const wireEls = {}, nodeEls = {}; let sel = null, off = {};
WIRES.forEach(w => {
  const [x1, y1] = C(w.a), [x2, y2] = C(w.b), key = [w.a, w.b].sort().join(); off[key] = (off[key] || 0) + 1;
  const o = (off[key] - 1) * 14, mx = (x1 + x2) / 2;
  const horiz = Math.abs(x2 - x1) > Math.abs(y2 - y1);
  const d = horiz ? `M${x1} ${y1 + o} C${mx} ${y1 + o} ${mx} ${y2 + o} ${x2} ${y2 + o}` : `M${x1 + o} ${y1} C${x1 + o} ${(y1 + y2) / 2} ${x2 + o} ${(y1 + y2) / 2} ${x2 + o} ${y2}`;
  const g = el("g", { class: "wire", tabindex: 0, role: "button", "aria-label": w.label + ", " + NET[w.net][0] }, gW);
  el("path", { class: "line", d, stroke: NET[w.net][1], "stroke-width": 2 + w.dia * 0.6 }, g); el("path", { class: "hit", d }, g);
  g.append(Object.assign(document.createElementNS(NS, "title"), { textContent: w.label }));
  g.onclick = () => pickWire(w.id); g.onkeydown = e => { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); pickWire(w.id); } };
  wireEls[w.id] = g;
});
Object.entries(NODES).forEach(([id, n]) => {
  const g = el("g", { class: "node", tabindex: 0, role: "button", "aria-label": n.label }, gN);
  el("rect", { x: n.x, y: n.y, width: n.w, height: n.h }, g);
  const t = el("text", { x: n.x + n.w / 2, y: n.y + (n.sub ? n.h / 2 - 3 : n.h / 2 + 4), "text-anchor": "middle" }, g); t.textContent = n.label;
  if (n.sub) { const s = el("text", { class: "sub", x: n.x + n.w / 2, y: n.y + n.h / 2 + 12, "text-anchor": "middle" }, g); s.textContent = n.sub.length > 28 ? n.sub.slice(0, 27) + "…" : n.sub; }
  g.onclick = () => pickNode(id); g.onkeydown = e => { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); pickNode(id); } };
  nodeEls[id] = g;
});
const fb = document.getElementById("filters"), on = {};
Object.entries(NET).forEach(([k, v]) => { on[k] = true; const b = document.createElement("button"); b.setAttribute("aria-pressed", "true"); b.innerHTML = `<span class="sw" style="background:${v[1]}"></span>${v[0]}`;
  b.onclick = () => { on[k] = !on[k]; b.setAttribute("aria-pressed", on[k]); WIRES.forEach(w => { if (w.net === k) wireEls[w.id].style.display = on[k] ? "" : "none"; }); }; fb.append(b); });
function show(ids, title, html) {
  sel = ids; WIRES.forEach(w => { const s = ids.includes(w.id); wireEls[w.id].classList.toggle("sel", s); wireEls[w.id].classList.toggle("dim", ids.length > 0 && !s); });
  const used = new Set(WIRES.filter(w => ids.includes(w.id)).flatMap(w => [w.a, w.b]));
  Object.keys(nodeEls).forEach(n => { nodeEls[n].classList.toggle("sel", used.has(n) && ids.length === 1); nodeEls[n].classList.toggle("dim", ids.length > 0 && !used.has(n)); });
  info.innerHTML = html; const b = info.querySelector("#clr"); if (b) b.onclick = () => clear();
  frame.contentWindow.postMessage({ type: "discobox-hl", names: ids }, "*");
}
function pickWire(id, fromViewer) { const w = WIRES.find(x => x.id === id); if (!w) return;
  sel = [id]; show([id], w.label, `<h2>${w.label}</h2><p><span class="tag" style="background:${NET[w.net][1]}">${NET[w.net][0]}</span> &middot; ${w.dia} mm in the model</p><p><b>${NODES[w.a].label}</b> &rarr; <b>${NODES[w.b].label}</b></p><p>${w.note}</p><button id="clr" type="button">Clear</button>`); }
function pickNode(id) { const ws = WIRES.filter(w => w.a === id || w.b === id); const n = NODES[id];
  show(ws.map(w => w.id), n.label, `<h2>${n.label}</h2><p>${n.sub || ""}</p><p>${ws.length} wires:</p><ul style="margin:.2rem 0 0 1.1rem;padding:0;font-size:.85rem">${ws.map(w => `<li>${w.label} <span style="color:${NET[w.net][1]}">&#9679;</span></li>`).join("")}</ul><button id="clr" type="button">Clear</button>`); }
function clear() { show([], "", `<h2>Pick a wire or a part</h2><p>Click a wire to see where it goes and to light it up in the 3D model. Click a part to light up every wire on it.</p>`); }
window.addEventListener("message", e => { const d = e.data; if (d && d.type === "discobox-wire") { if (d.names.length === 1) { const w = WIRES.find(x => x.id === d.names[0]); if (w) { sel = [w.id]; WIRES.forEach(x => { wireEls[x.id].classList.toggle("sel", x.id === w.id); wireEls[x.id].classList.toggle("dim", x.id !== w.id); }); info.innerHTML = `<h2>${w.label}</h2><p><span class="tag" style="background:${NET[w.net][1]}">${NET[w.net][0]}</span></p><p><b>${NODES[w.a].label}</b> &rarr; <b>${NODES[w.b].label}</b></p><p>${w.note}</p>`; } } } });
const hp = new URLSearchParams(location.hash.slice(1)); if (hp.get("wire")) frame.addEventListener("load", () => pickWire("wire_" + hp.get("wire")), { once: true });
</script></body></html>
"""
html = (HTML.replace("__NET__", json.dumps({k: list(v) for k, v in NET.items()})).replace("__NODES__", json.dumps(nodes))
        .replace("__WIRES__", json.dumps(wires)).replace("__VIEWER__", "../viewer/discobox_viewer.html"))
open(OUT, "w", encoding="utf-8").write(html)
print("wrote", os.path.relpath(OUT), len(wires), "wires,", len(nodes), "parts")
