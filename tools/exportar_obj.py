"""
Exporta los .blend de assets/models/<Integrante>/<Modelo>/ a OBJ para cargarlos con Assimp.

Uso (desde la raíz del repositorio):

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b --factory-startup ^
        --python tools/exportar_obj.py -- CarlosParra

El argumento después de "--" es la carpeta del integrante. Sin argumento se exportan todos.

Por cada modelo se generan, junto al .blend:
    <Modelo>.obj, <Modelo>.mtl y textures/ con las imágenes que usa.

Notas:
- No modifica ni guarda el .blend.
- Ejes: Y arriba, -Z adelante (el estándar de OpenGL), así que en C++ no hace falta rotar.
- Los materiales procedurales no se pueden exportar a OBJ. Si un material no tiene imagen
  y existe assets/textures/imagenes_texturizados/<Material>_Bake.png, se usa esa imagen.
- Se usa el exportador de OBJ, no FBX: el FBX de Blender 4.1+ llega a Assimp sin normales.
- Si un .blend trae varios modelos (SPLIT), cada uno se exporta a su propio OBJ con los objetos
  cuyo nombre empieza con alguno de sus prefijos; así se pueden poner en lugares distintos.
"""
import bpy
import glob
import os
import re
import shutil
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BAKE_DIR = os.path.join(ROOT, "assets", "textures", "imagenes_texturizados")

# Modelo (carpeta) -> {OBJ de salida: prefijos de los objetos de Blender que lleva}
SPLIT = {
    "MasElementos": {
        "Bandera": ["Asta_", "Bandera"],
        "Barril":  ["Barril"],
        "Remo":    ["Remo_"],
    },
}


def safe_name(name):
    base = re.sub(r"[^A-Za-z0-9_.-]", "_", name)
    return base or "textura"


def link_bake_images():
    """Conecta <Material>_Bake.png a los materiales que no tienen imagen."""
    linked = []
    for mat in bpy.data.materials:
        if not mat.use_nodes:
            continue
        nodes = mat.node_tree.nodes
        if any(n.type == "TEX_IMAGE" and n.image for n in nodes):
            continue
        bake = os.path.join(BAKE_DIR, mat.name + "_Bake.png")
        bsdf = next((n for n in nodes if n.type == "BSDF_PRINCIPLED"), None)
        if not os.path.isfile(bake) or bsdf is None:
            continue
        tex = nodes.new("ShaderNodeTexImage")
        tex.image = bpy.data.images.load(bake, check_existing=True)
        mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
        linked.append(mat.name)
    return linked


def write_images(tex_dir):
    """Guarda las imágenes (empaquetadas o no) en tex_dir y apunta cada imagen a su copia."""
    os.makedirs(tex_dir, exist_ok=True)
    used = set()
    for img in bpy.data.images:
        if img.type != "IMAGE" or img.users == 0:
            continue
        src = bpy.path.abspath(img.filepath) if img.filepath else ""
        ext = os.path.splitext(src)[1].lower()
        if ext not in (".png", ".jpg", ".jpeg", ".tga", ".bmp"):
            ext = ".jpg" if img.file_format == "JPEG" else ".png"
        name = safe_name(os.path.splitext(img.name)[0]) + ext
        while name.lower() in used:
            name = "_" + name
        used.add(name.lower())
        dst = os.path.join(tex_dir, name)
        if img.packed_file:
            with open(dst, "wb") as fh:
                fh.write(img.packed_file.data)
        elif os.path.isfile(src):
            shutil.copyfile(src, dst)
        else:
            print(f"  AVISO: no se encontró la imagen {img.name} ({src})")
            continue
        if img.packed_file:
            img.unpack(method="REMOVE")
        img.filepath = dst
    return sorted(used)


def export(blend):
    bpy.ops.wm.open_mainfile(filepath=blend, load_ui=False)
    folder = os.path.dirname(blend)
    model = os.path.basename(folder)
    linked = link_bake_images()
    images = write_images(os.path.join(folder, "textures"))
    pieces = SPLIT.get(model, {model: None})
    for name, prefixes in pieces.items():
        if prefixes is not None:
            for o in bpy.data.objects:
                o.select_set(any(o.name.startswith(p) for p in prefixes))
        obj_path = os.path.join(folder, name + ".obj")
        bpy.ops.wm.obj_export(
            filepath=obj_path,
            export_selected_objects=prefixes is not None,
            apply_modifiers=True,
            export_uv=True,
            export_normals=True,
            export_materials=True,
            export_triangulated_mesh=True,
            path_mode="RELATIVE",
            forward_axis="NEGATIVE_Z",
            up_axis="Y",
        )
        print(f"OK {os.path.relpath(obj_path, ROOT)}  texturas={len(images)}"
              + (f"  bake={','.join(linked)}" if linked else ""))

def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    members = argv or ["*"]
    for member in members:
        pattern = os.path.join(ROOT, "assets", "models", member, "*", "*.blend")
        for blend in sorted(glob.glob(pattern)):
            export(blend)


main()
