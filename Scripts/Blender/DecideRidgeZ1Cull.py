"""Z1 ED 시각 인스턴스 중 덜어낼 것을 규칙으로 판정한다(U6 러프 PIE 지적 정리). 결과는 UE `CullRidgeZ1Instances.py build`가 읽는다.

실행: blender --background --python Scripts/Blender/DecideRidgeZ1Cull.py -- <버전>
입력: Saved/.../AssemblyTest_20261119/Cull/Export_<버전>.json(UE `CullRidgeZ1Instances.py export`),
      큰 덩어리 형상은 U5 캐시(V012_Z1Collision_V001/cache/<키>/decimated.npz, Export_V034 메시 순서)를 쓴다.
출력: Cull/Decide_<버전>.json(소유 Level Instance별 제거 목록·규칙별 수·표본 위치).

규칙(U6 러프 PIE 2026-11-23 사용자 지적, 지각은 렌더링 지각 V012 = V010 + 경사로):
- C 동굴 절개: 중심 xy가 동굴 cut 다각형 안이면 뺀다(절개 안은 U5 충돌이 없어 보이기만 하고 통과된다).
- B 고원 밖 드레이프: 드레이프는 CR 섬 평면 그룹을 평탄 고원(Z1MassifCover V002)에 앉힌 층이다. 고원 가중치가
  PLATEAU_WEIGHT 미만인 자리(동굴 절개 둘레에 남은 V012 봉우리·받침)에 얹힌 인스턴스를 뺀다. 첫 판(V001 판정)의
  "반경 4m 지각 높이 폭 3m" 기준은 고원 위 ED 섬 +3m 둑 고리까지 잡아 드레이프의 61%를 지워 버려 바꿨다.
- A 받침: 땅에 닿은 것부터 이어서 따진다. 상자 바닥(현지 Up 최저점)이 지각 + SUPPORT_GAP_CM 안이면 땅에 닿았다.
  아니면 바닥 표본점 바로 아래(SUPPORT_GAP_CM)에 있는 큰 덩어리(식생·물·납작한 타일이 아닌 BIG_BODY_CM 이상)가
  받침이고, 받침 중 하나라도 땅에 닿아 있으면(반복 전파) 남긴다. 프랍끼리 받친 가지와, 큰 바위끼리 받치며 통째로 뜬 덮개가 빠진다.
- D 꽂힌 판: 얇은 인스턴스(두께/길이 < THIN_RATIO, 길이 ≥ THIN_MIN_CM)가 지각 THIN_NEAR_CM 안에 있고 얇은 축이
  지각 법선에서 THIN_ANGLE_DEG 넘게 벌어지면 뺀다.
데칼은 C·B만 본다.
"""
import json
import math
from pathlib import Path
import sys

import numpy as np
from mathutils.bvhtree import BVHTree

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parent)]
import BuildRidgeEDCollision as collision
import RidgeZ1CompositionSpec as spec

ROOT = HERE.parents[1]
OUT = ROOT / "Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/Cull"
U5 = ROOT / "Saved/SliceOctant/Ridge_00/V012_Z1Collision_V001"
U5_EXPORT = ROOT / "Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/Collision/Export_V034"
PLANT_NAMES = ("Vine", "Ivy", "Hornbeam", "MoneyPlant")
FLAT_RATIO = 0.35
PLATEAU_WEIGHT = 0.95
SUPPORT_GAP_CM, SUPPORT_LIFT_CM = 100.0, 30.0
BIG_BODY_CM = 400.0
THIN_RATIO, THIN_MIN_CM, THIN_NEAR_CM, THIN_ANGLE_DEG = 0.35, 150.0, 300.0, 60.0
GRID_CM = 100.0
PROBE_RADIUS_CM, PROBE_HEIGHT_CM = 600.0, 1200.0


