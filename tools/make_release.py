# Builds dist/TTTRecomp-v<version>-win64.zip: TTTRecomp-Setup.exe + port/ (the files tracked by git)
import argparse, os, re, shutil, subprocess, sys, zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def git(*a):
    return subprocess.run(['git', *a], cwd=REPO, capture_output=True, text=True, check=True).stdout


def version():
    s = open(os.path.join(REPO, 'tools', 'setup', 'setup.cpp'), encoding='utf-8').read()
    return re.search(r'VERSION = "([^"]+)"', s).group(1)


def find_bin(extra):
    home = os.path.expanduser('~')
    dirs = ([extra] if extra else []) + [None, r'C:\msys64\ucrt64\bin', r'C:\msys64\mingw64\bin', r'C:\mingw64\bin',
                                        os.path.join(home, 'Games', 'TTTRecomp', 'toolchain', 'mingw64', 'bin')]
    for d in dirs:
        g = shutil.which('g++', path=d) if d else shutil.which('g++')
        w = shutil.which('windres', path=d) if d else shutil.which('windres')
        if g and w:
            return os.path.dirname(g)
    sys.exit('no encuentro g++ y windres; usa --gcc "C:\\ruta\\a\\mingw64\\bin"')


def build_setup(out, bindir):
    env = dict(os.environ, PATH=bindir + os.pathsep + os.environ.get('PATH', ''))
    gen = os.path.join(REPO, 'generated')
    os.makedirs(gen, exist_ok=True)
    res = os.path.join(gen, 'setup_res.o')
    subprocess.run([os.path.join(bindir, 'windres'), os.path.join('tools', 'setup', 'setup.rc'), '-O', 'coff', '-o', res], cwd=REPO, check=True, env=env)
    libs = ['-lopengl32', '-lglu32', '-lgdi32', '-lwinhttp', '-lbcrypt', '-lole32', '-luuid', '-lshell32', '-lcomdlg32']
    subprocess.run([os.path.join(bindir, 'g++'), '-O2', '-std=c++17', '-municode', '-mwindows', '-static', '-s', '-o', out,
                    os.path.join('tools', 'setup', 'setup.cpp'), res, *libs], cwd=REPO, check=True, env=env)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--setup', help='use this TTTRecomp-Setup.exe instead of compiling it')
    ap.add_argument('--gcc', help='folder with g++ and windres')
    ap.add_argument('--dirty', action='store_true', help='allow uncommitted changes')
    a = ap.parse_args()
    if git('status', '--porcelain').strip() and not a.dirty:
        sys.exit('hay cambios sin commit; haz commit primero o usa --dirty')
    files = [f for f in git('ls-files', '-z').split('\0') if f and os.path.isfile(os.path.join(REPO, f))]
    bad = [f for f in files if re.search(r'\.(iso|cso|elf|exe|dll|o)$|^(game|generated|assets|mods|server|library_cache)/', f, re.I)]
    if bad:
        sys.exit('archivos que no deben publicarse:\n  ' + '\n  '.join(bad))
    for need in ('tools/build_from_iso.py', 'games/ttt/fonts/sz_name/font.fnt', 'LICENSE', 'README.md'):
        if need not in files:
            sys.exit('falta en git: ' + need)

    ver = version()
    dist = os.path.join(REPO, 'dist')
    os.makedirs(dist, exist_ok=True)
    exe = os.path.join(dist, 'TTTRecomp-Setup.exe')
    if a.setup:
        shutil.copyfile(a.setup, exe)
    else:
        bindir = find_bin(a.gcc)
        print('compilando el asistente con ' + bindir)
        build_setup(exe, bindir)

    out = os.path.join(dist, 'TTTRecomp-v%s-win64.zip' % ver)
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.write(exe, 'TTTRecomp-Setup.exe')
        for f in files:
            z.write(os.path.join(REPO, f), 'port/' + f)
    print('%d archivos del port + asistente' % len(files))
    print('listo: %s (%.1f MB)' % (out, os.path.getsize(out) / 1048576))


if __name__ == '__main__':
    main()
