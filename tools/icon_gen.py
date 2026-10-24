#!/usr/bin/env python3
"""Gera o icone do Zandatsu (Pillow): uma lamina escura com o fio em brasa e as
letras MGR em blocos largos e metalicos (como o logotipo antigo do Metal Gear), cortadas a
meio por um golpe brilhante, com a metade de cima ligeiramente deslocada.

Saidas (app/src/main/res):
  drawable-*/ic_launcher_foreground.png   camada da frente do icone adaptativo (108 dp)
  drawable-*/ic_launcher_monochrome.png   silhueta para os icones tematicos (Android 13+)
  mipmap-*/ic_launcher.png, ic_launcher_round.png   icones antigos (fundo incluido)
  drawable/ic_launcher_background.xml     fundo (vetor)

Uso: python3 tools/icon_gen.py
"""
import math
import os

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "app", "src", "main", "res")
S = 1728  # 108 dp x 16: desenho grande, reduzido no fim
U = S / 108.0  # pixeis por dp


def dp(v):
    return v * U


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(len(a)))


def vertical_gradient(size, top, bottom):
    w, h = size
    g = Image.new("RGBA", (1, h))
    for y in range(h):
        g.putpixel((0, y), lerp(top, bottom, y / max(1, h - 1)))
    return g.resize((w, h))


# ------------------------------------------------------------------ letras
def letters_mask():
    """MGR em blocos largos (grelha de 1 dp), centrado na zona segura do icone."""
    m = Image.new("L", (S, S), 0)
    d = ImageDraw.Draw(m)
    h = dp(18)      # altura das letras
    t = dp(4.6)     # espessura do traco
    w = dp(15)      # largura de cada letra
    gap = dp(2.4)
    total = 3 * w + 2 * gap
    x0 = S / 2 - total / 2
    y0 = S / 2 - dp(4)  # quase ao centro (dentro da zona segura de 66 dp)

    def rect(x1, y1, x2, y2):
        d.rectangle([x1, y1, x2, y2], fill=255)

    def poly(pts):
        d.polygon(pts, fill=255)

    # M
    x = x0
    rect(x, y0, x + t, y0 + h)
    rect(x + w - t, y0, x + w, y0 + h)
    poly([(x, y0), (x + t * 1.1, y0), (x + w / 2 + t * 0.35, y0 + h * 0.58), (x + w / 2 - t * 0.35, y0 + h * 0.58 + t * 0.9)])
    poly([(x + w, y0), (x + w - t * 1.1, y0), (x + w / 2 - t * 0.35, y0 + h * 0.58), (x + w / 2 + t * 0.35, y0 + h * 0.58 + t * 0.9)])
    # G (quadrado)
    x = x0 + w + gap
    rect(x, y0, x + w, y0 + t)                    # topo
    rect(x, y0, x + t, y0 + h)                    # esquerda
    rect(x, y0 + h - t, x + w, y0 + h)            # base
    rect(x + w - t, y0 + h * 0.47, x + w, y0 + h)  # direita em baixo
    rect(x + w * 0.45, y0 + h * 0.47, x + w, y0 + h * 0.47 + t * 0.8)  # barra do meio
    # R
    x = x0 + 2 * (w + gap)
    rect(x, y0, x + t, y0 + h)
    rect(x, y0, x + w - t * 0.4, y0 + t)
    rect(x + w - t, y0 + t * 0.4, x + w, y0 + h * 0.52)
    rect(x, y0 + h * 0.48, x + w - t * 0.4, y0 + h * 0.48 + t)
    poly([(x + w * 0.38, y0 + h * 0.48 + t), (x + w * 0.38 + t * 1.15, y0 + h * 0.48 + t * 0.2),
          (x + w, y0 + h), (x + w - t * 1.25, y0 + h)])
    # Cantos chanfrados (cortes a 45 graus, como placas de metal).
    return m, y0, h


def stripes(mask, y0, h):
    """Duas linhas finas horizontais em cada letra (as faixas do logotipo antigo)."""
    d = ImageDraw.Draw(mask)
    for f in (0.80,):
        y = y0 + h * f
        d.rectangle([0, y - dp(0.35), S, y + dp(0.35)], fill=0)
    return mask


