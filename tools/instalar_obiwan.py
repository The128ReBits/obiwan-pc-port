"""Play with your own ISO / Jugar con tu ISO.

Builds the Obi-Wan PC port from your own copy of the game:
  1. reads default.xbe from your ISO and checks it is the supported edition;
  2. extracts the game files to private\\game\\usa (nothing is ever written to the ISO);
  3. translates the game's code to C (tools/generate_obiwan.py);
  4. compiles obiwan_recomp.exe, the launcher and the settings tool with MSVC 2022.

Usage: py -3 tools\\instalar_obiwan.py [--iso PATH] [--regenerar] [--comprobar-iso]

Nothing here downloads the game or ships any part of it: everything is made on your PC
from the ISO you give it.
"""
from pathlib import Path, PurePosixPath
import argparse, hashlib, json, os, shutil, struct, subprocess, sys

ROOT = Path(__file__).resolve().parents[1]
TITLE = ROOT / 'work' / 'obiwan-claude-completo'
SDK = ROOT / 'work' / 'xboxrecomp-claude-completo9'
GAME = ROOT / 'private' / 'game' / 'usa'
GEN = TITLE / 'src' / 'recomp' / 'gen'
SECTOR = 2048
PARTITIONS = (0, 0x18300000, 0x0FD90000, 0x02080000)
NEED_GB = 4.0  # game files 2.1 GB + generated C + build


def say(en, es=None):
    print(en if es is None else f'{en}\n  {es}', flush=True)


def fail(en, es):
    say('ERROR: ' + en, es)
    sys.exit(1)


class Xdvdfs:
    """Read-only reader for an Xbox DVD image (XDVDFS), redump or xiso."""

    def __init__(self, path):
        self.size = path.stat().st_size
        self.f = path.open('rb')
        self.base = None
        for base in PARTITIONS:
            off = base + 32 * SECTOR
            if off + SECTOR > self.size:
                continue
            s = self.read(off, SECTOR)
            if s[:20] == b'MICROSOFT*XBOX*MEDIA' and s[0x7EC:0x800] == b'MICROSOFT*XBOX*MEDIA':
                self.base = base
                self.root = struct.unpack_from('<II', s, 20)
                break
        if self.base is None:
            raise ValueError('not an Xbox game image')

    def read(self, off, n):
        if off < 0 or n < 0 or off + n > self.size:
            raise ValueError('read outside the image')
        self.f.seek(off)
        d = self.f.read(n)
        if len(d) != n:
            raise ValueError('short read')
        return d

    def directory(self, sector, size):
        if size == 0:
            return []
        if size > 64 * 1024 * 1024:
            raise ValueError('directory too large')
        t = self.read(self.base + sector * SECTOR, size)
        out, todo, seen = [], [0], set()
        while todo:
            off = todo.pop()
            if off in seen:
                raise ValueError('directory loop')
            seen.add(off)
            if off + 14 > len(t):
                raise ValueError('bad directory entry')
            left, right, first, length, attr, nl = struct.unpack_from('<HHIIBB', t, off)
            if left == 0xFFFF and right == 0xFFFF:
                continue
            name = t[off + 14:off + 14 + nl].decode('latin-1')
            if not name or '/' in name or '\\' in name or ':' in name or name in ('.', '..'):
                raise ValueError(f'unsafe name {name!r}')
            out.append((name, first, length, bool(attr & 0x10)))
            for c in (left, right):
                if c:
                    todo.append(c * 4)
        return out

    def walk(self):
        stack = [(PurePosixPath(), *self.root)]
        tables = set()  # a real disc never reuses a directory table; a broken one could loop forever
        while stack:
            prefix, sector, size = stack.pop()
            if size:  # empty directories have no table (and may all point at the same sector)
                if sector in tables:
                    raise ValueError('directory loop')
                tables.add(sector)
            for name, first, length, is_dir in self.directory(sector, size):
                rel = prefix / name
                if is_dir:
                    stack.append((rel, first, length))
                else:
                    yield rel.as_posix(), first, length

    def copy(self, first, length, out, h=None):
        off, left = self.base + first * SECTOR, length
        while left:
            d = self.read(off, min(left, 8 * 1024 * 1024))
            out.write(d)
            if h:
                h.update(d)
            off += len(d)
            left -= len(d)


