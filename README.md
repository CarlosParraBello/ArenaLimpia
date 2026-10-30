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

> Todavía no hay código ni solución de Visual Studio. Esta sección se completará cuando se
> agreguen.

1. Instala Visual Studio con la carga de trabajo **Desarrollo para el escritorio con C++**.
2. Instala [Git LFS](https://git-lfs.com) y actívalo una vez en tu equipo:

   ```
   git lfs install
   ```

3. Clona el repositorio dentro de la carpeta `src` del curso, junto a `deps`:

   ```
   src/
   ├── deps/          GLFW, GLAD, GLM y Assimp del curso
   └── ArenaLimpia/   este repositorio
   ```

   ```
   git clone https://github.com/CarlosParraBello/ArenaLimpia.git
   ```

4. Abre la solución en Visual Studio y selecciona la plataforma **x64**.
5. Configura las dependencias con rutas relativas a la solución, por ejemplo
   `$(SolutionDir)..\deps\glfw\MSVC2022\include`.
6. Compila y ejecuta.

## Estructura de carpetas

```
src/              código C++
include/          headers propios y de terceros (glad, glm, stb)
shaders/          shaders .vs / .fs
assets/models/    modelos exportados (.obj/.fbx/.gltf)
assets/textures/  texturas
assets/skybox/    imágenes del skybox
blender/          archivos .blend fuente
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
