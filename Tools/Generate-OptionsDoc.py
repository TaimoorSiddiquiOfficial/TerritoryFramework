# Wave 4 part A: generate the complete per-option reference from the C++ inventory.
# Deterministic: same inventory in -> byte-identical doc out, so the doc can be diffed and tested.
import json, os, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # plugin root; this file is in Tools/
OUT = os.path.join(ROOT, "Docs", "37_Every_Option_Reference.md")
rows = json.load(open(os.path.join(ROOT, "Tools", "options_inventory.json"), encoding="utf-8"))

# Only the dev-facing surface: EditAnywhere (what a designer can set in the editor).
opts = [r for r in rows if r["edit_spec"]]

AREA_NAMES = {
    "Core": "Core — definitions, districts, places, guards",
    "Combat": "Combat — assaults, counterattacks, targeting",
    "Economy": "Economy — production, resources, currency",
    "AI": "AI — NPC behaviour, diplomacy, perception",
    "UI": "UI — Command Center, HUD, journal, theme hooks",
    "Tales": "Tales — narrative tasks, quest cascades, story events",
    "Cinematics": "Cinematics — presentation and shot settings",
    "Subsystems": "Subsystems — world-level managers",
    "Stealth": "Stealth",
    "Map": "Map, roads and navigation",
    "Save": "Save and load",
    "Debug": "Debug and diagnostics",
}

def area_of(r):
    parts = r["file"].split("/")
    # Source/TerritoryFramework/{Public|Private}/<Area>/...
    if len(parts) > 3:
        return parts[3]
    return "Other"

def kind_of(cls):
    if cls.startswith("U") and cls.endswith("Settings"):
        return "settings class"
    if cls.startswith("U") and cls.endswith("Profile"):
        return "profile DataAsset"
    if cls.startswith("U") and cls.endswith("Definition"):
        return "definition DataAsset"
    if cls.startswith("U") and cls.endswith("Component"):
        return "component"
    if cls.startswith("U") and cls.endswith("Task"):
        return "AI / narrative task"
    if cls.startswith("U") and cls.endswith("Event"):
        return "narrative event"
    if cls.startswith("U") and cls.endswith("Subsystem"):
        return "subsystem"
    if cls.startswith("A"):
        return "actor"
    if cls.startswith("F"):
        return "struct"
    if cls.startswith("E"):
        return "enum"
    return "class"

by_area = collections.defaultdict(lambda: collections.defaultdict(list))
for r in opts:
    by_area[area_of(r)][r["class"]].append(r)

def esc(s):
    return (s.replace("|", "\\|").replace("\n", " ").strip())

L = []
w = L.append
w("# 37 — Every Option Reference")
w("")
w("**Complete, generated reference for every option a designer can set from the editor.**")
w("")
w("This file is **generated from the plugin's C++**, not written by hand. Every option that carries")
w("`EditAnywhere` in `Source/TerritoryFramework` appears here, with its real name, its real type, the")
w("default the code actually initialises it to, and the description the author wrote into `meta`.")
w("Nothing in the tables is invented — if a description is missing, the table shows it as missing")
w("rather than filling the gap with a guess.")
w("")
w("| | |")
w("|---|---|")
n_areas = len(by_area)
n_classes = len({r["class"] for r in opts})
w(f"| Options documented | **{len(opts)}** |")
w(f"| Classes / structs covered | **{n_classes}** |")
w(f"| Source areas | **{n_areas}** |")
w(f"| Options whose author wrote a description | **{sum(1 for r in opts if r['tooltip'])}** |")
w(f"| Options with no description in source | **{sum(1 for r in opts if not r['tooltip'])}** |")
w(f"| Options with a default written in code | **{sum(1 for r in opts if r['default'])}** |")
w("")
w("**What this file deliberately does not cover.** It lists options, not behaviour. For *what the")
w("system does* and *why*, read the plain-English guide that pairs with this one, and the numbered")
w("system docs (00–36). This file answers one question well: *\"this option in the Details panel —")
w("what is it, what type is it, and what does it start as?\"*")
w("")
w("**Scope note.** Only `Source/TerritoryFramework` is scanned. The separate")
w("`Source/TerritoryFrameworkEditor` module is editor-only tooling and its options are not shipped to")
w("a game build, so they are out of scope here.")
w("")
w("---")
w("")
w("## How to read a row")
w("")
w("| Column | Meaning |")
w("|---|---|")
w("| **Option** | The C++ property name. This is what appears in the Details panel unless a display name is set. |")
w("| **Shown as** | The `DisplayName` the author set. Blank means the panel shows the C++ name. |")
w("| **Type** | The declared type, exactly as written in the header. |")
w("| **Default** | The value the code initialises it to. `—` means no initialiser is written, so the type's zero/empty value applies (0, false, empty array, null pointer). |")
w("| **What it does** | The author's own `ToolTip` text, verbatim. **“no description in source”** is printed when none was written — that is a real gap in the plugin, shown honestly rather than guessed. |")
w("| **Rules** | Numeric clamps and `EditCondition` gates, as declared. |")
w("")
w("---")
w("")
w("## Index")
w("")
for area in sorted(by_area, key=lambda a: (a == "Other", a)):
    classes = by_area[area]
    total = sum(len(v) for v in classes.values())
    w(f"**{AREA_NAMES.get(area, area)}** — {total} option(s) in {len(classes)} class(es)")
    w("")
    for cls in sorted(classes):
        w(f"- `{cls}` — {len(classes[cls])}")
    w("")
w("---")
w("")

for area in sorted(by_area, key=lambda a: (a == "Other", a)):
    w(f"## {AREA_NAMES.get(area, area)}")
    w("")
    for cls in sorted(by_area[area]):
        lst = by_area[area][cls]
        f = lst[0]["file"]
        w(f"### `{cls}`")
        w("")
        w(f"*{kind_of(cls)}* · `{f}` · {len(lst)} option(s)")
        w("")
        w("| Option | Shown as | Type | Default | What it does | Rules |")
        w("|---|---|---|---|---|---|")
        for r in sorted(lst, key=lambda x: x["name"]):
            desc = esc(r["tooltip"]) if r["tooltip"] else "*no description in source*"
            rules = []
            for k in ("ClampMin", "ClampMax", "UIMin", "UIMax", "EditCondition",
                      "EditConditionHides", "Categories"):
                if k in r["meta_keys"]:
                    m = r["spec"]
                    rules.append(f"`{k}`")
            shown = esc(r["display"]) if r["display"] else "—"
            default = esc(r["default"]) if r["default"] else "—"
            w(f"| `{esc(r['name'])}` | {shown} | `{esc(r['type'])}` | `{default}` | {desc} | {' '.join(rules) if rules else '—'} |")
        w("")

body = "\n".join(L).rstrip() + "\n"
open(OUT, "w", encoding="utf-8", newline="\n").write(body)

print("WROTE:", OUT)
print("OPTIONS_IN_DOC:", len(opts))
print("CLASSES_IN_DOC:", n_classes)
print("BYTES:", len(body))
print("LINES:", body.count("\n"))
