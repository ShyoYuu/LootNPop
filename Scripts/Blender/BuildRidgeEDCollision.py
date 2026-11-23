"""Z1 ED 지형의 충돌 프록시(U5)를 굽는다: 지면층 래스터를 넣은 충돌 지각 + 덩어리 `_Floor`·`_Shell`.

실행: blender --background --python Scripts/Blender/BuildRidgeEDCollision.py -- build
입력: UE `ExportRidgeEDCollision.py export`가 쓴 Saved/.../Collision/Export_V034/(인스턴스 world transform, 고유 메시
      Nanite fallback 이진 파일), 렌더링 지각 V010 FBX, Meadow 이음매 원본(Baseline/MeadowSeam.fbx).
출력: Saved/SliceOctant/Ridge_00/V012_Z1MassifCover_V011/(충돌 지각, UE `importcrust V011`과 같은 manifest 형식)
      Saved/SliceOctant/Ridge_00/V012_Z1Collision_V001/(SM_Ridge_EDTerrain_V001_Z1_Floor·_Shell FBX, manifest·검사·p95)

처리 순서(U5 기준, 2026-11-22 사용자 확인):
1. 고유 메시를 한 번씩 미리 줄인다(지면층용은 약 20cm 변, 덩어리용은 더 거칠게). 원본 fallback 정점과의 거리로 오차를 잰다.
2. 지면층(납작한 타일·낮은 덩어리)을 50cm 격자로 위에서 레이캐스트해, 렌더링 지각 V010과 높은 쪽을 높이장으로 삼는다.
   타일 판이 지각 위에 겹치는 두 번째 Support 층을 만들지 않으려고 이 높이장을 충돌 지각 V011 특징점으로 굽는다.
3. 덩어리(크기 100cm·돌출 50cm 이상)와 두께를 준 선반 타일을 40m 칸별로 voxel remesh(30cm)해 이어 붙이고 decimate한다.
4. 덩어리 부피 안에 든 지각 격자 칸(네 꼭짓점 모두 안)을 절개 다각형으로 V011을 굽는다(동굴 cut과 같은 방식).
   구운 V011 표본 중 50cm 넘게 안쪽인 점의 칸을 더해 다시 굽는다(안쪽 판정은 현지 Up 광선 교차 홀짝).
5. 덩어리에서 지각보다 50cm 넘게 묻힌 면을 지우고, 법선·현지 Up(-normalize(pos)) 내적 0.71로 Floor/Shell을 나눈다.
   4㎡ 미만 Floor 조각은 Shell로 보낸다.
6. 닫힌 덩어리 안에서 표면까지 50cm 넘게 들어간 V011 표본(정점·변 중점·면 중심)이 0개인지 assert한다.
7. ED 시각 메시(미리 줄인 메시) 위의 걷는 면 표본에서 충돌면까지 거리를 재 p50·p95를 기록한다.
좌표: 계산은 UE 월드 cm, Blender 객체는 BuildRidgeOctant 규약(bl: (x, -y, z)/100)이다. transform_apply·EMPTY를 쓰지 않는다.
"""
import hashlib
import json
import math
from pathlib import Path
import random
import sys
import time

import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
import numpy as np

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parent)]
import BuildRidgeOctant as builder
import RidgeZ1CompositionSpec as spec

ridge = spec.ridge
ROOT = HERE.parents[1]
SLICE = ROOT / "Saved/SliceOctant/Ridge_00"
EXPORT = ROOT / "Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/Collision/Export_V034"
RENDER_CRUST = SLICE / "V012_Z1MassifCover_V010/SM_Octant_Ridge_00_V012_Z1MassifCover_V010_R30000.fbx"
CRUST_VERSION = spec.MASSIF_COVER_VERSIONS["V011"][0]
PROXY_VERSION = "V012_Z1Collision_V001"
PROXY_NAME = "SM_Ridge_EDTerrain_V001_Z1_{}"
GROUND, BODY, PLATE = 0, 1, 2

GRID_CM = 50.0                 # 지면층 래스터·발자국 격자
TILE_CM = 2000.0               # 래스터·표본 BVH 칸
RAY_ABOVE_CM, RAY_BELOW_CM = 600.0, 150.0   # 지면층 래스터 높이 창(렌더링 지각 기준)
VOXEL_CM = 30.0
REMESH_TILE_CM, REMESH_MARGIN_CM = 4000.0, 150.0   # 덩어리 remesh 칸과 칸 밖 인스턴스 여유(voxel 5칸)
PLATE_THICKNESS_CM = 40.0      # 판·열린 외피에 주는 두께(완만한 면은 현지 아래로, 급한 면은 법선 반대로)
SLAB_GENTLE_DOT = 0.3          # |꼭짓점 법선·현지 Up|이 이 값 이상이면 완만한 면
BODY_TRI_AREA_CM2 = 1000.0     # remesh 뒤 decimate 목표 평균 삼각형 넓이(약 48cm 변, 500으로 줄여도 Floor p95 같음)
SKIRT_CM = 50.0                # 덩어리 면을 지각 아래 이 깊이까지 남겨 절개 이음을 막는다
FLOOR_DOT = 0.71
MIN_FLOOR_AREA_CM2 = 40000.0   # 4㎡(V001). V002 재분할·V003 빌드는 50㎡(GX-032)
INSIDE_TOLERANCE_CM = 50.0
CUT_PASSES = 12              # V011 실제 표본으로 절개 칸을 더하는 반복 상한(V003 빌드는 8회 뒤 1표본이 남아 늘렸다)
WELD_CM, WELD_BOX_SLACK = 0.01, 0.02   # 사전 축소 전 용접 거리, 축소 결과가 원본 상자를 벗어나도 되는 비율(대각선 대비)
GROUND_TRIS_PER_M2, GROUND_TRIS = 60.0, (200, 3000)
BODY_TRIS_PER_M2, BODY_TRIS = 20.0, (200, 6000)
SAMPLE_SEED, SAMPLES_PER_M2 = 51101, 0.5
S2, S3, S6 = math.sqrt(2), math.sqrt(3), math.sqrt(6)
R = ridge.RADIUS


# ---- 구면 좌표(벡터화) ----------------------------------------------------------------------------------------------

def to_local(p):
    # ridge.local의 numpy 판. p: (n, 3) UE cm → (n, 3) 옥탄트 로컬 x, y, H.
    depth = p.sum(axis=1) / S3
    x = R * (-p[:, 0] - p[:, 1] + 2 * p[:, 2]) / S6 / depth
    y = R * (-p[:, 0] + p[:, 1]) / S2 / depth
    return np.stack([x, y, R - np.linalg.norm(p, axis=1)], axis=1)


def to_world(x, y, h):
    raw = np.stack([R / S3 - x / S6 - y / S2, R / S3 - x / S6 + y / S2, R / S3 + 2 * x / S6], axis=-1)
    return raw * ((R - h) / np.linalg.norm(raw, axis=-1))[..., None]


def radial(x, y):
    # 현지 아래(구 바깥) 방향 단위 벡터.
    raw = np.array([R / S3 - x / S6 - y / S2, R / S3 - x / S6 + y / S2, R / S3 + 2 * x / S6])
    return raw / np.linalg.norm(raw)


def bl_array(p):
    return np.stack([p[:, 0] / 100.0, -p[:, 1] / 100.0, p[:, 2] / 100.0], axis=1)


def ue_array(p):
    return np.stack([p[:, 0] * 100.0, -p[:, 1] * 100.0, p[:, 2] * 100.0], axis=1)


