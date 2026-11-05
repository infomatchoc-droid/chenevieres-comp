"""Source/layouts/*.json -> Source/Layouts.inc (données des façades pour le plugin)."""
import glob, json, math, os

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, '..', '..', 'Source'))


def f(x):
    return f'{float(x):.3f}f'


def colour(hexs):
    return '0xff' + hexs.lstrip('#')


out = ['// Généré par tools/facades/gen_layouts.py — ne pas modifier à la main', '']
names = []
for path in sorted(glob.glob(os.path.join(SRC, 'layouts', '*.json'))):
    lay = json.load(open(path))
    m = lay['model']
    names.append(m)
    out.append(f'static PanelDef make_{m}()')
    out.append('{')
    out.append('    PanelDef p;')
    out.append(f'    p.model = "{m}"; p.width = {lay["size"][0]}; p.height = {lay["size"][1]};')
    for c in lay['controls']:
        t = c['type']
        if t == 'knob':
            cal = ', '.join('{ %s, %s }' % (f(a), f(b)) for a, b in c['cal'])
            auto = f(c['auto_angle']) if c.get('auto_angle') is not None else 'NAN'
            steps = ', '.join(f(v) for v in c.get('steps', []))
            out.append('    p.knobs.push_back ({ "%s", "%s", %s, %s, %s, %d, { %s }, %s, %s, %s, %s, %s, %s, %s, %s, "%s", { %s } });' % (
                c['id'], c['param'], f(c['cx']), f(c['cy']), f(c['rs']), c['sprite_w'], cal,
                'true' if c['log'] else 'false', f(c['map'][0]), f(c['map'][1]), auto,
                f(c.get('ptr_in', c['rs'] * 0.04)), f(c['ptr_len']), f(c['ptr_w']), colour(c['ptr_colour']),
                c['fmt'].replace('%', '%%').replace('{:', '%').replace('}', ''), steps))
        elif t == 'buttons':
            vals = ', '.join(f(v) for v in c['values'])
            rects = ', '.join('{ %d, %d, %d, %d }' % tuple(r) for r in c['rects'])
            also = ', '.join('{ "%s", %s }' % (pid, f(v)) for pid, v in c.get('also', []))
            nan = lambda v: 'NAN' if v is None else f(v)
            out.append('    p.buttons.push_back ({ "%s", "%s", { %s }, { %s }, %d, %s, { %s }, %s, %s });' % (
                c['id'], c['param'], vals, rects, c['glow'], 'true' if c.get('toggle') else 'false', also,
                nan(c.get('off')), nan(c.get('lit_above'))))
        elif t == 'ledbar':
            leds = ', '.join('{ { %d, %d, %d, %d }, %s }' % (x, y, w, h, f(v)) for x, y, w, h, v in c['leds'])
            out.append('    p.ledbars.push_back ({ "%s", %d, { %s } });' % (c['id'], c['channel'], leds))
        elif t == 'led':
            r = c['rect']
            out.append('    p.leds.push_back ({ "%s", "%s", { %d, %d, %d, %d } });' % (c['id'], c['param'], r[0], r[1], r[2] - r[0], r[3] - r[1]))
        elif t == 'needle':
            sc = ', '.join('{ %s, %s }' % (f(a), f(b)) for a, b in c['scale'])
            fc = c['face']
            out.append('    p.needles.push_back ({ "%s", %d, %s, %s, %s, { %d, %d, %d, %d }, { %s }, "%s" });' % (
                c['id'], c['channel'], f(c['pivot'][0]), f(c['pivot'][1]), f(c['len']),
                fc[0], fc[1], fc[2] - fc[0], fc[3] - fc[1], sc, c['source']))
    out.append('    return p;')
    out.append('}')
    out.append('')

out.append('static const PanelDef* findPanel (const juce::String& model)')
out.append('{')
out.append('    static const std::vector<PanelDef> all { %s };' % ', '.join(f'make_{m}()' for m in names))
out.append('    for (auto& p : all) if (model == p.model) return &p;')
out.append('    return nullptr;')
out.append('}')
open(os.path.join(SRC, 'Layouts.inc'), 'w').write('\n'.join(out) + '\n')
print('Layouts.inc :', ', '.join(names))
