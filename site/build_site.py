#!/usr/bin/env python3
"""
build_site.py — builds the Discobox GitHub Pages site from the files in this repo.

Run:  pip install markdown && python3 site/build_site.py [outdir]      -> _site/ (or outdir, e.g. docs)
Two ways to publish: (1) Settings > Pages > Deploy from a branch > dev > /docs, after `python3 site/build_site.py docs`;
(2) Settings > Pages > GitHub Actions (see .github/workflows/pages.yml).

Pages: Home, 3D viewer, wiring viewer, enclosure, journal, BOM tables, relay, SD card, shell, CAD notes.
To add a page: add a line to PAGES. Markdown links between pages are rewritten; links to other repo files point to GitHub.
"""
import os, re, csv, shutil, html, posixpath
import markdown

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import sys
OUT = os.path.normpath(os.path.join(ROOT, sys.argv[1])) if len(sys.argv) > 1 else os.path.join(ROOT, "_site")
REPO = "https://github.com/JestingDart4369/Discobox"
BRANCH = "dev"

# source (repo path) -> (output file, title, nav group)
PAGES = {
    "README.md": ("project.html", "Project", "main"),
    "JOURNAL.md": ("journal.html", "Journal", "main"),
    "doc/ENCLOSURE.md": ("enclosure.html", "Enclosure", "main"),
    "doc/RELAY.md": ("relay.html", "Relay", "docs"),
    "doc/SD_CARD.md": ("sd-card.html", "SD card", "docs"),
    "doc/SHELL.md": ("shell.html", "Serial shell", "docs"),
    "cad/README.md": ("cad.html", "CAD files", "docs"),
}
# repo path prefix -> site path
ASSETS = {"doc/img/": "img/", "doc/journal/": "img/journal/"}
SPECIAL = {"doc/wiring/README.md": "wiring/index.html", "doc/viewer/discobox_viewer.html": "3d/index.html",
           "doc/wiring/index.html": "wiring/index.html", "BOM.csv": "bom.html", "BOM_enclosure.csv": "bom.html"}

