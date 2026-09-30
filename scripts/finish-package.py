from pathlib import Path
import json, plistlib, shutil, subprocess
root = Path(__file__).resolve().parent.parent
apps = list((root / '.runtime/package').glob('**/FiveCubes.app'))
if len(apps) != 1:
    raise SystemExit(f'Expected one packaged app, got {len(apps)}')
app = apps[0]
# UAT's archive filter omits NonUFS runtime files in this UE release. Preserve
# the complete staged application, including dylibs, cooked pak and fonts.
staged = root / 'unreal/FiveCubes/Saved/StagedBuilds/Mac/FiveCubes.app'
if not staged.is_dir():
    raise SystemExit('Staged application is missing')
shutil.copytree(staged, app, dirs_exist_ok=True, symlinks=False)
# Some vendor dylibs have a more specific install name than their staged name.
for library in list(app.rglob('*.dylib')):
    lines = subprocess.check_output(['otool', '-arch', 'arm64', '-D', str(library)], text=True).splitlines()
    if len(lines) > 1 and lines[1].startswith('@rpath/'):
        alias = library.parent / Path(lines[1]).name
        if not alias.exists():
            shutil.copy2(library, alias)
resources = app / 'Contents/Resources/Lab'
resources.mkdir(parents=True, exist_ok=True)
shutil.copy2(root / '.cache/node/node-v22.22.3-darwin-arm64/bin/node', resources / 'node')
shutil.copytree(root / 'server', resources / 'server', dirs_exist_ok=True)
shutil.copytree(root / 'node_modules', resources / 'node_modules', dirs_exist_ok=True)
shutil.copy2(root / 'package.json', resources / 'package.json')
scenario_dir = resources / 'unreal/FiveCubes/Content/Lab'
scenario_dir.mkdir(parents=True, exist_ok=True)
for name in ('scenario.json', 'targets.json'):
    shutil.copy2(root / 'unreal/FiveCubes/Content/Lab' / name, scenario_dir / name)
plist_path = app / 'Contents/Info.plist'
with plist_path.open('rb') as f: plist = plistlib.load(f)
plist['NSMicrophoneUsageDescription'] = 'Голосовой разговор с помощником лаборатории. Звук передаётся в OpenAI только после начала разговора.'
plist['CFBundleDisplayName'] = 'Пять кубиков'
plist['CFBundleIdentifier'] = 'local.fivecubes.lab'
with plist_path.open('wb') as f: plistlib.dump(plist, f)
settings = Path.home() / 'Library/Application Support/FiveCubes'
settings.mkdir(parents=True, exist_ok=True, mode=0o700)
config = settings / 'settings.json'
if not config.exists():
    with config.open('x') as f: json.dump({'env_file': str(root / '.env')}, f)
    config.chmod(0o600)
subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(app)], check=True)
subprocess.run(['python3', str(root / 'scripts/verify-mac-bundle.py'), str(app)], check=True)
shortcut = Path.home() / 'Desktop/Пять кубиков.app'
if not shortcut.exists(): shortcut.symlink_to(app, target_is_directory=True)
print(f'Application: {app}\nShortcut: {shortcut}')
