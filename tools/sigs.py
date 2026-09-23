"""Infer Turbo C cdecl signatures from the listing: params from [bp+N] usage, arg bytes from caller cleanup, return from dx use."""
import re, sys, collections
L = sys.argv[1]
funcs = collections.OrderedDict(); cur = None; body = collections.defaultdict(list)
for line in open(L):
    if line.startswith('; sub_'):
        cur = line.split()[1]; funcs[cur] = line
    elif cur and line.startswith('  '):
        body[cur].append(line.rstrip())
argbytes = collections.defaultdict(collections.Counter)
dxuse = collections.Counter(); calls = collections.Counter()
for f, lines in body.items():
    for i, l in enumerate(lines):
        m = re.search(r'\s(l?call)\s+(sub_\w+)', l)
        if not m: continue
        tgt = m.group(2); calls[tgt] += 1
        nb = 0; j = i + 1
        while j < len(lines) and j < i + 4:
            n = lines[j]
            if re.search(r'add\s+sp, (0x[0-9a-f]+|\d+)', n):
                nb = int(re.search(r'add\s+sp, (0x[0-9a-f]+|\d+)', n).group(1), 0); j += 1; break
            if re.search(r'\s(inc\s+sp|pop\s+cx)$', n): nb += 2; j += 1; continue
            break
        argbytes[tgt][nb] += 1
        # dx used after call before being written?
        for n in lines[j-0:j+6] if nb else lines[i+1:i+7]:
            if re.search(r'\b(mov|or|push|cmp|xchg)\s+.*\bdx\b', n) and not re.search(r'mov\s+dx,', n):
                dxuse[tgt] += 1; break
            if re.search(r'mov\s+dx,|cwd|\bmul\b|\bdiv\b|idiv|imul', n): break
out = []
for f, hdr in funcs.items():
    lines = body[f]
    near = False
    far = any(re.search(r'\sretf', l) for l in lines)
    uses_bp = len(lines) > 1 and 'push    bp' in lines[0] and 'mov     bp, sp' in lines[1]
    base = 6 if far else 4
    ptrs = set(); words = set(); maxo = 0
    for l in lines:
        for m in re.finditer(r'\[bp \+ (0x[0-9a-f]+|\d+)\]', l):
            o = int(m.group(1), 0)
            if o < base: continue
            if re.search(r'l[ed]s\s+\w+, ptr \[bp \+', l): ptrs.add(o); maxo = max(maxo, o + 2)
            else:
                sz = 1 if 'byte ptr' in l else 2
                words.add(o); maxo = max(maxo, o + sz - 1)
    nb = argbytes[f].most_common(1)[0][0] if argbytes[f] else None
    nparam = max(maxo - base + 1, nb or 0) if uses_bp else (nb or 0)
    params = []; o = base
    while o < base + nparam:
        if o in ptrs: params.append('ptr'); o += 4
        else: params.append('w'); o += 2
    rets = 'long' if dxuse[f] >= 1 else 'int'
    out.append('%s %s %s(%s) callers=%d argbytes=%s' % (f, 'far' if far else 'near', rets, ','.join(params), calls[f], dict(argbytes[f])))
open(sys.argv[2], 'w').write('\n'.join(out) + '\n')
print(len(out))
