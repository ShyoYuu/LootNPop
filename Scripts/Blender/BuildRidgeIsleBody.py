"""Phase 1-BR 부유섬 2개의 닫힌 시각 몸체와 중간 단계 부유석을 만든다(GX-037·GX-038).

Blender --background --factory-startup --python Scripts/Blender/BuildRidgeIsleBody.py -- build V002
(작업 폴더는 프로젝트 루트.) 결과: Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Geometry_<판>/ 의 FBX·Build.json.

- 몸체는 V011 부유섬(평탄 윗면 + 30%로 좁아지는 뿔대)을 바깥에서 감싸 숨기는 시각 전용 닫힌 메시다.
  윗면 테두리는 외곽선과 같고 윗면 덮개는 V011 윗면보다 3cm 아래라 기존 보행면이 그대로 보인다.
  아래는 엇갈린 갈래로 갈라지며 뾰족해진다. 충돌은 기존 PreservedRocks Floor/Shell이 계속 맡는다.
- 중간 단계 부유석은 섬 반경 밖(29m 보호 원판 밖)에 둔다. 기존 섬을 보강하는 표현이며 새 빈 공간 장식이 아니다(GX-038).
- 정점은 월드 좌표(UE cm)이고 FBX 규약·4m 우세 축 UV는 U5a 몸체(BuildRidgeClosedBodyTrial.py)와 같다.
- 보호 공간 51개 표본은 기존 정의(DiagnosePlacement.samples)로 만들고 BVH 안쪽 판정으로 검사한다.
  기존 V011 섬 몸체 안에 이미 있던 표본(새로 차지할 수 없는 자리)은 따로 분류해 보고한다.
"""
import hashlib
import json
import math
from pathlib import Path
import random
import sys

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "Scripts")]
import RidgeZ1CompositionSpec as spec  # noqa: E402

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else ["build", "V002"]
MODE, VERSION = ARGS[0], ARGS[1]
OUT = ROOT / ("Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Geometry_" + VERSION)
BLUEPRINT = ROOT / "Scripts/Profiling/Graphics/RidgeBlueprint_V001.json"
TRIAL = ROOT / "Scripts/Profiling/Graphics/ClosedBodyTrial_V001/BodyTrial.json"
HELPER = ROOT / "Saved/Profiling/Graphics/Phase01/U4_2_V002/DiagnosePlacement.py"