class Crust:
    # 렌더링 지각 V012 높이를 1m 격자에 캐시하고 쌍선형 보간한다.
    def __init__(self):
        self.cache = {}

    def at(self, i, j):
        key = (i, j)
        if key not in self.cache:
            self.cache[key] = spec.massif_ramp_ground(i * GRID_CM, j * GRID_CM)
        return self.cache[key]

    def __call__(self, x, y):
        fx, fy = x / GRID_CM, y / GRID_CM
        i, j = math.floor(fx), math.floor(fy)
        u, v = fx - i, fy - j
        return ((self.at(i, j) * (1 - u) + self.at(i + 1, j) * u) * (1 - v)
                + (self.at(i, j + 1) * (1 - u) + self.at(i + 1, j + 1) * u) * v)

    def normal(self, x, y):
        p = np.array(spec.ridge.point(x, y, self(x, y)))
        a = np.array(spec.ridge.point(x + 100, y, self(x + 100, y))) - p
        b = np.array(spec.ridge.point(x, y + 100, self(x, y + 100))) - p
        n = np.cross(a, b)
        n /= np.linalg.norm(n)
        up = -p / np.linalg.norm(p)
        return n if n @ up > 0 else -n


def kind(path):
    name = path.rsplit("/", 1)[1]
    if "/3D_Plants/" in path or any(w in name for w in PLANT_NAMES):
        return "plant"
    if name.startswith("SM_WaterPlane"):
        return "water"
    return "mesh"


def quat_matrices(q):
    x, y, z, w = q[:, 0], q[:, 1], q[:, 2], q[:, 3]
    m = np.empty((len(q), 3, 3))
    m[:, 0, 0] = 1 - 2 * (y * y + z * z); m[:, 0, 1] = 2 * (x * y - z * w); m[:, 0, 2] = 2 * (x * z + y * w)
    m[:, 1, 0] = 2 * (x * y + z * w); m[:, 1, 1] = 1 - 2 * (x * x + z * z); m[:, 1, 2] = 2 * (y * z - x * w)
    m[:, 2, 0] = 2 * (x * z - y * w); m[:, 2, 1] = 2 * (y * z + x * w); m[:, 2, 2] = 1 - 2 * (x * x + y * y)
    return m


def big_body_bvh(data, inst, rot, big):
    # 큰 덩어리 인스턴스의 축소 메시(U5 캐시)를 world로 합친다. 캐시에 없는 메시는 상자로 대신한다.
    u5_manifest = json.loads((U5_EXPORT / "manifest.json").read_text(encoding="utf-8"))
    report = json.loads((U5 / "manifest.json").read_text(encoding="utf-8"))["report"]
    cache = np.load(U5 / "cache" / report["cache"] / "decimated.npz")
    low = {item["mesh"]: (cache["v%d" % k], cache["t%d" % k]) for k, item in enumerate(u5_manifest["meshes"])}
    verts, tris, owner, offset, boxed = [], [], [], 0, 0
    for idx in np.nonzero(big)[0]:
        path, lo, hi = data["meshes"][int(inst[idx, 0])]
        if path in low:
            v, t = low[path]
        else:
            lo, hi = np.array(lo), np.array(hi)
            v = np.array([[x, y, z] for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])])
            t = np.array([[0, 1, 3], [0, 3, 2], [4, 6, 7], [4, 7, 5], [0, 4, 5], [0, 5, 1], [2, 3, 7], [2, 7, 6],
                          [0, 2, 6], [0, 6, 4], [1, 5, 7], [1, 7, 3]])
            boxed += 1
        world = (v * inst[idx, 9:12]) @ rot[idx].T + inst[idx, 2:5]
        verts.append(world)
        tris.append(t + offset)
        owner.append(np.full(len(t), idx))
        offset += len(v)
    verts, tris, owner = np.concatenate(verts), np.concatenate(tris), np.concatenate(owner)
    return BVHTree.FromPolygons(verts.tolist(), tris.tolist(), all_triangles=True), owner, boxed


