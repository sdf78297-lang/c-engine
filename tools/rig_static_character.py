import argparse
import json
import math
from pathlib import Path

import bpy
from mathutils import Euler, Matrix, Vector


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--report", required=True)
    argv = []
    if "--" in __import__("sys").argv:
        argv = __import__("sys").argv[__import__("sys").argv.index("--") + 1 :]
    return parser.parse_args(argv)


def clear_scene():
    bpy.ops.object.mode_set(mode="OBJECT") if bpy.ops.object.mode_set.poll() else None
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()


def import_model(path):
    bpy.ops.import_scene.gltf(filepath=str(path))
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    if not meshes:
        raise RuntimeError("Imported file has no mesh objects")
    if len(meshes) > 1:
        bpy.ops.object.select_all(action="DESELECT")
        for obj in meshes:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        bpy.ops.object.join()
        meshes = [bpy.context.view_layer.objects.active]
    mesh = meshes[0]
    bpy.ops.object.select_all(action="DESELECT")
    mesh.select_set(True)
    bpy.context.view_layer.objects.active = mesh
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    mesh.name = "nurse_actor_mesh"
    mesh.data.name = "nurse_actor_mesh"
    return mesh


def mesh_bounds_world(mesh):
    corners = [mesh.matrix_world @ Vector(corner) for corner in mesh.bound_box]
    min_v = Vector((min(v.x for v in corners), min(v.y for v in corners), min(v.z for v in corners)))
    max_v = Vector((max(v.x for v in corners), max(v.y for v in corners), max(v.z for v in corners)))
    return min_v, max_v


def create_armature(bounds_min, bounds_max):
    center = (bounds_min + bounds_max) * 0.5
    height = bounds_max.z - bounds_min.z
    half_span = max(abs(bounds_min.x - center.x), abs(bounds_max.x - center.x))
    torso_half = max(half_span * 0.25, 0.18)
    leg_half = max(half_span * 0.09, 0.07)

    def z(t):
        return bounds_min.z + height * t

    cy = center.y
    bpy.ops.object.armature_add(enter_editmode=True, align="WORLD", location=(0.0, 0.0, 0.0))
    armature = bpy.context.object
    armature.name = "nurse_actor_rig"
    armature.data.name = "nurse_actor_rig"
    armature.show_in_front = True

    edit_bones = armature.data.edit_bones
    edit_bones.remove(edit_bones[0])

    created = {}

    def bone(name, head, tail, parent=None, deform=True):
        b = edit_bones.new(name)
        b.head = Vector(head)
        b.tail = Vector(tail)
        b.roll = 0.0
        b.use_deform = deform
        if parent is not None:
            b.parent = created[parent]
        created[name] = b
        return b

    cx = center.x
    root = bone("root", (cx, cy, z(0.46)), (cx, cy, z(0.52)), deform=False)
    bone("pelvis", (cx, cy, z(0.44)), (cx, cy, z(0.56)), "root")
    bone("spine_01", (cx, cy, z(0.56)), (cx, cy, z(0.68)), "pelvis")
    bone("chest", (cx, cy, z(0.68)), (cx, cy, z(0.80)), "spine_01")
    bone("neck", (cx, cy, z(0.80)), (cx, cy, z(0.87)), "chest")
    bone("head", (cx, cy, z(0.87)), (cx, cy, z(0.98)), "neck")

    shoulder_z = z(0.765)
    shoulder_l = cx + torso_half
    shoulder_r = cx - torso_half
    elbow_l = cx + half_span * 0.58
    elbow_r = cx - half_span * 0.58
    wrist_l = cx + half_span * 0.84
    wrist_r = cx - half_span * 0.84
    hand_l = cx + half_span * 0.98
    hand_r = cx - half_span * 0.98
    bone("upper_arm.L", (shoulder_l, cy, shoulder_z), (elbow_l, cy, shoulder_z), "chest")
    bone("forearm.L", (elbow_l, cy, shoulder_z), (wrist_l, cy, shoulder_z), "upper_arm.L")
    bone("hand.L", (wrist_l, cy, shoulder_z), (hand_l, cy, shoulder_z), "forearm.L")
    bone("upper_arm.R", (shoulder_r, cy, shoulder_z), (elbow_r, cy, shoulder_z), "chest")
    bone("forearm.R", (elbow_r, cy, shoulder_z), (wrist_r, cy, shoulder_z), "upper_arm.R")
    bone("hand.R", (wrist_r, cy, shoulder_z), (hand_r, cy, shoulder_z), "forearm.R")

    hip_z = z(0.44)
    knee_z = z(0.25)
    ankle_z = z(0.075)
    foot_z = z(0.035)
    foot_y = cy - max((bounds_max.y - bounds_min.y) * 0.42, 0.12)
    for suffix, sign in ((".L", 1.0), ((".R", -1.0))):
        x = cx + sign * leg_half
        knee_x = cx + sign * leg_half * 0.94
        ankle_x = cx + sign * leg_half * 0.85
        bone(f"thigh{suffix}", (x, cy, hip_z), (knee_x, cy, knee_z), "pelvis")
        bone(f"shin{suffix}", (knee_x, cy, knee_z), (ankle_x, cy, ankle_z), f"thigh{suffix}")
        bone(f"foot{suffix}", (ankle_x, cy, ankle_z), (ankle_x, foot_y, foot_z), f"shin{suffix}")

    bpy.ops.object.mode_set(mode="OBJECT")
    return armature


