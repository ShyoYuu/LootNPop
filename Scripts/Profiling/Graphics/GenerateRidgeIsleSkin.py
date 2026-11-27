"""Phase 1-BR 부유섬 2개·하늘 석교 ED 외피 배치를 오프라인으로 생성하고 보호 공간 51개 표본을 검사한다.

실행(프로젝트 루트): python Scripts/Profiling/Graphics/GenerateRidgeIsleSkin.py V001
입력: RidgeBlueprint_V001.json(부유섬·석교 외곽선), ClosedBodyTrial_V001/BodyTrial.json(보호 공간 51개),
      IsleSkin_V001/MeshAnalysis.json(에디터 실측 메시 경계·열린 방향).
출력: IsleSkin_V001/IsleSkin_<판>.json(배치 원본), Saved/.../IsleSkin_V001/Check_<판>.json(표본 검사).

배치는 모두 시각 전용(NoCollision)이며 V011 보존 구조물(PreservedRocks Floor/Shell)의 충돌을 바꾸지 않는다.
보호 표본은 기존 정의·표본화(DiagnosePlacement.samples)를 그대로 쓰고, 메시 경계 상자(OBB)로 보수적으로 검사한다.
표본에 닿는 배치는 축척을 줄여 다시 시도하고, 그래도 닿으면 빼고 그 사실을 보고한다(보호 표본을 빼지 않는다).
"""
import hashlib
import json
import math
from pathlib import Path
import random
import sys

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "Scripts"))
import RidgeZ1CompositionSpec as spec  # noqa: E402

AUTHOR = HERE / "IsleSkin_V001"
EVIDENCE = ROOT / "Saved/Profiling/Graphics/Phase01/IsleSkin_V001"
HELPER = ROOT / "Saved/Profiling/Graphics/Phase01/U4_2_V002/DiagnosePlacement.py"
MEGA = "/Game/Megascans/3D_Assets/"
SMART = "/Game/SmartAssets/AssetMeshes/"

# 메시 별칭. 앞면 = 메시 +Y(MeshAnalysis area_normal의 반대), 열린 뒤 = -Y, 열린 바닥 = -Z.
MESH = {
    "HugeCliff02": MEGA + "HugeSandstoneCliff/SM_HugeSandstoneCliff_02",
    "HugeCliff05": MEGA + "HugeSandstoneCliff/SM_HugeSandstoneCliff_05",
    "HugeCliff07": MEGA + "HugeSandstoneCliff/SM_HugeSandstoneCliff_07",
    "MassCliff01": MEGA + "MassiveSandstoneCliff/SM_MassiveSandstoneCliff_01",
    "MassCliff02": MEGA + "MassiveSandstoneCliff/SM_MassiveSandstoneCliff_02",
    "MassCliff04": MEGA + "MassiveSandstoneCliff/SM_MassiveSandstoneCliff_04",
    "Mesa01": MEGA + "MassiveCanyonSandstoneMesa/SM_MassiveCanyonSandstoneMesa_01",
    "Nordic01": MEGA + "NordicBoulder/SM_NordicBoulder_01",
    "Nordic02": MEGA + "NordicBoulder/SM_NordicBoulder_02",
    "Cliff02": SMART + "SM_Cliff_02",
    "Cliff06": SMART + "SM_Cliff_06",
    "Cliff07": SMART + "SM_Cliff_07",
    "Cliff08": SMART + "SM_Cliff_08",
    "Cliff09": SMART + "SM_Cliff_09",
    "Cliff12": SMART + "SM_Cliff_12",
    "Cliff14": SMART + "SM_Cliff_14",
    "Cliff16": SMART + "SM_Cliff_16",
}

