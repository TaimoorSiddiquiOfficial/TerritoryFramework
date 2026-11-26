# Repeatable Native release host

Install the licensed Narrative Pro 2.4.2 build matching UE 5.7 or 5.8. Do not copy
vendor source into a Territory package. Export the plugin's tracked release inputs
to a separate source directory; include the newly added source and tooling files.

Run `Tools/Build-Release.ps1` with `-EngineRoot`, `-PluginSource`, a new
`-OutputRoot`, the original `-SourceCommit`, and explicit
`-ApplyNarrativeCompatibilityPatch`. The script verifies the vendor version and
source hash, copies that dependency into the isolated host, and applies only the
approved save-subsystem correction there. Engine installations are untouched.
UE 5.7 uses normal unity builds for Native's supported source layout.

The host uses Native's template configuration/content and extends the existing
NPCDefinition/PlayerDefinition scans with `/TerritoryFramework`. Do not register
both a project copy and the plugin copy of a same-named Native primary asset.
Territory's validator rejects the unregistered duplicate. Both LF and CRLF vendor
templates are regression-tested; an unknown template layout is rejected.

Every successful pipeline builds Editor, Development and Shipping targets, runs
fresh-source options checks, the bounded full automation suite, and stopped-editor
asset/Blueprint validation. `Package/TerritoryFramework/BUILD_INFO.json` records
the source commit AND exact file snapshot hash, engine/dependency versions,
dependency patch hash, and the actual test/content results. A commit identifier
alone does not describe an uncommitted source export.

`-Resume` requires the exact same source snapshot. An explicit
`-Resume -RefreshSource` can reuse an isolated host's compiler outputs for a
reviewed repair: it preserves the previous source manifest, copies changed inputs,
rejects removed/unexpected source inputs, and reruns every build/test gate. It does
not skip tests, delete old source, reset the vendor dependency, or certify a failed
build. Use a new host when files must be removed. Prior packages are archived
separately before a replacement package is assembled.

Compilation and automation are not cooked-game or multiplayer certification.
Record socket multiplayer, cold process restart, physical streaming, late join,
performance and Win64 cook/package results separately for the exact tested code
and content. The composed fixture uses a controlled quest checkpoint and real
Native damage/save/login, not a complete manual dialogue/weapon-input playthrough.
Performance in an editor with NullRHI excludes rendering/GPU and is not a shipping
frame-rate claim. Public artifact upload remains a separate action.

Before migration, keep matching code/content and campaign-save backups. New save
fields and reconstructed UE 5.7 assets are described in the repair/backport ledgers.
Network peers must use matching plugin binaries. Do not infer that a newer save
can safely be loaded by an older binary.
