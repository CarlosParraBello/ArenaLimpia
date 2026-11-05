"""
Crea un ciclo de caminar sobre el esqueleto de un personaje y lo exporta a glTF.

Los 11 personajes de Gabriel comparten el mismo esqueleto ("Esqueleto", 19 huesos), así que
este script sirve para cualquiera. No modifica el .blend original: lo abre, agrega la acción
"Caminar" en memoria y exporta a assets/models/GabrielYukioayax/<Modelo>/animado/<Modelo>.gltf.

Uso (desde la raíz del repositorio):
    blender -b --factory-startup --python tools/animar_caminar.py -- JugadorMX [JugadorPR ...]

Las rotaciones se definen en ejes del esqueleto (el personaje mira hacia -Y, su izquierda es +X)
y se convierten al eje local de cada hueso, así no dependen del "roll" de cada hueso.
"""
import bpy
import glob
import math
import os
import sys
from mathutils import Matrix, Quaternion

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODELS = os.path.join(ROOT, "assets", "models", "GabrielYukioayax")

FRAMES = 24          # un ciclo completo (dos pasos)
FPS = 24             # 1 segundo por ciclo a velocidad de caminata
X = (1.0, 0.0, 0.0)  # eje lateral: rotar sobre él balancea piernas y brazos adelante/atrás
Y = (0.0, 1.0, 0.0)  # eje frente-espalda: rotar sobre él baja los brazos de la pose T
Z = (0.0, 0.0, 1.0)  # eje vertical


def rot(axis, degrees):
    return Quaternion(axis, math.radians(degrees))


def wave(frame, phase=0.0):
    """Seno de periodo FRAMES; phase en fracciones de ciclo."""
    return math.sin(2.0 * math.pi * (frame / FRAMES + phase))


def pose_at(frame):
    """Rotación de cada hueso (en ejes del esqueleto) en un cuadro del ciclo.
    Ángulo negativo sobre X = hacia adelante (el personaje mira hacia -Y)."""
    swing = 28.0 * wave(frame)            # muslo izquierdo: + atrás, - adelante
    pose = {}

    # Piernas: el muslo se balancea; la rodilla se dobla cuando la pierna regresa al frente
    pose["muslo.L"] = rot(X, swing)
    pose["muslo.R"] = rot(X, -swing)
    pose["pierna.L"] = rot(X, 8.0 + 32.0 * max(0.0, wave(frame, 0.25)))
    pose["pierna.R"] = rot(X, 8.0 + 32.0 * max(0.0, wave(frame, 0.75)))
    pose["pie.L"] = rot(X, -0.4 * swing)
    pose["pie.R"] = rot(X, 0.4 * swing)

    # Brazos: bajarlos de la pose T y balancearlos al revés que la pierna del mismo lado
    pose["brazo.L"] = rot(X, 0.8 * -swing) @ rot(Y, 72.0)
    pose["brazo.R"] = rot(X, 0.8 * swing) @ rot(Y, -72.0)
    pose["antebrazo.L"] = rot(X, -15.0)
    pose["antebrazo.R"] = rot(X, -15.0)

    # Tronco: la cadera gira un poco y el pecho compensa
    pose["cadera"] = rot(Z, 4.0 * wave(frame))
    pose["pecho"] = rot(Z, -6.0 * wave(frame))
    return pose


def idle_pose(frame):
    """Parado: brazos abajo y piernas rectas, con una respiración leve en el pecho."""
    breath = 1.5 * math.sin(2.0 * math.pi * frame / FRAMES)
    return {
        "brazo.L": rot(Y, 74.0),
        "brazo.R": rot(Y, -74.0),
        "antebrazo.L": rot(X, -8.0),
        "antebrazo.R": rot(X, -8.0),
        "pecho": rot(X, -breath),
        "pierna.L": rot(X, 3.0),
        "pierna.R": rot(X, 3.0),
    }


def lowest_foot(arm):
    """Altura (en ejes del esqueleto) del punto más bajo de los pies en la pose actual."""
    bpy.context.view_layer.update()
    zs = []
    for name in ("pie.L", "pie.R"):
        pb = arm.pose.bones[name]
        zs += [pb.head.z, pb.tail.z]
    return min(zs)


