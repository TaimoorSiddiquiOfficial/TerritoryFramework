# Audit branch checkpoint — 17 September 2026

This is a development checkpoint, not a release certification.

## Verified native light adapter batch

All six native source/test files listed in `DIRECT_LIGHT_ELEMENTS_2026-09-14.json`
still match their recorded SHA-256 hashes. The retained local evidence records
successful UE 5.8 and 5.7 Editor, Development and Shipping builds, and 340 passing
automation tests per engine (including existing warning results). Native code was
reviewed again on 17 September; no new native changes were made for this push.
The authority remains Narrative sequence playback and the local camera. The
adapter adds transient presentation state, no campaign records or RPCs.

The older asset and package results apply to the exact files recorded on
14 September. They do not certify subsequent project asset moves or preset edits.

## Additional saved work awaiting validation

The player-controller and five UI assets have further binary changes. The plugin
descriptor also enables optional Narrative tutorial, RPG UI, item-inspector and
UDS integrations. These changes are preserved separately as an unverified
checkpoint. They still require Blueprint compilation, asset-reference validation,
testing with optional dependencies absent, gameplay checks and a fresh cook.

The TDA project moved its LightRig examples into `LightRig/Blueprint`,
`LightRig/DataAsset` and `LightRig/DataTable` subfolders. Existing guides and test
scripts describe the older verified paths. Update their paths and verify saved
references before using those scripts as current acceptance tests. Individual
element preset-row and UDS switching work was interrupted during a DataTable
import; it is not accepted as complete by this checkpoint.

Unreal Editor was not running during this review. No new editor validation,
Blueprint compilation, PIE or cook result is claimed. The remaining AlMalik,
World Partition, rendered dialogue, HDR and performance gates remain open.
