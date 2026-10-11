"""암반·붙은 경사로·굴의 외피 통합과 구면 FBX 반입을 검증하는 독립 시험편.

Blender에서 이 파일을 읽은 뒤 build_pilot(repo, version)을 호출한다.
Hollow 완성 지형이 아니며 기존 장면/게임 에셋을 대체하지 않는다.
"""

import json
import types
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector


def mesh_object(name, vertices, faces, collection):
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices, [], faces)
    bm = bmesh.new()
    bm.from_mesh(data)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(data)
    bm.free()
    obj = bpy.data.objects.new(name, data)
    collection.objects.link(obj)
    return obj


def prism(name, bounds, collection, ramp=False):
    x0, x1, y0, y1, h0, h1 = bounds
    vertices = [(x, y, h0) for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))]
    # 경사로의 낮은 끝은 H=0, 높은 끝은 H=h1이며 바닥까지 암반이 채워진다.
    vertices += [(x, y, 0.0 if ramp and y == y0 else h1)
                 for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))]
    faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4),
             (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    return mesh_object(name, vertices, faces, collection)


def topology_report(bm):
    bm.verts.index_update()
    keys = [tuple(sorted(tuple(round(c, 6) for c in v.co) for v in f.verts)) for f in bm.faces]
    remaining = set(bm.verts)
    components = 0
    while remaining:
        stack = [remaining.pop()]
        components += 1
        while stack:
            for edge in stack.pop().link_edges:
                for vertex in edge.verts:
                    if vertex in remaining:
                        remaining.remove(vertex)
                        stack.append(vertex)
    return {
        "vertices": len(bm.verts), "triangles": len(bm.faces),
        "non_manifold_edges": sum(not e.is_manifold for e in bm.edges),
        "inconsistent_edges": sum(e.is_manifold and not e.is_contiguous for e in bm.edges),
        "duplicate_faces": len(keys) - len(set(keys)),
        "degenerate_faces": sum(f.calc_area() < 1e-10 for f in bm.faces),
        "components": components, "signed_volume_m3": bm.calc_volume(signed=True),
    }


def build_pilot(repo, version="V001"):
    repo = Path(repo)
    out = repo / "Saved" / "OctantWorkflow" / version
    scene_name = f"OctantWorkflow_{version}"
    if out.exists() or scene_name in bpy.data.scenes:
        raise ValueError("Pilot version already exists; choose a new version")
    script = repo / "Scripts/Blender/GenerateSliceOctant.py"
    generator = types.ModuleType("octant_generator")
    exec(compile(script.read_text(encoding="utf-8"), str(script), "exec"), generator.__dict__)
    original_objects = set(bpy.data.objects.keys())
    scene = bpy.data.scenes.new(scene_name)
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    bpy.context.window.scene = scene
    source = bpy.data.collections.new(f"PilotSource_{version}")
    export = bpy.data.collections.new(f"PilotExport_{version}")
    scene.collection.children.link(source)
    scene.collection.children.link(export)
    body = prism(f"PilotBody_{version}", (-12, 12, 0, 14, -2, 16), source)
    ramp = prism(f"PilotRamp_{version}", (10, 20, -14, 14, -2, 16), source, ramp=True)
    pocket = prism(f"PilotPocket_{version}", (-6, 6, -1, 6, 0, 7), source)
    for operation, operand in (("UNION", ramp), ("DIFFERENCE", pocket)):
        modifier = body.modifiers.new(operation, "BOOLEAN")
        modifier.operation = operation
        modifier.solver = "EXACT"
        modifier.object = operand
    evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
    evaluated_mesh = evaluated.to_mesh()
    bm = bmesh.new()
    bm.from_mesh(evaluated_mesh)
    evaluated.to_mesh_clear()
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    # 최대 변 길이 2m는 이번 반입 시험편의 분할 기준이다. 전 옥탄트 고정 규격이 아니다.
    for _ in range(8):
        long_edges = [e for e in bm.edges if e.calc_length() > 2.0]
        if not long_edges:
            break
        bmesh.ops.subdivide_edges(bm, edges=long_edges, cuts=1, use_grid_fill=True)
        bmesh.ops.triangulate(bm, faces=bm.faces[:])
    if any(e.calc_length() > 2.0 for e in bm.edges):
        raise ValueError("Subdivision did not converge")
    local_positions = {v: v.co.copy() for v in bm.verts}

    def project(co):
        return Vector(generator.to_blender(generator.point(6000 + co.x * 100, -6500 + co.y * 100, 1500 + co.z * 100)))

    error = max((project((local_positions[e.verts[0]] + local_positions[e.verts[1]]) * 0.5)
                 - (project(local_positions[e.verts[0]]) + project(local_positions[e.verts[1]])) * 0.5).length
                for e in bm.edges)
    for vertex in bm.verts:
        vertex.co = project(local_positions[vertex])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.faces.index_update()
    report = topology_report(bm)
    if (any(report[key] for key in ("non_manifold_edges", "inconsistent_edges", "duplicate_faces", "degenerate_faces"))
            or report["components"] != 1 or report["signed_volume_m3"] <= 0 or error > 0.01):
        raise ValueError(f"Pilot topology gate failed: {report}; chord error {error}")
    objects = generator.split_and_link(bm, f"WorkflowPilot_{version}", export)
    bm.free()
    if len(objects) != 2:
        raise ValueError("Both Floor and Shell are required")
    out.mkdir(parents=True)
    exports = {}
    for obj in objects:
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        obj.data.calc_loop_triangles()
        ue = [(v.co.x * 100, -v.co.y * 100, v.co.z * 100) for v in obj.data.vertices]
        exports[obj.name] = {
            "file": str(out / (obj.name + ".fbx")),
            "triangles": len(obj.data.loop_triangles),
            "bounds_cm": {"min": [min(v[i] for v in ue) for i in range(3)],
                          "max": [max(v[i] for v in ue) for i in range(3)]},
        }
        bpy.ops.export_scene.fbx(filepath=exports[obj.name]["file"], use_selection=True,
                                 object_types={"MESH"}, apply_scale_options="FBX_SCALE_NONE", global_scale=1.0,
                                 axis_forward="-Z", axis_up="Y", mesh_smooth_type="FACE",
                                 add_leaf_bones=False, bake_anim=False)
    for obj in source.objects:
        obj.hide_set(True)
        obj.hide_render = True
    report.update({"blender_version": bpy.app.version_string, "version": version,
                   "max_chord_error_cm": error * 100, "exports": exports,
                   "original_objects_preserved": original_objects.issubset(bpy.data.objects.keys()),
                   "limits": ["No self-intersection test", "No capsule or gameplay validation", "Not production terrain"]})
    if sum(item["triangles"] for item in exports.values()) != report["triangles"]:
        raise ValueError("Floor/Shell partition lost faces")
    (out / "manifest.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    bpy.ops.wm.save_as_mainfile(filepath=str(out / "OctantWorkflow.blend"), check_existing=False, copy=True)
    return report