CSS = """
/* Novaroma colours and shapes (same variables as novaroma-homelab.uk) */
:root { --bg:#f6f7fb; --main:#5E282C; --text:#190607; --accent:#EEE3C9; --muted:#7e6e56; --line:2px; --corner-radius:26px; --pading:1rem; --head:76px; }
* { margin:0; padding:0; box-sizing:border-box; }
html { scroll-padding-top: calc(var(--head) + 16px); }
body { background:var(--bg); color:var(--main); font-family:'Roboto', system-ui, -apple-system, 'Segoe UI', Arial, sans-serif; min-height:100vh; display:flex; flex-direction:column; line-height:1.6; }
a { color:var(--main); text-decoration:none; } a:hover { color:var(--muted); }
img { max-width:100%; height:auto; border-radius:12px; }
/* Header: stays on screen */
header { position:sticky; top:0; z-index:50; background:var(--bg); border-bottom:var(--line) solid var(--main); display:flex; align-items:center; gap:1rem; padding:.75rem 2rem; min-height:var(--head); }
.website-logo { height:48px; width:auto; border-radius:6px; }
.website-title .site-title { font-size:1rem; font-weight:600; color:var(--text); display:block; } .website-title p { font-size:.75rem; color:var(--main); }
nav { margin-left:auto; }
.nav-links { display:flex; gap:1rem; list-style:none; align-items:center; }
.nav-links > li > a { font-size:.9rem; padding:6px 12px; border-radius:999px; border:var(--line) solid transparent; transition:.2s ease; color:var(--main); display:inline-block; }
.nav-links > li > a:hover { border-color:var(--accent); background:var(--bg); }
.nav-links a.active { background:var(--accent); border-color:var(--accent); }
.dropdown { position:relative; }
.dropdown > a::after { content:" \25BE"; font-size:.7rem; }
.dropdown-menu { display:none; position:absolute; top:100%; left:0; min-width:210px; list-style:none; background:var(--bg); border:var(--line) solid var(--accent); border-radius:var(--corner-radius); padding:.5rem; }
.dropdown:hover .dropdown-menu, .dropdown:focus-within .dropdown-menu, .dropdown.open .dropdown-menu { display:block; }
.dropdown-menu a { display:block; padding:6px 12px; border-radius:999px; font-size:.88rem; border:var(--line) solid transparent; }
.dropdown-menu a:hover { border-color:var(--accent); } .dropdown-menu a.active { background:var(--accent); }
.btn-api { margin-left:.5rem; padding:.4rem 1.2rem; border:var(--line) solid var(--main); border-radius:999px; font-size:.85rem; white-space:nowrap; color:var(--main); background:var(--accent); }
.btn-api:hover { background:var(--main); color:var(--bg); }
.hamburger-menu { display:none; background:var(--accent); border:1px solid var(--main); border-radius:var(--corner-radius); color:var(--main); padding:.3rem .6rem; cursor:pointer; font-size:1.2rem; margin-left:auto; }
/* Layout */
main { display:flex; gap:2rem; padding:2rem; flex:1; max-width:1240px; width:100%; margin:0 auto; align-items:flex-start; }
main.stack { flex-direction:column; align-items:stretch; }
main.full { max-width:none; padding:0; }
.card { border:var(--line) solid var(--accent); border-radius:var(--corner-radius); padding:var(--pading); background:#fff8; }
.content { flex:1; min-width:0; padding:1.4rem 1.8rem; }
.toc { width:250px; flex:none; position:sticky; top:calc(var(--head) + 16px); max-height:calc(100vh - var(--head) - 40px); overflow:auto; }
.toc h2 { font-size:.75rem; text-transform:uppercase; letter-spacing:.08em; color:var(--text); font-weight:700; margin:0 0 .5rem; border:0; padding:0; }
.toc a { display:block; font-size:.84rem; padding:4px 10px; border-radius:999px; border:var(--line) solid transparent; margin-bottom:2px; }
.toc a:hover { border-color:var(--accent); }
.toc .sub { font-size:.78rem; margin-left:.8rem; color:var(--muted); }
.content h1 { font-size:2rem; color:var(--text); line-height:1.2; margin-bottom:.8rem; }
.content h2 { font-size:1.3rem; color:var(--text); margin:2rem 0 .6rem; padding-bottom:.3rem; border-bottom:var(--line) solid var(--accent); }
.content h3 { font-size:1.05rem; color:var(--text); margin:1.4rem 0 .4rem; }
.content p, .content li { margin-bottom:.7rem; } .content ul, .content ol { padding-left:1.4rem; margin-bottom:.8rem; }
.content a { text-decoration:underline; text-decoration-color:var(--accent); text-underline-offset:3px; } .content a:hover { text-decoration-color:var(--main); }
.content blockquote { border-left:4px solid var(--accent); padding:2px 14px; color:var(--muted); margin:12px 0; }
pre, code { background:var(--accent); border-radius:6px; font-family:ui-monospace, Menlo, Consolas, monospace; font-size:.82rem; } code { padding:1px 6px; } pre { padding:12px; overflow:auto; margin-bottom:1rem; } pre code { padding:0; background:none; }
table { border-collapse:separate; border-spacing:0; margin:14px 0; width:100%; font-size:.86rem; display:block; overflow-x:auto; border:var(--line) solid var(--accent); border-radius:14px; }
th, td { border-bottom:1px solid var(--accent); padding:7px 11px; text-align:left; vertical-align:top; } th { background:var(--accent); color:var(--text); }
tr:last-child td { border-bottom:0; }
/* Home */
.hero { flex:2; display:flex; gap:1.6rem; align-items:center; padding:1.6rem; }
.hero-text { flex:1; } .hero img { flex:1; max-width:48%; }
.hero-sub { font-size:.85rem; border:var(--line) solid var(--accent); border-radius:999px; padding:2px 12px; display:inline-block; margin-bottom:.4rem; }
.hero h1 { font-size:2.5rem; margin:.6rem 0; line-height:1.15; color:var(--text); } .hero p { max-width:520px; margin-bottom:1rem; }
.btn-primary, .btn-secondary { display:inline-block; padding:.4rem 1.2rem; border-radius:999px; font-size:.9rem; border:var(--line) solid var(--main); margin:0 .6rem .5rem 0; }
.btn-primary { background:var(--accent); } .btn-primary:hover { background:var(--main); color:var(--bg); }
.btn-secondary { border-color:var(--accent); } .btn-secondary:hover { border-color:var(--main); color:var(--main); }
.info-panel { flex:1; display:flex; flex-direction:column; gap:1rem; min-width:220px; }
.info-block h2 { font-size:.75rem; text-transform:uppercase; color:var(--text); margin-bottom:.5rem; letter-spacing:.08em; font-weight:700; } .info-block p { font-size:.85rem; line-height:1.5; }
.tags { display:flex; flex-wrap:wrap; gap:.4rem; margin-top:.4rem; } .tag { border:1px solid var(--accent); border-radius:999px; font-size:.75rem; padding:.2rem .7rem; }
.group h2 { font-size:1.3rem; color:var(--text); margin:0 0 .2rem; } .group > p { font-size:.85rem; margin-bottom:1rem; }
.cards { display:grid; grid-template-columns:repeat(auto-fill,minmax(230px,1fr)); gap:1.2rem; }
.project-card { border:var(--line) solid var(--accent); border-radius:21px; padding:1rem; display:block; background:#fff8; transition:.2s ease; }
.project-card:hover { border-color:var(--main); } .project-card h3 { font-size:1rem; margin-bottom:.3rem; color:var(--main); } .project-card p { font-size:.85rem; color:var(--muted); line-height:1.5; margin:0; }
.gallery { display:grid; grid-template-columns:repeat(auto-fill,minmax(250px,1fr)); gap:12px; }
/* Journal: handwriting */
.journal { font-family:'Eric Handwriting','Segoe Print','Bradley Hand','Chalkboard SE','Comic Sans MS',cursive; }
.entry { margin-bottom:1.6rem; padding:1.2rem 1.6rem 1rem; border:var(--line) solid var(--accent); border-radius:var(--corner-radius);
  background:#fffdf6 repeating-linear-gradient(transparent, transparent 1.9rem, #EEE3C9 1.9rem, #EEE3C9 calc(1.9rem + 1px)); }
.entry h2 { font-family:inherit; font-size:1.7rem; margin:0 0 .6rem; border:0; color:var(--text); }
.entry p { font-size:1.15rem; line-height:1.9rem; margin-bottom:.9rem; }
.entry img { margin:.4rem 0; }
.journal figure { margin:0; } .journal figcaption { font-size:.85rem; color:var(--muted); }
.hours { display:inline-block; font-family:'Roboto',system-ui,sans-serif; font-size:.85rem !important; line-height:1.2 !important; background:var(--accent); border-radius:999px; padding:3px 14px; color:var(--text); }
.total { font-size:.85rem; border:var(--line) solid var(--accent); border-radius:14px; padding:8px 12px; margin-top:.8rem; }
/* Embedded viewers keep the header */
.embed { width:100%; height:calc(100vh - var(--head)); border:0; display:block; background:#dcebf8; }
/* Footer */
footer { display:flex; justify-content:space-between; align-items:center; padding:1rem 2rem; font-size:.8rem; border-top:var(--line) solid var(--main); }
.footer-nav { display:flex; gap:.8rem; flex-wrap:wrap; } .footer-nav a { border:var(--line) solid var(--accent); border-radius:999px; padding:4px 14px; transition:.2s ease; } .footer-nav a:hover { border-color:var(--main); }

/* Hidden credits: press the copyright line 5 times. Slow colour cycling only (no flashing). */
.footer-copy { background:none; border:0; font:inherit; color:inherit; cursor:default; user-select:none; padding:0; -webkit-tap-highlight-color:transparent; }
.credits[hidden] { display:none; }
.credits { position:fixed; inset:0; z-index:100; display:flex; align-items:center; justify-content:center; background:#12002b; overflow:auto; padding:1.5rem; }
.disco-bg { position:absolute; inset:0; overflow:hidden; pointer-events:none; }
.beams { position:absolute; inset:-60%; mix-blend-mode:screen; animation:spin 40s linear infinite, hue 24s linear infinite;
  background:conic-gradient(from 0deg at 50% 38%, #ff2fa366, transparent 7%, #2ff3ff66 13%, transparent 20%, #ffe12f55 26%, transparent 34%, #7a2fff66 40%, transparent 48%, #ff2fa366 54%, transparent 62%, #2fff8a55 68%, transparent 76%, #ff7a2f66 82%, transparent 90%); }
.floor { position:absolute; left:-20%; right:-20%; bottom:-10%; height:46%; transform:perspective(420px) rotateX(62deg); transform-origin:50% 100%; animation:hue 18s linear infinite;
  background:repeating-conic-gradient(#ff2fa3 0 25%, #2ff3ff 0 50%, #7a2fff 0 75%, #ffe12f 0 100%) 0 0/90px 90px; opacity:.55; }
.ball { position:absolute; left:50%; top:5%; width:130px; height:130px; margin-left:-65px; border-radius:50%; animation:hue 24s linear infinite;
  background:repeating-linear-gradient(0deg,#0007 0 7px,transparent 7px 14px), repeating-linear-gradient(90deg,#0007 0 7px,transparent 7px 14px), radial-gradient(circle at 35% 30%, #fff, #cfd6ff 22%, #7d7fa6 58%, #231b3d); box-shadow:0 0 60px 12px #ffffff55; }
.ball::before { content:""; position:absolute; left:50%; bottom:100%; width:2px; height:60vh; background:#ffffff88; }
.credits-card { position:relative; z-index:2; max-width:600px; width:100%; margin:auto; background:#ffffffe0; backdrop-filter:blur(6px); color:var(--main); border:var(--line) solid var(--accent); border-radius:var(--corner-radius); padding:1.4rem 1.8rem; }
.credits-card h2 { color:var(--text); font-size:1.5rem; margin-bottom:.2rem; } .credits-card h3 { color:var(--text); font-size:.8rem; text-transform:uppercase; letter-spacing:.08em; margin:1rem 0 .3rem; }
.credits-card p, .credits-card li { font-size:.9rem; margin-bottom:.3rem; } .credits-card ul { padding-left:1.2rem; }
.credits-card button { margin-top:1rem; padding:.4rem 1.4rem; border:var(--line) solid var(--main); border-radius:999px; background:var(--accent); color:var(--main); cursor:pointer; font-size:.9rem; }
.credits-card button:hover { background:var(--main); color:var(--bg); }
@keyframes spin { to { transform:rotate(360deg); } } @keyframes hue { to { filter:hue-rotate(360deg); } }
@media (prefers-reduced-motion: reduce) { .beams, .floor, .ball { animation:none; } * { transition:none !important; } a:active, button:active { transform:none; } }
a:focus-visible, button:focus-visible { outline:3px solid var(--text); outline-offset:2px; border-radius:6px; }
a:active, button:active { transform:scale(.98); }
.btn-primary:active { background:#451d20; color:var(--bg); }
.sr-only { position:absolute; width:1px; height:1px; overflow:hidden; clip:rect(0 0 0 0); white-space:nowrap; }
@media (max-width: 860px) {
  header { padding:.75rem 1rem; flex-wrap:wrap; }
  .hamburger-menu { display:block; } .btn-api { display:none; }
  nav { width:100%; margin:0; order:5; } .nav-links { display:none; flex-direction:column; align-items:stretch; gap:.4rem; padding:.6rem 0; }
  .nav-links.open { display:flex; } .dropdown-menu { display:block; position:static; border:0; box-shadow:none; padding:0 0 0 1rem; }
  main { flex-direction:column; padding:1rem; } .toc { position:static; width:100%; max-height:none; } .hero { flex-direction:column; } .hero img { max-width:100%; }
  footer { flex-direction:column; gap:.8rem; text-align:center; } .embed { height:calc(100vh - 120px); }
}
"""