def build_action(arm, name, pose_fn):
    """Crea una acción de FRAMES cuadros a partir de una función de pose.
    La cadera sube o baja en cada cuadro para que el pie más bajo toque el piso,
    así el personaje no flota ni se hunde al abrir las piernas."""
    action = bpy.data.actions.new(name)
    action.use_fake_user = True          # para que el exportador la incluya
    arm.animation_data.action = action

    hb = arm.pose.bones["cadera"]
    to_local = hb.bone.matrix_local.to_3x3().inverted()

    # Altura de los pies en la pose de reposo
    for pb in arm.pose.bones:
        pb.rotation_quaternion = Quaternion()
        pb.location = (0.0, 0.0, 0.0)
    ground = lowest_foot(arm)

    for frame in range(0, FRAMES + 1, 2):
        pose = pose_fn(frame)
        for pb in arm.pose.bones:
            q_arm = pose.get(pb.name, Quaternion())
            # Pasar la rotación de ejes del esqueleto a ejes locales del hueso
            rest = pb.bone.matrix_local.to_quaternion()
            pb.rotation_quaternion = rest.inverted() @ q_arm @ rest
            pb.location = (0.0, 0.0, 0.0)
        drop = ground - lowest_foot(arm)
        hb.location = to_local @ (Matrix.Identity(3).col[2] * drop)
        for pb in arm.pose.bones:
            pb.keyframe_insert("rotation_quaternion", frame=frame)
        hb.keyframe_insert("location", frame=frame)
    return action


def build_actions(arm):
    scene = bpy.context.scene
    scene.render.fps = FPS
    scene.frame_start = 0
    scene.frame_end = FRAMES

    arm.animation_data_create()
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"

    build_action(arm, "Parado", idle_pose)
    walk = build_action(arm, "Caminar", pose_at)

    bpy.ops.object.mode_set(mode="OBJECT")
    arm.animation_data.action = walk


def report(arm, frames=(0, 6, 12, 18)):
    """Imprime dónde quedan pies y manos para revisar la pose sin abrir Blender."""
    for f in frames:
        bpy.context.scene.frame_set(f)
        parts = []
        for name in ("pie.L", "pie.R", "mano.L", "mano.R"):
            p = arm.matrix_world @ arm.pose.bones[name].head
            parts.append(f"{name}=({p.x:+.2f},{p.y:+.2f},{p.z:+.2f})")
        print(f"cuadro {f:2d}: " + "  ".join(parts))


def export(model):
    pattern = os.path.join(MODELS, model, "*.blend")
    files = glob.glob(pattern)
    if not files:
        print(f"ERROR no hay .blend en {pattern}")
        return
    bpy.ops.wm.open_mainfile(filepath=files[0], load_ui=False)
    arm = bpy.data.objects.get("Esqueleto")
    if arm is None or arm.type != "ARMATURE":
        print(f"ERROR {model}: no tiene el objeto 'Esqueleto'")
        return

    build_actions(arm)
    report(arm)

    out_dir = os.path.join(MODELS, model, "animado")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, model + ".gltf")

    # Exportar solo la malla y el esqueleto: quitar cámara y luz, y asegurar que el
    # esqueleto sea visible (si está oculto, el exportador lo omite)
    for o in list(bpy.data.objects):
        if o.type not in ("MESH", "ARMATURE"):
            bpy.data.objects.remove(o, do_unlink=True)
        else:
            print(f"  {o.type} {o.name}: hide_viewport={o.hide_viewport} hide={o.hide_get()} hide_render={o.hide_render}")
            o.hide_viewport = False
            o.hide_render = False
            o.hide_set(False)
    bpy.context.scene.frame_set(0)
    bpy.ops.export_scene.gltf(
        filepath=out,
        export_format="GLTF_SEPARATE",
        use_selection=False,
        export_yup=True,
        export_apply=False,
        export_skins=True,
        export_animations=True,
        export_animation_mode="ACTIONS",
        export_force_sampling=True,
        export_frame_range=True,
        export_texture_dir="",
        export_image_format="AUTO",
    )
    print(f"OK {os.path.relpath(out, ROOT)}")


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    for model in argv or ["JugadorMX"]:
        export(model)


main()
