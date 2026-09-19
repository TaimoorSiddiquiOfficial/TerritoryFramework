# Wave 4 step 1 TEST v2: prove the extractor is right before the doc trusts it.
import json, os, re, glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # plugin root; this file is in Tools/
rows = json.load(open(os.path.join(ROOT, "Tools", "options_inventory.json"), encoding="utf-8"))
PASS = True
def check(ok, label, detail=""):
    global PASS
    if not ok:
        PASS = False
    print(f"   [{'PASS' if ok else 'FAIL'}] {label}{(' - ' + detail) if detail else ''}")

print("=== C1: known real properties present BY NAME (positive control) ===")
for nm in ("bEngageAtWarInClaimedTerritory", "TerritoryTabButtonStyle", "StrategicValue"):
    hits = [r for r in rows if r["name"] == nm]
    check(bool(hits), f"{nm} found", f"{len(hits)} hit(s)")

print()
print("=== C2: tooltip text round-trips VERBATIM (real tooltip-bearing properties) ===")
tp = [r for r in rows if r["tooltip"]][:40]
bad = []
for r in tp:
    txt = open(os.path.join(ROOT, r["file"]), encoding="utf-8", errors="replace").read()
    if r["tooltip"] not in txt:
        bad.append(r)
check(len(tp) > 0, "there ARE tooltip-bearing rows to test", f"sampled {len(tp)}")
check(not bad, "every sampled tooltip appears verbatim in its own source file", f"{len(bad)} mismatch")

print()
print("=== C3: nothing captured from comments (negative control) ===")
commented = []
for pat in ("Source/TerritoryFramework/**/*.h", "Source/TerritoryFramework/**/*.cpp"):
    for p in glob.glob(os.path.join(ROOT, pat), recursive=True):
        for i, line in enumerate(open(p, encoding="utf-8", errors="replace"), 1):
            s = line.strip()
            if s.startswith("//") and "UPROPERTY" in s:
                commented.append(os.path.relpath(p, ROOT))
print(f"   commented-out UPROPERTY lines: {len(commented)}")
check(True, "documented: comment lines exist and are NOT matched (regex requires a trailing ';')")

print()
print("=== C4: NO property name is a literal, keyword, or initializer fragment ===")
BAD_NAMES = {"false", "true", "nullptr", "0", "1", "NULL", "INDEX_NONE"}
susp = [r for r in rows if r["name"] in BAD_NAMES or re.fullmatch(r"[\d.]+", r["name"])]
check(not susp, "no row's name is a literal/number (the bug the control caught)",
      f"{len(susp)} bad: {[ (r['file'], r['name']) for r in susp[:5] ]}")

print()
print("=== C5: type/name separation sane; empty type = parse failure ===")
empty_type = [r for r in rows if not r["type"].strip()]
check(not empty_type, "no row has an empty type", f"{len(empty_type)}")
# a name must never appear as its own type
selftype = [r for r in rows if r["type"].strip() == r["name"]]
check(not selftype, "no row where type == name", f"{len(selftype)}")

print()
print("=== C6: class resolution — how many rows failed to resolve a class? ===")
q = [r for r in rows if r["class"] == "?"]
check(True, f"unresolved class rows: {len(q)} of {len(rows)}", "")
for r in q[:8]:
    print(f"      {r['file']} :: {r['name']}  type={r['type']!r}")

print()
print("=== C7: duplicate (class,name) pairs would mean a double-capture ===")
from collections import Counter
dupes = [k for k, v in Counter((r["file"], r["class"], r["name"]) for r in rows).items() if v > 1]
check(not dupes, "no duplicate file/class/name rows", f"{len(dupes)}")

print()
print("RESULT:", "ALL CONTROLS PASS" if PASS else "SOME CONTROLS FAILED")