CENTERS = {"ValleySkyIsle": (7800.0, -5600.0), "WestSkyIsle": (-14600.0, 4800.0)}
RULES = {
    "V002": {
        "seed": 51128, "angular": 120, "isle_depth_cm": 2300, "bottom_scale": 0.3,
        # (윗면 아래 깊이 cm, 외곽선 대비 반경 배율). V011 뿔대(1-0.7d/2300)보다 항상 바깥.
        "profile": [(3, 1.0), (60, 1.0), (200, 1.035), (450, 1.0), (800, 0.88), (1200, 0.745),
                    (1600, 0.6), (2000, 0.43), (2400, 0.32), (2700, 0.2)],
        "noise": [(0, 0.0), (60, 0.0), (400, 0.07), (1400, 0.07), (2000, 0.035), (2700, 0.05)],
        # 갈래 아래로 늘이기는 20m 보호 고리(반경 14.5m) 아래에서만 한다.
        "prongs": 5, "prong_start_cm": 1300, "prong_min": 0.35, "prong_drop_cm": 650, "prong_drop_start_cm": 2000,
        "tip_center_cm": 2900,
        "frustum_margin": 0.03,
        "satellites": {"rocks": 5, "chips": 4, "radius_cm": [3800, 4700], "depth_cm": [400, 2100],
                       "rock_size_cm": [350, 750], "chip_size_cm": [90, 200]},
    },
    # V003: 갈래 사이로 V011 밑면이 비치던 문제 → 모든 깊이에서 V011 뿔대 바깥을 유지(바닥은 넓은 사발),
    # 갈래는 별도 닫힌 원뿔 메시로 매단다. 해상도·지층 띠를 늘려 저폴리 면을 줄인다.
    "V003": {
        "seed": 51128, "angular": 240, "isle_depth_cm": 2300, "bottom_scale": 0.3,
        "profile": [(3, 1.0), (60, 1.0), (150, 1.012), (300, 1.015), (450, 1.0), (650, 0.94), (850, 0.87),
                    (1050, 0.8), (1250, 0.73), (1450, 0.66), (1650, 0.58), (1850, 0.5), (2000, 0.43),
                    (2200, 0.37), (2400, 0.31), (2550, 0.2), (2620, 0.06)],
        "noise": [(0, 0.0), (60, 0.0), (300, 0.06), (1400, 0.07), (2000, 0.035), (2620, 0.05)],
        # 테두리 바로 아래 돌출은 골짜기 섬 그래플 사선(북쪽 테두리 3m 아래 스침)을 피하도록 작게 둔다.
        "strata": {"amplitude": 0.022, "period_cm": 210, "warp": 0.9}, "detail_fade_cm": 600,
        "fine": 0.018,
        "prongs": 0, "prong_start_cm": 99999, "prong_min": 1.0, "prong_drop_cm": 0, "prong_drop_start_cm": 99999,
        "tip_center_cm": 2650,
        "frustum_margin": 0.03,
        "spikes": {"count": 6, "radius_frac": [0.12, 0.27], "top_depth_cm": 2050, "length_cm": [650, 1250],
                   "base_cm": [260, 430], "max_tip_depth_cm": 3350},
        "satellites": {"rocks": 5, "chips": 4, "radius_cm": [3800, 4700], "depth_cm": [400, 2100],
                       "rock_size_cm": [350, 750], "chip_size_cm": [90, 200]},
    },
}
# V005(사용자 판정 2026-11-27): 섬 밑 갈래(뾰족한 원뿔)는 어색 → 빼고 밑면을 둥글게 닫는다.
# 부유석은 V003과 같게 두려고 갈래 생성의 난수 소비만 유지한다(spikes_discard).
# 반듯한 직육면체 석교 → 상판을 감싸는 자연 형상 몸체(양 끝이 두껍고 가운데가 얇은 아치, 불규칙 밑면).
#   A: 보호 공간 SkyBridgeDeck(상판 아래 4.5m까지 반폭 3.5m)을 지킨다. 옆 띠는 3.5m 안에서만 깎는다.
#   B: 석교 자신의 외피에 한해 옆 띠를 바깥으로 부풀린다. 그 표본 침범은 숨기지 않고 보고한다(사용자 판단 대기).
RULES["V005"] = json.loads(json.dumps(RULES["V003"]))
RULES["V005"]["profile"] = [list(p) for p in RULES["V003"]["profile"][:-2]] + [[2550, 0.23], [2650, 0.13], [2710, 0.05]]
RULES["V005"].update({
    "tip_center_cm": 2725, "spikes_discard": True,
    "bridge_body": {"ring_step_cm": 60, "lip_cm": 6, "top_gap_cm": 3, "deck_half_cm": 300,
                    "slab_thickness_cm": 500, "guard_half_cm": 348, "guard_depth_cm": 460,
                    "end_depth_cm": 1500, "mid_depth_cm": 800, "end_half_cm": 760, "mid_half_cm": 560,
                    "noise": 0.12, "variants": ["A", "B"]},
})


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def bl(v):
    return np.asarray(v) * np.array([.01, -.01, .01])


def ue(v):
    return np.asarray(v) * np.array([100., -100., 100.])


def world(x, y, h):
    return np.asarray(spec.ridge.point(float(x), float(y), float(h)), dtype=np.float64)


