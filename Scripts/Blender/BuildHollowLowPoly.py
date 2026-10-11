"""Hollow 전체 내부 지형을 닫힌 단순 덩어리로 재제작한다.

이음매 지각·부유섬·마커는 C++ 소유다. 좌표는 옥탄트 접평면 cm다.
원본 덩어리와 순차 Boolean 결과를 보존하고 마지막에만 구면으로 투영한다.
"""

import json
import math
import types
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree


MOUNTAIN = [(-9000, -13500, 0), (5000, -13500, 2000), (5000, -11000, 2000),
            (-9000, -11000, 4000), (-9000, -8500, 4000), (4000, -8500, 6000),
            (4000, -6000, 6000), (2000, -6000, 6000)]
SECRET = [(-4500, 4500, -12000), (-6400, 4500, -12000), (-13500, 3500, -9500),
          (-13500, -8500, -4500), (-3500, -8500, -2000), (-3500, -11500, -2000),
          (-12500, -11500, 0), (-12500, -12500, 0)]
EAST = [(-13500, 13500, 0), (4000, 11000, 2000), (4000, 8500, 2000),
        (-6000, 8500, 4500), (-6000, 6000, 4500)]
BOUNDARY = [(-15500, -15500), (6000, -15500), (15500, -8500),
            (15500, 8500), (6000, 15500), (-15500, 15500)]


def walk_height(t):
    # 양 끝 1/6 구간에서 가감속한다. 중앙의 기울기는 평균의 1.2배다.
    return 3.6*t*t if t < 1/6 else 1-3.6*(1-t)**2 if t > 5/6 else 1.2*(t-1/12)


def route_offsets(route):
    sides = [Vector((-(b[1]-a[1]),b[0]-a[0],0)).normalized() for a,b in zip(route,route[1:])]
    offsets = [sides[0]]
    for before, after in zip(sides,sides[1:]):
        miter = (before+after).normalized()
        offsets.append(miter/miter.dot(before))
    return offsets+[sides[-1]]


def validate_routes(obj, generator):
    tree = BVHTree.FromPolygons([v.co for v in obj.data.vertices],
                               [tuple(f.vertices) for f in obj.data.polygons], all_triangles=True)
    probes, maximum = 0, 0.0
    for route in (SECRET, MOUNTAIN, EAST):
        route = [Vector(p) for p in route]
        offsets = route_offsets(route)
        for segment, (a,b) in enumerate(zip(route,route[1:])):
            steps = math.ceil((b-a).xy.length/100)
            for lateral in (-700,-350,0,350,700):
                for i in range(steps+1):
                    t = i/steps
                    p = a.lerp(b,t)+offsets[segment].lerp(offsets[segment+1],t)*lateral
                    p.z = a.z+(b.z-a.z)*walk_height(t)
                    world = Vector(generator.to_blender(generator.point(*p)))
                    up = -world.normalized()
                    hit = tree.ray_cast(world+up,-up,2)
                    if hit[0] is None:
                        raise ValueError(f"Missing route floor: {segment}, {t}, {lateral}")
                    maximum = max(maximum,math.degrees(math.acos(max(-1,min(1,hit[1].dot(up))))))
                    probes += 1
    if maximum > 35:
        raise ValueError(f"Actual route floor slope: {maximum:.2f} > 35 degrees")
    return {"floor_probes":probes,"missing_floor":0,"max_hit_slope_degrees":maximum}


