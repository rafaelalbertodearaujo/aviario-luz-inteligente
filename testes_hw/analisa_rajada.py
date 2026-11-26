import re, sys, os, glob, datetime

pasta = sys.argv[1]
tempos = {}
with open(os.path.join(pasta, 'tempos.txt')) as f:
    for l in f:
        k, t = l.split()
        h, m, s = t.split(':')
        tempos[int(k)] = int(h) * 3600 + int(m) * 60 + float(s)

reg = re.compile(r'^\s*0x[0-9A-Fa-f]{8}\s*:\s*((?:[0-9A-Fa-f]{2}\s*)+)$', re.M)
amostras = []  # (t, min, sp, raw, luz, duty, I, noite)
ks = sorted(tempos)
for k in ks:
    txt = open(os.path.join(pasta, 'r%02d.txt' % k), encoding='ascii', errors='ignore').read()
    linhas = [[int(x, 16) for x in m.group(1).split()] for m in reg.finditer(txt)]
    blocos = []
    cur = []
    for l in linhas:
        cur += l
        if len(cur) >= 44:
            blocos.append(cur[:44]); cur = cur[44:]
    n = len(blocos)
    t0 = tempos[k]
    dur = (tempos[k + 1] - t0) if (k + 1) in tempos else 2.5
    for i, b in enumerate(blocos):
        t = t0 + dur * i / max(n, 1)
        u16 = lambda o: b[o] + 256 * b[o + 1]
        s16 = lambda o: u16(o) - 65536 if u16(o) >= 32768 else u16(o)
        amostras.append((t, u16(0), b[6], u16(8), s16(10), u16(12), s16(18), b[4]))

# estados unicos consecutivos
uni = []
for a in amostras:
    if not uni or a[1:] != uni[-1][1:]:
        uni.append(a)
print('leituras', len(amostras), 'estados distintos', len(uni))

rampa = [a for a in uni if 300 <= a[1] <= 331]
if not rampa:
    print('rampa de amanhecer nao capturada; minutos vistos:', sorted(set(a[1] for a in uni))[:5], '...')
    sys.exit(0)
t0 = rampa[0][0]
print('t(s)  hora  sp  raw  luz  duty%  mA')
ant = None
rev = 0
for a in rampa:
    d = a[5] / 10.0
    seta = ''
    if ant is not None:
        if d < ant - 1e-9: seta = ' v'
    print('%5.2f %02d:%02d %3d %5d %4d %6.1f %3d%s' % (a[0] - t0, a[1] // 60, a[1] % 60, a[2], a[3], a[4], d, a[6], seta))
    ant = d
# quedas de duty entre estados vizinhos durante a rampa
quedas = [(rampa[i][0] - t0, rampa[i - 1][5] / 10.0, rampa[i][5] / 10.0) for i in range(1, len(rampa)) if rampa[i][5] < rampa[i - 1][5]]
print('quedas de duty na rampa:', len(quedas), 'de', len(rampa) - 1, 'passos')
if quedas:
    print('maior queda (permil):', max((p - q) * 10 for _, p, q in quedas))