def make(name, v, t):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(bl(v).tolist(), [], np.asarray(t)[:, ::-1].tolist())
    mesh.validate()
    mesh.update()
    # 면 방향을 서로 맞춘 뒤(bmesh), Blender에서 바깥 법선(부피 양수)이 되도록 맞춘다.
    # UE 반입 뒤 앞면이 바깥을 본다(U5a와 같은 규약).
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()
    volume = 0.0
    co = [v.co for v in mesh.vertices]
    for poly in mesh.polygons:
        a = co[poly.vertices[0]]
        for i in range(1, len(poly.vertices) - 1):
            volume += a.dot(co[poly.vertices[i]].cross(co[poly.vertices[i + 1]])) / 6.0
    if volume < 0:
        mesh.flip_normals()
        mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def arrays(obj):
    m = obj.data
    m.calc_loop_triangles()
    v = np.asarray([p.co[:] for p in m.vertices], dtype=np.float64)
    t = np.asarray([p.vertices[:] for p in m.loop_triangles], dtype=np.int64)
    return ue(v), t[:, ::-1]


def topology(obj):
    edges = {}
    for poly in obj.data.polygons:
        vs = list(poly.vertices)
        for a, b in zip(vs, vs[1:] + vs[:1]):
            k = (min(a, b), max(a, b))
            edges[k] = edges.get(k, 0) + 1
    return {"vertices": len(obj.data.vertices), "faces": len(obj.data.polygons),
            "boundary_edges": sum(1 for c in edges.values() if c == 1),
            "nonmanifold_edges": sum(1 for c in edges.values() if c > 2)}


def interp(table, d):
    for (d0, v0), (d1, v1) in zip(table, table[1:]):
        if d <= d1:
            t = 0.0 if d1 == d0 else (d - d0) / (d1 - d0)
            return v0 + (v1 - v0) * t
    return table[-1][1]


def ray_radius(outline, center, theta):
    d = (math.cos(theta), math.sin(theta))
    best = None
    for a, b in zip(outline, outline[1:] + outline[:1]):
        e = (b[0] - a[0], b[1] - a[1])
        w = (a[0] - center[0], a[1] - center[1])
        den = d[0] * e[1] - d[1] * e[0]
        if abs(den) < 1e-9:
            continue
        t = (w[0] * e[1] - w[1] * e[0]) / den
        u = (w[0] * d[1] - w[1] * d[0]) / den
        if t > 0 and -1e-9 <= u <= 1 + 1e-9:
            best = t if best is None else min(best, t)
    return best


def isle_body(rid, outline, top, rules, rng):
    """고리(깊이별 각도 표본) 격자 + 윗면 덮개 + 아래 꼭짓점으로 닫힌 몸체를 만든다."""
    cx, cy = CENTERS[rid]
    n = rules["angular"]
    thetas = [math.tau * i / n for i in range(n)]
    r_out = [ray_radius(outline, (cx, cy), th) for th in thetas]
    phase = [rng.uniform(0, math.tau) for _ in range(4)]
    prong_phase = rng.uniform(0, math.tau)
    depths = [d for d, _ in rules["profile"]]
    frustum = lambda d: 1 - (1 - rules["bottom_scale"]) * min(d, rules["isle_depth_cm"]) / rules["isle_depth_cm"]
    verts, rings = [], []
    for d in depths:
        ring = []
        g = interp(rules["profile"], d)
        amp = interp(rules["noise"], d)
        prong_t = max(0.0, (d - rules["prong_start_cm"]) / (depths[-1] - rules["prong_start_cm"]))
        for i, th in enumerate(thetas):
            noise = (0.55 * math.sin(3 * th + phase[0] + 0.0012 * d) + 0.3 * math.sin(7 * th + phase[1])
                     + 0.15 * math.sin(13 * th + phase[2] + 0.002 * d))
            lobe = max(0.0, math.cos(rules["prongs"] * th + prong_phase + 0.35 * math.sin(2 * th + phase[3])))
            prong = 1 - prong_t * (1 - rules["prong_min"]) * (1 - lobe ** 0.6)
            scale = g * (1 + amp * noise) * prong
            # 지층 띠·잔요철은 윗면 테두리(외곽선 일치)에서 3m 아래까지 서서히 켠다.
            fade = min(1.0, d / rules.get("detail_fade_cm", 300.0))
            st = rules.get("strata")
            if st:
                band = math.tanh(4 * math.sin(math.tau * d / st["period_cm"] + st["warp"] * math.sin(3 * th + phase[3])))
                scale *= 1 + fade * st["amplitude"] * band
            if rules.get("fine"):
                scale *= 1 + fade * rules["fine"] * math.sin(23 * th + phase[1] + 0.004 * d) * math.sin(0.03 * d + phase[2])
            # V011 뿔대보다 안쪽으로 들어가면 바깥으로 민다(기존 몸체를 완전히 감싼다).
            scale = max(scale, frustum(d) + rules["frustum_margin"]) if d <= rules["isle_depth_cm"] else scale
            drop_t = max(0.0, (d - rules["prong_drop_start_cm"]) / (depths[-1] - rules["prong_drop_start_cm"]))
            depth = d + drop_t * rules["prong_drop_cm"] * lobe
            r = r_out[i] * scale
            ring.append(len(verts))
            verts.append(world(cx + r * math.cos(th), cy + r * math.sin(th), top - depth))
        rings.append(ring)
    top_center = len(verts)
    verts.append(world(cx, cy, top - depths[0]))
    tip = len(verts)
    verts.append(world(cx, cy, top - rules["tip_center_cm"]))
    tris = []
    for i in range(n):
        j = (i + 1) % n
        # 윗면 덮개(위를 향함), 옆면, 아래 꼭짓점. 바깥 법선 기준 반시계(UE 쪽에서 make가 뒤집는다).
        tris.append((top_center, rings[0][j], rings[0][i]))
        for a, b in zip(rings, rings[1:]):
            tris.append((a[i], a[j], b[j]))
            tris.append((a[i], b[j], b[i]))
        tris.append((rings[-1][i], rings[-1][j], tip))
    return np.asarray(verts), np.asarray(tris)


