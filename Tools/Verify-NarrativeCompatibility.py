"""Verify the licensed local dependency. Mutation requires --apply and an external backup.

Ships only Territory-authored replacement functions; no vendor source or original
backup belongs in a Territory release. Unknown versions/hashes are never patched.
"""
import argparse
import hashlib
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
RELATIVE_SOURCE = Path("Source/NarrativeSaveSystem/Private/Subsystems/NarrativeSaveSubsystem.cpp")


def digest(text):
    return hashlib.sha256(text.replace("\r\n", "\n").encode("utf-8")).hexdigest()


def replacement(source, signature, body):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[:start] + body.rstrip() + source[end:]


def patched_source(source):
    for name in ("LoadPlayerOnlySave", "MultiplayerLoad"):
        body = (HERE / "Compatibility" / (name + ".cpp.txt")).read_text(encoding="utf-8")
        source = replacement(source, "bool UNarrativeSaveSubsystem::" + name + "(", body)
    anchor = '#include "Subsystems/NarrativeSaveSubsystem.h"'
    additions = '\n#include "Engine/Engine.h"\n#include "UObject/StrongObjectPtr.h"\n#include "UObject/UnrealType.h"\n#include "Misc/EngineVersion.h"\n#include "Serialization/CustomVersion.h"'
    source = source.replace(anchor, anchor + additions, 1)
    helper = (HERE / "Compatibility/ValidatedSlot.cpp.txt").read_text(encoding="utf-8")
    marker = "UNarrativeSaveSubsystem::UNarrativeSaveSubsystem()"
    return source.replace(marker, helper + "\n" + marker, 1)


def select_manifest(version):
    """Pick the reviewed manifest that pins this installed version.

    One manifest per version keeps the old pin auditable when the dependency is
    upgraded. A version with no manifest is never patched or rolled back.
    """
    for candidate in sorted((HERE / "Compatibility").glob("narrative-*.json")):
        data = json.loads(candidate.read_text(encoding="utf-8"))
        if data.get("version") == version:
            return candidate, data
    return None, None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--narrative-root", type=Path, required=True)
    actions = parser.add_mutually_exclusive_group()
    actions.add_argument("--apply", action="store_true")
    actions.add_argument("--restore", action="store_true")
    parser.add_argument("--backup-root", type=Path)
    args = parser.parse_args()
    root = args.narrative_root.resolve(strict=True)
    descriptor = json.loads((root / "NarrativePro.uplugin").read_text(encoding="utf-8-sig"))
    manifest_path, manifest = select_manifest(descriptor.get("VersionName"))
    if manifest is None:
        parser.error("No compatibility manifest pins Narrative version "
                     + repr(descriptor.get("VersionName"))
                     + "; review the dependency and add one with the documented exception "
                       "before patching.")
    path = root / RELATIVE_SOURCE
    original_bytes = path.read_bytes()
    text = original_bytes.decode("utf-8").replace("\r\n", "\n")
    current = digest(text)
    if current not in (manifest["original_sha256_lf"], manifest["patched_sha256_lf"]):
        parser.error("Unexpected Narrative source hash; refusing a blind patch or rollback: " + current)
    patched = current == manifest["patched_sha256_lf"]
    if args.apply and not patched:
        if args.backup_root is None:
            parser.error("--apply requires --backup-root outside the plugin/release tree.")
        backup_root = args.backup_root.resolve()
        if backup_root.is_relative_to(HERE.parent) or backup_root.is_relative_to(root):
            parser.error("Backup must be external to both plugin trees.")
        result = patched_source(text)
        if digest(result) != manifest["patched_sha256_lf"]:
            parser.error("Patch payload hash differs from the reviewed manifest.")
        backup_root.mkdir(parents=True, exist_ok=True)
        backup = backup_root / (current + ".cpp.bak")
        if backup.exists() and digest(backup.read_text(encoding="utf-8")) != current:
            parser.error("Backup path already contains unrelated data.")
        if not backup.exists():
            backup.write_bytes(original_bytes)
        newline = "\r\n" if b"\r\n" in original_bytes else "\n"
        path.write_bytes(result.replace("\n", newline).encode("utf-8"))
        patched = True
    elif args.restore and patched:
        if args.backup_root is None:
            parser.error("--restore requires the original --backup-root.")
        backup = args.backup_root / (manifest["original_sha256_lf"] + ".cpp.bak")
        restored = backup.read_bytes()
        if digest(restored.decode("utf-8")) != manifest["original_sha256_lf"]:
            parser.error("Rollback backup hash mismatch.")
        path.write_bytes(restored)
        patched = False
    print(json.dumps({"version": descriptor["VersionName"], "manifest": str(manifest_path),
        "source": str(path), "status": "patched" if patched else "unpatched",
        "sha256_lf": digest(path.read_text(encoding="utf-8"))}))
    # Verify is a release gate. An original but affected dependency cannot pass.
    return 0 if patched or args.restore else 2


if __name__ == "__main__":
    raise SystemExit(main())