# 생성 규칙(판별). 길이 단위 cm, 깊이는 섬 윗면(또는 석교 상판) 아래 양수.
RULES = {
    "V001": {
        "seed": 51127,
        "isle_depth_cm": 2300, "isle_bottom_scale": 0.3,
        "rim": {"count": 14, "top_gap_cm": 30, "max_height_cm": 900, "target_width_cm": 1700,
                "meshes": ["HugeCliff07", "HugeCliff05", "HugeCliff02", "Cliff14", "Cliff12", "Mesa01"]},
        "slope": {"count": 12, "top_depth_cm": 600, "slant_cm": 1300, "outward_cm": 80,
                  "meshes": ["MassCliff02", "MassCliff01", "Cliff06", "MassCliff04", "Cliff16"]},
        "tip": {"ring_count": 4, "ring_radius_cm": 750, "ring_top_depth_cm": 1750, "ring_width_cm": 1000,
                "core_top_depth_cm": 2150, "core_mesh": "Cliff07", "core_scale": 0.8,
                "ring_meshes": ["Cliff09", "Cliff08", "Nordic01", "Cliff09"]},
        "cover": {"mesh": "Nordic02", "embed_cm": 100},
        "satellite": {"pairs": 4, "shards": 2, "radius_cm": [3700, 4600], "depth_cm": [500, 2200],
                      "pair_meshes": ["Cliff09", "Nordic01", "Cliff08", "Nordic02"], "pair_size_cm": [450, 900],
                      "shard_scale": [0.45, 0.8]},
        "bridge": {"keel_step_cm": 520, "keel_top_depth_cm": 470, "keel_max_width_cm": 560,
                   "keel_meshes": ["Cliff09", "Nordic01", "Cliff08", "Cliff09", "Cliff07"],
                   "pendants": [0.35, 0.62], "pendant_scale": 0.9,
                   "fragments": 3, "fragment_depth_cm": [900, 1600], "fragment_scale": [0.3, 0.45]},
        "fit_retries": 3, "fit_shrink": 0.88, "line_margin_cm": 150,
    },
}
# V002: 부유섬 ED 껍질(V001 판자형 테두리)을 빼고 Blender 닫힌 몸체(BuildRidgeIsleBody.py)로 바꾼다.
# 석교는 V001 구성을 두고 색이 어긋난 회색 NordicBoulder를 사암 바위로 바꾼다.
RULES["V002"] = json.loads(json.dumps(RULES["V001"]))
RULES["V002"].update({"isle_shells": False, "bodies": "Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Geometry_V002/Build.json",
                      "body_material": "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/U5aBody_V006/"
                                       "MI_Ridge_U5a_CapRock_V001"})
RULES["V002"]["bridge"]["keel_meshes"] = ["Cliff09", "Cliff08", "Cliff09", "Cliff07", "Cliff08"]
# V003: 몸체 해상도·지층·별도 갈래(Geometry_V003). 몸체 재질은 큰 면에서 밋밋하던 CapRock 대신
# 보존 구조물과 같은 경사 기반 지각 재질(Surface V003)로 맞춘다.
RULES["V003"] = json.loads(json.dumps(RULES["V002"]))
RULES["V003"].update({"bodies": "Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Geometry_V003/Build.json",
                      "body_material": "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/"
                                       "Z1MassifSurface_V003/MI_Ridge_Z1_MassifSurface"})
# V004: V003 형상 그대로. 서쪽 섬이 이음매 띠(55m)에 걸려 지각 재질의 구역 판정으로 보라색이 되던 문제를
# 구역 판정만 뺀 내부 구조물용 재질(IsleSkin_V001/Surface_V001)로 고친다. 보존 구조물 재질 교체에도 같은 재질을 쓴다.
ISLE_SURFACE = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/IsleSkin_V001/Surface_V001/MI_Ridge_IsleSurface"
RULES["V004"] = json.loads(json.dumps(RULES["V003"]))
RULES["V004"].update({"body_material": ISLE_SURFACE, "preserved_rock_material": ISLE_SURFACE})
# V005/V006(사용자 판정): 섬 갈래 제거·석교 Cliff02 첨탑·파편 제거, 석교 자연 형상 몸체 추가(Geometry_V005).
# V005는 A(SkyBridgeDeck 보호 띠 유지), V006은 B(석교 자신의 외피가 옆 띠를 차지, 표본 침범은 REVIEW로 기록).
RULES["V005"] = json.loads(json.dumps(RULES["V004"]))
RULES["V005"]["bridge"].update({"pendants": [], "fragments": 0})
RULES["V005"].update({"bodies": "Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Geometry_V005/Build.json",
                      "body_variant": "A"})
