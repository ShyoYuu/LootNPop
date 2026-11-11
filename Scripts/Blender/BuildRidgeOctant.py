"""Ridge_00 형상 생성. Meadow 이음매를 보존한 지각 높이장 + 지각을 절단하는 소수의 닫힌 암반 몸체.

실행: blender -b --factory-startup --python Scripts/Blender/BuildRidgeOctant.py
출력: Saved/SliceOctant/Ridge_00/<VERSION>/ FBX·manifest·placement, Art/Octants/Ridge_00/Ridge_00_<VERSION>.blend
지각/암반 좌표 변환과 이음매 절차는 Grove V002 빌더와 같은 규약이다(옥탄트 원점 피벗, UE→Blender (x,-y,z)/100).
"""
import json
import math
import sys
from pathlib import Path
import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import delaunay_2d_cdt

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/"Scripts"))
import RidgeOctantSpec as spec
VERSION = spec.VERSION
OUT = ROOT/"Saved/SliceOctant/Ridge_00"/VERSION
ART = ROOT/"Art/Octants/Ridge_00"
EXPORTS = []
SOURCES = []


def bl(p):
    return Vector((p[0]/100, -p[1]/100, p[2]/100))


def ue(p):
    return Vector((p.x*100, -p.y*100, p.z*100))


def mesh(name, vertices, faces, recalc=True):
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices, [], faces)
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    if recalc:
        bm = bmesh.new()
        bm.from_mesh(data)
        bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=.0001)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        bmesh.ops.triangulate(bm, faces=bm.faces[:])
        bm.to_mesh(data)
        bm.free()
    return obj


def stats(bm, closed=True):
    bm.verts.index_update()
    keys = [tuple(sorted(v.index for v in f.verts)) for f in bm.faces]
    result = {"vertices": len(bm.verts), "triangles": len(bm.faces), "boundary_edges": sum(e.is_boundary for e in bm.edges),
              "non_manifold_non_boundary": sum(not e.is_manifold and not e.is_boundary for e in bm.edges),
              "inconsistent_edges": sum(e.is_manifold and not e.is_contiguous for e in bm.edges),
              "duplicate_faces": len(keys)-len(set(keys)), "degenerate_faces": sum(f.calc_area() < 1e-9 for f in bm.faces)}
    assert not any(result[k] for k in ("non_manifold_non_boundary", "inconsistent_edges", "duplicate_faces", "degenerate_faces")), result
    assert not closed or result["boundary_edges"] == 0, result
    return result