def validate_interfaces(obj, generator):
    tree = BVHTree.FromPolygons([v.co for v in obj.data.vertices],
                               [tuple(f.vertices) for f in obj.data.polygons], all_triangles=True)
    probes, maximum = 0, 0.0
    for a,b in zip(BOUNDARY,BOUNDARY[1:]+BOUNDARY[:1]):
        a,b = Vector(a)*0.98,Vector(b)*0.98
        along = (b-a).normalized()
        inward = Vector((-along.y,along.x))
        steps = math.ceil((b-a).length/400)
        for i in range(steps+1):
            for cross in range(-4,5):
                q = a.lerp(b,i/steps)+inward*cross*50
                p = Vector(generator.to_blender(generator.point(q.x,q.y,0)))
                up = -p.normalized()
                hit = tree.ray_cast(p+up,-up,2)
                if hit[0] is None:
                    raise ValueError(f"Missing interface floor: {list(q)}")
                maximum = max(maximum,math.degrees(math.acos(max(-1,min(1,hit[1].dot(up))))))
                probes += 1
    if maximum > 35:
        raise ValueError(f"Interface floor slope: {maximum}")
    shaft_rays = 0
    for x,y,top in ((0,0,-4500),(2500,9800,2200)):
        for dx,dy in ((0,0),(-60,0),(60,0),(0,-60),(0,60)):
            a,b = (Vector(generator.to_blender(generator.point(x+dx,y+dy,h))) for h in (-11890,top))
            if tree.ray_cast(a,(b-a).normalized(),(b-a).length)[0] is not None:
                raise ValueError(f"Blocked shaft ray: {x+dx}, {y+dy}")
            shaft_rays += 1
    return {"joint_floor_probes":probes,"missing_floor":0,"max_hit_slope_degrees":maximum,
            "shaft_rays":shaft_rays,"blocked_shaft_rays":0}


