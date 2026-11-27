"""Phase 1-BR 섬 윗면 단위(2a): 섬 외피 몸체 V005의 윗면 덮개만 잘게 다시 나눈 몸체를 만든다.

Blender --background --factory-startup --python Scripts/Blender/BuildRidgeIsleTop.py -- cap V010
(작업 폴더는 프로젝트 루트.) 결과: Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Geometry_<판>/ 의 FBX·Build.json.

- 문제: V005 덮개는 중심 정점 하나에서 테두리 240정점으로 퍼진 부채꼴(한 장 길이 약 29m)이다. 평면 삼각형의
  가운데가 구면(반지름 약 24.5km)보다 위로 떠서 보행 Floor보다 최대 약 25cm 위에 보인다(발이 묻혀 보임).
- 해법: 옆면·밑면·테두리 고리(윗면 3cm 아래)는 V005와 정점까지 같게 두고, 덮개만 약 2.9m 간격 동심 고리
  (각도 수 240 → 120 → 60 → 30 → 15)로 다시 나눈다. 모든 덮개 정점은 V011 윗면 3cm 아래 구면 위에 있고,
  변이 3m 안팎이라 현의 처짐은 0.5cm 안이다. 따라서 덮개는 어디서나 Floor 아래에 숨는다.
- V005 몸체는 동결된 BuildRidgeIsleBody.py(V006 Acceptance 보존 대상)를 고치지 않고 라이브러리로 불러
  같은 규칙·같은 난수 흐름으로 재현한다. 부유석 재시도 횟수는 Geometry_V005/Build.json 기록을 따른다.
  재현한 정점이 V005 기록(정점 수·경계·부유석 중심)과 같은지 먼저 확인한다.
- 부유석·석교 몸체는 바꾸지 않는다(V005 FBX를 계속 쓴다).
- 보호 51공간·16,552표본 BVH 검사와 Floor 대비 높이 측정(충돌 덤프 Dump_V009의 PreservedRocks_Floor)을 함께 기록한다.
"""
import hashlib
import json
import math
from pathlib import Path
import random
import sys

import bpy  # noqa: F401  (BuildRidgeIsleBody가 bpy를 쓴다)
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(Path(__file__).resolve().parent), str(ROOT / "Scripts")]
import BuildRidgeIsleBody as ib  # noqa: E402  (읽기 전용으로 함수만 쓴다)

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else ["cap", "V010"]
MODE, VERSION = ARGS[0], ARGS[1]
OUT = ROOT / ("Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Geometry_" + VERSION)
SOURCE = ROOT / "Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Geometry_V005/Build.json"
DUMP = ROOT / "Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Collision/Dump_V009"

# 판별 규칙. 고리는 테두리(f=1)에서 중심(f=0)으로, 각 고리의 각도 수는 바깥 고리 수와 같거나 절반이다.
RULES = {
    "V010": {
        "source_rules": "V005",
        "cap_depth_cm": 3,
        "rings": [(1.0, 240), (0.9, 120), (0.8, 60), (0.7, 60), (0.6, 60), (0.5, 60), (0.4, 30), (0.3, 30),
                  (0.2, 15), (0.1, 15)],
        "floor_tolerance_cm": 0.0,
    },
}


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def replay(rules_v005, islands):
    """BuildRidgeIsleBody.build()의 V005 난수 흐름을 그대로 따라 섬 몸체 정점·삼각형을 다시 만든다."""
    source = json.loads(SOURCE.read_text(encoding="utf-8"))
    placed = {o["isle"]: o["placed"] for o in source["outputs"] if o["role"] == "satellites"}
    body_rows = {o["isle"]: o for o in source["outputs"] if o["role"] == "isle_body"}
    rng = random.Random(rules_v005["seed"])
    out, checks = {}, {}
    for rid in ("ValleySkyIsle", "WestSkyIsle"):
        it = islands[rid]
        outline = [tuple(p[:2]) for p in it["outline"]]
        top = it["island_top"]
        v, t = ib.isle_body(rid, outline, top, rules_v005, rng)
        assert rules_v005.get("spikes") and rules_v005.get("spikes_discard")
        ib.spikes(rid, outline, top, rules_v005, rng)
        sat = rules_v005["satellites"]
        cx, cy = ib.CENTERS[rid]
        total = sat["rocks"] + sat["chips"]
        centers = []
        for i in range(total):
            for attempt in range(placed[rid][i]["retries"] + 1):
                th = math.tau * (i + rng.uniform(0.1, 0.9)) / total
                radius = rng.uniform(*sat["radius_cm"])
                depth = rng.uniform(*sat["depth_cm"])
                size = rng.uniform(*(sat["rock_size_cm"] if i < sat["rocks"] else sat["chip_size_cm"]))
                center = (cx + radius * math.cos(th), cy + radius * math.sin(th), top - depth)
                ib.rock(center, size, rng)
            centers.append(center)
        src = body_rows[rid]
        center_err = max(abs(a - b) for c, r in zip(centers, placed[rid]) for a, b in zip(c, r["center_xyh_cm"]))
        bounds_err = max(abs(a - b) for g, e in zip([v.min(axis=0).tolist(), v.max(axis=0).tolist()], src["bounds_cm"])
                         for a, b in zip(g, e))
        checks[rid] = {"vertices": [len(v), src["vertices"]], "triangles": [len(t), src["triangles"]],
                       "bounds_error_cm": bounds_err, "satellite_center_error_cm": center_err}
        assert len(v) == src["vertices"] and len(t) == src["triangles"], checks[rid]
        assert bounds_err < 0.05 and center_err < 1e-6, checks[rid]
        out[rid] = (v, t, outline, top)
    return out, checks


def recap(rid, v, t, rules):
    """부채꼴 덮개(중심 정점 + 첫 고리)를 동심 고리 덮개로 바꾼다. 다른 정점·삼각형은 그대로 둔다."""
    top_center = len(v) - 2
    rim = sorted({int(i) for tri in t if top_center in tri for i in tri if i != top_center})
    count = rules["rings"][0][1]
    assert rim == list(range(count)), "cap ring must be the first ring (indices 0..n-1)"
    keep = np.array([top_center not in tri for tri in t])
    old_cap = len(t) - int(keep.sum())
    tris = t[keep].copy()
    # 중심 정점을 빼고 그 뒤 인덱스(꼭짓점 tip)를 하나씩 당긴다.
    verts = np.delete(v, top_center, axis=0)
    tris[tris > top_center] -= 1
    cx, cy = ib.CENTERS[rid]
    rim_local = [ib.spec.ridge.local(tuple(verts[i])) for i in range(count)]
    h = rim_local[0][2]
    assert max(abs(p[2] - h) for p in rim_local) < 0.01, "rim ring is not at one height"
    verts = verts.tolist()
    rings = [list(range(count))]
    for f, m in rules["rings"][1:]:
        step = count // m
        ring = []
        for j in range(m):
            x, y, _ = rim_local[j * step]
            ring.append(len(verts))
            verts.append(ib.world(cx + f * (x - cx), cy + f * (y - cy), h).tolist())
        rings.append(ring)
    center = len(verts)
    verts.append(ib.world(cx, cy, h).tolist())
    new = []
    for outer, inner in zip(rings, rings[1:]):
        no, ni = len(outer), len(inner)
        if no == ni:
            for j in range(no):
                k = (j + 1) % no
                new += [(inner[j], outer[k], outer[j]), (inner[j], inner[k], outer[k])]
        else:
            assert no == 2 * ni, (no, ni)
            for j in range(ni):
                k = (j + 1) % ni
                o0, o1, o2 = outer[2 * j], outer[2 * j + 1], outer[(2 * j + 2) % no]
                new += [(inner[j], o1, o0), (inner[j], o2, o1), (inner[j], inner[k], o2)]
    last = rings[-1]
    for j in range(len(last)):
        new.append((center, last[(j + 1) % len(last)], last[j]))
    tris = np.concatenate([tris, np.asarray(new)])
    cap_tris = np.asarray(new)
    return np.asarray(verts), tris, {"old_cap_triangles": old_cap, "new_cap_triangles": len(new),
                                     "cap_height_h_cm": round(h, 3), "rings": rules["rings"]}, cap_tris


# ---- Floor 대비 높이(현지 Up 광선) ----

def load_floor():
    meta = json.loads(DUMP.with_suffix(".json").read_text(encoding="utf-8"))
    raw = np.fromfile(DUMP.with_suffix(".bin"), dtype="<f4").reshape(-1, 3, 3).astype(np.float64)
    labels = []
    for m in meta["meshes"]:
        labels += [m["actor"]] * m["dumped_triangles"]
    labels = np.array(labels)
    return raw[np.char.find(labels, "PreservedRocks_Floor") >= 0], sha(DUMP.with_suffix(".bin"))


def floor_heights(floor, points):
    """월드 점마다 같은 현지 Up 선이 Floor와 만나는 높이 H(가장 높은 것)."""
    v0, e1, e2 = floor[:, 0], floor[:, 1] - floor[:, 0], floor[:, 2] - floor[:, 0]
    out = []
    for p in points:
        d = p / np.linalg.norm(p)
        q = np.cross(d, e2)
        det = (e1 * q).sum(1)
        ok = np.abs(det) > 1e-9
        inv = np.where(ok, 1.0 / np.where(ok, det, 1), 0)
        t0 = -v0
        u = (t0 * q).sum(1) * inv
        r = np.cross(t0, e1)
        w = (r * d).sum(1) * inv
        dist = (e2 * r).sum(1) * inv
        hit = ok & (u >= -1e-6) & (w >= -1e-6) & (u + w <= 1 + 1e-6) & (dist > 0)
        out.append(float(ib.spec.ridge.RADIUS - dist[hit].min()) if hit.any() else None)
    return out


def cap_vs_floor(floor, verts, cap_tris):
    """덮개 삼각형마다 무게중심·세 변 중점의 높이에서 Floor 높이를 뺀다(양수 = 덮개가 Floor 위로 보임)."""
    pts = []
    for a, b, c in cap_tris:
        A, B, C = verts[a], verts[b], verts[c]
        pts += [(A + B + C) / 3, (A + B) / 2, (B + C) / 2, (C + A) / 2]
    pts = np.asarray(pts)
    heights = floor_heights(floor, pts)
    above = [ib.spec.ridge.local(tuple(p))[2] - f for p, f in zip(pts, heights) if f is not None]
    return {"points": len(pts), "with_floor": len(above), "max_above_cm": round(max(above), 3),
            "p95_above_cm": round(float(np.percentile(above, 95)), 3), "min_above_cm": round(min(above), 3)}