def quat_matrix(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


# ---- Blender 메시 도우미 ------------------------------------------------------------------------------------------

def make_object(name, verts_bl, tris):
    data = bpy.data.meshes.new(name)
    data.vertices.add(len(verts_bl))
    data.vertices.foreach_set("co", np.asarray(verts_bl, dtype=np.float32).ravel())
    data.loops.add(len(tris) * 3)
    data.loops.foreach_set("vertex_index", np.asarray(tris, dtype=np.int32).ravel())
    data.polygons.add(len(tris))
    data.polygons.foreach_set("loop_start", np.arange(0, len(tris) * 3, 3, dtype=np.int32))
    data.polygons.foreach_set("loop_total", np.full(len(tris), 3, dtype=np.int32))
    data.update(calc_edges=True)
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def mesh_arrays(obj, evaluated=True):
    # 모디파이어를 적용한 결과를 삼각형 배열로 읽는다(객체 자체는 바꾸지 않는다).
    graph = bpy.context.evaluated_depsgraph_get()
    source = obj.evaluated_get(graph) if evaluated else obj
    data = source.to_mesh()
    data.calc_loop_triangles()
    verts = np.empty(len(data.vertices) * 3, dtype=np.float64)
    data.vertices.foreach_get("co", verts)
    tris = np.empty(len(data.loop_triangles) * 3, dtype=np.int64)
    data.loop_triangles.foreach_get("vertices", tris)
    source.to_mesh_clear()
    return verts.reshape(-1, 3), tris.reshape(-1, 3)


def remove(obj):
    data = obj.data
    bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.meshes.remove(data)


def bvh(verts, tris):
    return BVHTree.FromPolygons(verts.tolist(), tris.tolist(), all_triangles=True)


def tri_area(verts, tris):
    a, b, c = verts[tris[:, 0]], verts[tris[:, 1]], verts[tris[:, 2]]
    return 0.5 * np.linalg.norm(np.cross(b - a, c - a), axis=1)


def quantiles(values):
    if not len(values):
        return {}
    v = np.sort(np.asarray(values))
    return {str(q): round(float(v[min(len(v) - 1, int(q / 100 * len(v)))]), 2) for q in (50, 90, 95, 99)} | {
        "max": round(float(v[-1]), 2), "count": int(len(v))}


# ---- 1. 입력과 고유 메시 미리 줄이기 -------------------------------------------------------------------------------

def load_export():
    manifest = json.loads((EXPORT / "manifest.json").read_text(encoding="utf-8"))
    rows = json.loads((EXPORT / "instances.json").read_text(encoding="utf-8"))["instances"]
    meshes = []
    for item in manifest["meshes"]:
        data = (EXPORT / "meshes" / item["file"]).read_bytes()
        assert hashlib.sha256(data).hexdigest() == item["sha256"], item["file"]
        nv, nt = np.frombuffer(data[:8], dtype=np.int32)
        verts = np.frombuffer(data[8:8 + nv * 12], dtype=np.float32).reshape(-1, 3).astype(np.float64)
        tris = np.frombuffer(data[8 + nv * 12:], dtype=np.int32).reshape(-1, 3).astype(np.int64)
        assert len(tris) == nt, item["file"]
        meshes.append({"path": item["mesh"], "verts": verts, "tris": tris})
    inst = np.array(rows, dtype=np.float64)
    return manifest, inst, meshes


def weld(verts, tris):
    # 같은 위치(0.01cm 안)의 정점을 하나로 합치고 퇴화 삼각형을 뺀다.
    key = np.round(verts / WELD_CM).astype(np.int64)
    _, first, inverse = np.unique(key, axis=0, return_index=True, return_inverse=True)
    inverse = inverse.ravel()
    t = inverse[tris]
    t = t[(t[:, 0] != t[:, 1]) & (t[:, 1] != t[:, 2]) & (t[:, 2] != t[:, 0])]
    return verts[first], t


def decimate_meshes(meshes, inst):
    # 쓰임(지면층/덩어리)에 따라 목표 삼각형 수를 정해 메시 로컬 좌표(cm)에서 collapse decimate한다.
    # 오차(error_*_cm)는 원본 fallback 정점에서 줄인 면까지 거리에 대표 스케일을 곱한 인스턴스 크기 값이다.
    # 목표 삼각형 수와 오차는 인스턴스 크기 기준이다(ED는 같은 메시를 0.01배~수 배로 쓴다). 메시별 대표 스케일은 쓰임 중앙값이다.
    use, scales = {}, {}
    for row in inst:
        use.setdefault(int(row[0]), set()).add(int(row[1]))
        scales.setdefault(int(row[0]), []).append(float(np.abs(row[9:12]).max()))
    report = []
    rng = np.random.default_rng(SAMPLE_SEED)
    for index, m in enumerate(meshes):
        scale = float(np.median(scales.get(index, [1.0])))
        area_m2 = tri_area(m["verts"], m["tris"]).sum() / 1e4 * scale * scale
        target = 0
        if GROUND in use.get(index, ()):
            target = max(target, int(np.clip(area_m2 * GROUND_TRIS_PER_M2, *GROUND_TRIS)))
        if use.get(index, set()) & {BODY, PLATE}:
            target = max(target, int(np.clip(area_m2 * BODY_TRIS_PER_M2, *BODY_TRIS)))
        # fallback(RenderData)은 UV·법선 경계에서 정점이 갈라져 있다. 열린 이음매째 collapse하면 정점이 메시 밖 수십 m로
        # 튀므로(첫 빌드에서 41개 메시) 먼저 용접하고, 줄인 결과가 원본 상자를 벗어나면 비율을 올려 다시 줄인다.
        verts, tris = weld(m["verts"], m["tris"])
        # 용접 뒤에도 경계 변이 남으면 열린 외피(뒤가 없는 스캔 절벽 등)다. 덩어리로 쓰면 판처럼 두께를 준다(bodies()).
        open_edges = edge_counts(tris)[0]
        lo, hi = m["verts"].min(axis=0), m["verts"].max(axis=0)
        slack = WELD_BOX_SLACK * float(np.linalg.norm(hi - lo)) + 1.0
        ratio, retries = min(1.0, target / max(1, len(tris))), 0
        while ratio < 1.0:
            obj = make_object("Decimate", verts, tris)
            modifier = obj.modifiers.new("Decimate", "DECIMATE")
            modifier.decimate_type, modifier.ratio, modifier.use_collapse_triangulate = "COLLAPSE", ratio, True
            low_verts, low_tris = mesh_arrays(obj)
            remove(obj)
            if (low_verts.min(axis=0) >= lo - slack).all() and (low_verts.max(axis=0) <= hi + slack).all():
                verts, tris = low_verts, low_tris
                break
            ratio, retries = min(1.0, ratio * 2), retries + 1
        # 오차: 원본 fallback 정점(최대 3,000개)에서 줄인 면까지 거리.
        tree = bvh(verts, tris)
        picks = m["verts"][rng.choice(len(m["verts"]), min(3000, len(m["verts"])), replace=False)]
        dist = np.array([tree.find_nearest(Vector(p))[3] for p in picks])
        m["low_verts"], m["low_tris"], m["open"] = verts, tris, bool(open_edges)
        m["box"] = (verts.min(axis=0), verts.max(axis=0))
        assert (verts.min(axis=0) >= lo - slack).all() and (verts.max(axis=0) <= hi + slack).all(), m["path"]
        report.append({"mesh": m["path"], "fallback_tris": int(len(m["tris"])), "tris": int(len(tris)), "retries": retries,
                       "open_edges": int(open_edges),
                       "scale": round(scale, 4), "area_m2": round(float(area_m2), 2), "uses": sorted(use.get(index, ())),
                       "error_p95_cm": round(float(np.percentile(dist, 95)) * scale, 2),
                       "error_max_cm": round(float(dist.max()) * scale, 2)})
    return report


def instance_world(m, row, low=True):
    verts = m["low_verts"] if low else m["verts"]
    rot = quat_matrix(row[5:9])
    return (verts * row[9:12]) @ rot.T + row[2:5]


def instance_box_local(m, row):
    # 메시 상자 8 꼭짓점의 옥탄트 로컬 xy 범위.
    lo, hi = m["box"]
    corners = np.array([[x, y, z] for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])])
    local = to_local((corners * row[9:12]) @ quat_matrix(row[5:9]).T + row[2:5])
    return local[:, 0].min(), local[:, 0].max(), local[:, 1].min(), local[:, 1].max()


