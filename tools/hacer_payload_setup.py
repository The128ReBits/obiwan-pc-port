"""Builds build/setup/payload.zip, the data inside Instalar-Obi-Wan.exe (Claude, 2026-11-26).

Contents:
  * every file tracked by git in this repository (the port's source; no game data, no generated code);
  * python/: the Python 3.12 that runs this script, trimmed (no tests, no tkinter, no site-packages)
    plus the capstone 5.0.9 package, so players don't need to install Python. Their licenses
    (python/LICENSE.txt, python/Lib/site-packages/capstone-5.0.9.dist-info/) go with them.

Run it with the Python 3.12 that has capstone 5.0.9 installed:
    py -3.12 -I -B tools\\hacer_payload_setup.py build\\setup\\payload.zip
"""
from pathlib import Path
import subprocess, sys, zipfile

ROOT = Path(__file__).resolve().parents[1]
CAPSTONE = '5.0.9'
SKIP_LIB = {'site-packages', 'test', 'idlelib', 'tkinter', 'turtledemo', 'ensurepip', '__pycache__', 'pydoc_data'}
SKIP_DLL_PREFIX = ('_test', '_tkinter', 'tcl', 'tk')


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'build' / 'setup' / 'payload.zip'
    if sys.version_info[:2] != (3, 12):
        sys.exit(f'needs Python 3.12, this is {sys.version.split()[0]}')
    import capstone, importlib.metadata
    found = importlib.metadata.version('capstone')  # capstone.__version__ itself says 5.0.7 in the 5.0.9 wheel
    if found != CAPSTONE:
        sys.exit(f'needs capstone {CAPSTONE}, found {found}')
    base = Path(sys.base_prefix)
    site = Path(capstone.__file__).resolve().parent.parent

    tracked = subprocess.run(['git', '-C', str(ROOT), 'ls-files', '-z'], check=True,
                             capture_output=True).stdout.decode('utf-8').split('\0')
    tracked = [t for t in tracked if t]
    items = []  # (zip name, source path)
    for t in tracked:
        p = ROOT / t
        if p.is_file():
            items.append((t, p))

    for name in ('python.exe', 'python3.dll', 'python312.dll', 'vcruntime140.dll', 'vcruntime140_1.dll', 'LICENSE.txt'):
        items.append((f'python/{name}', base / name))
    for p in sorted((base / 'DLLs').iterdir()):
        if p.is_file() and not p.name.startswith(SKIP_DLL_PREFIX):
            items.append((f'python/DLLs/{p.name}', p))
    lib = base / 'Lib'
    for p in sorted(lib.rglob('*')):
        rel = p.relative_to(lib)
        if not p.is_file() or rel.parts[0] in SKIP_LIB or '__pycache__' in rel.parts or p.suffix in ('.pyc', '.pyo'):
            continue
        if any(part in ('test', 'tests', 'idle_test') for part in rel.parts[:-1]):
            continue
        items.append(('python/Lib/' + rel.as_posix(), p))
    for d in (site / 'capstone', site / f'capstone-{CAPSTONE}.dist-info'):
        for p in sorted(d.rglob('*')):
            rel = p.relative_to(site)
            if p.is_file() and '__pycache__' not in rel.parts and p.suffix != '.pyc':
                items.append(('python/Lib/site-packages/' + rel.as_posix(), p))

    missing = [str(p) for _, p in items if not p.is_file()]
    if missing:
        sys.exit('missing: ' + ', '.join(missing))
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = out.with_suffix('.tmp')
    with zipfile.ZipFile(tmp, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, p in items:
            z.write(p, name)
    tmp.replace(out)
    print(f'{out}: {len(items)} files ({len(tracked)} from git), {out.stat().st_size / 1e6:.1f} MB')


if __name__ == '__main__':
    main()
