"""Ridge 생성 형상의 원본 검사: 경로 바닥·경사·몸 높이 통과, 경계 진입, Pod 자리·머리 공간, 사선 변화, 훅 시야·착지, 지각 열린 경계.

실행: blender -b --factory-startup --python Scripts/Blender/VerifyRidgeOctant.py
결과: Saved/SliceOctant/Ridge_00/<VERSION>/geometry-verification.json, native-probes-*.json(UE 실제 충돌 재확인용)
광선 검사이며 캡슐 sweep·Mover·훅 관성·NPC 교전을 대신하지 않는다.
"""
import json
import math
import sys
from pathlib import Path
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/"Scripts"))
import RidgeOctantSpec as spec
OUT = ROOT/"Saved/SliceOctant/Ridge_00"/spec.VERSION
bpy.ops.wm.open_mainfile(filepath=str(ROOT/f"Art/Octants/Ridge_00/Ridge_00_{spec.VERSION}.blend"))
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH" and o.name.startswith("SM_")]
WALK_COS = .71
EYE = 170


def bl(p):
    return Vector((p[0]/100, -p[1]/100, p[2]/100))


def ue(p):
    return Vector((p.x*100, -p.y*100, p.z*100))


def build(objects):
    vertices, faces, owner = [], [], []
    for obj in objects:
        obj.data.calc_loop_triangles()
        start = len(vertices)
        vertices.extend(v.co.copy() for v in obj.data.vertices)
        for f in obj.data.loop_triangles:
            faces.append(tuple(start+i for i in f.vertices))
            owner.append(obj.name)
    return BVHTree.FromPolygons(vertices, faces, all_triangles=True), owner


ALL, OWNER = build(meshes)
failures = []
probes = []


def ray(a, b):
    """로컬 a→b 광선. 첫 충돌의 (월드 cm 거리, 로컬 점, 법선 경사°, 소유 메시) 또는 None."""
    pa, pb = bl(spec.point(*a)), bl(spec.point(*b))
    delta = pb-pa
    hit, normal, index, distance = ALL.ray_cast(pa, delta.normalized(), delta.length)
    if hit is None:
        return None
    up = -hit.normalized()
    return distance*100, spec.local(tuple(ue(hit))), math.degrees(math.acos(min(1, abs(normal.dot(up))))), OWNER[index]


def floor(label, x, y, max_slope=44.8, record=True, layer="surface"):
    # 층별 기대 바닥: surface=지면(협곡은 석교 아래), deck=석교·부유섬 윗면, cave=지하 공간 바닥.
    expected = spec.layer_height(x, y, layer)
    start, end = [x, y, expected+300], [x, y, expected-300]
    result = ray(start, end)
    if result is None:
        failures.append({"kind": "MissingFloor", "where": label, "local": [x, y, expected]})
        return None
    distance, p, slope, owner = result
    if owner.endswith("_Shell"):
        failures.append({"kind": "ShellUnderfoot", "where": label, "local": p, "owner": owner})
    if slope > max_slope:
        failures.append({"kind": "Slope", "where": label, "local": p, "slope": slope})
    if record:
        probes.append([start, end, distance, "Floor"])
    return {"h": p[2], "slope": slope, "error": abs(p[2]-expected), "owner": owner}


def clear(label, a, b, kind="PassageClearance", record=True):
    result = ray(a, b)
    if result is not None:
        failures.append({"kind": "Blocked", "where": label, "from": a, "to": b, "hit": result[1], "owner": result[3]})
    elif record:
        probes.append([list(a), list(b), None, kind])
    return result is None


def sees(a, b):
    return ray(a, b) is None


report = {"version": spec.VERSION, "stage": spec.STAGE, "routes": {}, "pods": {}, "hooks": {}, "boundary": {}}

# 1. 보행 경로: 중심선과 양옆 2m 바닥, 무릎/머리 높이 통과.
for route in spec.active(spec.ROUTES):
    samples = []
    for (ax, ay), (bx, by) in zip(route["xy"], route["xy"][1:]):
        length = math.hypot(bx-ax, by-ay)
        n = max(1, int(length/200))
        side = (-(by-ay)/length, (bx-ax)/length)
        for k in range(n):
            samples.append((ax+(bx-ax)*k/n, ay+(by-ay)*k/n, side))
    slopes, errors, actual = [], [], []
    for x, y, side in samples:
        for offset in (-200, 0, 200):
            r = floor(route["id"], x+side[0]*offset, y+side[1]*offset, layer=spec.route_layer(route))
            if r:
                slopes.append(r["slope"])
                errors.append(r["error"])
            if offset == 0:
                # 이음매 전이 띠의 실제 지면은 Meadow 원본과 섞이므로 통과 광선은 실제 바닥 높이를 기준으로 한다.
                actual.append(r["h"] if r else spec.ground(x, y))
    for (x0, y0, _), (x1, y1, _), h0, h1 in zip(samples, samples[1:], actual, actual[1:]):
        for lift in (60, 180):
            clear(route["id"], [x0, y0, h0+lift], [x1, y1, h1+lift])
    report["routes"][route["id"]] = {"kind": route["kind"], "samples": len(samples)*3, "max_slope_deg": round(max(slopes), 2),
                                     "max_source_height_error_cm": round(max(errors), 2)}

