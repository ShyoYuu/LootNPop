"""Phase 1-BR 산괴 동쪽 탑(M1)을 갈라진 첨탑·부유 모노리스로 재해석하는 시각 외피(A안, GX-038).

Blender --background --factory-startup --python Scripts/Blender/BuildRidgeMonolithSkin.py -- build V001
(작업 폴더는 프로젝트 루트.) 결과: Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Monolith_<판>/ 의 FBX·Build.json.

대상 M1은 동굴 보존 구조물(PreservedRocks_Shell) 북동 모서리 벽(꼭대기 52~56m)과 그 동쪽 렌더링 지각 절벽(56m→고원 7m)이다.
지각·충돌은 건드리지 않는다(A안). 따라서 하늘이 비치는 틈은 기존 형상 위(꼭대기 위 공중)에만 생긴다.

- 몸체(Shroud): 지각+보존 구조물 높이장을 높이별 반경으로 바깥으로 넓힌(층 띠가 생기는 팽창) 두꺼운 외피.
  위는 외피 윗면, 아래는 기존 표면 1.5m 아래라 닫힌 메시다. 평지·착지면에서는 기존 표면 30cm 아래로 묻힌다.
  서쪽(StairRamp3 쪽) 면은 거의 부풀리지 않는다. 탑 꼭대기는 가로 균열로 남·북 두 덩이로 가르고 조금 올린다.
- 모노리스(Cap): 북쪽 덩이 위 3.2m 공중에 뜬 기울어진 각진 덩어리. 기존 탑의 재해석이며 빈 공간 장식이 아니다(GX-038).
- 파편(Chips): 틈 둘레의 둥근 닫힌 부유석 3개. 뾰족한 갈래·Cliff02 파편은 쓰지 않는다.
입력 형상은 에디터 InspectRidgeMonolithTargets.py `dump`(IsleSkin_V006 맵)의 삼각형이다. 보호 51공간 표본은
BuildRidgeIsleBody.py와 같은 정의·BVH 안쪽 판정으로 검사하고, 기존 형상 안에 원래 있던 표본은 따로 센다.
"""
import hashlib
import json
import math
from pathlib import Path
import random
import sys

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "Scripts/Blender"), str(ROOT / "Scripts")]
import BuildRidgeIsleBody as ib  # noqa: E402  (make·export·rock·bvh/inside·helper 재사용)
import RidgeOctantSpec as ridge  # noqa: E402

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else ["build", "V001"]
MODE, VERSION = ARGS[0], ARGS[1]
OUT = ROOT / ("Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Monolith_" + VERSION)
DUMP = ROOT / "Saved/Profiling/Graphics/Phase01/Monolith_V001/Dump_M1_V002.json"
# 보존 구조물은 Floor(걷는 윗면)·Shell(가파른 면)이 함께 닫힌 형상이다. 바위 높이·기존 형상 판정에는 둘 다 쓴다.
ROCK = ("SM_Octant_Ridge_00_V012_Z1MassifCover_V012_R30000", "SM_Ridge_00_V012_PreservedRocks_Shell",
        "SM_Ridge_00_V012_PreservedRocks_Floor")
WALK = ("SM_Ridge_00_V012_PreservedRocks_Floor", "CONN_StairRamp3_V011")

