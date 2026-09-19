# Wave 4 part B TEST: the plain-English guide must not invent an option, a class, or a default.
#
# Why this exists: file 37 is generated, so it cannot lie about an option name or a default.
# File 38 is HAND-WRITTEN prose, so it can - a typo'd option name reads perfectly well and a
# remembered default is indistinguishable from a checked one. This test makes the hand-written
# file answerable to the same C++ inventory the generated one is built from.
#
# What it checks:
#   C1  every "### `Class`" section heading names a class that really has designer options
#   C2  every "- `Option` ..." bullet resolves inside its own section's class
#   C3  the Defaults table agrees with the C++ initialiser, exactly
#   C4  positive control: the options the reported bug turns on ARE discussed
#   C5  no backticked identifier is unknown (catches a typo'd option name anywhere in the prose)
#
# Known limitations, stated rather than hidden:
#   * C2 only checks BULLETS. An option named in ordinary prose or in a table cell is not checked
#     for which class declares it - only for existing at all. C5 covers that half.
#   * C5 matches a backticked run only when the whole run is ONE identifier, so a multi-token
#     reference like `Class::Method` is not checked. Prefer single-identifier backticks in prose.
#   * C6 requires the appendix to contain every taught option, but not the reverse: an option in
#     the appendix that the prose never explains is not a failure, because the appendix is built
#     from the prose in the first place.
# C5 is what covers inline mentions - any backticked token must be a real name from C++ or an
# entry in EXTRA below, so an invented name fails wherever it appears. EXTRA is deliberately
# short and explicit: adding to it is a decision that must be justified from the source, not an
# accident.
import json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # plugin root; this file is in Tools/
DOC = os.path.join(ROOT, "Docs", "38_Options_Guide.md")
rows = json.load(open(os.path.join(ROOT, "Tools", "options_inventory.json"), encoding="utf-8"))

EDIT = {(r["class"], r["name"]): r for r in rows if r["edit_spec"]}
CLASSES_WITH_OPTIONS = {r["class"] for r in rows if r["edit_spec"]}
ALL_PROPS = {r["name"] for r in rows}
ALL_CLASSES = {r["class"] for r in rows}

# Every identifier that appears in any declared type, so `bool`, `TArray`, `FGameplayTag`,
# `TObjectPtr`, `UNPCDefinition` and friends are known without hand-listing them.
TYPE_WORDS = set()
for r in rows:
    TYPE_WORDS |= set(re.findall(r"[A-Za-z_]\w*", r["type"]))

# Identifiers that appear only in code this guide quotes, with the reason each is allowed.
EXTRA = {
    # Code quoted from the plugin, not an option name.
    "BuildDefenceFront", "EvaluateTerritoryTarget", "OutReason", "Result",
    "ShouldActivateWaitingAssault", "bProximityAllowsActivation", "bProximityRequired",
    "bRelevantPlayerNearby", "bGarrisonHoldsDefence", "CollectRegisteredDefenders",
    "ReplicatedTreaties", "AccumulatedTime", "GetLifetimeReplicatedProps",
    "DOREPLIFETIME", "GetFactionAttitudeTowardsFaction", "RestorePersistentState",
    # A Blueprint lookup function, not an option: named because it is what reads TerritoryTag.
    "GetTerritoryByTag",
    # The diplomacy gate on UTerritoryControlSubsystem, named because the guide's point is that all
    # 8 of its call sites are server-side. Verified: it is declared at TerritoryControlSubsystem.cpp
    # and called 8 times, all server-side.
    "CanFactionCaptureTerritory",
    # Types and engines named in prose.
    "UPROPERTY", "UCLASS", "EditAnywhere", "uint8", "UObject",
}

KNOWN = ALL_PROPS | ALL_CLASSES | TYPE_WORDS | EXTRA

if not os.path.exists(DOC):
    print(f"RESULT: FAIL\n  - guide does not exist yet: {DOC}")
    sys.exit(1)

