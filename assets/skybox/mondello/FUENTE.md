# Skybox: Spiaggia di Mondello

- **Fuente:** Poly Haven, "Spiaggia di Mondello" — https://polyhaven.com/a/spiaggia_di_mondello
- **Autor:** Andreas Mischok
- **Lugar:** playa de Mondello, Palermo, Sicilia (Italia)
- **Licencia:** CC0 (dominio público). No exige atribución; se cita por transparencia en el reporte.
- **Archivo original:** panorámica equirectangular "Tonemapped JPG" de 8192×4096
  (`spiaggia_di_mondello.jpg`, md5 `ae13689e3fe92ef57f0e09f220bf97cb`).

## Conversión

Las 6 caras (2048×2048) se generaron con `tools/equirect_a_cubemap.py`:

```
python tools/equirect_a_cubemap.py spiaggia_di_mondello.jpg assets/skybox/mondello 2048 -91.4
```

El giro de -91.4° pone el sol de la foto en la misma dirección horizontal que la luz del sol
de la escena (`sunDirection` en `src/main.cpp`), para que las sombras y el brillo coincidan con
el cielo. Si se cambia `sunDirection`, hay que recalcular el giro.