def build():
    rules = RULES[VERSION]
    OUT.mkdir(parents=True, exist_ok=True)
    ib.OUT = OUT  # export()가 쓰는 출력 폴더
    bpy.ops.wm.read_factory_settings(use_empty=True)
    blueprint = json.loads(ib.BLUEPRINT.read_text(encoding="utf-8"))
    islands = {i["id"]: i for i in blueprint["preserve"]["islands"]}
    bodies, replay_checks = replay(ib.RULES[rules["source_rules"]], islands)
    env = ib.helper()
    spaces = json.loads(ib.TRIAL.read_text(encoding="utf-8"))["protected_spaces"]
    samples = [(c["id"], env["samples"](c)) for c in spaces]
    floor, floor_sha = load_floor()
    rows, protected = [], []
    for rid, (v, t, outline, top) in bodies.items():
        # 비교용: V005 부채꼴 덮개도 같은 방식으로 잰다.
        old_cap = t[np.array([(len(v) - 2) in tri for tri in t])]
        before = cap_vs_floor(floor, v, old_cap)
        nv, nt, cap, cap_tris = recap(rid, v, t, rules)
        after = cap_vs_floor(floor, nv, cap_tris)
        name = "SM_Ridge_IsleBody_%s_%s" % (rid, VERSION)
        obj = ib.make(name, nv, nt)
        av, at = ib.arrays(obj)
        bvh = ib.bvh_of(av, at)
        lo, hi = av.min(axis=0), av.max(axis=0)
        new_hits, existing = 0, 0
        for sid, pts in samples:
            hits, inside_v011 = [], []
            for k, p in enumerate(pts):
                q = np.asarray(p)
                if (q < lo).any() or (q > hi).any() or not ib.inside(bvh, q):
                    continue
                p_local = ib.spec.ridge.local(tuple(q))
                if ib.v011_isle_inside(rid, outline, top, p_local, ib.RULES[rules["source_rules"]]):
                    inside_v011.append(k)
                else:
                    hits.append({"sample": k, "local_xyh_cm": [round(c, 1) for c in p_local]})
            if hits or inside_v011:
                protected.append({"object": name, "space": sid, "new_hits": hits,
                                  "inside_existing_v011_isle": inside_v011})
            new_hits += len(hits)
            existing += len(inside_v011)
        exported = ib.export(obj)
        exported.update({"role": "isle_body", "isle": rid, "collision": "NoCollision", "cap": cap,
                         "cap_vs_floor_before": before, "cap_vs_floor_after": after, "new_hits": new_hits,
                         "inside_existing_v011_isle": existing, "replay": replay_checks[rid]})
        rows.append(exported)
    report = {"version": VERSION, "unit": "IsleTop 2a", "rules": rules, "outputs": rows,
              "protected_spaces": len(spaces), "samples": sum(len(p) for _, p in samples), "protected": protected,
              "new_hits": sum(r["new_hits"] for r in rows),
              "inside_existing_v011_isle": sum(r["inside_existing_v011_isle"] for r in rows),
              "topology_pass": all(r["topology"]["boundary_edges"] == 0 and r["topology"]["nonmanifold_edges"] == 0
                                   for r in rows),
              "cap_below_floor": all(r["cap_vs_floor_after"]["max_above_cm"] <= rules["floor_tolerance_cm"] for r in rows),
              "inputs": {p.relative_to(ROOT).as_posix(): sha(p) for p in
                         (ib.BLUEPRINT, ib.TRIAL, ib.HELPER, SOURCE, Path(ib.__file__), Path(__file__))},
              "floor_dump_sha256": floor_sha}
    report["pass"] = report["new_hits"] == 0 and report["topology_pass"] and report["cap_below_floor"]
    (OUT / "Build.json").write_text(json.dumps(report, ensure_ascii=False, indent=1), encoding="utf-8")
    print("ISLE_TOP_CAP " + json.dumps({"pass": report["pass"], "new_hits": report["new_hits"],
                                         "topology_pass": report["topology_pass"],
                                         "caps": {r["isle"]: [r["cap_vs_floor_before"]["max_above_cm"],
                                                              r["cap_vs_floor_after"]["max_above_cm"],
                                                              r["triangles"]] for r in rows}}), flush=True)


# ---- 2b: 섬 위 엄폐 각기둥(GX-042) ----------------------------------------------------------------------------------
# V011 엄폐 각기둥(RidgeOctantSpec.ISLAND_COVER: 외곽 4점·꼭짓점별 절대 윗면·섬 윗면 1m 아래 바닥, 윗면은 중심 부채꼴)
# 안쪽에 지층·요철·둥근 모서리를 준 닫힌 몸체를 만든다. 몸체는 V011 각기둥 안에 있으므로 보호 표본 점유는
# 모두 "V011 엄폐 복원"(V011 각기둥 안 표본)으로만 나와야 한다. 충돌은 몸체를 단순화한 Shell 하나다.
COVER_RULES = {
    "V001": {
        "seed": 42042, "angular": 48, "rings": 22, "round_window": 3,
        "inset": 0.97, "taper": 0.07, "strata": {"amplitude": 0.035, "period_cm": 55, "warp": 0.8},
        "fine": 0.015, "detail_start_cm": 35, "cap_drop_cm": 12, "bevel": [(0.93, 22), (0.84, 8)],
        "cap_rings": [0.6, 0.3], "cap_dome_cm": 4,
        "shell_ratios": [0.08, 0.12, 0.18, 0.25, 0.35, 0.5, 0.7, 1.0], "fit_p95_cm": 25.0, "fit_max_cm": 75.0,
        "fit_samples": 4000, "floor_band_cm": [-2.0, 40.0], "seam_collision_cm": 2000.0, "floor_min_m2": 50.0,
        "visual_name": "SM_Ridge_IsleCover_{isle}_V001", "shell_name": "SM_Ridge_IsleCover_V001_Shell",
        "shell_folder": "Shell_Cover_V001",
    },
}
# V002: V001은 55cm 주기 지층 띠가 규칙적이라 쌓은 타이어처럼 보였다. 띠 주기·위상을 높이에 따라 흔들고 폭을 줄이며,
# 각도별 덩어리 요철(3·5갈래)과 약한 기울기·고르지 않은 윗면을 준다. V011 각기둥 안쪽 조건은 그대로다(inset으로 여유).
COVER_RULES["V002"] = dict(COVER_RULES["V001"], **{
    "seed": 42043, "inset": 0.93, "taper": 0.05,
    "strata": {"amplitude": 0.022, "period_cm": 95, "warp": 1.1, "drift": 0.9, "sharp": 2.2},
    "fine": 0.012, "lobes": [(3, 0.045), (5, 0.025)], "lean_cm": 14, "cap_noise_cm": 7,
    "visual_name": "SM_Ridge_IsleCover_{isle}_V002", "shell_name": "SM_Ridge_IsleCover_V002_Shell",
    "shell_folder": "Shell_Cover_V002",
})
COVER_ISLE = {"Valley": "ValleySkyIsle", "West": "WestSkyIsle"}


def cover_rocks():
    ridge = ib.spec.ridge
    return [r for r in ridge.ROCKS if r["role"] == "Cover" and r["id"].endswith(("IsleCoverA", "IsleCoverB", "IsleCoverC"))]


def cover_top(outline, center_h, x, y, cx, cy):
    """V011 부채꼴 윗면 높이: 중심(꼭짓점 윗면 평균)과 외곽 변의 선형 보간."""
    th = math.atan2(y - cy, x - cx)
    r_edge = ib.ray_radius([p[:2] for p in outline], (cx, cy), th)
    ex, ey = cx + r_edge * math.cos(th), cy + r_edge * math.sin(th)
    h_edge = None
    for a, b in zip(outline, outline[1:] + outline[:1]):
        ab = math.hypot(b[0] - a[0], b[1] - a[1])
        ae = math.hypot(ex - a[0], ey - a[1])
        if abs(ae + math.hypot(b[0] - ex, b[1] - ey) - ab) < 0.5:
            h_edge = a[2] + (b[2] - a[2]) * ae / ab
            break
    f = min(1.0, math.hypot(x - cx, y - cy) / r_edge)
    return center_h + (h_edge - center_h) * f, r_edge, f


def inside_polygon(poly, x, y):
    c = False
    for a, b in zip(poly, poly[1:] + poly[:1]):
        if (a[1] > y) != (b[1] > y) and x < a[0] + (y - a[1]) * (b[0] - a[0]) / (b[1] - a[1]):
            c = not c
    return c


def cover_body(rock, rules, rng):
    outline = rock["outline"]
    n = len(outline)
    cx, cy = sum(p[0] for p in outline) / n, sum(p[1] for p in outline) / n
    center_h = sum(p[2] for p in outline) / n
    bottom = rock["bottom_abs"]
    k = rules["angular"]
    thetas = [math.tau * i / k for i in range(k)]
    r0 = [ib.ray_radius([p[:2] for p in outline], (cx, cy), th) for th in thetas]
    w = rules["round_window"]
    smooth = [sum(r0[(i + j) % k] for j in range(-w, w + 1)) / (2 * w + 1) for i in range(k)]
    rr = [min(a, b) for a, b in zip(r0, smooth)]  # 둥근 모서리. min이라 V011 외곽 안에 남는다
    h_edge = [cover_top(outline, center_h, cx + r * math.cos(th), cy + r * math.sin(th), cx, cy)[0]
              for r, th in zip(r0, thetas)]
    phase = [rng.uniform(0, math.tau) for _ in range(4)]
    st = rules["strata"]
    # V002 이후 선택 규칙(없으면 V001과 같은 결과): 덩어리 요철·기울기·윗면 요철·띠 위상 흔들기.
    lobes = rules.get("lobes", [])
    lobe_phase = [rng.uniform(0, math.tau) for _ in lobes]
    lean = rules.get("lean_cm", 0.0)
    lean_dir = rng.uniform(0, math.tau) if lean else 0.0
    cap_noise = rules.get("cap_noise_cm", 0.0)
    drift, sharp = st.get("drift", 0.0), st.get("sharp", 4.0)
    verts, rings = [], []

    def put(x, y, h):
        verts.append(ib.world(x, y, h))
        return len(verts) - 1

    def lobe(th):
        return 1 - sum(a * (0.5 + 0.5 * math.sin(m * th + p)) for (m, a), p in zip(lobes, lobe_phase))

    def shift(u):
        return lean * u * math.cos(lean_dir), lean * u * math.sin(lean_dir)

    for j in range(rules["rings"] + 1):
        u = j / rules["rings"]
        ox, oy = shift(u)
        ring = []
        for i, th in enumerate(thetas):
            top_i = h_edge[i] - rules["cap_drop_cm"]
            h = bottom + (top_i - bottom) * u
            above = h - (bottom + 100)  # 섬 윗면(바닥 + 1m) 위 높이
            fade = max(0.0, min(1.0, (above - rules["detail_start_cm"]) / 60.0))
            band = math.tanh(sharp * math.sin(math.tau * above / st["period_cm"] + st["warp"] * math.sin(3 * th + phase[0])
                                              + drift * math.sin(above / 170.0 + phase[2])))
            s = rules["inset"] * (1 - rules["taper"] * u * u) * lobe(th)
            s *= 1 - fade * st["amplitude"] * (0.5 + 0.5 * band)
            s *= 1 - fade * rules["fine"] * (0.5 + 0.5 * math.sin(7 * th + phase[1] + 0.02 * above))
            ring.append(put(cx + ox + rr[i] * s * math.cos(th), cy + oy + rr[i] * s * math.sin(th), h))
        rings.append(ring)
    # 윗면 모서리 둥글리기와 덮개 고리(약간 볼록).
    last_s = rules["inset"] * (1 - rules["taper"])
    ox, oy = shift(1.0)
    for scale, drop in rules["bevel"]:
        ring = []
        for i, th in enumerate(thetas):
            g = last_s * scale * lobe(th)
            ring.append(put(cx + ox + rr[i] * g * math.cos(th), cy + oy + rr[i] * g * math.sin(th),
                            h_edge[i] - drop - cap_noise * 0.5 * (1 + math.sin(2 * th + phase[3]))))
        rings.append(ring)
    for f in rules["cap_rings"]:
        ring = []
        for i, th in enumerate(thetas):
            g = last_s * f * lobe(th)
            x, y = cx + ox + rr[i] * g * math.cos(th), cy + oy + rr[i] * g * math.sin(th)
            h = (cover_top(outline, center_h, x - ox, y - oy, cx, cy)[0] - 8 + rules["cap_dome_cm"] * (1 - f)
                 - cap_noise * 0.5 * (1 + math.sin(2 * th + phase[3])) * f)
            ring.append(put(x, y, h))
        rings.append(ring)
    top_center = put(cx + ox, cy + oy, center_h - 8 + rules["cap_dome_cm"])
    bottom_center = put(cx, cy, bottom)
    tris = []
    for i in range(k):
        j = (i + 1) % k
        tris.append((bottom_center, rings[0][i], rings[0][j]))
        for a, b in zip(rings, rings[1:]):
            tris.append((a[i], a[j], b[j]))
            tris.append((a[i], b[j], b[i]))
        tris.append((rings[-1][i], rings[-1][j], top_center))
    return np.asarray(verts), np.asarray(tris), {"center_local": [round(cx, 1), round(cy, 1)], "bottom_h": bottom,
                                                 "v011_top_h": [p[2] for p in outline], "outline": outline}