RULES["V006"] = json.loads(json.dumps(RULES["V005"]))
RULES["V006"].update({"body_variant": "B", "review_hits": "SkyBridgeDeck side band (-450cm, half 350cm) occupied by "
                                                         "the bridge's own skin; pending user decision"})


# ---- 수학 ---------------------------------------------------------------------------------------------------------

def add(a, b): return [x + y for x, y in zip(a, b)]
def sub(a, b): return [x - y for x, y in zip(a, b)]
def mul(a, s): return [x * s for x in a]
def dot(a, b): return sum(x * y for x, y in zip(a, b))
def cross(a, b): return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]
def norm(a):
    s = math.sqrt(dot(a, a))
    return [x / s for x in a]


def rot_from_basis(X, Y, Z):
    """UE FMatrix::Rotator와 같은 식으로 (X, Y, Z) 열 기저에서 pitch·yaw·roll(도)을 구한다."""
    pitch = math.degrees(math.atan2(X[2], math.hypot(X[0], X[1])))
    yaw = math.degrees(math.atan2(X[1], X[0]))
    sy = [-math.sin(math.radians(yaw)), math.cos(math.radians(yaw)), 0.0]
    roll = math.degrees(math.atan2(dot(Z, sy), dot(Y, sy)))
    return [pitch, yaw, roll]


def quat_from_rot(pitch, yaw, roll):
    """UE FRotator::Quaternion."""
    p, y, r = (math.radians(v) * 0.5 for v in (pitch, yaw, roll))
    sp, cp, sy, cy, sr, cr = math.sin(p), math.cos(p), math.sin(y), math.cos(y), math.sin(r), math.cos(r)
    return [cr*sp*sy - sr*cp*cy, -cr*sp*cy - sr*cp*sy, cr*cp*sy - sr*sp*cy, cr*cp*cy + sr*sp*sy]


def basis(yaw_out, tilt=0.0, spin=0.0, flip=False):
    """현지 (x, y, up) 기저. 메시 +Y(앞면)를 yaw_out 방향 수평에서 아래로 tilt만큼 숙인다.
    spin은 앞면 축 둘레 회전, flip은 X축 둘레 180°(바닥이 위로)."""
    c, s = math.cos(math.radians(yaw_out)), math.sin(math.radians(yaw_out))
    t = math.radians(tilt)
    Y = [c * math.cos(t), s * math.cos(t), -math.sin(t)]
    Z = [c * math.sin(t), s * math.sin(t), math.cos(t)]
    X = cross(Y, Z)
    if spin:
        a = math.radians(spin)
        X, Z = add(mul(X, math.cos(a)), mul(Z, math.sin(a))), add(mul(Z, math.cos(a)), mul(X, -math.sin(a)))
    if flip:
        Y, Z = mul(Y, -1), mul(Z, -1)
    return X, Y, Z


# ---- 입력 ---------------------------------------------------------------------------------------------------------

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_bounds():
    rows = json.loads((AUTHOR / "MeshAnalysis.json").read_text(encoding="utf-8"))["rows"]
    by_path = {r["mesh"]: r for r in rows if not r.get("missing")}
    out = {}
    for key, path in MESH.items():
        r = by_path[path]
        out[key] = {"min": r["bounds_min"], "max": r["bounds_max"], "boundary_edges": r["boundary_edges"]}
    return out


