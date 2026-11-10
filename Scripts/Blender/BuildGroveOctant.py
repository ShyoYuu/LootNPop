"""빈 백그라운드 Blender에서 Grove 대표 구역을 생성한다.

줄기·뿌리는 중첩 없는 하나의 방사 외피다. 지각의 매입 경계도 같은 윤곽을 쓴다.
"""
import json
import math
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Scripts"))
import GroveOctantSpec as spec

VERSION = "V001"
OUT = ROOT / "Saved/SliceOctant/Grove_00" / VERSION
ART = ROOT / "Art/Octants/Grove_00"
EXPORTS = []
SOURCES = []
SEGMENTS = 48


def blender(p):
    return Vector((p[0]/100, -p[1]/100, p[2]/100))


def ue(p):
    return Vector((p.x*100, -p.y*100, p.z*100))


def topology(bm):
    bm.verts.index_update()
    keys = [tuple(sorted(v.index for v in f.verts)) for f in bm.faces]
    return {"vertices": len(bm.verts), "triangles": len(bm.faces),
            "boundary_edges": sum(e.is_boundary for e in bm.edges),
            "non_manifold_non_boundary": sum(not e.is_manifold and not e.is_boundary for e in bm.edges),
            "inconsistent_edges": sum(e.is_manifold and not e.is_contiguous for e in bm.edges),
            "duplicate_faces": len(keys)-len(set(keys)),
            "degenerate_faces": sum(f.calc_area() < 1e-8 for f in bm.faces)}


def check(bm, closed):
    report = topology(bm)
    assert not any(report[k] for k in ("non_manifold_non_boundary", "inconsistent_edges", "duplicate_faces", "degenerate_faces")), (report,
        [{"area": f.calc_area(), "vertices": [list(v.co) for v in f.verts], "edges": [e.calc_length() for e in f.edges]} for f in bm.faces if f.calc_area() < 1e-8])
    assert not closed or report["boundary_edges"] == 0, report
    return report


def mesh(name, vertices, faces):
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices, [], faces)
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def export(obj, role):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    obj.data.calc_loop_triangles()
    coordinates = [ue(v.co) for v in obj.data.vertices]
    path = OUT / (obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"},
        apply_scale_options="FBX_SCALE_NONE", global_scale=1, axis_forward="-Z", axis_up="Y",
        mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False)
    EXPORTS.append({"name": obj.name, "role": role, "file": str(path),
        "triangles": len(obj.data.loop_triangles),
        "bounds_cm": {side: [function(v[i] for v in coordinates) for i in range(3)]
                      for side, function in (("min", min), ("max", max))}})


def outline(tree, scale=1):
    x, y = tree["xy"]
    return [(x+scale*spec.footprint(tree, a)*math.cos(a),
             y+scale*spec.footprint(tree, a)*math.sin(a))
            for a in (i*math.tau/SEGMENTS for i in range(SEGMENTS))]