def decimate(name, v, t, ratio):
    obj = ib.make(name, v, t)
    if ratio < 1.0:
        mod = obj.modifiers.new("dec", "DECIMATE")
        mod.decimate_type = "COLLAPSE"
        mod.ratio = ratio
        mod.use_collapse_triangulate = True
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.ops.object.modifier_apply(modifier="dec")
    return obj


def fit(av, at, bv, bt, n, seed):
    from mathutils import Vector
    rng = np.random.default_rng(seed)
    out = {}
    for key, (sv, stt, dv, dt) in {"body_to_shell": (av, at, bv, bt), "shell_to_body": (bv, bt, av, at)}.items():
        a, b, c = sv[stt[:, 0]], sv[stt[:, 1]], sv[stt[:, 2]]
        area = np.linalg.norm(np.cross(b - a, c - a), axis=1) / 2
        idx = rng.choice(len(stt), size=n, p=area / area.sum())
        r1, r2 = rng.random(n), rng.random(n)
        s = np.sqrt(r1)
        pts = a[idx] * (1 - s)[:, None] + b[idx] * (s * (1 - r2))[:, None] + c[idx] * (s * r2)[:, None]
        bvh = ib.bvh_of(dv, dt)
        d = np.array([bvh.find_nearest(Vector(p))[3] for p in pts])
        out[key] = [round(float(np.quantile(d, 0.95)), 1), round(float(d.max()), 1)]
    out["p95_cm"] = max(out["body_to_shell"][0], out["shell_to_body"][0])
    out["max_cm"] = max(out["body_to_shell"][1], out["shell_to_body"][1])
    return out


def up_faces(v, t, floor, band, floor_min):
    """위를 보는 면(법선·Up ≥ 0.71): 넓이 합과, 중심이 Floor 위 band 안인 면(걷는 면 턱 후보) 넓이."""
    a, b, c = v[t[:, 0]], v[t[:, 1]], v[t[:, 2]]
    nrm = np.cross(b - a, c - a)
    area = np.linalg.norm(nrm, axis=1) / 2
    cen = (a + b + c) / 3
    up = -cen / np.linalg.norm(cen, axis=1, keepdims=True)
    dot = (nrm / np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-9) * up).sum(1)
    sel = np.nonzero(dot >= 0.71)[0]
    hf = floor_heights(floor, cen[sel]) if len(sel) else []
    lip = 0.0
    for k, f in zip(sel, hf):
        if f is not None and band[0] <= ib.spec.ridge.local(tuple(cen[k]))[2] - f <= band[1]:
            lip += area[k]
    return {"up_area_m2": round(float(area[sel].sum()) / 1e4, 2), "lip_m2": round(lip / 1e4, 3),
            "floor_role": float(area[sel].sum()) / 1e4 >= floor_min}