# Class sections are `##`-level in this guide; the `###` sub-headings inside them (e.g.
# "### Guarding") carry no backticks, so requiring backticks is what separates the two.
HEAD = re.compile(r"^#{2,4} `(?P<cls>[^`]+)`")
# No trailing \b: the character just consumed is a backtick, which is not a word character,
# so a boundary there cannot exist and the whole pattern would never match. Found by running
# this control, not by reading it.
#
# The leading run captures EVERY name in a multi-option bullet (`- `A`, `B` and `C` — ...`).
# An earlier version took only the first, so a bullet naming three options had two of them
# unchecked - which is exactly how a wrong class slipped through.
BULLET = re.compile(r"^- ((?:`[A-Za-z_]\w*`(?:, | / | and )?)+)")
NAME = re.compile(r"`([A-Za-z_]\w*)`")
TOKEN = re.compile(r"`([A-Za-z_]\w*)`")
# | `Class` | `Option` | `Default` |
DROW = re.compile(r"^\| `(?P<cls>[^`]+)` \| `(?P<name>[^`]+)` \| `(?P<def>[^`]*)` \|")

text = open(DOC, encoding="utf-8").read()
lines = text.split("\n")

fails = []

print("=== C1: section headings name real classes ===")
cls = None
seen_classes = []
for line in lines:
    m = HEAD.match(line)
    if m:
        cls = m.group("cls")
        seen_classes.append(cls)
        if cls not in CLASSES_WITH_OPTIONS:
            fails.append(f"C1: heading names `{cls}` which has no designer options in C++")
print(f"   sections: {len(seen_classes)}")
print(f"   distinct classes: {len(set(seen_classes))}")
if not seen_classes:
    fails.append("C1: no `### `Class`` section headings found - the guide has no per-class sections")

print()
print("=== C2: option bullets resolve inside their own section ===")
# Scope rule, and the reason C8 exists: a class section is a `##`/`###` heading that names a
# class in backticks. A `##` heading WITHOUT backticks (e.g. "## Economy") is a concept section
# and scopes nothing - so it RESETS the scope to None rather than leaving the previous class in
# force. Without the reset, a bullet under "## Economy" would be silently validated against
# whatever class happened to precede it, which is a false pass, not a check. C8 then fails on
# any bullet written in that unscoped state, so the reset cannot quietly hide one.
cls = None
bullets = 0
unscoped = []
mentioned = set()          # (class, option) pairs the prose actually teaches
for i, line in enumerate(lines, 1):
    if line.startswith("## ") and not HEAD.match(line):
        cls = None
        continue
    m = HEAD.match(line)
    if m:
        cls = m.group("cls")
        continue
    m = BULLET.match(line)
    if m:
        if cls is None:
            unscoped.append((i, line.strip()[:70]))
            continue
        for name in NAME.findall(m.group(1)):
            bullets += 1
            if (cls, name) not in EDIT:
                near = sorted({c for (c, n) in EDIT if n == name})
                hint = f" (it is a real option, but on `{near[0]}`)" if near else ""
                fails.append(f"C2 line {i}: `{cls}::{name}` is not a designer option in C++{hint}")
            else:
                mentioned.add((cls, name))
print(f"   bullets checked: {bullets}")
if bullets == 0:
    fails.append("C2: no '- `Option`' bullets found - nothing was actually verified")

print()
print("=== C8: no option bullet sits outside a class section ===")
for i, s in unscoped[:10]:
    print(f"      line {i}: {s}")
print(f"   unscoped bullets: {len(unscoped)}")
if unscoped:
    fails.append(f"C8: {len(unscoped)} option bullet(s) are not inside a `Class` section, so "
                 f"nothing checked which class declares them")

print()
print("=== C3: the Defaults table matches the C++ initialiser exactly ===")
drows = 0
documented = set()
for i, line in enumerate(lines, 1):
    m = DROW.match(line)
    if not m:
        continue
    drows += 1
    c, n, d = m.group("cls"), m.group("name"), m.group("def").strip()
    if (c, n) not in EDIT:
        fails.append(f"C3 line {i}: defaults row for `{c}::{n}` - no such designer option")
        continue
    documented.add((c, n))
    real = EDIT[(c, n)]["default"].strip()
    real = "—" if not real else real
    if d != real:
        fails.append(f"C3 line {i}: `{c}::{n}` guide says `{d}`, C++ initialises `{real}`")
print(f"   defaults rows checked: {drows}")
if drows == 0:
    fails.append("C3: no defaults table found - stated defaults are unverified")