def inside(x, y, polygon):
    result = False
    for a, b in zip(polygon, polygon[1:]+polygon[:1]):
        if (a[1] > y) != (b[1] > y) and x < (b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:
            result = not result
    return result


def build_tree(tree):
    x, y = tree["xy"]
    # 높이가 증가할수록 모든 방사 단면이 축소된다. 투영 전 자체 교차가 없다.
    sections = [(-100, 1.0), (120, 1.0), (400, .82), (750, .55), (1100, .28), (1500, 0)]
    sections += [(h, 0) for h in range(2000, tree["height"], 500)]
    sections.append((tree["height"], 0))
    vertices = []
    for h, weight in sections:
        for i in range(SEGMENTS):
            a = i*math.tau/SEGMENTS
            radius = tree["radius"]*(1-.12*max(h, 0)/tree["height"])
            radius += (spec.footprint(tree, a)-tree["radius"])*weight
            xx, yy = x+radius*math.cos(a), y+radius*math.sin(a)
            vertices.append((xx, yy, spec.ground(xx, yy)+h))
    vertices.extend(((x, y, spec.ground(x, y)-100), (x, y, spec.ground(x, y)+tree["height"])))
    faces = []
    for ring in range(len(sections)-1):
        for i in range(SEGMENTS):
            j = (i+1) % SEGMENTS
            faces.append((ring*SEGMENTS+i, ring*SEGMENTS+j, (ring+1)*SEGMENTS+j, (ring+1)*SEGMENTS+i))
    for i in range(SEGMENTS):
        j = (i+1) % SEGMENTS
        faces += [(len(vertices)-2, j, i), (len(vertices)-1, (len(sections)-1)*SEGMENTS+i, (len(sections)-1)*SEGMENTS+j)]
    obj = mesh(tree["id"], vertices, faces)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    for v in bm.verts:
        v.co = blender(spec.point(*v.co))
    # 축 반전 후 닫힌 몸체의 바깥 방향을 다시 맞춘다.
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.normal_update()
    report = check(bm, True)
    bm.verts.index_update()
    coordinates = [v.co.copy() for v in bm.verts]
    groups = {"Floor": [], "Shell": []}
    for f in bm.faces:
        role = "Floor" if f.normal.dot(-f.calc_center_median().normalized()) >= .71 else "Shell"
        groups[role].append(tuple(v.index for v in f.verts))
    assert sum(map(len, groups.values())) == report["triangles"]
    bm.free()
    bpy.data.objects.remove(obj, do_unlink=True)
    for role, faces in groups.items():
        if not faces:
            continue
        used = sorted({v for f in faces for v in f})
        indices = {v: i for i, v in enumerate(used)}
        output = mesh(f"SM_Grove_00_{VERSION}_{tree['id']}_{role}", [coordinates[v] for v in used],
                      [tuple(indices[v] for v in f) for f in faces])
        export(output, role)
    SOURCES.append({"id": tree["id"], "topology": report,
                    "floor_triangles": len(groups["Floor"]), "shell_triangles": len(groups["Shell"]),
                    "ownership": "Single nested radial envelope; no separate root solids"})


def build_crust():
    bpy.ops.import_scene.fbx(filepath=str(OUT.parent / "Baseline/MeadowSeam.fbx"))
    obj = next(o for o in bpy.context.selected_objects if o.type == "MESH")
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    original = {}
    radial, tangent, bitangent = Vector((1, 1, 1)).normalized(), Vector((-1, 1, 0)).normalized(), Vector((-1, -1, 2)).normalized()
    for v in bm.verts:
        p = ue(obj.matrix_world @ v.co)
        if min(p) <= spec.SEAM_MARGIN+.01:
            original[v] = p.copy()
        depth = p.dot(radial)
        v.co = Vector((spec.RADIUS*p.dot(bitangent)/depth, spec.RADIUS*p.dot(tangent)/depth, spec.RADIUS-p.length))
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=.001)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.calc_area() < .0001], context="FACES")
    candidates = {e for e in bm.edges if e.is_boundary}
    sealed = 0
    while candidates:
        group = {candidates.pop()}
        stack = list(group)
        while stack:
            for v in stack.pop().verts:
                for edge in v.link_edges:
                    if edge in candidates:
                        candidates.remove(edge)
                        group.add(edge)
                        stack.append(edge)
        if min(min(spec.point(*v.co)) for e in group for v in e.verts) > spec.SEAM_MARGIN:
            new = bmesh.ops.holes_fill(bm, edges=list(group), sides=0)["faces"]
            bmesh.ops.triangulate(bm, faces=new)
            sealed += 1
    for v in bm.verts:
        p = spec.point(*v.co)
        t = min(1, max(0, (min(p)-spec.SEAM_MARGIN)/spec.SEAM_BAND))
        weight = t*t*(3-2*t)
        v.co.z += weight*(spec.ground(v.co.x, v.co.y)-v.co.z)
    cut_edges = []
    for tree in spec.TREES:
        polygon = outline(tree, .98)
        x0, x1 = min(p[0] for p in polygon)-100, max(p[0] for p in polygon)+100
        y0, y1 = min(p[1] for p in polygon)-100, max(p[1] for p in polygon)+100
        for a, b in zip(polygon, polygon[1:]+polygon[:1]):
            region = [f for f in bm.faces if x0 < f.calc_center_median().x < x1 and y0 < f.calc_center_median().y < y1]
            edges = {e for f in region for e in f.edges}
            vertices = {v for f in region for v in f.verts}
            bmesh.ops.bisect_plane(bm, geom=list(vertices)+list(edges)+region, dist=.001,
                plane_co=Vector((*a, 0)), plane_no=Vector((a[1]-b[1], b[0]-a[0], 0)), clear_inner=False, clear_outer=False)
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if inside(*f.calc_center_median()[:2], polygon)], context="FACES")
        cut_edges.append({"id": tree["id"], "cut_scale": .98, "base_burial_cm": 100})
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    for v in bm.verts:
        v.co = blender(original[v]) if v in original else blender(spec.point(*v.co))
    inner = [v for v in bm.verts if min(ue(v.co)) > spec.SEAM_MARGIN+.01]
    # 절단면의 교차로 생긴 0.06cm 이하 변만 정리한다. 보존 띠는 대상에서 제외한다.
    bmesh.ops.remove_doubles(bm, verts=inner, dist=.0006)
    bmesh.ops.dissolve_degenerate(bm, edges=[e for e in bm.edges if all(min(ue(v.co)) > spec.SEAM_MARGIN+.01 for v in e.verts)], dist=.0006)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.normal_update()
    if sum(f.normal.dot(-f.calc_center_median().normalized()) for f in bm.faces) < 0:
        for f in bm.faces:
            f.normal_flip()
    report = check(bm, False)
    report.update({"sealed_meadow_openings": sealed, "tree_cuts": cut_edges})
    bm.to_mesh(obj.data)
    bm.free()
    obj.matrix_world.identity()
    obj.name = f"SM_Octant_Grove_00_{VERSION}_R30000"
    export(obj, "Crust")
    SOURCES.append({"id": "Crust", "topology": report})


def build():
    assert not (OUT / "manifest.json").exists(), "Choose a new geometry version"
    assert not (ART / f"Grove_00_{VERSION}.blend").exists()
    OUT.mkdir(parents=True, exist_ok=True)
    ART.mkdir(parents=True, exist_ok=True)
    for tree in spec.TREES:
        build_tree(tree)
    build_crust()
    manifest = {"name": "Grove_00", "version": VERSION, "stage": "Representative",
                "radius_cm": spec.RADIUS, "seam_margin_cm": spec.SEAM_MARGIN,
                "seam_transition_cm": spec.SEAM_BAND, "trees": spec.TREES,
                "exports": EXPORTS, "sources": SOURCES,
                "limits": ["Native seam snap required", "Engine collision, bake and PIE pending",
                           "Crust cut edge embedded 2 percent inside closed tree base"]}
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    (OUT / "placement.json").write_text(json.dumps(spec.placement(), indent=2), encoding="utf-8")
    bpy.ops.wm.save_as_mainfile(filepath=str(ART / f"Grove_00_{VERSION}.blend"), check_existing=False)
    print("GROVE_GEOMETRY_SUCCESS", json.dumps({"exports": len(EXPORTS), "triangles": sum(e["triangles"] for e in EXPORTS)}))


if __name__ == "__main__":
    build()
