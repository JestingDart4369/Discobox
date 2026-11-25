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
:root { --ink:#1c2b3a; --soft:#4a6075; --bg:#eef5fc; --card:#fff; --line:#c9d7e6; --accent:#c8191e; }
* { box-sizing: border-box; }
body { margin:0; font-family: -apple-system, Segoe UI, Helvetica, Arial, sans-serif; color:var(--ink); background:var(--bg); line-height:1.55; }
header { background:#fff; border-bottom:1px solid var(--line); position:sticky; top:0; z-index:5; }
nav { max-width:1100px; margin:0 auto; padding:10px 20px; display:flex; flex-wrap:wrap; gap:6px 18px; align-items:center; }
nav .brand { font-weight:700; font-size:18px; color:var(--accent); text-decoration:none; margin-right:10px; }
nav a { color:var(--ink); text-decoration:none; font-size:14px; padding:4px 0; border-bottom:2px solid transparent; }
nav a:hover, nav a.on { border-bottom-color:var(--accent); }
nav .gh { margin-left:auto; }
main { max-width:900px; margin:0 auto; padding:24px 20px 60px; }
main.wide { max-width:1100px; }
h1,h2,h3 { line-height:1.25; } h1 { font-size:30px; } h2 { margin-top:34px; border-bottom:1px solid var(--line); padding-bottom:4px; }
a { color:#0b5cad; } img { max-width:100%; height:auto; border-radius:6px; }
pre, code { background:#e3edf7; border-radius:4px; font-family: ui-monospace, Menlo, Consolas, monospace; font-size:13px; }
code { padding:1px 5px; } pre { padding:12px; overflow:auto; } pre code { padding:0; background:none; }
table { border-collapse:collapse; margin:14px 0; width:100%; background:#fff; font-size:14px; display:block; overflow-x:auto; }
th,td { border:1px solid var(--line); padding:6px 9px; text-align:left; vertical-align:top; } th { background:#e3edf7; }
blockquote { border-left:4px solid var(--line); margin:12px 0; padding:2px 14px; color:var(--soft); }
.hero { display:grid; grid-template-columns: 1.1fr 1fr; gap:26px; align-items:center; margin-bottom:26px; }
.hero img { border-radius:12px; box-shadow:0 8px 28px #1c2b3a22; }
.hero p { color:var(--soft); font-size:17px; }
.cards { display:grid; grid-template-columns:repeat(auto-fill,minmax(220px,1fr)); gap:14px; margin:20px 0; }
.card { background:var(--card); border:1px solid var(--line); border-radius:10px; padding:14px 16px; text-decoration:none; color:var(--ink); }
.card:hover { border-color:var(--accent); box-shadow:0 4px 14px #1c2b3a18; }
.card b { display:block; font-size:16px; margin-bottom:4px; } .card span { color:var(--soft); font-size:13px; }
.gallery { display:grid; grid-template-columns:repeat(auto-fill,minmax(260px,1fr)); gap:12px; }
.badge { display:inline-block; background:#fff3cd; border:1px solid #f0d98a; color:#6b5200; border-radius:12px; padding:2px 10px; font-size:13px; }
footer { text-align:center; color:var(--soft); font-size:13px; padding:26px 10px; border-top:1px solid var(--line); }
@media (max-width:760px) { .hero { grid-template-columns:1fr; } }
"""

NAV = [("index.html", "Home"), ("3d/index.html", "3D viewer"), ("wiring/index.html", "Wiring"), ("enclosure.html", "Enclosure"),
       ("journal.html", "Journal"), ("bom.html", "BOM"), ("project.html", "Project")]
DOCS_NAV = [("relay.html", "Relay"), ("sd-card.html", "SD card"), ("shell.html", "Shell"), ("cad.html", "CAD")]


def page(title, body, cur, depth=0, wide=False):
    pre = "../" * depth
    links = "".join(f'<a href="{pre}{h}"{" class=on" if h == cur else ""}>{t}</a>' for h, t in NAV + DOCS_NAV)
    return f"""<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(title)} · Discobox</title><link rel="stylesheet" href="{pre}style.css"></head><body>
<header><nav><a class="brand" href="{pre}index.html">Discobox</a>{links}<a class="gh" href="{REPO}">GitHub</a></nav></header>
<main{' class="wide"' if wide else ''}>{body}</main>
<footer>Discobox by JestingDart4369 · MIT license · <a href="{REPO}">source on GitHub</a> · built from the repository files</footer></body></html>"""


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
        out.append(f"<h2>{title}</h2><table><thead><tr>" + "".join(f"<th>{html.escape(c)}</th>" for c in rows[0]) + "</tr></thead><tbody>")
        for r in rows[1:]:
            cells = []
            for c in r:
                c = c.strip()
                cells.append(f'<td><a href="{html.escape(c)}">link</a></td>' if c.startswith("http") and " " not in c else f"<td>{html.escape(c)}</td>")
            out.append("<tr>" + "".join(cells) + "</tr>")
        out.append("</tbody></table>")
    return "".join(out)


def landing():
    cards = [("3d/index.html", "3D viewer", "Rotate the record player, switch versions, hide parts."),
             ("wiring/index.html", "Wiring diagrams", "v1.2, v2 and v3 with a version switch."),
             ("enclosure.html", "Enclosure", "Record-player housing, mains isolation, printing."),
             ("journal.html", "Journal", "Build log with hours and photos."),
             ("bom.html", "Bill of materials", "Parts with prices and links."),
             ("project.html", "Project readme", "Features, pins, build and tests."),
             ("relay.html", "Relay", "Wiring, library and console command."),
             ("sd-card.html", "SD card", "DFPlayer folder layout."),
             ("cad.html", "CAD files", "Generators, STL and 3MF files.")]
    cards_html = "".join(f'<a class="card" href="{h}"><b>{t}</b><span>{d}</span></a>' for h, t, d in cards)
    gallery = "".join(f'<a href="img/{f}"><img src="img/{f}" alt="{f}"></a>' for f in
                      ("recordplayer_scene.png", "recordplayer_front.png", "recordplayer_side.png", "recordplayer_top.png", "interior_top.png", "interior_iso.png")
                      if os.path.exists(os.path.join(ROOT, "doc/img", f)))
    return f"""<div class="hero"><div><h1>Discobox</h1>
<p>A button-controlled party box on an Arduino Uno R4 WiFi: LED ring and strip effects, a piezo buzzer, MP3 playback, a relay for a spinning disco light, all inside a record-player enclosure.</p>
<p><span class="badge">Work in progress</span> Firmware and electronics run on the bench; the enclosure is in block-out phase.</p></div>
<img src="img/recordplayer_product.png" alt="Discobox record player render"></div>
<div class="cards">{cards_html}</div><h2>Pictures</h2><div class="gallery">{gallery}</div>"""


def main():
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(os.path.join(OUT, "img", "journal"))
    os.makedirs(os.path.join(OUT, "3d")); os.makedirs(os.path.join(OUT, "wiring"))
    open(os.path.join(OUT, "style.css"), "w").write(CSS)
    open(os.path.join(OUT, ".nojekyll"), "w").write("")
    # assets
    for pre, dst in ASSETS.items():
        src = os.path.join(ROOT, pre)
        if os.path.isdir(src):
            for f in os.listdir(src):
                if os.path.isfile(os.path.join(src, f)):
                    shutil.copy(os.path.join(src, f), os.path.join(OUT, dst, f))
    # viewers (standalone pages with a way back)
    v = open(os.path.join(ROOT, "doc/viewer/discobox_viewer.html"), encoding="utf-8").read()
    v = v.replace("<h1>Discobox 3D viewer</h1>", '<a href="../index.html" style="font-size:12px">&larr; Discobox site</a><h1>Discobox 3D viewer</h1>', 1)
    open(os.path.join(OUT, "3d", "index.html"), "w", encoding="utf-8").write(v)
    wdir = os.path.join(ROOT, "doc/wiring")
    for f in os.listdir(wdir):
        if f == "README.md":
            continue
        shutil.copy(os.path.join(wdir, f), os.path.join(OUT, "wiring", f))
    w = open(os.path.join(OUT, "wiring", "index.html"), encoding="utf-8").read()
    w = w.replace("<h1>Discobox wiring</h1>", '<a href="../index.html" style="font-size:12px">&larr; Discobox site</a><h1>Discobox wiring</h1>', 1)
    open(os.path.join(OUT, "wiring", "index.html"), "w", encoding="utf-8").write(w)
    # pages
    open(os.path.join(OUT, "index.html"), "w", encoding="utf-8").write(page("Home", landing(), "index.html", wide=True))
    for src, (name, title, _) in PAGES.items():
        if os.path.exists(os.path.join(ROOT, src)):
            open(os.path.join(OUT, name), "w", encoding="utf-8").write(page(title, render_md(src), name))
    open(os.path.join(OUT, "bom.html"), "w", encoding="utf-8").write(page("Bill of materials", bom_page(), "bom.html", wide=True))
    n = sum(len(fs) for _, _, fs in os.walk(OUT))
    print(f"site built: {n} files in {os.path.relpath(OUT, ROOT)}/")


if __name__ == "__main__":
    main()
