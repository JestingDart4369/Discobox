#!/usr/bin/env python3
"""
make_design_guide.py — writes doc/DESIGN.md, the Discobox theme and design guide (Novaroma base + 80s accents).
Run: python3 site/make_design_guide.py     (contrast ratios are computed here, so the table cannot drift from the colours)
The site build (site/build_site.py) turns doc/DESIGN.md into the "Design guide" page.
"""
import os

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "doc", "DESIGN.md")


def lum(h):
    h = h.lstrip("#"); c = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    c = [x / 12.92 if x <= 0.03928 else ((x + 0.055) / 1.055) ** 2.4 for x in c]
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


def ratio(a, b):
    la, lb = sorted((lum(a), lum(b)), reverse=True); return (la + 0.05) / (lb + 0.05)


def sw(h): return f'<span class="sw" style="background:{h}"></span>`{h}`'


NOVAROMA = [("Burgundy (main)", "#5E282C", "text, outlines, active pill"), ("Ink (text)", "#190607", "headings, body on light"),
            ("Cream (accent)", "#EEE3C9", "card outlines, buttons, quiet fills"), ("Paper (background)", "#f6f7fb", "page background"),
            ("Muted brown", "#7e6e56", "secondary text, hints")]
NEON = [("Hot pink", "#ff2fa3", "stripes, focus, pink button, slider thumbs"), ("Cyan", "#2ff3ff", "title shadow, grid lines, outlines"),
        ("Sunshine yellow", "#ffe12f", "third stripe, pointers, mains lid"), ("Violet", "#7a2fff", "counterweight, laser, deep accents"),
        ("Night purple", "#1d0646", "footer, secret disco, vinyl"), ("Deep night", "#12002b", "footer background, marquee"),
        ("Sunset orange", "#ff8c28", "record label, sun"), ("Mint", "#2fff8a", "laser, audio wires")]
PRODUCT = [("Shell (PLA/PETG)", "#e81e8c", "hot magenta"), ("Frame (PETG)", "#1ecde1", "cyan, hidden inside"), ("Mains lid", "#ffe12f", "yellow = careful, mains"),
           ("Platter", "#341664", "purple"), ("Slip mat / control plate", "#160830", "near black purple"), ("Pipe window cover", "#ff2fa3", "hot pink, at the back of the pipe"),
           ("Record body", "#140a22", "black purple"), ("Record grooves", "#502c7d", "violet"), ("Record label", "#ff8c28", "sunset orange"),
           ("Diffuser", "#eee0ff", "milky lilac"), ("RGB button cap", "#ff2fa3", "hot pink"), ("Big red button", "#e11923", "buzzer red"),
           ("Tonearm", "#c8c8cd", "chrome, counterweight violet")]

PAIRS = [("#190607", "#f6f7fb", "body text on paper"), ("#5E282C", "#f6f7fb", "burgundy text on paper"), ("#EEE3C9", "#5E282C", "active menu pill (cream on burgundy)"),
         ("#5E282C", "#EEE3C9", "burgundy on cream (buttons)"), ("#7e6e56", "#f6f7fb", "muted text on paper"), ("#ffffff", "#c2186f", "white on deep pink"),
         ("#ffffff", "#ff2fa3", "white on hot pink (do not use for small text)"), ("#2ff3ff", "#ffffff", "cyan on white (decoration only)"),
         ("#2ff3ff", "#12002b", "cyan on deep night (footer links)")]

out = ["# Discobox theme and design guide", "",
       "One look for everything: the website, the 3D viewer, the wiring page, the secret disco screen and the object itself. "
       "The base is the **Novaroma** design system (burgundy, cream, big round corners, thin outlines, calm layouts) and on top of it sits an **80s layer** "
       "(neon pink, cyan and yellow, vinyl, grids, sunsets). Rule of thumb: Novaroma carries the text and layout, the 80s layer is decoration and accent.",
       "", "## 1. Principles", "",
       "- **Readable first.** Text is always dark on light (or cream on burgundy). Neon colours are for lines, glows and shapes, not for body text.",
       "- **Round and calm.** 26 px corner radius on cards, pill buttons, 2 px outlines, no drop shadows (neon glow only on the secret screen).",
       "- **Slow motion.** Effects drift slowly; nothing flashes more than 3 times per second. Everything stops with the browser setting 'reduce motion'.",
       "- **Light only.** The pages declare a light colour scheme so browser dark-mode filters do not recolour them.",
       "- **Accessible.** WCAG 2.2 AA: 3 px cyan focus ring, 44 px touch targets on phones, real buttons and links, `aria-label` on icon buttons.",
       "- **One object, one palette.** The printed box uses the same colours as the site (section 6).", "", "## 2. Colours", "", "### Novaroma base", "",
       "| Name | Colour | Use |", "|---|---|---|"]
out += [f"| {n} | {sw(h)} | {u} |" for n, h, u in NOVAROMA]
out += ["", "### 80s layer", "", "| Name | Colour | Use |", "|---|---|---|"] + [f"| {n} | {sw(h)} | {u} |" for n, h, u in NEON]
out += ["", "### Contrast check (computed)", "", "| Pair | Ratio | Result | Where |", "|---|---|---|---|"]
for a, b, w in PAIRS:
    r = ratio(a, b)
    res = "AA text" if r >= 4.5 else ("large text and UI only" if r >= 3 else "decoration only")
    out.append(f"| {sw(a)} on {sw(b)} | {r:.1f} : 1 | {res} | {w} |")