def point_segment_distance(point, a, b):
    ab = b - a
    denom = ab.dot(ab)
    if denom <= 1e-8:
        return (point - a).length
    t = max(0.0, min(1.0, (point - a).dot(ab) / denom))
    closest = a + ab * t
    return (point - closest).length


def bind_mesh(mesh, armature, bounds_min, bounds_max):
    for group in mesh.vertex_groups:
        mesh.vertex_groups.remove(group)

    deform_bones = [bone for bone in armature.data.bones if bone.use_deform]
    groups = {bone.name: mesh.vertex_groups.new(name=bone.name) for bone in deform_bones}
    segments = {
        bone.name: (bone.head_local.copy(), bone.tail_local.copy())
        for bone in deform_bones
    }

    center = (bounds_min + bounds_max) * 0.5
    height = bounds_max.z - bounds_min.z
    half_span = max(abs(bounds_min.x - center.x), abs(bounds_max.x - center.x))
    torso_half = max(half_span * 0.28, 0.2)
    leg_half = max(half_span * 0.11, 0.08)

    inv = armature.matrix_world.inverted()
    softness = max(height * 0.055, 0.06)

    def region_penalty(name, p):
        ax = abs(p.x - center.x)
        z_norm = (p.z - bounds_min.z) / max(height, 1e-5)
        penalty = 0.0
        is_arm = "arm" in name or "hand" in name
        is_leg = "thigh" in name or "shin" in name or "foot" in name
        is_torso = name in {"pelvis", "spine_01", "chest", "neck", "head"}
        if is_arm and ax < torso_half * 0.82:
            penalty += 0.35
        if is_leg and z_norm > 0.52:
            penalty += 0.55
        if is_torso and ax > torso_half * 1.2 and z_norm < 0.83:
            penalty += 0.22
        if name.endswith(".L") and p.x < center.x - leg_half * 0.2:
            penalty += 0.28
        if name.endswith(".R") and p.x > center.x + leg_half * 0.2:
            penalty += 0.28
        if name == "head" and z_norm < 0.78:
            penalty += 0.42
        if name in {"pelvis", "spine_01", "chest"} and z_norm > 0.86:
            penalty += 0.36
        return penalty

    for vertex in mesh.data.vertices:
        p = inv @ (mesh.matrix_world @ vertex.co)
        scored = []
        for name, (a, b) in segments.items():
            distance = point_segment_distance(p, a, b) + region_penalty(name, p)
            scored.append((distance, name))
        scored.sort(key=lambda item: item[0])
        top = scored[:4]
        raw = []
        for distance, name in top:
            w = 1.0 / (distance * distance + softness * softness)
            raw.append((name, w))
        total = sum(weight for _, weight in raw) or 1.0
        for name, weight in raw:
            groups[name].add([vertex.index], weight / total, "ADD")

    modifier = mesh.modifiers.new("nurse_actor_skin", "ARMATURE")
    modifier.object = armature
    modifier.use_vertex_groups = True
    bpy.ops.object.select_all(action="DESELECT")
    mesh.select_set(True)
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature


def solve_basis_for_direction(armature, bone_name, target_direction):
    pose_bone = armature.pose.bones[bone_name]
    bone = armature.data.bones[bone_name]
    rest_dir = (bone.tail_local - bone.head_local).normalized()
    target = Vector(target_direction).normalized()
    q = rest_dir.rotation_difference(target)
    desired_rot = q.to_matrix().to_4x4() @ bone.matrix_local.to_3x3().to_4x4()
    desired = Matrix.Translation(bone.head_local) @ desired_rot.to_3x3().to_4x4()
    if pose_bone.parent is not None:
        parent_pose = pose_bone.parent.matrix.copy()
        parent_rest_inv = pose_bone.parent.bone.matrix_local.inverted()
        basis = (parent_pose @ parent_rest_inv @ bone.matrix_local).inverted() @ desired
    else:
        basis = bone.matrix_local.inverted() @ desired
    pose_bone.rotation_mode = "QUATERNION"
    pose_bone.rotation_quaternion = basis.to_quaternion()


def add_local_rotation(armature, bone_name, euler_xyz):
    pose_bone = armature.pose.bones[bone_name]
    pose_bone.rotation_mode = "QUATERNION"
    delta = Euler((
        math.radians(euler_xyz[0]),
        math.radians(euler_xyz[1]),
        math.radians(euler_xyz[2]),
    ), "XYZ").to_quaternion()
    pose_bone.rotation_quaternion = (pose_bone.rotation_quaternion @ delta).normalized()


def reset_pose(armature):
    for pose_bone in armature.pose.bones:
        pose_bone.rotation_mode = "QUATERNION"
        pose_bone.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
        pose_bone.location = (0.0, 0.0, 0.0)
        pose_bone.scale = (1.0, 1.0, 1.0)


def apply_base_pose(armature):
    reset_pose(armature)
    solve_basis_for_direction(armature, "upper_arm.L", (0.18, -0.04, -0.98))
    solve_basis_for_direction(armature, "forearm.L", (0.10, -0.03, -0.99))
    solve_basis_for_direction(armature, "hand.L", (0.16, -0.02, -0.98))
    solve_basis_for_direction(armature, "upper_arm.R", (-0.18, -0.04, -0.98))
    solve_basis_for_direction(armature, "forearm.R", (-0.10, -0.03, -0.99))
    solve_basis_for_direction(armature, "hand.R", (-0.16, -0.02, -0.98))
    bpy.context.view_layer.update()


def bake_current_deformed_mesh(mesh):
    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = mesh.evaluated_get(depsgraph)
    evaluated_mesh = evaluated.to_mesh()
    try:
        if len(evaluated_mesh.vertices) != len(mesh.data.vertices):
            raise RuntimeError("Cannot bake posed mesh: evaluated topology changed")
        for source, target in zip(evaluated_mesh.vertices, mesh.data.vertices):
            target.co = source.co
        mesh.data.update()
    finally:
        evaluated.to_mesh_clear()