# nav: (label, href or None, children)
NAV = [("Home", "index.html", None),
       ("Build", None, [("3D viewer", "3d/index.html"), ("Wiring diagrams", "wiring/index.html"), ("Enclosure", "enclosure.html"),
                        ("CAD files", "cad.html"), ("Bill of materials", "bom.html")]),
       ("Docs", None, [("Project readme", "project.html"), ("Relay", "relay.html"), ("SD card", "sd-card.html"), ("Serial shell", "shell.html")]),
       ("Journal", "journal.html", None)]
LOGO = "https://novaroma-homelab.uk/assets/nova-roma-logo-256.webp"


def head_html(cur, pre):
    items = []
    for label, href, kids in NAV:
        if kids is None:
            items.append(f'<li><a href="{pre}{href}"{" class=active" if href == cur else ""}>{label}</a></li>')
        else:
            on = any(h == cur for _, h in kids)
            sub = "".join(f'<li><a href="{pre}{h}"{" class=active" if h == cur else ""}>{t}</a></li>' for t, h in kids)
            items.append(f'<li class="dropdown"><a href="#" aria-haspopup="true"{" class=active" if on else ""} onclick="this.parentNode.classList.toggle(\'open\');return false">{label}</a><ul class="dropdown-menu">{sub}</ul></li>')
    return f"""<header>
<a href="{pre}index.html"><img class="website-logo" src="{LOGO}" alt="" onerror="this.style.display='none'"></a>
<div class="website-title"><a href="{pre}index.html" class="site-title">Discobox</a><p>by JestingDart4369</p></div>
<button class="hamburger-menu" aria-label="Menu" onclick="document.getElementById('nl').classList.toggle('open')">&#9776;</button>
<nav><ul class="nav-links" id="nl">{''.join(items)}</ul></nav>
<a class="btn-api" href="https://novaroma-homelab.uk">Novaroma &#8599;</a></header>"""