def terrain_body(rock):
    """윗면이 지형 높이장을 샘플링한 닫힌 몸체. 테두리 고리는 spec 외곽(300cm 간격)과 같고, 안쪽은 2m 격자다."""
    ring = [(x, y) for x, y, _ in rock["outline"]]
    coords = [Vector(p) for p in ring]
    edges = [(i, (i+1) % len(ring)) for i in range(len(ring))]
    xs, ys = [p[0] for p in ring], [p[1] for p in ring]
    # 지각과 같은 형상선 표본(경사로·잔도·단 가장자리)을 넣고, 50cm 안에 겹치는 2m 격자점은 뺀다.
    placed = set()
    for x, y in spec.feature_points():
        if inside(x, y, ring) and spec.signed_distance(x, y, ring) > 80:
            key = (round(x/50), round(y/50))
            if key not in placed:
                placed.add(key)
                coords.append(Vector((x, y)))
    for x in range(int(min(xs))//200*200, int(max(xs))+1, 200):
        for y in range(int(min(ys))//200*200, int(max(ys))+1, 200):
            if inside(x, y, ring) and spec.signed_distance(x, y, ring) > 80:
                kx, ky = round(x/50), round(y/50)
                if not any((kx+dx, ky+dy) in placed for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
                    coords.append(Vector((x, y)))
    verts, _, faces, orig, _, _ = delaunay_2d_cdt(coords, edges, [list(range(len(ring)))], 1, .001)
    bm = bmesh.new()
    top = [bm.verts.new((p.x, p.y, spec.ground(p.x, p.y))) for p in verts]
    # output_type=1은 제약 다각형 안쪽 삼각형만 돌려준다(중심점 판정을 다시 하면 테두리의 가는 삼각형이 빠져 구멍이 난다).
    for f in faces:
        bm.faces.new([top[i] for i in f])
    # 테두리 고리 정점(원래 인덱스 0..n-1)을 찾아 밑면·옆면을 만든다.
    rim = []
    for i in range(len(ring)):
        rim.append(next(top[j] for j, ids in enumerate(orig) if i in ids))
    bottom = [bm.verts.new((v.co.x, v.co.y, rock["bottom_abs"])) for v in rim]
    bm.faces.new(list(reversed(bottom)))
    for i in range(len(rim)):
        j = (i+1) % len(rim)
        bm.faces.new([rim[j], rim[i], bottom[i], bottom[j]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    data = bpy.data.meshes.new(rock["id"])
    bm.to_mesh(data)
    bm.free()
    body = bpy.data.objects.new(rock["id"], data)
    bpy.context.scene.collection.objects.link(body)
    carve(body, rock)
    return body


def rock_body(rock):
    if rock.get("terrain_top"):
        return terrain_body(rock)
    """볼록 외곽의 수직 각기둥. 윗면 정점은 각 외곽점 지면 위 지정 높이(top_abs면 절대 높이),
    밑면은 가장 낮은 지면보다 4m 아래(bottom_abs면 그 절대 높이: 협곡 위 석교)다."""
    outline = rock["outline"]
    n = len(outline)
    if "bottom_abs" in rock:
        bottom = rock["bottom_abs"]
    else:
        bottom = min(spec.ground(x, y) for x, y, _ in outline)-400
        # 볼록 외곽 사이 지면이 더 낮을 수 있으므로 변 중간도 확인한다.
        for (ax, ay, _), (bx, by, _) in zip(outline, outline[1:]+outline[:1]):
            for k in range(1, 8):
                bottom = min(bottom, spec.ground(ax+(bx-ax)*k/8, ay+(by-ay)*k/8)-400)
    cx, cy = spec.rock_center(rock)
    tops = [spec.rock_top(rock, x, y, top) for x, y, top in outline]
    # 부유섬은 밑면을 중심 쪽으로 줄여 뿔대로 만든다.
    k = rock.get("bottom_scale", 1.0)
    vertices = [(cx+(x-cx)*k, cy+(y-cy)*k, bottom) for x, y, _ in outline]+[(x, y, t) for (x, y, _), t in zip(outline, tops)]
    # 윗면 중앙점을 두어 비평면 윗면을 부채꼴 삼각형으로 닫는다.
    vertices.append((cx, cy, sum(tops)/n))
    faces = [tuple(reversed(range(n)))]
    faces += [(i, (i+1) % n, (i+1) % n+n, i+n) for i in range(n)]
    faces += [(n+i, n+(i+1) % n, 2*n) for i in range(n)]
    body = mesh(rock["id"], vertices, faces)
    if "cave" in rock:
        carve(body, rock)
    return body


def solid(name, bottom, top):
    """같은 꼭짓점 수의 아래/위 고리로 닫힌 몸체(다각형 뚜껑은 오목해도 된다)."""
    n = len(bottom)
    faces = [tuple(reversed(range(n))), tuple(range(n, 2*n))]
    faces += [(i, (i+1) % n, (i+1) % n+n, i+n) for i in range(n)]
    return mesh(name, list(bottom)+list(top), faces)


def loft_solid(name, rings):
    """고리(바닥 좌·우, 천장 우·좌) 목록을 이어 만든 닫힌 통로 절단체."""
    vertices = [v for ring in rings for v in ring]
    faces = [(3, 2, 1, 0)]
    for i in range(len(rings)-1):
        a, b = 4*i, 4*(i+1)
        faces += [(a, b, b+1, a+1), (a+1, b+1, b+2, a+2), (a+2, b+2, b+3, a+3), (a+3, b+3, b, a)]
    last = 4*(len(rings)-1)
    faces.append((last, last+1, last+2, last+3))
    return mesh(name, vertices, faces)


def carve(body, rock):
    """지하 공간: 방(바닥~천장)과 열린 내리막(지면 위로 뚫린 도랑)을 EXACT Boolean으로 판다.
    내리막은 방 벽 30cm 앞에서 끝나고, 짧은 지붕 있는 연결부가 방과 겹쳐 지붕에 틈을 남기지 않는다."""
    cutters = []
    for i, (kind, bottom, tops) in enumerate(spec.cave_parts(rock)):
        if kind == "loft":
            cutters.append(loft_solid(f"{rock['id']}_{kind}{i}", bottom))
            continue
        cutters.append(solid(f"{rock['id']}_{kind}{i}", bottom, [(x, y, t) for (x, y, _), t in zip(bottom, tops)]))
    for cutter in cutters:
        modifier = body.modifiers.new("Carve_"+cutter.name, "BOOLEAN")
        modifier.operation, modifier.solver, modifier.object = "DIFFERENCE", "EXACT", cutter
    bpy.context.view_layer.objects.active = body
    for modifier in list(body.modifiers):
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    for cutter in cutters:
        bpy.data.objects.remove(cutter, do_unlink=True)
    bm = bmesh.new()
    bm.from_mesh(body.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=.01)
    bmesh.ops.dissolve_degenerate(bm, dist=.01, edges=bm.edges[:])
    # 거의 같은 평면끼리의 Boolean이 남긴 작은 구멍(변 8개 이하)만 메운다. 큰 열린 경계는 그대로 두어 검사에서 드러나게 한다.
    bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if e.is_boundary], sides=8)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    # 큰 오목 몸체에서는 recalc가 전체를 안쪽으로 뒤집을 수 있다(V008 동굴). 부호 있는 부피로 바깥 방향을 확정한다.
    volume = sum(f.verts[0].co.dot(f.verts[1].co.cross(f.verts[2].co)) for f in bm.faces)
    if volume < 0:
        for f in bm.faces:
            f.normal_flip()
    bm.to_mesh(body.data)
    bm.free()


def export(obj, role):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    obj.data.calc_loop_triangles()
    positions = [ue(v.co) for v in obj.data.vertices]
    path = OUT/(obj.name+".fbx")
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"}, apply_scale_options="FBX_SCALE_NONE", global_scale=1,
                             axis_forward="-Z", axis_up="Y", mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False)
    EXPORTS.append({"name": obj.name, "role": role, "file": str(path), "triangles": len(obj.data.loop_triangles),
                    "bounds_cm": {side: [function(v[i] for v in positions) for i in range(3)] for side, function in (("min", min), ("max", max))}})


def finish(obj, name):
    """로컬 X/Y/H 닫힌 몸체를 긴 변만 분할한 뒤 구면 투영하고 Floor/Shell로 나눈다."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    stats(bm)
    bm.verts.index_update()
    positions = [v.co.copy() for v in bm.verts]
    triangles = [tuple(v.index for v in f.verts) for f in bm.faces]
    # 공유 변의 중점을 재사용해 부분 분할에서 T-정점이 생기지 않게 한다.
    for _ in range(9):
        selected = {tuple(sorted((a, b))) for f in triangles for a, b in zip(f, f[1:]+f[:1]) if (positions[a]-positions[b]).length > 600}
        if not selected:
            break
        middle = {}
        for a, b in selected:
            middle[(a, b)] = len(positions)
            positions.append((positions[a]+positions[b])*.5)
        result = []
        for f in triangles:
            flags = [tuple(sorted((f[i], f[(i+1) % 3]))) in selected for i in range(3)]
            count = sum(flags)
            if count == 0:
                result.append(f)
                continue
            start = flags.index(True) if count == 1 else (flags.index(False)+1) % 3 if count == 2 else 0
            a, b, c = (f[(start+i) % 3] for i in range(3))
            ab = middle.get(tuple(sorted((a, b))))
            bc = middle.get(tuple(sorted((b, c))))
            ca = middle.get(tuple(sorted((c, a))))
            if count == 1:
                result.extend(((a, ab, c), (ab, b, c)))
            elif count == 2:
                result.extend(((b, bc, ab), (a, ab, bc), (a, bc, c)))
            else:
                result.extend(((a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca)))
        triangles = result
    bm.clear()
    vertices = [bm.verts.new(bl(spec.point(*p))) for p in positions]
    for face in triangles:
        bm.faces.new([vertices[i] for i in face])
    # 연결 성분마다 법선을 맞춘다. 합친 메시 전체에 recalc를 한 번 걸면 큰 오목 몸체(지하 동굴)가 통째로 뒤집힐 수 있다(V008).
    # 구 중심이 원점이고 몸체는 원점을 감싸지 않으므로, 성분의 부호 있는 부피가 음수면 안쪽을 향한 것이다.
    remaining = set(bm.faces)
    while remaining:
        seed = remaining.pop()
        component, stack = [seed], [seed]
        while stack:
            f = stack.pop()
            for e in f.edges:
                for g in e.link_faces:
                    if g in remaining:
                        remaining.discard(g)
                        component.append(g)
                        stack.append(g)
        bmesh.ops.recalc_face_normals(bm, faces=component)
        volume = sum(f.verts[0].co.dot(f.verts[1].co.cross(f.verts[2].co)) for f in component)
        if volume < 0:
            for f in component:
                f.normal_flip()
    bm.normal_update()
    report = stats(bm)
    bm.verts.index_update()
    coordinates = [v.co.copy() for v in bm.verts]
    groups = {"Floor": [], "Shell": []}
    for f in bm.faces:
        floor = f.normal.dot(-f.calc_center_median().normalized()) >= .71
        groups["Floor" if floor else "Shell"].append(tuple(v.index for v in f.verts))
    assert sum(map(len, groups.values())) == report["triangles"]
    bm.free()
    for role, faces in groups.items():
        if not faces:
            continue
        used = sorted({v for f in faces for v in f})
        indices = {v: i for i, v in enumerate(used)}
        output = mesh(f"SM_Ridge_00_{VERSION}_{name}_{role}", [coordinates[v] for v in used],
                      [tuple(indices[v] for v in f) for f in faces], False)
        export(output, role)
    SOURCES.append({"id": name, "topology": report, "roles": {k: len(v) for k, v in groups.items()}})


def inside(x, y, polygon):
    result = False
    for a, b in zip(polygon, polygon[1:]+polygon[:1]):
        if (a[1] > y) != (b[1] > y) and x < (b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:
            result = not result
    return result


def surface_height(x, y, source_h):
    t = max(0, min(1, (min(spec.point(x, y, 0))-spec.SEAM_MARGIN)/spec.SEAM_BAND))
    return source_h+(spec.ground(x, y)-source_h)*t*t*(3-2*t)


def crust(cuts):
    """Meadow 지각의 이음매 띠(SEAM_MARGIN)는 원래 정점을 유지하고 내부만 4m 격자로 다시 삼각분할한다."""
    bpy.ops.import_scene.fbx(filepath=str(OUT.parent/"Baseline/MeadowSeam.fbx"))
    obj = next(o for o in bpy.context.selected_objects if o.type == "MESH")
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    radial, tangent, bitangent = Vector((1, 1, 1)).normalized(), Vector((-1, 1, 0)).normalized(), Vector((-1, -1, 2)).normalized()
    native = {}
    for v in bm.verts:
        p = ue(obj.matrix_world@v.co)
        if min(p) <= spec.SEAM_MARGIN+.01:
            native[v] = p.copy()
        depth = p.dot(radial)
        v.co = Vector((spec.RADIUS*p.dot(bitangent)/depth, spec.RADIUS*p.dot(tangent)/depth, spec.RADIUS-p.length))
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=.001)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.calc_area() < .0001], context="FACES")
    bm.verts.index_update()
    source_vertices = [v.co.copy() for v in bm.verts]
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    source = BVHTree.FromPolygons(source_vertices, [tuple(v.index for v in f.verts) for f in bm.faces], all_triangles=True)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if all(min(spec.point(*v.co)) > 2000 for v in f.verts)], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    boundary = [e for e in bm.edges if e.is_boundary and all(min(spec.point(*v.co)) > 1500 for v in e.verts)]
    adjacent = {}
    for e in boundary:
        for v in e.verts:
            adjacent.setdefault(v, []).append(e)
    assert all(len(edges) == 2 for edges in adjacent.values()), "Interior seam must be a loop"
    start = next(iter(adjacent))
    loop = [start]
    previous = None
    current = start
    while True:
        edge = next(e for e in adjacent[current] if e != previous)
        vertex = edge.other_vert(current)
        if vertex == start:
            break
        loop.append(vertex)
        current, previous = vertex, edge
    assert len(loop) == len(adjacent), "Unexpected interior hole"
    polygon = [tuple(v.co[:2]) for v in loop]
    if sum(a[0]*b[1]-b[0]*a[1] for a, b in zip(polygon, polygon[1:]+polygon[:1])) < 0:
        loop.reverse()
        polygon.reverse()
    coordinates = [Vector(p) for p in polygon]
    edges = [(i, (i+1) % len(loop)) for i in range(len(loop))]
    for cut in cuts:
        offset = len(coordinates)
        dense = []
        for a, b in zip(cut, cut[1:]+cut[:1]):
            a, b = Vector(a), Vector(b)
            count = max(1, math.ceil((b-a).length/300))
            dense.extend(a.lerp(b, i/count) for i in range(count))
        coordinates.extend(dense)
        edges.extend((offset+i, offset+(i+1) % len(dense)) for i in range(len(dense)))
    # 4m 격자. 네 모서리 높이차가 4m(약 45°)를 넘는 칸은 2m로 세분해 절벽 가장자리 톱니를 줄인다.
    heights = {}

    def height(x, y):
        key = (x, y)
        if key not in heights:
            heights[key] = spec.ground(x, y)
        return heights[key]
    fine = 0
    # 형상선 표본(경사로·절벽·분지 테)을 먼저 넣고, 1m 안에 겹치는 격자점은 뺀다(가는 삼각형 방지).
    occupied = set()
    features = 0
    placed = {}
    for x, y in spec.feature_points():
        key = (round(x/50), round(y/50))
        if key in placed or not inside(x, y, polygon) or any(inside(x, y, p) for p in cuts):
            continue
        if min(spec.point(x, y, 0)) < spec.SEAM_MARGIN+200:
            continue
        placed[key] = (x, y)
        coordinates.append(Vector((x, y)))
        features += 1

    def near_feature(x, y):
        kx, ky = round(x/50), round(y/50)
        return any(math.hypot(p[0]-x, p[1]-y) < 70 for dx in (-2, -1, 0, 1, 2) for dy in (-2, -1, 0, 1, 2)
                   for p in [placed.get((kx+dx, ky+dy))] if p)
    for x in range(-21200, 42500, 400):
        for y in range(-36800, 36900, 400):
            if inside(x, y, polygon) and not any(inside(x, y, p) for p in cuts):
                if not near_feature(x, y):
                    coordinates.append(Vector((x, y)))
                corners = [height(x+dx, y+dy) for dx in (0, 400) for dy in (0, 400)]
                if max(corners)-min(corners) > 150:
                    for dx, dy in ((200, 0), (0, 200), (200, 200)):
                        if inside(x+dx, y+dy, polygon) and not any(inside(x+dx, y+dy, p) for p in cuts) and not near_feature(x+dx, y+dy):
                            coordinates.append(Vector((x+dx, y+dy)))
                            fine += 1
    verts, _, faces, orig, _, _ = delaunay_2d_cdt(coordinates, edges, [list(range(len(loop)))], 1, .001)
    new = []
    for p, ids in zip(verts, orig):
        existing = [i for i in ids if i < len(loop)]
        if existing:
            new.append(loop[existing[0]])
        else:
            hit = source.ray_cast(Vector((*p, 3000)), Vector((0, 0, -1)), 8000)
            new.append(bm.verts.new((*p, hit[0].z if hit[0] is not None else 0)))
    for face in faces:
        center = sum((verts[i] for i in face), Vector((0, 0)))/len(face)
        if not any(inside(*center, p) for p in cuts):
            bm.faces.new([new[i] for i in face])
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    for v in bm.verts:
        if v in native:
            v.co = bl(native[v])
        else:
            x, y, h = v.co
            v.co = bl(spec.point(x, y, surface_height(x, y, h)))
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.normal_update()
    if sum(f.normal.dot(-f.calc_center_median().normalized()) for f in bm.faces) < 0:
        for f in bm.faces:
            f.normal_flip()
    report = stats(bm, False)
    bm.to_mesh(obj.data)
    bm.free()
    obj.matrix_world.identity()
    obj.name = f"SM_Octant_Ridge_00_{VERSION}_R30000"
    export(obj, "Crust")
    SOURCES.append({"id": "Crust", "topology": report, "interior_grid_cm": 400, "steep_cell_grid_cm": 200, "steep_extra_points": fine, "feature_points": features,
                    "native_seam_vertices": len(native)})


def build():
    assert not (OUT/"manifest.json").exists(), "Choose a new version"
    OUT.mkdir(parents=True, exist_ok=True)
    ART.mkdir(parents=True, exist_ok=True)
    rocks = [rock_body(rock) for rock in spec.active(spec.ROCKS)]
    body = rocks[0]
    # 서로 떨어진 닫힌 몸체를 한 메시로 합친다(Boolean 없이 연결 성분만 모은다).
    if len(rocks) > 1:
        bpy.ops.object.select_all(action="DESELECT")
        for r in rocks:
            r.select_set(True)
        bpy.context.view_layer.objects.active = body
        bpy.ops.object.join()
    body.name = "RocksSource"
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/"BeforeProjection.blend"), check_existing=False)
    finish(body, "Rocks")
    # 석교(cut=False)는 협곡 위에 떠 있으므로 지각을 자르지 않는다.
    cuts = [spec.rock_outline(rock, rock.get("cut_scale", .995)) for rock in spec.active(spec.ROCKS) if rock.get("cut", True)]
    crust(cuts)
    body.hide_render = True
    body.hide_set(True)
    manifest = {"name": "Ridge_00", "version": VERSION, "stage": spec.STAGE, "radius_cm": spec.RADIUS,
                "seam_margin_cm": spec.SEAM_MARGIN, "seam_transition_cm": spec.SEAM_BAND,
                "features": spec.features(),
                "rocks": spec.active(spec.ROCKS), "exports": EXPORTS, "sources": SOURCES,
                "limits": ["Native seam snap required in UE", "No engine capsule, bake or PIE validation yet"]}
    (OUT/"manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    (OUT/"placement.json").write_text(json.dumps(spec.placement(), indent=2), encoding="utf-8")
    bpy.ops.wm.save_as_mainfile(filepath=str(ART/f"Ridge_00_{VERSION}.blend"), check_existing=False)
    print("RIDGE_GEOMETRY_SUCCESS", json.dumps({"exports": len(EXPORTS), "triangles": sum(e["triangles"] for e in EXPORTS)}))


if __name__ == "__main__":
    build()