def cover():
    import MeasureRidgeIsleSkinCollision as meas  # 그래플·LandmarkTop 사선 정의 재사용(읽기 전용)
    rules = COVER_RULES[VERSION]
    out = ROOT / ("Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Cover_" + VERSION)
    shell_out = ROOT / "Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Collision" / rules["shell_folder"]
    out.mkdir(parents=True, exist_ok=True)
    shell_out.mkdir(parents=True, exist_ok=True)
    ib.OUT = out
    bpy.ops.wm.read_factory_settings(use_empty=True)
    rng = random.Random(rules["seed"])
    env = ib.helper()
    spaces = json.loads(ib.TRIAL.read_text(encoding="utf-8"))["protected_spaces"]
    samples = [(c["id"], env["samples"](c)) for c in spaces]
    blueprint = json.loads(ib.BLUEPRINT.read_text(encoding="utf-8"))
    lines = {**meas.landmark_lines(), **meas.grapple_lines(blueprint)}
    floor, floor_sha = load_floor()
    rocks = cover_rocks()
    assert len(rocks) == 6, [r["id"] for r in rocks]
    pillars, issues = [], []
    by_isle = {}
    for rock in rocks:
        v, t, info = cover_body(rock, rules, rng)
        isle = COVER_ISLE[rock["id"][:4] if rock["id"].startswith("West") else "Valley"]
        by_isle.setdefault(isle, []).append((rock, v, t, info))
    rows, shell_v, shell_t, shell_rows = [], [], [], []
    for isle, items in by_isle.items():
        verts, tris = [], []
        for rock, v, t, info in items:
            # 보호 표본: 몸체 안 표본을 V011 각기둥 안(복원)과 새 점유로 나눈다.
            bvh = ib.bvh_of(v, t)
            lo, hi = v.min(axis=0), v.max(axis=0)
            restore, new = {}, {}
            poly = [p[:2] for p in rock["outline"]]
            min_top = min(p[2] for p in rock["outline"])
            for sid, pts in samples:
                for p in pts:
                    q = np.asarray(p)
                    if (q < lo).any() or (q > hi).any() or not ib.inside(bvh, q):
                        continue
                    x, y, h = ib.spec.ridge.local(tuple(q))
                    if inside_polygon(poly, x, y) and rock["bottom_abs"] <= h <= min_top:
                        restore[sid] = restore.get(sid, 0) + 1
                    else:
                        new[sid] = new.get(sid, 0) + 1
            blocked = {name: sum(1 for p in pts if (np.asarray(p) >= lo).all() and (np.asarray(p) <= hi).all()
                                 and ib.inside(bvh, np.asarray(p))) for name, pts in lines.items()}
            blocked = {k: c for k, c in blocked.items() if c}
            seam = float(v.min())
            if new:
                issues.append("new protected %s %s" % (rock["id"], new))
            if blocked:
                issues.append("lines %s %s" % (rock["id"], blocked))
            if seam < rules["seam_collision_cm"]:
                issues.append("seam %s %.1f" % (rock["id"], seam))
            # Shell: 비율을 낮은 쪽부터 시험해 맞춤·위상을 만족하는 첫 비율.
            chosen = None
            for ratio in rules["shell_ratios"]:
                obj = decimate(rock["id"] + "_Shell", v, t, ratio)
                sv, stt = ib.arrays(obj)
                topo = ib.topology(obj)
                f = fit(v, t, sv, stt, rules["fit_samples"], rules["seed"])
                ok = (f["p95_cm"] <= rules["fit_p95_cm"] and f["max_cm"] <= rules["fit_max_cm"]
                      and topo["boundary_edges"] == 0 and topo["nonmanifold_edges"] == 0)
                if ok:
                    chosen = (ratio, sv, stt, topo, f)
                    break
                bpy.data.objects.remove(obj)
            assert chosen, rock["id"]
            ratio, sv, stt, topo, f = chosen
            up = up_faces(sv, stt, floor, rules["floor_band_cm"], rules["floor_min_m2"])
            if up["lip_m2"] > 0:
                issues.append("walk lip %s %.3f" % (rock["id"], up["lip_m2"]))
            if up["floor_role"]:
                issues.append("up area >= 50 m2 %s" % rock["id"])
            shell_t.append(stt + sum(len(x) for x in shell_v))
            shell_v.append(sv)
            shell_rows.append({"rock": rock["id"], "ratio": ratio, "triangles": int(len(stt)), "fit": f, "topology": topo,
                               **up})
            rows.append({"rock": rock["id"], "isle": isle, "triangles": int(len(t)), "restore_v011_cover": restore,
                         "new_protected": new, "blocked_lines": blocked, "seam_min_cm": round(seam, 1), **info})
            tris.append(t + len(verts))
            verts.extend(v.tolist())
        name = rules["visual_name"].format(isle=isle)
        obj = ib.make(name, np.asarray(verts), np.concatenate(tris))
        exported = ib.export(obj)
        exported.update({"role": "isle_cover", "isle": isle, "collision": "NoCollision",
                         "rocks": [r["rock"] for r in rows if r["isle"] == isle]})
        by_isle[isle] = exported
    visual = list(by_isle.values())
    # Shell 하나로 합쳐 내보낸다(PlaceRidgeIsleSkinCollision.import_shell이 읽는 manifest 형식).
    v = np.concatenate(shell_v)
    t = np.concatenate(shell_t)
    sobj = ib.make(rules["shell_name"], v, t)
    stopo = ib.topology(sobj)
    while sobj.data.uv_layers:
        sobj.data.uv_layers.remove(sobj.data.uv_layers[0])
    bpy.ops.object.select_all(action="DESELECT")
    sobj.select_set(True)
    bpy.context.view_layer.objects.active = sobj
    path = shell_out / (rules["shell_name"] + ".fbx")
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"},
                             apply_scale_options="FBX_SCALE_NONE", global_scale=1, axis_forward="-Z", axis_up="Y",
                             mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False)
    av, at = ib.arrays(sobj)
    bounds = {"min": av.min(axis=0).round(4).tolist(), "max": av.max(axis=0).round(4).tolist()}
    manifest = {"exports": [{"name": rules["shell_name"], "file": str(path), "triangles": int(len(at)), "bounds_cm": bounds,
                             "role": "Shell", "sha256": sha(path)}]}
    (shell_out / "manifest.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    if stopo["boundary_edges"] or stopo["nonmanifold_edges"]:
        issues.append("shell topology %s" % stopo)
    report = {"version": VERSION, "unit": "IsleTop 2b cover", "decision": "GX-042", "rules": rules,
              "visual": visual, "pillars": rows, "shell": {"name": rules["shell_name"],
                                                           "file": path.relative_to(ROOT).as_posix(), "sha256": sha(path),
                                                           "triangles": int(len(at)), "bounds_cm": bounds,
                                                           "topology": stopo, "parts": shell_rows},
              "restore_v011_cover_total": sum(sum(r["restore_v011_cover"].values()) for r in rows),
              "new_protected_total": sum(sum(r["new_protected"].values()) for r in rows),
              "issues": issues, "pass": not issues, "floor_dump_sha256": floor_sha,
              "inputs": {p.relative_to(ROOT).as_posix(): sha(p) for p in
                         (ib.BLUEPRINT, ib.TRIAL, ib.HELPER, ROOT / "Scripts/RidgeOctantSpec.py", Path(meas.__file__),
                          Path(__file__))}}
    (out / "Build.json").write_text(json.dumps(report, ensure_ascii=False, indent=1), encoding="utf-8")
    print("ISLE_TOP_COVER " + json.dumps({"pass": report["pass"], "issues": issues,
                                           "restore": report["restore_v011_cover_total"],
                                           "new": report["new_protected_total"],
                                           "visual_tris": [x["triangles"] for x in visual],
                                           "shell_tris": len(at),
                                           "shell_parts": [(r["rock"], r["ratio"], r["fit"]["p95_cm"], r["fit"]["max_cm"],
                                                            r["up_area_m2"], r["lip_m2"]) for r in shell_rows]}),
          flush=True)


# ---- 2b V003: 각진·갈라진 사암 덩어리(사용자 "더 다듬기" 4방향) ------------------------------------------------------
# V001/V002의 둥근 통 대신 평면으로 깎은 볼록 덩어리를 쓴다. 각기둥마다 수직 균열로 2~3조각, 조각마다 좁은 밑동 위에
# 넓은 윗단(윗단 돌출·아랫단 파임), 모서리 깎기, 밑동 잔돌 무리(NoCollision, Shell 제외)다. 계산은 옥탄트 로컬
# (x, y, H)를 직교 좌표로 보고 하고(7m 안 구면 처짐 약 2.5cm), 정점만 ridge.point로 옮긴다.
# 모든 덩어리는 V011 외곽을 안쪽으로 줄인 변 평면 안에 둔다. 윗면은 V011 가장 낮은 꼭짓점 윗면보다 낮다.
COVER_RULES["V003"] = {
    "style": "faceted", "seed": 42044,
    "head_inset_cm": 8, "neck_inset_cm": 45, "facets": 6, "facet_factor": [0.86, 0.97],
    "head_tilt_deg": [3, 9], "neck_tilt_deg": [-4, 4], "neck_height_cm": [50, 85], "overlap_cm": 6,
    "top_drop_cm": [6, 30], "top_slope": 0.06, "head_bottom_slope": 0.05,
    "split_gap_cm": [12, 18], "split_tilt_deg": 3, "three_piece_min_size": 320,
    "chips": 7, "chip_cut_cm": [8, 26], "neck_chips": 3,
    "rubble": [4, 7], "rubble_size_cm": [25, 70], "rubble_height": [0.35, 0.6], "rubble_ring_cm": [15, 120],
    "rubble_sink_cm": 6, "rubble_pod_clear_cm": 900,
    "fit_p95_cm": 25.0, "fit_max_cm": 75.0, "floor_band_cm": [-2.0, 40.0], "seam_collision_cm": 2000.0,
    "floor_min_m2": 50.0,
    "visual_name": "SM_Ridge_IsleCover_{isle}_V003", "shell_name": "SM_Ridge_IsleCover_V003_Shell",
    "shell_folder": "Shell_Cover_V003",
}
# V004: V003과 같은 규칙·시드. V003은 ib.make의 법선 재계산이 볼록 조각 일부(골짜기 섬 31성분 중 3개)를 안쪽으로 뒤집어
# 윗면이 빠져 보였다. 조각마다 삼각형을 중심 바깥으로 직접 감고 재계산 없이 만든다(orient_out·make_oriented).
COVER_RULES["V004"] = dict(COVER_RULES["V003"], **{
    "visual_name": "SM_Ridge_IsleCover_{isle}_V004", "shell_name": "SM_Ridge_IsleCover_V004_Shell",
    "shell_folder": "Shell_Cover_V004",
})
# V005(사용자 PIE Q06, GX-043): V004와 같은 규칙·시드(시각 형상 동일). 밑동 잔돌 중 섬 윗면 위 8cm 이상 솟은 것을
# Shell(Blocker)에 넣는다. 잔돌은 바닥 위 장애물이라 걷는 면 턱 검사에서 따로 센다(엄폐 조각만 턱 0 요구).
COVER_RULES["V005"] = dict(COVER_RULES["V004"], **{
    "visual_name": "SM_Ridge_IsleCover_{isle}_V005", "shell_name": "SM_Ridge_IsleCover_V005_Shell",
    "shell_folder": "Shell_Cover_V005", "rubble_collision": True, "rubble_collision_min_height_cm": 8.0,
})


def hull_box(lo, hi):
    (x0, y0, z0), (x1, y1, z1) = lo, hi
    P = [np.array(p, dtype=np.float64) for p in
         [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]]
    idx = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    return [[P[i] for i in f] for f in idx]


def hull_clip(faces, n, d, eps=1e-6):
    """볼록 다면체(면 목록)를 반공간 n·p ≤ d로 자르고 자른 면을 덮는다."""
    n = np.asarray(n, dtype=np.float64)
    out_faces, cap = [], []
    for f in faces:
        out, m = [], len(f)
        for i in range(m):
            a, b = f[i], f[(i + 1) % m]
            da, db = float(n @ a - d), float(n @ b - d)
            if da <= eps:
                out.append(a)
                if da >= -eps:
                    cap.append(a)
            if (da < -eps and db > eps) or (da > eps and db < -eps):
                p = a + (b - a) * (da / (da - db))
                out.append(p)
                cap.append(p)
        if len(out) >= 3:
            out_faces.append(out)
    uniq = {}
    for p in cap:
        uniq.setdefault(tuple(np.round(p, 4)), p)
    cap = list(uniq.values())
    if len(cap) >= 3:
        c = np.mean(cap, axis=0)
        u = np.cross(n, [0, 0, 1.0]) if abs(n[2]) < 0.9 else np.cross(n, [1.0, 0, 0])
        u /= np.linalg.norm(u)
        w = np.cross(n, u)
        cap.sort(key=lambda p: math.atan2(float((p - c) @ w), float((p - c) @ u)))
        out_faces.append(cap)
    return out_faces


def hull_points(faces):
    return np.array([p for f in faces for p in f])


def hull_mesh(faces):
    """면 목록 → (로컬 정점, 삼각형). 정점을 0.001cm로 용접하고 볼록 면은 부채꼴로 나눈다."""
    key, verts, tris = {}, [], []
    for f in faces:
        ids = []
        for p in f:
            k = tuple(np.round(p, 3))
            if k not in key:
                key[k] = len(verts)
                verts.append(p)
            if not ids or ids[-1] != key[k]:
                ids.append(key[k])
        if len(ids) > 1 and ids[0] == ids[-1]:
            ids.pop()
        if len(set(ids)) < 3:
            continue
        for i in range(1, len(ids) - 1):
            tris.append((ids[0], ids[i], ids[i + 1]))
    return np.asarray(verts), np.asarray(tris)


def to_world(local_v):
    return np.asarray([ib.world(*p) for p in local_v])


def orient_out(v, t):
    """볼록 조각: 삼각형 법선이 조각 중심에서 바깥을 보게 감는 방향을 맞춘다(월드 UE cm)."""
    v, t = np.asarray(v), np.asarray(t).copy()
    c = v.mean(axis=0)
    a, b, d = v[t[:, 0]], v[t[:, 1]], v[t[:, 2]]
    flip = (np.cross(b - a, d - a) * ((a + b + d) / 3 - c)).sum(1) < 0
    t[flip] = t[flip][:, ::-1]
    return t


def make_oriented(name, v, t):
    """ib.make와 같은 축 규약이지만 법선 재계산을 하지 않는다(삼각형 감는 방향을 그대로 쓴다)."""
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(ib.bl(v).tolist(), [], np.asarray(t)[:, ::-1].tolist())
    mesh.validate()
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def poly_edges(poly):
    """외곽 다각형의 변마다 바깥 수평 법선과 거리(n·p ≤ d가 안쪽)."""
    area = sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(poly, poly[1:] + poly[:1]))
    out = []
    for a, b in zip(poly, poly[1:] + poly[:1]):
        e = np.array([b[0] - a[0], b[1] - a[1]])
        n = np.array([e[1], -e[0]]) if area > 0 else np.array([-e[1], e[0]])
        n /= np.linalg.norm(n)
        out.append((n, float(n @ np.array(a[:2]))))
    return out


