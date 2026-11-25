#!/usr/bin/env python3
"""
build_site.py — builds the Discobox GitHub Pages site from the files in this repo.

Run:  pip install markdown && python3 site/build_site.py [outdir]      -> _site/ (or outdir, e.g. docs)
Two ways to publish: (1) Settings > Pages > Deploy from a branch > dev > /docs, after `python3 site/build_site.py docs`;
(2) Settings > Pages > GitHub Actions (see .github/workflows/pages.yml).

Pages: Home, 3D viewer, wiring viewer, enclosure, journal, BOM tables, relay, SD card, shell, CAD notes.
To add a page: add a line to PAGES. Markdown links between pages are rewritten; links to other repo files point to GitHub.
"""
import os, re, csv, json, shutil, html, posixpath
import markdown

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import sys
OUT = os.path.normpath(os.path.join(ROOT, sys.argv[1])) if len(sys.argv) > 1 else os.path.join(ROOT, "_site")
REPO = "https://github.com/JestingDart4369/Discobox"
BRANCH = "dev"
DOMAIN = "discobox.novaroma-homelab.uk"      # custom domain; written to CNAME in the output so the setting survives every deploy

# source (repo path) -> (output file, title, nav group)
PAGES = {
    "README.md": ("project.html", "Project", "main"),
    "JOURNAL.md": ("journal.html", "Journal", "main"),
    "doc/ENCLOSURE.md": ("enclosure.html", "Enclosure", "main"),
    "doc/RELAY.md": ("relay.html", "Relay", "docs"),
    "doc/SD_CARD.md": ("sd-card.html", "SD card", "docs"),
    "doc/SHELL.md": ("shell.html", "Serial shell", "docs"),
    "cad/README.md": ("cad.html", "CAD files", "docs"),
    "doc/CABLES.md": ("cables.html", "Cables and plugs", "docs"),
    "doc/DESIGN.md": ("design.html", "Design guide", "docs"),
}
# repo path prefix -> site path
ASSETS = {"doc/img/": "img/", "doc/journal/": "img/journal/"}
SPECIAL = {"doc/wiring/README.md": "wiring/index.html", "doc/viewer/discobox_viewer.html": "3d/index.html",
           "doc/wiring/index.html": "wiring/index.html", "BOM.csv": "bom.html", "BOM_enclosure.csv": "bom.html"}

CSS = open(os.path.join(ROOT, "site", "discobox-theme.css"), encoding="utf-8").read()