def helper():
    env = {}
    source = HELPER.read_text(encoding="utf-8").split("\nraw =")[0]
    exec(compile(source, str(HELPER), "exec"), env)
    return env


# ---- 배치 ---------------------------------------------------------------------------------------------------------

class Placer:
    def __init__(self, bounds):
        self.bounds = bounds
        self.rows = []

    def put(self, label, group, key, X, Y, Z, scale, anchor, target):
        """메시 로컬 anchor 점(cm, 축척 전)이 현지 target(x, y, H)에 오도록 피벗을 정한다. 기저는 현지 (x, y, up)."""
        offset = add(add(mul(X, anchor[0] * scale), mul(Y, anchor[1] * scale)), mul(Z, anchor[2] * scale))
        pivot = sub(target, offset)
        self.rows.append({"label": label, "group": group, "mesh_key": key, "mesh": MESH[key],
                          "local_xyh_cm": [round(v, 3) for v in pivot], "basis": [X, Y, Z],
                          "rotation_deg": [round(v, 6) for v in rot_from_basis(X, Y, Z)],
                          "scale": [round(scale, 5)] * 3, "anchor": anchor, "target": target})

    def box(self, key):
        return self.bounds[key]


def ray_radius(outline, center, theta):
    """중심에서 theta 방향 광선이 외곽선 다각형과 만나는 거리."""
    d = [math.cos(theta), math.sin(theta)]
    best = None
    for a, b in zip(outline, outline[1:] + outline[:1]):
        e = [b[0] - a[0], b[1] - a[1]]
        w = [a[0] - center[0], a[1] - center[1]]
        den = d[0] * e[1] - d[1] * e[0]
        if abs(den) < 1e-9:
            continue
        t = (w[0] * e[1] - w[1] * e[0]) / den
        u = (w[0] * d[1] - w[1] * d[0]) / den
        if t > 0 and 0 <= u <= 1:
            best = t if best is None else min(best, t)
    return best


