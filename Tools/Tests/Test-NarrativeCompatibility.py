"""Exercise patch admission/rollback using a caller-owned licensed fixture."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--narrative-root", type=Path, required=True,
                    help="Root holding the UNPATCHED vendor source to exercise (for an "
                         "already-patched tree, stage the manifest's original backup).")
args = parser.parse_args()
tool = Path(__file__).resolve().parents[1] / "Verify-NarrativeCompatibility.py"
relative = Path("Source/NarrativeSaveSystem/Private/Subsystems/NarrativeSaveSubsystem.cpp")

# The gate pins one manifest per installed version, so the fixture descriptor must claim
# the version whose manifest actually pins the supplied source. Reusing the source bytes
# keeps this test honest: it can only pass when the manifest's hash really describes them.
supplied = (args.narrative_root / relative).read_text(encoding="utf-8").replace("\r\n", "\n")
supplied_digest = hashlib.sha256(supplied.encode("utf-8")).hexdigest()
version = None
for manifest_path in sorted((tool.parent / "Compatibility").glob("narrative-*.json")):
    data = json.loads(manifest_path.read_text(encoding="utf-8"))
    if data["original_sha256_lf"] == supplied_digest:
        version = data["version"]
if version is None:
    raise SystemExit("No compatibility manifest pins this source as original ("
                     + supplied_digest + "); supply the unpatched vendor file.")

with tempfile.TemporaryDirectory(prefix="territory-compat-test-") as temp:
    root = Path(temp) / "NarrativePro"
    source = root / relative
    source.parent.mkdir(parents=True)
    source.write_bytes((args.narrative_root / relative).read_bytes())
    descriptor = root / "NarrativePro.uplugin"
    descriptor.write_text(json.dumps({"VersionName": version}), encoding="utf-8")
    baseline = source.read_bytes()
    backup = Path(temp) / "Backup"

    def run(expected, *extra):
        result = subprocess.run([sys.executable, str(tool), "--narrative-root", str(root), *extra],
            capture_output=True, text=True)
        if result.returncode != expected:
            raise AssertionError(f"{extra}: expected {expected}, got {result.returncode}\n{result.stdout}\n{result.stderr}")

    run(2)  # A recognized but unpatched dependency is a failing release gate.
    run(2, "--apply")
    assert source.read_bytes() == baseline
    run(0, "--apply", "--backup-root", str(backup))
    changed = source.read_bytes()
    assert changed != baseline
    run(0)
    backups = {p.name: p.read_bytes() for p in backup.iterdir()}
    run(0, "--apply", "--backup-root", str(backup))
    assert source.read_bytes() == changed
    assert backups == {p.name: p.read_bytes() for p in backup.iterdir()}
    source.write_bytes(changed + b"\n// unrelated local change\n")
    refused = source.read_bytes()
    run(2, "--apply", "--backup-root", str(backup))
    run(2, "--restore", "--backup-root", str(backup))
    assert source.read_bytes() == refused
    source.write_bytes(changed)
    run(0, "--restore", "--backup-root", str(backup))
    assert source.read_bytes() == baseline
    descriptor.write_text(json.dumps({"VersionName": "9.9.9"}), encoding="utf-8")
    run(2, "--apply", "--backup-root", str(backup))
    assert source.read_bytes() == baseline
print("PASS: unpatched gate, opt-in, apply, idempotence, backup, unknown-hash refusal, rollback, version refusal")
