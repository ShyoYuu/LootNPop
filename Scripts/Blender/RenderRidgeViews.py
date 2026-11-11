"""Ridge 생성 형상의 검토용 렌더. 개요·진입 분지 눈높이·마루 눈높이 시점을 Workbench로 찍는다.

실행: blender -b --factory-startup --python Scripts/Blender/RenderRidgeViews.py
출력: Saved/SliceOctant/Ridge_00/<VERSION>/Render_*.png (원본 .blend는 저장하지 않는다)
"""
import math
import sys
from pathlib import Path
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/"Scripts"))
import RidgeOctantSpec as spec
OUT = ROOT/"Saved/SliceOctant/Ridge_00"/spec.VERSION
bpy.ops.wm.open_mainfile(filepath=str(ROOT/f"Art/Octants/Ridge_00/Ridge_00_{spec.VERSION}.blend"))
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "OBJECT"
scene.display.shading.show_cavity = True
scene.display.shading.show_shadows = True
scene.render.resolution_x, scene.render.resolution_y = 1600, 900
scene.world.color = (0.55, 0.6, 0.7) if scene.world else None
for obj in scene.objects:
    if obj.type == "MESH" and obj.name.startswith("SM_"):
        obj.color = (0.576, 0.522, 0.647, 1) if obj.name.startswith("SM_Octant") else (0.42, 0.38, 0.46, 1)
# Pod 위치 표시 기둥.
for pod in spec.active(spec.PODS):
    base = Vector(spec.point(*spec.ground_point(pod["xy"])))
    up = -base.normalized()
    bpy.ops.mesh.primitive_cylinder_add(radius=1.5, depth=30, location=(Vector((base.x, -base.y, base.z))/100+Vector((up.x, -up.y, up.z))*15))
    marker = bpy.context.active_object
    marker.rotation_mode = "QUATERNION"
    marker.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(Vector((up.x, -up.y, up.z)))
    marker.color = (1, 0.65, 0.1, 1)


def bl(p):
    return Vector((p[0]/100, -p[1]/100, p[2]/100))


def shot(name, eye, target, lens=24):
    cam_data = bpy.data.cameras.new(name)
    cam_data.lens = lens
    cam_data.clip_end = 3000
    cam = bpy.data.objects.new(name, cam_data)
    scene.collection.objects.link(cam)
    e, t = bl(spec.point(*eye)), bl(spec.point(*target))
    up = -e.normalized()
    forward = (t-e).normalized()
    right = forward.cross(up).normalized()
    true_up = right.cross(forward)
    from mathutils import Matrix
    rot = Matrix((right, true_up, -forward)).transposed()
    cam.matrix_world = Matrix.Translation(e)@rot.to_4x4()
    scene.camera = cam
    scene.render.filepath = str(OUT/f"Render_{name}.png")
    bpy.ops.render.render(write_still=True)


for name, eye, target, lens in spec.VIEWS:
    if eye[2] is None:
        eye = (eye[0], eye[1], spec.ground(eye[0], eye[1])+170)
    shot(name, eye, target, lens)
