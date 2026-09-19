# Wave 4 step 1: authoritative option inventory from plugin C++.
# Source is authoritative (AGENTS.md), so the doc is generated FROM this, never hand-listed.
#
# v2: replaces the regex core with a balanced-paren scanner. The regex silently failed on
# multiline UCLASS specs with nested parens (`ClassGroup=(Territory), meta=(...)`) - 417 of
# 1832 rows resolved no class - and on `bool bFoo = false;` it picked `false` as the name.
import os, re, json, glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # plugin root; this file is in Tools/
SRC = os.path.join(ROOT, "Source", "TerritoryFramework")
OUT = os.path.join(ROOT, "Tools", "options_inventory.json")

def read(p):
    return open(p, encoding="utf-8", errors="replace").read()

def consume_balanced(text, i):
    """text[i] must be '('. Return (body, index_after_closing_paren)."""
    assert text[i] == "(", (i, text[i:i + 20])
    depth, start, j, inq = 0, i + 1, i, None
    while j < len(text):
        ch = text[j]
        if inq:
            if ch == "\\":
                j += 2
                continue
            if ch == inq:
                inq = None
        elif ch in "\"'":
            inq = ch
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return text[start:j], j + 1
        j += 1
    raise ValueError("unbalanced paren")

def find_uproperties(text):
    """Yield (spec, decl) for every real UPROPERTY, skipping comments and strings."""
    out, i, inq, in_comment = [], 0, None, None
    while i < len(text):
        ch = text[i]
        if in_comment == "//":
            if ch == "\n":
                in_comment = None
            i += 1
            continue
        if in_comment == "/*":
            if text.startswith("*/", i):
                in_comment = None
                i += 2
                continue
            i += 1
            continue
        if inq:
            if ch == "\\":
                i += 2
                continue
            if ch == inq:
                inq = None
            i += 1
            continue
        if text.startswith("//", i):
            in_comment = "//"; i += 2; continue
        if text.startswith("/*", i):
            in_comment = "/*"; i += 2; continue
        if ch in "\"'":
            inq = ch; i += 1; continue
        if text.startswith("UPROPERTY", i) and not (i and (text[i - 1].isalnum() or text[i - 1] == "_")):
            j = i + len("UPROPERTY")
            while j < len(text) and text[j] in " \t\r\n":
                j += 1
            if j < len(text) and text[j] == "(":
                spec, after = consume_balanced(text, j)
                # declaration runs to the next top-level ';'
                k, depth, dq = after, 0, None
                while k < len(text):
                    c = text[k]
                    if dq:
                        if c == "\\":
                            k += 2; continue
                        if c == dq:
                            dq = None
                    elif c in "\"'":
                        dq = c
                    elif c in "([<{":
                        depth += 1
                    elif c in ")]>}":
                        depth -= 1
                    elif c == ";" and depth == 0:
                        break
                    k += 1
                decl = text[after:k]
                # yield the TRUE declaration offset: re-finding decl by text later would locate
                # the first occurrence, so two classes with an identically-named property both
                # resolved to the earlier class - that was the 83-duplicate anomaly.
                out.append((" ".join(spec.split()), " ".join(decl.split()), after))
                i = k + 1
                continue
        i += 1
    return out

DECL = re.compile(r"(?:class|struct)\s+(?:\w+_API\s+)?(?P<name>\w+)\s*(?:final\s*|abstract\s*)?(?=[:{])")

def enclosing_class(text, offset):
    """Nearest preceding real type declaration. Requires ':' or '{' after the name, which
    excludes forward declarations (`class UDialogue;`) and elaborated parameters (`class UX* p`)."""
    hits = list(DECL.finditer(text, 0, offset))
    return hits[-1].group("name") if hits else "?"

META = re.compile(r"meta\s*=\s*\((?P<meta>(?:[^()]|\([^()]*\))*)\)", re.S)

def split_meta(meta_body):
    out, depth, buf, inq = [], 0, "", None
    for ch in meta_body:
        if inq:
            buf += ch
            if ch == inq:
                inq = None
            continue
        if ch in "\"'":
            inq = ch; buf += ch; continue
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(buf.strip()); buf = ""; continue
        buf += ch
    if buf.strip():
        out.append(buf.strip())
    return out

def strip_top_level_initializer(decl):
    depth, inq = 0, None
    for i, ch in enumerate(decl):
        if inq:
            if ch == inq:
                inq = None
            continue
        if ch in "\"'":
            inq = ch
        elif ch in "<([":
            depth += 1
        elif ch in ">)]":
            depth -= 1
        elif ch == "=" and depth == 0:
            prev = decl[i - 1] if i else ""
            nxt = decl[i + 1] if i + 1 < len(decl) else ""
            if prev not in "=!<>" and nxt != "=":
                return decl[:i].rstrip()
    return decl

def unquote(s):
    s = s.strip()
    if len(s) >= 2 and s[0] == s[-1] and s[0] in "\"'":
        return s[1:-1]
    return s

rows = []
files = sorted(glob.glob(os.path.join(SRC, "**", "*.h"), recursive=True)) + \
        sorted(glob.glob(os.path.join(SRC, "**", "*.cpp"), recursive=True))

for path in files:
    text = read(path)
    rel = os.path.relpath(path, ROOT).replace("\\", "/")
    for spec, decl, off in find_uproperties(text):
        keys, vals = {}, {}
        mm = META.search(spec)
        if mm:
            for kv in split_meta(mm.group("meta")):
                if "=" in kv:
                    k, v = kv.split("=", 1)
                    keys[k.strip()] = True; vals[k.strip()] = unquote(v)
                elif kv:
                    keys[kv.strip()] = True
        base = strip_top_level_initializer(decl)
        names = re.findall(r"\b([A-Za-z_]\w*)\b", base)
        name = names[-1] if names else "?"
        typ = base[:base.rfind(name)].strip() if name in base else base
        # capture the default initializer, if the author wrote one: `= false`, `= 328.f`, `= {}`
        init = decl[len(base):].lstrip()
        default = init[1:].strip() if init.startswith("=") else ""
        rows.append({
            "file": rel,
            "class": enclosing_class(text, off),
            "name": name,
            "type": typ,
            "default": default,
            "spec": spec,
            "meta_keys": sorted(keys),
            "tooltip": vals.get("ToolTip", ""),
            "display": vals.get("DisplayName", ""),
            "edit_spec": "EditAnywhere" in spec,
            "config_spec": "Config" in spec,
        })

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(rows, f, indent=1)

print("OUT:", OUT)
print("UPROPERTY_TOTAL:", len(rows))
print("FILES_SCANNED:", len(files))
print("EDITANYWHERE:", sum(1 for r in rows if r["edit_spec"]))
print("CONFIG:", sum(1 for r in rows if r["config_spec"]))
print("WITH_TOOLTIP:", sum(1 for r in rows if r["tooltip"]))
print("WITH_DISPLAYNAME:", sum(1 for r in rows if r["display"]))
print("DISTINCT_CLASSES:", len({r["class"] for r in rows}))
print("UNRESOLVED_CLASS:", sum(1 for r in rows if r["class"] == "?"))
