# Fresh licensed Native host verification

This procedure builds a local host and a Territory-only package. It does not
publish a release and does not certify the separate multiplayer, streaming, cook
or performance gates. Use a matching licensed Narrative Pro 2.4.2 installation
and UE 5.7 or 5.8, Windows C++ build tools, Python 3 and Git.

Run from PowerShell with a **new** output directory outside both the source and
Engine directories:

```powershell
./Tools/Build-Release.ps1 `
  -EngineRoot 'D:/Program Files/Epic Games/UE_5.8' `
  -PluginSource 'D:/TerritoryFramework' `
  -OutputRoot 'D:/Verification/Territory-UE58-run1' `
  -ApplyNarrativeCompatibilityPatch
```

`-NarrativeRoot` selects a specific licensed installation if multiple ones exist.
Without explicit patch opt-in, an affected unpatched dependency fails preflight.
The command copies Native into `HostProject/Plugins/NarrativePro` and modifies
only that copy, retaining the exact original outside the package under
`DependencyBackups`. Unknown versions/hashes are rejected before correction.
Native source and its backup must never be uploaded with the Territory package.

The host uses Native's project template content, tags and configuration with an
empty startup map. It extends the existing Native NPCDefinition/PlayerDefinition
scan directories with `/TerritoryFramework`, registers the template's missing
`SetByCaller.Damage` tag and selects the included road-guide Blueprint. These are
host configuration changes, not vendor source patches. Native's `SpawnedData`
bundles still load character appearance and activity data. Omitted registration
was reproduced as a Native appearance assertion in a cold multiplayer host;
loading a definition object alone was insufficient. The content validator now
checks the actual primary-asset path before accepting plugin NPC definitions.
Do not scan same-named `/Game` demo copies alongside the plugin originals.

The script explicitly compiles patched NarrativeSaveSystem for Editor. Game
Development and Shipping use `-BuildDependantPlugins` with Territory so the patched
Native runtime modules compile with the corresponding target.
For UE 5.7 the script explicitly disables adaptive unity, retaining normal unity
compilation. Stock Native 2.4.2's movement-task implementation lacks direct Engine
includes and fails when writable copied files are pulled out of their unity group
for a Game build. The supported build setting avoids changing another licensed
source file; it does not certify Native 2.4.2 for non-unity Game compilation.
The receipt records this compiler compatibility argument.
It then checks generated documentation against current source, runs the bounded
full automation filter against the newly built host and validates every included
asset/Blueprint with PIE stopped. A zero-test, partial, failed, timed-out or
crashed run produces no successful receipt and blocks CI upload.

`BUILD_INFO.json` is written only after these checks pass. It records the source
commit and actual source snapshot hash, Engine version, Native version and
reviewed LF-normalized patch hash, build configurations and test/content counts.
`SOURCE_SNAPSHOT.json` outside the package lists input file hashes. An uncommitted
working tree is identified by its snapshot hash as well as its base commit.
For an exported source tree without Git metadata, provide `-SourceCommit` with
the export's original revision. Do not label an arbitrary export as that commit's
unchanged contents.

Use `-Resume` only for the exact same source snapshot and output path. If any
source/content/documentation changed, choose a new output directory; the script
rejects a mismatched resume to prevent stale files or test binaries. Inspect the
logs beside the host after a failed build; never reuse a previous green receipt
to approve different inputs. `-TestTimeoutSeconds` bounds each automation/content
process; timeout termination targets only the process started by the runner.

After these gates, run the approved three-repeat composed scenario on both TDA
and this host: server plus two clients, capture/recruit/finite assault/casualty,
save/cold restart, physical World Partition unload/reload, late join, and all
terminal outcomes. Record actual configured NPC/schedule/history limits and
cold/warm client cost/bandwidth. The focused world fixtures do not certify socket
transport or physical streaming. A packaged plugin build is not a cooked-game
test; run Win64 cook/package and packaged gameplay separately.

Keep matching source/content and pre-migration campaign/player saves outside the
release. Rollback restores that entire matching set, plus the reviewed dependency
backup if needed. Never assume a new-format save can be read safely by an older
binary. Public push/upload is a separate authorized action.
