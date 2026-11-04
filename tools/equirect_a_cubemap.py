"""
Arena Limpia - Convierte una panorámica equirectangular (360°) en las 6 caras
de un cubemap de OpenGL (posx, negx, posy, negy, posz, negz).

Uso:
    python tools/equirect_a_cubemap.py <panoramica.jpg> <carpeta_salida> [tam_cara] [giro_grados]

- tam_cara: lado de cada cara en píxeles (por defecto 2048).
- giro_grados: gira la panorámica alrededor del eje Y. Sirve para que el sol
  de la foto quede en la misma dirección que la luz del sol de la escena.

Las caras siguen la convención de GL_TEXTURE_CUBE_MAP con las imágenes
cargadas sin voltear (stbi_set_flip_vertically_on_load desactivado), igual
que en include/cubemap.h. El centro de la panorámica queda hacia -Z, que es
hacia donde mira la cámara al arrancar.

Requiere: numpy, pillow.
"""
import sys
import numpy as np
from PIL import Image

# Dirección de cada pixel de la cara a partir de (sc, tc) en [-1, 1].
CARAS = {
    "posx": lambda sc, tc: ( np.ones_like(sc), -tc, -sc),
    "negx": lambda sc, tc: (-np.ones_like(sc), -tc,  sc),
    "posy": lambda sc, tc: ( sc,  np.ones_like(sc),  tc),
    "negy": lambda sc, tc: ( sc, -np.ones_like(sc), -tc),
    "posz": lambda sc, tc: ( sc, -tc,  np.ones_like(sc)),
    "negz": lambda sc, tc: (-sc, -tc, -np.ones_like(sc)),
}


def muestrear_bilineal(pano, u, v):
    """Lee la panorámica en coordenadas (u, v) en [0, 1) con interpolación bilineal."""
    h, w, _ = pano.shape
    x = u * w - 0.5
    y = np.clip(v * h - 0.5, 0, h - 1)
    x0 = np.floor(x).astype(int)
    y0 = np.floor(y).astype(int)
    fx = (x - x0)[..., None]
    fy = (y - y0)[..., None]
    x0 %= w                          # horizontalmente la panorámica da la vuelta
    x1 = (x0 + 1) % w
    y1 = np.minimum(y0 + 1, h - 1)
    arriba = pano[y0, x0] * (1 - fx) + pano[y0, x1] * fx
    abajo = pano[y1, x0] * (1 - fx) + pano[y1, x1] * fx
    return arriba * (1 - fy) + abajo * fy


def convertir(ruta_pano, carpeta, tam=2048, giro=0.0):
    pano = np.asarray(Image.open(ruta_pano).convert("RGB"), dtype=np.float32)
    c = (np.arange(tam) + 0.5) / tam * 2 - 1
    sc, tc = np.meshgrid(c, c)
    for nombre, f in CARAS.items():
        dx, dy, dz = f(sc, tc)
        n = np.sqrt(dx * dx + dy * dy + dz * dz)
        dx, dy, dz = dx / n, dy / n, dz / n
        lon = np.arctan2(dx, -dz) + np.radians(giro)      # 0 = hacia -Z, crece hacia +X
        lat = np.arcsin(np.clip(dy, -1, 1))
        u = (0.5 + lon / (2 * np.pi)) % 1.0
        v = 0.5 - lat / np.pi
        cara = muestrear_bilineal(pano, u, v)
        Image.fromarray(np.clip(cara + 0.5, 0, 255).astype(np.uint8)).save(f"{carpeta}/{nombre}.png", optimize=True)
        print(f"{nombre}.png listo")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    convertir(sys.argv[1], sys.argv[2],
              int(sys.argv[3]) if len(sys.argv) > 3 else 2048,
              float(sys.argv[4]) if len(sys.argv) > 4 else 0.0)
