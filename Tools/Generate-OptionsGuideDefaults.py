# Wave 4 part B: append the Defaults appendix to Docs/38 from the C++ inventory.
#
# The prose in file 38 is hand-written, so any default it states is a memory. This generator
# makes the stated defaults mechanical instead: it walks the guide's own bullets and prints the
# real initialiser for each one. Idempotent - re-running replaces the appendix rather than
# appending a second copy, so the file can be regenerated after every section added.
import json, os, re, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOC = os.path.join(ROOT, "Docs", "38_Options_Guide.md")
rows = json.load(open(os.path.join(ROOT, "Tools", "options_inventory.json"), encoding="utf-8"))

EDIT = {(r["class"], r["name"]): r for r in rows if r["edit_spec"]}

MARK = "## Appendix — the defaults used in this file"
# Class sections are `##`-level; their `###` sub-headings carry no backticks, which is what
# tells the two apart. Keep these in step with Verify-OptionsGuide.py - and note that an
# earlier version of this file was NOT in step: it captured only the FIRST name on a bullet,
# so every option listed after the first in `- `A`, `B` and `C`` was silently missing from the
# appendix. Its own comment claimed otherwise. 19 options were absent before the verifier's C6
# check was added to notice.
HEAD = re.compile(r"^#{2,4} `(?P<cls>[^`]+)`")
BULLET = re.compile(r"^- ((?:`[A-Za-z_]\w*`(?:, | / | and )?)+)")
NAME = re.compile(r"`([A-Za-z_]\w*)`")

text = open(DOC, encoding="utf-8").read()
text = text.split(MARK)[0].rstrip() + "\n"

pairs, cls = [], None
for line in text.split("\n"):
    m = HEAD.match(line)
    if m:
        cls = m.group("cls")
        continue
    m = BULLET.match(line)
    if m and cls:
        for name in NAME.findall(m.group(1)):
            pairs.append((cls, name))

# preserve first-seen order, drop repeats (an option may be mentioned in two sections)
seen, ordered = set(), []
for p in pairs:
    if p not in seen:
        seen.add(p)
        ordered.append(p)

by_class = collections.OrderedDict()
for c, n in ordered:
    by_class.setdefault(c, []).append(n)

def esc(s):
    return s.replace("|", "\\|").replace("\n", " ").strip()

missing = [p for p in ordered if p not in EDIT]

L = []
w = L.append
w(MARK)
w("")
w("**Generated from the C++ inventory, not typed by hand.** A test fails if any value here disagrees")
w("with the initialiser in the source, so this table cannot drift. `—` means the author wrote no")
w("initialiser, so the type's zero or empty value applies (0, false, empty array, null pointer).")
w("")
w("| Class | Option | Default in C++ |")
w("|---|---|---|")
for c, names in by_class.items():
    for n in names:
        r = EDIT.get((c, n))
        d = esc(r["default"]) if r and r["default"] else "—"
        w(f"| `{c}` | `{n}` | `{d}` |")
w("")

body = (text + "\n" + "\n".join(L)).rstrip() + "\n"
open(DOC, "w", encoding="utf-8", newline="\n").write(body)

print("SECTIONS:", len(by_class))
print("OPTIONS_IN_APPENDIX:", len(ordered))
print("NOT_IN_INVENTORY:", len(missing))
for p in missing:
    print("   MISSING:", p)
print("BYTES:", len(body))
