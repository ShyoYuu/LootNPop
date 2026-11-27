"""IsleSkin 외피 몸체에서 충돌 Shell(Blocker) 하나를 파생한다(3b, GX-041).

입력은 3a와 같다(IsleSkin_<원본 판>.json 몸체 FBX + UE 충돌 덤프 Dump_<원본 판>). 원본 메시·맵은 바꾸지 않는다.

규칙(V001)
- 대상: 외피 몸체 9개의 연결 덩어리 전부. 단 이음매(월드 x·y·z = 0 평면) 20m 안 덩어리(후속 충돌 20m 금지)와
  가장 긴 축이 100cm 미만인 덩어리(GX-031 100cm 미만 덩어리 제외)는 뺀다.
- 단순화: 몸체마다 Blender Decimate(collapse) 비율을 낮은 쪽부터 시험해, 외피 ↔ 프록시 양방향 표본 거리 p95 ≤ 25cm·최대 ≤ 75cm이고
  열린 변·비다양체 변 0인 첫 비율을 쓴다(만족하는 비율이 없으면 원본 1.0).
- 걷는 면 아래로 내리기: 프록시가 기존 Floor 역할 충돌(LNPStaticTerrain: 충돌 지각·Floor)보다 위 40cm ~ 아래 15cm 사이면
  그 면 5cm 아래로 현지 Up을 따라 내린다(정점마다 바로 아래 Floor, 없으면 옆으로 1m 안 최근접 Floor 점).
  평평한 큰 삼각형 가운데는 구면 Floor보다 뜨므로(현의 처짐) 걷는 면 근처 윗면의 정점은 단순화하지 않는다.
  남은 턱은 면 표본 7개로 찾아 그 면 꼭짓점을 더 내린다(최대 3회).
  기존 보행면 위에 Blocker 턱이 생기지 않게 한다.
- 역할: Shell만(새 Floor 없음, GX-041). 위를 보는 50㎡ 이상 성분도 Shell로 둔다(기록만 한다).
- 검사: 이음매 최소 ≥ 20m, 보호 51공간·16,552표본(새 점유는 석교 B의 SkyBridgeDeck만 허용, GX-041),
  LandmarkTop·그래플 사선·착지점 표본 막힘 0, 프록시 위 걷는 면 턱(위를 보는 프록시 면이 Floor 위 0~25cm) 넓이.

- V002: V001의 Floor 높이 부호 오류(Floor 위 Shell을 내리지 못함)를 고치고, Floor 삼각형 정점·중심에서 Shell 면이
  Floor 위 -2 ~ 40cm에 있으면(위를 보는 Shell 면만) 그 면을 내리고, 남으면 실패로 하는 검사를 더했다. 턱 반복은 최대 6회다. V001은 legacy_floor_sign으로 재현만 한다.

실행: blender --background --factory-startup --python Scripts/Blender/BuildRidgeIsleSkinCollision.py -- build V002
결과: Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Collision/Shell_<판>/(FBX·manifest.json·Build.json),
      Git 기록 Scripts/Profiling/Graphics/IsleSkin_V001/Collision_<판>.json
"""
import hashlib
import json
from pathlib import Path
import random
import sys

import bpy
import bmesh
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "Scripts/Blender"), str(ROOT / "Scripts")]
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else ["build", "V001"]
MODE, VERSION = ARGS[0], ARGS[1]
import MeasureRidgeIsleSkinCollision as m  # noqa: E402  (덤프·몸체 읽기·판정 함수 재사용)
import RidgeOctantSpec as ridge  # noqa: E402

