"""Run in Unreal Editor Python. TF_RELEASE_REPORT selects the JSON output path."""
import json
import os
from pathlib import Path
import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
registry.wait_for_completion()
# Common Input may request these while a widget is compiling. Load them first.
for dependency in registry.get_all_assets():
    if str(dependency.asset_name).startswith('ControllerData_'):
        dependency.get_asset()
# UE 5.7 can also report a Blueprint's generated class as an asset in its package.
# Validate the primary saved object once; compiling its Blueprint validates the class.
assets = [a for a in registry.get_assets_by_path('/TerritoryFramework', True, True)
          if str(a.asset_name) == str(a.package_name).rsplit('/', 1)[1]]
validator = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
rows = []
blueprints = 0
for asset in assets:
    row = {'asset': str(asset.package_name), 'errors': [], 'warnings': []}
    try:
        obj = asset.get_asset()
        if not obj:
            raise RuntimeError('Asset did not load')
        if isinstance(obj, unreal.Blueprint):
            unreal.BlueprintEditorLibrary.compile_blueprint(obj)
            blueprints += 1
        result, errors, warnings = validator.is_asset_valid(asset, unreal.DataValidationUsecase.MANUAL)
        row.update(result=str(result), errors=[str(e) for e in errors], warnings=[str(w) for w in warnings])
    except Exception as error:
        row['errors'].append(str(error))
    rows.append(row)
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
external = []
optional = []
for asset in assets:
    for dependency in registry.get_dependencies(asset.package_name, options):
        dep = str(dependency)
        if dep.startswith(('/Engine/', '/Script/', '/NarrativePro/', '/TerritoryFramework/')):
            continue
        pair = {'asset': str(asset.package_name), 'dependency': dep}
        if str(asset.package_name) == '/TerritoryFramework/EUB_TerritoryHDRSceneMaker' and dep == '/NP_UltraDynamicSky/Narrative_UDS_Sky':
            optional.append(pair)
        else:
            external.append(pair)
report = {
    'engine_version': unreal.SystemLibrary.get_engine_version(),
    'assets_checked': len(rows), 'blueprints_compiled': blueprints,
    'errors': sum(len(r['errors']) for r in rows),
    'warnings': sum(len(r['warnings']) for r in rows),
    'invalid': [r['asset'] for r in rows if 'INVALID' in r.get('result', '') or r['errors']],
    'external_dependencies': external, 'optional_editor_dependencies': optional, 'assets': rows,
}
path = Path(os.environ.get('TF_RELEASE_REPORT', str(Path(unreal.Paths.project_saved_dir()) / 'TerritoryContentValidation.json')))
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(json.dumps(report, indent=2), encoding='utf-8')
print('TERRITORY_CONTENT_VALIDATION ' + json.dumps({k: v for k, v in report.items() if k != 'assets'}))
assert len(rows) == 118, 'Expected all 118 included assets'
assert not report['invalid'] and not external, 'Content validation failed; see ' + str(path)