CREDITS = """<div id="credits" class="credits" hidden role="dialog" aria-modal="true" aria-label="Credits">
<div class="disco-bg" aria-hidden="true"><div class="beams"></div><div class="floor"></div><div class="ball"></div></div>
<div class="credits-card"><h2>Credits</h2><p>You found the disco. Thanks for looking behind the footer.</p>
<h3>Made by</h3><ul><li>JestingDart4369: idea, electronics, firmware, enclosure design, photos and journal</li></ul>
<h3>Music</h3><ul><li>Want a disco track? <a href="https://www.youtube.com/watch?v=5QWpNSWxXFE" target="_blank" rel="noopener">JAWS, &ldquo;Dive Back In Time&rdquo; on YouTube</a> (opens in a new tab). The song is not hosted or embedded here; all rights belong to its owners.</li></ul>
<h3>Sound</h3><ul><li><a href="https://kenney.nl">Kenney UI Audio</a> by Kenney Vleugels, CC0</li><li>UI SFX sound packs, CC0</li></ul>
<h3>Software</h3><ul><li>PlatformIO, Unity test framework, FastLED, the Arduino core for the Uno R4 WiFi</li><li>Python with trimesh, manifold3d, numpy, numba for the CAD files, renders and 3D viewer</li><li>Site layout and colours follow novaroma-homelab.uk</li></ul>
<h3>Hardware</h3><ul><li>Arduino Uno R4 WiFi, WS2812 ring, WS2805 strip, DFPlayer Mini, 1-channel relay, PAM8403 amplifier</li></ul>
<h3>AI</h3><ul><li>Developed with the help of Claude (Anthropic) for documentation, code review, tests, CAD scripts and this site. The journal is written by me.</li></ul>
<button id="cclose" type="button">Close</button></div></div>
<script>(function(){var el=document.getElementById('credits'),fc=document.getElementById('fc'),t=[];
function open(){el.hidden=false;document.body.style.overflow='hidden';document.getElementById('cclose').focus();}
function close(){el.hidden=true;document.body.style.overflow='';}
fc.addEventListener('click',function(){var n=Date.now();t.push(n);t=t.filter(function(x){return n-x<4000;});if(t.length>=5){t=[];open();}});
document.getElementById('cclose').addEventListener('click',close);
document.addEventListener('keydown',function(e){if(e.key==='Escape')close();});
})();</script>"""