RULES = {
    "V001": {"source_layout": "V007", "seam_collision_cm": 2000.0, "min_part_cm": 100.0,
             "ratios": [0.2, 0.3, 0.4, 0.5, 0.65, 0.8, 1.0], "fit_p95_cm": 25.0, "fit_max_cm": 75.0,
             "fit_samples": 6000, "seed": 41041, "floor_above_cm": 40.0, "floor_below_cm": 15.0, "floor_sink_cm": 5.0,
             "floor_lateral_cm": 100.0,
             "lip_cm": 25.0, "allowed_new_protected": {"BODY_SkyBridgeBodyB_V005": ["SkyBridgeDeck"]},
             "boundary_cm": 1.0, "name": "SM_Ridge_IsleSkin_V001_Shell", "legacy_floor_sign": True},
    # V002: Floor 높이 부호 수정. Floor 삼각형 표본 기준 검사(Shell 면이 Floor 위 -2 ~ 40cm면 실패)를 더한다.
    "V002": {"source_layout": "V007", "seam_collision_cm": 2000.0, "min_part_cm": 100.0,
             "ratios": [0.2, 0.3, 0.4, 0.5, 0.65, 0.8, 1.0], "fit_p95_cm": 25.0, "fit_max_cm": 75.0,
             "fit_samples": 6000, "seed": 41041, "floor_above_cm": 40.0, "floor_below_cm": 15.0, "floor_sink_cm": 5.0,
             "floor_lateral_cm": 100.0,
             "lip_cm": 25.0, "allowed_new_protected": {"BODY_SkyBridgeBodyB_V005": ["SkyBridgeDeck"]},
             "boundary_cm": 1.0, "name": "SM_Ridge_IsleSkin_V002_Shell",
             "floor_check_band_cm": [-2.0, 40.0], "sink_rounds": 6, "sink_cycles": 4},
}
RULE = RULES[VERSION]
m.VERSION = RULE["source_layout"]
m.DUMP = m.VERSION
m.LAYOUT = ROOT / ("Scripts/Profiling/Graphics/IsleSkin_V001/IsleSkin_%s.json" % m.VERSION)
OUT = ROOT / ("Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Collision/Shell_" + VERSION)
RECORD = ROOT / ("Scripts/Profiling/Graphics/IsleSkin_V001/Collision_%s.json" % VERSION)


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def parts_of(v, t, weld):
    area = np.ones(len(t))
    return [np.asarray(g) for g, _ in m.components(np.ones(len(t), dtype=bool), t, weld, area)]


def make(name, v, t):
    """UE cm 정점·삼각형으로 Blender 물체를 만들고 바깥 법선으로 맞춘다(BuildRidgeIsleBody.make와 같은 축 규약)."""
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata((np.asarray(v) * np.array([.01, -.01, .01])).tolist(), [], np.asarray(t)[:, ::-1].tolist())
    mesh.validate()
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()
    return obj


def ue_arrays(obj):
    mesh = obj.data
    mesh.calc_loop_triangles()
    v = np.array([tuple(x.co) for x in mesh.vertices], dtype=np.float64) * np.array([100., -100., 100.])
    t = np.array([tuple(lt.vertices) for lt in mesh.loop_triangles], dtype=np.int64)
    return v, t


def topology(obj):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    boundary = sum(1 for e in bm.edges if e.is_boundary)
    nonmanifold = sum(1 for e in bm.edges if not e.is_manifold and not e.is_boundary)
    bm.free()
    return {"boundary_edges": boundary, "nonmanifold_edges": nonmanifold}


def surface_samples(v, t, n, rng):
    a, b, c = v[t[:, 0]], v[t[:, 1]], v[t[:, 2]]
    area = np.linalg.norm(np.cross(b - a, c - a), axis=1) / 2
    idx = rng.choice(len(t), size=n, p=area / area.sum())
    r1, r2 = rng.random(n), rng.random(n)
    s = np.sqrt(r1)
    return a[idx] * (1 - s)[:, None] + b[idx] * (s * (1 - r2))[:, None] + c[idx] * (s * r2)[:, None]


def tree(v, t):
    return BVHTree.FromPolygons([tuple(p) for p in v.tolist()], [tuple(x) for x in t.tolist()], all_triangles=True)