def faceted_pillar(rock, rules, rng, pods):
    poly = [tuple(p[:2]) for p in rock["outline"]]
    cx, cy = np.mean([p[0] for p in poly]), np.mean([p[1] for p in poly])
    c2 = np.array([cx, cy])
    tops = [p[2] for p in rock["outline"]]
    bottom = rock["bottom_abs"]
    floor_h = bottom + 100
    span = max(math.hypot(x - cx, y - cy) for x, y in poly)
    edges = poly_edges(poly)
    lo = (cx - span - 50, cy - span - 50, bottom - 1)
    hi = (cx + span + 50, cy + span + 50, max(tops) + 50)

    def support(nxy):
        return max(float(nxy @ (np.array(p) - c2)) for p in poly)

    def facet_planes(tilt_range, center_h, factor_range):
        rows = []
        base = rng.uniform(0, math.tau)
        for k in range(rules["facets"]):
            a = base + math.tau * k / rules["facets"] + rng.uniform(-0.3, 0.3)
            nxy = np.array([math.cos(a), math.sin(a)])
            t = math.radians(rng.uniform(*tilt_range))
            n = np.array([nxy[0] * math.cos(t), nxy[1] * math.cos(t), -math.sin(t)])
            d = float(n @ np.array([cx, cy, center_h])) + support(nxy) * math.cos(t) * rng.uniform(*factor_range)
            rows.append((n, d))
        return rows

    def chips(faces, count, cut_range, z_sign=None):
        for _ in range(count):
            a = rng.uniform(0, math.tau)
            z = rng.uniform(0.45, 0.9) * (z_sign if z_sign else rng.choice([-1, 1]))
            n = np.array([math.cos(a), math.sin(a), z])
            n /= np.linalg.norm(n)
            pts = hull_points(faces)
            if not len(pts):
                return faces
            d = float((pts @ n).max()) - rng.uniform(*cut_range)
            faces = hull_clip(faces, n, d)
        return faces

    # 균열: 2~3조각. 조각마다 (n, d) 반공간 목록.
    size = max(abs(p[0] - cx) + abs(p[1] - cy) for p in poly) / 1.4
    pieces = 3 if size >= rules["three_piece_min_size"] else 2
    a = rng.uniform(0, math.pi)
    gap = rng.uniform(*rules["split_gap_cm"])
    nxy = np.array([math.cos(a), math.sin(a)])
    ext = support(nxy) + support(-nxy)
    cuts = [rng.uniform(-0.12, 0.12) * ext] if pieces == 2 else [-ext / 6 + rng.uniform(-30, 30), ext / 6 + rng.uniform(-30, 30)]
    split_planes = []
    for off in cuts:
        b = a + math.radians(rng.uniform(-12, 12))
        t = math.radians(rng.uniform(-rules["split_tilt_deg"], rules["split_tilt_deg"]))
        n = np.array([math.cos(b) * math.cos(t), math.sin(b) * math.cos(t), math.sin(t)])
        d = float(n @ np.array([cx + nxy[0] * off, cy + nxy[1] * off, floor_h + 150]))
        split_planes.append((n, d))
    regions = []
    for i in range(pieces):
        reg = []
        if i > 0:
            n, d = split_planes[i - 1]
            reg.append((-n, -(d + gap / 2)))
        if i < pieces - 1:
            n, d = split_planes[i]
            reg.append((n, d - gap / 2))
        regions.append(reg)

    mains, notes = [], []
    head_facets = facet_planes(rules["head_tilt_deg"], (floor_h + min(tops)) / 2, rules["facet_factor"])
    neck_facets = facet_planes(rules["neck_tilt_deg"], floor_h, [f * 0.8 for f in rules["facet_factor"]])
    for i, reg in enumerate(regions):
        neck_h = floor_h + rng.uniform(*rules["neck_height_cm"])
        top_h = min(tops) - rng.uniform(*rules["top_drop_cm"])
        s = rules["top_slope"]
        tn = np.array([rng.uniform(-s, s), rng.uniform(-s, s), 1.0])
        tn /= np.linalg.norm(tn)
        bs = rules["head_bottom_slope"]
        bn = np.array([rng.uniform(-bs, bs), rng.uniform(-bs, bs), -1.0])
        bn /= np.linalg.norm(bn)
        # 윗단
        head = hull_box(lo, hi)
        for n, d in [(np.array([*en, 0.0]), ed - rules["head_inset_cm"]) for en, ed in edges] + head_facets + reg:
            head = hull_clip(head, n, d)
        head = hull_clip(head, tn, float(tn @ np.array([cx, cy, top_h])))
        head = hull_clip(head, bn, float(bn @ np.array([cx, cy, neck_h - rules["overlap_cm"]])))
        head = chips(head, rules["chips"], rules["chip_cut_cm"])
        # 밑동(좁다): 바닥은 섬 윗면 1m 아래
        neck = hull_box(lo, hi)
        for n, d in [(np.array([*en, 0.0]), ed - rules["neck_inset_cm"]) for en, ed in edges] + neck_facets + reg:
            neck = hull_clip(neck, n, d)
        neck = hull_clip(neck, np.array([0, 0, 1.0]), neck_h + rules["overlap_cm"])
        neck = hull_clip(neck, np.array([0, 0, -1.0]), -bottom)
        neck = chips(neck, rules["neck_chips"], [c * 0.6 for c in rules["chip_cut_cm"]], z_sign=1)
        for part, faces in (("head", head), ("neck", neck)):
            lv, lt = hull_mesh(faces)
            if len(lt) < 4:
                notes.append("empty %s piece %d" % (part, i))
                continue
            wv = to_world(lv)
            mains.append((wv, orient_out(wv, lt), {"piece": i, "part": part,
                                             "h_range": [round(float(lv[:, 2].min()), 1), round(float(lv[:, 2].max()), 1)]}))
    # 밑동 잔돌 무리(NoCollision, Shell 제외). 섬 Pod 중심 9m 안은 비운다.
    rubble = []
    for _ in range(rng.randint(*rules["rubble"])):
        for attempt in range(10):
            a = rng.uniform(0, math.tau)
            dxy = np.array([math.cos(a), math.sin(a)])
            r = support(dxy) * 0.9 + rng.uniform(*rules["rubble_ring_cm"])
            px, py = cx + dxy[0] * r, cy + dxy[1] * r
            if min(math.hypot(px - qx, py - qy) for qx, qy in pods) >= rules["rubble_pod_clear_cm"]:
                break
        else:
            continue
        sz = rng.uniform(*rules["rubble_size_cm"])
        hz = sz * rng.uniform(*rules["rubble_height"])
        faces = hull_box((-sz, -sz * 0.8, 0.0), (sz, sz * 0.8, hz))
        faces = chips(faces, 5, [sz * 0.15, sz * 0.45])
        yaw = rng.uniform(0, math.tau)
        R = np.array([[math.cos(yaw), -math.sin(yaw), 0], [math.sin(yaw), math.cos(yaw), 0], [0, 0, 1.0]])
        lv, lt = hull_mesh(faces)
        if len(lt) < 4:
            continue
        lv = lv @ R.T + np.array([px, py, floor_h - rules["rubble_sink_cm"]])
        wv = to_world(lv)
        rubble.append((wv, orient_out(wv, lt), {"center_local": [round(px, 1), round(py, 1)], "size_cm": round(sz, 1),
                                          "height_cm": round(hz - rules["rubble_sink_cm"], 1)}))
    info = {"center_local": [round(cx, 1), round(cy, 1)], "pieces": pieces, "bottom_h": bottom,
            "v011_top_h": tops, "notes": notes, "rubble": [r[2] for r in rubble], "parts": [m[2] for m in mains]}
    return mains, rubble, info


def merge(parts):
    verts, tris = [], []
    for v, t, *_ in parts:
        tris.append(np.asarray(t) + len(verts))
        verts.extend(np.asarray(v).tolist())
    return np.asarray(verts), np.concatenate(tris)


def cover_faceted():
    import MeasureRidgeIsleSkinCollision as meas
    rules = COVER_RULES[VERSION]
    out = ROOT / ("Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Cover_" + VERSION)
    shell_out = ROOT / "Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Collision" / rules["shell_folder"]
    out.mkdir(parents=True, exist_ok=True)
    shell_out.mkdir(parents=True, exist_ok=True)
    ib.OUT = out
    bpy.ops.wm.read_factory_settings(use_empty=True)
    rng = random.Random(rules["seed"])
    env = ib.helper()
    spaces = json.loads(ib.TRIAL.read_text(encoding="utf-8"))["protected_spaces"]
    samples = [(c["id"], env["samples"](c)) for c in spaces]
    pods = [tuple(c["points_xyh_cm"][0][:2]) for c in spaces if c["id"].endswith("SkyIslePod")]
    blueprint = json.loads(ib.BLUEPRINT.read_text(encoding="utf-8"))
    lines = {**meas.landmark_lines(), **meas.grapple_lines(blueprint)}
    floor, floor_sha = load_floor()
    rocks = cover_rocks()
    assert len(rocks) == 6 and len(pods) == 2
    issues, rows, shell_parts, by_isle = [], [], [], {}
    for rock in rocks:
        isle = COVER_ISLE["West" if rock["id"].startswith("West") else "Valley"]
        mains, rubble, info = faceted_pillar(rock, rules, rng, pods)
        poly = [p[:2] for p in rock["outline"]]
        min_top = min(p[2] for p in rock["outline"])
        bvhs = [(ib.bvh_of(v, t), v.min(axis=0), v.max(axis=0)) for v, t, _ in mains + rubble]

        def inside_any(q):
            return any((q >= lo).all() and (q <= hi).all() and ib.inside(b, q) for b, lo, hi in bvhs)

        restore, new = {}, {}
        for sid, pts in samples:
            for p in pts:
                q = np.asarray(p)
                if not inside_any(q):
                    continue
                x, y, h = ib.spec.ridge.local(tuple(q))
                key = restore if inside_polygon(poly, x, y) and rock["bottom_abs"] <= h <= min_top else new
                key[sid] = key.get(sid, 0) + 1
        blocked = {}
        for name, pts in lines.items():
            n = sum(1 for p in pts if inside_any(np.asarray(p)))
            if n:
                blocked[name] = n
        mv, mt = merge(mains)
        seam = float(mv.min())
        topo_parts = []
        for v, t, meta in mains:
            obj = make_oriented("%s_%d_%s" % (rock["id"], meta["piece"], meta["part"]), v, t)
            topo_parts.append(ib.topology(obj))
            bpy.data.objects.remove(obj)
        up = up_faces(mv, mt, floor, rules["floor_band_cm"], rules["floor_min_m2"])
        if new:
            issues.append("new protected %s %s" % (rock["id"], new))
        if blocked:
            issues.append("lines %s %s" % (rock["id"], blocked))
        if seam < rules["seam_collision_cm"]:
            issues.append("seam %s %.1f" % (rock["id"], seam))
        if up["lip_m2"] > 0:
            issues.append("walk lip %s %.3f" % (rock["id"], up["lip_m2"]))
        if up["floor_role"]:
            issues.append("up area >= 50 m2 %s" % rock["id"])
        if any(tp["boundary_edges"] or tp["nonmanifold_edges"] for tp in topo_parts):
            issues.append("topology %s %s" % (rock["id"], topo_parts))
        if info["notes"]:
            issues.append("notes %s %s" % (rock["id"], info["notes"]))
        shell_parts.append({"rock": rock["id"], "hulls": len(mains), "triangles": int(len(mt)),
                            "fit": {"p95_cm": 0.0, "max_cm": 0.0, "note": "shell = visual main hulls (rubble excluded)"},
                            **up})
        rows.append({"rock": rock["id"], "isle": isle, "hulls": len(mains), "rubble": len(rubble),
                     "triangles_main": int(len(mt)), "restore_v011_cover": restore, "new_protected": new,
                     "blocked_lines": blocked, "seam_min_cm": round(seam, 1), "hull_topology": topo_parts, **info})
        by_isle.setdefault(isle, {"main": [], "rubble": []})
        by_isle[isle]["main"] += mains
        by_isle[isle]["rubble"] += rubble
    visual, shell_all, rubble_shell, rubble_skipped = [], [], [], []
    for isle, parts in by_isle.items():
        v, t = merge(parts["main"] + parts["rubble"])
        obj = make_oriented(rules["visual_name"].format(isle=isle), v, t)
        exported = ib.export(obj)
        exported.update({"role": "isle_cover", "isle": isle, "collision": "NoCollision",
                         "rocks": [r["rock"] for r in rows if r["isle"] == isle]})
        visual.append(exported)
        shell_all += parts["main"]
        if rules.get("rubble_collision"):
            for rv, rt, rinfo in parts["rubble"]:
                if rinfo["height_cm"] >= rules["rubble_collision_min_height_cm"]:
                    shell_all.append((rv, rt, rinfo))
                    rubble_shell.append(dict(rinfo, isle=isle))
                else:
                    rubble_skipped.append(dict(rinfo, isle=isle))
    v, t = merge(shell_all)
    sobj = make_oriented(rules["shell_name"], v, t)
    stopo = ib.topology(sobj)
    while sobj.data.uv_layers:
        sobj.data.uv_layers.remove(sobj.data.uv_layers[0])
    bpy.ops.object.select_all(action="DESELECT")
    sobj.select_set(True)
    bpy.context.view_layer.objects.active = sobj
    path = shell_out / (rules["shell_name"] + ".fbx")
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"},
                             apply_scale_options="FBX_SCALE_NONE", global_scale=1, axis_forward="-Z", axis_up="Y",
                             mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False)
    av, at = ib.arrays(sobj)
    bounds = {"min": av.min(axis=0).round(4).tolist(), "max": av.max(axis=0).round(4).tolist()}
    manifest = {"exports": [{"name": rules["shell_name"], "file": str(path), "triangles": int(len(at)), "bounds_cm": bounds,
                             "role": "Shell", "sha256": sha(path)}]}
    (shell_out / "manifest.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    report = {"version": VERSION, "unit": "IsleTop 2b cover", "decision": "GX-042", "rules": rules, "visual": visual,
              "pillars": rows, "shell": {"name": rules["shell_name"], "file": path.relative_to(ROOT).as_posix(),
                                         "sha256": sha(path), "triangles": int(len(at)), "bounds_cm": bounds,
                                         "topology_merged": stopo, "parts": shell_parts,
                                         "note": "overlapping closed hulls (neck/head) merged; per-hull topology checked",
                                         "rubble_in_shell": rubble_shell, "rubble_skipped_low": rubble_skipped},
              "restore_v011_cover_total": sum(sum(r["restore_v011_cover"].values()) for r in rows),
              "new_protected_total": sum(sum(r["new_protected"].values()) for r in rows),
              "issues": issues, "pass": not issues, "floor_dump_sha256": floor_sha,
              "inputs": {p.relative_to(ROOT).as_posix(): sha(p) for p in
                         (ib.BLUEPRINT, ib.TRIAL, ib.HELPER, ROOT / "Scripts/RidgeOctantSpec.py", Path(meas.__file__),
                          Path(__file__))}}
    (out / "Build.json").write_text(json.dumps(report, ensure_ascii=False, indent=1), encoding="utf-8")
    print("ISLE_TOP_COVER " + json.dumps({"pass": report["pass"], "issues": issues,
                                           "restore": report["restore_v011_cover_total"],
                                           "new": report["new_protected_total"],
                                           "visual_tris": [x["triangles"] for x in visual], "shell_tris": len(at),
                                           "pillars": [(r["rock"], r["pieces"], r["hulls"], r["rubble"], r["triangles_main"])
                                                       for r in rows],
                                           "up": [(s["rock"], s["up_area_m2"], s["lip_m2"]) for s in shell_parts]}),
          flush=True)