def gather(meshes, inst, rows, low=True):
    # 인스턴스 행들의 world 정점·삼각형을 하나로 합친다(UE cm). low=False면 미리 줄이기 전 fallback이다.
    verts, tris, offset = [], [], 0
    for row in rows:
        m = meshes[int(row[0])]
        verts.append(instance_world(m, row, low))
        tris.append((m["low_tris"] if low else m["tris"]) + offset)
        offset += len(m["low_verts"] if low else m["verts"])
    if not verts:
        return np.zeros((0, 3)), np.zeros((0, 3), dtype=np.int64)
    return np.concatenate(verts), np.concatenate(tris)


# ---- 렌더링 지각 V010 --------------------------------------------------------------------------------------------

class RenderCrust:
    def __init__(self):
        bpy.ops.import_scene.fbx(filepath=str(RENDER_CRUST))
        obj = next(o for o in bpy.context.selected_objects if o.type == "MESH")
        verts, tris = mesh_arrays(obj, evaluated=False)
        world = np.array(obj.matrix_world)
        verts = verts @ world[:3, :3].T + world[:3, 3]
        remove(obj)
        self.tree = bvh(ue_array(verts), tris)
        self.cache = {}

    def at(self, i, j):
        # 격자 점(i, j)의 렌더링 지각 H(cm). 지각이 없는 곳(동굴 절개)은 None.
        key = (i, j)
        if key not in self.cache:
            x, y = i * GRID_CM, j * GRID_CM
            down = radial(x, y)
            start = to_world(np.array(x), np.array(y), np.array(12000.0))
            hit = self.tree.ray_cast(Vector(start), Vector(down), 40000.0)[0]
            self.cache[key] = None if hit is None else float(to_local(np.array([hit]))[0, 2])
        return self.cache[key]


# ---- 2. 지면층 래스터 ----------------------------------------------------------------------------------------------

def tiles_of(boxes):
    # 인스턴스 로컬 xy 상자 → 래스터 칸(TILE_CM)별 인스턴스 번호.
    tiles = {}
    for k, (x0, x1, y0, y1) in enumerate(boxes):
        for ti in range(math.floor(x0 / TILE_CM), math.floor(x1 / TILE_CM) + 1):
            for tj in range(math.floor(y0 / TILE_CM), math.floor(y1 / TILE_CM) + 1):
                tiles.setdefault((ti, tj), []).append(k)
    return tiles


def raster(meshes, ground_rows, crust):
    boxes = [instance_box_local(meshes[int(r[0])], r) for r in ground_rows]
    tiles = tiles_of(boxes)
    per = int(TILE_CM / GRID_CM)
    heights, offsets, stats = {}, [], {"cells": 0, "hits": 0, "below_crust": 0, "no_crust": 0}
    for (ti, tj), members in sorted(tiles.items()):
        cells = set()
        for k in members:
            x0, x1, y0, y1 = boxes[k]
            for i in range(max(ti * per, math.ceil(x0 / GRID_CM)), min((ti + 1) * per, math.floor(x1 / GRID_CM) + 1)):
                for j in range(max(tj * per, math.ceil(y0 / GRID_CM)), min((tj + 1) * per, math.floor(y1 / GRID_CM) + 1)):
                    cells.add((i, j))
        if not cells:
            continue
        verts, tris = gather(meshes, None, [ground_rows[k] for k in members])
        tree = bvh(verts, tris)
        for i, j in sorted(cells):
            stats["cells"] += 1
            c = crust.at(i, j)
            if c is None:
                stats["no_crust"] += 1
                continue
            x, y = i * GRID_CM, j * GRID_CM
            start = to_world(np.array(x), np.array(y), np.array(c + RAY_ABOVE_CM))
            hit = tree.ray_cast(Vector(start), Vector(radial(x, y)), RAY_ABOVE_CM + RAY_BELOW_CM)[0]
            if hit is None:
                continue
            h = float(to_local(np.array([hit]))[0, 2])
            stats["hits"] += 1
            stats["below_crust"] += h < c
            heights[(i, j)] = max(c, h)
            offsets.append(h - c)
    stats["hit_minus_render_crust_cm"] = quantiles(offsets)
    return heights, stats


# ---- 3. 덩어리 remesh ----------------------------------------------------------------------------------------------
# 덩어리는 거의 하나로 이어져 있어(첫 빌드: 군집 1개에 10,756개·1,670만 삼각형) 한 번에 remesh하면 OpenVDB가 예외로 죽는다.
# 옥탄트 로컬 xy 칸(REMESH_TILE_CM)마다 칸 + 여유(REMESH_MARGIN_CM)에 걸친 인스턴스 전체를 remesh하고, 삼각형 중심이 그 칸에
# 든 것만 남긴다. 모든 칸이 원점 기준 같은 voxel 격자를 쓰고 여유가 narrow band보다 넓어 칸 경계 근처 결과가 같으므로,
# 정점을 위치로 용접하면 이음매 없이 이어진다. decimate는 이어 붙인 뒤 한 번 건다.

def edge_counts(tris):
    edges = np.sort(np.concatenate([tris[:, [0, 1]], tris[:, [1, 2]], tris[:, [2, 0]]]), axis=1)
    _, counts = np.unique(edges, axis=0, return_counts=True)
    return int((counts == 1).sum()), int((counts > 2).sum())


def body_input(meshes, rows):
    # 덩어리 + 아래쪽 두께를 준 판(열린 면이라 voxel 부피가 새므로 법선 반대쪽, 대개 아래로 두께를 준다). bl 좌표.
    parts = []
    for cls, thick in ((BODY, 0.0), (PLATE, PLATE_THICKNESS_CM)):
        chosen = rows[rows[:, 1] == cls]
        if not len(chosen):
            continue
        verts, tris = gather(meshes, None, chosen)
        if not thick:
            parts.append((bl_array(verts), tris))
            continue
        # 판·열린 외피에 두께를 준다. Blender Solidify는 균등 오프셋이면 퇴화 면에서 정점을 수천 km 밖으로 밀고(25GB),
        # 법선 방향이면 안쪽을 향한 스캔 법선 때문에 윗면이 두께만큼 뜨며(Floor p95 40cm), 양쪽 반씩이면 모든 윗면이 반만큼
        # 뜬다(중앙값 20cm). 그래서 완만한 면은 윗면을 그대로 두고 현지 아래로 밀고, 급한 면만 꼭짓점 법선 방향으로 민다.
        v, t = slab(verts, tris, thick)
        lo, hi = verts.min(axis=0), verts.max(axis=0)
        assert (v.min(axis=0) >= lo - thick - 1).all() and (v.max(axis=0) <= hi + thick + 1).all(), (lo, hi)
        v = bl_array(v)
        parts.append((v, t))
    offset, verts, tris = 0, [], []
    for v, t in parts:
        verts.append(v)
        tris.append(t + offset)
        offset += len(v)
    return np.concatenate(verts), np.concatenate(tris)