def fit(src_v, src_t, dst_v, dst_t, n, seed):
    rng = np.random.default_rng(seed)
    out = {}
    for name, (av, at, bv, bt) in {"skin_to_proxy": (src_v, src_t, dst_v, dst_t),
                                   "proxy_to_skin": (dst_v, dst_t, src_v, src_t)}.items():
        b = tree(bv, bt)
        d = np.array([b.find_nearest(Vector(p))[3] for p in surface_samples(av, at, n, rng)])
        out[name] = [round(float(np.quantile(d, 0.95)), 1), round(float(d.max()), 1)]
    out["p95_cm"] = max(out["skin_to_proxy"][0], out["proxy_to_skin"][0])
    out["max_cm"] = max(out["skin_to_proxy"][1], out["proxy_to_skin"][1])
    return out


def decimated(name, v, t, ratio, keep=None):
    """Decimate(collapse). keep 정점(걷는 면 근처 윗면)은 정점 그룹으로 줄이지 않는다(큰 현의 처짐을 막는다)."""
    obj = make(name, v, t)
    if ratio < 1.0:
        mod = obj.modifiers.new("Decimate", "DECIMATE")
        mod.decimate_type = "COLLAPSE"
        mod.ratio = ratio
        mod.use_collapse_triangulate = True
        if keep is not None and keep.any():
            group = obj.vertex_groups.new(name="Decimate")
            group.add([int(i) for i in np.nonzero(~keep)[0]], 1.0, "REPLACE")
            mod.vertex_group = group.name
        bpy.context.view_layer.objects.active = obj
        with bpy.context.temp_override(object=obj, active_object=obj):
            bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


def face_points(v, t):
    """면마다 중심·변 중점·꼭짓점 7개 표본(평평한 큰 삼각형 가운데가 구면 Floor보다 뜨는 것을 잡는다)."""
    a, b, c = v[t[:, 0]], v[t[:, 1]], v[t[:, 2]]
    return np.stack([(a + b + c) / 3, (a + b) / 2, (b + c) / 2, (c + a) / 2, a, b, c], axis=1)


def floor_height(floor_bvh, p, above, below):
    """p가 바로 아래(현지 Up 반대) Floor 역할 면보다 위에 있는 높이. 위 above ~ 아래 below 밖이면 None."""
    up = m.up_at(p)
    hit = floor_bvh.ray_cast(Vector(p + up * below), Vector(-up), above + below)
    if hit[0] is None:
        return None
    # 광선은 p 위 below에서 출발한다. Floor까지 거리 t에서 p의 높이는 t - below다.
    # V001은 부호가 뒤집혀(below - t) Floor 위 Shell을 내리지 못했다(3c-1 베이크에서 발견). 재현용으로만 남긴다.
    return (below - float(hit[3])) if RULE.get("legacy_floor_sign") else (float(hit[3]) - below)


def lip_faces(v, t, floor_bvh, limit):
    """위를 보는 면 중 표본 하나라도 Floor 면 위 0 ~ limit cm에 있는 면(걷는 면 위 Blocker 턱)."""
    cen, nrm, area, _ = m.tri_geometry(v, t)
    ups = np.einsum("ij,ij->i", nrm, -cen / np.linalg.norm(cen, axis=1, keepdims=True))
    pts = face_points(v, t)
    out = []
    for i in np.nonzero(ups >= m.FLOOR_UP)[0]:
        hs = [floor_height(floor_bvh, p, limit, 0.0) for p in pts[i]]
        if any(h is not None and h >= 0.0 for h in hs):
            out.append(i)
    return np.asarray(out, dtype=np.int64), area


def walk_vertices(v, t, floor_bvh):
    """위를 보는 면 중 표본이 기존 Floor 역할 면 위 40cm ~ 아래 15cm에 있는 면의 정점(단순화에서 지킨다)."""
    above, below = RULE["floor_above_cm"], RULE["floor_below_cm"]
    cen, nrm, _, _ = m.tri_geometry(v, t)
    ups = np.einsum("ij,ij->i", nrm, -cen / np.linalg.norm(cen, axis=1, keepdims=True))
    pts = face_points(v, t)
    keep = np.zeros(len(v), dtype=bool)
    for f in np.nonzero(ups >= m.FLOOR_UP)[0]:
        if any(floor_height(floor_bvh, p, above, below) is not None for p in pts[f]):
            keep[t[f]] = True
    return keep