# ---- 2c: 섬 윗면 잔돌·식생(GX-042) ----------------------------------------------------------------------------------
# NoCollision 작은 ED 잔돌(<100cm)·Clover·Fern을 섬당 약 150~250개 배치한다.
# Pod 중심 9m·착지점 4m·그래플 사선·엄폐 각기둥 및 GX-043 밑동 잔돌(Cover_V005)을 비우고,
# PreservedRocks_Floor 실제 높이에 투영한 뒤 보호 51공간·16,552표본 전수 OBB 검사를 통과해야 한다.
SCATTER_COVER_BUILD = ROOT / "Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Cover_V005/Build.json"
FERN_MATERIAL = "/Game/Graphics/Environment/ElectricDreams/RidgeFernWind_V001/MI_Ridge_Fern"

# UE StaticMeshTools.get_bounds 실측값(cm).
SCATTER_ASSETS = {
    "stone": [
        {"mesh": "/Game/Megascans/3D_Assets/SmallStonesPack/SM_SmallStonesPack_01",
         "min": [-6.55, -4.62, -4.15], "max": [6.55, 4.63, 4.15], "weight": 1.0, "center_pivot": True},
        {"mesh": "/Game/Megascans/3D_Assets/SmallStonesPack/SM_SmallStonesPack_02",
         "min": [-5.54, -3.38, -3.85], "max": [5.55, 3.38, 3.85], "weight": 0.9, "center_pivot": True},
        {"mesh": "/Game/Megascans/3D_Assets/SmallStonesPack/SM_SmallStonesPack_07",
         "min": [-9.04, -5.01, -5.93], "max": [9.04, 5.01, 5.94], "weight": 1.1, "center_pivot": True},
        {"mesh": "/Game/Megascans/3D_Assets/SmallStonesPack/SM_SmallStonesPack_14",
         "min": [-8.99, -4.97, -5.93], "max": [8.99, 4.97, 5.90], "weight": 1.0, "center_pivot": True},
    ],
    "clover": [
        {"mesh": "/Game/Megascans/3D_Plants/CloverVarieties/SM_CloverVarieties_01",
         "min": [-64.22, -60.89, -0.62], "max": [71.49, 67.02, 13.96], "weight": 1.0, "scale": [0.75, 1.05]},
        {"mesh": "/Game/Megascans/3D_Plants/CloverVarieties/SM_CloverVarieties_02",
         "min": [-33.15, -17.88, -0.29], "max": [33.91, 33.82, 12.14], "weight": 1.3, "scale": [0.85, 1.20]},
        {"mesh": "/Game/Megascans/3D_Plants/CloverVarieties/SM_CloverVarieties_03",
         "min": [-51.28, -36.32, -0.37], "max": [91.33, 54.75, 13.77], "weight": 1.1, "scale": [0.75, 1.05]},
    ],
    "fern": [
        {"mesh": "/Game/Megascans/3D_Plants/Fern/SM_Fern_01", "material": FERN_MATERIAL,
         "min": [-67.67, -91.16, -3.90], "max": [88.58, 96.84, 99.31], "weight": 0.6},
        {"mesh": "/Game/Megascans/3D_Plants/Fern/SM_Fern_02", "material": FERN_MATERIAL,
         "min": [-85.08, -102.39, -3.66], "max": [81.57, 73.14, 91.33], "weight": 0.7},
        {"mesh": "/Game/Megascans/3D_Plants/Fern/SM_Fern_03", "material": FERN_MATERIAL,
         "min": [-76.45, -48.68, -2.05], "max": [72.17, 88.97, 61.05], "weight": 1.4},
        {"mesh": "/Game/Megascans/3D_Plants/Fern/SM_Fern_04", "material": FERN_MATERIAL,
         "min": [-84.32, -82.70, -2.18], "max": [94.38, 81.95, 70.75], "weight": 1.3},
        {"mesh": "/Game/Megascans/3D_Plants/Fern/SM_Fern_05", "material": FERN_MATERIAL,
         "min": [-97.20, -101.34, -2.24], "max": [104.42, 86.08, 89.06], "weight": 0.6},
        {"mesh": "/Game/Megascans/3D_Plants/Fern/SM_Fern_06", "material": FERN_MATERIAL,
         "min": [-130.20, -94.05, -5.87], "max": [27.22, 64.94, 60.25], "weight": 1.4},
    ],
}

SCATTER_RULES = {
    "V017": {
        "seed": 42047,
        "pod_clear_cm": 900.0,
        "landing_clear_cm": 400.0,
        "line_clear_cm": 150.0,
        "seam_visual_cm": 500.0,
        "rim_margin_cm": {"stone": 160.0, "clover": 160.0, "fern": 220.0},
        "cover_clear_cm": {"stone": 55.0, "clover": 40.0, "fern": 45.0},
        "rubble_clear_cm": {"stone": 45.0, "clover": 35.0, "fern": 40.0},
        "stone_scale": [1.25, 2.15],
        "stone_embed_frac": 0.15,   # 중심 피벗 메시: center = floor_h + 0.15 * max_z * s (약 42% 묻힘, 58% 돌출)
        "stone_tilt_deg": 5.0,
        "clover_sink_cm": 0.8,
        "fern_scale": [0.55, 0.80],
        "fern_sink_cm": 2.0,
        "fern_wind_pad_xy_cm": 30.0,
        "fern_wind_pad_z_cm": 15.0,
        "max_stone_dim_cm": 99.0,   # GX-042: 100cm 미만 작은 잔돌
        "targets": {
            "cover_fern": 10, "rim_fern": 14,
            "cover_clover": 22, "rim_clover": 32, "mid_clover": 20,
            "cover_stone": 38, "rim_stone": 42, "mid_stone": 28,
        },
        "rim_pockets": 7,
        "count_range": [150, 250],
    },
    # V018: 사용자 요청 "더 풍성하게". GX-042 범위(150~250개) 상단(약 244개)을 쓰고,
    # Clover 큰 변형(01/03) 비중·스케일 확대 + Fern 군집(44개, 스케일 0.72~0.98) + 2단 크기 잔돌(최대 ~50cm·돌출 ≤17cm)로
    # 16m 부감·WestTopEye에서도 식생·잔돌 리듬이 분명히 읽히되 Pod 9m·착지·사선·보호 51공간·보행면 인지를 유지한다.
    "V018": {
        "seed": 42048,
        "pod_clear_cm": 900.0,
        "landing_clear_cm": 400.0,
        "line_clear_cm": 150.0,
        "seam_visual_cm": 500.0,
        "rim_margin_cm": {"stone": 145.0, "clover": 145.0, "fern": 200.0},
        "cover_clear_cm": {"stone": 50.0, "clover": 35.0, "fern": 40.0},
        "rubble_clear_cm": {"stone": 42.0, "clover": 32.0, "fern": 38.0},
        "stone_scale": [1.35, 2.25],
        "stone_anchor_prob": 0.35,
        "stone_anchor_scale": [2.0, 2.8],
        "stone_embed_frac": 0.12,
        "stone_tilt_deg": 5.0,
        "clover_sink_cm": 0.8,
        "clover_scale_mult": 1.22,
        "clover_weights": [1.6, 0.7, 1.4],
        "clover_overlap_factor": 0.58,
        "fern_scale": [0.72, 0.98],
        "fern_sink_cm": 2.2,
        "fern_wind_pad_xy_cm": 30.0,
        "fern_wind_pad_z_cm": 15.0,
        "fern_cluster": True,
        "max_stone_dim_cm": 60.0,   # GX-042 <100cm, 걷는 면 높이를 흐리지 않는 작은 잔돌 유지
        "targets": {
            "cover_fern": 19, "rim_fern": 25,
            "cover_clover": 30, "rim_clover": 40, "mid_clover": 28,
            "cover_stone": 36, "rim_stone": 40, "mid_stone": 26,
        },
        "rim_pockets": 8,
        "count_range": [150, 250],
    },
}


def poly_signed_dist(poly, x, y):
    """다각형 안쪽이면 양수(변까지 최소 거리), 바깥이면 음수."""
    p = np.array([x, y], dtype=np.float64)
    best = 1e18
    for a, b in zip(poly, poly[1:] + poly[:1]):
        pa, pb = np.array(a[:2], dtype=np.float64), np.array(b[:2], dtype=np.float64)
        ab = pb - pa
        t = max(0.0, min(1.0, float(np.dot(p - pa, ab) / max(float(np.dot(ab, ab)), 1e-12))))
        d = float(np.linalg.norm(p - (pa + ab * t)))
        if d < best:
            best = d
    return best if inside_polygon(poly, x, y) else -best


def local_frame_axes(x, y, h):
    """UE BuildRidgeZ1AssemblyTest.frame_rotation(x, y, 0)과 같은 직교 기저 (ex, ey, ez) 월드 벡터."""
    p = np.asarray(ib.spec.ridge.point(x, y, h), dtype=np.float64)
    ez = -p / np.linalg.norm(p)
    p2 = np.asarray(ib.spec.ridge.point(x + 10.0, y, h), dtype=np.float64)
    d = p2 - p
    ex = d - ez * float(np.dot(d, ez))
    ex /= np.linalg.norm(ex)
    # UE 왼손 좌표계 MakeRotFromXZ: Y = cross(Z, X)
    ey = np.cross(ez, ex)
    ey /= np.linalg.norm(ey)
    return p, ex, ey, ez


