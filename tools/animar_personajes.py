"""
Crea las animaciones de los personajes sobre su esqueleto y las exporta a glTF.

Los 11 personajes de Gabriel comparten el mismo esqueleto ("Esqueleto", 19 huesos), así que
este script sirve para cualquiera. No modifica el .blend original: lo abre, agrega las acciones
en memoria y exporta a assets/models/GabrielYukioayax/<Modelo>/animado/<Modelo>.gltf.

Acciones:
  Todos:      Parado (ciclo), Caminar (ciclo, calculada con senos cada 2 cuadros)
  Jugadores:  Saque, Remate, Festejo (cuadros clave)
  Árbitro:    Marcar (cuadros clave)

Uso (desde la raíz del repositorio):
    blender -b --factory-startup --python tools/animar_personajes.py -- JugadorMX [Arbitro ...]

Ejes: las rotaciones se escriben en ejes del esqueleto y se convierten al eje local de cada
hueso, así no dependen del "roll" de cada hueso. El personaje mira hacia -Y y su izquierda
es +X. Reglas útiles:
  - rot(X, a) con a < 0 lleva hacia adelante una extremidad que apunta hacia abajo (piernas,
    brazos caídos); con a > 0 inclina el tronco hacia adelante.
  - Las rotaciones de un hueso son relativas a su padre (el brazo sigue al pecho).
"""
import bpy
import glob
import math
import os
import sys
from mathutils import Matrix, Quaternion

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODELS = os.path.join(ROOT, "assets", "models", "GabrielYukioayax")

FPS = 24
CYCLE = 24           # cuadros de Parado y Caminar (1 s)
HEIGHT_M = 1.8       # altura de referencia del personaje en la escena (para saltos en metros)
X = (1.0, 0.0, 0.0)
Y = (0.0, 1.0, 0.0)
Z = (0.0, 0.0, 1.0)

PLAYERS = {"JugadorMX", "JugadorPR", "JugadoraHA", "JugadoraJA"}


def rot(axis, degrees):
    return Quaternion(axis, math.radians(degrees))


# ---------------------------------------------------------------------------
# Piezas de pose
# ---------------------------------------------------------------------------
def arm_side(side, elev):
    """Brazo hacia el costado con elevación elev (-72 = caído, 0 = pose T, 90 = arriba)."""
    return rot(Y, -elev) if side == "L" else rot(Y, elev)


def arm_front(side, elev):
    """Brazo hacia el frente con elevación elev (0 = horizontal, 90 = arriba,
    150 = arriba y hacia atrás, -60 = abajo al frente)."""
    return rot(X, -elev) @ (rot(Z, -90.0) if side == "L" else rot(Z, 90.0))


def arm_down(side, swing=0.0):
    """Brazo caído a un lado, balanceado swing grados hacia adelante (negativo = atrás)."""
    return rot(X, -swing) @ arm_side(side, -72.0)


def crouch(depth):
    """Flexión de piernas: muslos al frente, rodillas atrás y pies planos."""
    return {
        "muslo.L": rot(X, -depth), "muslo.R": rot(X, -depth),
        "pierna.L": rot(X, 2.0 * depth), "pierna.R": rot(X, 2.0 * depth),
        "pie.L": rot(X, -depth), "pie.R": rot(X, -depth),
    }


def idle_base():
    return {
        "brazo.L": arm_down("L"), "brazo.R": arm_down("R"),
        "antebrazo.L": rot(X, -8.0), "antebrazo.R": rot(X, -8.0),
        "pierna.L": rot(X, 3.0), "pierna.R": rot(X, 3.0),
    }


def merge(*parts):
    out = {}
    for p in parts:
        out.update(p)
    return out


# ---------------------------------------------------------------------------
# Ciclos calculados (Parado y Caminar)
# ---------------------------------------------------------------------------
def wave(frame, phase=0.0):
    return math.sin(2.0 * math.pi * (frame / CYCLE + phase))


def walk_pose(frame):
    swing = 28.0 * wave(frame)            # muslo izquierdo: + atrás, - adelante
    return {
        "muslo.L": rot(X, swing),
        "muslo.R": rot(X, -swing),
        "pierna.L": rot(X, 8.0 + 32.0 * max(0.0, wave(frame, 0.25))),
        "pierna.R": rot(X, 8.0 + 32.0 * max(0.0, wave(frame, 0.75))),
        "pie.L": rot(X, -0.4 * swing),
        "pie.R": rot(X, 0.4 * swing),
        "brazo.L": arm_down("L", 0.8 * swing),
        "brazo.R": arm_down("R", -0.8 * swing),
        "antebrazo.L": rot(X, -15.0),
        "antebrazo.R": rot(X, -15.0),
        "cadera": rot(Z, 4.0 * wave(frame)),
        "pecho": rot(Z, -6.0 * wave(frame)),
    }


def idle_pose(frame):
    breath = 1.5 * math.sin(2.0 * math.pi * frame / CYCLE)
    return merge(idle_base(), {"pecho": rot(X, -breath)})


def sampled(pose_fn):
    """Convierte una función de pose en cuadros clave cada 2 cuadros (sin salto)."""
    return [(f, pose_fn(f), 0.0) for f in range(0, CYCLE + 1, 2)]