def spikes(rid, outline, top, rules, rng):
    """섬 밑에 매다는 갈래: 몸체 안(윗면 아래 20.5m)에서 시작해 아래로 뾰족해지는 닫힌 원뿔 여러 개(서로 겹치지 않음)."""
    sp = rules["spikes"]
    cx, cy = CENTERS[rid]
    verts, tris = [], []
    start = rng.uniform(0, math.tau)
    for k in range(sp["count"]):
        th = start + math.tau * k / sp["count"] + rng.uniform(-0.15, 0.15)
        r = ray_radius(outline, (cx, cy), th) * rng.uniform(*sp["radius_frac"])
        base = rng.uniform(*sp["base_cm"])
        length = min(rng.uniform(*sp["length_cm"]), sp["max_tip_depth_cm"] - sp["top_depth_cm"])
        bend = (rng.uniform(-1, 1) * 0.25 * length, rng.uniform(-1, 1) * 0.25 * length)
        px, py = cx + r * math.cos(th), cy + r * math.sin(th)
        seg, rings = 12, 8
        phase = [rng.uniform(0, math.tau) for _ in range(3)]
        idx = []
        for i in range(rings):
            t = i / rings
            radius = base * (1 - t) ** 0.85
            ox, oy = bend[0] * t * t, bend[1] * t * t
            row = []
            for j in range(seg):
                a = math.tau * j / seg
                nz = 1 + 0.16 * math.sin(3 * a + phase[0] + 4 * t) + 0.08 * math.sin(5 * a + phase[1])
                row.append(len(verts))
                verts.append(world(px + ox + radius * nz * math.cos(a), py + oy + radius * nz * math.sin(a),
                                   top - sp["top_depth_cm"] - length * t))
            idx.append(row)
        cap = len(verts)
        verts.append(world(px, py, top - sp["top_depth_cm"]))
        tip = len(verts)
        verts.append(world(px + bend[0], py + bend[1], top - sp["top_depth_cm"] - length))
        for j in range(seg):
            n = (j + 1) % seg
            tris.append((cap, idx[0][n], idx[0][j]))
            for r0, r1 in zip(idx, idx[1:]):
                tris.append((r0[j], r0[n], r1[n]))
                tris.append((r0[j], r1[n], r1[j]))
            tris.append((idx[-1][j], idx[-1][n], tip))
    return np.asarray(verts), np.asarray(tris)


