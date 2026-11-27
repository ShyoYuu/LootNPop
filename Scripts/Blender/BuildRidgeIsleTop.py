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


if __name__ == "__main__":
    if MODE == "cover" and COVER_RULES.get(VERSION, {}).get("style") == "faceted":
        cover_faceted()
    else:
        {"cap": build, "cover": cover}[MODE]()
