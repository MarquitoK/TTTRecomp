import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, 'games', 'ttt')


def sections(name, game=GAME):
    out, cur = [], None
    path = os.path.join(game, name)
    for n, raw in enumerate(open(path, encoding='utf-8'), 1):
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        if line.startswith('['):
            cur = (line[1:line.index(']')].split(), [])
            out.append(cur)
        elif cur is None:
            raise SystemExit('%s:%d: line outside a section' % (path, n))
        else:
            cur[1].append(line)
    return out


def section(name, key, game=GAME):
    for head, lines in sections(name, game):
        if head[0] == key:
            return lines
    raise SystemExit('%s: missing section [%s]' % (name, key))


def addresses(lines):
    return [int(w, 16) for l in lines for w in l.split()]


def option(args, new, old=(), default=None):
    for k in (new,) + tuple(old):
        if k in args:
            return args[args.index(k) + 1]
    return default


def flag(args, new, old=()):
    return any(k in args for k in (new,) + tuple(old))