# 2. 경계 진입: 세 변 각 19방향, 경계 안쪽 1~40m. Meadow 지각 높이에서 설계 지형으로 이어지는지 본다.
corners = [(42426.4, 0), (-21213.2, -36742.3), (-21213.2, 36742.3)]
center = (0, 0)
count = 0
for edge, (a, b) in enumerate(zip(corners, corners[1:]+corners[:1])):
    for i in range(1, 20):
        t = i/20
        ex, ey = a[0]+(b[0]-a[0])*t, a[1]+(b[1]-a[1])*t
        dx, dy = center[0]-ex, center[1]-ey
        n = math.hypot(dx, dy)
        for depth in (100, 1000, 2500, 4000):
            # 성분 거리로 depth만큼 안쪽인 점을 이분 탐색한다.
            lo, hi = 0.0, n
            for _ in range(30):
                mid = (lo+hi)/2
                if min(spec.point(ex+dx/n*mid, ey+dy/n*mid, 0)) < depth:
                    lo = mid
                else:
                    hi = mid
            x, y = ex+dx/n*hi, ey+dy/n*hi
            start, end = [x, y, 1500], [x, y, -1500]
            result = ray(start, end)
            if result is None or result[3].endswith("_Shell") or result[2] > 44.8:
                failures.append({"kind": "BoundaryFloor", "edge": edge, "local": [x, y], "result": result})
            else:
                probes.append([start, end, result[0], "Floor"])
            count += 1
report["boundary"] = {"edges": 3, "directions_per_edge": 19, "depths_cm": [100, 1000, 2500, 4000], "probes": count}

# 3. Pod: 중심과 반경 2.5m/5m 링의 보행 바닥(경사 30° 이하), 반경 3m 9점의 머리 공간.
# 머리 공간은 비행형 편성이면 27m(출현 요구 26.3m), 지상 적 전용(지하 동굴)이면 6m.
for pod in spec.active(spec.PODS):
    x, y = pod["xy"]
    layer = pod.get("layer", "surface")
    h = spec.layer_height(x, y, layer)
    headroom = 2700 if spec.pod_has_flyers(pod) else 600
    worst = 0
    for r, n in ((0, 1), (250, 8), (500, 12)):
        for i in range(n):
            a = i*math.tau/n
            res = floor("Pod_"+pod["id"], x+r*math.cos(a), y+r*math.sin(a), max_slope=30, layer=layer)
            if res:
                worst = max(worst, res["slope"])
    head = 0
    for i in range(9):
        r = 0 if i == 8 else 300
        a = i*math.tau/8
        xx, yy = x+r*math.cos(a), y+r*math.sin(a)
        gh = spec.layer_height(xx, yy, layer)
        clear("PodHeadroom_"+pod["id"], [xx, yy, gh+100], [xx, yy, gh+headroom], "PodHeadroom")
        head += 1
    report["pods"][pod["id"]] = {"condition": pod["condition"], "h": round(h), "max_slope_within_5m": round(worst, 2), "headroom_rays": head}

# 4. 사선: 검토점 눈높이 → Pod(150cm)·보스 암릉 상단. 지각과 암반 모두 포함한다.
def node_point(n, lift):
    return (n["xy"][0], n["xy"][1], spec.layer_height(*n["xy"], n.get("layer", "surface"))+lift)


nodes = {n["id"]: node_point(n, EYE) for n in spec.active(spec.NODES)}
targets = {"Pod_"+p["id"]: (p["xy"][0], p["xy"][1], spec.layer_height(*p["xy"], p.get("layer", "surface"))+150) for p in spec.active(spec.PODS)}
landmark = next(r for r in spec.active(spec.ROCKS) if r["id"] == spec.LANDMARK_ROCK)
top = max(landmark["outline"], key=lambda p: p[2])
# 첨탑 꼭대기 정점 바로 안쪽(몸체 표면 밖 2m 위)을 목표로 한다.
targets["LandmarkTop"] = (top[0], top[1], spec.rock_top(landmark, top[0], top[1], top[2])+200)
sight = {a: {b: sees(pa, pb) for b, pb in targets.items() if b != "Pod_"+a} for a, pa in nodes.items()}
report["sight_from_eye_170cm"] = sight
# 핵심 사선 변화: 진입부·계단 발치에서는 대분지가 가려지고, 정상·대지 가장자리에서는 내려다보인다.
for a, b, want in spec.SIGHT_EXPECTATIONS:
    if a in sight and b in sight[a] and sight[a][b] != want:
        failures.append({"kind": "SightExpectation", "from": a, "to": b, "expected": want})