def slab(verts, tris, thick):
    # 열린 면(UE cm)을 두께 thick의 닫힌 판으로 만든다: 원래 면 + 민 면(뒤집음) + 경계 변 옆면.
    normals = np.zeros_like(verts)
    face = np.cross(verts[tris[:, 1]] - verts[tris[:, 0]], verts[tris[:, 2]] - verts[tris[:, 0]])
    for k in range(3):
        np.add.at(normals, tris[:, k], face)
    normals /= np.maximum(np.linalg.norm(normals, axis=1), 1e-9)[:, None]
    up = -verts / np.linalg.norm(verts, axis=1)[:, None]
    gentle = np.abs(np.einsum("ij,ij->i", normals, up)) >= SLAB_GENTLE_DOT
    direction = np.where(gentle[:, None], -up, -normals)
    n = len(verts)
    bottom = verts + direction * thick
    edges = np.concatenate([tris[:, [0, 1]], tris[:, [1, 2]], tris[:, [2, 0]]])
    key = np.sort(edges, axis=1)
    _, inverse, counts = np.unique(key, axis=0, return_inverse=True, return_counts=True)
    border = edges[counts[inverse.ravel()] == 1]
    a, b = border[:, 0], border[:, 1]
    sides = np.concatenate([np.stack([a, b, b + n], axis=1), np.stack([a, b + n, a + n], axis=1)])
    return np.concatenate([verts, bottom]), np.concatenate([tris, tris[:, ::-1] + n, sides])


def remesh(verts_bl, tris):
    obj = make_object("BodyTile", verts_bl, tris)
    modifier = obj.modifiers.new("Remesh", "REMESH")
    modifier.mode, modifier.voxel_size, modifier.adaptivity = "VOXEL", VOXEL_CM / 100.0, 0.0
    v, t = mesh_arrays(obj)
    remove(obj)
    if not len(t):
        return v, t
    # 칸 결과 중 일부 섬이 안팎이 뒤집혀 나온다(첫 칸 빌드: 바깥 점이 최대 61m 안쪽으로 판정되고 윗면이 Shell로 분류).
    # 칸을 자르기 전 닫힌 결과에서 섬마다 바깥 법선으로 맞춘다(bmesh는 연결 섬별로 처리한다).
    obj = make_object("BodyTileOrient", v, t)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(obj.data)
    bm.free()
    v, t = mesh_arrays(obj, evaluated=False)
    remove(obj)
    return v, t


def bodies(meshes, inst):
    rows = inst[(inst[:, 1] == BODY) | (inst[:, 1] == PLATE)].copy()
    # 뒤가 열린 외피는 voxel 부피가 생기지 않아 remesh에서 사라진다(3차 빌드: MassiveSandstoneCliff_04·NordicBoulder_01
    # 윗면이 프록시에서 50~250cm 떨어짐). 판과 같이 법선 반대쪽으로 두께를 준다.
    opened = np.array([meshes[int(r[0])]["open"] for r in rows], dtype=bool) & (rows[:, 1] == BODY)
    rows[opened, 1] = PLATE
    boxes = [instance_box_local(meshes[int(r[0])], r) for r in rows]
    tiles = {}
    for k, (x0, x1, y0, y1) in enumerate(boxes):
        for ti in range(math.floor((x0 - REMESH_MARGIN_CM) / REMESH_TILE_CM),
                        math.floor((x1 + REMESH_MARGIN_CM) / REMESH_TILE_CM) + 1):
            for tj in range(math.floor((y0 - REMESH_MARGIN_CM) / REMESH_TILE_CM),
                            math.floor((y1 + REMESH_MARGIN_CM) / REMESH_TILE_CM) + 1):
                tiles.setdefault((ti, tj), []).append(k)
    verts, tris, offset = [], [], 0
    stats = {"tiles": len(tiles), "largest_tile_instances": max(len(m) for m in tiles.values()), "input_tris": 0,
             "remesh_tris": 0, "open_bodies_as_plates": int(opened.sum())}
    started = time.time()
    for n, ((ti, tj), members) in enumerate(sorted(tiles.items())):
        v_in, t_in = body_input(meshes, rows[members])
        v, t = remesh(v_in, t_in)
        stats["input_tris"] += int(len(t_in))
        if len(t):
            centers = to_local(ue_array(v[t].mean(axis=1)))
            mine = ((np.floor(centers[:, 0] / REMESH_TILE_CM) == ti) & (np.floor(centers[:, 1] / REMESH_TILE_CM) == tj))
            t = t[mine]
            stats["remesh_tris"] += int(len(t))
            verts.append(v)
            tris.append(t + offset)
            offset += len(v)
        print("RIDGE_ED_COLLISION_TILE %d/%d (%d,%d) instances %d input %d kept %d %.0fs" % (
            n + 1, len(tiles), ti, tj, len(members), len(t_in), len(t), time.time() - started), flush=True)
    v, t = weld(np.concatenate(verts), np.concatenate(tris))
    stats["welded"] = {"vertices": int(len(v)), "triangles": int(len(t))}
    stats["welded"]["boundary_edges"], stats["welded"]["non_manifold_edges"] = edge_counts(t)
    area_cm2 = tri_area(v, t).sum() * 1e4
    stats["remesh_area_m2"] = round(area_cm2 / 1e4, 1)
    target = max(200, int(area_cm2 / BODY_TRI_AREA_CM2))
    if target < len(t):
        obj = make_object("BodyRemesh", v, t)
        modifier = obj.modifiers.new("Decimate", "DECIMATE")
        modifier.decimate_type, modifier.ratio, modifier.use_collapse_triangulate = "COLLAPSE", target / len(t), True
        v, t = mesh_arrays(obj)
        remove(obj)
    # voxel remesh는 바깥 법선으로 일관되게 감는다. 혹시 뒤집혔으면(부호 있는 부피 음수) 통째로 뒤집는다.
    # 몸체는 원점을 감싸지 않으므로 원점 기준 삼중곱 합의 부호가 부피의 부호다.
    if np.einsum("ij,ij->i", v[t[:, 0]], np.cross(v[t[:, 1]], v[t[:, 2]])).sum() < 0:
        t = t[:, ::-1].copy()
    boundary, non_manifold = edge_counts(t)
    stats["decimated"] = {"vertices": int(len(v)), "triangles": int(len(t)), "boundary_edges": boundary,
                          "non_manifold_edges": non_manifold}
    return v, t, stats


def face_normals_ue(verts_bl, tris):
    # bl 좌표의 감은 방향(바깥) 법선을 UE 좌표로 옮긴다(y 반전은 반사라 법선도 y만 뒤집는다).
    n = np.cross(verts_bl[tris[:, 1]] - verts_bl[tris[:, 0]], verts_bl[tris[:, 2]] - verts_bl[tris[:, 0]])
    n /= np.maximum(np.linalg.norm(n, axis=1), 1e-12)[:, None]
    return np.stack([n[:, 0], -n[:, 1], n[:, 2]], axis=1)


class Volume:
    # 덩어리 안쪽 판정. 현지 Up(구 중심 쪽)으로 쏜 광선이 덩어리 면을 홀수 번 지나면 안쪽이다.
    # 가장 가까운 면의 법선으로 판정하면 뒤가 열린 ED 절벽 외피(remesh 뒤에도 뒤쪽이 닫히지 않음) 뒤의 지각이 수십 m 안쪽으로
    # 오판된다(2차 빌드: 50cm 넘게 안쪽 24,070개, 위쪽 교차 0회가 대부분). 진짜 바위 속 점은 위로 윗면을 한 번 지난다.
    def __init__(self, verts_ue, tris, normals_ue):
        self.tree = bvh(verts_ue, tris)
        self.normals = normals_ue

    def crossings(self, p):
        up = Vector(-p / np.linalg.norm(p))
        start, count = Vector(p), 0
        for _ in range(64):
            hit = self.tree.ray_cast(start + up * 0.5, up, 20000.0)[0]
            if hit is None:
                break
            count += 1
            start = hit
        return count

    def depth(self, p):
        # 안쪽이면 표면까지 거리(양수), 바깥이면 음수 거리.
        location, _, _, distance = self.tree.find_nearest(Vector(p))
        if location is None:
            return -1e9
        return distance if self.crossings(np.asarray(p, dtype=float)) % 2 == 1 else -distance