def floor_points(raw, owner, floor_idx, lo, hi):
    """몸체 경계 ±1m 안 Floor 역할 삼각형의 정점·중심(고유 점, 0.1cm 격자)과 그 메시 번호."""
    sel = np.isin(owner, floor_idx)
    tri, own = raw[sel], owner[sel]
    box = ((tri.max(axis=1) >= lo - 100).all(axis=1)) & ((tri.min(axis=1) <= hi + 100).all(axis=1))
    pts = np.concatenate([tri[box].reshape(-1, 3), tri[box].mean(axis=1)])
    ids = np.concatenate([np.repeat(own[box], 3), own[box]])
    _, first = np.unique(np.round(pts * 10).astype(np.int64), axis=0, return_index=True)
    return pts[first], ids[first]


def floor_band_hits(shell_bvh, pts, band):
    """Floor 점 바로 위 Shell 면이 위를 보는 면이고 Floor 위 band 안이면 (점 번호, 높이, Shell 면 번호)."""
    band_lo, band_hi = band
    out = []
    for i, fp in enumerate(pts):
        up = m.up_at(fp)
        hit = shell_bvh.ray_cast(Vector(fp + up * 300.0), Vector(-up), 300.0 - band_lo + 1.0)
        if hit[0] is None:
            continue
        # 벽이 바닥과 만나는 선(가파른 면)은 턱이 아니다. 위를 보는 Shell 면만 센다.
        if abs(float(np.dot(np.asarray(hit[1]), up))) < m.FLOOR_UP:
            continue
        h = float(np.dot(np.asarray(hit[0]) - fp, up))
        if band_lo < h <= band_hi:
            out.append((i, h, int(hit[2])))
    return out


def sink_under_floor(v, t, floor_bvh, fpts=None):
    """기존 Floor 역할 면(충돌 지각·Floor) 위 40cm ~ 아래 15cm 안의 정점을 그 면 5cm 아래로 내린다(현지 Up).
    정점마다 바로 아래 Floor 높이를 쓰고, 없으면 옆으로 1m 안 최근접 Floor 점을 쓴다(테두리 정점은 Floor 외곽선 바로 밖에 있다).
    그래도 위를 보는 면 표본이 Floor 위에 남으면 그 면 꼭짓점을 남은 높이만큼 더 내린다(V001 최대 3회).
    V002는 Floor 점 바로 위 Shell 면을 내리는 단계를 더해 두 단계를 번갈아 돈다(최대 4번, 면 단계는 번마다 6회)."""
    above, below, sink = RULE["floor_above_cm"], RULE["floor_below_cm"], RULE["floor_sink_cm"]
    lateral_max = RULE["floor_lateral_cm"]
    out = v.copy()
    total = np.zeros(len(v))
    for i, p in enumerate(v):
        up = m.up_at(p)
        height = floor_height(floor_bvh, p, above, below)
        if height is None:
            hit = floor_bvh.find_nearest(Vector(p), lateral_max + above)
            if hit[0] is None:
                continue
            floor = np.asarray(hit[0])
            h = float(np.dot(p - floor, up))
            if float(np.linalg.norm((p - floor) - up * h)) > lateral_max:
                continue
            height = h
        if -below < height <= above:
            out[i] = p - up * (height + sink)
            total[i] += height + sink
    rounds = 0
    # V002는 면 표본 단계와 Floor 표본 단계를 둘 다 고칠 것이 없을 때까지 번갈아 돈다(최대 4번).
    for _ in range(RULE.get("sink_cycles", 1)):
        changed = False
        for _ in range(RULE.get("sink_rounds", 3)):
            rounds += 1
            cen, nrm, _, _ = m.tri_geometry(out, t)
            ups = np.einsum("ij,ij->i", nrm, -cen / np.linalg.norm(cen, axis=1, keepdims=True))
            pts = face_points(out, t)
            need = np.zeros(len(out))
            for f in np.nonzero(ups >= m.FLOOR_UP)[0]:
                hs = [h for h in (floor_height(floor_bvh, p, RULE["lip_cm"], 0.0) for p in pts[f]) if h is not None and h >= 0.0]
                if hs:
                    need[t[f]] = np.maximum(need[t[f]], max(hs) + sink)
            if not need.any():
                break
            changed = True
            moving = need > 0
            out[moving] -= np.stack([m.up_at(p) for p in out[moving]]) * need[moving][:, None]
            total += need
        # Floor 표본 단계(V002): Floor 점 바로 위에 남은 위를 보는 Shell 면은 그 면 꼭짓점을 직접 내린다.
        if fpts is not None and "floor_check_band_cm" in RULE:
            for _ in range(4):
                hits = floor_band_hits(tree(out, t), fpts, RULE["floor_check_band_cm"])
                if not hits:
                    break
                changed = True
                need = np.zeros(len(out))
                for _, h, face in hits:
                    need[t[face]] = np.maximum(need[t[face]], h + sink)
                moving = need > 0
                out[moving] -= np.stack([m.up_at(p) for p in out[moving]]) * need[moving][:, None]
                total += need
        if not changed:
            break
    return out, int((total > 0).sum()), round(float(total.max()), 1), rounds


