"""동쪽 외곽 접안 굽이와 Rim 하부 접합을 하나의 구면 외피로 제작한다.

진입축의 양 끝과 폭·섬 치수를 보존하고 경사/도착부의 내부 경계를 조정한다.
local에서 합친 뒤 구면 투영과 방사 절단을 수행한다. 기존 Blender 장면은 보존한다.
"""

import json
import math
import types
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree


BOUNDS = (-14000, -7000, 7800, 12000)
BRIDGES = {
    "OuterEastDock": ((-12800, 11300, 2400), (-12800, 10000, 2400), 800),
    "OuterEastBridge": ((-12800, 10000, 2400), (-10300, 10000, 3000), 800),
    "OuterEastLanding": ((-10300, 10000, 3000), (-9800, 10000, 3000), 800),
    "RimLanding": ((-6960, 10610, 3000), (-7700, 10210, 3000), 1600),
    "RimApproach": ((-4300, 12000, 1600), (-6960, 10610, 3000), 1600),
    "RimCliffExit": ((-7300, 9500, 3000), (-6500, 9500, 3000), 800),
}
REPLACED = ["Ridge04", "OuterEastRockJoin", "Rim", *BRIDGES]


def east_ramp_height(x):
    t = max(0, min(1, (x+12800)/2500))
    return 2400+600*t*t*(3-2*t)


def subdivide(points, faces, needs_split):
    for _ in range(16):
        edges = {tuple(sorted((f[i],f[(i+1)%3]))) for f in faces for i in range(3)}
        midpoints = {}
        for edge in edges:
            a,b = (points[i] for i in edge)
            if needs_split(a,b,edge):
                midpoints[edge] = len(points)
                points.append((a+b)/2)
        if not midpoints:
            return points,faces
        # 공유 변의 중점을 하나만 만들고 각 삼각형을 직접 나눈다.
        divided = []
        for face in faces:
            a,b,c = face
            mids = [midpoints.get(tuple(sorted(e))) for e in ((a,b),(b,c),(c,a))]
            count = sum(m is not None for m in mids)
            if count == 0:
                divided.append(face)
            elif count == 3:
                ab,bc,ca = mids
                divided.extend(((a,ab,ca),(ab,b,bc),(ca,bc,c),(ab,bc,ca)))
            else:
                index = next(i for i in range(3) if mids[i] is not None and (count == 1 or mids[(i+1)%3] is not None))
                a,b,c = (face[(index+i)%3] for i in range(3))
                ab,bc = mids[index],mids[(index+1)%3]
                if count == 1:
                    divided.extend(((a,ab,c),(ab,b,c)))
                else:
                    divided.extend(((b,bc,ab),(a,ab,c),(ab,bc,c)))
        faces = divided
    raise ValueError("Projection subdivision did not converge")