def foot_html(pre):
    links = "".join(f'<a href="{pre}{h}">{t}</a>' for t, h in (("Home", "index.html"), ("3D viewer", "3d/index.html"), ("Journal", "journal.html"), ("BOM", "bom.html")))
    return f"""<footer><span><button class="footer-copy" id="fc" type="button">&copy; 2026 JestingDart4369</button> &middot; MIT license &middot; <a href="{REPO}">source on GitHub</a></span>
<div class="footer-nav">{links}<a href="https://novaroma-homelab.uk">novaroma-homelab.uk</a></div></footer>{CREDITS}"""


def page(title, body, cur, depth=0, layout="stack", extra_head=""):
    pre = "../" * depth
    return f"""<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#5E282C"><title>{html.escape(title)} · Discobox</title><link rel="stylesheet" href="{pre}style.css">{extra_head}</head><body>
{head_html(cur, pre)}
<main class="{layout}">{body}</main>
{foot_html(pre)}</body></html>"""


def toc_html(body, levels=("2",), title="On this page", extra=""):
    items = []
    for m in re.finditer(r'<h([1-3]) id="([^"]+)">(.*?)</h\1>', body, flags=re.S):
        lvl, hid, txt = m.groups()
        if lvl in levels:
            items.append(f'<a href="#{hid}">{re.sub("<[^>]+>", "", txt)}</a>')
    if not items:
        return ""
    return f'<aside class="toc card"><h2>{title}</h2>{"".join(items)}{extra}</aside>'