def isle(placer, rng, rules, rid, center, outline, top, covers):
    depth = rules["isle_depth_cm"]
    shrink = 1 - rules["isle_bottom_scale"]
    slope = math.atan(shrink * 2900 / depth)  # 옆면이 수직에서 기운 각(약 41°)

    def surface(theta, d):
        r = ray_radius(outline, center, theta) * (1 - shrink * d / depth)
        return [center[0] + r * math.cos(theta), center[1] + r * math.sin(theta)]

    # 1) 윗면 테두리 수직 띠: 앞면 평면 = 외곽선, 꼭대기 = 윗면 - top_gap.
    rim = rules["rim"]
    for i in range(rim["count"]):
        theta = math.tau * i / rim["count"] + rng.uniform(-0.08, 0.08)
        key = rim["meshes"][i % len(rim["meshes"])]
        lo, hi = placer.box(key)
        w, h = hi[0] - lo[0], hi[2] - lo[2]
        s = max(0.45, min(1.1, rim["target_width_cm"] / w, rim["max_height_cm"] / h))
        X, Y, Z = basis(math.degrees(theta), 0.0)
        xy = surface(theta, 0)
        placer.put("%s_Rim%02d" % (rid, i), rid + "/Rim", key, X, Y, Z, s,
                   [(lo[0] + hi[0]) / 2 + rng.uniform(-0.1, 0.1) * w, hi[1], hi[2]],
                   [xy[0], xy[1], top - rim["top_gap_cm"]])

    # 2) 옆면 경사 띠: 앞면이 바깥·아래(옆면 법선), 메시 위쪽이 옆면을 따라 위로. 중간 평면을 옆면에 둔다.
    sl = rules["slope"]
    for i in range(sl["count"]):
        theta = math.tau * (i + 0.5) / sl["count"] + rng.uniform(-0.06, 0.06)
        key = sl["meshes"][i % len(sl["meshes"])]
        lo, hi = placer.box(key)
        s = max(0.35, min(0.7, sl["slant_cm"] / (hi[2] - lo[2])))
        X, Y, Z = basis(math.degrees(theta), math.degrees(slope), spin=rng.uniform(-8, 8))
        d = sl["top_depth_cm"]
        xy = surface(theta, d)
        out = [math.cos(theta) * math.cos(slope) * sl["outward_cm"], math.sin(theta) * math.cos(slope) * sl["outward_cm"]]
        placer.put("%s_Slope%02d" % (rid, i), rid + "/Slope", key, X, Y, Z, s,
                   [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, hi[2]],
                   [xy[0] + out[0], xy[1] + out[1], top - d - math.sin(slope) * sl["outward_cm"]])

    # 3) 끝 덩어리: 바닥이 열린 덩어리 바위를 뒤집어(열린 면이 위 = 몸체 안) 매단다.
    tip = rules["tip"]
    for i in range(tip["ring_count"]):
        theta = math.tau * i / tip["ring_count"] + 0.4 + rng.uniform(-0.1, 0.1)
        key = tip["ring_meshes"][i % len(tip["ring_meshes"])]
        lo, hi = placer.box(key)
        s = min(1.0, tip["ring_width_cm"] / max(hi[0] - lo[0], hi[1] - lo[1]))
        X, Y, Z = basis(math.degrees(theta) + rng.uniform(-30, 30), rng.uniform(-12, 12), flip=True)
        r = tip["ring_radius_cm"]
        placer.put("%s_Tip%02d" % (rid, i), rid + "/Tip", key, X, Y, Z, s,
                   [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]],
                   [center[0] + r * math.cos(theta), center[1] + r * math.sin(theta), top - tip["ring_top_depth_cm"]])
    key = tip["core_mesh"]
    lo, hi = placer.box(key)
    X, Y, Z = basis(rng.uniform(0, 360), 0.0, flip=True)
    placer.put("%s_TipCore" % rid, rid + "/Tip", key, X, Y, Z, tip["core_scale"],
               [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]], [center[0], center[1], top - tip["core_top_depth_cm"]])

    # 4) 윗면 엄폐 각기둥 외피: 열린 바닥을 윗면 아래로 묻는다(충돌은 기존 각기둥).
    cov = rules["cover"]
    for cid, (cx, cy), size in covers:
        lo, hi = placer.box(cov["mesh"])
        s = min(1.4, 2.0 * size / max(hi[0] - lo[0], hi[1] - lo[1]))
        X, Y, Z = basis(rng.uniform(0, 360), 0.0)
        placer.put("%s_Cover" % cid, rid + "/Cover", cov["mesh"], X, Y, Z, s,
                   [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]], [cx, cy, top - cov["embed_cm"]])

    # 5) 중간 단계 부유석: 반경 29m 밖. 바닥이 열린 바위는 위아래 한 쌍으로 맞붙여 닫힌 덩어리로 보이게 한다.
    sat = rules["satellite"]
    for i in range(sat["pairs"] + sat["shards"]):
        theta = math.tau * (i + rng.uniform(0.1, 0.9)) / (sat["pairs"] + sat["shards"])
        radius = rng.uniform(*sat["radius_cm"])
        h = top - rng.uniform(*sat["depth_cm"])
        p = [center[0] + radius * math.cos(theta), center[1] + radius * math.sin(theta), h]
        if i < sat["pairs"]:
            key = sat["pair_meshes"][i % len(sat["pair_meshes"])]
            lo, hi = placer.box(key)
            s = rng.uniform(*sat["pair_size_cm"]) / max(hi[0] - lo[0], hi[1] - lo[1])
            yaw, tilt = rng.uniform(0, 360), rng.uniform(-15, 15)
            X, Y, Z = basis(yaw, tilt)
            bottom = [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]]
            placer.put("%s_Float%02dA" % (rid, i), rid + "/Float", key, X, Y, Z, s, bottom, p)
            X2, Y2, Z2 = basis(yaw + 37, tilt, flip=True)
            s2 = s * 0.75
            # 아래 짝은 위 짝 바닥보다 높이의 15%만큼 위로 겹쳐 열린 바닥끼리 가린다.
            overlap = 0.15 * (hi[2] - lo[2]) * s2
            placer.put("%s_Float%02dB" % (rid, i), rid + "/Float", key, X2, Y2, Z2, s2, bottom,
                       add(p, mul(Z, overlap)))
        else:
            lo, hi = placer.box("Cliff02")
            s = rng.uniform(*sat["shard_scale"])
            X, Y, Z = basis(rng.uniform(0, 360), rng.uniform(25, 70), spin=rng.uniform(0, 360))
            placer.put("%s_Shard%02d" % (rid, i), rid + "/Float", "Cliff02", X, Y, Z, s,
                       [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2], p)