def apply_current_pose_as_rest(armature):
    bpy.ops.object.mode_set(mode="OBJECT") if bpy.ops.object.mode_set.poll() else None
    bpy.ops.object.select_all(action="DESELECT")
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.pose.select_all(action="SELECT")
    bpy.ops.pose.armature_apply(selected=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    reset_pose(armature)
    bpy.context.view_layer.update()


def key_all_pose_bones(armature, frame):
    for pose_bone in armature.pose.bones:
        pose_bone.keyframe_insert(data_path="rotation_quaternion", frame=frame)
        pose_bone.keyframe_insert(data_path="location", frame=frame)
        pose_bone.keyframe_insert(data_path="scale", frame=frame)


def make_action(armature, name, frames):
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    armature.animation_data_create()
    armature.animation_data.action = action
    for frame, pose_fn in frames:
        bpy.context.scene.frame_set(frame)
        apply_base_pose(armature)
        pose_fn()
        bpy.context.view_layer.update()
        key_all_pose_bones(armature, frame)
    action.frame_start = frames[0][0]
    action.frame_end = frames[-1][0]
    for fcurve in getattr(action, "fcurves", []):
        for key in fcurve.keyframe_points:
            key.interpolation = "BEZIER"
    return action


def create_actions(armature):
    def noop():
        pass

    def breathe_in():
        add_local_rotation(armature, "pelvis", (0.4, 0.0, 0.9))
        add_local_rotation(armature, "spine_01", (2.6, 0.0, 1.4))
        add_local_rotation(armature, "chest", (6.2, 0.0, 2.2))
        add_local_rotation(armature, "neck", (-1.8, 0.0, -2.2))
        add_local_rotation(armature, "head", (-3.4, 0.0, -3.8))
        solve_basis_for_direction(armature, "upper_arm.L", (0.23, -0.06, -0.97))
        solve_basis_for_direction(armature, "forearm.L", (0.18, -0.08, -0.98))
        solve_basis_for_direction(armature, "upper_arm.R", (-0.13, -0.04, -0.99))
        solve_basis_for_direction(armature, "forearm.R", (-0.08, -0.05, -1.0))

    def breathe_hold():
        add_local_rotation(armature, "pelvis", (0.0, 0.0, -0.75))
        add_local_rotation(armature, "spine_01", (0.8, 0.0, -1.4))
        add_local_rotation(armature, "chest", (1.8, 0.0, -2.5))
        add_local_rotation(armature, "neck", (0.6, 0.0, 2.2))
        add_local_rotation(armature, "head", (1.4, 0.0, 3.2))
        solve_basis_for_direction(armature, "upper_arm.L", (0.17, -0.05, -0.99))
        solve_basis_for_direction(armature, "forearm.L", (0.10, -0.05, -1.0))
        solve_basis_for_direction(armature, "upper_arm.R", (-0.20, -0.06, -0.98))
        solve_basis_for_direction(armature, "forearm.R", (-0.16, -0.07, -0.99))

    def breathe_out():
        add_local_rotation(armature, "pelvis", (-0.5, 0.0, 0.25))
        add_local_rotation(armature, "spine_01", (-2.0, 0.0, -0.35))
        add_local_rotation(armature, "chest", (-4.8, 0.0, -0.9))
        add_local_rotation(armature, "neck", (2.2, 0.0, 1.4))
        add_local_rotation(armature, "head", (3.9, 0.0, 2.0))
        solve_basis_for_direction(armature, "upper_arm.L", (0.13, -0.04, -0.99))
        solve_basis_for_direction(armature, "forearm.L", (0.06, -0.05, -1.0))
        solve_basis_for_direction(armature, "upper_arm.R", (-0.16, -0.05, -0.99))
        solve_basis_for_direction(armature, "forearm.R", (-0.11, -0.05, -0.99))

    def attend_patient():
        add_local_rotation(armature, "pelvis", (-0.8, 0.0, 0.55))
        add_local_rotation(armature, "spine_01", (-3.2, 0.0, 0.9))
        add_local_rotation(armature, "chest", (-7.0, 0.0, 1.7))
        add_local_rotation(armature, "neck", (3.0, 0.0, -2.0))
        add_local_rotation(armature, "head", (5.2, 0.0, -3.2))
        solve_basis_for_direction(armature, "upper_arm.L", (0.20, -0.05, -0.98))
        solve_basis_for_direction(armature, "forearm.L", (0.16, -0.07, -0.99))
        solve_basis_for_direction(armature, "upper_arm.R", (-0.12, -0.03, -0.99))
        solve_basis_for_direction(armature, "forearm.R", (-0.07, -0.04, -1.0))

    def check_monitor():
        add_local_rotation(armature, "pelvis", (0.2, 0.0, -0.4))
        add_local_rotation(armature, "spine_01", (0.4, 0.0, -2.2))
        add_local_rotation(armature, "chest", (1.0, 0.0, -5.5))
        add_local_rotation(armature, "neck", (-0.5, 0.0, 5.4))
        add_local_rotation(armature, "head", (-1.2, 0.0, 8.2))
        solve_basis_for_direction(armature, "upper_arm.L", (0.15, -0.04, -0.99))
        solve_basis_for_direction(armature, "forearm.L", (0.08, -0.04, -1.0))
        solve_basis_for_direction(armature, "upper_arm.R", (-0.24, -0.06, -0.97))
        solve_basis_for_direction(armature, "forearm.R", (-0.19, -0.07, -0.98))

    make_action(armature, "sit_idle", [
        (1, breathe_out),
        (24, attend_patient),
        (48, breathe_in),
        (72, check_monitor),
        (96, breathe_hold),
        (120, attend_patient),
        (144, breathe_in),
        (168, check_monitor),
        (192, breathe_out),
    ])

    def pre_lean():
        add_local_rotation(armature, "spine_01", (0.7, 0.0, 0.5))
        add_local_rotation(armature, "chest", (1.2, 0.0, 0.8))
        add_local_rotation(armature, "head", (-0.8, 0.0, -0.6))

    def lean():
        add_local_rotation(armature, "spine_01", (-2.6, 0.0, -1.5))
        add_local_rotation(armature, "chest", (-5.2, 0.0, -2.4))
        add_local_rotation(armature, "neck", (1.7, 0.0, 1.3))
        add_local_rotation(armature, "head", (3.0, 0.0, 1.9))

    def lean_settle():
        add_local_rotation(armature, "spine_01", (-2.1, 0.0, -1.2))
        add_local_rotation(armature, "chest", (-4.4, 0.0, -2.0))
        add_local_rotation(armature, "neck", (1.2, 0.0, 1.0))
        add_local_rotation(armature, "head", (2.2, 0.0, 1.5))

    make_action(armature, "lean_to_patient", [(1, noop), (22, pre_lean), (58, lean), (104, lean_settle), (150, lean)])

    def monitor_lead():
        add_local_rotation(armature, "neck", (0.0, 0.0, 5.0))
        add_local_rotation(armature, "head", (0.0, 0.0, 8.0))

    def monitor():
        add_local_rotation(armature, "chest", (0.0, 0.0, 3.5))
        add_local_rotation(armature, "neck", (0.0, 0.0, 9.0))
        add_local_rotation(armature, "head", (0.0, 0.0, 12.0))

    def monitor_settle():
        add_local_rotation(armature, "chest", (0.0, 0.0, 2.4))
        add_local_rotation(armature, "neck", (0.0, 0.0, 6.0))
        add_local_rotation(armature, "head", (0.0, 0.0, 8.0))

    make_action(armature, "look_to_monitor", [(1, noop), (18, monitor_lead), (46, monitor), (82, monitor), (116, monitor_settle)])

    def brace_anticipation():
        add_local_rotation(armature, "chest", (1.2, 0.0, 1.0))
        add_local_rotation(armature, "head", (-0.8, 0.0, -0.5))

    def brace():
        add_local_rotation(armature, "chest", (-2.0, 0.0, -2.1))
        solve_basis_for_direction(armature, "upper_arm.L", (0.22, -0.07, -0.97))
        solve_basis_for_direction(armature, "forearm.L", (0.13, -0.07, -0.99))
        solve_basis_for_direction(armature, "upper_arm.R", (-0.16, -0.03, -0.99))
        solve_basis_for_direction(armature, "forearm.R", (-0.08, -0.03, -1.0))

    def brace_recover():
        add_local_rotation(armature, "chest", (-0.6, 0.0, -0.8))
        add_local_rotation(armature, "head", (0.8, 0.0, 0.5))

    make_action(armature, "brace_during_jolt", [(1, noop), (10, brace_anticipation), (24, brace), (52, brace_recover), (86, noop)])

    def driver_lead():
        add_local_rotation(armature, "neck", (0.0, 0.0, -5.0))
        add_local_rotation(armature, "head", (0.0, 0.0, -8.0))

    def call_driver():
        add_local_rotation(armature, "spine_01", (0.0, 0.0, -2.4))
        add_local_rotation(armature, "chest", (0.0, 0.0, -6.5))
        add_local_rotation(armature, "neck", (0.0, 0.0, -9.5))
        add_local_rotation(armature, "head", (0.0, 0.0, -12.5))
        solve_basis_for_direction(armature, "upper_arm.R", (-0.18, -0.04, -0.98))
        solve_basis_for_direction(armature, "forearm.R", (-0.10, -0.04, -0.99))

    def call_driver_settle():
        add_local_rotation(armature, "spine_01", (0.0, 0.0, -1.6))
        add_local_rotation(armature, "chest", (0.0, 0.0, -4.2))
        add_local_rotation(armature, "neck", (0.0, 0.0, -7.0))
        add_local_rotation(armature, "head", (0.0, 0.0, -9.0))

    make_action(armature, "call_driver_urgent", [(1, noop), (18, driver_lead), (52, call_driver), (92, call_driver), (128, call_driver_settle)])
    armature.animation_data.action = bpy.data.actions["sit_idle"]


def export_glb(output):
    bpy.ops.export_scene.gltf(
        filepath=str(output),
        export_format="GLB",
        export_skins=True,
        export_animations=True,
        export_animation_mode="ACTIONS",
        export_force_sampling=True,
        export_frame_step=1,
        export_influence_nb=4,
        export_all_influences=False,
        export_materials="EXPORT",
        export_cameras=False,
        export_lights=False,
        export_yup=True,
        export_apply=False,
    )


def main():
    args = parse_args()
    input_path = Path(args.input)
    output_path = Path(args.output)
    report_path = Path(args.report)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.parent.mkdir(parents=True, exist_ok=True)

    clear_scene()
    mesh = import_model(input_path)
    bounds_min, bounds_max = mesh_bounds_world(mesh)
    armature = create_armature(bounds_min, bounds_max)
    bind_mesh(mesh, armature, bounds_min, bounds_max)
    apply_base_pose(armature)
    bake_current_deformed_mesh(mesh)
    apply_current_pose_as_rest(armature)
    bounds_min, bounds_max = mesh_bounds_world(mesh)
    create_actions(armature)
    export_glb(output_path)

    report = {
        "input": str(input_path),
        "output": str(output_path),
        "mesh": {
            "name": mesh.name,
            "vertices": len(mesh.data.vertices),
            "triangles": len(mesh.data.polygons),
            "materials": [slot.material.name if slot.material else "" for slot in mesh.material_slots],
            "boundsMin": [round(float(v), 4) for v in bounds_min],
            "boundsMax": [round(float(v), 4) for v in bounds_max],
        },
        "armature": {
            "name": armature.name,
            "bones": [bone.name for bone in armature.data.bones],
        },
        "actions": [action.name for action in bpy.data.actions],
    }
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
