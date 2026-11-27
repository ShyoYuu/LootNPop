"""IsleSkin 외피 몸체와 기존 충돌의 어긋남을 잰다(3a, 읽기 전용). 메시·맵·에셋을 만들거나 바꾸지 않는다.

입력
- 외피 몸체 FBX: Scripts/Profiling/Graphics/IsleSkin_V001/IsleSkin_<판>.json의 bodies(월드 좌표 저작).
- 기존 충돌 삼각형: UE InspectRidgeIsleSkinCollision.py dump <판> → Saved/.../IsleSkin_V001/Collision/Dump_<판>.bin/.json.

측정(몸체 삼각형마다 중심점·바깥 법선)
- 기존 충돌 표면까지 최근접 거리 d, 기존 형상 안/밖(현지 Up 광선이 기존 충돌을 맞힌 횟수의 홀짝, §7 판정과 같은 규약).
- 분류: 맞음(d ≤ 25cm, GX-031 Floor p95 목표), 노출(밖·d > 25cm = 보이는데 충돌이 없는 면), 묻힘(안·d > 25cm).
- 노출 면 중 위를 보는 면(법선·Up ≥ 0.71, GX-031 Floor 기준)의 연결 성분 넓이와 50㎡ 이상 수(GX-032).
- 이음매(월드 좌표 평면 x·y·z = 0)까지 최소 거리(후속 충돌 20m 금지).
- 보호 51공간·16,552표본(BuildRidgeIsleBody와 같은 표본화)이 몸체 안인지 + 그 표본이 기존 충돌 형상 안인지.
- LandmarkTop 사선 3개(BuildRidgeMonolithSkin.landmark_lines와 같은 정의)와 그래플 사선(RidgeBlueprint anchors: from 노드 +170cm → 앵커,
  착지점 +100/+170cm)의 50cm 표본이 몸체 안인지, 몸체까지 최소 거리.

실행: blender --background --python Scripts/Blender/MeasureRidgeIsleSkinCollision.py -- <배치 판> [덤프 이름]
결과: Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Collision/Measure_<덤프 이름|배치 판>.json
"""
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "Scripts")]
import RidgeOctantSpec as ridge  # noqa: E402

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else ["V007"]
VERSION = ARGS[0]
DUMP = ARGS[1] if len(ARGS) > 1 else VERSION
LAYOUT = ROOT / ("Scripts/Profiling/Graphics/IsleSkin_V001/IsleSkin_%s.json" % VERSION)
COLL = ROOT / "Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Collision"
BLUEPRINT = ROOT / "Scripts/Profiling/Graphics/RidgeBlueprint_V001.json"
TRIAL = ROOT / "Scripts/Profiling/Graphics/ClosedBodyTrial_V001/BodyTrial.json"
HELPER = ROOT / "Saved/Profiling/Graphics/Phase01/U4_2_V002/DiagnosePlacement.py"
MATCH_CM = 25.0
FLOOR_UP = 0.71
FLOOR_MIN_M2 = 50.0
SEAM_COLLISION_CM = 2000.0
LINE_STEP_CM = 50.0


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def q(values, ps=(0.5, 0.95, 1.0)):
    if not len(values):
        return None
    return [round(float(np.quantile(values, p)), 1) for p in ps]


def up_at(p):
    p = np.asarray(p, dtype=np.float64)
    return -p / np.linalg.norm(p)


def load_dump():
    meta = json.loads((COLL / ("Dump_%s.json" % DUMP)).read_text(encoding="utf-8"))
    raw = np.frombuffer((ROOT / meta["bin"]).read_bytes(), dtype="<f4").astype(np.float64).reshape(-1, 3, 3)
    assert hashlib.sha256((ROOT / meta["bin"]).read_bytes()).hexdigest() == meta["bin_sha256"]
    assert len(raw) == sum(m["dumped_triangles"] for m in meta["meshes"])
    owner = np.concatenate([np.full(m["dumped_triangles"], i) for i, m in enumerate(meta["meshes"])])
    return meta, raw, owner


def bvh_tris(tris):
    v = tris.reshape(-1, 3)
    f = np.arange(len(v)).reshape(-1, 3)
    return BVHTree.FromPolygons([tuple(p) for p in v.tolist()], [tuple(x) for x in f.tolist()], all_triangles=True)


def hits_along(bvh, origin, d, limit=64):
    count, o = 0, Vector(origin)
    d = Vector(d)
    for _ in range(limit):
        hit = bvh.ray_cast(o, d)
        if hit[0] is None:
            break
        count += 1
        o = hit[0] + d * 0.05
    return count