RULES = {
    "V001": {
        "seed": 61207, "grid_cm": 50, "raster_cm": 25,
        # 외피 범위(옥탄트 로컬). 남쪽은 동굴 덩어리(M2) 남면을 건드리지 않도록 -5400에서 끊는다.
        "region": [-7950, -6250, -5400, -900], "edge_taper_cm": 250,
        # 팽창 반경: 서쪽(착지면·StairRamp3) 0.45m → 동쪽 1.7m. 층 띠 주기 2.7m, 진폭 38%.
        "radius_west_cm": 45, "radius_east_cm": 170, "radius_split_x": [-7650, -7300], "radius_noise": 0.3,
        "strata": {"period_cm": 270, "amplitude": 0.38}, "max_radius_cm": 260,
        "sink_cm": 30, "skin_min_cm": 15, "walk_skirt_cm": 80, "thickness_cm": 150,
        "keepout_spaces": ["StairRamp2CentralPassage", "StairRamp3CentralPassage", "StairRamp4CentralPassage"],
        "keepout_margin_cm": 110,
        # 탑 꼭대기: 43m 이상. 가로 균열 y = -2650 + 0.15(x+7300), 반폭 70cm. 북 덩이 +2.2m, 남 덩이 +1.4m.
        "top_h_cm": 4300, "crack": {"y0": -2650, "slope": 0.15, "half_cm": 70}, "lobes_cm": [140, 220],
        "lift_cm": [35, 25],
        "cap": {"gap_cm": 320, "height_cm": 750, "half_cm": [260, 420], "tilt_deg": 6.0, "yaw_deg": 8.0},
        "chips": [{"offset_cm": [380, 150, 100], "size_cm": 170}, {"offset_cm": [330, -260, 220], "size_cm": 120},
                  {"offset_cm": [-120, 420, 60], "size_cm": 95}],
    },
}
# V002(V001 화면 검토): 꼭대기 변화가 약하고 둥근 모노리스가 알처럼 보였다.
# - 꼭대기를 V자 균열로 갈라 남 조각 +6m·북 조각 +11m로 세운다(가장자리 3m 안쪽에서 어깨가 깎여 위가 좁아진다).
# - 모노리스는 모서리만 깎은 각진 판(상자)으로 바꾸고 남 조각 위에도 작은 판을 띄운다.
# - 동쪽 매끈한 절벽 면에서 2m 떨어져 뜬 석판 2장(면이 갈라져 떨어져 나온 표현)을 더한다.
RULES["V002"] = json.loads(json.dumps(RULES["V001"]))
RULES["V002"].update({
    "crack": {"y0": -2650, "slope": 0.15, "half_cm": 90, "notch_cm": 380}, "lobes_cm": [600, 1100],
    "lobe_shoulder_cm": 300, "lobe_jag": 0.12,
    "caps": [{"lobe": "north", "gap_cm": 350, "half_cm": [230, 380, 450], "tilt_deg": 8.0, "yaw_deg": 9.0},
             {"lobe": "south", "gap_cm": 260, "half_cm": [150, 210, 230], "tilt_deg": -6.0, "yaw_deg": -14.0}],
    "face_slabs": [{"y_cm": -3150, "h_cm": 3300, "gap_cm": 200, "half_cm": [75, 330, 500], "lean_deg": 5.0,
                    "yaw_deg": 6.0},
                   {"y_cm": -1950, "h_cm": 4300, "gap_cm": 220, "half_cm": [65, 260, 400], "lean_deg": 7.0,
                    "yaw_deg": -8.0}],
})


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def raster(tris, x0, y0, nx, ny, step, mode):
    """로컬 투영 삼각형을 격자에 래스터한다. mode=max는 가장 높은 면, min은 가장 낮은 면."""
    fill = -1e9 if mode == "max" else 1e9
    grid = np.full((nx, ny), fill)
    gx = x0 + step * np.arange(nx)
    gy = y0 + step * np.arange(ny)
    for tri in tris:
        a, b, c = (np.asarray(ridge.local(p)) for p in tri)
        lo = np.minimum(np.minimum(a, b), c)
        hi = np.maximum(np.maximum(a, b), c)
        i0, i1 = np.searchsorted(gx, lo[0]), np.searchsorted(gx, hi[0], "right")
        j0, j1 = np.searchsorted(gy, lo[1]), np.searchsorted(gy, hi[1], "right")
        if i0 >= i1 or j0 >= j1:
            continue
        X, Y = np.meshgrid(gx[i0:i1], gy[j0:j1], indexing="ij")
        v0, v1 = b[:2] - a[:2], c[:2] - a[:2]
        den = v0[0] * v1[1] - v0[1] * v1[0]
        if abs(den) < 1e-6:
            continue
        wx, wy = X - a[0], Y - a[1]
        u = (wx * v1[1] - wy * v1[0]) / den
        v = (v0[0] * wy - v0[1] * wx) / den
        ok = (u >= -1e-4) & (v >= -1e-4) & (u + v <= 1 + 1e-4)
        h = a[2] + u * (b[2] - a[2]) + v * (c[2] - a[2])
        block = grid[i0:i1, j0:j1]
        if mode == "max":
            block[ok] = np.maximum(block[ok], h[ok])
        else:
            block[ok] = np.minimum(block[ok], h[ok])
    return grid


def filt(grid, mode, k=1):
    """k칸 정사각 최대/최소 필터(정의 안 된 칸은 무시)."""
    out = grid.copy()
    for di in range(-k, k + 1):
        for dj in range(-k, k + 1):
            sh = np.roll(np.roll(grid, di, 0), dj, 1)
            out = np.maximum(out, sh) if mode == "max" else np.minimum(out, sh)
    return out


