"""Fail packaging if dyld cannot resolve any non-system load dependency locally."""
from pathlib import Path
import subprocess, sys, re
app = Path(sys.argv[1]).resolve()
exe = app / 'Contents/MacOS/FiveCubes'
def expand(value, loader):
    return Path(value.replace('@executable_path', str(exe.parent)).replace('@loader_path', str(loader.parent)))
def rpaths(binary):
    raw = subprocess.check_output(['otool', '-arch', 'arm64', '-l', str(binary)], text=True)
    return [expand(p, binary) for p in re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset', raw)]
base = rpaths(exe)
checked = set()
def visit(binary):
    binary = binary.resolve()
    if binary in checked: return
    checked.add(binary)
    raw = subprocess.check_output(['otool', '-arch', 'arm64', '-L', str(binary)], text=True).splitlines()[1:]
    if binary.suffix == '.dylib': raw = raw[1:]  # install id is not a load command
    for line in raw:
        name = line.strip().split(' (compatibility')[0]
        if name.startswith(('/System/Library/', '/usr/lib/')): continue
        candidates = ([p / name[len('@rpath/'):] for p in rpaths(binary) + base]
                      if name.startswith('@rpath/') else [expand(name, binary)])
        match = next((p for p in candidates if p.is_file() and p.resolve().is_relative_to(app)), None)
        if not match: raise SystemExit(f'Unresolved bundled dependency: {binary.name} -> {name}')
        visit(match)
visit(exe)
visit(app / 'Contents/Resources/Lab/node')
assert list((app / 'Contents/UE/FiveCubes/Content/Paks').glob('*.pak')), 'Cooked game pak missing'
subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
print(f'Bundle verified: {len(checked)} Mach-O files, dependencies local, cooked pak present, signature valid')
