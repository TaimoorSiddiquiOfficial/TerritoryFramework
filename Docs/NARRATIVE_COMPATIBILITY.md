# Narrative save compatibility

Territory normally extends Narrative through public APIs. The approved exception
is limited to two defects in
`Source/NarrativeSaveSystem/Private/Subsystems/NarrativeSaveSubsystem.cpp`, first
confirmed on Narrative Pro **2.4.2** and re-confirmed against **2.5.0**:

- `LoadPlayerOnlySave` assigned the player slot to the campaign pointer, including
  assigning null for a missing slot. The correction keeps a local strong reference,
  validates the records before invoking Native loaders, restores the loading flag
  on every exit, and checks player lifetime/possession after callbacks.
- `MultiplayerLoad` rejected existing slots and could report success without a
  travel request. The correction validates the save and map, requires authoritative
  travel context, rejects overlapping travel, and returns `UWorld::ServerTravel`'s
  admission result. That result means **queued**, not that network travel has finished.

Both entrypoints use a private bounded decoder for the existing UE5 GVAS format 3
container. Invalid/truncated headers, excessive names/counts, archive errors and
incompatible save classes fail before player callbacks. This avoids Unreal's
unsafe fallback from a corrupt tag to an ancient unversioned class-name stream.
It delegates object serialization to the configured Native save class; no new
save format, slot or persistent authority is introduced. Pre-UE5 containers require
explicit migration and are not silently interpreted as current saves.

The authoritative campaign object, actor `RecordMap`, current campaign slot and
player slot naming/account policy are unchanged. The existing `LoadActorFromRecord`
continues to own player deserialization. This correction is not a general validator
for arbitrary untrusted actor byte streams.

## Version pins

`Tools/Compatibility/narrative-<version>.json` holds one reviewed version: the
SHA-256 of the original source and of the patched result. The verifier reads the
installed `NarrativePro.uplugin` `VersionName` and selects the matching manifest, so
an installed version with no manifest is refused instead of patched. `Build-Release.ps1`
uses the same rule: the supported version is whichever manifest pins the installed
Vendor source, not a literal string in the script.

After a vendor update, rebase the manifest before building:

```powershell
python Tools/Rebase-NarrativeCompatibility.py --narrative-root "D:/YourProject/Plugins/Narrative383be31f74eV7" --version 2.5.0
```

It refuses unless every patch anchor (`#include "Subsystems/NarrativeSaveSubsystem.h"`,
the constructor, and both function signatures) appears exactly once, prints both
digests, and writes nothing without `--write`. Review the two upstream functions, then
commit the new manifest beside the previous one so the retired pin stays auditable.

Exercise the admission/rollback path against an unpatched fixture. Use a staged copy of
the manifest's original source (a patched tree no longer matches the original digest),
for example the backup written by `--backup-root`:

```powershell
python Tools/Tests/Test-NarrativeCompatibility.py --narrative-root "<staged unpatched root>"
```

## Check or apply

Run from the Territory plugin directory with Python 3.9 or newer:

```powershell
python Tools/Verify-NarrativeCompatibility.py --narrative-root "D:/YourProject/Plugins/NarrativePro"
python Tools/Verify-NarrativeCompatibility.py --narrative-root "D:/YourProject/Plugins/NarrativePro" --apply --backup-root "D:/YourProjectBackups/Narrative242"
```

Verification changes nothing. Exit 0 means the reviewed patch is present; exit 2
means the original affected source or an unsupported dependency. `--apply` is an
explicit opt-in and requires an external backup directory. The manifest pins both
original and patched SHA-256 after line-ending normalization. An unexpected source
hash or version is rejected before writing. Reapplying the reviewed patch is a no-op.

Rebuild the dependency explicitly with UnrealBuildTool `-Module=NarrativeSaveSystem`
and rebuild Territory before running. An installed dependency may otherwise update
only its import library and retain its previous DLL.

```powershell
Tools/Run-Tests.ps1 -Filter TerritoryFramework.Persistence.Regression -EngineRoot "D:/Program Files/Epic Games/UE_5.8"
```

The source check cannot prove a loaded binary contains the correction. Behavioral
tests are mandatory after compiling; a currently open Editor may hold old DLLs.
For an isolated host, copy the licensed dependency to that host before applying
the correction. Never patch unrelated Engine installs automatically.

## Updates and rollback

After a vendor update, rerun the verifier. Do not relax its hash gate merely to
make a build pass. Inspect the two upstream functions, run the regressions against
the new dependency, and either retire this correction or review a new manifest.

To restore the exact backed-up source:

```powershell
python Tools/Verify-NarrativeCompatibility.py --narrative-root "D:/YourProject/Plugins/NarrativePro" --restore --backup-root "D:/YourProjectBackups/Narrative242"
```

Rollback refuses locally modified patched source. Rebuild matching binaries after
restore. Keep pre-migration campaign backups when rolling back other Territory
save-schema changes. The Territory package includes only its own replacement
functions, verifier, manifest and tests. Licensed vendor source and backups must
never be included in the release ZIP.

## Proof status

See `Verification/2026-10-05-repair-ledger.md`. A described test or authored fix is
not a passing result. Dedicated/listen multiplayer and the composed cold-restart
scenario remain separate acceptance gates.

## Restore contract (2026-10 repair)

Guard posts retain Native save identities. Saved occupancy is restored per post in
both Claimed and Contested owned Places; an empty saved post is not an instruction
to hire a replacement. Only saves with no authored post records at all use the
legacy aggregate defender count, assigned deterministically by stable post GUID.
New-format post records include the remaining reserve delay. Old records use the
configured delay; explicit zero reserve counts remain exhausted.

Ownership replacement, streaming retirement and save restoration have distinct
teardown intent. Restoration/streaming detach physical guards while retaining saved
occupancy and finite pending reserve requests. Timers resume once a valid available
owner and post are bound; repeated binding does not restart an active timer. Actual
story unlock remains an explicit provisioning transition.

The Territory NPC activity component rebinds its existing perception/rescore timer
after Native's load callback. It does not create a second timer or activity owner.