# ---------------------------------------------------------------------------
# Acciones por cuadros clave: lista de (cuadro, pose, elevación en metros)
# ---------------------------------------------------------------------------
def saque():
    """Saque por arriba: lanza el balón con la izquierda y golpea con la derecha."""
    idle = idle_base()
    stance = merge(idle, {"muslo.L": rot(X, -12.0), "muslo.R": rot(X, 8.0),
                          "pierna.L": rot(X, 10.0), "pierna.R": rot(X, 6.0)})
    return [
        (0, idle, 0.0),
        # Preparación: pie izquierdo al frente, brazo izquierdo sostiene el balón
        (8, merge(stance, {"brazo.L": arm_front("L", 0.0), "antebrazo.L": rot(X, 0.0),
                           "brazo.R": arm_down("R", -20.0)}), 0.0),
        # Lanzamiento: brazo izquierdo arriba, derecho se arma atrás de la cabeza
        (16, merge(stance, {"brazo.L": arm_front("L", 100.0),
                            "brazo.R": arm_front("R", 150.0), "antebrazo.R": rot(X, -70.0),
                            "pecho": rot(X, -10.0) @ rot(Z, 15.0),
                            "cabeza": rot(X, -20.0)}), 0.0),
        # Golpe: brazo derecho extendido arriba al frente, el tronco gira hacia adelante
        (22, merge(stance, {"brazo.L": arm_front("L", 20.0),
                            "brazo.R": arm_front("R", 70.0), "antebrazo.R": rot(X, 0.0),
                            "pecho": rot(X, 15.0) @ rot(Z, -10.0),
                            "cabeza": rot(X, -5.0)}), 0.0),
        # Seguimiento: el brazo baja cruzando el cuerpo
        (28, merge(stance, {"brazo.L": arm_down("L", -10.0),
                            "brazo.R": arm_front("R", -40.0),
                            "pecho": rot(X, 18.0) @ rot(Z, -15.0)}), 0.0),
        (40, idle, 0.0),
    ]


def remate():
    """Remate: carrera corta, flexión, salto con el brazo armado y golpe hacia abajo."""
    idle = idle_base()
    arms_back = {"brazo.L": arm_down("L", -50.0), "brazo.R": arm_down("R", -50.0)}
    return [
        (0, idle, 0.0),
        # Flexión con los brazos atrás para tomar impulso
        (8, merge(crouch(35.0), arms_back, {"pecho": rot(X, 25.0)}), 0.0),
        # Despegue: brazos arriba
        (13, merge(crouch(8.0), {"brazo.L": arm_front("L", 120.0), "brazo.R": arm_front("R", 120.0),
                                 "pecho": rot(X, -5.0)}), 0.25),
        # En el aire: izquierdo apunta al balón, derecho armado atrás
        (17, merge({"pierna.L": rot(X, 40.0), "pierna.R": rot(X, 40.0),
                    "brazo.L": arm_front("L", 110.0),
                    "brazo.R": arm_front("R", 160.0), "antebrazo.R": rot(X, -80.0),
                    "pecho": rot(X, -15.0) @ rot(Z, 20.0)}), 0.6),
        # Golpe: el brazo derecho baja con fuerza y el tronco se cierra
        (21, merge({"pierna.L": rot(X, 30.0), "pierna.R": rot(X, 30.0),
                    "brazo.L": arm_down("L", 20.0),
                    "brazo.R": arm_front("R", 40.0), "antebrazo.R": rot(X, 0.0),
                    "pecho": rot(X, 25.0) @ rot(Z, -15.0)}), 0.55),
        # Seguimiento en la caída
        (25, merge({"pierna.L": rot(X, 15.0), "pierna.R": rot(X, 15.0),
                    "brazo.L": arm_down("L", 0.0),
                    "brazo.R": arm_front("R", -30.0),
                    "pecho": rot(X, 20.0)}), 0.2),
        # Aterrizaje amortiguado
        (30, merge(crouch(30.0), {"brazo.L": arm_down("L", 20.0), "brazo.R": arm_down("R", 20.0),
                                  "pecho": rot(X, 20.0)}), 0.0),
        (42, idle, 0.0),
    ]