def main(version):
    data = json.loads((OUT / ("Export_%s.json" % version)).read_text(encoding="utf-8"))
    comps = data["components"]
    rows = np.array([[comps[r[0]][3], r[0], r[1]] + r[2:] for r in data["instances"]], dtype=np.float64)
    # rows: mesh, comp, index, px, py, pz, qx, qy, qz, qw, sx, sy, sz
    inst = np.concatenate([rows[:, [0]], rows[:, [1]], rows[:, 3:6], rows[:, 6:10], rows[:, 10:13]], axis=1)
    # inst: mesh, comp, px, py, pz, q(4), s(3) (BuildRidgeEDCollision 행과 같은 2:5·5:9·9:12 배치)
    n = len(inst)
    boxes = np.array([[m[1], m[2]] for m in data["meshes"]], dtype=np.float64)
    kinds = np.array([kind(m[0]) for m in data["meshes"]])
    mesh = inst[:, 0].astype(int)
    lo, hi = boxes[mesh, 0], boxes[mesh, 1]
    scale = inst[:, 9:12]
    ext = (hi - lo) * np.abs(scale)
    rot = quat_matrices(inst[:, 5:9])
    corners = np.stack([np.stack([lo[:, 0] if cx == 0 else hi[:, 0], lo[:, 1] if cy == 0 else hi[:, 1],
                                  lo[:, 2] if cz == 0 else hi[:, 2]], axis=1)
                        for cx in (0, 1) for cy in (0, 1) for cz in (0, 1)], axis=1)
    world = np.einsum("nij,nkj->nki", rot, corners * scale[:, None, :]) + inst[:, None, 2:5]
    center = world.mean(axis=1)
    local_c = collision.to_local(center)
    local_k = collision.to_local(world.reshape(-1, 3)).reshape(n, 8, 3)
    bottom = local_k[:, :, 2].min(axis=1)
    lowest = world[np.arange(n), local_k[:, :, 2].argmin(axis=1)]
    up = -center / np.linalg.norm(center, axis=1)[:, None]
    owner = np.array([comps[int(c)][0] for c in inst[:, 1]])
    cat = kinds[mesh]
    flat = (ext[:, 2] < FLAT_RATIO * np.maximum(ext[:, 0], ext[:, 1])) & (cat == "mesh")
    crust = Crust()
    ground = np.array([crust(x, y) for x, y in local_c[:, :2]])
    print("Z1_CULL_STAGE ground", flush=True)

    rule = np.full(n, "", dtype=object)
    cuts = spec.base.CUTS
    for c in cuts:
        xs, ys = [p[0] for p in c], [p[1] for p in c]
        box = ((local_c[:, 0] >= min(xs)) & (local_c[:, 0] <= max(xs)) & (local_c[:, 1] >= min(ys))
               & (local_c[:, 1] <= max(ys)))
        for k in np.nonzero(box)[0]:
            if spec.ridge.inside(local_c[k, 0], local_c[k, 1], c):
                rule[k] = "C"
    for k in np.nonzero((rule == "") & (owner == "CR_Z1_Drape"))[0]:
        if spec.massif_cover_weight(local_c[k, 0], local_c[k, 1], "V002") < PLATEAU_WEIGHT:
            rule[k] = "B"
    print("Z1_CULL_STAGE CB", flush=True)

    big = (cat == "mesh") & ~flat & (ext.max(axis=1) >= BIG_BODY_CM) & (rule == "")
    tree, face_owner, boxed = big_body_bvh(data, inst, rot, big)
    on_ground = bottom <= ground + SUPPORT_GAP_CM
    floating = np.nonzero((rule == "") & ~on_ground)[0]
    supporters = {}
    for k in floating:
        down = -up[k]
        bottom_center = center[k] + down * (local_c[k, 2] - bottom[k])
        found = set()
        for p in (bottom_center, lowest[k]):
            origin = p - down * SUPPORT_LIFT_CM
            reach = SUPPORT_LIFT_CM + SUPPORT_GAP_CM
            start = origin.copy()
            while reach > 0:
                hit, _, face, dist = tree.ray_cast(start.tolist(), down.tolist(), reach)
                if hit is None:
                    break
                if face_owner[face] != k:
                    found.add(int(face_owner[face]))
                start = np.array(hit) + down * 1.0
                reach -= dist + 1.0
        supporters[int(k)] = found
    grounded = set(np.nonzero((rule == "") & on_ground)[0].tolist())
    changed = True
    while changed:
        changed = False
        for k, found in supporters.items():
            if k not in grounded and found & grounded:
                grounded.add(k)
                changed = True
    unsupported = [k for k in supporters if k not in grounded]
    rule[unsupported] = "A"
    # 진단: Cull/Probes.json([[x, y, H], ...])이 있으면 각 점 반경 PROBE_RADIUS_CM·높이 ±PROBE_HEIGHT_CM 인스턴스의 받침 관계를 쓴다.
    probe_file = OUT / "Probes.json"
    if probe_file.exists():
        probes = []
        for px, py, ph in json.loads(probe_file.read_text(encoding="utf-8")):
            near = np.nonzero((np.hypot(local_c[:, 0] - px, local_c[:, 1] - py) < PROBE_RADIUS_CM)
                              & (np.abs(local_c[:, 2] - ph) < PROBE_HEIGHT_CM))[0]
            rows_out = []
            for k in near[np.argsort(-ext[near].max(axis=1))][:40]:
                rows_out.append({"owner": owner[k], "mesh": data["meshes"][mesh[k]][0].rsplit("/", 1)[1],
                                 "rule": rule[k], "ext": [round(v) for v in ext[k]], "h": round(local_c[k, 2]),
                                 "bottom": round(bottom[k]), "ground": round(ground[k]), "big": bool(big[k]),
                                 "supporters": [[owner[j], data["meshes"][mesh[j]][0].rsplit("/", 1)[1],
                                                 round(bottom[j]), j in grounded] for j in supporters.get(int(k), ())]})
            probes.append({"point": [px, py, ph], "instances": rows_out})
        (OUT / ("Probe_%s.json" % version)).write_text(json.dumps(probes, indent=1, ensure_ascii=False), encoding="utf-8")
    print("Z1_CULL_STAGE A", flush=True)

    thin_axis = np.argmin(ext, axis=1)
    thin = (ext.min(axis=1) < THIN_RATIO * ext.max(axis=1)) & (ext.max(axis=1) >= THIN_MIN_CM) & (cat == "mesh")
    near = np.abs(local_c[:, 2] - ground) <= THIN_NEAR_CM
    limit = math.cos(math.radians(THIN_ANGLE_DEG))
    for k in np.nonzero((rule == "") & thin & near)[0]:
        axis = rot[k][:, thin_axis[k]]
        if abs(axis @ crust.normal(*local_c[k, :2])) < limit:
            rule[k] = "D"
    print("Z1_CULL_STAGE D", flush=True)

    remove, stats, samples = {}, {}, {}
    for k in np.nonzero(rule != "")[0]:
        label, actor, comp_name = comps[int(inst[k, 1])][:3]
        remove.setdefault(label, {}).setdefault(actor + "|" + comp_name, []).append(int(rows[k, 2]))
        s = stats.setdefault(label, {})
        s[rule[k]] = s.get(rule[k], 0) + 1
        samples.setdefault(rule[k], []).append([round(local_c[k, 0]), round(local_c[k, 1]), round(local_c[k, 2]),
                                                 label, data["meshes"][mesh[k]][0].rsplit("/", 1)[1]])
    for label in set(owner):
        stats.setdefault(label, {})["total"] = int((owner == label).sum())
    remove_decals = {}
    for d in data["decals"]:
        label, name = d[0], d[1]
        x, y, _ = spec.ridge.local(d[3:6])
        cut = any(spec.ridge.signed_distance(x, y, c) > 0 for c in cuts)
        relief = label == "CR_Z1_Drape" and spec.massif_cover_weight(x, y, "V002") < PLATEAU_WEIGHT
        if cut or relief:
            remove_decals.setdefault(label, []).append(name)
    totals = {r: int((rule == r).sum()) for r in "ABCD"}
    result = {"version": version, "instances": n, "removed": totals, "by_owner": stats,
              "parameters": {"plateau_weight": PLATEAU_WEIGHT, "support": [SUPPORT_GAP_CM, SUPPORT_LIFT_CM],
                             "big_body_cm": BIG_BODY_CM, "thin": [THIN_RATIO, THIN_MIN_CM, THIN_NEAR_CM, THIN_ANGLE_DEG],
                             "crust": spec.MASSIF_RAMP_VERSION},
              "big_bodies": int(big.sum()), "big_bodies_boxed": boxed,
              "remove": remove, "remove_decals": remove_decals, "samples": samples}
    (OUT / ("Decide_%s.json" % version)).write_text(json.dumps(result, separators=(",", ":")) + "\n", encoding="utf-8")
    print("Z1_CULL_DECIDE " + json.dumps({"instances": n, "removed": totals, "decals": sum(map(len, remove_decals.values())),
                                          "big": int(big.sum()), "boxed": boxed}), flush=True)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    assert len(args) == 1, args
    main(args[0])
