# Checklist de requisitos

## Escena y modelos

- [ ] 80 modelos (≥64 originales)
- [x] 2 modos de cámara: libre y tercera persona siguiendo a un jugador (tecla V)
- [ ] 15 objetos interactivos

## Materiales, iluminación y texturas

- [ ] Materiales opacos
- [ ] Materiales plásticos
- [ ] Materiales traslúcidos
- [ ] Materiales metálicos
- [ ] 2 técnicas de iluminación
- [x] 1 mapeo de ambiente (skybox/cubemap): `assets/skybox/mondello/`
- [ ] Texturizado de modelos
- [ ] Texturizado de ambiente

## Animaciones

- [ ] 10 animaciones simples
- [x] 5 animaciones por keyframes (esqueleto, `tools/animar_personajes.py`):
  1. Caminar: jugadores, al moverse en tercera persona
  2. Saque: jugadores, tecla Q
  3. Remate: jugadores, tecla E (con salto)
  4. Festejo: jugadores, tecla F
  5. Marcar: árbitro, cada 8 s
  - Extra: Parado (todos los personajes animados, en reposo)
- [ ] 5 animaciones procedurales (3 de 5):
  1. Tiro parabólico del balón en el saque (Q) y el remate (E): gravedad, rebote en la
     arena, choque con la red, rodar hasta detenerse y giro según la velocidad
  2. Palmera con viento: jerarquía tronco -> hojas/cocos, senos con desfase por hoja
  3. Fuego de la fogata: escala con ruido (suma de senos) y emisión que parpadea

## Entregables

- [ ] `setup.exe`
- [ ] `manual.pdf`
- [ ] Reporte en español
- [ ] Reporte en inglés
- [ ] Video de 10 a 15 minutos