def bridge(placer, rng, rules, deck):
    br = rules["bridge"]
    (ax, ay), (bx, by), ha, hb, half = deck
    length = math.hypot(bx - ax, by - ay)
    along = math.degrees(math.atan2(by - ay, bx - ax))
    count = int(length // br["keel_step_cm"])
    for i in range(count + 1):
        t = (i + 0.5) / (count + 1)
        key = br["keel_meshes"][i % len(br["keel_meshes"])]
        lo, hi = placer.box(key)
        # 긴 축을 다리 방향으로: 메시 X가 다리 방향이 되도록 앞면(+Y)을 옆으로 둔다.
        wide_y = (hi[1] - lo[1]) > (hi[0] - lo[0])
        yaw = along + (0 if wide_y else 90) + rng.choice([0, 180]) + rng.uniform(-6, 6)
        across = (hi[0] - lo[0]) if wide_y else (hi[1] - lo[1])
        s = min(0.9, br["keel_max_width_cm"] / across)
        X, Y, Z = basis(yaw, 0.0, flip=True)
        x, y = ax + (bx - ax) * t, ay + (by - ay) * t
        h = ha + (hb - ha) * t - br["keel_top_depth_cm"]
        placer.put("SkyBridge_Keel%02d" % i, "SkyBridge/Keel", key, X, Y, Z, s,
                   [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]], [x, y, h])
    for i, t in enumerate(br["pendants"]):
        lo, hi = placer.box("Cliff02")
        X, Y, Z = basis(along + rng.uniform(-20, 20), rng.uniform(-6, 6), flip=True)
        x, y = ax + (bx - ax) * t, ay + (by - ay) * t
        h = ha + (hb - ha) * t - br["keel_top_depth_cm"]
        # 뒤집었으므로 원래 바닥(lo z)이 위로 간다. 그 면을 상판 보호 부피 아래에 맞춘다.
        placer.put("SkyBridge_Pendant%02d" % i, "SkyBridge/Pendant", "Cliff02", X, Y, Z, br["pendant_scale"],
                   [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]], [x, y, h])
    for i in range(br["fragments"]):
        t = 0.25 + 0.5 * (i + rng.uniform(0.2, 0.8)) / br["fragments"]
        lo, hi = placer.box("Cliff02")
        side = rng.uniform(-400, 400)
        nx, ny = -(by - ay) / length, (bx - ax) / length
        x, y = ax + (bx - ax) * t + nx * side, ay + (by - ay) * t + ny * side
        h = ha + (hb - ha) * t - rng.uniform(*br["fragment_depth_cm"])
        X, Y, Z = basis(rng.uniform(0, 360), rng.uniform(30, 80), spin=rng.uniform(0, 360))
        placer.put("SkyBridge_Fragment%02d" % i, "SkyBridge/Fragment", "Cliff02", X, Y, Z,
                   rng.uniform(*br["fragment_scale"]), [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2],
                   [x, y, h])