def build(repo, version="V001"):
    repo = Path(repo)
    out = repo / "Saved/SliceOctant/Hollow_00" / ("LowPoly" + version)
    name = "HollowLowPoly_" + version
    if (out / "manifest.json").exists() or name in bpy.data.scenes:
        raise ValueError("Version already exists")
    modules = []
    for filename in ("BuildOctantWorkflowPilot.py", "GenerateSliceOctant.py", "BuildHollowEastRim.py"):
        path = repo / "Scripts/Blender" / filename
        module = types.ModuleType(path.stem)
        exec(compile(path.read_text(encoding="utf-8"), str(path), "exec"), module.__dict__)
        modules.append(module)
    pilot, generator, refinement = modules
    # 구면 투영과 경로 양 끝·양 가장자리의 순간 기울기를 포함한다.
    slopes = {}
    for label, route, width in (("Mountain", MOUNTAIN,1600), ("Secret", SECRET,1800), ("East", EAST,1800)):
        maximum = 0.0
        offsets = route_offsets(route)
        for segment, (a, b) in enumerate(zip(route, route[1:])):
          for lateral in (-width/2,0,width/2):
            previous = None
            for i in range(201):
                t = i / 200
                p = Vector(a).lerp(Vector(b), t)+offsets[segment].lerp(offsets[segment+1],t)*lateral
                p.z = a[2] + (b[2]-a[2])*walk_height(t)
                world = Vector(generator.point(*p))
                if previous is not None:
                    delta = world-previous
                    up = -(world+previous).normalized()
                    angle = math.degrees(math.asin(min(1, abs(delta.normalized().dot(up)))))
                    maximum = max(maximum, angle)
                previous = world
        if maximum > 35:
            raise ValueError(f"Walking slope: {label}: {maximum:.2f} > 35 degrees")
        slopes[label] = maximum
    original = set(bpy.data.objects)
    scene = bpy.data.scenes.new(name)
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    bpy.context.window.scene = scene
    source, work, export = [bpy.data.collections.new(name + suffix)
                            for suffix in ("_Source", "_Work", "_Export")]
    for collection in (source, work, export):
        scene.collection.children.link(collection)
    out.mkdir(parents=True, exist_ok=True)

    def mesh(label, points, faces):
        obj = pilot.mesh_object(name + "_" + label, [tuple(Vector(p) / 100) for p in points], faces, source)
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.triangulate(bm, faces=bm.faces[:])
        report = pilot.topology_report(bm)
        bm.free()
        if any(report[k] for k in ("non_manifold_edges", "inconsistent_edges", "duplicate_faces", "degenerate_faces")):
            raise ValueError(f"Source topology: {label}: {report}")
        return obj

    def prism(label, outline, bottom, top):
        count = len(outline)
        points = [(x, y, bottom) for x, y in outline] + [(x, y, top) for x, y in outline]
        faces = [tuple(reversed(range(count))), tuple(range(count, 2 * count))]
        faces += [(i, (i + 1) % count, (i + 1) % count + count, i + count) for i in range(count)]
        return mesh(label, points, faces)

    def box(label, x0, x1, y0, y1, bottom, top):
        return prism(label, [(x0, y0), (x1, y0), (x1, y1), (x0, y1)], bottom, top)

    def route_mesh(label, route, width, cutter=False):
        route = [Vector(p) for p in route]
        offsets = [v*width/2 for v in route_offsets(route)]
        points, faces = [], []
        for segment, (a,b) in enumerate(zip(route,route[1:])):
            steps = max(2, math.ceil((b-a).xy.length/250))
            for i in range(0 if segment == 0 else 1, steps+1):
                t = i/steps
                p = a.lerp(b,t)
                p.z = a.z+(b.z-a.z)*walk_height(t)
                side = offsets[segment].lerp(offsets[segment+1],t)
                lo, hi = (p.z,p.z+1400) if cutter else (-200,p.z)
                for h,sign in ((lo,-1),(lo,1),(hi,1),(hi,-1)):
                    q = p+side*sign
                    points.append((q.x,q.y,h))
        steps = len(points)//4-1
        for i in range(steps):
            for j in range(4):
                faces.append((4*i+j, 4*i+(j+1)%4, 4*(i+1)+(j+1)%4, 4*(i+1)+j))
        faces += [(3, 2, 1, 0), tuple(4*steps+j for j in range(4))]
        return mesh(label, points, faces)

    # 모든 램프를 감싸는 소유 경계 바깥에는 지각 밑으로 들어가는 접합 띠를 둔다.
    inner = [(x * 0.98, y * 0.98) for x, y in BOUNDARY]
    count = len(inner)
    points = [(x,y,-15000) for x,y in BOUNDARY] + [(x,y,-80) for x,y in BOUNDARY] + [(x,y,0) for x,y in inner]
    faces = [tuple(reversed(range(count))), tuple(range(2*count, 3*count))]
    for i in range(count):
        j = (i+1) % count
        faces += [(i,j,j+count,i+count), (i+count,j+count,j+2*count,i+2*count)]
    body = mesh("Ground", points, faces)
    operations = []
    operations.append(("UNION", box("WestCliff", -10000, 12000, -7000, -3500, -200, 6000)))
    operations.append(("UNION", box("EastSouth", -10000, -7500, 3500, 7000, -200, 10000)))
    operations.append(("UNION", box("EastSaddle", -7500, -4500, 3500, 7000, -200, 4500)))
    operations.append(("UNION", box("EastNorth", -4500, 11000, 3500, 7000, -200, 10000)))
    for route_name, route, width in (("Mountain", MOUNTAIN, 1600), ("East", EAST, 1800)):
        operations.append(("UNION", route_mesh(route_name, route, width)))
    # 협곡은 낮은 지면에서 잘라내고 작은 보정 조각으로 외형을 덮지 않는다.
    canyon = [(-12000, -6000), (-7000, -3500), (5000, -3500), (12500, 4000),
              (10500, 8000), (4000, 3500), (-7000, 3500), (-12000, -1000)]
    operations.append(("DIFFERENCE", prism("CanyonVoid", canyon, -7500, 12000)))
    # 중간 잔도는 절벽에 매입하고 최종 외피에서 내부 면을 제거한다.
    for label, x, y0, y1, h in (("WestLow", -3500, -3800, -2200, -5000),
                                  ("WestPocketLip", -2500, -3800, -2200, -2500),
                                  ("WestMid", -1500, -3800, -2200, 1500),
                                  ("WestHigh", -4500, -3800, -2200, 3500),
                                  ("WestPanel", 3000, -3800, -1800, 2000),
                                  ("EastLow", 4500, 1800, 3800, -5000),
                                  ("EastMid", -1000, 2200, 3800, 4500),
                                  ("EastHigh", 5000, 2400, 4200, 7500),
                                  ("EastPanel", 3000, 1800, 3800, 2000),
                                  ("BackPocketLip", 6000, -7800, -6500, 1500),
                                  ("ShuttleDock", 5000, -8000, -6500, 6000)):
        operations.append(("UNION", box(label, x-1600, x+1600, y0, y1, h-300, h)))
    # 이어지는 지하 공간은 같은 몸체에서 뺀다.
    operations += [("DIFFERENCE", prism("CavernVoid", [(-6000,-1500),(5500,-1500),(5500,11000),
                    (1500,11000),(1500,6000),(-6000,6000)], -12000, -9000)),
                   ("DIFFERENCE", box("BasinShaft", -900, 900, -900, 900, -9100, -7000)),
                   ("DIFFERENCE", box("MesaShaft", 2200, 3400, 9500, 10100, -9100, 300)),
                   ("DIFFERENCE", box("WestPocketVoid", -4300, -700, -6500, -2200, -2500, 500)),
                   ("DIFFERENCE", box("EastPocketVoid", 2200, 5800, 1800, 6000, 2000, 5000)),
                   ("DIFFERENCE", box("BackPocketVoid", 4200, 7800, -7800, -4500, 1500, 4800))]
    # 꺾이는 단면을 공유해 회전 공간의 중복 절삭과 수직 단차를 없앤다.
    operations.append(("DIFFERENCE", route_mesh("SecretVoid", SECRET, 1800, True)))
    # 입구 천장은 지상까지 열어 마지막 구간에 낮은 지붕을 남기지 않는다.
    operations.append(("DIFFERENCE", box("SecretMouthVoid", -13800, -11200, -13800, -11000, 0, 1800)))
    bpy.ops.wm.save_as_mainfile(filepath=str(out / "Sources.blend"), check_existing=False, copy=True)
    result = body.copy()
    result.data = body.data.copy()
    result.name = name + "_Result"
    work.objects.link(result)
    completed = []
    for operation, operand in operations:
        bpy.ops.object.select_all(action="DESELECT")
        result.select_set(True)
        bpy.context.view_layer.objects.active = result
        mod = result.modifiers.new(operation, "BOOLEAN")
        mod.operation, mod.solver, mod.object = operation, "EXACT", operand
        bpy.ops.object.modifier_apply(modifier=mod.name)
        completed.append(operand.name)
        (out / "progress.json").write_text(json.dumps(completed), encoding="utf-8")
    bm = bmesh.new()
    bm.from_mesh(result.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    planar_edges = [e for e in bm.edges if len(e.link_faces) == 2
                    and e.link_faces[0].normal.dot(e.link_faces[1].normal) > 0.999999]
    bmesh.ops.beautify_fill(bm, faces=bm.faces[:], edges=planar_edges, method="AREA")
    pre = pilot.topology_report(bm)
    if any(pre[k] for k in ("non_manifold_edges", "inconsistent_edges", "duplicate_faces", "degenerate_faces")):
        raise ValueError(f"Boolean topology: {pre}")
    bm.verts.index_update()
    points = [v.co.copy() * 100 for v in bm.verts]
    faces = [tuple(v.index for v in f.verts) for f in bm.faces]
    bm.free()

    def project(p):
        return Vector(generator.to_blender(generator.point(*p)))

    def split(a, b, edge):
        return (project((a+b)/2) - (project(a)+project(b))/2).length > 0.04

    points, faces = refinement.subdivide(points, faces, split)
    # 가느다란 삼각형은 변의 현 오차가 작아도 구면 투영 뒤 법선이 크게 기울 수 있다.
    # 투영의 미분으로 예상 법선을 구하고, 차이가 큰 면의 최장 공유 변만 추가 분할한다.
    radial, tangent, bitangent = map(Vector, generator.center_frame())
    max_normal_error = 0.0
    normal_progress = []
    for normal_pass in range(32):
        # 이웃 면의 공유 변 분할로 생긴 긴 조각은 같은 평면 안에서만 대각선을 교체한다.
        planar = bmesh.new()
        for p in points:
            planar.verts.new(p)
        planar.verts.ensure_lookup_table()
        for face in faces:
            planar.faces.new([planar.verts[i] for i in face])
        planar.normal_update()
        edges = [e for e in planar.edges if len(e.link_faces) == 2
                 and e.link_faces[0].normal.dot(e.link_faces[1].normal) > 0.999999]
        bmesh.ops.beautify_fill(planar,faces=planar.faces[:],edges=edges,method="AREA")
        planar.verts.index_update()
        faces = [tuple(v.index for v in f.verts) for f in planar.faces]
        planar.free()
        projected = [project(p) for p in points]
        bad_edges = set()
        max_normal_error = 0.0
        for face in faces:
            a, b, c = (points[i] for i in face)
            center = (a+b+c)/3
            v = radial*30000+bitangent*center.x+tangent*center.y
            u = v.normalized()
            scale = (30000-center.z)/v.length
            jx, jy, jh = (Vector((q.x,-q.y,q.z)) for q in
                          ((bitangent-u*u.dot(bitangent))*scale,
                           (tangent-u*u.dot(tangent))*scale, -u))
            n = (b-a).cross(c-a).normalized()
            expected = (jy.cross(jh)*n.x+jh.cross(jx)*n.y+jx.cross(jy)*n.z).normalized()
            pa, pb, pc = (projected[i] for i in face)
            actual = (pb-pa).cross(pc-pa).normalized()
            error = math.degrees(math.acos(max(-1,min(1,actual.dot(expected)))))
            max_normal_error = max(max_normal_error,error)
            if error > 5:
                edges = [tuple(sorted((face[i],face[(i+1)%3]))) for i in range(3)]
                bad_edges.add(max(edges,key=lambda e:(points[e[0]]-points[e[1]]).length_squared))
        if not bad_edges:
            break
        normal_progress.append({"pass":normal_pass,"triangles":len(faces),"bad_edges":len(bad_edges),"max_error":max_normal_error})
        (out / "normal_progress.json").write_text(json.dumps(normal_progress),encoding="utf-8")
        if normal_pass == 31:
            debug = [{"points":[list(points[i]) for i in face]} for face in faces
                     if any(tuple(sorted((face[i],face[(i+1)%3]))) in bad_edges for i in range(3))]
            (out / "normal_failure.json").write_text(json.dumps(debug[:10]),encoding="utf-8")
        points, faces = refinement.subdivide(points, faces, lambda a,b,edge: edge in bad_edges)
    else:
        raise ValueError(f"Projected normal refinement did not converge: {max_normal_error}")
    projected = [project(p) for p in points]
    final = pilot.mesh_object(name + "_Projected", projected, faces, work)
    bm = bmesh.new()
    bm.from_mesh(final.data)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    report = pilot.topology_report(bm)
    if any(report[k] for k in ("non_manifold_edges", "inconsistent_edges", "duplicate_faces", "degenerate_faces")) or report["components"] != 1:
        raise ValueError(f"Projected topology: {report}")
    bm.faces.index_update()
    bm.verts.index_update()
    overlaps = BVHTree.FromBMesh(bm, epsilon=0).overlap(BVHTree.FromBMesh(bm, epsilon=0))
    intersections = [(a,b) for a,b in overlaps if a < b and not set(bm.faces[a].verts).intersection(bm.faces[b].verts)]
    if intersections:
        raise ValueError(f"Disjoint triangle overlaps: {intersections[:10]} ({len(intersections)})")
    route_report = validate_routes(final, generator)
    interface_report = validate_interfaces(final, generator)
    # 하나의 외피를 높이 띠로 나눠 Support의 반지름 표현 상한을 지킨다.
    groups = {}
    for face in bm.faces:
        center = face.calc_center_median()
        if face.normal.dot(-center.normalized()) < 0.71:
            key = "Shell"
        else:
            h = sum(points[v.index].z for v in face.verts) / len(face.verts)
            local = sum((points[v.index] for v in face.verts), Vector()) / len(face.verts)
            inside = all((inner[(i+1)%count][0]-inner[i][0])*(local.y-inner[i][1])
                         - (inner[(i+1)%count][1]-inner[i][1])*(local.x-inner[i][0]) >= -0.1 for i in range(count))
            broad = any(all(abs(points[v.index].z-level) < 0.1 for v in face.verts) for level in (-12000,-7500,0))
            # 좁은 비밀길 진입부에는 동굴 본실의 1m 격자를 적용하지 않는다.
            if abs(h+12000) < 0.1 and local.x < -6000:
                broad = False
            key = ("Coarse" if broad else "Floor") + str(math.floor((h + (15000 if broad else 14000)) / 7000)) if inside else "Shell"
        groups.setdefault(key, []).append(face)
    exports = {}
    for key, selected in sorted(groups.items()):
        role = "Shell" if key == "Shell" else "Coarse" if key.startswith("Coarse") else "Floor"
        label = f"SM_Hollow_00_LowPoly{version}_{key}_{role}" if role != "Shell" else f"SM_Hollow_00_LowPoly{version}_Shell"
        verts, remap, tris = [], {}, []
        for face in selected:
            tri = []
            for vertex in face.verts:
                if vertex.index not in remap:
                    remap[vertex.index] = len(verts)
                    verts.append(tuple(vertex.co))
                tri.append(remap[vertex.index])
            tris.append(tuple(tri))
        # 열린 역할 조각에서 법선을 재계산하면 닫힌 외피의 면 방향을 잃는다.
        data = bpy.data.meshes.new(label)
        data.from_pydata(verts,[],tris)
        data.update()
        obj = bpy.data.objects.new(label,data)
        export.objects.link(obj)
        if any(actual.normal.dot(expected.normal) < 0.999 for actual,expected in zip(data.polygons,selected)):
            raise ValueError(f"Role winding differs from closed shell: {label}")
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        filepath = out / (label + ".fbx")
        bpy.ops.export_scene.fbx(filepath=str(filepath), use_selection=True, object_types={"MESH"},
                                 apply_scale_options="FBX_SCALE_NONE", global_scale=1, axis_forward="-Z", axis_up="Y",
                                 mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False)
        ue = [(v[0]*100, -v[1]*100, v[2]*100) for v in verts]
        if role != "Shell" and max(abs(c) for v in ue for c in v) > 32767:
            raise ValueError(f"Support coordinate exceeds int16 cap: {label}")
        exports[label] = {"file": str(filepath), "triangles": len(tris),
                          "bounds_cm": {"min": [min(v[i] for v in ue) for i in range(3)],
                                        "max": [max(v[i] for v in ue) for i in range(3)]}}
    bm.free()
    for collection in (source, work):
        for obj in collection.objects:
            obj.hide_set(True)
            obj.hide_render = True
    report.update({"version": version, "exports": exports, "source_topology": pre,
                   "self_intersections_disjoint_faces": len(intersections),
                   "role_winding_verified": True,
                   "max_edge_midpoint_chord_error_cm": 4, "boundary": BOUNDARY,
                   "max_projected_normal_error_degrees": max_normal_error,
                   "mountain": MOUNTAIN, "secret": SECRET, "east": EAST,
                   "route_max_slope_degrees": slopes,
                   "route_preflight": route_report,
                   "interface_preflight": interface_report,
                   "original_objects_preserved": all(obj in bpy.data.objects[:] for obj in original),
                   "limits": ["Edge-midpoint curvature bound, not a full surface bound", "Engine collision and gameplay require separate validation"]})
    if sum(e["triangles"] for e in exports.values()) != report["triangles"]:
        raise ValueError("Role partition lost triangles")
    (out / "manifest.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    bpy.ops.wm.save_as_mainfile(filepath=str(out / "HollowLowPoly.blend"), check_existing=False, copy=True)
    return report
