"""Recompute the reviewed compatibility manifest for a new Narrative Pro version.

`Verify-NarrativeCompatibility.py` refuses to touch a source whose hash is not pinned
in a manifest, and it refuses a manifest whose version does not match the installed
`.uplugin`. Upgrading Narrative Pro therefore needs exactly one reviewed step: take the
new vendor source, confirm the Territory replacement functions still have unambiguous
anchors, compute the original and patched hashes with the same transform the gate uses,
and write a new manifest beside the old one.

This tool never edits vendor source. It only reads it and (with --write) adds a
`Compatibility/narrative-<version>.json`. Applying the patch is still the gate's job:
`Verify-NarrativeCompatibility.py --apply --backup-root <external dir>`.

Refusals are deliberate:

- the installed version must match `--version` and the descriptor's `VersionName`;
- every patch anchor must appear exactly once (a duplicated anchor means the transform
  would silently replace the wrong body);
- a source whose hash already equals a known patched hash is not "re-based";
- an existing manifest for that version is kept unless --force is given.
"""
import argparse
import importlib.util
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent


def load_gate():
    """Import the hyphenated gate module so both share one patch transform."""
    path = HERE / "Verify-NarrativeCompatibility.py"
    spec = importlib.util.spec_from_file_location("verify_narrative_compatibility", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


ANCHORS = (
    '#include "Subsystems/NarrativeSaveSubsystem.h"',
    "UNarrativeSaveSubsystem::UNarrativeSaveSubsystem()",
    "bool UNarrativeSaveSubsystem::LoadPlayerOnlySave(",
    "bool UNarrativeSaveSubsystem::MultiplayerLoad(",
)


def manifests(compatibility_dir):
    for candidate in sorted(compatibility_dir.glob("narrative-*.json")):
        yield candidate, json.loads(candidate.read_text(encoding="utf-8"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--narrative-root", type=Path, required=True)
    parser.add_argument("--version", required=True,
                        help="Expected plugin VersionName, e.g. 2.5.0")
    parser.add_argument("--write", action="store_true",
                        help="Write Compatibility/narrative-<version>.json")
    parser.add_argument("--force", action="store_true",
                        help="Overwrite an existing manifest for this version")
    args = parser.parse_args()

    gate = load_gate()
    root = args.narrative_root.resolve(strict=True)
    compatibility_dir = HERE / "Compatibility"
    source_path = root / gate.RELATIVE_SOURCE
    if not source_path.is_file():
        parser.error("Vendor source not found: " + str(source_path))

    descriptor = json.loads((root / "NarrativePro.uplugin").read_text(encoding="utf-8-sig"))
    installed = descriptor.get("VersionName")
    if installed != args.version:
        parser.error("Installed VersionName is %r, not %r" % (installed, args.version))

    original_bytes = source_path.read_bytes()
    text = original_bytes.decode("utf-8").replace("\r\n", "\n")
    current = gate.digest(text)

    known = list(manifests(compatibility_dir))
    for _, manifest in known:
        if current == manifest["patched_sha256_lf"]:
            parser.error("Source already matches the patched hash pinned for Narrative "
                         + manifest["version"] + "; nothing to rebase.")
    for _, manifest in known:
        if manifest["version"] == args.version and not args.force:
            parser.error("Manifest for " + args.version + " already exists; use --force to "
                         "replace it after re-reviewing the patch transform.")

    anchor_counts = {anchor: text.count(anchor) for anchor in ANCHORS}
    missing = [anchor for anchor, count in anchor_counts.items() if count != 1]
    if missing:
        parser.error("Patch anchors are not unique in the installed source: "
                     + json.dumps(anchor_counts, indent=2))

    patched_text = gate.patched_source(text)
    patched = gate.digest(patched_text)
    if patched == current:
        parser.error("Patch transform changed nothing; the anchors no longer describe "
                     "the installed functions.")
    for required in ("LoadTerritoryValidatedNativeSlot", "FTerritoryBoundedSaveArchive"):
        if required not in patched_text:
            parser.error("Patched source is missing " + required + ".")

    target = compatibility_dir / ("narrative-" + args.version + ".json")
    manifest = {
        "version": args.version,
        "original_sha256_lf": current,
        "patched_sha256_lf": patched,
        "scope": ["LoadPlayerOnlySave", "MultiplayerLoad"],
        "regressions": [
            "TerritoryFramework.Persistence.Regression.PlayerSaveIsolation",
            "TerritoryFramework.Persistence.Regression.MultiplayerLoadValidation",
        ],
    }
    if args.write:
        target.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "version": installed,
        "source": str(source_path),
        "anchors": anchor_counts,
        "original_sha256_lf": current,
        "patched_sha256_lf": patched,
        "manifest": str(target),
        "written": bool(args.write),
    }, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