def strip_frontmatter(t):
    return re.sub(r"\A---\n.*?\n---\n", "", t, flags=re.S)


def target(src_dir, link):
    """Rewrite one relative link found in a markdown page."""
    if re.match(r"^(https?:|mailto:|#|data:)", link):
        return link
    path, _, frag = link.partition("#")
    full = posixpath.normpath(posixpath.join(src_dir, path)) if path else ""
    if not full:
        return link
    if full in PAGES:
        return PAGES[full][0] + ("#" + frag if frag else "")
    if full in SPECIAL:
        return SPECIAL[full] + ("#" + frag if frag else "")
    for pre, dst in ASSETS.items():
        if full.startswith(pre):
            return dst + full[len(pre):]
    if os.path.isdir(os.path.join(ROOT, full)):
        return f"{REPO}/tree/{BRANCH}/{full}"
    return f"{REPO}/blob/{BRANCH}/{full}"


def render_md(src):
    text = open(os.path.join(ROOT, src), encoding="utf-8").read()
    body = markdown.markdown(strip_frontmatter(text), extensions=["tables", "fenced_code", "sane_lists", "toc"])
    d = posixpath.dirname(src)
    body = re.sub(r'(href|src)="([^"]+)"', lambda m: f'{m.group(1)}="{target(d, m.group(2))}"', body)
    return body


def bom_page():
    out = ["<h1>Bill of materials</h1><p>Prices are in CHF and partly estimates; check them before ordering. Source files: "
           f'<a href="{REPO}/blob/{BRANCH}/BOM.csv">BOM.csv</a>, <a href="{REPO}/blob/{BRANCH}/BOM_enclosure.csv">BOM_enclosure.csv</a>.</p>']
    for fn, title in (("BOM.csv", "Electronics"), ("BOM_enclosure.csv", "Enclosure, cables and mains side")):
        rows = list(csv.reader(open(os.path.join(ROOT, fn), encoding="utf-8")))
        rows = [r for r in rows if any(c.strip() for c in r)]
        out.append(f'<h2 id="{fn[:-4].lower()}">{title}</h2><table><thead><tr>' + "".join(f"<th>{html.escape(c)}</th>" for c in rows[0]) + "</tr></thead><tbody>")
        for r in rows[1:]:
            cells = []
            for c in r:
                c = c.strip()
                cells.append(f'<td><a href="{html.escape(c)}">link</a></td>' if c.startswith("http") and " " not in c else f"<td>{html.escape(c)}</td>")
            out.append("<tr>" + "".join(cells) + "</tr>")
        out.append("</tbody></table>")
    body = "".join(out)
    return toc_html(body) + f'<div class="content card">{body}</div>'