def rot_matrix_local(pitch_deg, yaw_deg, roll_deg):
    """UE Rotator(pitch, yaw, roll)을 로컬 기저에 적용하는 3x3 회전 행렬 (열 벡터 기준)."""
    cp, sp = math.cos(math.radians(pitch_deg)), math.sin(math.radians(pitch_deg))
    cy, sy = math.cos(math.radians(yaw_deg)), math.sin(math.radians(yaw_deg))
    cr, sr = math.cos(math.radians(roll_deg)), math.sin(math.radians(roll_deg))
    # UE FRotationMatrix (왼손계: +Yaw = X->Y, +Pitch = X->Z, +Roll = Y->-Z)
    ax = np.array([cp * cy, cp * sy, sp], dtype=np.float64)
    ay = np.array([sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp], dtype=np.float64)
    az = np.array([-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp], dtype=np.float64)
    return ax, ay, az


def obb_from_pose(x, y, h, rot_deg, scale, asset, pad_xy=0.0, pad_z=0.0):
    """인스턴스의 월드 OBB (origin, wx, wy, wz, bmin, bmax, world_aabb_lo, world_aabb_hi)."""
    p, ex, ey, ez = local_frame_axes(x, y, h)
    lx, ly, lz = rot_matrix_local(*rot_deg)
    wx = lx[0] * ex + lx[1] * ey + lx[2] * ez
    wy = ly[0] * ex + ly[1] * ey + ly[2] * ez
    wz = lz[0] * ex + lz[1] * ey + lz[2] * ez
    bmin = np.array([asset["min"][0] * scale - pad_xy, asset["min"][1] * scale - pad_xy, asset["min"][2] * scale], dtype=np.float64)
    bmax = np.array([asset["max"][0] * scale + pad_xy, asset["max"][1] * scale + pad_xy, asset["max"][2] * scale + pad_z], dtype=np.float64)
    corners = np.array([p + wx * bx + wy * by + wz * bz
                        for bx in (bmin[0], bmax[0]) for by in (bmin[1], bmax[1]) for bz in (bmin[2], bmax[2])])
    return p, wx, wy, wz, bmin, bmax, corners.min(axis=0), corners.max(axis=0)


def obb_dist_to_points(obb, pts):
    """점 배열(N, 3)에서 OBB까지의 최소 유클리드 거리(안이면 0)."""
    p, wx, wy, wz, bmin, bmax, _, _ = obb
    rel = pts - p
    u = rel @ wx
    v = rel @ wy
    w = rel @ wz
    dx = np.maximum(0.0, np.maximum(bmin[0] - u, u - bmax[0]))
    dy = np.maximum(0.0, np.maximum(bmin[1] - v, v - bmax[1]))
    dz = np.maximum(0.0, np.maximum(bmin[2] - w, w - bmax[2]))
    return np.sqrt(dx * dx + dy * dy + dz * dz)