def noise2(x, y, phase):
    return (0.55 * np.sin(x / 520 + phase[0]) * np.cos(y / 610 + phase[1]) + 0.3 * np.sin((x + y) / 230 + phase[2])
            + 0.15 * np.sin((x - 2 * y) / 97 + phase[3]))


def crack_y(rules, x):
    c = rules["crack"]
    return c["y0"] + c["slope"] * (x + 7300)


def shroud(rules, rng):
    """높이장 팽창 외피. 반환: 정점 격자(로컬 x, y, 윗면 H, 아랫면 H), 메타."""
    dump = json.loads(DUMP.read_text(encoding="utf-8"))
    tri_of = lambda names: [t for n in names for t in dump["actors"][n]["triangles"]]
    x0, x1, y0, y1 = rules["region"]
    g, rs = rules["grid_cm"], rules["raster_cm"]
    pad = int(math.ceil((rules["max_radius_cm"] + 100) / g)) * g
    ox, oy = x0 - pad, y0 - pad
    nx, ny = int(round((x1 - x0 + 2 * pad) / rs)) + 1, int(round((y1 - y0 + 2 * pad) / rs)) + 1
    # 보존 구조물 윗면과 착지면 판 사이 좁은 틈(x -7800 부근 약 50cm)에서는 가장 높은 면이 동굴 천장이 된다.
    # 1.5m보다 좁은 틈은 닫힘 필터(최대 → 최소 ±75cm)로 메워 외피 아랫면이 동굴로 내려가지 않게 한다.
    close = lambda a: filt(filt(a, "max", 3), "min", 3)
    rock_raw = close(raster(tri_of(ROCK), ox, oy, nx, ny, rs, "max"))
    walk_raw = raster(tri_of(WALK), ox, oy, nx, ny, rs, "max")
    # 드러나는 외피는 최대 필터(±25cm) 높이를 감싸고, 묻는 자리·아랫면은 최소 필터(±50cm) 높이를 쓴다.
    # 50cm 격자 삼각형이 가파른 사면 위로 뜨거나 기존 면을 뚫지 않게 하는 보수값이다.
    rock = filt(rock_raw, "max")
    comb_raw = np.maximum(rock_raw, walk_raw)
    low = np.where(comb_raw > -1e8, filt(np.where(comb_raw > -1e8, comb_raw, 1e9), "min", 2), -1e9)
    walk = np.where(walk_raw > -1e8, filt(np.where(walk_raw > -1e8, walk_raw, 1e9), "min", 2), -1e9)
    comb = np.maximum(rock, walk)
    step = g // rs
    # 50cm 정점 격자(패딩 포함) 위 값.
    E = rock[::step, ::step]
    W = walk[::step, ::step]
    C = comb[::step, ::step]
    L = low[::step, ::step]
    gx = ox + g * np.arange(E.shape[0])
    gy = oy + g * np.arange(E.shape[1])
    X, Y = np.meshgrid(gx, gy, indexing="ij")
    phase = [rng.uniform(0, math.tau) for _ in range(8)]
    # 범위 가장자리에서 팽창을 0으로 줄인다.
    edge = np.minimum.reduce([X - x0, x1 - X, Y - y0, y1 - Y])
    taper = np.clip(edge / rules["edge_taper_cm"], 0, 1)
    taper = taper * taper * (3 - 2 * taper)
    sx0, sx1 = rules["radius_split_x"]
    east = np.clip((X - sx0) / (sx1 - sx0), 0, 1)
    east = east * east * (3 - 2 * east)
    base_r = (rules["radius_west_cm"] + (rules["radius_east_cm"] - rules["radius_west_cm"]) * east) \
        * (1 + rules["radius_noise"] * noise2(X, Y, phase[:4])) * taper
    in_crack = np.abs(Y - crack_y(rules, X)) < rules["crack"]["half_cm"]
    base_r = np.where(in_crack, 5.0, base_r)
    st = rules["strata"]
    kmax = int(rules["max_radius_cm"] // g) + 1
    S = np.where(E > -1e8, E, -1e9)
    for di in range(-kmax, kmax + 1):
        for dj in range(-kmax, kmax + 1):
            d = g * math.hypot(di, dj)
            if d == 0 or d > rules["max_radius_cm"]:
                continue
            Eq = np.roll(np.roll(E, -di, 0), -dj, 1)  # 이웃 q = v + (di, dj)
            band = np.tanh(3 * np.sin(math.tau * Eq / st["period_cm"] + 0.8 * np.sin(Y / 900 + phase[4])))
            r = base_r * (1 + st["amplitude"] * band)
            S = np.where((d <= r) & (Eq > S), Eq, S)
    # 탑 꼭대기: 기존보다 살짝 올리고 균열 양쪽을 두 덩이로 만든다.
    top = E >= rules["top_h_cm"]
    north = Y > crack_y(rules, X) + rules["crack"]["half_cm"]
    if "lobe_shoulder_cm" in rules:
        # 꼭대기 영역 가장자리에서 안쪽 거리(50cm 침식 반복)로 어깨를 깎고, 균열에서 notch 폭만큼 V자로 올린다.
        inner = np.zeros(E.shape)
        mask = top.copy()
        for _ in range(int(rules["lobe_shoulder_cm"] // g)):
            inner += mask
            mask = mask & np.roll(mask, 1, 0) & np.roll(mask, -1, 0) & np.roll(mask, 1, 1) & np.roll(mask, -1, 1)
        w_edge = np.clip(inner * g / rules["lobe_shoulder_cm"], 0, 1) ** 0.7
        dc = np.abs(Y - crack_y(rules, X)) - rules["crack"]["half_cm"]
        w_crack = np.clip(dc / rules["crack"]["notch_cm"], 0, 1)
        w_crack = w_crack * w_crack * (3 - 2 * w_crack)
        jag = 1 + rules["lobe_jag"] * noise2(X * 2.3, Y * 2.3, phase[2:6])
        lobe = np.where(north, rules["lobes_cm"][1], rules["lobes_cm"][0]) * w_edge * w_crack * jag
    else:
        lobe_w = np.clip((E - rules["top_h_cm"]) / 600, 0, 1)
        lobe = np.where(north, rules["lobes_cm"][1], rules["lobes_cm"][0]) * lobe_w * lobe_w * (3 - 2 * lobe_w)
    lift = rules["lift_cm"][0] + rules["lift_cm"][1] * noise2(X, Y, phase[4:])
    S_top = np.where(in_crack, E + 4, E + lift + lobe)
    S = np.where(top, np.maximum(S, S_top), S)
    # 착지면·경사로 위: 벽 치마(80cm 넘게 솟은 곳)만 남기고 걷는 면 아래로 묻는다.
    walk_here = W > -1e8
    S = np.where(walk_here & (S < W + rules["walk_skirt_cm"]), W - rules["sink_cm"], S)
    # 넓은 평지(외피가 기존 표면보다 15cm 미만으로만 뜨는 곳)는 묻는다.
    flat = (~walk_here) & (~top) & (S - C < rules["skin_min_cm"])
    S = np.where(flat, L - rules["sink_cm"], S)
    # 보호 통로 띠(중앙선 ± 반폭 + 여유) 안은 외피를 묻는다. 기존 형상이 이미 차지한 자리는 그대로 드러난다.
    spaces = {c["id"]: c for c in json.loads(ib.TRIAL.read_text(encoding="utf-8"))["protected_spaces"]}
    keep_out = np.zeros(X.shape, dtype=bool)
    for sid in rules.get("keepout_spaces", []):
        pts, halves = spaces[sid]["points_xyh_cm"], spaces[sid]["half_width_cm"]
        for (ax_, ay_, _), (bx_, by_, _), ha, hb in zip(pts, pts[1:], halves, halves[1:]):
            dx, dy = bx_ - ax_, by_ - ay_
            tt = np.clip(((X - ax_) * dx + (Y - ay_) * dy) / (dx * dx + dy * dy), 0, 1)
            dist = np.hypot(X - ax_ - tt * dx, Y - ay_ - tt * dy)
            keep_out |= dist <= ha + (hb - ha) * tt + rules["keepout_margin_cm"]
    S = np.where(keep_out, np.minimum(S, L - rules["sink_cm"]), S)
    B = np.minimum(L, S) - rules["thickness_cm"]
    # 범위 정점만 남긴다.
    keep_i = np.where((gx >= x0 - 1) & (gx <= x1 + 1))[0]
    keep_j = np.where((gy >= y0 - 1) & (gy <= y1 + 1))[0]
    sl = np.ix_(keep_i, keep_j)
    assert (E[sl] > -1e8).all(), "rock height missing inside region"
    meta = {"dump_sha256": sha(DUMP), "grid": [len(keep_i), len(keep_j)],
            "exposed_vertices": int(((S[sl] - C[sl]) > 0).sum()),
            "max_offset_cm": float((S[sl] - C[sl]).max()), "max_top_h_cm": float(S[sl].max())}
    return X[sl], Y[sl], S[sl], B[sl], E[sl], C[sl], meta


def heightfield_solid(X, Y, S, B):
    """윗면 S·아랫면 B 격자와 가장자리 벽으로 닫힌 메시를 만든다(월드 좌표)."""
    ni, nj = X.shape
    verts = []
    top_idx = np.zeros((ni, nj), dtype=np.int64)
    bot_idx = np.zeros((ni, nj), dtype=np.int64)
    for i in range(ni):
        for j in range(nj):
            top_idx[i, j] = len(verts)
            verts.append(ib.world(X[i, j], Y[i, j], S[i, j]))
            bot_idx[i, j] = len(verts)
            verts.append(ib.world(X[i, j], Y[i, j], B[i, j]))
    tris = []
    for i in range(ni - 1):
        for j in range(nj - 1):
            a, b, c, d = top_idx[i, j], top_idx[i + 1, j], top_idx[i + 1, j + 1], top_idx[i, j + 1]
            tris += [(a, b, c), (a, c, d)]
            a, b, c, d = bot_idx[i, j], bot_idx[i + 1, j], bot_idx[i + 1, j + 1], bot_idx[i, j + 1]
            tris += [(a, c, b), (a, d, c)]
    ring = [(i, 0) for i in range(ni)] + [(ni - 1, j) for j in range(1, nj)] + \
           [(i, nj - 1) for i in range(ni - 2, -1, -1)] + [(0, j) for j in range(nj - 2, 0, -1)]
    for (i0, j0), (i1, j1) in zip(ring, ring[1:] + ring[:1]):
        a, b = top_idx[i0, j0], top_idx[i1, j1]
        c, d = bot_idx[i1, j1], bot_idx[i0, j0]
        tris += [(a, d, c), (a, c, b)]
    return np.asarray(verts), np.asarray(tris)


def monolith(center, bottom_h, rules, rng):
    """기울어진 각진 덩어리: 모서리를 깎은 8각 고리를 높이별로 쌓은 닫힌 몸체(로컬 → 월드)."""
    cap = rules["cap"]
    ax, ay = cap["half_cm"]
    yaw, tilt = math.radians(cap["yaw_deg"]), math.radians(cap["tilt_deg"])
    corners = [(1, 0.55), (0.55, 1), (-0.55, 1), (-1, 0.6), (-1, -0.55), (-0.6, -1), (0.55, -1), (1, -0.6)]
    phase = [rng.uniform(0, math.tau) for _ in range(6)]
    levels = [0.0, 0.06, 0.18, 0.35, 0.5, 0.65, 0.8, 0.92, 1.0]
    rings, verts = [], []
    for t in levels:
        # 아래·위 끝은 조금 좁혀 덩어리 모서리를 둥글린다. 층 띠로 옆면에 단을 준다.
        shrink = 0.86 + 0.14 * math.sin(math.pi * min(1.0, t * 1.15)) ** 0.6
        band = 1 + 0.05 * math.tanh(3 * math.sin(t * 9.0 + phase[0]))
        ring = []
        for k, (u, v) in enumerate(corners):
            n = 1 + 0.07 * math.sin(3 * k + phase[1] + 4 * t) + 0.04 * math.sin(5 * k + phase[2])
            lx, ly = u * ax * shrink * band * n, v * ay * shrink * band * n
            # 윗면은 동쪽으로 기울고, 밑면 가운데는 처진다.
            h = bottom_h + t * cap["height_cm"] + lx * math.tan(tilt) * t
            rx, ry = lx * math.cos(yaw) - ly * math.sin(yaw), lx * math.sin(yaw) + ly * math.cos(yaw)
            ring.append(len(verts))
            verts.append(ib.world(center[0] + rx, center[1] + ry, h))
        rings.append(ring)
    bottom = len(verts)
    verts.append(ib.world(center[0], center[1], bottom_h - 60))
    top = len(verts)
    verts.append(ib.world(center[0], center[1], bottom_h + cap["height_cm"] + 15))
    m = len(corners)
    tris = []
    for j in range(m):
        k = (j + 1) % m
        tris.append((bottom, rings[0][k], rings[0][j]))
        for r0, r1 in zip(rings, rings[1:]):
            tris += [(r0[j], r0[k], r1[k]), (r0[j], r1[k], r1[j])]
        tris.append((rings[-1][j], rings[-1][k], top))
    return np.asarray(verts), np.asarray(tris)


def box_body(center, axes, half, rng, chamfer=0.18):
    """모서리를 깎은 8각 단면을 축 C 방향으로 쌓은 각진 닫힌 판(로컬 xyh 중심·축 → 월드)."""
    A, B, Cv = (np.asarray(a, dtype=float) for a in axes)
    a, b, c = half
    oct_uv = [(a, b * (1 - chamfer)), (a * (1 - chamfer), b), (-a * (1 - chamfer), b), (-a, b * (1 - chamfer)),
              (-a, -b * (1 - chamfer)), (-a * (1 - chamfer), -b), (a * (1 - chamfer), -b), (a, -b * (1 - chamfer))]
    phase = [rng.uniform(0, math.tau) for _ in range(4)]
    levels = [-1.0, -0.93, -0.5, 0.0, 0.5, 0.93, 1.0]
    rings, verts = [], []
    for t in levels:
        ring = []
        for k, (u, v) in enumerate(oct_uv):
            n = 1 + 0.03 * math.sin(3 * k + phase[0] + 2 * t) + 0.02 * math.sin(5 * k + phase[1])
            sc = (0.9 if abs(t) == 1.0 else 1.0) * n
            q = np.asarray(center) + A * u * sc + B * v * sc + Cv * c * t
            ring.append(len(verts))
            verts.append(ib.world(*q))
        rings.append(ring)
    lo, hi = len(verts), len(verts) + 1
    verts.append(ib.world(*(np.asarray(center) - Cv * c * 1.02)))
    verts.append(ib.world(*(np.asarray(center) + Cv * c * 1.02)))
    m = len(oct_uv)
    tris = []
    for j in range(m):
        k = (j + 1) % m
        tris.append((lo, rings[0][k], rings[0][j]))
        for r0, r1 in zip(rings, rings[1:]):
            tris += [(r0[j], r0[k], r1[k]), (r0[j], r1[k], r1[j])]
        tris.append((rings[-1][j], rings[-1][k], hi))
    return np.asarray(verts), np.asarray(tris)


def up_axes(yaw_deg, tilt_deg):
    """위로 선 판: A=판 가로(기울어 올라감), B=판 세로, C=판 높이(기울기만큼 눕는다)."""
    y, t = math.radians(yaw_deg), math.radians(tilt_deg)
    A = (math.cos(y) * math.cos(t), math.sin(y) * math.cos(t), math.sin(t))
    B = (-math.sin(y), math.cos(y), 0.0)
    C = (-math.cos(y) * math.sin(t), -math.sin(y) * math.sin(t), math.cos(t))
    return A, B, C


def face_axes(yaw_deg, lean_deg):
    """동쪽 면에 붙어 선 판: A=두께(동쪽), B=폭(남북), C=높이(위가 동쪽으로 기운다)."""
    y, l = math.radians(yaw_deg), math.radians(lean_deg)
    A = (math.cos(y) * math.cos(l), math.sin(y) * math.cos(l), -math.sin(l))
    B = (-math.sin(y), math.cos(y), 0.0)
    C = (math.cos(y) * math.sin(l), math.sin(y) * math.sin(l), math.cos(l))
    return A, B, C


def landmark_lines():
    """LandmarkTop 사선(WestEntry·CrownSummit·DeepBasin → 쌍둥이 첨탑 북쪽 꼭대기 +2m)의 50cm 표본(월드)."""
    crag = next(r for r in ridge.ROCKS if r["id"] == ridge.LANDMARK_ROCK)
    top = max(crag["outline"], key=lambda p: p[2])
    target = (top[0], top[1], ridge.ground(top[0], top[1]) + top[2] + 200)
    nodes = {n["id"]: n["xy"] for n in ridge.NODES}
    rows = {}
    for start in ("WestEntry", "CrownSummit", "DeepBasin"):
        x, y = nodes[start]
        a = np.asarray(ridge.point(x, y, ridge.ground(x, y) + 170))
        b = np.asarray(ridge.point(*target))
        n = int(np.linalg.norm(b - a) // 50)
        rows[start] = [a + (b - a) * k / n for k in range(n + 1)]
    return rows


def build():
    rules = RULES[VERSION]
    rng = random.Random(rules["seed"])
    ib.OUT = OUT
    OUT.mkdir(parents=True, exist_ok=True)
    import bpy
    bpy.ops.wm.read_factory_settings(use_empty=True)
    X, Y, S, B, E, C, meta = shroud(rules, rng)
    objects = []
    v, t = heightfield_solid(X, Y, S, B)
    objects.append({"obj": ib.make("SM_Ridge_MonolithShroud_%s" % VERSION, v, t), "role": "monolith_shroud"})
    north = (Y > crack_y(rules, X) + rules["crack"]["half_cm"]) & (E >= rules["top_h_cm"] + 600)
    assert north.any(), "north lobe not found"
    center = (float(X[north].mean()), float(Y[north].mean()))
    if "caps" in rules:
        # 각진 판: 각 조각 꼭대기(외피 윗면 최고점) 위 gap만큼 띄운다. 조각 범위는 꼭대기 영역·균열 남/북이다.
        verts, tris, placed = [], [], []
        for cap in rules["caps"]:
            side = Y > crack_y(rules, X) if cap["lobe"] == "north" else Y < crack_y(rules, X)
            lobe = side & (E >= rules["top_h_cm"] + 600)
            cx, cy = float(X[lobe].mean()), float(Y[lobe].mean())
            a, b, c = cap["half_cm"]
            base = float(S[lobe].max()) + cap["gap_cm"]
            lift = a * abs(math.sin(math.radians(cap["tilt_deg"])))
            cv, ct = box_body((cx, cy, base + c + lift), up_axes(cap["yaw_deg"], cap["tilt_deg"]), (a, b, c), rng)
            tris.append(ct + len(verts))
            verts.extend(cv.tolist())
            placed.append({"lobe": cap["lobe"], "center_xy_cm": [cx, cy], "bottom_h_cm": base, "half_cm": [a, b, c]})
            if cap["lobe"] == "north":
                center, bottom_h = (cx, cy), base
        objects.append({"obj": ib.make("SM_Ridge_MonolithCap_%s" % VERSION, np.asarray(verts), np.concatenate(tris)),
                        "role": "monolith_cap", "placed": placed})
        verts, tris, placed = [], [], []
        for slab in rules.get("face_slabs", []):
            # 그 남북 위치·높이에서 외피 동쪽 면(외피가 그 높이 이상인 가장 동쪽 정점) 바깥으로 gap만큼 띄운다.
            j = int(np.argmin(np.abs(Y[0, :] - slab["y_cm"])))
            col = np.where(S[:, j] >= slab["h_cm"])[0]
            x_face = float(X[col.max(), j]) + rules["grid_cm"] * 0.5
            a, b, c = slab["half_cm"]
            ctr = (x_face + slab["gap_cm"] + a, slab["y_cm"], slab["h_cm"])
            cv, ct = box_body(ctr, face_axes(slab["yaw_deg"], slab["lean_deg"]), (a, b, c), rng, chamfer=0.12)
            tris.append(ct + len(verts))
            verts.extend(cv.tolist())
            placed.append({"center_xyh_cm": list(ctr), "face_x_cm": x_face, "half_cm": [a, b, c]})
        if verts:
            objects.append({"obj": ib.make("SM_Ridge_MonolithSlabs_%s" % VERSION, np.asarray(verts),
                                           np.concatenate(tris)), "role": "monolith_slabs", "placed": placed})
    else:
        bottom_h = float(S[north].max()) + rules["cap"]["gap_cm"]
        v, t = monolith(center, bottom_h, rules, rng)
        objects.append({"obj": ib.make("SM_Ridge_MonolithCap_%s" % VERSION, v, t), "role": "monolith_cap",
                        "center_xyh_cm": [center[0], center[1], bottom_h]})
    verts, tris, placed = [], [], []
    for chip in rules["chips"]:
        dx, dy, dh = chip["offset_cm"]
        gap = rules["cap"]["gap_cm"] if "cap" in rules else rules["caps"][0]["gap_cm"]
        c = (center[0] + dx, center[1] + dy, bottom_h - gap * 0.5 + dh)
        cv, ct = ib.rock(c, chip["size_cm"], rng, squash=0.7, keel=1.25)
        tris.append(ct + len(verts))
        verts.extend(cv.tolist())
        placed.append({"center_xyh_cm": list(c), "size_cm": chip["size_cm"]})
    objects.append({"obj": ib.make("SM_Ridge_MonolithChips_%s" % VERSION, np.asarray(verts), np.concatenate(tris)),
                    "role": "monolith_chips", "placed": placed})

    # 기존 형상(지각·보존 구조물·착지면) 안의 표본은 새 점유가 아니다. 표본에서 현지 Up(구 중심 쪽)으로 광선을 쏴
    # 만난 기존 면 수가 홀수면 기존 형상 안이다. 지각은 보존 구조물 자리가 뚫린 한 장의 높이장 면이고 보존 구조물은
    # 닫힌 형상(동굴 빈 곳은 천장+윗면 두 번)이라 위쪽 광선 홀짝이 성립한다. 경사로 돌(CONN)은 바위 위에 얹힌
    # 면이라 홀짝을 깨므로 넣지 않는다. Floor는 Shell과 함께 닫힌 형상이라 ROCK에 들어 있다.
    dump = json.loads(DUMP.read_text(encoding="utf-8"))
    old = np.asarray([p for n in ROCK for tri in dump["actors"][n]["triangles"] for p in tri])
    old_bvh = ib.bvh_of(old, np.arange(len(old)).reshape(-1, 3))
    from mathutils import Vector

    def inside_existing(q):
        up = Vector(tuple(-np.asarray(q) / np.linalg.norm(q)))
        origin, count = Vector(tuple(q)), 0
        for _ in range(64):
            hit = old_bvh.ray_cast(origin, up)
            if hit[0] is None:
                break
            count += 1
            origin = hit[0] + up * 0.01
        return count % 2 == 1

    env = ib.helper()
    spaces = json.loads(ib.TRIAL.read_text(encoding="utf-8"))["protected_spaces"]
    samples = [(c["id"], env["samples"](c)) for c in spaces]
    lines = landmark_lines()
    rows, protected, sight = [], [], {}
    for row in objects:
        v, t = ib.arrays(row["obj"])
        bvh = ib.bvh_of(v, t)
        lo, hi = v.min(axis=0), v.max(axis=0)
        for sid, pts in samples:
            new_hits, existing = [], []
            for k, p in enumerate(pts):
                q = np.asarray(p)
                if (q < lo).any() or (q > hi).any() or not ib.inside(bvh, q):
                    continue
                p_local = ridge.local(tuple(q))
                (existing if inside_existing(q) else new_hits).append(
                    {"sample": k, "local_xyh_cm": [round(c, 1) for c in p_local]})
            if new_hits or existing:
                protected.append({"object": row["obj"].name, "space": sid, "new_hits": new_hits,
                                  "inside_existing": existing})
        for start, pts in lines.items():
            blocked = sum(1 for p in pts if (p >= lo).all() and (p <= hi).all() and ib.inside(bvh, p))
            near = min(float(np.linalg.norm(np.asarray(bvh.find_nearest(tuple(p))[0]) - p))
                       for p in pts[::4] if bvh.find_nearest(tuple(p))[0] is not None)
            sight.setdefault(start, {})[row["obj"].name] = {"blocked_samples": blocked, "min_distance_cm": round(near, 1)}
        exported = ib.export(row["obj"])
        exported.update({"role": row["role"], "isle": "Monolith", "collision": "NoCollision", "variant": None,
                         "placed": row.get("placed"), "center_xyh_cm": row.get("center_xyh_cm"),
                         "new_hits": sum(len(p["new_hits"]) for p in protected if p["object"] == row["obj"].name)})
        rows.append(exported)
    report = {"version": VERSION, "unit": "MonolithSkin", "rules": rules, "shroud": meta, "outputs": rows,
              "protected_spaces": len(spaces), "samples": sum(len(p) for _, p in samples), "protected": protected,
              "new_hits": sum(len(p["new_hits"]) for p in protected),
              "inside_existing": sum(len(p["inside_existing"]) for p in protected),
              "landmark_sight": sight,
              "topology_pass": all(r["topology"]["boundary_edges"] == 0 and r["topology"]["nonmanifold_edges"] == 0
                                   for r in rows),
              "inputs": {p.relative_to(ROOT).as_posix(): sha(p) for p in
                         (DUMP, ib.TRIAL, ib.HELPER, Path(__file__), Path(ib.__file__), ROOT / "Scripts/RidgeOctantSpec.py")}}
    (OUT / "Build.json").write_text(json.dumps(report, ensure_ascii=False, indent=1), encoding="utf-8")
    print("MONOLITH_SKIN_BUILD " + json.dumps({k: report[k] for k in ("version", "samples", "new_hits",
                                                                     "inside_existing", "topology_pass")}), flush=True)


if __name__ == "__main__":
    assert MODE == "build", MODE
    build()