# 5. 훅: 실제 출발 눈높이(190cm)에서 앵커까지 시야, 거리, 앵커 아래 착지 바닥(반경 4m).
for anchor in spec.placement()["anchors"]:
    entry = {}
    for source in anchor["from"]:
        start = node_point(next(n for n in spec.active(spec.NODES) if n["id"] == source), 190)
        ok = clear("Hook_"+anchor["id"], start, anchor["local"], "HookApproach")
        distance = (Vector(spec.point(*start))-Vector(spec.point(*anchor["local"]))).length
        if distance > anchor["max_distance"]:
            failures.append({"kind": "HookRange", "id": anchor["id"], "distance": distance})
        entry[source] = {"clear": ok, "distance_cm": round(distance)}
    lx, ly = anchor["landing"]
    landing = [floor("HookLanding_"+anchor["id"], lx+r*math.cos(a), ly+r*math.sin(a), layer=anchor["layer"])
               for r, n in ((0, 1), (400, 8)) for a in (i*math.tau/n for i in range(n))]
    under = ray(anchor["local"], [lx, ly, spec.layer_height(lx, ly, anchor["layer"])-300])
    if under is None or under[3].endswith("_Shell"):
        failures.append({"kind": "HookLandingUnder", "id": anchor["id"], "result": under})
    entry["landing_max_slope_deg"] = round(max(r["slope"] for r in landing if r), 2)
    report["hooks"][anchor["id"]] = entry

# 6. 지각 열린 경계: 이음매 평면 위(성분 거리 ≤ 1cm) 또는 암반 몸체 안쪽만 허용한다.
crust = next(o for o in meshes if o.name.startswith("SM_Octant_"))
import bmesh
bm = bmesh.new()
bm.from_mesh(crust.data)
# 지형 윗면 몸체는 지각을 외곽선에서 정확히 자르므로 확대하지 않은 외곽선과 거리로 판정한다
# (무게중심 기준 확대는 오목 꼭짓점 근처 변을 안쪽으로 옮겨 경계 위 점을 바깥으로 만든다).
rock_cuts = [spec.rock_outline(r) for r in spec.active(spec.ROCKS) if r.get("cut", True)]


def inside(x, y, polygon):
    result = False
    for a, b in zip(polygon, polygon[1:]+polygon[:1]):
        if (a[1] > y) != (b[1] > y) and x < (b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:
            result = not result
    return result


seam_edges = rock_edges = 0
for e in bm.edges:
    if not e.is_boundary:
        continue
    pts = [tuple(ue(v.co)) for v in e.verts]
    if all(min(p) <= 1.0 for p in pts):
        seam_edges += 1
        continue
    loc = [spec.local(p) for p in pts]
    # 지형 윗면 몸체는 지각을 외곽선에서 정확히 자르므로, 외곽선 위(5cm 이내) 점도 몸체 경계로 인정한다.
    if all(any(inside(l[0], l[1], c) or -spec.signed_distance(l[0], l[1], c) < 5 for c in rock_cuts) for l in loc):
        rock_edges += 1
        continue
    failures.append({"kind": "CrustOpenBoundary", "local": loc})
bm.free()
report["crust_open_boundary"] = {"seam_plane_edges": seam_edges, "inside_rock_edges": rock_edges}

report["probes"] = {k: sum(1 for p in probes if p[3] == k) for k in ("Floor", "PassageClearance", "PodHeadroom", "HookApproach")}
report["failures"] = failures[:200]
report["failure_count"] = len(failures)
report["limits"] = ["Source mesh rays only; not Chaos capsule sweeps, Mover, hook inertia, camera or NPC combat"]
chunks = []
for offset in range(0, len(probes), 250):
    name = "native-probes-"+str(offset//250).zfill(2)+".json"
    (OUT/name).write_text(json.dumps(probes[offset:offset+250]), encoding="utf-8")
    chunks.append(name)
(OUT/"native-probe-index.json").write_text(json.dumps(chunks), encoding="utf-8")
(OUT/"geometry-verification.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
print("RIDGE_VERIFY", json.dumps({"failures": len(failures), "probes": report["probes"]}))
