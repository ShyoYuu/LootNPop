"""Hollow 서쪽 접합 한 구역의 편집 원본과 구면 외피를 만든다.

기존 장면은 보존한다. 구역 경계의 절단 뚜껑은 출력에서 제거하며,
최종 조립 시 기존 Generated 단면과의 접합 검사는 별도로 수행한다.
"""

import json
import math
import types
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree


BOUNDS = (3300, 8400, -10000, -3000)
# Flyer01 출현 요구 2630cm보다 여유를 남긴다. 바닥 H1500은 보존한다.
POCKET_ROOF = 4150
WEST = [(-10985, -7500), (-7166, -7400), (-4777, -7350), (-2693, -7294),
        (3536, -7280), (8934, -5649), (12972, -1358), (14135, 3319),
        (13862, 6687), (12576, 6878), (12404, 3813), (11133, 335),
        (7681, -2381), (3243, -3291), (-2861, -3297), (-5721, -3607),
        (-8785, -4443), (-12311, -5812)]


def build_region(repo, version="V001"):
    repo = Path(repo)
    out = repo / "Saved/SliceOctant/Hollow_00" / f"BackPocket{version}"
    scene_name = f"HollowBackPocket_{version}"
    if (out / "manifest.json").exists() or scene_name in bpy.data.scenes:
        raise ValueError("Region version exists; choose a new version")
    if any(f"SM_Hollow_00_BackPocket{version}_{role}" in bpy.data.meshes or
           f"SM_Hollow_00_BackPocket{version}_{role}" in bpy.data.objects for role in ("Floor", "Shell")):
        raise ValueError("Region export name exists; choose a new version")
    modules = []
    for filename in ("BuildOctantWorkflowPilot.py", "GenerateSliceOctant.py"):
        module = types.ModuleType(filename[:-3])
        path = repo / "Scripts/Blender" / filename
        exec(compile(path.read_text(encoding="utf-8"), str(path), "exec"), module.__dict__)
        modules.append(module)
    pilot, generator = modules
    original_objects = set(bpy.data.objects.keys())
    scene = bpy.data.scenes.new(scene_name)
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    bpy.context.window.scene = scene
    source = bpy.data.collections.new(scene_name + "_Source")
    export = bpy.data.collections.new(scene_name + "_Export")
    scene.collection.children.link(source)
    scene.collection.children.link(export)

    def polyhedron(name, lower, upper):
        count = len(lower)
        faces = [tuple(reversed(range(count))), tuple(range(count, count * 2))]
        faces += [(i, (i + 1) % count, (i + 1) % count + count, i + count)
                  for i in range(count)]
        obj = pilot.mesh_object(name, [tuple(c / 100 for c in p) for p in lower + upper], faces, source)
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.triangulate(bm, faces=bm.faces[:])
        bm.to_mesh(obj.data)
        bm.free()
        return obj

    def prism(name, outline, bottom, top):
        return polyhedron(name, [(x, y, bottom) for x, y in outline],
                          [(x, y, top) for x, y in outline])

    def mesa(name, x, y, hx, hy, bottom, top):
        outline = [(x-hx, y-hy*.6), (x-hx*.6, y-hy), (x+hx*.6, y-hy), (x+hx, y-hy*.6),
                   (x+hx, y+hy*.6), (x+hx*.6, y+hy), (x-hx*.6, y+hy), (x-hx, y+hy*.6)]
        return prism(name, outline, bottom, top)

    body = prism("W01_Source", WEST, -7600, 6000)
    operands = [mesa("BackPocketLip_Source", 6000, -6350, 2200, 1000, 1200, 1500),
                mesa("WestPanel_Source", 3000, -2900, 1700, 900, 1700, 2000),
                mesa("WestBackHigh_Source", 4000, -7800, 1800, 750, 4500, 4800),
                mesa("MountainTurn03_Source", 4000, -8500, 600, 900, -2000, 6000)]
    replacements = ["W01", "BackPocketLip", "WestPanel", "WestBackHigh", "MountainTurn03", "ShuttleDock", "BackPocket"]
    bridge_profiles = {}

    def bridge(name, a, b, width=1000, back=-7000):
        bridge_profiles[name] = (Vector(a), Vector(b), width, back)
        direction = Vector((b[0]-a[0], b[1]-a[1])).normalized()
        side = Vector((-direction.y, direction.x))
        inner_sign = 1 if side.y > 0 else -1
        lower, upper = [], []
        for p in (a, b):
            scale = 30000 / (30000-p[2])
            outside = Vector(p[:2]) - side * inner_sign * width * .5 * scale
            base = Vector(p[:2]) - side * inner_sign * width * .9 * scale
            if back:
                upper.extend([(outside.x, outside.y, p[2]), (p[0], back, p[2])])
                lower.extend([(base.x, base.y, -2000), (p[0], back, -2000)])
            else:
                inside = Vector(p[:2]) + side * inner_sign * width * .5 * scale
                inside_base = Vector(p[:2]) + side * inner_sign * width * .9 * scale
                upper.extend([(outside.x, outside.y, p[2]), (inside.x, inside.y, p[2])])
                lower.extend([(base.x, base.y, -2000), (inside_base.x, inside_base.y, -2000)])
        # A외측, B외측, B내측, A내측 순서로 닫는다.
        order = (0, 2, 3, 1)
        return polyhedron(name + "_Source", [lower[i] for i in order], [upper[i] for i in order])

    mountain = [(-6500, -13200, 0), (4500, -11200, 2000), (-6500, -9400, 4000), (4000, -8500, 6000)]
    for i in range(3):
        a, b = Vector(mountain[i]), Vector(mountain[i+1])
        d = Vector((b.x-a.x, b.y-a.y, 0)).normalized()
        start = a + d * (2200 if i == 2 else 3000) if i else a
        end = b - d * (3000 if i < 2 else 3400)
        for name, p, q in [(f"MountainRamp{i+1:02d}", start, end),
                           (f"MountainArrival{i+1:02d}", end, b)] + (
                           [(f"MountainDeparture{i:02d}", a, start)] if i else []):
            # 구역에 실제로 닿는 몸체만 포함한다. 끝막음의 폭도 고려한다.
            if max(p.x, q.x) + 1250 < BOUNDS[0]:
                continue
            operands.append(bridge(name, p, q))
            replacements.append(name)
    operands.append(bridge("MountainSummit", mountain[3], (2000, -7400, 6000)))
    operands.append(bridge("ShuttleDock", (5000, -6200, 6000), (5000, -7500, 6000), 800, 0))
    replacements.append("MountainSummit")
    for operand in operands:
        modifier = body.modifiers.new("Union_" + operand.name, "BOOLEAN")
        modifier.operation = "UNION"
        modifier.solver = "EXACT"
        modifier.object = operand
    # 입구 잔도와 절벽 내부를 같은 cutter로 판다. 뒷벽은 -4300에서 보존한다.
    half = 1800 * 30000 / 28500
    cutter = prism("BackPocket_Cutter", [(6000-half, -7350), (6000+half, -7350),
                                         (6000+half, -4300), (6000-half, -4300)], 1500, POCKET_ROOF)
    entry_outline = [(6000-half, -8500), (6000+half, -8500), (6000+half, -7350), (6000-half, -7350)]
    entry = polyhedron("BackPocket_EntryCutter",
                       [(x, y, 1500 + (-7350-y)*500/750) for x, y in entry_outline],
                       [(x, y, POCKET_ROOF) for x, y in entry_outline])
    x0, x1, y0, y1 = BOUNDS
    interfaces = ((0, x0), (0, x1), (1, y0), (1, y1))
    crop = prism("RegionBoundary", [(x0, y0), (x1, y0), (x1, y1), (x0, y1)], -8000, 6500)
    for operation, operand in (("DIFFERENCE", cutter), ("DIFFERENCE", entry), ("INTERSECT", crop)):
        modifier = body.modifiers.new(operation, "BOOLEAN")
        modifier.operation, modifier.solver, modifier.object = operation, "EXACT", operand
    evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
    data = evaluated.to_mesh()
    bm = bmesh.new()
    bm.from_mesh(data)
    evaluated.to_mesh_clear()
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-4)
    bmesh.ops.dissolve_degenerate(bm, dist=1e-4, edges=bm.edges[:])
    bmesh.ops.dissolve_limit(bm, angle_limit=.001, verts=bm.verts[:], edges=bm.edges[:], use_dissolve_boundaries=True)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])

    def project(co):
        return Vector(generator.to_blender(generator.point(*(c*100 for c in co))))

    def needs_split(edge):
        a, b = (v.co for v in edge.verts)
        error = (project((a+b)*.5) - (project(a)+project(b))*.5).length
        return error > .005 or (edge.calc_length() > 2 and any(f.normal.z > .71 for f in edge.link_faces))

    for _ in range(16):
        bm.normal_update()
        edges = [e for e in bm.edges if needs_split(e)]
        if not edges:
            break
        bmesh.ops.subdivide_edges(bm, edges=edges, cuts=1, use_grid_fill=True)
        bmesh.ops.triangulate(bm, faces=bm.faces[:])
        bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
        bmesh.ops.dissolve_degenerate(bm, dist=1e-5, edges=bm.edges[:])
        bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bmesh.ops.dissolve_degenerate(bm, dist=1e-4, edges=bm.edges[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    flat_edges = {e for f in bm.faces if f.calc_area() < 1e-10 for e in f.edges}
    if flat_edges:
        bmesh.ops.dissolve_edges(bm, edges=list(flat_edges), use_verts=True)
        bmesh.ops.triangulate(bm, faces=bm.faces[:], ngon_method="EAR_CLIP")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    closed = pilot.topology_report(bm)
    if any(closed[k] for k in ("non_manifold_edges", "inconsistent_edges", "duplicate_faces", "degenerate_faces")) or closed["components"] != 1:
        raise ValueError(f"Closed region topology failed: {closed}")
    # 공유 단면에는 새 끝막음 벽을 남기지 않는다.
    caps = [f for f in bm.faces if any(all(abs(v.co[axis]*100-limit) < .01 for v in f.verts)
                                      for axis, limit in interfaces)]
    cap_count = len(caps)
    bmesh.ops.delete(bm, geom=caps, context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    local = {v: v.co.copy() for v in bm.verts}

    chord_error = max((project((local[e.verts[0]]+local[e.verts[1]])*.5)
                       - (project(local[e.verts[0]])+project(local[e.verts[1]]))*.5).length for e in bm.edges)
    for v in bm.verts:
        v.co = project(local[v])
    # 보고된 남쪽 경계의 Departure01 시작 끝면은 기존 BuildBridge의
    # 폭 분할/삼각형을 그대로 절단한 선에 맞춘다. 구면 투영 전 면 일치만으로는 부족하다.
    a, b, width, back = bridge_profiles["MountainDeparture01"]
    along = Vector((b.x-a.x, b.y-a.y)).normalized()
    side = Vector((-along.y, along.x))
    sign = 1 if side.y > 0 else -1
    scale = 30000 / (30000-a.z)
    inner = Vector((a.x, a.y)) + side * sign * width * .5 * scale
    base = Vector((a.x, a.y)) + side * sign * width * .9 * scale
    low, high = Vector((base.x, base.y, -2000)), Vector((inner.x, inner.y, a.z))
    back_low, back_high = Vector((a.x, back, -2000)), Vector((a.x, back, a.z))
    bands = math.ceil(max(abs(a.y-back), abs(b.y-back)) / 200)
    # Y=-10000의 구 중심을 지나는 동일 방사 평면.
    plane_normal = project(Vector((0, y0/100, 0))).cross(project(Vector((1, y0/100, 0)))).normalized()
    seam_segments = []
    for i in range(bands):
        points = [project(p / 100) for p in (
            low.lerp(back_low, i/bands), low.lerp(back_low, (i+1)/bands),
            high.lerp(back_high, (i+1)/bands), high.lerp(back_high, i/bands))]
        for indices in ((0, 1, 2), (0, 2, 3)):
            triangle = [points[j] for j in indices]
            hits = []
            for j in range(3):
                p, q = triangle[j], triangle[(j+1) % 3]
                dp, dq = p.dot(plane_normal), q.dot(plane_normal)
                if dp*dq < 0:
                    hits.append(p.lerp(q, dp/(dp-dq)))
            if len(hits) == 2:
                seam_segments.append(hits)

    def nearest_seam(p):
        nearest = []
        for a, b in seam_segments:
            d = b-a
            q = a + d * max(0, min(1, (p-a).dot(d)/d.length_squared))
            nearest.append(q)
        return min(nearest, key=lambda q: (p-q).length_squared)

    seam_edges = [e for e in bm.edges if e.is_boundary and abs(local[e.verts[0]].z-local[e.verts[1]].z) > .01 and all(
        abs(local[v].y*100-y0) < .01 and 4400 < local[v].x*100 < 4800 for v in e.verts)]
    seam_verts = {v for e in seam_edges for v in e.verts}
    if len(seam_edges) != 3 or not seam_segments:
        raise ValueError("Departure01 boundary ownership changed; inspect before snapping")
    snap_max = max((v.co-nearest_seam(v.co)).length for v in seam_verts)
    for v in seam_verts:
        v.co = nearest_seam(v.co)
    min(seam_verts, key=lambda v: local[v].z).co = max((p for s in seam_segments for p in s), key=lambda p: p.length)
    max(seam_verts, key=lambda v: local[v].z).co = min((p for s in seam_segments for p in s), key=lambda p: p.length)
    added = 0
    for p in [p for segment in seam_segments for p in segment]:
        if any((v.co-p).length < 1e-4 for v in seam_verts):
            continue
        height = 300-p.length
        for e in list(seam_edges):
            va, vb = e.verts
            ha, hb = 300-va.co.length, 300-vb.co.length
            if min(ha, hb)+1e-5 < height < max(ha, hb)-1e-5:
                new_edge, vertex = bmesh.utils.edge_split(e, va, (height-ha)/(hb-ha))
                vertex.co = p
                seam_verts.add(vertex)
                seam_edges.append(new_edge)
                added += 1
                break
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    radial, tangent, bitangent = (Vector(p) for p in generator.center_frame())
    for v in seam_verts:
        p = Vector((v.co.x*100, -v.co.y*100, v.co.z*100))
        scale = 30000 / p.dot(radial)
        local[v] = Vector((p.dot(bitangent)*scale, p.dot(tangent)*scale, 30000-p.length)) / 100
        if abs(local[v].y*100-y0) > .05:
            raise ValueError("Departure01 seam left the radial interface plane")
        local[v].y = y0 / 100
    seam_error = max((v.co-nearest_seam(v.co)).length for v in seam_verts)
    if seam_error > 1e-4:
        raise ValueError("Departure01 interface segment mismatch")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    report = pilot.topology_report(bm)
    if any(report[k] for k in ("inconsistent_edges", "duplicate_faces", "degenerate_faces")):
        raise ValueError(f"Exported region topology failed: {report}")
    boundary_edges = [e for e in bm.edges if e.is_boundary]
    unexpected = [e for e in boundary_edges if e not in seam_edges and not any(
        all(abs(local[v][axis]*100-limit) < .01 for v in e.verts)
        for axis, limit in interfaces)]
    if unexpected or any(not e.is_manifold and not e.is_boundary for e in bm.edges):
        raise ValueError(f"Unexpected open/non-manifold edge: {len(unexpected)}/"
                         f"{sum(not e.is_manifold and not e.is_boundary for e in bm.edges)}, "
                         f"seam {len(seam_edges)}/{added}, "
                         f"unexpected local {[[list(local[v]) for v in e.verts] for e in unexpected[:4]]}")
    tree = BVHTree.FromBMesh(bm)
    bm.faces.ensure_lookup_table()
    # 인접 삼각형은 공통 정점에서 만나는 정상 접합이다.
    overlaps = [(a, b) for a, b in tree.overlap(tree) if a < b
                and not set(bm.faces[a].verts).intersection(bm.faces[b].verts)]
    walk_error = max((project((local[e.verts[0]]+local[e.verts[1]])*.5)
                      - (project(local[e.verts[0]])+project(local[e.verts[1]]))*.5).length
                     for e in bm.edges if any(f.normal.dot(-f.calc_center_median().normalized()) >= .71 for f in e.link_faces))
    if overlaps or chord_error > .015 or walk_error > .005:
        raise ValueError(f"Intersection/chord gate failed: {len(overlaps)}, {chord_error}")
    boundaries = [[list(v.co) for v in e.verts] for e in bm.edges if e.is_boundary]
    objects = generator.split_and_link(bm, f"Hollow_00_BackPocket{version}", export)
    bm.free()
    out.mkdir(parents=True, exist_ok=True)
    exports = {}
    for obj in objects:
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        obj.data.calc_loop_triangles()
        ue = [(v.co.x*100, -v.co.y*100, v.co.z*100) for v in obj.data.vertices]
        exports[obj.name] = {"file": str(out / (obj.name + ".fbx")), "triangles": len(obj.data.loop_triangles),
                             "bounds_cm": {"min": [min(p[i] for p in ue) for i in range(3)],
                                           "max": [max(p[i] for p in ue) for i in range(3)]}}
        bpy.ops.export_scene.fbx(filepath=exports[obj.name]["file"], use_selection=True, object_types={"MESH"},
                                 apply_scale_options="FBX_SCALE_NONE", global_scale=1, axis_forward="-Z", axis_up="Y",
                                 mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False)
    for obj in source.objects:
        obj.hide_set(True)
        obj.hide_render = True
    report.update({"version": version, "region_xy_cm": BOUNDS, "pocket_roof_h_cm": POCKET_ROOF, "replaced_generated_regions": replacements,
                   "closed_before_interface_removal": closed, "removed_interface_triangles": cap_count,
                   "open_interface_edges": len(boundary_edges), "unexpected_open_edges": len(unexpected),
                   "nonadjacent_bvh_intersections": len(overlaps), "max_chord_error_cm": chord_error*100,
                   "departure01_seam_snap_max_cm": snap_max*100, "departure01_seam_added_vertices": added,
                   "departure01_seam_max_error_cm": seam_error*100,
                   "max_walkable_chord_error_cm": walk_error*100,
                   "exports": exports, "interface_edges_blender_m": boundaries,
                   "original_objects_preserved": original_objects.issubset(bpy.data.objects.keys()),
                   "limits": ["Interface assembly and engine collision not yet checked", "No bake or PIE validation"]})
    if sum(e["triangles"] for e in exports.values()) != report["triangles"]:
        raise ValueError("Role partition lost faces")
    (out / "manifest.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    bpy.ops.wm.save_as_mainfile(filepath=str(out / "BackPocket.blend"), check_existing=False, copy=True)
    return {k: v for k, v in report.items() if k != "interface_edges_blender_m"}