def sha256_file(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for d in iter(lambda: f.read(8 * 1024 * 1024), b''):
            h.update(d)
    return h.hexdigest()


def pick_iso():
    try:
        import tkinter, tkinter.filedialog
        root = tkinter.Tk()
        root.withdraw()
        p = tkinter.filedialog.askopenfilename(
            title='Star Wars: Obi-Wan (USA) - ISO', filetypes=[('Xbox ISO', '*.iso'), ('*', '*.*')])
        root.destroy()
        if p:
            return Path(p)
    except Exception:
        pass
    p = input('Path to your ISO / Ruta de tu ISO: ').strip().strip('"')
    return Path(p) if p else None


def find_vs():
    vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / \
        'Microsoft Visual Studio' / 'Installer' / 'vswhere.exe'
    if vswhere.exists():
        r = subprocess.run([str(vswhere), '-version', '[17.0,18.0)', '-products', '*', '-requires',
                            'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
                            'Microsoft.VisualStudio.Component.VC.CMake.Project',
                            '-property', 'installationPath'], capture_output=True, text=True)
        for line in r.stdout.splitlines():
            if line.strip() and (Path(line.strip()) / 'VC/Auxiliary/Build/vcvars64.bat').exists():
                return line.strip()
    return None


def open_iso(iso, target):
    """Open the ISO and check its default.xbe is the supported edition. Returns (disc, files)."""
    if not iso or not iso.is_file():
        fail('No ISO given.', 'No has elegido ninguna ISO.')
    try:
        disc = Xdvdfs(iso)
        files = list(disc.walk())
    except (ValueError, OSError) as e:
        fail(f'{iso.name} is not an Xbox game image ({e}).', f'{iso.name} no es una imagen de Xbox ({e}).')
    entry = [f for f in files if f[0].casefold() == target['executable']]
    if not entry:
        fail('default.xbe is not in that ISO.', 'Esa ISO no tiene default.xbe.')
    h = hashlib.sha256()

    class Sink:
        def write(self, d):
            pass
    disc.copy(entry[0][1], entry[0][2], Sink(), h)
    if h.hexdigest() != target['sha256']:
        fail('This ISO is not the supported edition. For now only Star Wars: Obi-Wan (USA) works '
             f'(default.xbe SHA-256 {target["sha256"]}).',
             'Esta ISO no es la edición compatible. De momento solo funciona Star Wars: Obi-Wan (USA).')
    return disc, files


def run_cmd(script, env, what):
    say(f'{what}: running tools\\{script} ...')
    # Relative path + cwd: cmd.exe would expand a literal %...% in an absolute path.
    r = subprocess.run(['cmd', '/d', '/c', f'tools\\{script}'], cwd=ROOT, env=env)
    if r.returncode:
        fail(f'{what} failed (tools\\{script}, code {r.returncode}).',
             f'Ha fallado: {what} (tools\\{script}, código {r.returncode}).')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--iso', type=Path, help='your Star Wars: Obi-Wan (USA) ISO')
    ap.add_argument('--regenerar', action='store_true', help='translate the game code again even if it exists')
    ap.add_argument('--sin-compilar', action='store_true', help='only extract and translate, do not compile')
    ap.add_argument('--comprobar-iso', action='store_true', help='only check that --iso is the supported edition')
    args = ap.parse_args()
    target = json.loads((TITLE / 'target.json').read_text())
    if args.comprobar_iso:
        disc, _ = open_iso(args.iso, target)
        disc.f.close()
        say('ISO OK: Star Wars: Obi-Wan (USA).', 'ISO correcta: Star Wars: Obi-Wan (USA).')
        return

    # 1. tools
    if os.name != 'nt':
        fail('Windows only.', 'Solo Windows.')
    if sys.version_info < (3, 10):
        fail('Python 3.10 or newer is needed.', 'Hace falta Python 3.10 o más nuevo.')
    try:
        import capstone  # noqa: F401
    except ImportError:
        say('The Python package "capstone" is missing. Install it with:',
            'Falta el paquete de Python "capstone". Instálalo con:')
        say(f'    "{sys.executable}" -m pip install capstone==5.0.9')
        sys.exit(1)
    vs = find_vs()
    if not vs:
        fail('Visual Studio 2022 (or its Build Tools) with "Desktop development with C++" was not found.',
             'No se encuentra Visual Studio 2022 (o sus Build Tools) con "Desarrollo para el escritorio con C++".')
    say(f'Visual Studio: {vs}')

    # 2. game files
    xbe = GAME / target['executable']
    if xbe.exists() and sha256_file(xbe) == target['sha256'] and (GAME / 'EXTRACCION.json').exists():
        say(f'Game files already extracted in {GAME}', f'Los archivos del juego ya están en {GAME}')
    else:
        iso = args.iso or pick_iso()
        disc, files = open_iso(iso, target)
        total = sum(f[2] for f in files)
        free = shutil.disk_usage(ROOT).free
        if free < total + (NEED_GB - 2.1) * 1e9:
            fail(f'Not enough disk space: {NEED_GB:.0f} GB needed, {free / 1e9:.1f} GB free.',
                 f'No hay espacio: hacen falta {NEED_GB:.0f} GB y hay {free / 1e9:.1f} GB libres.')
        say(f'Extracting {len(files)} files ({total / 1e9:.1f} GB) to {GAME} ...',
            f'Extrayendo {len(files)} archivos ({total / 1e9:.1f} GB) en {GAME} ...')
        done = []
        for i, (rel, first, length) in enumerate(files, 1):
            dest = GAME.joinpath(*rel.split('/'))
            dest.parent.mkdir(parents=True, exist_ok=True)
            tmp = dest.with_name(dest.name + '.partial')
            with tmp.open('wb') as out:
                disc.copy(first, length, out)
            tmp.replace(dest)
            done.append({'path': rel, 'size': length})
            if i % 200 == 0:
                say(f'  {i}/{len(files)}')
        disc.f.close()
        (GAME / 'EXTRACCION.json').write_text(json.dumps(
            {'iso_name': iso.name, 'files': done,
             'note': 'Extracted from your own copy. Do not share or upload it.'}, indent=1) + '\n')

    # 3. game code -> C
    env = dict(os.environ, XBOXRECOMP_DIR=str(SDK), VS=vs, PYTHONDONTWRITEBYTECODE='1')
    installer = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / \
        'Microsoft Visual Studio' / 'Installer'
    if installer.is_dir():  # vcvars64.bat calls vswhere.exe from PATH
        env['PATH'] = str(installer) + os.pathsep + env.get('PATH', '')
    if args.regenerar or not (GEN / 'recomp_observed_entries.c').exists():
        if GEN.exists():
            shutil.rmtree(GEN)
        say('Translating the game code to C (several minutes) ...',
            'Traduciendo el código del juego a C (varios minutos) ...')
        r = subprocess.run([sys.executable, '-B', str(TITLE / 'tools' / 'generate_obiwan.py'),
                            '--game-dir', str(GAME)], cwd=TITLE, env=env)
        if r.returncode:
            fail('Code generation failed (see work\\obiwan-claude-completo\\build\\obiwan-analysis\\*.log).',
                 'Ha fallado la generación (mira work\\obiwan-claude-completo\\build\\obiwan-analysis\\*.log).')
    else:
        say('Game code already translated (use --regenerar to redo it).',
            'El código del juego ya está traducido (usa --regenerar para repetirlo).')

    if args.sin_compilar:
        say('Stopped before compiling (--sin-compilar).', 'Parado antes de compilar (--sin-compilar).')
        return
    # 4. build
    sdl = ROOT / 'upstream' / 'SDL2' / 'CMakeLists.txt'
    if sdl.exists() and not (ROOT / 'build' / 'sdl2-win').exists():
        run_cmd('build_sdl2.cmd', env, 'SDL2 (generic gamepads)')
    elif not sdl.exists():
        say('SDL2 not found in upstream\\SDL2: only XInput (Xbox-style) gamepads will work.',
            'No hay SDL2 en upstream\\SDL2: solo funcionarán los mandos XInput (tipo Xbox).')
    run_cmd('build_claude_completo9.cmd', env, 'Game / Juego')
    run_cmd('build_lanzador_obiwan_completo9.cmd', env, 'Launcher / Lanzador')
    run_cmd('build_configurador_obiwan.cmd', env, 'Settings / Configurador')
    say('\nDone. Start the game with "Jugar Obi-Wan.exe"; settings in "Configurar Obi-Wan.exe".',
        'Listo. Abre el juego con "Jugar Obi-Wan.exe"; los ajustes están en "Configurar Obi-Wan.exe".')


if __name__ == '__main__':
    main()