def landing():
    groups = [
        ("Build it", "The enclosure and the electronics.",
         [("3d/index.html", "3D viewer", "Rotate the record player, switch versions, hide parts."),
          ("enclosure.html", "Enclosure", "Record-player housing, mains isolation, printing."),
          ("wiring/index.html", "Wiring diagrams", "v1.2, v2 and v3 with a version switch."),
          ("bom.html", "Bill of materials", "Parts with prices and links."),
          ("cad.html", "CAD files", "Generators, STL and 3MF files.")]),
        ("Understand it", "How the firmware and the parts work.",
         [("project.html", "Project readme", "Features, pins, build and tests."),
          ("relay.html", "Relay", "Wiring, library and console command."),
          ("sd-card.html", "SD card", "DFPlayer folder layout."),
          ("shell.html", "Serial shell", "Console commands.")]),
        ("Follow it", "What happened so far.", [("journal.html", "Journal", "Build log with hours, in my handwriting.")]),
    ]
    sections = ""
    for t, d, cs in groups:
        cards = "".join(f'<a class="project-card" href="{h}"><h3>{n}</h3><p>{x}</p></a>' for h, n, x in cs)
        sections += f'<section class="group card"><h2>{t}</h2><p>{d}</p><div class="cards">{cards}</div></section>'
    gallery = "".join(f'<a href="img/{f}"><img src="img/{f}" alt="{f}"></a>' for f in
                      ("recordplayer_scene.png", "recordplayer_front.png", "recordplayer_side.png", "recordplayer_top.png", "interior_top.png", "interior_iso.png")
                      if os.path.exists(os.path.join(ROOT, "doc/img", f)))
    hero = f"""<div class="top" style="display:flex;gap:2rem;align-items:flex-start;flex-wrap:wrap">
<section class="hero card" style="flex:2;min-width:300px"><div class="hero-text"><span class="hero-sub">party box &bull; Arduino &bull; record player</span><h1>A record player that runs the party.</h1>
<p>A button-controlled party box on an Arduino Uno R4 WiFi: LED ring and strip effects, a piezo buzzer, MP3 playback and a relay for a spinning disco light, built into a record-player enclosure.</p>
<a class="btn-primary" href="3d/index.html">Open 3D viewer</a><a class="btn-secondary" href="journal.html">Read the journal</a></div>
<img src="img/recordplayer_product.png" alt="Discobox record player render"></section>
<div class="info-panel" style="flex:1">
<div class="info-block card"><h2>Now</h2><p>Designing the record-player enclosure: block-outs, module mounts, cables and the mains compartment.</p></div>
<div class="info-block card"><h2>Status</h2><p>Work in progress. Firmware and electronics run on the bench; the enclosure is in the block-out phase.</p></div>
<div class="info-block card"><h2>Stack</h2><div class="tags"><span class="tag">Arduino Uno R4 WiFi</span><span class="tag">PlatformIO</span><span class="tag">C++</span><span class="tag">FastLED</span><span class="tag">WS2812 / WS2805</span><span class="tag">DFPlayer Mini</span><span class="tag">Python CAD</span></div></div>
</div></div>"""
    return hero + sections + f'<section class="group card"><h2>Pictures</h2><p>Renders of the current design.</p><div class="gallery">{gallery}</div></section>'


def viewer_page(title, cur, src):
    return page(title, f'<h1 class="sr-only">{title}</h1><iframe class="embed" src="{src}" title="{title}"></iframe>', cur, depth=1, layout="full")