def inside_existing(bvh, p):
    """현지 Up 광선 홀짝: 기존 충돌(지각 + 닫힌 덩어리) 아래/안이면 홀수."""
    return hits_along(bvh, p, up_at(p)) % 2 == 1


def inside_closed(bvh, p):
    """닫힌 몸체 안 판정(BuildRidgeIsleBody.inside와 같은 세 방향 다수결)."""
    votes = 0
    for d in ((1, 0.0013, 0.0007), (0.0011, 1, 0.0009), (0.0008, 0.0012, 1)):
        votes += hits_along(bvh, p, d) % 2
    return votes >= 2


def load_body(row):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    path = ROOT / row["file"]
    assert sha(path) == row["fbx_sha256"], row["label"]
    bpy.ops.import_scene.fbx(filepath=str(path), axis_forward="-Z", axis_up="Y", global_scale=1.0)
    objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    assert len(objs) == 1, (row["label"], len(objs))
    obj = objs[0]
    mw = obj.matrix_world
    v = np.array([tuple(mw @ x.co) for x in obj.data.vertices], dtype=np.float64) * np.array([100., -100., 100.])
    t = []
    for poly in obj.data.polygons:
        ids = list(poly.vertices)
        for k in range(1, len(ids) - 1):
            t.append((ids[0], ids[k], ids[k + 1]))
    t = np.asarray(t)
    # 같은 위치 정점을 묶어 연결 성분을 셀 수 있게 한다(UV 이음매로 갈린 정점 대비, 0.1cm 격자).
    key = np.round(v * 10).astype(np.int64)
    _, weld = np.unique(key, axis=0, return_inverse=True)
    return v, t, weld.reshape(-1)


def tri_geometry(v, t):
    a, b, c = v[t[:, 0]], v[t[:, 1]], v[t[:, 2]]
    cr = np.cross(b - a, c - a)
    area = np.linalg.norm(cr, axis=1) / 2
    n = cr / np.maximum(np.linalg.norm(cr, axis=1, keepdims=True), 1e-12)
    cen = (a + b + c) / 3
    # 바깥 법선: 닫힌 몸체의 부호 부피가 양수가 되도록 방향을 맞춘다.
    volume = float(np.sum(np.einsum("ij,ij->i", a, np.cross(b, c))) / 6)
    if volume < 0:
        n = -n
    return cen, n, area, abs(volume)


def components(mask, t, weld, area):
    """mask 삼각형끼리 용접 정점을 공유하면 같은 성분(union-find)."""
    idx = np.nonzero(mask)[0]
    parent = {int(i): int(i) for i in idx}

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    owner = {}
    for i in idx:
        for w in weld[t[i]]:
            w = int(w)
            if w in owner:
                ra, rb = find(owner[w]), find(int(i))
                if ra != rb:
                    parent[ra] = rb
            else:
                owner[w] = int(i)
    groups = {}
    for i in idx:
        groups.setdefault(find(int(i)), []).append(int(i))
    return [(g, float(area[g].sum())) for g in groups.values()]