def build():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    layout = json.loads(m.LAYOUT.read_text(encoding="utf-8"))
    meta, raw, owner = m.load_dump()
    coll_all = m.bvh_tris(raw)
    floor_idx = [i for i, row in enumerate(meta["meshes"]) if row["profile"] == "LNPStaticTerrain"]
    floor_bvh = m.bvh_tris(raw[np.isin(owner, floor_idx)])
    blueprint = json.loads(m.BLUEPRINT.read_text(encoding="utf-8"))
    env = m.helper()
    spaces = json.loads(m.TRIAL.read_text(encoding="utf-8"))["protected_spaces"]
    samples = [(c["id"], [np.asarray(p, dtype=np.float64) for p in env["samples"](c)]) for c in spaces]
    lines = {**m.landmark_lines(), **m.grapple_lines(blueprint)}
    OUT.mkdir(parents=True, exist_ok=True)
    rows, merged_v, merged_t, issues = [], [], [], []
    for b_index, row in enumerate(layout["bodies"]):
        v, t, weld = m.load_body(row)
        keep, dropped = [], []
        for g in parts_of(v, t, weld):
            pv = v[np.unique(t[g])]
            span = float((pv.max(axis=0) - pv.min(axis=0)).max())
            seam = float(pv.min())
            center = [round(x, 1) for x in ridge.local(tuple(pv.mean(axis=0)))]
            if seam < RULE["seam_collision_cm"]:
                dropped.append({"reason": "seam", "seam_min_cm": round(seam, 1), "span_cm": round(span, 1), "center_local_xyh_cm": center})
            elif span < RULE["min_part_cm"]:
                dropped.append({"reason": "small", "seam_min_cm": round(seam, 1), "span_cm": round(span, 1), "center_local_xyh_cm": center})
            else:
                keep.append(g)
        if not keep:
            rows.append({"label": row["label"], "kept_parts": 0, "dropped": dropped})
            continue
        sel = np.concatenate(keep)
        used = np.unique(t[sel])
        remap = -np.ones(len(v), dtype=np.int64)
        remap[used] = np.arange(len(used))
        sv, st = v[used], remap[t[sel]]
        # 용접: FBX 반입의 UV 이음매 중복 정점을 합쳐 닫힌 몸체로 만든다.
        key, inv = np.unique(np.round(sv * 10).astype(np.int64), axis=0, return_inverse=True)
        inv = inv.reshape(-1)
        wv = np.zeros((len(key), 3))
        wv[inv] = sv
        wt = inv[st]
        wt = wt[(wt[:, 0] != wt[:, 1]) & (wt[:, 1] != wt[:, 2]) & (wt[:, 0] != wt[:, 2])]
        keep_mask = walk_vertices(wv, wt, floor_bvh)
        chosen, trials = None, []
        for ratio in RULE["ratios"]:
            obj = decimated("P_%d_%s" % (b_index, ratio), wv, wt, ratio, keep_mask)
            dv, dt = ue_arrays(obj)
            topo = topology(obj)
            f = fit(wv, wt, dv, dt, RULE["fit_samples"], RULE["seed"] + b_index)
            ok = (f["p95_cm"] <= RULE["fit_p95_cm"] and f["max_cm"] <= RULE["fit_max_cm"]
                  and not topo["boundary_edges"] and not topo["nonmanifold_edges"])
            trials.append({"ratio": ratio, "triangles": int(len(dt)), "fit": f, "topology": topo, "pass": ok})
            bpy.data.objects.remove(obj, do_unlink=True)
            if ok or ratio == 1.0:
                chosen = (ratio, dv, dt, f, topo)
                break
        ratio, dv, dt, f, topo = chosen
        fpts, fown = floor_points(raw, owner, floor_idx, dv.min(axis=0), dv.max(axis=0))
        sunk, moved, max_shift, sink_rounds = sink_under_floor(dv, dt, floor_bvh, fpts)
        post = fit(wv, wt, sunk, dt, RULE["fit_samples"], RULE["seed"] + 100 + b_index)
        # 검사: 이음매·보호 표본·사선.
        pb = tree(sunk, dt)
        lo, hi = sunk.min(axis=0), sunk.max(axis=0)
        prot_new, prot_existing = {}, {}
        for sid, pts in samples:
            for k, p in enumerate(pts):
                if (p < lo).any() or (p > hi).any() or not m.inside_closed(pb, p):
                    continue
                boundary = coll_all.find_nearest(Vector(p))[3] <= RULE["boundary_cm"]
                if boundary or m.inside_existing(coll_all, p):
                    prot_existing[sid] = prot_existing.get(sid, 0) + 1
                else:
                    prot_new[sid] = prot_new.get(sid, 0) + 1
        allowed = RULE["allowed_new_protected"].get(row["label"], [])
        bad_new = {k: n for k, n in prot_new.items() if k not in allowed}
        blocked = {}
        for name, pts in lines.items():
            n = sum(1 for p in pts if (p >= lo).all() and (p <= hi).all() and m.inside_closed(pb, p))
            if n:
                blocked[name] = n
        cen, nrm, area, _ = m.tri_geometry(sunk, dt)
        ups = np.einsum("ij,ij->i", nrm, -cen / np.linalg.norm(cen, axis=1, keepdims=True))
        lip_idx, _ = lip_faces(sunk, dt, floor_bvh, RULE["lip_cm"])
        lip = float(area[lip_idx].sum()) if len(lip_idx) else 0.0
        lip_where = []
        for i in lip_idx[np.argsort(-area[lip_idx])][:10] if len(lip_idx) else []:
            hs = [floor_height(floor_bvh, p, RULE["lip_cm"], 0.0) for p in face_points(sunk, dt)[i]]
            lip_where.append({"local_xyh_cm": [round(c, 1) for c in ridge.local(tuple(cen[i]))],
                              "area_m2": round(float(area[i]) / 1e4, 3),
                              "max_above_floor_cm": round(max(h for h in hs if h is not None), 1)})
        floor_hits, floor_where = {}, []
        if "floor_check_band_cm" in RULE:
            for i, h, _ in floor_band_hits(pb, fpts, RULE["floor_check_band_cm"]):
                key = meta["meshes"][int(fown[i])]["actor"]
                floor_hits[key] = floor_hits.get(key, 0) + 1
                floor_where.append({"floor": key, "local_xyh_cm": [round(c, 1) for c in ridge.local(tuple(fpts[i]))],
                                    "shell_above_cm": round(h, 1)})
            if floor_hits:
                issues.append("shell near floor %s %s" % (row["label"], floor_hits))
        comps = m.components((ups >= m.FLOOR_UP), dt, np.arange(len(sunk)), area)
        seam = float(sunk.min())
        if seam < RULE["seam_collision_cm"]:
            issues.append("seam " + row["label"])
        if bad_new:
            issues.append("protected %s %s" % (row["label"], bad_new))
        if blocked:
            issues.append("lines %s %s" % (row["label"], blocked))
        if topo["boundary_edges"] or topo["nonmanifold_edges"]:
            issues.append("topology " + row["label"])
        if lip > 0:
            issues.append("walk lip %s %.2f m2" % (row["label"], lip / 1e4))
        rows.append({"label": row["label"], "role": row["role"], "kept_parts": len(keep), "dropped": dropped,
                     "source_triangles": int(len(wt)), "ratio": ratio, "triangles": int(len(dt)), "trials": trials,
                     "fit": f, "fit_after_sink": post, "kept_walk_vertices": int(keep_mask.sum()), "sunk_vertices": moved, "max_sink_cm": max_shift, "sink_rounds": sink_rounds,
                     "topology": topo, "seam_min_cm": round(seam, 1), "protected_new": prot_new,
                     "protected_inside_existing": prot_existing, "blocked_lines": blocked,
                     "walk_lip_m2": round(lip / 1e4, 2), "floor_band_samples": floor_hits, "floor_band_where": floor_where[:40], "walk_lip_where": lip_where,
                     "up_components_ge50_m2": sorted([round(a / 1e4, 1) for _, a in comps if a / 1e4 >= m.FLOOR_MIN_M2],
                                                     reverse=True)})
        merged_t.append(dt + sum(len(x) for x in merged_v))
        merged_v.append(sunk)
        print("ISLESKIN_SHELL_BODY " + json.dumps({k: rows[-1][k] for k in (
            "label", "kept_parts", "ratio", "triangles", "fit", "sunk_vertices", "protected_new", "walk_lip_m2")}), flush=True)
    v = np.concatenate(merged_v)
    t = np.concatenate(merged_t)
    obj = make(RULE["name"], v, t)
    topo = topology(obj)
    v, t = ue_arrays(obj)
    path = OUT / (RULE["name"] + ".fbx")
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"},
                             apply_scale_options="FBX_SCALE_NONE", global_scale=1, axis_forward="-Z", axis_up="Y",
                             mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False)
    bounds = {"min": v.min(axis=0).round(4).tolist(), "max": v.max(axis=0).round(4).tolist()}
    manifest = {"exports": [{"name": RULE["name"], "file": str(path), "triangles": int(len(t)), "bounds_cm": bounds,
                             "role": "Shell", "sha256": sha(path)}]}
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    inputs = {p.relative_to(ROOT).as_posix(): sha(p) for p in (
        m.LAYOUT, m.COLL / ("Dump_%s.json" % m.DUMP), ROOT / meta["bin"], m.BLUEPRINT, m.TRIAL, m.HELPER,
        ROOT / "Scripts/RidgeOctantSpec.py", Path(m.__file__), Path(__file__))}
    report = {"version": VERSION, "unit": "IsleSkinCollisionShell", "decision": "GX-041", "rules": RULE,
              "output": {"name": RULE["name"], "file": path.relative_to(ROOT).as_posix(), "sha256": sha(path),
                         "triangles": int(len(t)), "bounds_cm": bounds, "topology": topo},
              "bodies": rows, "issues": issues, "pass": not issues, "inputs": inputs}
    (OUT / "Build.json").write_text(json.dumps(report, ensure_ascii=False, indent=1), encoding="utf-8")
    record = {k: report[k] for k in ("version", "unit", "decision", "rules", "output", "issues", "pass", "inputs")}
    record["bodies"] = [{k: r.get(k) for k in ("label", "kept_parts", "dropped", "ratio", "triangles", "fit",
                                               "fit_after_sink", "sunk_vertices", "max_sink_cm", "seam_min_cm", "protected_new",
                                               "protected_inside_existing", "walk_lip_m2", "floor_band_samples", "up_components_ge50_m2")}
                        for r in rows]
    RECORD.write_text(json.dumps(record, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    print("ISLESKIN_SHELL_DONE " + json.dumps({"pass": report["pass"], "issues": issues, "triangles": int(len(t)),
                                               "topology": topo}), flush=True)


if __name__ == "__main__":
    assert MODE == "build", MODE
    build()