# C3 checks that the rows present are RIGHT; C6 checks that the rows present are ALL of them.
# The two are independent: an appendix that silently omits options passes C3 perfectly.
print()
print("=== C6: every option the prose teaches has a row in the Defaults appendix ===")
absent = sorted(mentioned - documented)
print(f"   options taught in prose: {len(mentioned)}")
print(f"   options in the appendix: {len(documented)}")
print(f"   taught but NOT in the appendix: {len(absent)}")
for c, n in absent[:10]:
    print(f"      `{c}::{n}`")
if absent:
    fails.append(f"C6: {len(absent)} option(s) are explained in the prose but missing from the "
                 f"generated Defaults appendix (the appendix is incomplete)")

print()
print("=== C4: positive control - the reported bug's options ARE discussed ===")
for c, n in (("UTerritoryCounterAttackProfile", "bGarrisonTriggersActivation"),
             ("UTerritoryCounterAttackProfile", "bRequirePlayerProximityForActivation"),
             ("FTerritoryGuardBehaviorTemplate", "bEngageAtWarInClaimedTerritory"),
             ("FTerritoryGuardPostTemplate", "FactionOverride")):
    found = (bool(re.search(rf"^- `{n}`", text, re.M))
             and bool(re.search(rf"^#{{2,4}} `{re.escape(c)}`", text, re.M)))
    print(f"   [{'PASS' if found else 'FAIL'}] `{c}::{n}` explained in its own section")
    if not found:
        fails.append(f"C4: `{c}::{n}` is not explained - it is load-bearing for the reported bug")

print()
print("=== C5: every backticked identifier is a real name from C++ ===")
unknown = {}
for i, line in enumerate(lines, 1):
    # The Defaults appendix is the ONE place a literal is meant to be backticked (`false`,
    # `nullptr`), because its third cell is the raw C++ initialiser. C3 checks those cells
    # against the inventory exactly - a stronger check than "is this a known name" - so
    # skipping them here is not a hole in C5's coverage. Without this, every table row
    # default reads as an invented identifier.
    if DROW.match(line):
        continue
    for tok in TOKEN.findall(line):
        if len(tok) < 3 or tok in KNOWN:
            continue
        unknown.setdefault(tok, i)
print(f"   distinct backticked identifiers: {len(set(TOKEN.findall(text)))}")
print(f"   unknown: {len(unknown)}")
for tok, i in sorted(unknown.items(), key=lambda kv: kv[1])[:15]:
    print(f"      line {i}: `{tok}`")
if unknown:
    fails.append(f"C5: {len(unknown)} backticked identifier(s) are not in the inventory or EXTRA: "
                 + ", ".join(sorted(unknown)[:8]))

print()
print("=== C7: every in-document link points at a heading that exists ===")
# GitHub's slug rule, stated so this check is auditable rather than magic: lowercase, delete
# every character that is not a word character, a space or a hyphen, then spaces become
# hyphens. The em dash in this guide's headings is punctuation, so it is DELETED and both of
# the spaces around it become hyphens - "`A` — b" slugs to "a--b", not "a-b". A hand-typed
# single-hyphen anchor therefore reads correctly and lands nowhere.
def slug(h):
    h = h.lower()
    h = re.sub(r"[^\w\s-]", "", h, flags=re.UNICODE)
    return h.strip().replace(" ", "-")

anchors = {slug(l.lstrip("#").strip()) for l in lines if l.startswith("#")}
links = [(i, m.group(1)) for i, l in enumerate(lines, 1)
         for m in [re.search(r"\]\(#([^)]+)\)", l)] if m]
dead = [(i, a) for i, a in links if a not in anchors]
print(f"   headings: {len(anchors)}   in-document links: {len(links)}   dead: {len(dead)}")
for i, a in dead[:12]:
    print(f"      line {i}: #{a}")
if links and not dead:
    print("   (all links resolve)")
if dead:
    fails.append(f"C7: {len(dead)} in-document link(s) point at no heading in this file")

print()
if fails:
    print("RESULT: FAIL")
    for f in fails:
        print("  -", f)
    sys.exit(1)
print("RESULT: PASS - every class, option and default in the guide is answerable to the C++ inventory")