# ------------------------------------------------------------------ lamina
def blade_layers():
    """Lamina (silhueta) e o fio em brasa, em diagonal de baixo-esquerda para cima-direita."""
    body = Image.new("L", (S, S), 0)
    edge = Image.new("L", (S, S), 0)
    db = ImageDraw.Draw(body)
    de = ImageDraw.Draw(edge)
    ang = math.radians(-40)
    ux, uy = math.cos(ang), math.sin(ang)          # ao longo da lamina
    nx, ny = -uy, ux                               # perpendicular (para baixo-direita)
    cx, cy = S / 2 + dp(1), S / 2 - dp(10)
    L1, L2 = dp(40), dp(34)                        # tras (cabo), frente (ponta)
    wb = dp(3.0)                                   # meia largura

    def P(a, b):
        return (cx + ux * a + nx * b, cy + uy * a + ny * b)

    # Corpo da lamina: das costas ao fio, com ponta em bisel (estilo katana/HF blade).
    db.polygon([P(-L1 * 0.55, -wb), P(L2 - dp(6), -wb), P(L2, wb * 0.2), P(L2 - dp(2), wb),
                P(-L1 * 0.55, wb)], fill=255)
    # Guarda e cabo.
    db.polygon([P(-L1 * 0.55, -wb * 2.2), P(-L1 * 0.55 - dp(2.2), -wb * 2.2),
                P(-L1 * 0.55 - dp(2.2), wb * 2.2), P(-L1 * 0.55, wb * 2.2)], fill=255)
    db.polygon([P(-L1 * 0.55 - dp(2.2), -wb * 0.8), P(-L1, -wb * 0.8), P(-L1, wb * 0.8),
                P(-L1 * 0.55 - dp(2.2), wb * 0.8)], fill=255)
    # Fio (lado de baixo) em brasa.
    de.polygon([P(-L1 * 0.5, wb * 0.45), P(L2 - dp(2), wb * 0.45), P(L2, wb * 0.2), P(L2 - dp(2), wb),
                P(-L1 * 0.5, wb)], fill=255)
    return body, edge


def compose_foreground(with_color=True):
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    body, edge = blade_layers()
    letters, y0, h = letters_mask()
    letters = stripes(letters, y0, h)

    # Corte: linha um pouco inclinada que atravessa as letras a meio.
    cut_y = y0 + h * 0.46
    cut_a = math.radians(-6)
    def cut_line(img_d, width, fill):
        x1, x2 = dp(10), S - dp(10)
        y1 = cut_y + (x1 - S / 2) * math.tan(cut_a)
        y2 = cut_y + (x2 - S / 2) * math.tan(cut_a)
        img_d.line([(x1, y1), (x2, y2)], fill=fill, width=int(width))

    # As letras cortadas: metade de cima deslocada ao longo do corte.
    upper_clip = Image.new("L", (S, S), 0)
    ImageDraw.Draw(upper_clip).polygon(
        [(0, 0), (S, 0), (S, cut_y + (S / 2) * math.tan(cut_a)), (0, cut_y - (S / 2) * math.tan(cut_a))], fill=255)
    lower_clip = ImageChops.invert(upper_clip)
    upper = ImageChops.multiply(letters, upper_clip)
    lower = ImageChops.multiply(letters, lower_clip)
    shift = (int(dp(1.6)), int(-dp(0.5)))
    upper = ImageChops.offset(upper, shift[0], shift[1])
    gapline = Image.new("L", (S, S), 0)
    cut_line(ImageDraw.Draw(gapline), dp(0.9), 255)
    letters_cut = ImageChops.subtract(ImageChops.lighter(upper, lower), gapline)

    if not with_color:
        sil = ImageChops.lighter(body, letters_cut)
        out = Image.new("RGBA", (S, S), (255, 255, 255, 0))
        out.putalpha(sil)
        return out

    # Sombra suave por tras de tudo (separa do fundo).
    shadow = ImageChops.lighter(body, letters_cut).filter(ImageFilter.GaussianBlur(dp(1.6)))
    sh = Image.new("RGBA", (S, S), (0, 0, 0, 255))
    sh.putalpha(shadow.point(lambda v: int(v * 0.7)))
    img.alpha_composite(sh)

    # Lamina: aco escuro com brilho frio nas costas.
    steel = vertical_gradient((S, S), (58, 64, 74, 255), (18, 20, 24, 255))
    blade = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    blade.paste(steel, (0, 0), body)
    img.alpha_composite(blade)
    # Fio em brasa com brilho.
    glow = edge.filter(ImageFilter.GaussianBlur(dp(2.2)))
    gl = Image.new("RGBA", (S, S), (227, 38, 58, 255))
    gl.putalpha(glow.point(lambda v: min(255, int(v * 1.6))))
    img.alpha_composite(gl)
    hot = Image.new("RGBA", (S, S), (255, 120, 90, 255))
    hot.putalpha(edge)
    img.alpha_composite(hot)

    # Letras: metal frio (claro em cima, aco em baixo), contorno escuro.
    outline = letters_cut.filter(ImageFilter.MaxFilter(int(dp(0.9)) | 1))
    ol = Image.new("RGBA", (S, S), (6, 7, 9, 255))
    ol.putalpha(outline)
    img.alpha_composite(ol)
    metal = vertical_gradient((S, S), (0, 0, 0, 0), (0, 0, 0, 0))
    grad = Image.new("RGBA", (S, S))
    top, bottom = int(y0 - dp(1)), int(y0 + h + dp(1))
    band = vertical_gradient((S, bottom - top), (240, 243, 247, 255), (122, 132, 145, 255))
    grad.paste(band, (0, top))
    metal.paste(grad, (0, 0), letters_cut)
    img.alpha_composite(metal)

    # O golpe: brilho vermelho largo, nucleo branco fino.
    g = Image.new("L", (S, S), 0)
    cut_line(ImageDraw.Draw(g), dp(2.2), 255)
    g = g.filter(ImageFilter.GaussianBlur(dp(1.8)))
    red = Image.new("RGBA", (S, S), (227, 38, 58, 255))
    red.putalpha(g.point(lambda v: min(255, int(v * 1.5))))
    img.alpha_composite(red)
    core = Image.new("L", (S, S), 0)
    cut_line(ImageDraw.Draw(core), dp(0.45), 255)
    wh = Image.new("RGBA", (S, S), (255, 245, 240, 255))
    wh.putalpha(core)
    img.alpha_composite(wh)
    return img