class FastInside:
    # builder.inside와 같은 홀짝 교차 판정을 y 띠(100cm)별 변 목록으로 빠르게 한다. 결과는 원래 함수와 같다
    # (y 띠 밖의 변은 (a.y > y) != (b.y > y)를 만족할 수 없다). 이음매 고리처럼 정점이 많은 다각형에만 쓴다.
    BAND = 100.0

    def __init__(self, original):
        self.original, self.cache = original, {}

    def __call__(self, x, y, polygon):
        if len(polygon) < 64:
            return self.original(x, y, polygon)
        index = self.cache.get(id(polygon))
        if index is None:
            index = {}
            for a, b in zip(polygon, polygon[1:] + polygon[:1]):
                for band in range(math.floor(min(a[1], b[1]) / self.BAND), math.floor(max(a[1], b[1]) / self.BAND) + 1):
                    index.setdefault(band, []).append((a, b))
            self.cache[id(polygon)] = index
        result = False
        for a, b in index.get(math.floor(y / self.BAND), ()):
            if (a[1] > y) != (b[1] > y) and x < (b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:
                result = not result
        return result


def walk_height(heights, crust, i, j):
    h = heights.get((i, j))
    return h if h is not None else crust.at(i, j)


def footprint(volume, body_local, heights, crust):
    # 덩어리 정점 주변 격자 점에서 지각(지면층 포함) 점이 덩어리 안인지 본다. 네 꼭짓점 모두 안인 칸을 자른다.
    nodes = set()
    for i, j in {(math.floor(x / GRID_CM), math.floor(y / GRID_CM)) for x, y in body_local[:, :2]}:
        for di in range(-2, 4):
            for dj in range(-2, 4):
                nodes.add((i + di, j + dj))
    inside = set()
    for i, j in nodes:
        h = walk_height(heights, crust, i, j)
        if h is None:
            continue
        if volume.depth(to_world(np.array(i * GRID_CM), np.array(j * GRID_CM), np.array(h))) > 0:
            inside.add((i, j))
    return {(i, j) for i, j in inside if {(i + 1, j), (i, j + 1), (i + 1, j + 1)} <= inside}, len(nodes)


def mask_loops(cells):
    # 칸 집합의 경계 고리들(바깥·구멍). 칸 모서리 점 좌표(cm), 곧은 변의 중간 점은 뺀다.
    edges = {}
    for i, j in cells:
        for a, b in (((i, j), (i + 1, j)), ((i + 1, j), (i + 1, j + 1)), ((i + 1, j + 1), (i, j + 1)), ((i, j + 1), (i, j))):
            if (b, a) in edges:
                del edges[(b, a)]
            else:
                edges[(a, b)] = True
    nxt = {}
    for a, b in edges:
        nxt.setdefault(a, []).append(b)
    loops, used = [], set()
    for start in list(nxt):
        for first in nxt[start]:
            if (start, first) in used:
                continue
            loop, a, b = [start], start, first
            while True:
                used.add((a, b))
                if b == start:
                    break
                loop.append(b)
                options = [c for c in nxt[b] if (b, c) not in used]
                # 대각 접점은 왼쪽으로 꺾어 고리를 단순하게 유지한다.
                d = (b[0] - a[0], b[1] - a[1])
                options.sort(key=lambda c: -((d[0] * (c[1] - b[1]) - d[1] * (c[0] - b[0]))))
                a, b = b, options[0]
            simple = [p for k, p in enumerate(loop)
                      if (p[0] - loop[k - 1][0]) * (loop[(k + 1) % len(loop)][1] - p[1])
                      != (p[1] - loop[k - 1][1]) * (loop[(k + 1) % len(loop)][0] - p[0])]
            loops.append([(p[0] * GRID_CM, p[1] * GRID_CM) for p in simple])
    return loops


# ---- 5. Floor/Shell -----------------------------------------------------------------------------------------------

def split_body(verts_ue, tris, normals_ue, heights, crust):
    local = to_local(verts_ue)
    buried = np.zeros(len(verts_ue), dtype=bool)
    for k, (x, y, h) in enumerate(local):
        w = walk_height(heights, crust, round(x / GRID_CM), round(y / GRID_CM))
        buried[k] = w is not None and h < w - SKIRT_CM
    keep = ~buried[tris].all(axis=1)
    centers = verts_ue[tris].mean(axis=1)
    up = -centers / np.linalg.norm(centers, axis=1)[:, None]
    floor = (np.einsum("ij,ij->i", normals_ue, up) >= FLOOR_DOT) & keep
    area = tri_area(verts_ue, tris)
    # 4㎡ 미만 Floor 연결 조각(변 공유)은 Shell로 보낸다.
    parent = {int(f): int(f) for f in np.nonzero(floor)[0]}

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    owner = {}
    for f in parent:
        for e in ((tris[f, 0], tris[f, 1]), (tris[f, 1], tris[f, 2]), (tris[f, 2], tris[f, 0])):
            key = (min(e), max(e))
            if key in owner:
                ra, rb = find(owner[key]), find(f)
                if ra != rb:
                    parent[ra] = rb
            else:
                owner[key] = f
    groups = {}
    for f in parent:
        groups.setdefault(find(f), []).append(f)
    small = 0
    for faces in groups.values():
        if area[faces].sum() < MIN_FLOOR_AREA_CM2:
            floor[faces] = False
            small += 1
    shell = keep & ~floor
    report = {"deleted_buried": int((~keep).sum()), "floor": int(floor.sum()), "shell": int(shell.sum()),
              "floor_components": len(groups) - small, "small_floor_to_shell": small,
              "floor_area_m2": round(float(area[floor].sum() / 1e4), 1),
              "shell_area_m2": round(float(area[shell].sum() / 1e4), 1)}
    return floor, shell, report


def export_role(name, verts_ue, tris, faces, role):
    used = np.unique(tris[faces].ravel())
    remap = np.full(len(verts_ue), -1, dtype=np.int64)
    remap[used] = np.arange(len(used))
    obj = make_object(name, bl_array(verts_ue[used]), remap[tris[faces]])
    builder.export(obj, role)
    return obj


def vertex_hash(verts_ue, tris):
    digest = hashlib.sha256()
    digest.update(np.round(verts_ue * 100).astype(np.int64).tobytes())
    digest.update(np.asarray(tris, dtype=np.int64).tobytes())
    return digest.hexdigest()


# ---- 6·7. 검사 ---------------------------------------------------------------------------------------------------

def crust_samples(crust_obj):
    verts_bl, tris = mesh_arrays(crust_obj, evaluated=False)
    verts = ue_array(verts_bl)
    edges = np.concatenate([tris[:, [0, 1]], tris[:, [1, 2]], tris[:, [2, 0]]])
    edges = np.unique(np.sort(edges, axis=1), axis=0)
    return verts, tris, np.concatenate([verts, verts[edges].mean(axis=1), verts[tris].mean(axis=1)])


def assert_inside(volume, body_verts, samples):
    lo, hi = body_verts.min(axis=0) - 100, body_verts.max(axis=0) + 100
    near = samples[np.all((samples >= lo) & (samples <= hi), axis=1)]
    depths = np.array([volume.depth(p) for p in near])
    inside = depths > 0
    deep = depths > INSIDE_TOLERANCE_CM
    return {"samples": int(len(samples)), "tested": int(len(near)), "inside_within_tolerance": int((inside & ~deep).sum()),
            "inside_deeper_than_tolerance": int(deep.sum()), "max_depth_cm": round(float(depths.max()), 2) if len(depths) else 0,
            "tolerance_cm": INSIDE_TOLERANCE_CM, "pass": bool(deep.sum() == 0)}, near[deep]


def measure(meshes, inst, crust, collision):
    # ED 시각 메시의 맨 위 걷는 면 표본 → 충돌면(V011·Floor·Shell)까지 거리.
    # 시각 표면은 미리 줄인 메시가 아니라 fallback 원본이다(축소 오차가 측정에 섞이지 않게). 원본으로 칸 BVH를 만들면
    # 메모리가 14GB를 넘으므로, 고유 메시마다 로컬 BVH를 한 번 만들고 표본 광선을 후보 인스턴스(로컬 xy 상자가 표본을
    # 덮는 것)의 메시 로컬 좌표로 바꿔 쏜다. 표본은 보이는 표면 분류(ground/body/plate/render_crust)와 가장 가까운
    # 충돌면으로 나눠 기록한다. 렌더링 지각(V010)이 ED 메시보다 위면 보이는 것은 지각이다.
    boxes = [instance_box_local(meshes[int(r[0])], r) for r in inst]
    tiles = tiles_of(boxes)
    rng = random.Random(SAMPLE_SEED)
    names = {GROUND: "ground", BODY: "body", PLATE: "plate"}
    local_trees = {}
    frames = {}
    result = {}
    for (ti, tj), members in sorted(tiles.items()):
        count = int(TILE_CM * TILE_CM / 1e4 * SAMPLES_PER_M2)
        for _ in range(count):
            x, y = (ti + rng.random()) * TILE_CM, (tj + rng.random()) * TILE_CM
            c = crust.at(round(x / GRID_CM), round(y / GRID_CM))
            if c is None:
                continue
            down = radial(x, y)
            start = to_world(np.array(x), np.array(y), np.array(c + 8000.0))
            best_hit = None
            for k in members:
                x0, x1, y0, y1 = boxes[k]
                if not (x0 <= x <= x1 and y0 <= y <= y1):
                    continue
                row = inst[k]
                mesh_index = int(row[0])
                if mesh_index not in local_trees:
                    m = meshes[mesh_index]
                    local_trees[mesh_index] = bvh(m["verts"], m["tris"])
                if k not in frames:
                    frames[k] = (quat_matrix(row[5:9]), row[9:12], row[2:5])
                rot, scale, origin = frames[k]
                o = (rot.T @ (start - origin)) / scale
                d = (rot.T @ down) / scale
                length = float(np.linalg.norm(d))
                hit, normal, _, _ = local_trees[mesh_index].ray_cast(Vector(o), Vector(d / length), 8500.0 * length)
                if hit is None:
                    continue
                world = rot @ (np.array(hit) * scale) + origin
                distance = float(np.linalg.norm(world - start))
                if best_hit is None or distance < best_hit[0]:
                    n = rot @ (np.array(normal) / scale)
                    best_hit = (distance, world, n / np.linalg.norm(n), names[int(row[1])])
            seen = crust.tree.ray_cast(Vector(start), Vector(down), 8500.0)
            if seen[0] is not None and (best_hit is None or (seen[0] - Vector(start)).length < best_hit[0]):
                best_hit = ((seen[0] - Vector(start)).length, np.array(seen[0]), np.array(seen[1]), "render_crust")
            if best_hit is None or abs(float(np.dot(best_hit[2], down))) < FLOOR_DOT:
                continue
            point = Vector(best_hit[1])
            best = ("shell", 1000.0)
            for name, target in collision.items():
                found = target.find_nearest(point, 1000.0)
                if found[0] is not None and found[3] < best[1]:
                    best = (name, found[3])
            result.setdefault("%s|%s" % (best_hit[3], best[0]), []).append(best[1])
    walk = [d for key, values in result.items() if not key.endswith("|shell") for d in values]
    every = [d for values in result.values() for d in values]
    ed = [d for key, values in result.items() if not key.startswith("render_crust") for d in values]
    ed_walk = [d for key, values in result.items() if not key.startswith("render_crust") and not key.endswith("|shell")
               for d in values]
    floor = [d for key, values in result.items() if key.endswith("|floor") for d in values]
    return {"reference": "fallback", "walkable_samples": len(every), "all": quantiles(every),
            "floor_or_crust": quantiles(walk), "ed_visible": quantiles(ed), "ed_visible_floor_or_crust": quantiles(ed_walk),
            "nearest_floor": quantiles(floor), "by_class": {key: quantiles(values) for key, values in sorted(result.items())}}


# 빌드 판(profile). V001은 U5 첫 빌드(GX-031), V003은 U6 러프 PIE 정리 뒤(시각 인스턴스 덜어냄 Z1Cull V001, 렌더링 지각
# V012 = V010 + BossValley 경사로, Floor 하한 50㎡ GX-032). 판을 고르면 입력·출력 버전과 지각 높이 함수가 함께 바뀐다.
PROFILES = {
    "V001": {"export": "Export_V034", "render": "V010", "crust": "V011", "proxy": "V012_Z1Collision_V001",
             "min_floor_cm2": 40000.0, "ground": "massif_flat_ground", "features": "massif_flat_feature_points"},
    "V003": {"export": "Export_V037", "render": "V012", "crust": "V013", "proxy": "V012_Z1Collision_V003",
             "min_floor_cm2": 500000.0, "ground": "massif_ramp_ground", "features": "massif_ramp_feature_points"},
}
GROUND_FN, FEATURE_FN = spec.massif_flat_ground, spec.massif_flat_feature_points


def use_profile(name):
    global EXPORT, RENDER_CRUST, CRUST_VERSION, PROXY_VERSION, MIN_FLOOR_AREA_CM2, GROUND_FN, FEATURE_FN
    p = PROFILES[name]
    EXPORT = EXPORT.with_name(p["export"])
    render = spec.MASSIF_COVER_VERSIONS[p["render"]][0]
    RENDER_CRUST = SLICE / render / ("SM_Octant_Ridge_00_%s_R30000.fbx" % render)
    spec.MASSIF_COVER_VERSIONS.setdefault(p["crust"], ("V012_Z1MassifCover_" + p["crust"], -2200, 500))
    CRUST_VERSION = spec.MASSIF_COVER_VERSIONS[p["crust"]][0]
    PROXY_VERSION = p["proxy"]
    MIN_FLOOR_AREA_CM2 = p["min_floor_cm2"]
    GROUND_FN, FEATURE_FN = getattr(spec, p["ground"]), getattr(spec, p["features"])


# ---- 실행 --------------------------------------------------------------------------------------------------------
# 단계 결과(미리 줄인 메시·래스터·렌더링 지각 표본·덩어리)는 OUT/cache에 남기고 다음 실행에서 다시 쓴다.
# 입력 지문(instances·매개변수)이 바뀌면 캐시를 버린다. `-- build fresh`는 캐시를 지우고 처음부터 돈다.

def cache_key(manifest):
    params = [GRID_CM, TILE_CM, RAY_ABOVE_CM, RAY_BELOW_CM, VOXEL_CM, PLATE_THICKNESS_CM, BODY_TRI_AREA_CM2, WELD_CM, WELD_BOX_SLACK, REMESH_TILE_CM, REMESH_MARGIN_CM,
              GROUND_TRIS_PER_M2, GROUND_TRIS, BODY_TRIS_PER_M2, BODY_TRIS, SAMPLE_SEED, "open-as-plate", "slab-down", SLAB_GENTLE_DOT]
    return hashlib.sha256(json.dumps([manifest["instances_sha256"], params]).encode()).hexdigest()[:16]


def cached_decimation(cache, meshes, inst):
    path = cache / "decimated.npz"
    if path.exists():
        data = np.load(path)
        for k, m in enumerate(meshes):
            m["low_verts"], m["low_tris"] = data["v%d" % k], data["t%d" % k]
            m["box"] = (m["low_verts"].min(axis=0), m["low_verts"].max(axis=0))
        report = json.loads((cache / "decimation.json").read_text(encoding="utf-8"))
        for m, row in zip(meshes, report):
            assert row["mesh"] == m["path"], row["mesh"]
            m["open"] = row["open_edges"] > 0
        return report
    report = decimate_meshes(meshes, inst)
    np.savez(path, **{("v%d" % k): m["low_verts"] for k, m in enumerate(meshes)},
             **{("t%d" % k): m["low_tris"] for k, m in enumerate(meshes)})
    (cache / "decimation.json").write_text(json.dumps(report), encoding="utf-8")
    return report


def cached_raster(cache, meshes, inst, crust):
    path = cache / "raster.json"
    if path.exists():
        data = json.loads(path.read_text(encoding="utf-8"))
        crust.cache.update({(i, j): h for i, j, h in data["crust"]})
        return {(i, j): h for i, j, h in data["heights"]}, data["stats"]
    heights, stats = raster(meshes, inst[inst[:, 1] == GROUND], crust)
    path.write_text(json.dumps({"heights": [[i, j, h] for (i, j), h in heights.items()],
                                "crust": [[i, j, h] for (i, j), h in crust.cache.items()], "stats": stats}),
                    encoding="utf-8")
    return heights, stats


def cached_bodies(cache, meshes, inst):
    path = cache / "bodies.npz"
    if path.exists():
        data = np.load(path)
        return data["v"], data["t"], json.loads((cache / "bodies.json").read_text(encoding="utf-8"))
    v, t, stats = bodies(meshes, inst)
    np.savez(path, v=v, t=t)
    (cache / "bodies.json").write_text(json.dumps(stats), encoding="utf-8")
    return v, t, stats


def proxy_hash(obj):
    v, t = mesh_arrays(obj, evaluated=False)
    return vertex_hash(ue_array(v), t)


def build(mode=""):
    sys.stdout.reconfigure(line_buffering=True)
    out = SLICE / PROXY_VERSION
    crust_out = SLICE / CRUST_VERSION
    assert not (out / "manifest.json").exists() and not (crust_out / "manifest.json").exists(), "Choose a new version"
    out.mkdir(parents=True, exist_ok=True)
    crust_out.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    clock, timing = time.time(), {}

    def lap(name):
        nonlocal clock
        timing[name] = round(time.time() - clock, 1)
        clock = time.time()
        print("RIDGE_ED_COLLISION_STAGE", name, timing[name], flush=True)

    manifest, inst, meshes = load_export()
    cache = out / "cache" / cache_key(manifest)
    if mode == "fresh" and cache.exists():
        for f in cache.iterdir():
            f.unlink()
    cache.mkdir(parents=True, exist_ok=True)
    decimation = cached_decimation(cache, meshes, inst)
    lap("decimate")
    crust = RenderCrust()
    heights, raster_stats = cached_raster(cache, meshes, inst, crust)
    lap("raster")
    body_bl, body_tris, body_stats = cached_bodies(cache, meshes, inst)
    body_ue, body_normals = ue_array(body_bl), face_normals_ue(body_bl, body_tris)
    volume = Volume(body_ue, body_tris, body_normals)
    lap("remesh")
    cells, tested_nodes = footprint(volume, to_local(body_ue), heights, crust)
    footprint_cells = len(cells)
    caves = list(spec.base.CUTS)

    def cut_test(x, y):
        # 절개 안쪽 판정은 칸 마스크(구멍 포함) + 동굴 cut 다각형이다. 고리는 V011 삼각분할의 제약 변으로만 쓴다.
        return (math.floor(x / GRID_CM), math.floor(y / GRID_CM)) in cells or any(builder.inside(x, y, p) for p in caves)
    lap("footprint")
    # 충돌 지각 V011: 래스터 점을 특징점으로 먼저 넣고(겹치는 V010 특징점은 빠진다), 그 점은 래스터 높이를 쓴다.
    raster_heights = {(round(i * GRID_CM), round(j * GRID_CM)): h for (i, j), h in heights.items()}

    def walk_ground(x, y):
        h = raster_heights.get((round(x), round(y)))
        return h if h is not None else GROUND_FN(x, y)

    def walk_features():
        for i, j in heights:
            if (i, j) not in cells:
                yield (i * GRID_CM, j * GRID_CM)
        yield from FEATURE_FN()
    # 50cm 칸 꼭짓점 판정만으로는 경계 칸·얇은 바위에서 지각이 칸 대각선만큼 남고, 절개 경계 정점은 래스터 대신 V010
    # 높이를 써서 V011 면이 판정 높이와 다르다(3차 빌드: 50cm 넘게 안쪽 3,308표본). 그래서 구운 V011의 실제 표본으로
    # assert와 같은 판정을 하고, 50cm 넘게 안쪽인 표본이 든 칸을 절개에 더해 다시 굽는다(CUT_PASSES회까지).
    passes = []
    original = builder.inside
    builder.inside = FastInside(original)
    for attempt in range(CUT_PASSES):
        loops = mask_loops(cells)
        stale = bpy.data.objects.get(f"SM_Octant_Ridge_00_{CRUST_VERSION}_R30000")
        if stale:
            remove(stale)
        builder.OUT, builder.VERSION, builder.EXPORTS, builder.SOURCES = crust_out, CRUST_VERSION, [], []
        builder.crust(caves + loops, ground=walk_ground, feature_points=walk_features, cut_test=cut_test)
        crust_obj = bpy.data.objects[f"SM_Octant_Ridge_00_{CRUST_VERSION}_R30000"]
        crust_verts, crust_tris, samples = crust_samples(crust_obj)
        inside, deep = assert_inside(volume, body_ue, samples)
        # 경계 정점·변 중점은 이웃 칸과 공유하므로 표본 ±1cm에 걸친 칸을 모두 더한다.
        added = {(math.floor((x + dx) / GRID_CM), math.floor((y + dy) / GRID_CM)) for x, y, _ in to_local(deep)
                 for dx in (-1.0, 1.0) for dy in (-1.0, 1.0)} - cells if len(deep) else set()
        passes.append({"cut_cells": len(cells), "loops": len(loops), "deep": inside["inside_deeper_than_tolerance"],
                       "max_depth_cm": inside["max_depth_cm"], "added_cells": len(added)})
        print("RIDGE_ED_COLLISION_CUT_PASS %d %s" % (attempt + 1, json.dumps(passes[-1])), flush=True)
        if inside["pass"] or not added:
            break
        cells |= added
    builder.inside = original
    crust_exports, crust_sources = builder.EXPORTS, builder.SOURCES
    raster_points = sum(1 for key in heights if key not in cells)
    (crust_out / "manifest.json").write_text(json.dumps({"exports": crust_exports, "sources": crust_sources}, indent=2)
                                             + "\n", encoding="utf-8")
    lap("crust")
    floor, shell, split = split_body(body_ue, body_tris, body_normals, heights, crust)
    builder.OUT, builder.EXPORTS = out, []
    floor_obj = export_role(PROXY_NAME.format("Floor"), body_ue, body_tris, floor, "Floor")
    shell_obj = export_role(PROXY_NAME.format("Shell"), body_ue, body_tris, shell, "Shell")
    proxy_exports = builder.EXPORTS
    lap("split")
    collision = {"crust": bvh(crust_verts, crust_tris)}
    for name, obj in (("floor", floor_obj), ("shell", shell_obj)):
        v, t = mesh_arrays(obj, evaluated=False)
        collision[name] = bvh(ue_array(v), t)
    fit = measure(meshes, inst, crust, collision)
    lap("measure")
    kept = floor | shell
    proxy_verts = body_ue[np.unique(body_tris[kept].ravel())]
    bounds = {"min": np.minimum(crust_verts.min(axis=0), proxy_verts.min(axis=0)).round(3).tolist(),
              "max": np.maximum(crust_verts.max(axis=0), proxy_verts.max(axis=0)).round(3).tolist()}
    report = {"version": PROXY_VERSION, "crust_version": CRUST_VERSION, "input": str(EXPORT.relative_to(ROOT)),
              "input_instances_sha256": manifest["instances_sha256"], "input_counts": manifest["counts"],
              "cache": cache.name,
              "parameters": {"grid_cm": GRID_CM, "tile_cm": TILE_CM, "ray_window_cm": [RAY_ABOVE_CM, RAY_BELOW_CM],
                             "voxel_cm": VOXEL_CM, "plate_thickness_cm": PLATE_THICKNESS_CM,
                             "body_tri_area_cm2": BODY_TRI_AREA_CM2, "skirt_cm": SKIRT_CM, "floor_dot": FLOOR_DOT,
                             "min_floor_area_cm2": MIN_FLOOR_AREA_CM2, "inside_tolerance_cm": INSIDE_TOLERANCE_CM,
                             "ground_tris_per_m2": GROUND_TRIS_PER_M2, "body_tris_per_m2": BODY_TRIS_PER_M2},
              "decimation": {"meshes": len(decimation),
                             "error_p95_cm_max": max(d["error_p95_cm"] for d in decimation),
                             "error_max_cm_max": max(d["error_max_cm"] for d in decimation), "rows": decimation},
              "raster": dict(raster_stats, points=raster_points),
              "body": body_stats, "footprint": {"tested_nodes": tested_nodes, "footprint_cells": footprint_cells,
                                                "cut_cells": len(cells), "loops": len(loops),
                                                "cut_area_m2": len(cells) * GRID_CM ** 2 / 1e4, "passes": passes},
              "split": split, "inside_assert": inside, "fit": fit, "bounds_cm": bounds,
              "abs_max_component_cm": float(np.abs(np.concatenate([crust_verts, proxy_verts])).max()),
              "crust": crust_exports, "proxies": proxy_exports,
              "hashes": {"crust": vertex_hash(crust_verts, crust_tris),
                         "floor": proxy_hash(floor_obj), "shell": proxy_hash(shell_obj)}, "timing_s": timing}
    (out / "manifest.json").write_text(json.dumps({"exports": proxy_exports, "report": report}, indent=1) + "\n",
                                       encoding="utf-8")
    bpy.ops.wm.save_as_mainfile(filepath=str(out / "Ridge_Z1Collision.blend"), check_existing=False)
    print("RIDGE_ED_COLLISION_READY " + json.dumps({"inside": inside["pass"], "fit": fit["floor_or_crust"],
                                                   "floor": split["floor"], "shell": split["shell"]}))
    assert inside["pass"], inside


def resplit(proxy_version, min_floor_m2):
    # 1~4단계(캐시·충돌 지각 V011)는 원본 빌드(PROXY_VERSION) 것을 그대로 쓰고 5~7단계만 새 Floor 하한으로 다시 한다.
    # U6에서 Floor 연결 성분마다 Support/Nav Layer가 생겨 Layer 400개가 런타임 상한 256을 넘었다(GX-032).
    global MIN_FLOOR_AREA_CM2
    sys.stdout.reconfigure(line_buffering=True)
    MIN_FLOOR_AREA_CM2 = float(min_floor_m2) * 1e4
    source = SLICE / PROXY_VERSION
    out = SLICE / ("V012_Z1Collision_" + proxy_version)
    assert out != source and not (out / "manifest.json").exists(), "Choose a new version"
    out.mkdir(parents=True, exist_ok=True)
    old = json.loads((source / "manifest.json").read_text(encoding="utf-8"))["report"]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    clock, timing = time.time(), {}

    def lap(name):
        nonlocal clock
        timing[name] = round(time.time() - clock, 1)
        clock = time.time()
        print("RIDGE_ED_COLLISION_STAGE", name, timing[name], flush=True)

    manifest, inst, meshes = load_export()
    assert manifest["instances_sha256"] == old["input_instances_sha256"], "Export changed; run a full build"
    cache = source / "cache" / cache_key(manifest)
    assert cache.name == old["cache"] and cache.exists(), cache
    cached_decimation(cache, meshes, inst)
    crust = RenderCrust()
    heights, _ = cached_raster(cache, meshes, inst, crust)
    body_bl, body_tris, _ = cached_bodies(cache, meshes, inst)
    body_ue, body_normals = ue_array(body_bl), face_normals_ue(body_bl, body_tris)
    volume = Volume(body_ue, body_tris, body_normals)
    lap("cache")
    name = f"SM_Octant_Ridge_00_{CRUST_VERSION}_R30000"
    with bpy.data.libraries.load(str(source / "Ridge_Z1Collision.blend"), link=False) as (src, dst):
        assert name in src.objects, name
        dst.objects = [name]
    crust_obj = dst.objects[0]
    bpy.context.scene.collection.objects.link(crust_obj)
    crust_verts, crust_tris, samples = crust_samples(crust_obj)
    assert vertex_hash(crust_verts, crust_tris) == old["hashes"]["crust"], "V011 crust differs from the source build"
    inside, _ = assert_inside(volume, body_ue, samples)
    lap("crust")
    floor, shell, split = split_body(body_ue, body_tris, body_normals, heights, crust)
    builder.OUT, builder.EXPORTS = out, []
    floor_obj = export_role(PROXY_NAME.format("Floor"), body_ue, body_tris, floor, "Floor")
    shell_obj = export_role(PROXY_NAME.format("Shell"), body_ue, body_tris, shell, "Shell")
    proxy_exports = builder.EXPORTS
    lap("split")
    collision = {"crust": bvh(crust_verts, crust_tris)}
    for role, obj in (("floor", floor_obj), ("shell", shell_obj)):
        v, t = mesh_arrays(obj, evaluated=False)
        collision[role] = bvh(ue_array(v), t)
    fit = measure(meshes, inst, crust, collision)
    lap("measure")
    proxy_verts = body_ue[np.unique(body_tris[floor | shell].ravel())]
    report = dict(old)
    report.update({
        "version": out.name, "resplit_of": source.name,
        "parameters": dict(old["parameters"], min_floor_area_cm2=MIN_FLOOR_AREA_CM2),
        "split": split, "inside_assert": inside, "fit": fit,
        "bounds_cm": {"min": np.minimum(crust_verts.min(axis=0), proxy_verts.min(axis=0)).round(3).tolist(),
                      "max": np.maximum(crust_verts.max(axis=0), proxy_verts.max(axis=0)).round(3).tolist()},
        "abs_max_component_cm": float(np.abs(np.concatenate([crust_verts, proxy_verts])).max()),
        "proxies": proxy_exports,
        "hashes": dict(old["hashes"], floor=proxy_hash(floor_obj), shell=proxy_hash(shell_obj)),
        "timing_s": timing})
    (out / "manifest.json").write_text(json.dumps({"exports": proxy_exports, "report": report}, indent=1) + "\n",
                                       encoding="utf-8")
    bpy.ops.wm.save_as_mainfile(filepath=str(out / "Ridge_Z1Collision.blend"), check_existing=False)
    print("RIDGE_ED_COLLISION_READY " + json.dumps({"inside": inside["pass"], "fit": fit["floor_or_crust"],
                                                   "floor": split["floor"], "shell": split["shell"],
                                                   "floor_components": split["floor_components"]}))
    assert inside["pass"], inside


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if args and args[0] == "split":
        # -- split <새 프록시 버전(V002…)> <Floor 최소 면적 ㎡>
        assert len(args) == 3, args
        resplit(args[1], args[2])
    else:
        # -- build [V001|V003] [fresh]
        assert args and args[0] == "build", args
        rest = args[1:]
        if rest and rest[0] in PROFILES:
            use_profile(rest.pop(0))
        assert rest in ([], ["fresh"]), args
        build(*rest)