def build_region(repo, version="V001", sources_only=False, interface_targets=None):
    repo = Path(repo)
    out = repo / "Saved/SliceOctant/Hollow_00" / f"EastRim{version}"
    name = f"HollowEastRim_{version}"
    if (out / "manifest.json").exists() or name in bpy.data.scenes:
        raise ValueError("Region version exists; choose a new version")
    modules = []
    for filename in ("BuildOctantWorkflowPilot.py", "GenerateSliceOctant.py"):
        path = repo / "Scripts/Blender" / filename
        module = types.ModuleType(path.stem)
        exec(compile(path.read_text(encoding="utf-8"), str(path), "exec"), module.__dict__)
        modules.append(module)
    pilot, generator = modules
    original = set(bpy.data.objects.keys())
    scene = bpy.data.scenes.new(name)
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1
    bpy.context.window.scene = scene
    source, export = (bpy.data.collections.new(name + suffix) for suffix in ("_Source", "_Export"))
    scene.collection.children.link(source)
    scene.collection.children.link(export)

    def project(p):
        return Vector(generator.to_blender(generator.point(*p)))

    def mesh(name, points, faces):
        obj = pilot.mesh_object(name, [tuple(Vector(p)/100) for p in points], faces, source)
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        report = pilot.topology_report(bm)
        if any(report[k] for k in ("non_manifold_edges", "inconsistent_edges", "duplicate_faces", "degenerate_faces")):
            raise ValueError(f"Source topology failed: {name}: {report}")
        bm.to_mesh(obj.data)
        bm.free()
        return obj

    def bridge(name, a, b, width):
        a, b = Vector(a), Vector(b)
        side = Vector((-(b.y-a.y), b.x-a.x)).normalized()
        steps = max(2, math.ceil(Vector((b.x-a.x, b.y-a.y)).length / (50 if name == "OuterEastBridge" else 200)))
        bands = max(2, math.ceil(width / 200))
        points, faces = [], []
        # 밑면도 같은 폭 분할을 공유한다. 높이는 C++와 같은 지면 아래 H−2000이다.
        for bottom in (False, True):
            for i in range(steps+1):
                p = a.lerp(b, i/steps)
                if name == "OuterEastBridge":
                    p.z = east_ramp_height(p.x)
                scale = 30000/(30000-p.z)
                for j in range(bands+1):
                    offset = side * (-1+2*j/bands) * width * (.9 if bottom else .5) * scale
                    points.append((p.x+offset.x, p.y+offset.y, -2000 if bottom else p.z))
        row = bands+1
        count = (steps+1)*row
        def quad(a, b, c, d):
            faces.extend(((a, b, c), (a, c, d)))
        for i in range(steps):
            for j in range(bands):
                k = i*row+j
                quad(k, k+row, k+row+1, k+1)
                quad(k+count, k+row+count, k+row+1+count, k+1+count)
            for j in (0, bands):
                k = i*row+j
                quad(k+count, k+row+count, k+row, k)
        for i in (0, steps):
            for j in range(bands):
                k = i*row+j
                quad(k+count, k+1+count, k+1, k)
        return mesh(name+"_Source", points, faces)

    def prism(name, outline, bottom, height):
        # 윤곽/높이 함수는 편집 원본이다. 공면 합치기는 local에서 수행한다.
        count = len(outline)
        # Z에는 높이 비율을 둔다. 분할 시 좌표 보간으로 비율도 함께 보존한다.
        points = [(x/100, y/100, 0) for x, y in outline]
        points += [(x/100, y/100, 1) for x, y in outline]
        faces = [tuple(reversed(range(count))), tuple(range(count, count*2))]
        faces += [(i, (i+1)%count, (i+1)%count+count, i+count) for i in range(count)]
        obj = pilot.mesh_object(name+"_Source", points, faces, source)
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        def local(co):
            return Vector((co.x, co.y, (bottom+(height(co.x*100, co.y*100)-bottom)*co.z)/100))
        bmesh.ops.triangulate(bm, faces=bm.faces[:])
        bm.verts.index_update()
        points = [v.co.copy() for v in bm.verts]
        faces = [tuple(v.index for v in f.verts) for f in bm.faces]
        bm.free()
        def needs_split(a,b,edge):
            pa,pb,mid = (project(local(p)*100) for p in (a,b,(a+b)/2))
            return (mid-(pa+pb)/2).length > .005 or (Vector((a.x-b.x,a.y-b.y)).length > 2 and a.z == b.z and a.z in (0,1))
        points,faces = subdivide(points,faces,needs_split)
        obj.data.clear_geometry()
        obj.data.from_pydata([local(p) for p in points], [], faces)
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        report = pilot.topology_report(bm)
        if any(report[k] for k in ("non_manifold_edges", "inconsistent_edges", "duplicate_faces", "degenerate_faces")) or report["components"] != 1:
            raise ValueError(f"Source topology failed: {name}: {report}")
        bm.to_mesh(obj.data)
        bm.free()
        return obj

    def mesa(name, x, y, hx, hy, bottom, top):
        outline = [(x-hx,y-hy*.6),(x-hx*.6,y-hy),(x+hx*.6,y-hy),(x+hx,y-hy*.6),
                   (x+hx,y+hy*.6),(x+hx*.6,y+hy),(x-hx*.6,y+hy),(x-hx,y+hy*.6)]
        return prism(name, outline, bottom, lambda x, y: top)

    objects = [bridge(name, *args) for name, args in BRIDGES.items()]
    objects.append(mesa("Ridge04", -11000, 11300, 2200, 500, -1000, 2400))

    def join_height(x, y):
        h = east_ramp_height(x)
        south, north = max(0, y-10000-400*30000/(30000-h)), max(0, 10800-y)
        return (h*north+2400*south)/(south+north)-2 if south+north > 0 else 2398

    objects.append(prism("OuterEastRockJoin", [(-13400,10000),(-8800,10000),(-8800,11300),(-13400,11300)], -2000, join_height))
    objects.append(mesa("DockBendApron", -12800, 10000, 1000, 1000, -2000, 2400))
    points, faces = [], []
    sides, rings, radius = 24, 4, 1600*30000/27000
    for h in (3000, 2400):
        center = len(points)
        points.append((-8500,9800,h))
        for ring in range(1, rings+1):
            for s in range(sides):
                angle = math.tau*s/sides
                points.append((-8500+radius*ring/rings*math.cos(angle),9800+radius*ring/rings*math.sin(angle),h))
        for s in range(sides):
            faces.append((center,center+1+s,center+1+(s+1)%sides))
        for ring in range(rings-1):
            for s in range(sides):
                a, b = center+1+ring*sides+s, center+1+ring*sides+(s+1)%sides
                faces.extend(((a,a+sides,b+sides),(a,b+sides,b)))
    offset = 1+sides*rings
    for s in range(sides):
        a, b = 1+(rings-1)*sides+s, 1+(rings-1)*sides+(s+1)%sides
        faces.extend(((a,b,b+offset),(a,b+offset,a+offset)))
    objects.append(mesh("Rim_Source", points, faces))
    # 네이티브 Boolean 크래시에도 편집 원본과 마지막 완료 단계를 남긴다.
    out.mkdir(parents=True, exist_ok=True)
    source_report = {obj.name: {"vertices": len(obj.data.vertices), "polygons": len(obj.data.polygons)} for obj in objects}
    (out/"source-report.json").write_text(json.dumps(source_report, indent=2), encoding="utf-8")
    bpy.ops.wm.save_as_mainfile(filepath=str(out/"EastRim_Sources.blend"), check_existing=False, copy=True)
    if sources_only:
        return source_report
    body = objects[0].copy()
    body.data = objects[0].data.copy()
    body.name = "EastRim_UnionWork"
    source.objects.link(body)
    for operand in objects[1:]:
        (out/"union-progress.json").write_text(json.dumps({"status": "Running", "operand": operand.name,
                                                         "vertices": len(body.data.vertices), "polygons": len(body.data.polygons)}, indent=2), encoding="utf-8")
        modifier = body.modifiers.new("Union_"+operand.name, "BOOLEAN")
        modifier.operation, modifier.solver, modifier.object = "UNION", "EXACT", operand
        # 긴 modifier 스택을 한꺼번에 평가하지 않고 각 결과를 확정한다.
        bpy.context.view_layer.objects.active = body
        bpy.ops.object.modifier_apply(modifier=modifier.name)
        (out/"union-progress.json").write_text(json.dumps({"status": "Completed", "operand": operand.name,
                                                         "vertices": len(body.data.vertices), "polygons": len(body.data.polygons)}, indent=2), encoding="utf-8")
    evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
    bm = bmesh.new()
    data = evaluated.to_mesh()
    bm.from_mesh(data)
    evaluated.to_mesh_clear()
    # 구면 공면 합치기의 수치 찌꺼기만 0.1cm 이내에서 용접한다.
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-3)
    bmesh.ops.dissolve_degenerate(bm, dist=1e-3, edges=bm.edges[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    flat_edges = {e for f in bm.faces if f.calc_area() < 1e-10 for e in f.edges}
    if flat_edges:
        bmesh.ops.dissolve_edges(bm, edges=list(flat_edges), use_verts=True)
        bmesh.ops.triangulate(bm, faces=bm.faces[:], ngon_method="EAR_CLIP")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.verts.index_update()
    groups = {}
    for face in bm.faces:
        groups.setdefault(tuple(sorted(v.index for v in face.verts)), []).append(face)
    # 같은 정점의 반대 방향 두 면은 부피 없는 내부 막이므로 쌍으로 제거한다.
    cancelled = [f for group in groups.values() if len(group) == 2 and group[0].normal.dot(group[1].normal) < -.9999 for f in group]
    bmesh.ops.delete(bm, geom=cancelled, context="FACES_ONLY")
    bmesh.ops.delete(bm, geom=[e for e in bm.edges if not e.link_faces], context="EDGES")
    closed = pilot.topology_report(bm)
    if any(closed[k] for k in ("non_manifold_edges", "inconsistent_edges", "duplicate_faces", "degenerate_faces")) or closed["components"] != 1:
        debug = pilot.mesh_object("EastRim_InvalidUnion", [], [], source)
        bm.to_mesh(debug.data)
        bpy.ops.wm.save_as_mainfile(filepath=str(out/"EastRim_InvalidUnion.blend"), check_existing=False, copy=True)
        raise ValueError(f"Union topology failed: {closed}")
    for v in bm.verts:
        v.co = project(v.co*100)
    radial, tangent, bitangent = (Vector(p) for p in generator.center_frame())
    x0, x1, y0, y1 = BOUNDS
    boundaries = [((x0,y0),(x1,y0)),((x1,y0),(x1,y1)),((x1,y1),(x0,y1)),((x0,y1),(x0,y0))]
    planes = []
    inside = project(((x0+x1)/2,(y0+y1)/2,0))
    for a, b in boundaries:
        normal = project((*a,0)).cross(project((*b,0))).normalized()
        if normal.dot(inside) < 0:
            normal = -normal
        planes.append(normal)
        bmesh.ops.bisect_plane(bm, geom=list(bm.verts)+list(bm.edges)+list(bm.faces),
                              dist=1e-6, plane_co=Vector((0,0,0)), plane_no=normal, clear_inner=True, clear_outer=False)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bmesh.ops.dissolve_degenerate(bm, dist=1e-5, edges=bm.edges[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    conformance = {"max_vertex_movement_cm":0,"inserted_kinks":0}
    if interface_targets:
        targets = []
        for axis in ("x","y"):
            data = json.loads((Path(interface_targets)/f"seam-target-{axis}.json").read_text(encoding="utf-8"))
            normal = Vector(data["normal_ue"])
            normal.y = -normal.y
            segments = [[Vector((p[0],-p[1],p[2]))/100 for p in edge] for edge in data["segments_ue_cm"]]
            targets.append((normal,segments))

        def nearest(p,edges):
            candidates = []
            for a,b in edges:
                t = max(0,min(1,(p-a).dot(b-a)/(b-a).length_squared))
                q = a+(b-a)*t
                candidates.append(((p-q).length,q))
            return min(candidates,key=lambda pair:pair[0])

        for vertex in {v for e in bm.edges if e.is_boundary for v in e.verts}:
            touching = [(n,edges) for n,edges in targets if abs(vertex.co.dot(n)) < .0005]
            if not touching:
                raise ValueError("No reconstructed target for interface vertex")
            if len(touching) == 1:
                delta,position = nearest(vertex.co,touching[0][1])
            else:
                corners = [p for edge in touching[0][1] for p in edge if all(abs(p.dot(n)) < .0005 for n,_ in touching)]
                position = min(corners,key=lambda p:(p-vertex.co).length)
                delta = (position-vertex.co).length
            conformance["max_vertex_movement_cm"] = max(conformance["max_vertex_movement_cm"],delta*100)
            vertex.co = position
        bmesh.ops.remove_doubles(bm,verts=bm.verts[:],dist=1e-5)
        bmesh.ops.dissolve_degenerate(bm,edges=bm.edges[:],dist=1e-5)
        for normal,segments in targets:
            for point in {tuple(p) for edge in segments for p in edge}:
                p = Vector(point)
                vertices = {v for e in bm.edges if e.is_boundary for v in e.verts}
                if min((v.co-p).length for v in vertices) < .0001:
                    continue
                edges = [e for e in bm.edges if e.is_boundary and all(abs(v.co.dot(normal)) < .0005 for v in e.verts)]
                edge = min(edges,key=lambda e:nearest(p,[[v.co for v in e.verts]])[0])
                delta,_ = nearest(p,[[v.co for v in edge.verts]])
                if delta > .36:
                    raise ValueError(f"Target kink too far from interface: {delta*100}cm")
                _,vertex = bmesh.utils.edge_split(edge,edge.verts[0],.5)
                vertex.co = p
                conformance["inserted_kinks"] += 1
        bmesh.ops.triangulate(bm, faces=bm.faces[:], ngon_method="EAR_CLIP")
    # C++ 접합 띠와 가파른 외벽 현을 보존하고 완만한 면의 근사를 세분화한다.
    bm.verts.index_update()
    fixed = {tuple(sorted(v.index for v in e.verts)) for e in bm.edges if e.is_boundary}
    points = []
    for vertex in bm.verts:
        p = Vector((vertex.co.x,-vertex.co.y,vertex.co.z))*100
        points.append(Vector((30000*p.dot(bitangent)/p.dot(radial),30000*p.dot(tangent)/p.dot(radial),30000-p.length))/100)
    faces = [tuple(v.index for v in f.verts) for f in bm.faces]
    def needs_refinement(a,b,edge):
        # 1m 접합 띠는 기존 C++ 현을 유지한다. 그 밖의 새 외피만 구면에 근사한다.
        near_interface = min(abs(a.x-x1/100),abs(b.x-x1/100),abs(a.y-y1/100),abs(b.y-y1/100)) < 1
        shallow = abs(a.z-b.z) < .8*Vector((a.x-b.x,a.y-b.y)).length
        return edge not in fixed and not near_interface and shallow and (project((a+b)*50)-(project(a*100)+project(b*100))*.5).length > .005
    points,faces = subdivide(points,faces,needs_refinement)
    bm.free()
    data = bpy.data.meshes.new("EastRim_RefinedWork")
    data.from_pydata([project(p*100) for p in points],[],faces)
    bm = bmesh.new()
    bm.from_mesh(data)
    bpy.data.meshes.remove(data)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    report = pilot.topology_report(bm)
    unexpected = [e for e in bm.edges if not e.is_manifold and
                  (not e.is_boundary or not any(all(abs(v.co.dot(n)) < .0005 for v in e.verts) for n in planes))]
    tree = BVHTree.FromBMesh(bm)
    bm.faces.ensure_lookup_table()
    overlaps = [(a,b) for a,b in tree.overlap(tree) if a<b and not set(bm.faces[a].verts).intersection(bm.faces[b].verts)]
    if unexpected or overlaps or any(report[k] for k in ("inconsistent_edges", "duplicate_faces", "degenerate_faces")):
        raise ValueError(f"Export topology failed: {report}, unexpected={len(unexpected)}, intersections={len(overlaps)}")
    boundary_edges = [[list(v.co) for v in e.verts] for e in bm.edges if e.is_boundary]
    objects = generator.split_and_link(bm, f"Hollow_00_EastRim{version}", export)
    bm.free()
    out.mkdir(parents=True, exist_ok=True)
    exports = {}
    for obj in objects:
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        obj.data.calc_loop_triangles()
        ue = [(v.co.x*100,-v.co.y*100,v.co.z*100) for v in obj.data.vertices]
        exports[obj.name] = {"file":str(out/(obj.name+".fbx")), "triangles":len(obj.data.loop_triangles),
                             "bounds_cm":{"min":[min(p[i] for p in ue) for i in range(3)],"max":[max(p[i] for p in ue) for i in range(3)]}}
        bpy.ops.export_scene.fbx(filepath=exports[obj.name]["file"], use_selection=True, object_types={"MESH"},
                                 apply_scale_options="FBX_SCALE_NONE",global_scale=1,axis_forward="-Z",axis_up="Y",
                                 mesh_smooth_type="FACE",add_leaf_bones=False,bake_anim=False)
    for obj in source.objects:
        obj.hide_set(True)
        obj.hide_render = True
    report.update({"version":version,"region_xy_cm":BOUNDS,"replaced_generated_regions":REPLACED,
                   "cancelled_opposing_internal_faces":len(cancelled),"boolean_space":"LocalThenSphericalProjection",
                   "interface_conformance":conformance,
                   "closed_before_crop":closed,"unexpected_open_edges":len(unexpected),"nonadjacent_bvh_intersections":len(overlaps),
                   "exports":exports,"interface_edges_blender_m":boundary_edges,"original_objects_preserved":original.issubset(bpy.data.objects.keys()),
                   "limits":["Final generated interface and capsule not yet checked","No bake or PIE validation"]})
    (out/"manifest.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    bpy.ops.wm.save_as_mainfile(filepath=str(out/"EastRim.blend"),check_existing=False,copy=True)
    return {k:v for k,v in report.items() if k != "interface_edges_blender_m"}