def background():
    bg = Image.new("RGBA", (S, S), (12, 13, 16, 255))
    glow = Image.new("L", (S, S), 0)
    ImageDraw.Draw(glow).ellipse([S * 0.1, S * 0.45, S * 0.9, S * 1.25], fill=255)
    glow = glow.filter(ImageFilter.GaussianBlur(S * 0.12))
    red = Image.new("RGBA", (S, S), (110, 16, 28, 255))
    red.putalpha(glow.point(lambda v: int(v * 0.55)))
    bg.alpha_composite(red)
    return bg


BACKGROUND_XML = """<?xml version="1.0" encoding="utf-8"?>
<!-- Fundo do icone Zandatsu: grafite com um brilho vermelho em baixo. Gerado por tools/icon_gen.py. -->
<vector xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:aapt="http://schemas.android.com/aapt"
    android:width="108dp" android:height="108dp"
    android:viewportWidth="108" android:viewportHeight="108">
    <path android:pathData="M0,0 L108,0 L108,108 L0,108Z" android:fillColor="#FF0C0D10" />
    <path android:pathData="M0,0 L108,0 L108,108 L0,108Z">
        <aapt:attr name="android:fillColor">
            <gradient android:type="radial" android:centerX="54" android:centerY="92" android:gradientRadius="62">
                <item android:offset="0.00" android:color="#8C6E101C" />
                <item android:offset="0.45" android:color="#406E101C" />
                <item android:offset="1.00" android:color="#006E101C" />
            </gradient>
        </aapt:attr>
    </path>
</vector>
"""

DENSITIES = {"mdpi": 1, "hdpi": 1.5, "xhdpi": 2, "xxhdpi": 3, "xxxhdpi": 4}


def main():
    fg = compose_foreground(True)
    mono = compose_foreground(False)
    bg = background()
    full = bg.copy()
    full.alpha_composite(fg)
    for name, k in DENSITIES.items():
        px = int(108 * k)
        ddir = os.path.join(ROOT, "drawable-" + name)
        os.makedirs(ddir, exist_ok=True)
        fg.resize((px, px), Image.LANCZOS).save(os.path.join(ddir, "ic_launcher_foreground.png"))
        mono.resize((px, px), Image.LANCZOS).save(os.path.join(ddir, "ic_launcher_monochrome.png"))
        # Icones antigos (48 dp): a zona central de 72 dp do icone adaptativo.
        size = int(48 * k)
        crop = full.crop((int(S * 18 / 108), int(S * 18 / 108), int(S * 90 / 108), int(S * 90 / 108)))
        legacy = crop.resize((size, size), Image.LANCZOS)
        mask = Image.new("L", (size * 4, size * 4), 0)
        ImageDraw.Draw(mask).rounded_rectangle([0, 0, size * 4 - 1, size * 4 - 1], radius=size * 4 * 0.22, fill=255)
        sq = legacy.copy()
        sq.putalpha(mask.resize((size, size), Image.LANCZOS))
        mdir = os.path.join(ROOT, "mipmap-" + name)
        os.makedirs(mdir, exist_ok=True)
        sq.save(os.path.join(mdir, "ic_launcher.png"))
        rmask = Image.new("L", (size * 4, size * 4), 0)
        ImageDraw.Draw(rmask).ellipse([0, 0, size * 4 - 1, size * 4 - 1], fill=255)
        rnd = legacy.copy()
        rnd.putalpha(rmask.resize((size, size), Image.LANCZOS))
        rnd.save(os.path.join(mdir, "ic_launcher_round.png"))
    with open(os.path.join(ROOT, "drawable", "ic_launcher_background.xml"), "w") as f:
        f.write(BACKGROUND_XML)
    # Previa para conferir (nao vai para o APK).
    prev = full.resize((512, 512), Image.LANCZOS)
    prev.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), "icon_preview.png"))
    print("icone gerado")


if __name__ == "__main__":
    main()