# nav: (label, href or None, children)
NAV = [("Home", "index.html", None),
       ("Build", None, [("3D viewer", "3d/index.html"), ("Wiring diagrams", "wiring/index.html"), ("Interactive wiring", "wiring/interactive.html"), ("Cables and plugs", "cables.html"), ("Enclosure", "enclosure.html"),
                        ("CAD files", "cad.html"), ("Bill of materials", "bom.html")]),
       ("Docs", None, [("Project readme", "project.html"), ("Design guide", "design.html"), ("Relay", "relay.html"), ("SD card", "sd-card.html"), ("Serial shell", "shell.html")]),
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
            items.append(f'<li class="dropdown"><a href="#" aria-haspopup="true"{" class=active" if on else ""} onclick="var o=this.parentNode.classList.toggle(\'open\');this.setAttribute(\'aria-expanded\',o);return false" aria-expanded="false">{label}</a><ul class="dropdown-menu">{sub}</ul></li>')
    return f"""<header>
<a href="{pre}index.html"><img class="website-logo" src="{LOGO}" alt="" onerror="this.style.display='none'"></a>
<svg class="brand-vinyl" viewBox="0 0 40 40" aria-hidden="true"><circle cx="20" cy="20" r="18" fill="#1d0646"/><circle cx="20" cy="20" r="13" fill="none" stroke="#7a2fff" stroke-width=".8"/><circle cx="20" cy="20" r="9.5" fill="none" stroke="#7a2fff" stroke-width=".8"/><circle cx="20" cy="20" r="6" fill="#ff2fa3"/><circle cx="20" cy="20" r="1.4" fill="#fff"/><path d="M6 11a16 16 0 0 1 11-7" fill="none" stroke="#2ff3ff" stroke-width="2" stroke-linecap="round"/></svg>
<div class="website-title"><a href="{pre}index.html" class="site-title">Discobox</a><p>by JestingDart4369</p></div>
<button class="hamburger-menu" aria-label="Menu" onclick="document.getElementById('nl').classList.toggle('open')">&#9776;</button>
<nav><ul class="nav-links" id="nl">{''.join(items)}</ul></nav>
<a class="btn-api" href="https://novaroma-homelab.uk">Novaroma &#8599;</a></header>"""


def load_quotes():
    """Quotes for the secret screen from site/secret_quotes.txt ('quote | source' per line, # = comment)."""
    f = os.path.join(ROOT, "site", "secret_quotes.txt"); out = []
    if os.path.exists(f):
        for line in open(f, encoding="utf-8"):
            line = line.strip()
            if line and not line.startswith("#") and "|" in line:
                q, src = line.rsplit("|", 1); out.append([q.strip().replace("\\n", "\n"), src.strip()])
    return out


CREDITS = """<div id="credits" class="credits" hidden role="dialog" aria-modal="true" aria-label="Credits">
<div class="disco-bg" aria-hidden="true"><div class="stars"></div><div class="sun"></div><div class="horizon"></div><div class="floor"></div><div class="lasers"><div class="laser"></div><div class="laser"></div><div class="laser"></div><div class="laser"></div><div class="laser"></div></div><div class="smoke"><i></i><i></i><i></i><i></i><i></i></div><div class="spots"></div><div class="eq"><i style="animation-delay:-0.00s;animation-duration:1.1s"></i><i style="animation-delay:-0.37s;animation-duration:1.8s"></i><i style="animation-delay:-0.74s;animation-duration:1.6s"></i><i style="animation-delay:-1.11s;animation-duration:1.4s"></i><i style="animation-delay:-1.48s;animation-duration:1.2s"></i><i style="animation-delay:-0.25s;animation-duration:1.9s"></i><i style="animation-delay:-0.62s;animation-duration:1.7s"></i><i style="animation-delay:-0.99s;animation-duration:1.5s"></i><i style="animation-delay:-1.36s;animation-duration:1.3s"></i><i style="animation-delay:-0.13s;animation-duration:1.1s"></i><i style="animation-delay:-0.50s;animation-duration:1.8s"></i><i style="animation-delay:-0.87s;animation-duration:1.6s"></i><i style="animation-delay:-1.24s;animation-duration:1.4s"></i><i style="animation-delay:-0.01s;animation-duration:1.2s"></i><i style="animation-delay:-0.38s;animation-duration:1.9s"></i><i style="animation-delay:-0.75s;animation-duration:1.7s"></i><i style="animation-delay:-1.12s;animation-duration:1.5s"></i><i style="animation-delay:-1.49s;animation-duration:1.3s"></i><i style="animation-delay:-0.26s;animation-duration:1.1s"></i><i style="animation-delay:-0.63s;animation-duration:1.8s"></i><i style="animation-delay:-1.00s;animation-duration:1.6s"></i><i style="animation-delay:-1.37s;animation-duration:1.4s"></i><i style="animation-delay:-0.14s;animation-duration:1.2s"></i><i style="animation-delay:-0.51s;animation-duration:1.9s"></i></div><div class="scan"></div></div>
<div class="ball" aria-hidden="true"></div>
<div class="marquee" aria-hidden="true"><span>&#9733; DISCOBOX &#9733; 1985 &#9733; PRESS PLAY &#9733; TURN IT UP &#9733; DISCOBOX &#9733;</span></div>
<div class="credits-card"><h2>&#9733; Credits &#9733;</h2><p>You found the disco. Welcome to 1985. Thanks for looking behind the footer.</p>
<div id="cquote-box" hidden><h3>Keepers of the Lost Cities</h3><blockquote id="cq" aria-live="off" class="cquote"></blockquote></div>
<h3>Made by</h3><ul><li>JestingDart4369: idea, electronics, firmware, enclosure design, photos and journal</li></ul>
<h3>Music</h3><ul><li>Want a disco track? <a href="https://www.youtube.com/watch?v=5QWpNSWxXFE" target="_blank" rel="noopener">JAWS, &ldquo;Dive Back In Time&rdquo; on YouTube</a> (opens in a new tab). The song is streamed from YouTube through its player, not hosted on this site; all rights belong to its owners. It starts when this window opens and stops when you close it. If your browser blocks it, press play.</li></ul><div id="cmusic" aria-label="Music player"></div>
<h3>Quotes</h3><ul><li><em>Keeper of the Lost Cities</em> by Shannon Messenger, &copy; the author and her publishers. Quotes are short excerpts shown with the source named; the wording comes from fan lists on <a href="https://www.goodreads.com/work/quotes/17993650" target="_blank" rel="noopener">Goodreads</a> and is unchecked. The Fitz and Keefe dialogue is fan-written by Aalishaawowzaa2763 (Reddit, r/KeeperoftheLostCities), shown with credit. Lines marked &ldquo;written for this page&rdquo; are my own. Not affiliated with the author or publisher.</li></ul>
<h3>Sound</h3><ul><li><a href="https://kenney.nl">Kenney UI Audio</a> by Kenney Vleugels, CC0</li><li>UI SFX sound packs, CC0</li></ul>
<h3>Software</h3><ul><li>PlatformIO, Unity test framework, FastLED, the Arduino core for the Uno R4 WiFi</li><li>Python with trimesh, manifold3d, numpy, numba for the CAD files, renders and 3D viewer</li><li>Site layout and colours follow novaroma-homelab.uk</li></ul>
<h3>Hardware</h3><ul><li>Arduino Uno R4 WiFi, WS2812 ring, WS2805 strip, DFPlayer Mini, 1-channel relay, PAM8403 amplifier</li></ul>
<h3>AI</h3><ul><li>Developed with the help of Claude (Anthropic) for documentation, code review, tests, CAD scripts and this site. The journal is written by me.</li></ul>
<button id="cclose" type="button">Close</button></div></div>
<script>(function(){var el=document.getElementById('credits'),fc=document.getElementById('fc'),t=[];
var mu=document.getElementById('cmusic');
function open(){el.hidden=false;document.body.style.overflow='hidden';document.getElementById('cclose').focus();if(!reduce){clearInterval(nt);nt=setInterval(note,450);}if(QJ.length){document.getElementById('cquote-box').hidden=false;qi=0;showQ();clearInterval(qt);qt=setInterval(showQ,9000);}
mu.innerHTML='<iframe width="280" height="158" style="border:0;border-radius:12px" src="https://www.youtube-nocookie.com/embed/5QWpNSWxXFE?autoplay=1&rel=0&playsinline=1" title="JAWS - Dive Back In Time (YouTube)" allow="autoplay; encrypted-media" referrerpolicy="strict-origin-when-cross-origin"></iframe>';}
var QJ=__QJ__,qi=0,qt=null;function showQ(){var b=document.getElementById('cq');if(!QJ.length)return;var x=QJ[qi%QJ.length];qi++;b.classList.add('fade');setTimeout(function(){b.textContent='\u201c'+x[0]+'\u201d';var m=document.createElement('small');m.textContent=x[1];b.appendChild(m);b.classList.remove('fade');},reduce0?0:450);}
var reduce0=window.matchMedia&&matchMedia('(prefers-reduced-motion: reduce)').matches;
var nt=null,reduce=window.matchMedia&&matchMedia('(prefers-reduced-motion: reduce)').matches;
function note(){var ns=document.querySelectorAll('.note');if(ns.length>14)return;var n=document.createElement('span');n.className='note';n.setAttribute('aria-hidden','true');
n.textContent='\u266a\u266b\u266c\u2669'.charAt(Math.floor(Math.random()*4));var left=Math.random()<.5;n.style.left=(left?8+Math.random()*18:74+Math.random()*18)+'vw';n.style.bottom='4vh';
n.style.fontSize=(22+Math.random()*22)+'px';n.style.color='hsl('+Math.floor(Math.random()*360)+',90%,65%)';n.style.setProperty('--dx',(Math.random()*120-60)+'px');n.style.setProperty('--rot',(Math.random()*50-25)+'deg');
document.body.appendChild(n);setTimeout(function(){n.remove();},4300);}
function close(){el.hidden=true;document.body.style.overflow='';mu.innerHTML='';clearInterval(nt);clearInterval(qt);document.querySelectorAll('.note').forEach(function(n){n.remove();});fc.focus();}
fc.addEventListener('click',function(){var n=Date.now();t.push(n);t=t.filter(function(x){return n-x<4000;});fc.classList.toggle('warm',t.length>=3&&t.length<5);if(t.length>=5){t=[];fc.classList.remove('warm');fc.classList.add('hot');setTimeout(function(){fc.classList.remove('hot');},900);open();}});
document.getElementById('cclose').addEventListener('click',close);
document.addEventListener('keydown',function(e){if(e.key==='Escape')close();});
})();</script>"""


CREDITS = CREDITS.replace("__QJ__", json.dumps(load_quotes()).replace("</", "<\\/"))


def foot_html(pre):
    links = "".join(f'<a href="{pre}{h}">{t}</a>' for t, h in (("Home", "index.html"), ("3D viewer", "3d/index.html"), ("Journal", "journal.html"), ("BOM", "bom.html")))
    return f"""<footer><span><button class="footer-copy" id="fc" type="button">&copy; 2026 JestingDart4369</button> &middot; MIT license &middot; <a href="{REPO}">source on GitHub</a></span>
<div class="footer-nav">{links}<a href="https://novaroma-homelab.uk">novaroma-homelab.uk</a></div></footer>{CREDITS}"""


def page(title, body, cur, depth=0, layout="row", extra_head=""):
    pre = "../" * depth
    return f"""<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#5E282C"><meta name="color-scheme" content="only light"><meta name="darkreader-lock"><title>{html.escape(title)} · Discobox</title><link rel="stylesheet" href="{pre}style.css">{extra_head}</head><body>
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
          ("wiring/interactive.html", "Interactive wiring", "Click a wire, see it light up in the 3D model."),
          ("cables.html", "Cables and plugs", "Real lengths and amounts for every cable."),
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
        if f == "interactive.html":
            t = open(os.path.join(wdir, f), encoding="utf-8").read().replace("../viewer/discobox_viewer.html", "../3d/viewer.html")
            open(os.path.join(OUT, "wiring", "interactive_inner.html"), "w", encoding="utf-8").write(t)
            continue
        shutil.copy(os.path.join(wdir, f), os.path.join(OUT, "wiring", "diagrams.html" if f == "index.html" else f))
    open(os.path.join(OUT, "3d", "index.html"), "w", encoding="utf-8").write(viewer_page("3D viewer", "3d/index.html", "viewer.html"))
    open(os.path.join(OUT, "wiring", "index.html"), "w", encoding="utf-8").write(viewer_page("Wiring diagrams", "wiring/index.html", "diagrams.html"))
    open(os.path.join(OUT, "wiring", "interactive.html"), "w", encoding="utf-8").write(viewer_page("Interactive wiring", "wiring/interactive.html", "interactive_inner.html"))
    open(os.path.join(OUT, "CNAME"), "w").write(DOMAIN + "\n")
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
