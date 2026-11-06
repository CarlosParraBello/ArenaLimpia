# Checklist de requisitos

## Escena y modelos

- [ ] 80 modelos (≥64 originales)
- [x] 2 modos de cámara: libre y tercera persona siguiendo a un jugador (tecla V)
- [x] 15 objetos interactivos (tecla R selecciona; G, flechas, RePág/AvPág, Z/X, +/-, Retroceso):
  frisbee, balón de playa, balón de fútbol, aro salvavidas, hielera, termo, garrafón, refresco,
  botella de agua, bloqueador, bote de basura, cubeta, gorra, lentes de sol, sombrilla

## Materiales, iluminación y texturas

- [x] Materiales opacos (todos los que no están en `objectMaterials`)
- [x] Materiales plásticos (balones, frisbee, hielera, aros, bloqueador, bote de basura, cubeta)
- [x] Materiales traslúcidos (garrafón y botellas de agua, mezcla con opacidad fija)
- [x] Materiales metálicos (termo, refresco, trofeo)
- [ ] 2 técnicas de iluminación
- [x] 1 mapeo de ambiente (skybox/cubemap): `assets/skybox/mondello/`
- [ ] Texturizado de modelos
- [x] Texturizado de ambiente: arena del piso (`assets/textures/arena_repetible.jpg`, repetible) y mar con olas

## Animaciones

- [x] 10 animaciones simples: giran el frisbee, la estrella de mar, el aro salvavidas y el
  globo; van y vienen el balón de playa, el bote y el cangrejo (en x) y la langosta y la tortuga
  (en z, volteándose); el gato voltea de un lado a otro
- [x] 5 animaciones por keyframes (esqueleto, `tools/animar_personajes.py`):
  1. Caminar: jugadores, al moverse en tercera persona
  2. Saque: jugadores, tecla Q
  3. Remate: jugadores, tecla E (con salto)
  4. Festejo: jugadores, tecla F
  5. Marcar: árbitro, cada 8 s
  - Extra: Parado (todos los personajes animados, en reposo)
- [x] 5 animaciones procedurales:
  1. Tiro parabólico del balón en el saque (Q) y el remate (E): gravedad, rebote en la
     arena, choque con la red, rodar hasta detenerse y giro según la velocidad
  2. Palmera con viento: jerarquía tronco -> hojas/cocos, senos con desfase por hoja
  3. Fuego de la fogata: escala con ruido (suma de senos) y emisión que parpadea
  4. Gaviota volando en círculo (cos/sen del tiempo), sube y baja y mira hacia donde vuela
  5. Olas del mar: suma de 4 senos en el vertex shader (`shaders/water.vs`), normal exacta por
     fragmento, espuma en la orilla; el bote flota y se inclina con la misma función en el CPU
- Extra: partido automático 2 contra 2 (tecla P)

## Entregables

- [ ] `setup.exe`
- [ ] `manual.pdf`
- [ ] Reporte en español
- [ ] Reporte en inglés
- [ ] Video de 10 a 15 minutos