def festejo():
    """Festejo: dos brincos con los brazos arriba y un saludo."""
    idle = idle_base()
    arms_up = {"brazo.L": arm_side("L", 75.0), "brazo.R": arm_side("R", 75.0),
               "antebrazo.L": rot(Y, -10.0), "antebrazo.R": rot(Y, 10.0)}
    jump_legs = {"pierna.L": rot(X, 25.0), "pierna.R": rot(X, 25.0)}
    return [
        (0, idle, 0.0),
        (6, merge(crouch(25.0), {"brazo.L": arm_down("L", -30.0), "brazo.R": arm_down("R", -30.0),
                                 "pecho": rot(X, 15.0)}), 0.0),
        (12, merge(jump_legs, arms_up, {"pecho": rot(X, -10.0), "cabeza": rot(X, -15.0)}), 0.35),
        (18, merge(crouch(20.0), arms_up), 0.0),
        (24, merge(jump_legs, arms_up, {"pecho": rot(X, -10.0), "cabeza": rot(X, -15.0)}), 0.35),
        (30, merge(crouch(15.0), arms_up), 0.0),
        # Saludo con la mano derecha, la izquierda baja
        (36, merge(idle, {"brazo.R": arm_side("R", 70.0), "antebrazo.R": rot(Y, 25.0),
                          "cabeza": rot(Z, -15.0)}), 0.0),
        (42, merge(idle, {"brazo.R": arm_side("R", 70.0), "antebrazo.R": rot(Y, -25.0),
                          "cabeza": rot(Z, -15.0)}), 0.0),
        (48, merge(idle, {"brazo.R": arm_side("R", 70.0), "antebrazo.R": rot(Y, 25.0),
                          "cabeza": rot(Z, -15.0)}), 0.0),
        (58, idle, 0.0),
    ]


def marcar():
    """Árbitro: levanta el brazo hacia el lado del equipo que anota y voltea a verlo."""
    idle = idle_base()
    point = merge(idle, {"brazo.R": arm_side("R", 25.0), "antebrazo.R": rot(X, 0.0),
                         "cabeza": rot(Z, -30.0), "pecho": rot(Z, -10.0)})
    return [
        (0, idle, 0.0),
        (8, point, 0.0),
        (12, merge(point, {"brazo.R": arm_side("R", 35.0)}), 0.0),
        (16, point, 0.0),
        (28, point, 0.0),
        (38, idle, 0.0),
    ]


def actions_for(model):
    actions = [("Parado", sampled(idle_pose)), ("Caminar", sampled(walk_pose))]
    if model in PLAYERS:
        actions += [("Saque", saque()), ("Remate", remate()), ("Festejo", festejo())]
    if model == "Arbitro":
        actions += [("Marcar", marcar())]
    return actions


# ---------------------------------------------------------------------------
# Construcción en Blender
# ---------------------------------------------------------------------------
def lowest_foot(arm):
    """Altura (en ejes del esqueleto) del punto más bajo de los pies en la pose actual."""
    bpy.context.view_layer.update()
    zs = []
    for name in ("pie.L", "pie.R"):
        pb = arm.pose.bones[name]
        zs += [pb.head.z, pb.tail.z]
    return min(zs)


def build_action(arm, name, keys, units_per_meter):
    """Crea una acción a partir de una lista de (cuadro, pose, elevación).
    En cada cuadro clave la cadera se ajusta para que el pie más bajo toque el piso y
    después se le suma la elevación (saltos), así nadie flota ni se hunde."""
    action = bpy.data.actions.new(name)
    action.use_fake_user = True          # para que el exportador la incluya
    arm.animation_data.action = action

    hb = arm.pose.bones["cadera"]
    to_local = hb.bone.matrix_local.to_3x3().inverted()
    up = Matrix.Identity(3).col[2]

    for pb in arm.pose.bones:
        pb.rotation_quaternion = Quaternion()
        pb.location = (0.0, 0.0, 0.0)
    ground = lowest_foot(arm)

    for frame, pose, lift in keys:
        for pb in arm.pose.bones:
            q_arm = pose.get(pb.name, Quaternion())
            rest = pb.bone.matrix_local.to_quaternion()
            pb.rotation_quaternion = rest.inverted() @ q_arm @ rest
            pb.location = (0.0, 0.0, 0.0)
        drop = ground - lowest_foot(arm) + lift * units_per_meter
        hb.location = to_local @ (up * drop)
        for pb in arm.pose.bones:
            pb.keyframe_insert("rotation_quaternion", frame=frame)
        hb.keyframe_insert("location", frame=frame)
    return action


def build_actions(arm, model):
    scene = bpy.context.scene
    scene.render.fps = FPS

    mesh = next(o for o in bpy.data.objects if o.type == "MESH" and o.parent == arm)
    units_per_meter = mesh.dimensions.z / HEIGHT_M

    arm.animation_data_create()
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"

    built = {}
    for name, keys in actions_for(model):
        built[name] = build_action(arm, name, keys, units_per_meter)
        print(f"  accion {name}: {keys[-1][0]} cuadros ({keys[-1][0] / FPS:.2f} s)")

    bpy.ops.object.mode_set(mode="OBJECT")
    arm.animation_data.action = built["Parado"]
    return built


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

    print(model)
    build_actions(arm, model)

    out_dir = os.path.join(MODELS, model, "animado")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, model + ".gltf")

    # Exportar solo la malla y el esqueleto: quitar cámara y luz, y asegurar que el
    # esqueleto sea visible (en los .blend de Gabriel está oculto y el exportador lo omitiría)
    for o in list(bpy.data.objects):
        if o.type not in ("MESH", "ARMATURE"):
            bpy.data.objects.remove(o, do_unlink=True)
        else:
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
        export_frame_range=False,      # cada acción con su propia duración
        export_texture_dir="",
        export_image_format="AUTO",
    )
    print(f"OK {os.path.relpath(out, ROOT)}")


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    for model in argv or ["JugadorMX"]:
        export(model)


main()