# ---- 검사 ---------------------------------------------------------------------------------------------------------

def world_pose(env, row):
    x, y, h = row["local_xyh_cm"]
    p, qf = env["frame"](x, y, h, 0.0)
    ql = quat_from_rot(*row["rotation_deg"])
    return list(p), env["mul"](qf, ql)


def check_basis(env, row):
    """rotation_deg가 basis와 같은 회전인지 확인한다(UE 변환식 일치 검증)."""
    ql = quat_from_rot(*row["rotation_deg"])
    for axis, unit in zip(row["basis"], ([1, 0, 0], [0, 1, 0], [0, 0, 1])):
        v = env["rotate"](ql, unit)
        assert max(abs(a - b) for a, b in zip(v, axis)) < 1e-6, (row["label"], v, axis)


def hits(env, row, bounds, samples, line_margin):
    pos, q = world_pose(env, row)
    qi = env["inv"](q)
    s = row["scale"][0]
    lo, hi = bounds[row["mesh_key"]]["min"], bounds[row["mesh_key"]]["max"]
    found = []
    for sid, is_line, points in samples:
        m = line_margin if is_line else 0.0
        for i, p in enumerate(points):
            v = env["rotate"](qi, sub(list(p), pos))
            v = [c / s for c in v]
            if all(lo[k] - m / s <= v[k] <= hi[k] + m / s for k in range(3)):
                found.append({"space": sid, "sample": i})
    return found


