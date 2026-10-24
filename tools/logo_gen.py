#!/usr/bin/env python3
"""Gera a logo do Zandatsu para o README (SVG): "ZANDATSU" em letras angulosas desenhadas aqui (poligonos, sem fonte),
cortadas por um golpe de lamina vermelho, com a metade de cima deslocada ao longo do corte. Original: nada da logo
do Metal Gear nem arte da Konami.

Saidas: docs/assets/zandatsu-logo-dark.svg (para o tema escuro: letras claras) e zandatsu-logo-light.svg (tema
claro: letras pretas).

Uso: python3 tools/logo_gen.py
"""
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs", "assets")

# Letras numa grelha de 70 x 100, traco de 20, cantos chanfrados. Cada letra: lista de contornos (o 2.o e o furo).
GLYPHS = {
    "Z": [[(0, 0), (70, 0), (70, 18), (26, 82), (70, 82), (70, 100), (0, 100), (0, 82), (44, 18), (0, 18)]],
    "A": [[(0, 100), (0, 22), (22, 0), (70, 0), (70, 100), (50, 100), (50, 66), (20, 66), (20, 100)],
          [(20, 46), (50, 46), (50, 20), (30, 20), (20, 30)]],
    "N": [[(0, 100), (0, 0), (20, 0), (50, 60), (50, 0), (70, 0), (70, 100), (50, 100), (20, 40), (20, 100)]],
    "D": [[(0, 0), (48, 0), (70, 22), (70, 78), (48, 100), (0, 100)],
          [(20, 20), (40, 20), (50, 30), (50, 70), (40, 80), (20, 80)]],
    "T": [[(0, 0), (70, 0), (70, 20), (45, 20), (45, 100), (25, 100), (25, 20), (0, 20)]],
    "S": [[(70, 0), (12, 0), (0, 12), (0, 48), (12, 60), (50, 60), (50, 80), (0, 80), (0, 100), (58, 100),
           (70, 88), (70, 52), (58, 40), (20, 40), (20, 20), (70, 20)]],
    "U": [[(0, 0), (20, 0), (20, 80), (50, 80), (50, 0), (70, 0), (70, 86), (56, 100), (14, 100), (0, 86)]],
}

W, H = 760, 220
ADVANCE = 84            # 70 de letra + 14 de espaco
X0, Y0 = (W - (8 * 70 + 7 * 14)) / 2, 60
# O corte: de (20, 140) a (740, 82), a atravessar as letras um pouco acima e abaixo do meio.
CUT = ((20, 140), (740, 82))
SHIFT = (12, -6)        # metade de cima deslocada ao longo do corte


def word_path():
    d = []
    for i, ch in enumerate("ZANDATSU"):
        ox = X0 + i * ADVANCE
        for contour in GLYPHS[ch]:
            pts = " L".join(f"{ox + x:.1f},{Y0 + y:.1f}" for x, y in contour)
            d.append(f"M{pts}Z")
    return " ".join(d)


def cut_y(x):
    (x1, y1), (x2, y2) = CUT
    return y1 + (y2 - y1) * (x - x1) / (x2 - x1)


def svg(theme):
    dark = theme == "dark"
    top, bottom = ("#F4F6F9", "#9BA4B0") if dark else ("#111216", "#3A3F48")
    edge = "#0B0C0F" if dark else "#000000"
    red, glow = ("#FF2D3F", "#FF1A2E") if dark else ("#D0102A", "#E0142E")
    d = word_path()
    above = f"0,0 {W},0 {W},{cut_y(W):.1f} 0,{cut_y(0):.1f}"
    below = f"0,{cut_y(0):.1f} {W},{cut_y(W):.1f} {W},{H} 0,{H}"
    (x1, y1), (x2, y2) = CUT
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" aria-label="Zandatsu">
  <title>Zandatsu</title>
  <defs>
    <linearGradient id="steel" x1="0" y1="{Y0}" x2="0" y2="{Y0 + 100}" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="{top}"/>
      <stop offset="1" stop-color="{bottom}"/>
    </linearGradient>
    <linearGradient id="slash" x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" gradientUnits="userSpaceOnUse">
      <stop offset="0" stop-color="{red}" stop-opacity="0"/>
      <stop offset="0.12" stop-color="{red}"/>
      <stop offset="0.88" stop-color="{red}"/>
      <stop offset="1" stop-color="{red}" stop-opacity="0"/>
    </linearGradient>
    <filter id="glow" x="-10%" y="-200%" width="120%" height="500%">
      <feGaussianBlur stdDeviation="5"/>
    </filter>
    <clipPath id="above"><polygon points="{above}"/></clipPath>
    <clipPath id="below"><polygon points="{below}"/></clipPath>
  </defs>
  <g fill="url(#steel)" fill-rule="evenodd" stroke="{edge}" stroke-width="2" stroke-linejoin="miter">
    <path d="{d}" clip-path="url(#below)"/>
    <g transform="translate({SHIFT[0]} {SHIFT[1]})"><path d="{d}" clip-path="url(#above)"/></g>
  </g>
  <line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{glow}" stroke-width="9" stroke-linecap="round" opacity="0.55" filter="url(#glow)"/>
  <line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="url(#slash)" stroke-width="3.5" stroke-linecap="round"/>
</svg>
"""


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for theme in ("dark", "light"):
        path = os.path.join(OUT, f"zandatsu-logo-{theme}.svg")
        with open(path, "w") as f:
            f.write(svg(theme))
        print(path)
