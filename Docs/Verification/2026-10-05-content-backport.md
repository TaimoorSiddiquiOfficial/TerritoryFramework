# UE 5.7 content reconstruction

Seventeen included assets had been saved by UE 5.8 and were rejected by UE 5.7.4.
The repair reconstructs their current authored data through Unreal object text
import and Blueprint compilation in an isolated UE 5.7 host. No package-header
version bypass, engine patch, vendor content change, or old Git asset rollback
was used. The import helper stays in the external verification workspace.

Comparison covers authored graphs and pin links/defaults, Blueprint GUIDs, data
asset properties, default objects, widget trees/animations and generated-class
construction scripts/component templates. Class-owned SCS objects and inherited
component handlers require explicit binding before compilation; compile success
alone failed to detect their loss during an early diagnostic. Final verification
reopens the assets in a new process, recompiles and compares exports again.

Territory, guard-post, NPC-definition and Blueprint component GUIDs are preserved.
Data assets, melee activity graph/defaults, WorldState, admission task and style
defaults compare exactly before reload. Reviewed engine differences are:

- Unreferenced editor/compiler orphan nodes and a dialogue shot are collected;
  their names have no references from surviving authored records.
- UE 5.7 regenerates WidgetTree's derived AllWidgets array and MovieScene change
  signatures. Animation bindings, playback ranges and curve values remain equal.
- Dynamic-cast purity is custom-serialized in UE 5.7; each affected cast is checked
  after reload. Execution links remain equal.
- Empty UE 5.8 physics/editor fields, default-root visualization, compiler pin
  display text/tooltips and one unlinked hidden tolerance pin differ by engine.
- UE 5.7 recompilation rekeys the inherited `Melee Attack` activity display text:
  namespace `[06B2F27CC79BC0494AC8E47ADB6D6CF9]`, old key
  `4C7CBEF740B254700ACB649A2C429719`, new key
  `BB113CAE4CD1419091AE2AB5FE3D17CE`. Text and gameplay remain unchanged. No
  localization resources ship here; external translation owners must map this
  single key when migrating. Other authored text namespaces/keys are preserved.

The isolated reconstruction passed 527 automation tests and stopped-PIE content
validation (123 assets, 79 Blueprints, zero errors, two prototype-appearance
warnings) before promotion. The later Native registration check adds a test and
requires a fresh final release receipt; these earlier results do not certify that
later source snapshot. Source/destination SHA-256 values and rollback binaries
are retained outside the release in the verification workspace.