def bridge_body(deck, rules, variant, rng):
    """석교 상판(V011 판)을 감싸는 자연 형상 닫힌 몸체. 단면 고리를 상판 방향으로 늘어놓는다.
    윗면은 상판보다 top_gap 아래라 기존 보행면이 보이고, 상판 가장자리만 lip만큼 덮어 반듯한 모서리를 지운다."""
    bb = rules["bridge_body"]
    (ax, ay), (bx, by), ha, hb, _ = deck
    length = math.hypot(bx - ax, by - ay)
    tx, ty = (bx - ax) / length, (by - ay) / length
    nx, ny = -ty, tx
    half = bb["deck_half_cm"]
    phases = [rng.uniform(0, math.tau) for _ in range(64)]

    def noise(k, u, slow=430.0, fast=170.0):
        return 0.6 * math.sin(u / slow + phases[k % 64]) + 0.4 * math.sin(u / fast + 2 * phases[(k * 7) % 64])

    def section(u):
        s = math.sin(math.pi * min(max(u / length, 0.0), 1.0))
        depth = bb["end_depth_cm"] - (bb["end_depth_cm"] - bb["mid_depth_cm"]) * s ** 0.8
        width = bb["end_half_cm"] - (bb["end_half_cm"] - bb["mid_half_cm"]) * s
        lump = 1 + 0.16 * max(0.0, math.sin(u / 260 + phases[3])) ** 3
        # 한쪽(+v) 반단면: 윗면 가운데 → 가장자리 덮개 → 옆 띠 → 밑면 → 바닥 가운데. 깊이는 상판 아래 양수.
        top = [(0, bb["top_gap_cm"]), (half - 40, bb["top_gap_cm"]), (half, -bb["lip_cm"]), (half + 22, 18)]
        if variant == "A":
            side = [(half + 36, 120), (half + 42, 260), (half + 45, 400), (bb["guard_half_cm"], bb["guard_depth_cm"])]
        else:
            side = [(half + 90, 110), (half + 170, 250), (0.8 * width, 420)]
        under = [(0.78 * width, 560), (width, 720), (0.9 * width, 0.55 * depth + 300),
                 (0.62 * width, 0.85 * depth * lump), (0.3 * width, 0.98 * depth * lump), (0, depth * lump)]
        return top + side + under

    profile0 = section(0.0)
    count = len(profile0)
    steps = max(2, int(math.ceil(length / bb["ring_step_cm"])))
    rings, verts = [], []
    for i in range(steps + 1):
        u = length * i / steps
        half_section = section(u)
        # 오른쪽(+v) → 바닥 가운데 → 왼쪽(-v) 거꾸로. 가운데 점(v=0) 두 개는 한 번만 쓴다.
        loop = [(v, d, k) for k, (v, d) in enumerate(half_section)]
        loop += [(-v, d, k + count) for k, (v, d) in reversed(list(enumerate(half_section)))][1:-1]
        ring = []
        for v, d, k in loop:
            guarded = variant == "A" and d <= bb["guard_depth_cm"]
            if d > 60:
                n = noise(k, u)
                v *= 1 + bb["noise"] * n * min(1.0, d / 300.0)
                if not guarded:
                    d *= 1 + 0.5 * bb["noise"] * noise(k + 31, u, 380.0, 140.0) * min(1.0, d / 600.0)
            if guarded:
                # A: 보호 띠(상판 아래 4.6m까지) 안의 점은 깊이를 흔들지 않고 반폭 3.48m 안으로 묶는다.
                v = max(-bb["guard_half_cm"], min(bb["guard_half_cm"], v))
            h = ha + (hb - ha) * u / length - d
            ring.append(len(verts))
            verts.append(world(ax + tx * u + nx * v, ay + ty * u + ny * v, h))
        rings.append(ring)
    m = len(rings[0])
    tris = []
    for r0, r1 in zip(rings, rings[1:]):
        for j in range(m):
            k = (j + 1) % m
            tris.append((r0[j], r0[k], r1[k]))
            tris.append((r0[j], r1[k], r1[j]))
    for ring, flip in ((rings[0], False), (rings[-1], True)):
        center = len(verts)
        verts.append(np.mean([verts[i] for i in ring], axis=0))
        for j in range(m):
            k = (j + 1) % m
            tris.append((center, ring[k], ring[j]) if not flip else (center, ring[j], ring[k]))
    return np.asarray(verts), np.asarray(tris)