out += ["", "## 3. Typography", "",
        "- **Body:** Roboto (system fallbacks: system-ui, Segoe UI, Arial), 16 px base, line height 1.6.",
        "- **Headings (h1, h2):** italic, slightly spaced, with a 2 px cyan offset shadow (`text-shadow: 2px 2px 0 #2ff3ff88`). The site title is italic upper case with wide letter spacing.",
        "- **Section labels** (small headings in cards and the viewer menu): upper case, italic, 0.08 em spacing, 12 to 13 px.",
        "- **Journal:** handwriting font on ruled paper (drop a font file into `site/fonts/`, see its README); everything around it stays Roboto.",
        "- **Credits and secret screen:** extra bold italic upper case with neon outline; the ribbon uses 0.3 em spacing.", "",
        "## 4. Shapes, spacing and motion", "",
        "| Token | Value |", "|---|---|", "| Corner radius | 26 px cards, 999 px (pill) buttons and tags, 12 px images |", "| Outline | 2 px, burgundy (buttons) or cream (cards) |",
        "| Spacing | 1 rem base; cards padded 1.2 to 1.6 rem |", "| Header | sticky, 68 px, three-stripe band (pink, cyan, yellow, 9 px) under it |",
        "| Focus ring | 3 px cyan, 2 px offset |", "| Motion | 0.2 s hover transitions; neon effects 20 to 60 s per loop; none with reduce-motion |", "",
        "```css", ":root {", "  --bg:#f6f7fb; --main:#5E282C; --text:#190607; --accent:#EEE3C9; --muted:#7e6e56;",
        "  --pink:#ff2fa3; --cyan:#2ff3ff; --yellow:#ffe12f; --violet:#7a2fff; --night:#12002b;", "  --line:2px; --corner-radius:26px;", "}", "```", "",
        "## 5. Components", "",
        "- **Header:** vinyl icon, title 'DISCOBOX' with 'by JestingDart4369', pill menu (active page = cream text on burgundy), Novaroma button right, stripe band below. On phones: hamburger with a 44 px target.",
        "- **Cards:** white, 2 px cream outline, 26 px radius, small italic label on top. Cards in a grid, never nested more than once.",
        "- **Buttons:** pill, 2 px burgundy outline; primary = cream fill; hover = burgundy fill with cream text. Toggles in the viewer use hot pink when on.",
        "- **Footer:** deep night background, cyan links, pink top border; the copyright line is the secret trigger (5 clicks within 4 seconds; underline hints at 3 clicks).",
        "- **3D viewer:** same tokens: paper menu with a stripe edge, pill buttons, pink checkboxes; the stage is a lilac gradient with a faint grid, and turns into a violet disco gradient when the disco is on.",
        "- **Wiring page:** nets keep fixed colours (230 V red, 12 V orange, 5 V pink-red, data blue, GND black, audio green, button LEDs red/green/blue).",
        "- **Secret disco screen:** night gradient, stars, sunset sun, neon grid floor, slow lasers and smoke, mirror ball with light spots, equalizer, rising notes, ribbon, quote corner, credits card with pink and cyan outline.",
        "", "## 6. The object (3D printed box)", "",
        "| Part | Colour | Note |", "|---|---|---|"] + [f"| {n} | {sw(h)} | {u} |" for n, h, u in PRODUCT]
out += ["", "Shell details (all cut into the outside, at most 1.2 mm of the 3 mm wall): rounded vertical corners R6, 2 mm top chamfer, stepped-in base, three pinstripe grooves (paint them cyan or yellow, or add a colour change), recessed panels round the side vents, a nameplate recess on the front and hex-perforated speaker grilles inside a round bezel.", "",
        "Filament: PETG for the shell if the box lives in a kitchen or washing room (ASA near a dryer); keep the mains lid yellow so nobody opens it by mistake.", "",
        "## 7. Voice", "",
        "Short, friendly, concrete. Say what it does and what to check; mark guesses with `(?)`. Use 'record player' and 'disco box' for the object, 'brick' for the 12 V supply, 'buck' for the converter. No hype words; one joke per page at most.", "",
        "## 8. Do and don't", "",
        "- **Do** use neon for lines, glows, icons and shapes; keep text dark on light.",
        "- **Do** keep every animation slow and make it optional (reduce motion).",
        "- **Do** give every icon button a label and keep touch targets 44 px.",
        "- **Don't** put white or cyan text on hot pink for anything small (see the contrast table).",
        "- **Don't** add drop shadows or gradients to cards; the Novaroma look is flat and outlined.",
        "- **Don't** flash, strobe or blink; the disco is a slow colour cycle.",
        "- **Don't** use a different corner radius or outline width in a new component.", "",
        "## 9. Where it lives in the code", "",
        "- Site CSS: `site/build_site.py` (`site/discobox-theme.css`, the single stylesheet; the 80s block is marked `/* 80s theme */`).",
        "- Viewer CSS: `cad/viewer_template.html`. Wiring page: `cad/make_wiring_interactive.py`. Old wiring page: `doc/wiring/index.html`.",
        "- Object colours: `cad/make_full_assembly.py` (`C` dict) and `cad/make_recordplayer.py`. Renders: `cad/render_scene.py` (synthwave background).",
        "- This guide: `python3 site/make_design_guide.py` rewrites `doc/DESIGN.md`.", ""]
open(OUT, "w", encoding="utf-8").write("\n".join(out))
print("wrote", os.path.relpath(OUT, ROOT), len(out), "lines")