def main(version):
    rules = RULES[version]
    rng = random.Random(rules["seed"])
    blueprint_path = HERE / "RidgeBlueprint_V001.json"
    trial_path = HERE / "ClosedBodyTrial_V001/BodyTrial.json"
    blueprint = json.loads(blueprint_path.read_text(encoding="utf-8"))
    spaces = json.loads(trial_path.read_text(encoding="utf-8"))["protected_spaces"]
    bounds = load_bounds()
    env = helper()
    placer = Placer({k: (v["min"], v["max"]) for k, v in bounds.items()})

    islands = {i["id"]: i for i in blueprint["preserve"]["islands"]}
    covers = {
        "ValleySkyIsle": [("ValleyIsleCoverA", (6700, -4900), 300), ("ValleyIsleCoverB", (8900, -6500), 350),
                          ("ValleyIsleCoverC", (7200, -7000), 280)],
        "WestSkyIsle": [("WestIsleCoverA", (-15700, 4100), 300), ("WestIsleCoverB", (-13500, 5700), 350),
                        ("WestIsleCoverC", (-14200, 3300), 280)],
    }
    centers = {"ValleySkyIsle": (7800, -5600), "WestSkyIsle": (-14600, 4800)}
    for rid in ("ValleySkyIsle", "WestSkyIsle"):
        if not rules.get("isle_shells", True):
            continue
        it = islands[rid]
        outline = [p[:2] for p in it["outline"]]
        isle(placer, rng, rules, rid, centers[rid], outline, it["island_top"], covers[rid])
    bridge(placer, rng, rules, blueprint["preserve"]["bridges"][0]["deck"])

    samples = [(c["id"], all(w == 0 for w in c.get("half_width_cm", [1])) and "disk_radius_cm" not in c,
                env["samples"](c)) for c in spaces]
    kept, dropped, refits = [], [], []
    for row in placer.rows:
        check_basis(env, row)
        found = hits(env, row, bounds, samples, rules["line_margin_cm"])
        tries = 0
        while found and tries < rules["fit_retries"]:
            # 앵커 점을 고정한 채 축척만 줄인다.
            tries += 1
            s = row["scale"][0] * rules["fit_shrink"]
            X, Y, Z = row["basis"]
            a = row["anchor"]
            offset = add(add(mul(X, a[0] * s), mul(Y, a[1] * s)), mul(Z, a[2] * s))
            row["local_xyh_cm"] = [round(v, 3) for v in sub(row["target"], offset)]
            row["scale"] = [round(s, 5)] * 3
            found = hits(env, row, bounds, samples, rules["line_margin_cm"])
        if found:
            spaces_hit = sorted({f["space"] for f in found})
            dropped.append({"label": row["label"], "spaces": spaces_hit, "samples": len(found)})
            continue
        if tries:
            refits.append({"label": row["label"], "shrinks": tries, "scale": row["scale"][0]})
        kept.append(row)

    out_rows = [{k: r[k] for k in ("label", "group", "mesh", "local_xyh_cm", "rotation_deg", "scale")} for r in kept]
    layout = {"schema": 1, "unit": "IsleSkin", "version": version, "rules": rules, "mesh_alias": MESH,
              "source_level": "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/U5aBody_V008/"
                              "LVI_Ridge_EDTerrain_U5aBody_V008",
              "work_level": "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/IsleSkin_{0}/"
                            "LVI_Ridge_EDTerrain_IsleSkin_{0}".format(version),
              "preserved_rock_material": rules.get("preserved_rock_material",
                                                   "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/"
                                                   "Z1MassifSurface_V003/MI_Ridge_Z1_MassifSurface"),
              "collision": "NoCollision", "tags": ["Decoration", "LNPDecoration", "IsleSkinTrial"],
              "inputs": {p.relative_to(ROOT).as_posix(): sha(p) for p in
                         (blueprint_path, trial_path, AUTHOR / "MeshAnalysis.json", HELPER, Path(__file__))},
              "placements": out_rows}
    if rules.get("bodies"):
        # Blender 몸체: 정점이 월드 좌표라 원점에 둔다. 보호 표본 검사는 Blender BVH가 했다(Build.json).
        build = json.loads((ROOT / rules["bodies"]).read_text(encoding="utf-8"))
        assert build["topology_pass"], "Blender body topology failed"
        chosen = [o for o in build["outputs"] if o.get("variant") in (None, rules.get("body_variant"))]
        body_hits = sum(o.get("new_hits", 0) for o in chosen)
        assert body_hits == 0 or rules.get("review_hits"), "Blender body protected-space hits: %d" % body_hits
        layout["review_hits"] = {"count": body_hits, "note": rules.get("review_hits")} if body_hits else None
        folder = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/IsleSkin_V001/Bodies_" + version
        layout["inputs"][rules["bodies"]] = sha(ROOT / rules["bodies"])
        layout["bodies"] = [{"label": o["name"].replace("SM_Ridge_", "BODY_"), "group": o["isle"] + "/Body",
                             "file": o["file"], "fbx_sha256": o["sha256"], "asset": folder + "/" + o["name"],
                             "triangles": o["triangles"], "bounds_cm": o["bounds_cm"], "role": o["role"],
                             "material": rules["body_material"]} for o in chosen]
    (AUTHOR / ("IsleSkin_%s.json" % version)).write_text(json.dumps(layout, ensure_ascii=False, indent=1) + "\n",
                                                          encoding="utf-8")
    groups = {}
    for r in kept:
        groups[r["group"]] = groups.get(r["group"], 0) + 1
    report = {"version": version, "generated": len(placer.rows), "kept": len(kept), "dropped": dropped,
              "refits": refits, "groups": groups, "protected_spaces": len(spaces),
              "samples": sum(len(p) for _, _, p in samples), "line_margin_cm": rules["line_margin_cm"],
              "check": "OBB of mesh bounds vs all protected samples; kept rows have zero hits"}
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / ("Check_%s.json" % version)).write_text(json.dumps(report, ensure_ascii=False, indent=1),
                                                         encoding="utf-8")
    print(json.dumps({k: v for k, v in report.items() if k not in ("refits",)}, ensure_ascii=False, indent=1))


if __name__ == "__main__":
    main(sys.argv[1])