def v011_bridge_inside(deck, rules, p_local):
    """표본이 기존 V011 석교 판(반폭 300, 두께 500) 안인지."""
    (ax, ay), (bx, by), ha, hb, _ = deck
    length = math.hypot(bx - ax, by - ay)
    tx, ty = (bx - ax) / length, (by - ay) / length
    x, y, h = p_local
    u = (x - ax) * tx + (y - ay) * ty
    v = -(x - ax) * ty + (y - ay) * tx
    if not -1.0 <= u <= length + 1.0:
        return False
    d = ha + (hb - ha) * u / length - h
    return abs(v) <= rules["bridge_body"]["deck_half_cm"] + 1.0 and -1.0 <= d <= rules["bridge_body"]["slab_thickness_cm"]


def rock(center_xyh, size, rng, squash=0.75, keel=1.7):
    """노이즈 타원체 부유석: 위는 납작하게 깎고 아래는 길게 뾰족하다."""
    m, k = 10, 20
    phase = [rng.uniform(0, math.tau) for _ in range(5)]
    axes = (size * rng.uniform(0.85, 1.15), size * rng.uniform(0.6, 0.95), size * squash)
    yaw = rng.uniform(0, math.tau)
    verts, idx = [], []
    for a in range(1, m):
        lat = math.pi * a / m
        row = []
        for b in range(k):
            lon = math.tau * b / k
            ux, uy, uz = math.sin(lat) * math.cos(lon), math.sin(lat) * math.sin(lon), math.cos(lat)
            noise = 1 + 0.16 * math.sin(3 * lon + phase[0]) * math.sin(2 * lat + phase[1]) \
                + 0.09 * math.sin(7 * lon + phase[2] + 3 * lat) + 0.05 * math.sin(11 * lon + phase[3])
            x, y, z = ux * axes[0] * noise, uy * axes[1] * noise, uz * axes[2] * noise
            if z > 0:
                z = min(z, axes[2] * 0.55 * (1 + 0.1 * math.sin(5 * lon + phase[4])))
            else:
                z *= keel
                pinch = 1 - 0.45 * (-uz)
                x, y = x * pinch, y * pinch
            c, s = math.cos(yaw), math.sin(yaw)
            row.append(len(verts))
            verts.append((center_xyh[0] + c * x - s * y, center_xyh[1] + s * x + c * y, center_xyh[2] + z))
        idx.append(row)
    top = len(verts)
    verts.append((center_xyh[0], center_xyh[1], center_xyh[2] + axes[2] * 0.5))
    bottom = len(verts)
    verts.append((center_xyh[0], center_xyh[1], center_xyh[2] - axes[2] * keel * 0.95))
    tris = []
    for b in range(k):
        c = (b + 1) % k
        tris.append((top, idx[0][b], idx[0][c]))
        for r0, r1 in zip(idx, idx[1:]):
            tris.append((r0[b], r1[b], r1[c]))
            tris.append((r0[b], r1[c], r0[c]))
        tris.append((idx[-1][b], bottom, idx[-1][c]))
    w = np.asarray([world(*p) for p in verts])
    return w, np.asarray(tris)


def helper():
    env = {}
    source = HELPER.read_text(encoding="utf-8").split("\nraw =")[0]
    exec(compile(source, str(HELPER), "exec"), env)
    return env


def bvh_of(v, t):
    return BVHTree.FromPolygons([tuple(p) for p in v.tolist()], [tuple(f) for f in t.tolist()], all_triangles=True)


def inside(bvh, p):
    """+x·+y·+z 세 방향 광선 교차 수의 다수결 홀짝."""
    votes = 0
    for d in (Vector((1, 0.0013, 0.0007)), Vector((0.0011, 1, 0.0009)), Vector((0.0008, 0.0012, 1))):
        count, origin = 0, Vector(p)
        for _ in range(64):
            hit = bvh.ray_cast(origin, d)
            if hit[0] is None:
                break
            count += 1
            origin = hit[0] + d * 0.01
        votes += count % 2
    return votes >= 2