def line_points(a, b):
    a, b = np.asarray(a), np.asarray(b)
    n = max(1, int(np.linalg.norm(b - a) // LINE_STEP_CM))
    return [a + (b - a) * k / n for k in range(n + 1)]


def landmark_lines():
    crag = next(r for r in ridge.ROCKS if r["id"] == ridge.LANDMARK_ROCK)
    top = max(crag["outline"], key=lambda p: p[2])
    target = (top[0], top[1], ridge.ground(top[0], top[1]) + top[2] + 200)
    nodes = {n["id"]: n["xy"] for n in ridge.NODES}
    rows = {}
    for start in ("WestEntry", "CrownSummit", "DeepBasin"):
        x, y = nodes[start]
        rows["LandmarkTop/" + start] = line_points(ridge.point(x, y, ridge.ground(x, y) + 170), ridge.point(*target))
    return rows


def grapple_lines(blueprint):
    nodes = {n["id"]: n["local"] for n in blueprint["nodes"]}
    rows = {}
    for a in blueprint["anchors"]:
        anchor = ridge.point(*a["local"])
        for f in a["from"]:
            x, y, h = nodes[f]
            rows["Grapple/%s<-%s" % (a["id"], f)] = line_points(ridge.point(x, y, h + 170), anchor)
        lx, ly, lh = a["landing_local"]
        rows["Landing/%s" % a["id"]] = [np.asarray(ridge.point(lx, ly, lh + off)) for off in (100, 170)]
    return rows


def helper():
    env = {}
    source = HELPER.read_text(encoding="utf-8").split("\nraw =")[0]
    exec(compile(source, str(HELPER), "exec"), env)
    return env


def measure():
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    meta, raw, owner = load_dump()
    coll_all = bvh_tris(raw)
    floor_like = [i for i, m in enumerate(meta["meshes"]) if m["profile"] == "LNPStaticTerrain"]
    blueprint = json.loads(BLUEPRINT.read_text(encoding="utf-8"))
    env = helper()
    spaces = json.loads(TRIAL.read_text(encoding="utf-8"))["protected_spaces"]
    samples = [(c["id"], [np.asarray(p, dtype=np.float64) for p in env["samples"](c)]) for c in spaces]
    lines = {**landmark_lines(), **grapple_lines(blueprint)}
    # 보호 표본이 기존 충돌 형상 안인지는 몸체와 무관하므로 한 번만 잰다(몸체 경계 안 표본만).
    existing_cache = {}

    def existing(sid, k, p):
        key = (sid, k)
        if key not in existing_cache:
            existing_cache[key] = inside_existing(coll_all, p)
        return existing_cache[key]

    bodies = []
    for row in layout["bodies"]:
        v, t, weld = load_body(row)
        lo, hi = v.min(axis=0), v.max(axis=0)
        bounds_err = float(max(np.abs(lo - row["bounds_cm"][0]).max(), np.abs(hi - row["bounds_cm"][1]).max()))
        cen, n, area, volume = tri_geometry(v, t)
        ups = np.einsum("ij,ij->i", n, -cen / np.linalg.norm(cen, axis=1, keepdims=True))
        dist = np.empty(len(t))
        near_mesh = np.empty(len(t), dtype=np.int64)
        inside = np.zeros(len(t), dtype=bool)
        for i, c in enumerate(cen):
            hit = coll_all.find_nearest(Vector(c))
            dist[i] = hit[3]
            near_mesh[i] = owner[hit[2]]
            inside[i] = inside_existing(coll_all, c)
        matched = dist <= MATCH_CM
        exposed = (~inside) & ~matched
        buried = inside & ~matched
        floor_up = ups >= FLOOR_UP
        comps = components(exposed & floor_up, t, weld, area)
        comps.sort(key=lambda x: -x[1])
        comp_rows = []
        for g, a in comps[:8]:
            w = np.average(cen[g], axis=0, weights=area[g])
            comp_rows.append({"area_m2": round(a / 1e4, 2), "triangles": len(g),
                              "center_local_xyh_cm": [round(x, 1) for x in ridge.local(tuple(w))],
                              "max_gap_cm": round(float(dist[g].max()), 1)})
        bvh = BVHTree.FromPolygons([tuple(p) for p in v.tolist()], [tuple(x) for x in t.tolist()], all_triangles=True)
        prot_new, prot_existing, prot_rows = {}, {}, []
        for sid, pts in samples:
            for k, p in enumerate(pts):
                if (p < lo).any() or (p > hi).any() or not inside_closed(bvh, p):
                    continue
                was = existing(sid, k, p)
                if was:
                    prot_existing[sid] = prot_existing.get(sid, 0) + 1
                else:
                    prot_new[sid] = prot_new.get(sid, 0) + 1
                hit = coll_all.find_nearest(Vector(p))
                prot_rows.append({"space": sid, "sample": k, "inside_existing": was,
                                  "local_xyh_cm": [round(x, 1) for x in ridge.local(tuple(p))],
                                  "collision_distance_cm": round(float(hit[3]), 1),
                                  "nearest_collision": meta["meshes"][int(owner[hit[2]])]["actor"],
                                  "body_surface_distance_cm": round(float(bvh.find_nearest(Vector(p))[3]), 1)})
        # 여러 덩어리 메시(부유석·판·석판·파편)는 덩어리별로 크기·이음매 거리·노출 틈을 남긴다.
        part_rows = []
        for g, a in components(np.ones(len(t), dtype=bool), t, weld, area):
            pv = v[np.unique(t[g])]
            w = np.average(cen[g], axis=0, weights=area[g])
            up_g = [i for i in g if floor_up[i] and exposed[i]]
            part_rows.append({"center_local_xyh_cm": [round(x, 1) for x in ridge.local(tuple(w))],
                              "extent_cm": round(float(np.linalg.norm(pv.max(axis=0) - pv.min(axis=0))), 1),
                              "area_m2": round(a / 1e4, 2), "exposed_up_m2": round(float(area[up_g].sum()) / 1e4, 2),
                              "min_collision_gap_cm": round(float(dist[g].min()), 1),
                              "seam_min_cm": round(float(pv.min()), 1)})
        line_rows = {}
        for name, pts in lines.items():
            blocked = sum(1 for p in pts if (p >= lo).all() and (p <= hi).all() and inside_closed(bvh, p))
            near = min(bvh.find_nearest(Vector(p))[3] for p in pts)
            if blocked or near < 1000:
                line_rows[name] = {"blocked_samples": blocked, "min_distance_cm": round(float(near), 1)}

        def area_m2(mask):
            return round(float(area[mask].sum()) / 1e4, 2)

        by_mesh = {}
        for i, m in enumerate(meta["meshes"]):
            sel = exposed & (near_mesh == i)
            if sel.any():
                by_mesh[m["actor"]] = area_m2(sel)
        bodies.append({
            "label": row["label"], "role": row["role"], "triangles": int(len(t)), "bounds_error_cm": round(bounds_err, 4),
            "volume_m3": round(volume / 1e6, 1), "area_m2": area_m2(np.ones(len(t), dtype=bool)),
            "matched_m2": area_m2(matched), "exposed_m2": area_m2(exposed), "buried_m2": area_m2(buried),
            "exposed_gap_cm_p50_p95_max": q(dist[exposed]), "buried_gap_cm_p50_p95_max": q(dist[buried]),
            "exposed_up_m2": area_m2(exposed & floor_up),
            "exposed_up_components": len(comps), "exposed_up_components_ge50": sum(1 for _, a in comps if a / 1e4 >= FLOOR_MIN_M2),
            "largest_exposed_up": comp_rows, "exposed_nearest_collision_m2": by_mesh,
            "seam_min_cm": round(float(v.min()), 1), "seam_pass": bool(v.min() >= SEAM_COLLISION_CM),
            "protected_new": prot_new, "protected_inside_existing": prot_existing, "protected_samples": prot_rows,
            "parts": part_rows if len(part_rows) > 1 else [], "lines": line_rows})
        print("ISLESKIN_MEASURE_BODY " + json.dumps({k: bodies[-1][k] for k in (
            "label", "exposed_m2", "exposed_up_m2", "exposed_up_components_ge50", "seam_min_cm", "protected_new")}),
            flush=True)
    stair3 = next(pts for sid, pts in samples if sid == "StairRamp3CentralPassage") \
        if any(sid == "StairRamp3CentralPassage" for sid, _ in samples) else []
    report = {
        "version": VERSION, "dump": DUMP, "unit": "IsleSkinCollisionMeasure",
        "rules": {"match_cm": MATCH_CM, "floor_up": FLOOR_UP, "floor_min_m2": FLOOR_MIN_M2,
                  "seam_collision_cm": SEAM_COLLISION_CM, "line_step_cm": LINE_STEP_CM,
                  "inside_existing": "local Up ray parity against all dumped collision (crust + Floor + Shell)",
                  "inside_body": "3-direction ray parity majority (BuildRidgeIsleBody.inside)"},
        "collision": [{k: m[k] for k in ("actor", "mesh", "profile", "tags", "visible", "mesh_triangles", "dumped_triangles")}
                      for m in meta["meshes"]],
        "floor_like_meshes": [meta["meshes"][i]["actor"] for i in floor_like],
        "protected_spaces": len(spaces), "samples": sum(len(p) for _, p in samples),
        "stair_ramp3_samples": len(stair3),
        "stair_ramp3_inside_existing": [
            {"sample": k, "local_xyh_cm": [round(x, 1) for x in ridge.local(tuple(p))],
             "nearest_collision": meta["meshes"][int(owner[coll_all.find_nearest(Vector(p))[2]])]["actor"],
             "collision_distance_cm": round(float(coll_all.find_nearest(Vector(p))[3]), 1)}
            for k, p in enumerate(stair3) if inside_existing(coll_all, p)],
        "lines": {k: len(v) for k, v in lines.items()},
        "bodies": bodies,
        "inputs": {p.relative_to(ROOT).as_posix(): sha(p) for p in (
            LAYOUT, COLL / ("Dump_%s.json" % DUMP), ROOT / meta["bin"], BLUEPRINT, TRIAL, HELPER,
            ROOT / "Scripts/RidgeOctantSpec.py", Path(__file__))}}
    out = COLL / ("Measure_%s.json" % DUMP)
    out.write_text(json.dumps(report, ensure_ascii=False, indent=1), encoding="utf-8")
    print("ISLESKIN_MEASURE_DONE %s" % out, flush=True)


if __name__ == "__main__":
    measure()