def scatter():
    import MeasureRidgeIsleSkinCollision as meas
    rules = SCATTER_RULES[VERSION]
    out = ROOT / ("Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Scatter_" + VERSION)
    out.mkdir(parents=True, exist_ok=True)
    rng = random.Random(rules["seed"])

    blueprint = json.loads(ib.BLUEPRINT.read_text(encoding="utf-8"))
    islands = {i["id"]: i for i in blueprint["preserve"]["islands"]}
    pods = {p["id"]: tuple(p["local"][:2]) for p in blueprint["pods"] if p["id"] in islands}
    landings = {}
    for a in blueprint["anchors"]:
        lx, ly = a["landing"]
        for rid, (px, py) in pods.items():
            if math.hypot(lx - px, ly - py) < 3500:
                landings.setdefault(rid, []).append((a["id"], (lx, ly)))
    covers_all = cover_rocks()
    cover_build = json.loads(SCATTER_COVER_BUILD.read_text(encoding="utf-8"))
    rubble_by_isle = {}
    for p in cover_build["pillars"]:
        rubble_by_isle.setdefault(p["isle"], []).extend(p["rubble"])

    env = ib.helper()
    spaces = json.loads(ib.TRIAL.read_text(encoding="utf-8"))["protected_spaces"]
    samples = [(c["id"], np.asarray(env["samples"](c), dtype=np.float64)) for c in spaces]
    lines = {k: np.asarray(v, dtype=np.float64) for k, v in {**meas.landmark_lines(), **meas.grapple_lines(blueprint)}.items()}
    all_line_pts = np.concatenate(list(lines.values()), axis=0)
    floor, floor_sha = load_floor()

    def pick_asset(kind):
        pool = SCATTER_ASSETS[kind]
        if kind == "clover" and "clover_weights" in rules:
            weights = rules["clover_weights"]
        else:
            weights = [a["weight"] for a in pool]
        return rng.choices(pool, weights=weights, k=1)[0]

    def asset_radius(asset, scale):
        rx = max(abs(asset["min"][0]), abs(asset["max"][0])) * scale
        ry = max(abs(asset["min"][1]), abs(asset["max"][1])) * scale
        return math.hypot(rx, ry)

    placements_by_isle = {}
    summary = {}
    issues = []

    for rid in ("ValleySkyIsle", "WestSkyIsle"):
        it = islands[rid]
        outline = [tuple(p[:2]) for p in it["outline"]]
        top_h = it["island_top"]
        cx, cy = pods[rid]
        isle_landings = [xy for _, xy in landings.get(rid, [])]
        isle_covers = [([tuple(p[:2]) for p in r["outline"]],
                        (sum(p[0] for p in r["outline"]) / len(r["outline"]),
                         sum(p[1] for p in r["outline"]) / len(r["outline"])))
                       for r in covers_all if COVER_ISLE["West" if r["id"].startswith("West") else "Valley"] == rid]
        isle_rubble = rubble_by_isle.get(rid, [])
        # 이 섬 주변 40m 안의 보호 표본만 빠른 필터로 추려 둔다(최종 검증은 전체 51공간·16,552표본으로 다시 실행).
        isle_center_w = np.asarray(ib.spec.ridge.point(cx, cy, top_h), dtype=np.float64)
        near_samples = []
        for sid, pts in samples:
            m = np.linalg.norm(pts - isle_center_w, axis=1) < 4200.0
            if m.any():
                near_samples.append((sid, pts[m]))
        near_lines = all_line_pts[np.linalg.norm(all_line_pts - isle_center_w, axis=1) < 4500.0]

        # 외곽 림 포켓 중심(7개, Pod·착지점·엄폐 밖 자연 덩어리 자리)
        rim_pockets = []
        base_ang = rng.uniform(0, math.tau)
        for k in range(rules["rim_pockets"]):
            for _ in range(30):
                th = base_ang + math.tau * k / rules["rim_pockets"] + rng.uniform(-0.28, 0.28)
                r_edge = ib.ray_radius(outline, (cx, cy), th)
                rad = r_edge - rng.uniform(280.0, 560.0)
                px, py = cx + rad * math.cos(th), cy + rad * math.sin(th)
                if math.hypot(px - cx, py - cy) < rules["pod_clear_cm"] + 250.0:
                    continue
                if any(math.hypot(px - lx, py - ly) < rules["landing_clear_cm"] + 250.0 for lx, ly in isle_landings):
                    continue
                if any(poly_signed_dist(cp, px, py) > -180.0 for cp, _ in isle_covers):
                    continue
                rim_pockets.append((px, py))
                break

        placed = []  # {"kind", "zone", "mesh", "material", "local_xyh_cm", "rotation_deg", "scale", "radius_cm", ...}
        rejects = {}

        def reject(reason):
            rejects[reason] = rejects.get(reason, 0) + 1
            return False

        def try_place(kind, zone, x, y, asset=None, scale=None):
            if asset is None:
                if kind == "stone" and rng.random() < rules.get("stone_anchor_prob", 0.0):
                    asset = rng.choice(SCATTER_ASSETS["stone"][2:])  # 07·14 앵커 잔돌
                    if scale is None:
                        scale = rng.uniform(*rules["stone_anchor_scale"])
                else:
                    asset = pick_asset(kind)
            if scale is None:
                if kind == "stone":
                    scale = rng.uniform(*rules["stone_scale"])
                elif kind == "clover":
                    mult = rules.get("clover_scale_mult", 1.0)
                    scale = rng.uniform(*asset["scale"]) * mult
                else:
                    scale = rng.uniform(*rules["fern_scale"])
            pad_xy = rules["fern_wind_pad_xy_cm"] if kind == "fern" else 0.0
            pad_z = rules["fern_wind_pad_z_cm"] if kind == "fern" else 0.0
            r_xy = asset_radius(asset, scale) + pad_xy
            # 1) 잔돌 100cm 미만 검증
            if kind == "stone":
                max_dim = max(asset["max"][i] - asset["min"][i] for i in range(3)) * scale
                if max_dim >= rules["max_stone_dim_cm"]:
                    return reject("stone_too_big")
            # 2) 섬 외곽 안쪽 여유
            if poly_signed_dist(outline, x, y) < rules["rim_margin_cm"][kind] + r_xy * 0.5:
                return reject("rim")
            # 3) Pod 중심 9m 제외
            if math.hypot(x - cx, y - cy) - r_xy < rules["pod_clear_cm"]:
                return reject("pod_9m")
            # 4) 착지점 4m 제외
            if any(math.hypot(x - lx, py - ly) - r_xy < rules["landing_clear_cm"] for lx, ly in isle_landings for py in (y,)):
                return reject("landing_4m")
            # 5) 엄폐 각기둥 제외
            for cp, _ in isle_covers:
                if poly_signed_dist(cp, x, y) > -(rules["cover_clear_cm"][kind] + r_xy):
                    return reject("cover")
            # 6) GX-043 엄폐 밑동 잔돌 제외
            for rb in isle_rubble:
                rx, ry = rb["center_local"]
                if math.hypot(x - rx, y - ry) < rb["size_cm"] + r_xy + rules["rubble_clear_cm"][kind]:
                    return reject("gx043_rubble")
            # 7) 기존 2c 배치와 겹침 방지
            for other in placed:
                d = math.hypot(x - other["x"], y - other["y"])
                okind = other["kind"]
                if kind == "stone" and okind == "stone":
                    min_d = max(r_xy + other["radius_cm"] + 8.0, 26.0)
                elif kind == "fern" and okind == "fern":
                    min_d = max(110.0, 0.80 * (r_xy + other["radius_cm"]))
                elif kind == "clover" and okind == "clover":
                    min_d = rules.get("clover_overlap_factor", 0.72) * (r_xy + other["radius_cm"])
                elif {kind, okind} == {"fern", "stone"}:
                    min_d = 0.45 * max(r_xy, other["radius_cm"]) + min(r_xy, other["radius_cm"]) + 12.0
                elif {kind, okind} == {"clover", "stone"}:
                    min_d = 0.35 * max(r_xy, other["radius_cm"]) + min(r_xy, other["radius_cm"]) + 6.0
                else:  # clover <-> fern
                    min_d = 0.45 * (r_xy + other["radius_cm"])
                if d < min_d:
                    return reject("overlap_" + okind)
            # 8) 실제 PreservedRocks_Floor 높이 투영
            w_probe = np.asarray(ib.spec.ridge.point(x, y, top_h + 150.0), dtype=np.float64)
            fh = floor_heights(floor, [w_probe])[0]
            if fh is None or abs(fh - top_h) > 35.0:
                return reject("no_floor")
            if kind == "stone":
                h = fh + rules["stone_embed_frac"] * asset["max"][2] * scale
                t = rules["stone_tilt_deg"]
                rot = [round(rng.uniform(-t, t), 3), round(rng.uniform(0.0, 360.0), 3), round(rng.uniform(-t, t), 3)]
            elif kind == "clover":
                h = fh - rules["clover_sink_cm"]
                rot = [0.0, round(rng.uniform(0.0, 360.0), 3), 0.0]
            else:
                h = fh - rules["fern_sink_cm"]
                rot = [0.0, round(rng.uniform(0.0, 360.0), 3), 0.0]
            # 9) OBB 기반 이음매·그래플/랜드마크 사선·보호 공간 검사
            obb = obb_from_pose(x, y, h, rot, scale, asset, pad_xy, pad_z)
            if float(obb[6].min()) < rules["seam_visual_cm"]:
                return reject("seam")
            if len(near_lines) and float(obb_dist_to_points(obb, near_lines).min()) < rules["line_clear_cm"]:
                return reject("sightline")
            for _, pts in near_samples:
                if float(obb_dist_to_points(obb, pts).min()) <= 0.0:
                    return reject("protected_space")
            protrude = (h + asset["max"][2] * scale) - fh
            placed.append({
                "isle": rid, "kind": kind, "zone": zone, "mesh": asset["mesh"],
                "material": asset.get("material"),
                "x": x, "y": y, "floor_h": round(fh, 3),
                "local_xyh_cm": [round(x, 3), round(y, 3), round(h, 3)],
                "rotation_deg": rot, "scale": [round(scale, 4)] * 3,
                "radius_cm": round(r_xy, 1), "protrude_cm": round(protrude, 1),
                "max_dim_cm": round(max(asset["max"][i] - asset["min"][i] for i in range(3)) * scale, 1),
            })
            return True

        def sample_cover_point(min_dist, max_dist):
            cp, (kx, ky) = rng.choice(isle_covers)
            a = rng.uniform(0, math.tau)
            r_c = ib.ray_radius(cp, (kx, ky), a)
            rad = r_c + rng.uniform(min_dist, max_dist)
            return kx + rad * math.cos(a), ky + rad * math.sin(a)

        def sample_rim_point(spread_cm):
            px, py = rng.choice(rim_pockets)
            a = rng.uniform(0, math.tau)
            r = spread_cm * math.sqrt(rng.random())
            return px + r * math.cos(a), py + r * math.sin(a)

        def sample_mid_point():
            a = rng.uniform(0, math.tau)
            r_edge = ib.ray_radius(outline, (cx, cy), a)
            r_lo = rules["pod_clear_cm"] + 120.0
            r_hi = max(r_lo + 50.0, r_edge - 260.0)
            rad = math.sqrt(rng.uniform(r_lo * r_lo, r_hi * r_hi))
            return cx + rad * math.cos(a), cy + rad * math.sin(a)

        def fill_singles(kind, zone, target, sampler):
            made, tries = 0, 0
            while made < target and tries < target * 40:
                tries += 1
                x, y = sampler()
                if try_place(kind, zone, x, y):
                    made += 1
            return made

        def fill_clusters(kind, zone, target, sampler, cluster_range, spread_cm):
            made, tries = 0, 0
            while made < target and tries < target * 35:
                tries += 1
                ax, ay = sampler()
                if not try_place(kind, zone, ax, ay):
                    continue
                made += 1
                want = min(target - made, rng.randint(*cluster_range) - 1)
                sub_tries = 0
                while want > 0 and sub_tries < 25:
                    sub_tries += 1
                    ang = rng.uniform(0, math.tau)
                    dist = rng.uniform(28.0, spread_cm)
                    if try_place(kind, zone, ax + dist * math.cos(ang), ay + dist * math.sin(ang)):
                        made += 1
                        want -= 1
            return made

        T = rules["targets"]
        # 1) Fern 먼저 (엄폐 주변·림 포켓 앵커; fern_cluster가 켜지면 2~3포기 자연 군집)
        if rules.get("fern_cluster"):
            fill_clusters("fern", "cover_skirt", T["cover_fern"], lambda: sample_cover_point(70.0, 320.0), (2, 3), 185.0)
            fill_clusters("fern", "rim_pocket", T["rim_fern"], lambda: sample_rim_point(360.0), (2, 3), 210.0)
        else:
            fill_singles("fern", "cover_skirt", T["cover_fern"], lambda: sample_cover_point(80.0, 290.0))
            fill_singles("fern", "rim_pocket", T["rim_fern"], lambda: sample_rim_point(340.0))
        # 2) Clover (엄폐 주변·림 포켓·중간 링 패치)
        fill_clusters("clover", "cover_skirt", T["cover_clover"], lambda: sample_cover_point(75.0, 420.0), (2, 4), 170.0)
        fill_clusters("clover", "rim_pocket", T["rim_clover"], lambda: sample_rim_point(430.0), (3, 5), 195.0)
        fill_clusters("clover", "mid_ring", T["mid_clover"], sample_mid_point, (2, 4), 165.0)
        # 3) 작은 잔돌 (엄폐 밖 군집·림 군집·중간 링 소군집)
        fill_clusters("stone", "cover_skirt", T["cover_stone"], lambda: sample_cover_point(95.0, 460.0), (2, 4), 95.0)
        fill_clusters("stone", "rim_pocket", T["rim_stone"], lambda: sample_rim_point(450.0), (2, 5), 110.0)
        fill_clusters("stone", "mid_ring", T["mid_stone"], sample_mid_point, (2, 3), 90.0)

        # ---- 전수 검증: 전체 51공간·16,552표본, 전체 사선, Pod 9m, 착지점 4m, GX-043 잔돌 겹침 0 ----
        asset_by_mesh = {a["mesh"]: a for pool in SCATTER_ASSETS.values() for a in pool}
        prot_hits, line_hits, pod_hits, land_hits, rubble_hits, big_stones = 0, 0, 0, 0, 0, 0
        min_seam = 1e9
        for row in placed:
            asset = asset_by_mesh[row["mesh"]]
            scale = row["scale"][0]
            pad_xy = rules["fern_wind_pad_xy_cm"] if row["kind"] == "fern" else 0.0
            pad_z = rules["fern_wind_pad_z_cm"] if row["kind"] == "fern" else 0.0
            obb = obb_from_pose(*row["local_xyh_cm"], row["rotation_deg"], scale, asset, pad_xy, pad_z)
            min_seam = min(min_seam, float(obb[6].min()))
            for sid, pts in samples:
                # AABB 빠른 거부 후 정밀 OBB 거리 검사
                lo, hi = obb[6], obb[7]
                m = ((pts >= lo) & (pts <= hi)).all(axis=1)
                if m.any() and float(obb_dist_to_points(obb, pts[m]).min()) <= 0.0:
                    prot_hits += 1
            if float(obb_dist_to_points(obb, all_line_pts).min()) < rules["line_clear_cm"]:
                line_hits += 1
            if math.hypot(row["x"] - cx, row["y"] - cy) - row["radius_cm"] < rules["pod_clear_cm"] - 1e-3:
                pod_hits += 1
            if any(math.hypot(row["x"] - lx, row["y"] - ly) - row["radius_cm"] < rules["landing_clear_cm"] - 1e-3 for lx, ly in isle_landings):
                land_hits += 1
            if any(math.hypot(row["x"] - rb["center_local"][0], row["y"] - rb["center_local"][1]) < rb["size_cm"] + row["radius_cm"] for rb in isle_rubble):
                rubble_hits += 1
            if row["kind"] == "stone" and row["max_dim_cm"] >= 100.0:
                big_stones += 1

        by_kind = {}
        by_mesh = {}
        for row in placed:
            by_kind[row["kind"]] = by_kind.get(row["kind"], 0) + 1
            short = row["mesh"].rsplit("/", 1)[-1]
            by_mesh[short] = by_mesh.get(short, 0) + 1
        lo_n, hi_n = rules["count_range"]
        if not (lo_n <= len(placed) <= hi_n):
            issues.append("count %s %d not in [%d,%d]" % (rid, len(placed), lo_n, hi_n))
        for tag, val in (("protected_hits", prot_hits), ("line_hits", line_hits), ("pod_9m_hits", pod_hits),
                         ("landing_4m_hits", land_hits), ("rubble_overlap", rubble_hits), ("stone_ge_100cm", big_stones)):
            if val:
                issues.append("%s %s %d" % (rid, tag, val))
        if min_seam < rules["seam_visual_cm"]:
            issues.append("%s seam %.1f" % (rid, min_seam))

        # JSON에는 내부 임시 키(x, y) 제거
        clean = [{k: v for k, v in r.items() if k not in ("x", "y")} for r in placed]
        placements_by_isle[rid] = clean
        summary[rid] = {
            "total": len(clean), "by_kind": by_kind, "by_mesh": dict(sorted(by_mesh.items())),
            "stone_max_dim_cm": max((r["max_dim_cm"] for r in clean if r["kind"] == "stone"), default=0.0),
            "stone_max_protrude_cm": max((r["protrude_cm"] for r in clean if r["kind"] == "stone"), default=0.0),
            "fern_max_protrude_cm": max((r["protrude_cm"] for r in clean if r["kind"] == "fern"), default=0.0),
            "protected_hits": prot_hits, "line_hits": line_hits, "pod_9m_hits": pod_hits,
            "landing_4m_hits": land_hits, "rubble_overlap": rubble_hits, "seam_min_cm": round(min_seam, 1),
            "rejects": rejects, "rim_pockets": [[round(a, 1), round(b, 1)] for a, b in rim_pockets],
        }

    report = {
        "version": VERSION, "unit": "IsleTop 2c scatter", "decision": "GX-042",
        "rules": rules, "protected_spaces": len(spaces), "samples": sum(len(p) for _, p in samples),
        "lines": {k: len(v) for k, v in lines.items()},
        "summary": summary, "placements": placements_by_isle,
        "issues": issues, "pass": not issues, "floor_dump_sha256": floor_sha,
        "inputs": {p.relative_to(ROOT).as_posix(): sha(p) for p in
                   (ib.BLUEPRINT, ib.TRIAL, ib.HELPER, SCATTER_COVER_BUILD,
                    ROOT / "Scripts/RidgeOctantSpec.py", Path(meas.__file__), Path(__file__))},
    }
    (out / "Build.json").write_text(json.dumps(report, ensure_ascii=False, indent=1), encoding="utf-8")
    print("ISLE_TOP_SCATTER " + json.dumps({
        "pass": report["pass"], "issues": issues,
        "summary": {k: {m: v[m] for m in ("total", "by_kind", "stone_max_dim_cm", "stone_max_protrude_cm",
                                          "fern_max_protrude_cm", "protected_hits", "line_hits",
                                          "pod_9m_hits", "landing_4m_hits", "rubble_overlap", "seam_min_cm")}
                    for k, v in summary.items()},
    }), flush=True)


if __name__ == "__main__":
    if MODE == "cover" and COVER_RULES.get(VERSION, {}).get("style") == "faceted":
        cover_faceted()
    else:
        {"cap": build, "cover": cover, "scatter": scatter}[MODE]()