def v011_isle_inside(rid, outline, top, p_local, rules):
    """표본이 기존 V011 섬(평탄 윗면 + 30% 뿔대) 안인지. 새 몸체가 차지해도 새 점유가 아닌 자리."""
    x, y, h = p_local
    d = top - h
    if d < 0 or d > rules["isle_depth_cm"]:
        return False
    cx, cy = CENTERS[rid]
    th = math.atan2(y - cy, x - cx)
    r = math.hypot(x - cx, y - cy)
    f = 1 - (1 - rules["bottom_scale"]) * d / rules["isle_depth_cm"]
    return r <= ray_radius(outline, (cx, cy), th) * f


def export(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    while obj.data.uv_layers:
        obj.data.uv_layers.remove(obj.data.uv_layers[0])
    layer = obj.data.uv_layers.new(name="BodyProjection4m")
    for poly in obj.data.polygons:
        axis = max(range(3), key=lambda i: abs(poly.normal[i]))
        uv_axes = [i for i in range(3) if i != axis]
        for loop_index in poly.loop_indices:
            p = obj.data.vertices[obj.data.loops[loop_index].vertex_index].co
            layer.data[loop_index].uv = (p[uv_axes[0]] / 4, p[uv_axes[1]] / 4)
    path = OUT / (obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"},
                             apply_scale_options="FBX_SCALE_NONE", global_scale=1, axis_forward="-Z", axis_up="Y",
                             mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False)
    v, t = arrays(obj)
    return {"name": obj.name, "file": path.relative_to(ROOT).as_posix(), "sha256": sha(path),
            "vertices": len(v), "triangles": len(t),
            "bounds_cm": [v.min(axis=0).tolist(), v.max(axis=0).tolist()], "topology": topology(obj)}


def build():
    rules = RULES[VERSION]
    rng = random.Random(rules["seed"])
    OUT.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    blueprint = json.loads(BLUEPRINT.read_text(encoding="utf-8"))
    islands = {i["id"]: i for i in blueprint["preserve"]["islands"]}
    env = helper()
    spaces = json.loads(TRIAL.read_text(encoding="utf-8"))["protected_spaces"]
    samples = [(c["id"], env["samples"](c)) for c in spaces]
    objects = []
    for rid in ("ValleySkyIsle", "WestSkyIsle"):
        it = islands[rid]
        outline = [tuple(p[:2]) for p in it["outline"]]
        top = it["island_top"]
        v, t = isle_body(rid, outline, top, rules, rng)
        objects.append({"obj": make("SM_Ridge_IsleBody_%s_%s" % (rid, VERSION), v, t), "isle": rid,
                        "outline": outline, "top": top, "role": "isle_body"})
        if rules.get("spikes") and rules.get("spikes_discard"):
            spikes(rid, outline, top, rules, rng)  # 부유석 난수 흐름을 V003과 같게 유지하려고 생성만 하고 버린다.
        elif rules.get("spikes"):
            v, t = spikes(rid, outline, top, rules, rng)
            objects.append({"obj": make("SM_Ridge_IsleSpikes_%s_%s" % (rid, VERSION), v, t), "isle": rid,
                            "outline": outline, "top": top, "role": "isle_spikes"})
        sat = rules["satellites"]
        cx, cy = CENTERS[rid]
        total = sat["rocks"] + sat["chips"]
        rock_rows = []
        for i in range(total):
            for attempt in range(12):
                th = math.tau * (i + rng.uniform(0.1, 0.9)) / total
                radius = rng.uniform(*sat["radius_cm"])
                depth = rng.uniform(*sat["depth_cm"])
                size = rng.uniform(*(sat["rock_size_cm"] if i < sat["rocks"] else sat["chip_size_cm"]))
                center = (cx + radius * math.cos(th), cy + radius * math.sin(th), top - depth)
                v, t = rock(center, size, rng)
                bvh = bvh_of(v, t)
                lo, hi = v.min(axis=0), v.max(axis=0)
                bad = any(inside(bvh, p) for _, pts in samples for p in pts
                          if (np.asarray(p) >= lo - 1).all() and (np.asarray(p) <= hi + 1).all())
                if not bad:
                    break
            rock_rows.append((v, t, center, size, attempt, bad))
        # 같은 섬의 부유석은 하나의 메시로 묶는다(액터 수를 줄인다).
        verts, tris = [], []
        for v, t, *_ in rock_rows:
            if _[3]:
                continue
            tris.append(t + len(verts) if verts else t)
            verts.extend(v.tolist())
        tris = np.concatenate([np.asarray(x) for x in tris])
        name = "SM_Ridge_IsleFloat_%s_%s" % (rid, VERSION)
        objects.append({"obj": make(name, np.asarray(verts), tris), "isle": rid, "outline": outline, "top": top,
                        "role": "satellites", "placed": [{"center_xyh_cm": list(r[2]), "size_cm": r[3],
                                                          "retries": r[4], "dropped": r[5]} for r in rock_rows]})
    deck = blueprint["preserve"]["bridges"][0]["deck"]
    if rules.get("bridge_body"):
        for variant in rules["bridge_body"]["variants"]:
            v, t = bridge_body(deck, rules, variant, random.Random(rules["seed"] + 7))
            objects.append({"obj": make("SM_Ridge_SkyBridgeBody%s_%s" % (variant, VERSION), v, t), "isle": "SkyBridge",
                            "role": "bridge_body", "variant": variant})
    report_rows, protected = [], []
    for row in objects:
        v, t = arrays(row["obj"])
        bvh = bvh_of(v, t)
        lo, hi = v.min(axis=0), v.max(axis=0)
        for sid, pts in samples:
            new_hits, existing = [], []
            for k, p in enumerate(pts):
                q = np.asarray(p)
                if (q < lo).any() or (q > hi).any() or not inside(bvh, q):
                    continue
                p_local = spec.ridge.local(tuple(q))
                if row["role"] in ("isle_body", "isle_spikes") and v011_isle_inside(row["isle"], row["outline"], row["top"], p_local,
                                                                   RULES[VERSION]):
                    existing.append(k)
                elif row["role"] == "bridge_body" and v011_bridge_inside(deck, RULES[VERSION], p_local):
                    existing.append(k)
                else:
                    new_hits.append({"sample": k, "local_xyh_cm": [round(c, 1) for c in p_local]})
            if new_hits or existing:
                protected.append({"object": row["obj"].name, "space": sid, "new_hits": new_hits,
                                  "inside_existing_v011_isle": existing})
        exported = export(row["obj"])
        exported.update({"role": row["role"], "isle": row["isle"], "collision": "NoCollision",
                         "placed": row.get("placed"), "variant": row.get("variant"),
                         "new_hits": sum(len(p["new_hits"]) for p in protected if p["object"] == row["obj"].name)})
        report_rows.append(exported)
    report = {"version": VERSION, "rules": rules, "outputs": report_rows,
              "protected_spaces": len(spaces), "samples": sum(len(p) for _, p in samples),
              "protected": protected,
              "new_hits": sum(len(p["new_hits"]) for p in protected),
              "inside_existing_v011_isle": sum(len(p["inside_existing_v011_isle"]) for p in protected),
              "topology_pass": all(r["topology"]["boundary_edges"] == 0 and r["topology"]["nonmanifold_edges"] == 0
                                   for r in report_rows),
              "inputs": {p.relative_to(ROOT).as_posix(): sha(p) for p in (BLUEPRINT, TRIAL, HELPER, Path(__file__))}}
    (OUT / "Build.json").write_text(json.dumps(report, ensure_ascii=False, indent=1), encoding="utf-8")
    print("ISLE_BODY_BUILD " + json.dumps({k: report[k] for k in ("version", "samples", "new_hits",
                                                                  "inside_existing_v011_isle", "topology_pass")}),
          flush=True)


if __name__ == "__main__":
    assert MODE == "build", MODE
    build()
