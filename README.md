# Arena Limpia — Torneo de Voleibol de Playa

Proyecto final del Laboratorio de Computación Gráfica e Interacción Humano-Computadora.
Facultad de Ingeniería, UNAM. Semestre 2027-1.

## Descripción

Arena Limpia es una escena 3D interactiva hecha en C++ con OpenGL 3.3 core y GLSL. Recrea
un torneo de voleibol de playa en el que también se recolecta la basura de la arena.

## Fin del proyecto

Es un simulador educativo y de entretenimiento que promueve dos Objetivos de Desarrollo
Sostenible de la Agenda 2030 de la ONU:

- **ODS 3, Salud y bienestar:** el torneo de voleibol de playa fomenta la actividad física.
- **ODS 14, Vida submarina:** la recolección de basura en la playa muestra cómo cuidar el
  medio marino.

## Tecnologías

- C++ y Visual Studio (Windows)
- OpenGL 3.3 core y GLSL
- GLFW, GLAD, GLM, Assimp y stb_image
- Blender para el modelado
- InstallForge para generar `setup.exe`

## Cómo compilar

1. Instala Visual Studio 2022 o 2026 con la carga de trabajo **Desarrollo para el escritorio
   con C++**.
2. Instala [Git LFS](https://git-lfs.com) y actívalo una vez en tu equipo:

   ```
   git lfs install
   ```

3. Clona el repositorio dentro de la carpeta `src` del curso, junto a `deps` y `bin`:

   ```
   src/
   ├── deps/          GLFW, GLAD, GLM y Assimp del curso
   ├── bin/           ejecutables del curso (de aquí se copia la DLL de Assimp)
   └── ArenaLimpia/   este repositorio
   ```

   ```
   git clone https://github.com/CarlosParraBello/ArenaLimpia.git
   ```

4. Abre `ArenaLimpia.slnx` en Visual Studio y selecciona la plataforma **x64**.
5. Compila y ejecuta con **F5**. No hay que configurar nada: el proyecto busca las
   dependencias en `..\deps` con rutas relativas, y al compilar copia
   `assimp-vc143-mt(d).dll` desde `..\bin` junto al ejecutable.

El ejecutable queda en `build\Debug\` o `build\Release\` (carpeta ignorada por git). Al
arrancar busca `shaders/` y `assets/` en la carpeta actual o en las carpetas superiores a la
del ejecutable, así que funciona desde Visual Studio y con doble clic.

### Controles

Hay dos cámaras; **V** cambia entre ellas.

**Cámara libre**

| Tecla | Acción |
|---|---|
| W A S D | Mover la cámara |
| Espacio / C | Subir / bajar |
| Shift | Moverse más rápido |
| Mouse / rueda | Mirar alrededor / zoom |

**Tercera persona** (sigue a un jugador)

| Tecla | Acción |
|---|---|
| W A S D | Caminar con el jugador, relativo a la cámara |
| Shift | Correr |
| Mouse | Girar la cámara alrededor del jugador |
| Rueda | Acercar / alejar |
| 1 2 3 4 | Elegir jugador (MX, PR, HA, JA) |

**Generales**

| Tecla | Acción |
|---|---|
| V | Cambiar de cámara |
| Tab | Mostrar u ocultar el cursor |
| M / N | Malla de alambre / relleno |
| F12 | Captura de pantalla en `docs/capturas/` |
| Esc | Salir |

### Agregar un modelo a la escena

1. Guarda el `.blend` en `assets/models/<TuCarpeta>/<Modelo>/<Modelo>.blend`, con las
   texturas empaquetadas (**Archivo → Externos → Empaquetar recursos**).
2. Expórtalo a OBJ con el script del repositorio (ejecutarlo desde la raíz del repositorio):

   ```
   "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup --python tools/exportar_obj.py -- <TuCarpeta>
   ```

   Genera `<Modelo>.obj`, `<Modelo>.mtl` y `textures/` junto al `.blend`. No uses FBX: el
   FBX de Blender 4.1 o posterior llega a Assimp sin normales.
3. Agrega una línea a `sceneObjects` en `src/main.cpp` con la ruta del `.obj`, la
   posición, la rotación y la escala. El modelo se apoya solo sobre la arena.
4. Regístralo en [docs/modelos.md](docs/modelos.md).

## Estructura de carpetas

```
ArenaLimpia.slnx  solución de Visual Studio
src/              código C++ (main.cpp)
include/          clases del curso (shader, cámara, modelo, cubemap) y stb_image
shaders/          shaders .vs / .fs
assets/models/    un .blend por modelo y su exportación .obj/.mtl, en una carpeta por integrante
assets/textures/  texturas
assets/skybox/    imágenes del skybox
tools/            scripts (exportar de Blender a OBJ, convertir panorámicas a cubemap)
docs/             reporte, bitácora y documentación
docs/capturas/    capturas de pantalla
installer/        proyecto de InstallForge
```

## Documentación

- [Registro de modelos](docs/modelos.md)
- [Checklist de requisitos](docs/requisitos.md)
- [Bitácora de experimentos](docs/bitacora.md)

## Reglas del equipo

1. **`main` siempre compila.** No se integra nada que no compile.
2. **Una rama por función.** Cada función o modelo nuevo se trabaja en su propia rama.
3. **Pull Requests para integrar.** Nadie sube cambios directamente a `main`.
4. **Rutas relativas siempre.** Nunca se usan rutas absolutas como `C:\Users\...`.

La calificación es individual y el historial de commits sirve como evidencia de quién hizo
qué, así que cada integrante hace sus commits con su propia cuenta.

La hoja de contribuciones no se sube al repositorio porque contiene números de cuenta.

## Integrantes

| Integrante | Rol |
|---|---|
| [Integrante 1] | [Rol] |
| [Integrante 2] | [Rol] |
| [Integrante 3] | [Rol] |
| [Integrante 4] | [Rol] |
| [Integrante 5] | [Rol] |
