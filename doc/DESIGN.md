# Discobox theme and design guide

One look for everything: the website, the 3D viewer, the wiring page, the secret disco screen and the object itself. The base is the **Novaroma** design system (burgundy, cream, big round corners, thin outlines, calm layouts) and on top of it sits an **80s layer** (neon pink, cyan and yellow, vinyl, grids, sunsets). Rule of thumb: Novaroma carries the text and layout, the 80s layer is decoration and accent.

## 1. Principles

- **Readable first.** Text is always dark on light (or cream on burgundy). Neon colours are for lines, glows and shapes, not for body text.
- **Round and calm.** 26 px corner radius on cards, pill buttons, 2 px outlines, no drop shadows (neon glow only on the secret screen).
- **Slow motion.** Effects drift slowly; nothing flashes more than 3 times per second. Everything stops with the browser setting 'reduce motion'.
- **Light only.** The pages declare a light colour scheme so browser dark-mode filters do not recolour them.
- **Accessible.** WCAG 2.2 AA: 3 px cyan focus ring, 44 px touch targets on phones, real buttons and links, `aria-label` on icon buttons.
- **One object, one palette.** The printed box uses the same colours as the site (section 6).

## 2. Colours

### Novaroma base

| Name | Colour | Use |
|---|---|---|
| Burgundy (main) | <span class="sw" style="background:#5E282C"></span>`#5E282C` | text, outlines, active pill |
| Ink (text) | <span class="sw" style="background:#190607"></span>`#190607` | headings, body on light |
| Cream (accent) | <span class="sw" style="background:#EEE3C9"></span>`#EEE3C9` | card outlines, buttons, quiet fills |
| Paper (background) | <span class="sw" style="background:#f6f7fb"></span>`#f6f7fb` | page background |
| Muted brown | <span class="sw" style="background:#7e6e56"></span>`#7e6e56` | secondary text, hints |

### 80s layer

| Name | Colour | Use |
|---|---|---|
| Hot pink | <span class="sw" style="background:#ff2fa3"></span>`#ff2fa3` | stripes, focus, pink button, slider thumbs |
| Cyan | <span class="sw" style="background:#2ff3ff"></span>`#2ff3ff` | title shadow, grid lines, outlines |
| Sunshine yellow | <span class="sw" style="background:#ffe12f"></span>`#ffe12f` | third stripe, pointers, mains lid |
| Violet | <span class="sw" style="background:#7a2fff"></span>`#7a2fff` | counterweight, laser, deep accents |
| Night purple | <span class="sw" style="background:#1d0646"></span>`#1d0646` | footer, secret disco, vinyl |
| Deep night | <span class="sw" style="background:#12002b"></span>`#12002b` | footer background, marquee |
| Sunset orange | <span class="sw" style="background:#ff8c28"></span>`#ff8c28` | record label, sun |
| Mint | <span class="sw" style="background:#2fff8a"></span>`#2fff8a` | laser, audio wires |

### Contrast check (computed)

| Pair | Ratio | Result | Where |
|---|---|---|---|
| <span class="sw" style="background:#190607"></span>`#190607` on <span class="sw" style="background:#f6f7fb"></span>`#f6f7fb` | 18.3 : 1 | AA text | body text on paper |
| <span class="sw" style="background:#5E282C"></span>`#5E282C` on <span class="sw" style="background:#f6f7fb"></span>`#f6f7fb` | 10.8 : 1 | AA text | burgundy text on paper |
| <span class="sw" style="background:#EEE3C9"></span>`#EEE3C9` on <span class="sw" style="background:#5E282C"></span>`#5E282C` | 9.1 : 1 | AA text | active menu pill (cream on burgundy) |
| <span class="sw" style="background:#5E282C"></span>`#5E282C` on <span class="sw" style="background:#EEE3C9"></span>`#EEE3C9` | 9.1 : 1 | AA text | burgundy on cream (buttons) |
| <span class="sw" style="background:#7e6e56"></span>`#7e6e56` on <span class="sw" style="background:#f6f7fb"></span>`#f6f7fb` | 4.6 : 1 | AA text | muted text on paper |
| <span class="sw" style="background:#ffffff"></span>`#ffffff` on <span class="sw" style="background:#c2186f"></span>`#c2186f` | 5.7 : 1 | AA text | white on deep pink |
| <span class="sw" style="background:#ffffff"></span>`#ffffff` on <span class="sw" style="background:#ff2fa3"></span>`#ff2fa3` | 3.4 : 1 | large text and UI only | white on hot pink (do not use for small text) |
| <span class="sw" style="background:#2ff3ff"></span>`#2ff3ff` on <span class="sw" style="background:#ffffff"></span>`#ffffff` | 1.4 : 1 | decoration only | cyan on white (decoration only) |
| <span class="sw" style="background:#2ff3ff"></span>`#2ff3ff` on <span class="sw" style="background:#12002b"></span>`#12002b` | 14.5 : 1 | AA text | cyan on deep night (footer links) |

## 3. Typography

- **Body:** Roboto (system fallbacks: system-ui, Segoe UI, Arial), 16 px base, line height 1.6.
- **Headings (h1, h2):** italic, slightly spaced, with a 2 px cyan offset shadow (`text-shadow: 2px 2px 0 #2ff3ff88`). The site title is italic upper case with wide letter spacing.
- **Section labels** (small headings in cards and the viewer menu): upper case, italic, 0.08 em spacing, 12 to 13 px.
- **Journal:** handwriting font on ruled paper (drop a font file into `site/fonts/`, see its README); everything around it stays Roboto.
- **Credits and secret screen:** extra bold italic upper case with neon outline; the ribbon uses 0.3 em spacing.

