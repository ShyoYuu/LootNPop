"""Grove 저장 원본의 바닥·경로·사선·훅과 외형을 검사한다. 실제 Chaos/Mover 검사는 아니다."""
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Scripts"))
import GroveOctantSpec as spec

OUT = ROOT / "Saved/SliceOctant/Grove_00/V001"
bpy.ops.wm.open_mainfile(filepath=str(ROOT / "Art/Octants/Grove_00/Grove_00_V001.blend"))
placement = spec.placement()
(OUT / "placement.json").write_text(json.dumps(placement, indent=2), encoding="utf-8")
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH" and o.name.startswith("SM_")]


def bl(p):
    return Vector((p[0]/100, -p[1]/100, p[2]/100))


vertices, polygons = [], []
for obj in meshes:
    obj.data.calc_loop_triangles()
    offset = len(vertices)
    vertices.extend(v.co for v in obj.data.vertices)
    polygons.extend(tuple(offset+i for i in tri.vertices) for tri in obj.data.loop_triangles)
bvh = BVHTree.FromPolygons(vertices, polygons, all_triangles=True)


def ray(a, b):
    start, end = bl(a), bl(b)
    delta = end-start
    hit = bvh.ray_cast(start, delta.normalized(), delta.length)
    return hit[3]*100 if hit[0] is not None else None


def floor(x, y):
    h = spec.ground(x, y)
    a, b = spec.point(x, y, h+100), spec.point(x, y, h-200)
    start, end = bl(a), bl(b)
    hit = bvh.ray_cast(start, (end-start).normalized(), (end-start).length)
    assert hit[0] is not None, ("Missing floor", x, y)
    slope = math.degrees(math.acos(max(-1, min(1, hit[1].dot(-hit[0].normalized())))))
    assert slope < 35, ("Steep floor", x, y, slope)
    # 이음매 보존/전이 띠는 Meadow의 원래 높이를 따른다.
    if min(spec.point(x, y, 0)) > spec.SEAM_MARGIN+spec.SEAM_BAND:
        assert abs(hit[3]*100-100) < 8, ("Floor mismatch", x, y, hit[3]*100)
    return {"local": [x, y, h], "hit_distance_cm": hit[3]*100,
            "error_cm": abs(hit[3]*100-100), "slope_deg": slope,
            "in_seam_transition": min(spec.point(x, y, 0)) <= spec.SEAM_MARGIN+spec.SEAM_BAND}


nodes = {n["id"]: n for n in placement["nodes"]}
checks = []
routes = []
for route in placement["routes"]:
    length = 0
    for first, last in zip(route["nodes"], route["nodes"][1:]):
        a, b = Vector(nodes[first]["xy"]), Vector(nodes[last]["xy"])
        direction = (b-a).normalized()
        side = Vector((-direction.y, direction.x))
        count = math.ceil((b-a).length/150)
        previous = None
        for i in range(count+1):
            center = a.lerp(b, i/count)
            p = Vector(spec.point(*center, spec.ground(*center)))
            if previous is not None:
                length += (p-previous).length
            previous = p
            for offset in (-400, 0, 400):
                checks.append(floor(*(center+offset*side)))
    routes.append({"id": route["id"], "center_length_cm": length, "tested_half_width_parameter_cm": 400})
pods = []
for pod in placement["pods"]:
    for i in range(9):
        a = i*math.tau/8
        r = 0 if i == 8 else 250
        pods.append(floor(pod["xy"][0]+r*math.cos(a), pod["xy"][1]+r*math.sin(a)))
a, b = nodes["ClearingA"]["local"], nodes["ClearingB"]["local"]
blocked = ray(spec.point(a[0], a[1], a[2]+190), spec.point(b[0], b[1], b[2]+190))
assert blocked is not None, "Trunk does not break line of sight between clearings"
hook_checks = []
for anchor in placement["anchors"]:
    a = nodes[anchor["from"]]["local"]
    start = spec.point(a[0], a[1], a[2]+190)
    end = spec.point(*anchor["local"])
    distance = (Vector(end)-Vector(start)).length
    assert distance <= anchor["max_distance"], ("Hook out of range", distance)
    assert ray(start, end) is None, "Hook visibility blocked"
    landing = floor(*anchor["landing"])
    hook_checks.append({"id": anchor["id"], "distance_cm": distance, "landing": landing})
report = {"route_floor_rays": len(checks), "pod_floor_rays": len(pods),
          "max_interior_floor_error_cm": max(v["error_cm"] for v in checks+pods if not v["in_seam_transition"]),
          "max_preserved_seam_height_difference_cm": max(v["error_cm"] for v in checks+pods if v["in_seam_transition"]),
          "max_floor_slope_deg": max(v["slope_deg"] for v in checks+pods),
          "routes": routes, "cover_hit_cm": blocked, "hooks": hook_checks,
          "floor_checks": checks+pods,
          "limits": ["Blender BVH rays only; not Chaos capsules, Mover, hook inertia or NPC combat"]}
(OUT / "geometry-verification.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
(OUT / "native-probes.json").write_text(json.dumps([
    [*p["local"], p["hit_distance_cm"]] for p in checks+pods], separators=(",", ":")), encoding="utf-8")

scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "OBJECT"
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.cavity_type = "BOTH"
for obj in meshes:
    obj.color = (.166, .242, .136, 1) if "Octant" in obj.name else (.133, .076, .041, 1)
scene.render.resolution_x, scene.render.resolution_y = 1400, 1000
scene.render.resolution_percentage = 100
camera = bpy.data.objects.new("GroveReviewCamera", bpy.data.cameras.new("GroveReviewCamera"))
scene.collection.objects.link(camera)
scene.camera = camera
camera.data.clip_end = 2000
for label, local, target, orthographic in (
    ("RepresentativeOverview", [-9500, 1000, 17000], [-9500, 1000, 0], True),
    ("ClearingAView", [-11600, -4600, 500], [-10000, 200, 2200], False),
):
    eye, aim = bl(spec.point(*local)), bl(spec.point(*target))
    forward = (aim-eye).normalized()
    up = bl(-Vector(spec.point(*local)).normalized()).normalized()
    right = forward.cross(up).normalized() if not orthographic else Vector((-.707, -.707, 0))
    up = right.cross(forward).normalized()
    camera.location = eye
    camera.rotation_euler = Matrix((right, up, -forward)).transposed().to_euler()
    camera.data.type = "ORTHO" if orthographic else "PERSP"
    camera.data.ortho_scale = 190
    camera.data.lens = 20
    scene.render.filepath = str(OUT / (label+".png"))
    bpy.ops.render.render(write_still=True)
print("GROVE_GEOMETRY_VERIFIED", json.dumps({k: v for k, v in report.items() if k != "floor_checks"}))