def journal_page(src):
    body = render_md(src)
    parts = re.split(r'(?=<h1 id=)', body)
    entries, hours = [], 0
    toc = []
    for p in parts:
        if not p.strip():
            continue
        m = re.match(r'<h1 id="([^"]+)">(.*?)</h1>', p, flags=re.S)
        h = re.search(r"Total time spent:\s*(\d+)", p)
        if h:
            hours += int(h.group(1))
        p = re.sub(r"<p><strong>(Total time spent:[^<]*)</strong></p>", r'<p class="hours">\1</p>', p)
        if m:
            toc.append(f'<a href="#{m.group(1)}">{re.sub("<[^>]+>", "", m.group(2))}</a>')
        p = re.sub(r'<h1 id="([^"]+)">(.*?)</h1>', r'<h2 id="\1">\2</h2>', p, count=1, flags=re.S)
        entries.append(f'<article class="entry">{p}</article>')
    toc.append('<a href="#design-renders">Design renders</a>')
    aside = f'<aside class="toc card"><h2>Entries</h2>{"".join(toc)}<div class="total">Total: {hours} hours logged</div></aside>'
    shots = [("recordplayer_product.png", "Product render"), ("recordplayer_scene.png", "On a side table"), ("recordplayer_front.png", "Front"),
             ("recordplayer_side.png", "Side: pipe, bend and hub"), ("recordplayer_top.png", "Top"), ("interior_top.png", "Inside, top view"), ("interior_iso.png", "Inside, iso view")]
    gal = "".join(f'<figure><a href="img/{f}"><img src="img/{f}" alt="{c}"></a><figcaption>{c}</figcaption></figure>' for f, c in shots
                  if os.path.exists(os.path.join(ROOT, "doc/img", f)))
    gallery = ('<article class="entry"><h2 id="design-renders">Design renders</h2><p>Renders of the enclosure design from the 3D model '
               '(not photos of the build). Real photos go into the entries above.</p><div class="gallery">' + gal + '</div></article>')
    return aside + f'<div class="content journal"><h1 class="sr-only">Journal</h1>{"".join(entries)}{gallery}</div>'


def doc_page(src):
    body = render_md(src)
    return toc_html(body) + f'<div class="content card">{body}</div>'


def main():
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(os.path.join(OUT, "img", "journal"))
    os.makedirs(os.path.join(OUT, "3d")); os.makedirs(os.path.join(OUT, "wiring")); os.makedirs(os.path.join(OUT, "fonts"))
    css = CSS
    fdir = os.path.join(ROOT, "site", "fonts")
    for f in ("handwriting.woff2", "handwriting.ttf"):
        if os.path.exists(os.path.join(fdir, f)):
            shutil.copy(os.path.join(fdir, f), os.path.join(OUT, "fonts", f))
            fmt = "woff2" if f.endswith("woff2") else "truetype"
            css = f"@font-face {{ font-family:'Eric Handwriting'; src:url(fonts/{f}) format('{fmt}'); font-display:swap; }}\n" + css
            break
    open(os.path.join(OUT, "style.css"), "w", encoding="utf-8").write(css)
    open(os.path.join(OUT, ".nojekyll"), "w").write("")
    for pre, dst in ASSETS.items():
        src = os.path.join(ROOT, pre)
        if os.path.isdir(src):
            for f in os.listdir(src):
                if os.path.isfile(os.path.join(src, f)):
                    shutil.copy(os.path.join(src, f), os.path.join(OUT, dst, f))
    # viewers: the standalone file goes in an iframe so the site header stays on screen
    shutil.copy(os.path.join(ROOT, "doc/viewer/discobox_viewer.html"), os.path.join(OUT, "3d", "viewer.html"))
    wdir = os.path.join(ROOT, "doc/wiring")
    for f in os.listdir(wdir):
        if f == "README.md":
            continue
        shutil.copy(os.path.join(wdir, f), os.path.join(OUT, "wiring", "diagrams.html" if f == "index.html" else f))
    open(os.path.join(OUT, "3d", "index.html"), "w", encoding="utf-8").write(viewer_page("3D viewer", "3d/index.html", "viewer.html"))
    open(os.path.join(OUT, "wiring", "index.html"), "w", encoding="utf-8").write(viewer_page("Wiring diagrams", "wiring/index.html", "diagrams.html"))
    # pages
    open(os.path.join(OUT, "index.html"), "w", encoding="utf-8").write(page("Home", landing(), "index.html", layout="stack"))
    for src, (name, title, _) in PAGES.items():
        if not os.path.exists(os.path.join(ROOT, src)):
            continue
        body = journal_page(src) if src == "JOURNAL.md" else doc_page(src)
        open(os.path.join(OUT, name), "w", encoding="utf-8").write(page(title, body, name))
    open(os.path.join(OUT, "bom.html"), "w", encoding="utf-8").write(page("Bill of materials", bom_page(), "bom.html"))
    n = sum(len(fs) for _, _, fs in os.walk(OUT))
    print(f"site built: {n} files in {os.path.relpath(OUT, ROOT)}/")


if __name__ == "__main__":
    main()