## 4. Shapes, spacing and motion

| Token | Value |
|---|---|
| Corner radius | 26 px cards, 999 px (pill) buttons and tags, 12 px images |
| Outline | 2 px, burgundy (buttons) or cream (cards) |
| Spacing | 1 rem base; cards padded 1.2 to 1.6 rem |
| Header | sticky, 68 px, three-stripe band (pink, cyan, yellow, 9 px) under it |
| Focus ring | 3 px cyan, 2 px offset |
| Motion | 0.2 s hover transitions; neon effects 20 to 60 s per loop; none with reduce-motion |

```css
:root {
  --bg:#f6f7fb; --main:#5E282C; --text:#190607; --accent:#EEE3C9; --muted:#7e6e56;
  --pink:#ff2fa3; --cyan:#2ff3ff; --yellow:#ffe12f; --violet:#7a2fff; --night:#12002b;
  --line:2px; --corner-radius:26px;
}
```

## 5. Components

- **Header:** vinyl icon, title 'DISCOBOX' with 'by JestingDart4369', pill menu (active page = cream text on burgundy), Novaroma button right, stripe band below. On phones: hamburger with a 44 px target.
- **Cards:** white, 2 px cream outline, 26 px radius, small italic label on top. Cards in a grid, never nested more than once.
- **Buttons:** pill, 2 px burgundy outline; primary = cream fill; hover = burgundy fill with cream text. Toggles in the viewer use hot pink when on.
- **Footer:** deep night background, cyan links, pink top border; the copyright line is the secret trigger (5 clicks within 4 seconds; underline hints at 3 clicks).
- **3D viewer:** same tokens: paper menu with a stripe edge, pill buttons, pink checkboxes; the stage is a lilac gradient with a faint grid, and turns into a violet disco gradient when the disco is on.
- **Wiring page:** nets keep fixed colours (230 V red, 12 V orange, 5 V pink-red, data blue, GND black, audio green, button LEDs red/green/blue).
- **Secret disco screen:** night gradient, stars, sunset sun, neon grid floor, slow lasers and smoke, mirror ball with light spots, equalizer, rising notes, ribbon, quote corner, credits card with pink and cyan outline.

## 6. The object (3D printed box)

| Part | Colour | Note |
|---|---|---|
| Shell (PLA/PETG) | <span class="sw" style="background:#e81e8c"></span>`#e81e8c` | hot magenta |
| Frame (PETG) | <span class="sw" style="background:#1ecde1"></span>`#1ecde1` | cyan, hidden inside |
| Mains lid | <span class="sw" style="background:#ffe12f"></span>`#ffe12f` | yellow = careful, mains |
| Platter | <span class="sw" style="background:#341664"></span>`#341664` | purple |
| Slip mat | <span class="sw" style="background:#160830"></span>`#160830` | near black purple |
| Pipe | <span class="sw" style="background:#5a5a5f"></span>`#5a5a5f` | dark grey |
| Record body | <span class="sw" style="background:#140a22"></span>`#140a22` | black purple |
| Record grooves | <span class="sw" style="background:#502c7d"></span>`#502c7d` | violet |
| Record label | <span class="sw" style="background:#ff8c28"></span>`#ff8c28` | sunset orange |
| Diffuser | <span class="sw" style="background:#eee0ff"></span>`#eee0ff` | milky lilac |
| RGB button cap | <span class="sw" style="background:#ff2fa3"></span>`#ff2fa3` | hot pink |
| Big red button | <span class="sw" style="background:#e11923"></span>`#e11923` | buzzer red |
| Tonearm | <span class="sw" style="background:#c8c8cd"></span>`#c8c8cd` | chrome, counterweight violet |

Shell details (all cut into the outside, at most 1.2 mm of the 3 mm wall): rounded vertical corners R6, 2 mm top chamfer, stepped-in base, three pinstripe grooves (paint them cyan or yellow, or add a colour change), recessed panels round the side vents, a nameplate recess on the front and hex-perforated speaker grilles inside a round bezel.

Filament: PETG for the shell if the box lives in a kitchen or washing room (ASA near a dryer); keep the mains lid yellow so nobody opens it by mistake.

## 7. Voice

Short, friendly, concrete. Say what it does and what to check; mark guesses with `(?)`. Use 'record player' and 'disco box' for the object, 'brick' for the 12 V supply, 'buck' for the converter. No hype words; one joke per page at most.

## 8. Do and don't

- **Do** use neon for lines, glows, icons and shapes; keep text dark on light.
- **Do** keep every animation slow and make it optional (reduce motion).
- **Do** give every icon button a label and keep touch targets 44 px.
- **Don't** put white or cyan text on hot pink for anything small (see the contrast table).
- **Don't** add drop shadows or gradients to cards; the Novaroma look is flat and outlined.
- **Don't** flash, strobe or blink; the disco is a slow colour cycle.
- **Don't** use a different corner radius or outline width in a new component.

## 9. Where it lives in the code

- Site CSS: `site/build_site.py` (`site/discobox-theme.css`, the single stylesheet; the 80s block is marked `/* 80s theme */`).
- Viewer CSS: `cad/viewer_template.html`. Wiring page: `cad/make_wiring_interactive.py`. Old wiring page: `doc/wiring/index.html`.
- Object colours: `cad/make_full_assembly.py` (`C` dict) and `cad/make_recordplayer.py`. Renders: `cad/render_scene.py` (synthwave background).
- This guide: `python3 site/make_design_guide.py` rewrites `doc/DESIGN.md`.
