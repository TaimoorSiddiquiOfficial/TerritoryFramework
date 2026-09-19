# Wave 4 part A TEST: the doc must be complete AND contain nothing the C++ does not have.
# Usage: python _tmp_verify_options_doc.py [path-to-doc]   (defaults to the real doc)
import json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # plugin root; this file is in Tools/
DOC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Docs", "37_Every_Option_Reference.md")
rows = json.load(open(os.path.join(ROOT, "Tools", "options_inventory.json"), encoding="utf-8"))

inv = {(r["class"], r["name"]): r for r in rows if r["edit_spec"]}

CLS = re.compile(r"^### `(?P<cls>[^`]+)`")
ROW = re.compile(r"^\| `(?P<name>[^`]+)` \| (?P<rest>.*)$")

doc = {}
cls = None
for i, line in enumerate(open(DOC, encoding="utf-8"), 1):
    m = CLS.match(line)
    if m:
        cls = m.group("cls")
        continue
    m = ROW.match(line)
    if m and cls:
        doc[(cls, m.group("name"))] = (i, m.group("rest"))

fails = []

print(f"inventory EditAnywhere options : {len(inv)}")
print(f"rows found in doc              : {len(doc)}")

missing = sorted(set(inv) - set(doc))
extra = sorted(set(doc) - set(inv))
if missing:
    fails.append(f"{len(missing)} option(s) in C++ are MISSING from the doc")
    for k in missing[:10]:
        print("   MISSING:", k)
if extra:
    fails.append(f"{len(extra)} row(s) in the doc are NOT in the C++ inventory (invented or misparsed)")
    for k in extra[:10]:
        print("   EXTRA:", k)

# Verbatim check: the description cell must contain the author's tooltip exactly, or the
# literal "no description in source" marker. No paraphrase may creep into a generated file.
bad_text = []
for (c, n), (line, rest) in doc.items():
    if (c, n) not in inv:
        continue
    tooltip = inv[(c, n)]["tooltip"]
    cells = rest.split(" | ")
    desc = cells[3] if len(cells) > 3 else ""
    if tooltip:
        want = tooltip.replace("|", "\\|").replace("\n", " ").strip()
        if want not in desc:
            bad_text.append((c, n, line, want[:60], desc[:60]))
    else:
        if "no description in source" not in desc:
            bad_text.append((c, n, line, "(expected the 'no description in source' marker)", desc[:60]))
if bad_text:
    fails.append(f"{len(bad_text)} row(s) have description text that is NOT verbatim from C++")
    for b in bad_text[:8]:
        print("   TEXT:", b)

# Every option that HAS a written default must show it, so the doc cannot silently drop defaults.
bad_def = []
for (c, n), (line, rest) in doc.items():
    if (c, n) not in inv:
        continue
    d = inv[(c, n)]["default"]
    cells = rest.split(" | ")
    cell = cells[2] if len(cells) > 2 else ""
    got = cell.strip("`").strip()
    if d and got != d.replace("|", "\\|").replace("\n", " ").strip():
        bad_def.append((c, n, line, d, got))
    if not d and got != "—":
        bad_def.append((c, n, line, "(none)", got))
if bad_def:
    fails.append(f"{len(bad_def)} row(s) show a default that does not match the C++ initialiser")
    for b in bad_def[:8]:
        print("   DEFAULT:", b)

print()
if fails:
    print("RESULT: FAIL")
    for f in fails:
        print("  -", f)
    sys.exit(1)
print("RESULT: PASS - doc is complete, has no invented rows, and every description/default is verbatim")
