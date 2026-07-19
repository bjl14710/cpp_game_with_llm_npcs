---
description: Build or refresh docs/learning/index.html — a home page linking every lesson, drill, and vocabulary drill, binding each trio by slug when they match.
argument-hint: Optional title for the home page
---

Regenerate the learning home page from whatever exists in docs/learning/.
This is deterministic file-plumbing — write the generator below and run it.

## What the index does

Scans three folders:
- docs/learning/lessons/*.html   (lessons from /teach)
- docs/learning/drills/*.html    (concept drills from /drill-me)
- docs/learning/vocab/*.html     (vocabulary drills from /terms)

The slug is the filename with the leading NNNN- stripped
(0003-scpi-parsing.html -> scpi-parsing). Files across all three folders
with the same slug are treated as a trio.

Lessons section (one row per lesson, numeric order):
- Lesson title and link on the left
- Concept drill link (drill ▸) in the middle; "no drill yet" if absent
- Vocabulary drill link (vocab ▸) on the right; "no vocab yet" if absent
- A completeness badge: "complete trio" / "pair" / "lesson only"

Standalone drills section: drills with no matching lesson.
Standalone vocab section: vocab files with no matching lesson.

Safe to re-run after every /teach, /drill-me, or /terms.

## Steps

1. Check docs/learning/ exists with at least one html file across the three
   folders. If nothing exists, tell the user and stop.

2. Write the generator to docs/learning/.tools/build_index.py exactly as
   in the Generator Source block below (overwrite to keep it current).

3. Run it:
   python3 docs/learning/.tools/build_index.py docs/learning
   or with a title:
   python3 docs/learning/.tools/build_index.py docs/learning --title "$1"

4. Print the open command for the user's platform:
   macOS:  open docs/learning/index.html
   Linux:  xdg-open docs/learning/index.html

5. Confirm what was indexed (e.g. "5 lessons, 3 drills, 2 vocab files --
   2 complete trios, 2 lesson+drill pairs, 1 lesson only").

## Generator Source

Write this exactly to docs/learning/.tools/build_index.py:

--- BEGIN build_index.py ---
import os, re, sys, argparse
from pathlib import Path
from html import escape

def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("learning_dir")
    p.add_argument("--title", default="")
    return p.parse_args()

def slug(filename):
    return re.sub(r"^\d+-", "", Path(filename).stem)

def extract_title(html_path):
    try:
        text = Path(html_path).read_text(encoding="utf-8", errors="replace")
        for pattern in [r"<h1[^>]*>(.*?)</h1>", r"<title>(.*?)</title>"]:
            m = re.search(pattern, text, re.IGNORECASE | re.DOTALL)
            if m:
                return re.sub(r"<[^>]+>", "", m.group(1)).strip()
    except Exception:
        pass
    return slug(html_path)

def scan(folder):
    result = {}
    if not folder.is_dir():
        return result
    for f in sorted(folder.glob("*.html")):
        s = slug(f.name)
        result[s] = (f.name, extract_title(f))
    return result

def badge(text, cls):
    return '<span class="badge ' + cls + '">' + escape(text) + '</span>'

def build(learning_dir, title):
    base = Path(learning_dir)
    lessons = scan(base / "lessons")
    drills  = scan(base / "drills")
    vocab   = scan(base / "vocab")

    if not title:
        prog = base / "PROGRESS.md"
        if prog.exists():
            m = re.search(r"^#\s+(.+)", prog.read_text(), re.MULTILINE)
            if m:
                title = m.group(1).strip()
    if not title:
        title = "Learning Pack"

    all_slugs_set = set(list(lessons) + list(drills) + list(vocab))

    def sort_key(s):
        for d in [lessons, drills, vocab]:
            if s in d:
                fn = d[s][0]
                m = re.match(r"(\d+)-", fn)
                if m:
                    return int(m.group(1))
        return 9999

    all_slugs = sorted(all_slugs_set, key=sort_key)

    lesson_rows   = []
    orphan_drills = []
    orphan_vocab  = []

    for s in all_slugs:
        if s in lessons:
            lesson_rows.append(s)
        else:
            if s in drills: orphan_drills.append(s)
            if s in vocab:  orphan_vocab.append(s)

    def row_html(s):
        fn, t = lessons[s]
        lesson_link = '<a href="lessons/' + escape(fn) + '">' + escape(t) + '</a>'
        if s in drills:
            dfn, _ = drills[s]
            drill_cell = '<a href="drills/' + escape(dfn) + '" class="link-drill">drill &#9658;</a>'
        else:
            drill_cell = '<span class="dim">no drill yet</span>'
        if s in vocab:
            vfn, _ = vocab[s]
            vocab_cell = '<a href="vocab/' + escape(vfn) + '" class="link-vocab">vocab &#9658;</a>'
        else:
            vocab_cell = '<span class="dim">no vocab yet</span>'
        count = sum([s in drills, s in vocab])
        if count == 2:  b = badge("complete trio", "trio")
        elif count == 1: b = badge("pair", "pair")
        else:            b = badge("lesson only", "solo")
        return ('<tr><td class="lesson-col">' + lesson_link + ' ' + b + '</td>'
                '<td class="drill-col">' + drill_cell + '</td>'
                '<td class="vocab-col">' + vocab_cell + '</td></tr>')

    lessons_html = "\n".join(row_html(s) for s in lesson_rows)

    def orphan_section(items, folder, cls, label):
        if not items: return ""
        src = drills if folder == "drills" else vocab
        rows = ["<li><a href='" + folder + "/" + escape(src[s][0]) + "' class='link-" + cls + "'>" + escape(src[s][1]) + "</a></li>" for s in items]
        return '<section class="orphan"><h2>' + label + '</h2><ul>' + "".join(rows) + '</ul></section>'

    orphan_drills_html = orphan_section(orphan_drills, "drills", "drill", "Standalone Concept Drills")
    orphan_vocab_html  = orphan_section(orphan_vocab,  "vocab",  "vocab", "Standalone Vocabulary Drills")

    total_l = len(lessons)
    total_d = len(drills)
    total_v = len(vocab)
    trios = sum(1 for s in lesson_rows if s in drills and s in vocab)
    pairs = sum(1 for s in lesson_rows if (s in drills) != (s in vocab))
    solo  = sum(1 for s in lesson_rows if s not in drills and s not in vocab)
    stats = (str(total_l) + " lesson" + ("s" if total_l != 1 else "") + ", " +
             str(total_d) + " drill" + ("s" if total_d != 1 else "") + ", " +
             str(total_v) + " vocab file" + ("s" if total_v != 1 else "") + " -- " +
             str(trios) + " complete trio" + ("s" if trios != 1 else "") + ", " +
             str(pairs) + " pair" + ("s" if pairs != 1 else "") + ", " +
             str(solo) + " lesson-only")

    css = """
  :root {
    --bg:#0f1117;--surface:#1a1d2e;--border:#2a2d3e;--text:#e2e8f0;--dim:#64748b;
    --lesson:#60a5fa;--drill:#a78bfa;--vocab:#34d399;
    --trio-bg:#14532d;--trio-fg:#86efac;
    --pair-bg:#1e3a5f;--pair-fg:#93c5fd;
    --solo-bg:#2d2d2d;--solo-fg:#94a3b8;
  }
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);font-family:system-ui,sans-serif;
       line-height:1.6;padding:2rem;max-width:900px;margin:0 auto}
  header{margin-bottom:2rem;border-bottom:1px solid var(--border);padding-bottom:1rem}
  h1{font-family:monospace;font-size:1.6rem;color:var(--lesson);margin-bottom:.4rem}
  .stats{color:var(--dim);font-size:.85rem}
  h2{font-family:monospace;font-size:.9rem;color:var(--dim);text-transform:uppercase;
     letter-spacing:.08em;margin:2rem 0 .75rem}
  table{width:100%;border-collapse:collapse}
  th{text-align:left;font-family:monospace;font-size:.75rem;color:var(--dim);
     text-transform:uppercase;letter-spacing:.06em;padding:.5rem .75rem;
     border-bottom:1px solid var(--border)}
  td{padding:.6rem .75rem;border-bottom:1px solid var(--border);vertical-align:middle}
  tr:last-child td{border-bottom:none}
  tr:hover td{background:var(--surface)}
  .lesson-col{width:55%}.drill-col,.vocab-col{width:22.5%}
  a{text-decoration:none}a:hover{text-decoration:underline}
  .lesson-col a{color:var(--lesson)}
  .link-drill{color:var(--drill)}.link-vocab{color:var(--vocab)}
  .dim{color:var(--dim);font-size:.85rem}
  .badge{font-size:.7rem;padding:.15em .5em;border-radius:3px;
         font-family:monospace;vertical-align:middle;margin-left:.4em}
  .trio{background:var(--trio-bg);color:var(--trio-fg)}
  .pair{background:var(--pair-bg);color:var(--pair-fg)}
  .solo{background:var(--solo-bg);color:var(--solo-fg)}
  .legend{display:flex;gap:1.5rem;font-size:.8rem;color:var(--dim);margin-bottom:1rem}
  .legend span{display:flex;align-items:center;gap:.4em}
  .dot{width:8px;height:8px;border-radius:50%;display:inline-block}
  .dot-l{background:var(--lesson)}.dot-d{background:var(--drill)}.dot-v{background:var(--vocab)}
  .orphan{margin-top:2rem}
  .orphan ul{list-style:none;padding:0}
  .orphan li{padding:.45rem 0;border-bottom:1px solid var(--border)}
  .orphan li:last-child{border-bottom:none}"""

    html = (
        "<!DOCTYPE html>\n<html lang='en'>\n<head>\n"
        "<meta charset='UTF-8'>\n"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>\n"
        "<title>" + escape(title) + "</title>\n"
        "<style>" + css + "\n</style>\n</head>\n<body>\n"
        "<header>\n<h1>" + escape(title) + "</h1>\n"
        "<p class='stats'>" + escape(stats) + "</p>\n</header>\n\n"
        "<section>\n<h2>Lessons</h2>\n"
        "<div class='legend'>"
        "<span><span class='dot dot-l'></span>Lesson</span>"
        "<span><span class='dot dot-d'></span>Concept drill</span>"
        "<span><span class='dot dot-v'></span>Vocabulary drill</span>"
        "</div>\n"
        "<table>\n<thead>\n<tr>"
        "<th>Lesson</th><th>Concept drill</th><th>Vocabulary drill</th>"
        "</tr>\n</thead>\n<tbody>\n"
        + lessons_html + "\n</tbody>\n</table>\n</section>\n\n"
        + orphan_drills_html + "\n" + orphan_vocab_html + "\n"
        "</body>\n</html>"
    )

    out = base / "index.html"
    out.write_text(html, encoding="utf-8")
    print("Written:", out, " (", stats, ")")

if __name__ == "__main__":
    args = parse_args()
    build(args.learning_dir, args.title)
--- END build_index.py ---

## Sharing the pack
The index uses relative links. Bundle cleanly with:
  zip -r learning-pack.zip docs/learning -x '*/.tools/*' '*PROGRESS.md' '*GLOSSARY.md'
Recipients unzip and open index.html -- no server, no internet, no setup.
