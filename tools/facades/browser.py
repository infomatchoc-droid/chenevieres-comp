from PIL import Image, ImageDraw, ImageFilter
import compose
base = Image.open('preview_a2500.png').convert('RGBA')
base.alpha_composite(Image.new('RGBA', base.size, (0, 0, 0, 130)))
d = ImageDraw.Draw(base)
gold = (232, 163, 58)
models = [('fet76','FET-76','FET'),('opto2a','Opto-2A','OPTIQUE'),('opto3a','Opto-3A','OPTIQUE'),('tubeoneb','Tube-One B','OPTIQUE À LAMPES'),
 ('buscompg','Bus-Comp G','VCA'),('a2500','A-2500','VCA'),('db160x','DB-160X','VCA'),('pressorx','Pressor-X','VCA'),
 ('fair670','Fair-670','VARI-MU'),('varimuv','Vari-Mu V','VARI-MU'),('shadowmaster','Shadow Master','HYBRIDE'),('n3309','N-3309','PONT DE DIODES'),
 ('zenerx','Zener-X','ZENER'),('procx','Pro-CX','NUMÉRIQUE'),('rcren','RC-Ren','NUMÉRIQUE'),('rcvox','RC-Vox','VOIX')]
x0, y0 = 94, 104
tw, th = 288, 96
gx, gy = 26, 12
cols = 4
x1 = x0 + 30 * 2 + cols * tw + (cols - 1) * gx
y1 = y0 + 24 + 4 * (th + 60) + 3 * gy + 14
sh = Image.new('RGBA', base.size, (0, 0, 0, 0))
ImageDraw.Draw(sh).rounded_rectangle([x0 + 6, y0 + 10, x1 + 6, y1 + 10], 16, fill=(0, 0, 0, 220))
base.alpha_composite(sh.filter(ImageFilter.GaussianBlur(14)))
d.rounded_rectangle([x0, y0, x1, y1], 16, fill=(20, 18, 16, 252), outline=(200, 137, 47), width=2)
for i, (key, name, fam) in enumerate(models):
    cx = x0 + 30 + (i % cols) * (tw + gx); cy = y0 + 24 + (i // cols) * (th + 60 + gy)
    im = Image.open(f'src/{key}.png').convert('RGBA')
    im = im.resize((tw, int(im.height * tw / im.width)), Image.LANCZOS)
    if im.height > th:
        o = (im.height - th) // 2; im = im.crop((0, o, tw, o + th))
    sel = key == 'a2500'
    if sel:
        d.rounded_rectangle([cx - 7, cy - 7, cx + tw + 7, cy + th + 56], 10, fill=(60, 44, 22), outline=gold, width=3)
    base.alpha_composite(im, (cx, cy + (th - im.height) // 2))
    d.text((cx, cy + th + 8), name, font=compose.font(25, '700'), fill=(255, 214, 140) if sel else (232, 224, 210))
    d.text((cx, cy + th + 36), fam, font=compose.font(17, '600'), fill=(150, 140, 128))
base.convert('RGB').save('preview_browser.png')
