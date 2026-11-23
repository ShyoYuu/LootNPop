"""UE Cmd에서 Z1 충돌 프록시(U5) 입력을 읽는다. 열린 Z1AssemblyWall 레벨(JSON 재현본, verify PASS)을 대상으로 한다.

인자:
- stats: 모든 인스턴스를 분류(식생·물·납작한 타일·덩어리)하고 크기·지각 위 돌출·묻힘·fallback 삼각형 수를 집계한다.
  결과는 Saved .../AssemblyTest_20261119/Collision/Stats_<버전>.json. 레벨과 에셋은 바꾸지 않는다.
- export: U5 기준으로 인스턴스를 지면층(래스터)·덩어리·두께 줄 판으로 나눠 world transform(UE cm, 쿼터니언)과 쓰인 고유 메시의
  Nanite fallback(RenderData LOD0) 정점·삼각형 이진 파일을 Collision/Export_<버전>/에 쓴다. Blender
  `BuildRidgeEDCollision.py`의 입력이다. 레벨과 에셋은 바꾸지 않는다.
- import: Blender가 구운 충돌 지각(Z1MassifCover V011)과 SM_Ridge_EDTerrain_V001_Z1_Floor·_Shell을 반입하고
  complex-as-simple 단일 trimesh·옥탄트 범위·성분 절댓값 32767 이하를 검사한다. 저장은 하지 않는다(사용자가 저장).
- layout: EDTerrainLayout_Z1.json을 V035(V034 + U5 충돌 3개)로 올리고 충돌 빌드 지문(collision_build)을 쓴다.
  그다음 `RidgeZ1Layout.py build V035` → (다음 Cmd) `verify`로 시험 레벨을 JSON에서 만들고 대조한다.
- check: 열린 레벨의 충돌 액터가 옥탄트 제작 가이드 §4.2 역할 표(태그·프로필)와 충돌 설정을 지키는지 검사한다.
  Level Instance가 로드한 하위 레벨까지 포함해 충돌 3개 외에 충돌이 켜진 StaticMeshComponent가 없어야 한다.
- visualoff save: ED 시각 레벨(VISUAL_LEVELS)을 차례로 열어 모든 StaticMeshComponent(ISM·BPP 상속 컴포넌트 포함)의
  충돌을 끄고 저장한다. 레벨의 BPP(Packed Level Blueprint) 액터는 블루프린트 템플릿도 끄고 컴파일·저장한다(템플릿이
  use_default_collision이면 레벨 인스턴스 값이 저장 때 유지되지 않는다). ED 파생 에셋은 Git 밖이므로 재Migrate 뒤에는 이 명령을 다시 돌린다. 끝나면 V035를 연다.
"""
import importlib
import json
import math
from pathlib import Path
import re
import sys

import unreal

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parents[1])]
import RidgeZ1Layout as layout

importlib.reload(layout)
test, spec = layout.test, layout.spec

OUT = test.OUT / "Collision"
# 판 판정은 드레이프(drape_flat)와 같다: 스케일을 곱한 메시 상자 Z가 XY 최대의 0.35배 미만이면 납작한 타일이다.
FLAT_RATIO = 0.35
WATER = ("SM_WaterPlane",)
SIZE_STEPS = (0, 50, 100, 200, 400, 800)
# 인스턴스 층의 배치 지각(EDTerrainLayout_Z1.json crust_build.placement).
PLACEMENT = {"CR_Z1_Drape": lambda x, y: spec.massif_cover_ground(x, y, "V002"),
             "CR_Z1_RimBand": spec.massif_terrace_ground, "CR_Z1_RimBandNorth": spec.massif_spur_ground,
             "CR_Z1_Flat": spec.massif_spur_ground}
PROTRUDE_STEPS = (0, 25, 50, 100, 200, 400)


# 3D_Plants 밖의 식생(ED IvyTest 덩굴·나무, RockFormation 덩굴). IvyTest의 SM_Embankment는 둑 지면이라 식생이 아니다.
PLANT_NAMES = ("Vine", "Ivy", "Hornbeam", "MoneyPlant")


def category(path, size):
    name = path.rsplit("/", 1)[1]
    if "/3D_Plants/" in path or any(word in name for word in PLANT_NAMES):
        return "plant"
    if any(name.startswith(w) for w in WATER):
        return "water"
    sx, sy, sz = size
    return "flat" if sz < FLAT_RATIO * max(sx, sy) else "solid"


def axes(t):
    r = t.rotation
    return [r.rotate_vector(u) for u in (unreal.Vector(1, 0, 0), unreal.Vector(0, 1, 0), unreal.Vector(0, 0, 1))]


# 렌더링 지각 버전별 높이 함수(묻힘·돌출 판정). V037부터 V012(V010 + BossValley 경사로)다.
RENDER_GROUND = {"V010": spec.massif_flat_ground, "V012": spec.massif_ramp_ground}


def read_instances(version=None, render="V010"):
    # 열린 레벨의 모든 인스턴스를 (소유 Level Instance, 분류, 크기, 지각 위 돌출·바닥, 묻힘, transform) 행으로 읽는다.
    # 시험 레벨(Z1AssemblyWall_Vxxx)은 이름에서 버전을 읽고, U6부터는 JSON에서 만든 베이크 LVI(Z1Bake_*)를 열고 버전을 준다.
    level = layout.world_package()
    match = re.search(r"Z1AssemblyWall_(V\d+)", level)
    assert match or (version and "/Z1Bake_" in level), level
    version = version or match.group(1)
    top, nested = layout.level_actors()
    crust = layout.CrustHeight(RENDER_GROUND[render])
    meshes, rows = {}, []
    owners = {}
    for actor in top:
        if isinstance(actor, unreal.LevelInstance):
            loaded = actor.get_loaded_level()
            assert loaded, actor.get_actor_label()
            owners[loaded.get_path_name()] = actor.get_actor_label()
    groups = [("top", [a for a in top if isinstance(a, unreal.StaticMeshActor)])]
    groups += [(owners.get(level_path, level_path), actors) for level_path, actors in nested.items()]
    for owner, actors in groups:
        role = "layer" if owner.startswith("CR_Z1_") and owner != "CR_Z1_LargeAssembly" else "assembly"
        for key, mesh, t in layout.actor_instances(actors):
            path = test.package(mesh)
            if path not in meshes:
                box = mesh.get_bounding_box()
                nanite = mesh.get_editor_property("nanite_settings")
                meshes[path] = {"box": box, "tris": mesh.get_num_triangles(0), "nanite": bool(nanite.enabled),
                                "count": 0}
            info = meshes[path]
            info["count"] += 1
            box = info["box"]
            s = t.scale3d
            ext = [abs((box.max.x - box.min.x) * s.x), abs((box.max.y - box.min.y) * s.y),
                   abs((box.max.z - box.min.z) * s.z)]
            center = unreal.MathLibrary.transform_location(t, (box.min + box.max) * 0.5)
            x, y, h = spec.ridge.local((center.x, center.y, center.z))
            up = center * (-1.0 / center.length())
            half = sum(abs(a.dot(up)) * e * 0.5 for a, e in zip(axes(t), ext))
            ground = crust(x, y)
            in_cut = any(spec.ridge.signed_distance(x, y, cut) > 0 for cut in spec.base.CUTS)
            rows.append({"owner": owner, "role": role, "mesh": path, "cat": category(path, ext),
                         "size": max(ext), "protrude": h + half - ground, "bottom": h - half - ground,
                         "buried": (not in_cut) and role == "assembly" and h + half < ground - layout.BURIED_MARGIN_CM,
                         "cut": in_cut, "tris": info["tris"], "xy": (x, y), "ground": ground, "t": t,
                         "mesh_obj": mesh})
    return version, level, rows, meshes


def stats():
    version, level, rows, meshes = read_instances()
    summary = summarize(rows)
    mesh_rows = sorted(({"mesh": p, "tris": m["tris"], "nanite": m["nanite"], "count": m["count"]}
                        for p, m in meshes.items()), key=lambda r: -r["tris"] * r["count"])
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / ("Stats_%s.json" % version)).write_text(json.dumps({"level": level, "summary": summary, "meshes": mesh_rows},
                                                              indent=1), encoding="utf-8")
    unreal.log("Z1_COLLISION_STATS %s instances %d meshes %d" % (version, len(rows), len(meshes)))


def quantiles(values):
    values = sorted(values)
    if not values:
        return {}
    return {q: round(values[min(len(values) - 1, int(q / 100 * len(values)))], 1) for q in (5, 25, 50, 75, 95)}


def bucket(value, steps):
    label = steps[0]
    for step in steps:
        if value >= step:
            label = step
    return label


def summarize(rows):
    out = {"instances": len(rows), "by_cat": {}, "buried": sum(r["buried"] for r in rows),
           "in_cut": sum(r["cut"] for r in rows)}
    for r in rows:
        if r["buried"]:
            continue
        cat = out["by_cat"].setdefault(r["cat"], {"count": 0, "tris": 0, "by_role": {}, "size": {}, "protrude": {},
                                                   "size_x_protrude": {}})
        cat["count"] += 1
        cat["tris"] += r["tris"]
        cat["by_role"][r["role"]] = cat["by_role"].get(r["role"], 0) + 1
        sb, pb = bucket(r["size"], SIZE_STEPS), bucket(r["protrude"], PROTRUDE_STEPS)
        for table, k in ((cat["size"], sb), (cat["protrude"], pb), (cat["size_x_protrude"], "%d|%d" % (sb, pb))):
            slot = table.setdefault(str(k), [0, 0])
            slot[0] += 1
            slot[1] += r["tris"]
    # 납작한 타일 윗면과 렌더링 지각의 높이 차(_Floor 높이 기준 근거). 지각에서 1m 넘게 떨어진 타일은 띠·조립체 판으로 보고 뺀다.
    tops = sorted(r["protrude"] for r in rows if r["cat"] == "flat" and not r["buried"] and abs(r["protrude"]) < 100)
    if tops:
        out["flat_top_minus_render_crust_cm"] = {q: round(tops[min(len(tops) - 1, int(q / 100 * len(tops)))], 1)
                                                 for q in (5, 25, 50, 75, 95)}
        out["flat_top_samples"] = len(tops)
    out["by_owner"] = {}
    for r in rows:
        o = out["by_owner"].setdefault(r["owner"], {"count": 0, "buried": 0, "tris": 0, "cat": {}})
        o["count"] += 1
        o["buried"] += r["buried"]
        o["tris"] += 0 if r["buried"] else r["tris"]
        if not r["buried"]:
            o["cat"][r["cat"]] = o["cat"].get(r["cat"], 0) + 1
    # 층별 납작한 타일 윗면 - 렌더링 지각 / - 배치 지각(GX-024) 분포.
    out["flat_top_by_owner"] = {}
    for owner, ground in PLACEMENT.items():
        crust = layout.CrustHeight(ground)
        render, placed = [], []
        for r in rows:
            if r["owner"] == owner and r["cat"] == "flat" and abs(r["protrude"]) < 150:
                render.append(r["protrude"])
                placed.append(r["protrude"] + r["ground"] - crust(*r["xy"]))
        out["flat_top_by_owner"][owner] = {"samples": len(render), "minus_render": quantiles(render),
                                           "minus_placement": quantiles(placed)}
    # 구역 범위(옥탄트 로컬 cm, 묻힘 제외).
    xs = [r["xy"][0] for r in rows if not r["buried"]]
    ys = [r["xy"][1] for r in rows if not r["buried"]]
    out["extent_cm"] = [min(xs), max(xs), min(ys), max(ys)]
    return out


# ---- 내보내기(U5 기준, 2026-11-22 사용자 확인) ------------------------------------------------------------------
# - 제외: 묻힘(GX-030)·식생·물·동굴 절개 안·옥탄트 메시(렌더링 지각·보존 바위, 원점 배치).
# - 덩어리는 크기 100cm 미만 제외, 돌출 50cm 미만은 지면층(래스터), 나머지는 덩어리 프록시(remesh).
# - 납작한 타일은 크기 50cm 미만 제외. 인스턴스 층(드레이프·띠·평면 묶음) 타일은 생성 규칙상 지각 위 지면이므로 모두 지면층이다
#   (기울어진 둑 타일은 상자 바닥·꼭대기가 지각에서 멀어 보인다. 이상치는 래스터 높이 창이 거른다). 조립체·산괴 타일은 상자
#   바닥이 렌더링 지각 ±150cm 안이고 돌출 250cm 이하면 지면층, 그 밖(선반 위 타일)은 두께를 준 뒤 덩어리 프록시에 넣는다.
GROUND, BODY, PLATE = 0, 1, 2
BODY_MIN_SIZE_CM, BODY_MIN_PROTRUDE_CM = 100.0, 50.0
TILE_MIN_SIZE_CM, TILE_BOTTOM_WINDOW_CM, TILE_MAX_PROTRUDE_CM = 50.0, 150.0, 250.0
OCTANT_MESH = ("/Z1MassifCover_", "_PreservedRocks_", "/Z1Collision_")
EXPORT_DIR = "Export_{}"


def classify(r):
    if r["buried"] or r["cut"] or r["cat"] in ("plant", "water") or any(k in r["mesh"] for k in OCTANT_MESH):
        return None
    if r["cat"] == "solid":
        if r["size"] < BODY_MIN_SIZE_CM:
            return None
        return GROUND if r["protrude"] < BODY_MIN_PROTRUDE_CM else BODY
    if r["size"] < TILE_MIN_SIZE_CM:
        return None
    if r["role"] == "layer":
        return GROUND
    if abs(r["bottom"]) <= TILE_BOTTOM_WINDOW_CM and r["protrude"] <= TILE_MAX_PROTRUDE_CM:
        return GROUND
    return PLATE


def mesh_arrays(mesh):
    # Nanite fallback(RenderData LOD0)의 정점(메시 로컬 cm)과 삼각형. 4M 원본(HiRes 소스)은 읽지 않는다.
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.set_editor_property("lod_type", unreal.GeometryScriptLODType.RENDER_DATA)
    lod.set_editor_property("lod_index", 0)
    dm, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(
        mesh, unreal.DynamicMesh(), unreal.GeometryScriptCopyMeshFromAssetOptions(), lod)
    assert outcome == unreal.GeometryScriptOutcomePins.SUCCESS, mesh.get_path_name()
    queries, lists = unreal.GeometryScript_MeshQueries, unreal.GeometryScript_List
    _, positions, gaps = queries.get_all_vertex_positions(dm, False)
    _, triangles, tri_gaps = queries.get_all_triangle_indices(dm, False)
    assert not gaps and not tri_gaps, mesh.get_path_name()
    verts = [c for v in lists.convert_vector_list_to_array(positions) for c in (v.x, v.y, v.z)]
    tris = [c for t in lists.convert_triangle_list_to_array(triangles) for c in (t.x, t.y, t.z)]
    return verts, tris


def export(version=None, render="V010"):
    # 인자 없이는 열린 시험 레벨(V034 방식). U6부터: `export V037 V012`(JSON V037로 만든 Z1Bake LVI, 렌더링 지각 V012).
    import array
    import hashlib
    assert not test.dirty(), test.dirty()
    version, level, rows, meshes = read_instances(version, render)
    out = OUT / EXPORT_DIR.format(version)
    (out / "meshes").mkdir(parents=True, exist_ok=True)
    index, mesh_rows, instances = {}, [], []
    counts = {"ground": 0, "body": 0, "plate": 0, "excluded": 0}
    names = {GROUND: "ground", BODY: "body", PLATE: "plate"}
    per_owner = {}
    for r in rows:
        cls = classify(r)
        owner = per_owner.setdefault(r["owner"], {"ground": 0, "body": 0, "plate": 0, "excluded": 0})
        if cls is None:
            counts["excluded"] += 1
            owner["excluded"] += 1
            continue
        counts[names[cls]] += 1
        owner[names[cls]] += 1
        path = r["mesh"]
        if path not in index:
            verts, tris = mesh_arrays(r["mesh_obj"])
            # 동적 메시 변환은 퇴화 삼각형을 버리므로 LOD0 삼각형 수와 조금 다를 수 있다.
            assert abs(len(tris) // 3 - meshes[path]["tris"]) <= max(2, meshes[path]["tris"] // 200), (
                path, len(tris) // 3, meshes[path]["tris"])
            data = array.array("i", [len(verts) // 3, len(tris) // 3]).tobytes() + array.array("f", verts).tobytes()                 + array.array("i", tris).tobytes()
            name = "m%03d.bin" % len(index)
            (out / "meshes" / name).write_bytes(data)
            index[path] = len(index)
            mesh_rows.append({"mesh": path, "file": name, "vertices": len(verts) // 3, "triangles": len(tris) // 3,
                              "lod0_triangles": meshes[path]["tris"], "sha256": hashlib.sha256(data).hexdigest()})
        t = r["t"]
        p, q, sc = t.translation, t.rotation, t.scale3d
        instances.append([index[path], cls] + [round(v, 4) for v in (p.x, p.y, p.z)]
                         + [round(v, 8) for v in (q.x, q.y, q.z, q.w)] + [round(v, 6) for v in (sc.x, sc.y, sc.z)])
    body = json.dumps({"level": level, "version": version, "columns": ["mesh", "class", "tx", "ty", "tz", "qx", "qy",
                                                                      "qz", "qw", "sx", "sy", "sz"],
                       "classes": names, "instances": instances}, separators=(",", ":"))
    (out / "instances.json").write_text(body, encoding="utf-8")
    manifest = {"level": level, "version": version, "rule": {
        "body_min_size_cm": BODY_MIN_SIZE_CM, "body_min_protrude_cm": BODY_MIN_PROTRUDE_CM,
        "tile_min_size_cm": TILE_MIN_SIZE_CM, "tile_bottom_window_cm": TILE_BOTTOM_WINDOW_CM,
        "tile_max_protrude_cm": TILE_MAX_PROTRUDE_CM, "layer_tiles": "ground", "flat_ratio": FLAT_RATIO, "excluded": [
            "buried", "inside_cuts", "plant", "water", "octant_mesh"]},
        "counts": counts, "per_owner": per_owner, "meshes": mesh_rows,
        "instances_sha256": hashlib.sha256(body.encode()).hexdigest()}
    (out / "manifest.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    unreal.log("Z1_COLLISION_EXPORT %s %s meshes %d" % (version, json.dumps(counts), len(mesh_rows)))


# ---- 반입·재현 원본·검사 -------------------------------------------------------------------------------------------
SLICE = Path(r"D:/UnrealProjects/LootNPop/Saved/SliceOctant/Ridge_00")
CRUST_VERSION = "V011"
PROXY_VERSION = "V012_Z1Collision_V001"
PROXY_FOLDER = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1Collision_V001"


# 프록시 버전별 충돌 지각과 Blender 명령. V002는 Floor 하한만 바꿔 다시 나눈 판(GX-032), V003은 U6 정리본(GX-033)·경사로
# 렌더링 지각 V012에서 다시 구운 판이다(충돌 지각 V013).
PROXY_BUILDS = {"V001": ("V011", "-- build"), "V002": ("V011", "-- build ; -- split V002 50"), "V003": ("V013", "-- build V003")}


def use_proxy(version):
    global PROXY_VERSION, PROXY_FOLDER, CRUST_VERSION
    CRUST_VERSION = PROXY_BUILDS[version][0]
    PROXY_VERSION = "V012_Z1Collision_" + version
    PROXY_FOLDER = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1Collision_" + version
# 옥탄트 제작 가이드 §4.2 역할 표. 지각은 기존 반입(AssembleRidgeOctant.place_geometry)과 같이 CoarseSupport를 더한다.
ROLE_CONTRACT = {
    "Crust": (["LNP.Surface.Blocker", "LNP.Surface.Static", "LNP.Surface.Support", "LNP.Surface.CoarseSupport"],
              "LNPStaticTerrain"),
    "Floor": (["LNP.Surface.Blocker", "LNP.Surface.Static", "LNP.Surface.Support"], "LNPStaticTerrain"),
    "Shell": (["LNP.Surface.Blocker", "LNP.Surface.Static"], "LNPStaticBlocker"),
}
COMPONENT_LIMIT_CM = 32767.0
# 충돌이 켜진 채 V035에 들어오는 시각 레벨(2026-11-22 조사). 충돌은 V011·Floor·Shell만 맡는다(GX-031).
# Band·Flat 층과 나머지 ED 조립체는 이미 NO_COLLISION이다. Drape는 생성기가 원본 충돌을 복사하던 V006이다.
VISUAL_LEVELS = ["/Game/LNPBake/CloseRange/CR_LargeAssembly"] + [
    "/Game/PCG/Assets/PCGAssemblies/Forest/PL_LargeCliff_" + s for s in ("01b", "01c", "02b", "02c", "02d", "02e")] + [
    "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1MassifDrape_V006/Z1MassifDrape_V006"]


def collision_meshes():
    crust_dir = SLICE / spec.MASSIF_COVER_VERSIONS[CRUST_VERSION][0]
    rows = []
    for folder, out in ((test.MASSIF_FOLDER.format(CRUST_VERSION), crust_dir), (PROXY_FOLDER, SLICE / PROXY_VERSION)):
        manifest = json.loads((out / "manifest.json").read_text(encoding="utf-8"))
        rows += [(folder, out, manifest, item) for item in manifest["exports"]]
    return rows


def mesh_check(asset):
    # 단순 충돌 도형이 없고 complex-as-simple인 단일 trimesh인지, 정점이 옥탄트(+X+Y+Z) 안이고 성분 절댓값 32767 이하인지.
    import AssembleRidgeOctant as assembly
    body = asset.get_editor_property("body_setup")
    geom = body.get_editor_property("agg_geom")
    simple = sum(len(geom.get_editor_property(k)) for k in ("box_elems", "convex_elems", "sphere_elems", "sphyl_elems",
                                                          "tapered_capsule_elems"))
    positions = assembly.native_mesh(asset)[1]
    low = min(min(p.x, p.y, p.z) for p in positions)
    high = max(max(abs(p.x), abs(p.y), abs(p.z)) for p in positions)
    trace = body.get_editor_property("collision_trace_flag")
    complex_as_simple = trace == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
    return {"triangles": asset.get_num_triangles(0), "lods": asset.get_num_lods(), "simple_shapes": simple,
            "complex_as_simple": complex_as_simple, "min_component_cm": round(low, 3),
            "abs_max_component_cm": round(high, 3),
            "pass": simple == 0 and complex_as_simple and low > -0.05 and high <= COMPONENT_LIMIT_CM
            and asset.get_num_lods() == 1}


def strip_simple(asset):
    # FBX 반입은 자동 상자 충돌을 만든다. complex-as-simple이면 질의에 쓰이지 않지만 단일 trimesh 계약을 지키려고 지운다.
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(asset)
    return asset


def recheck():
    # 이미 반입한 에셋의 단순 도형을 지우고 검사를 다시 쓴다(재반입은 새 버전을 요구한다).
    path = OUT / "Import_V001.json"
    report = json.loads(path.read_text(encoding="utf-8"))
    for key, row in report.items():
        row["check"] = mesh_check(strip_simple(unreal.load_asset(key)))
    path.write_text(json.dumps(report, indent=1), encoding="utf-8")
    unreal.log("Z1_COLLISION_RECHECK %s" % json.dumps({k.rsplit("/", 1)[1]: v["check"]["pass"] for k, v in report.items()}))


def import_collision(proxy="V001"):
    # 프록시 버전을 주면 그 버전을 반입한다. 이미 반입된 충돌 지각(V011)은 건너뛴다(재분할은 지각을 바꾸지 않는다).
    use_proxy(proxy)
    import AssembleRidgeOctant as assembly
    assert not test.dirty(), test.dirty()
    # 저장은 사용자가 한다(U5 제약). import_assets 안의 저장만 막는다.
    assembly.save = lambda asset: None
    report = {}
    done = set()
    for folder, out, manifest, _ in collision_meshes():
        if out in done:
            continue
        done.add(out)
        if all(unreal.EditorAssetLibrary.does_asset_exist(folder + "/" + item["name"]) for item in manifest["exports"])                 and out.name != PROXY_VERSION:
            continue
        for row in assembly.import_assets(manifest, folder, out):
            row["check"] = mesh_check(strip_simple(unreal.load_asset(row["path"])))
            report[row["path"]] = row
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / ("Import_%s.json" % proxy)).write_text(json.dumps(report, indent=1), encoding="utf-8")
    unreal.log("Z1_COLLISION_IMPORT %s" % json.dumps({k.rsplit("/", 1)[1]: v["check"]["pass"] for k, v in report.items()}))


def role_of(mesh):
    for role in ("Floor", "Shell"):
        if mesh.endswith("_" + role):
            return role
    assert mesh.endswith("_R30000"), mesh
    return "Crust"


def layout_v035(version="V035", proxy="V001"):
    # JSON 버전을 올리고 충돌 행을 그 프록시 버전으로 바꾼다. V036은 Floor 하한 50㎡로 다시 나눈 프록시 V002(GX-032)다.
    use_proxy(proxy)
    data = json.loads(layout.LAYOUT.read_text(encoding="utf-8"))
    assert data["version"] in ("V034", "V035", "V036", "V037"), data["version"]
    data["version"] = version
    data["level"] = test.WALL_FOLDER.format(version)
    data["actors"] = [row for row in data["actors"] if row["role"] != "collision"]
    builds = {}
    for folder, out, manifest, item in collision_meshes():
        mesh = folder + "/" + item["name"]
        role = role_of(mesh)
        tags, profile = ROLE_CONTRACT[role]
        data["actors"].append({"label": item["name"], "role": "collision", "collision_role": role, "mesh": mesh,
                               "material_overrides": {}, "tags": tags, "profile": profile, "visible": False,
                               "world_cm": [0.0, 0.0, 0.0], "world_rotation_deg": [0.0, 0.0, 0.0],
                               "scale": [1.0, 1.0, 1.0]})
        builds[item["name"]] = {"triangles": item["triangles"], "bounds_cm": item["bounds_cm"]}
    data["actors"].sort(key=lambda row: row["label"])
    report = json.loads((SLICE / PROXY_VERSION / "manifest.json").read_text(encoding="utf-8"))["report"]
    data["collision_build"] = {
        "decision": {"V001": "GX-031", "V002": "GX-031, GX-032"}.get(proxy, "GX-031, GX-032, GX-033"),
        "export": ("ExportRidgeEDCollision.py export (open Z1AssemblyWall_V034)" if proxy != "V003" else
                   "ExportRidgeEDCollision.py export V037 V012 (open Z1Bake LVI built from JSON V037)"),
        "blender": "blender --background --python Scripts/Blender/BuildRidgeEDCollision.py " + PROXY_BUILDS[proxy][1],
        "ue": "ExportRidgeEDCollision.py import -> visualoff save -> layout -> RidgeZ1Layout.py build V035 -> verify -> ExportRidgeEDCollision.py check",
        "input_instances_sha256": report["input_instances_sha256"], "input_counts": report["input_counts"],
        "parameters": report["parameters"], "meshes": builds, "hashes": report["hashes"],
        "inside_assert": report["inside_assert"], "fit": report["fit"]["floor_or_crust"],
        "output": ["Saved/SliceOctant/Ridge_00/" + spec.MASSIF_COVER_VERSIONS[CRUST_VERSION][0] + "/",
                   "Saved/SliceOctant/Ridge_00/" + PROXY_VERSION + "/"]}
    temporary = layout.LAYOUT.with_suffix(".tmp")
    temporary.write_text(json.dumps(data, indent=1, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")
    temporary.replace(layout.LAYOUT)
    unreal.log("Z1_COLLISION_LAYOUT %s collision %d" % (version, len(builds)))


def check():
    top, _ = layout.level_actors()
    rows, issues = [], []
    for actor in top:
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        comp = actor.static_mesh_component
        if comp.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
            continue
        mesh = test.package(comp.static_mesh)
        role = role_of(mesh)
        tags, profile = ROLE_CONTRACT[role]
        got = sorted(str(t) for t in comp.get_editor_property("component_tags"))
        row = {"label": actor.get_actor_label(), "role": role, "tags": got,
               "profile": str(comp.get_collision_profile_name()), "collision": str(comp.get_collision_enabled()),
               "mobility": str(comp.mobility), "visible": comp.is_visible(), "mesh": mesh_check(comp.static_mesh)}
        ok = (got == sorted(tags) and row["profile"] == profile and row["mesh"]["pass"]
              and comp.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS
              and comp.mobility == unreal.ComponentMobility.STATIC)
        row["pass"] = ok
        if not ok:
            issues.append(row["label"])
        rows.append(row)
    if sorted(r["role"] for r in rows) != ["Crust", "Floor", "Shell"]:
        issues.append("roles %s" % sorted(r["role"] for r in rows))
    visual = {}
    for actors in layout.level_actors()[1].values():
        for name, count in colliding(actors).items():
            visual[name] = visual.get(name, 0) + count
    if visual:
        issues.append("visual collision %d" % sum(visual.values()))
    report = {"level": layout.world_package(), "pass": not issues, "issues": issues, "actors": rows,
              "visual_collision": visual}
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / ("Check_%s.json" % layout.world_package().rsplit("_", 1)[-1])).write_text(json.dumps(report, indent=1), encoding="utf-8")
    unreal.log("Z1_COLLISION_CHECK %s %s" % ("PASS" if not issues else "FAIL", json.dumps(issues)))


def colliding(actors):
    # 충돌이 켜진 StaticMeshComponent(ISM 포함) 수를 레벨 패키지별로 센다(인스턴스 수가 아니라 컴포넌트 수).
    counts = {}
    for actor in actors:
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            if comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
                name = actor.get_level().get_outermost().get_name()
                counts[name] = counts.get(name, 0) + 1
    return counts


def blueprint_off(cls, done):
    # BPP 템플릿(SCS)의 StaticMeshComponent 충돌을 끈다. 바뀐 블루프린트만 컴파일한다(저장은 호출자).
    path = cls.get_path_name().rsplit(".", 1)[0]
    if path in done or not path.startswith("/Game/"):
        return
    bp = unreal.load_asset(path)
    if not isinstance(bp, unreal.Blueprint):
        done[path] = 0
        return
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    changed = 0
    for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
        template = lib.get_object(lib.get_data(handle))
        if isinstance(template, unreal.StaticMeshComponent) and (
                template.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION
                or template.get_editor_property("use_default_collision")
                or str(template.get_collision_profile_name()) != "NoCollision"):
            # Python 속성 설정은 패키지를 더럽히지 않는다. modify()가 없으면 저장되지 않는다.
            bp.modify()
            template.modify()
            template.set_editor_property("use_default_collision", False)
            template.set_collision_profile_name("NoCollision")
            template.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            changed += 1
    if changed:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    # 앞선 실행에서 메모리에서만 바뀐 템플릿도 디스크에 남도록 처음 본 블루프린트는 늘 저장한다.
    assert unreal.EditorAssetLibrary.save_asset(path, False), path
    done[path] = changed


def visual_off(mode=""):
    # 사용자 승인(2026-11-22 "U5에서 끄기")으로 VISUAL_LEVELS만 열고 저장한다. 다른 더러운 패키지가 있으면 멈춘다.
    assert mode == "save", "visualoff saves the listed levels; pass 'save'"
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    report, blueprints = {}, {}
    for level in VISUAL_LEVELS:
        assert not test.dirty(), test.dirty()
        assert levels.load_level(level), level
        changed = 0
        # 블루프린트 컴파일은 레벨 액터를 다시 만들므로 템플릿을 먼저 다 끄고 액터 목록을 새로 받는다.
        for cls in {a.get_class() for a in unreal.GameplayStatics.get_all_actors_of_class(test.world(), unreal.Actor)}:
            if isinstance(cls, unreal.BlueprintGeneratedClass):
                blueprint_off(cls, blueprints)
        for actor in unreal.GameplayStatics.get_all_actors_of_class(test.world(), unreal.Actor):
            for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
                if comp.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
                    continue
                # 메시 기본 충돌로 되돌아가지 않게 use_default_collision부터 끈다(StaticMeshActor는 기본 True).
                actor.modify()
                comp.modify()
                comp.set_editor_property("use_default_collision", False)
                # 이름 있는 프로필(BlockAll 등)이 남으면 로드 때 프로필이 충돌 설정을 다시 덮는다(PL_LargeCliff_02* 사례).
                comp.set_collision_profile_name("NoCollision")
                comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
                changed += 1
        dirty = test.dirty()
        assert bool(dirty) or not changed, level
        if dirty:
            assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True), level
        left = colliding(unreal.GameplayStatics.get_all_actors_of_class(test.world(), unreal.Actor))
        report[level] = {"components_off": changed, "saved": dirty, "left": left}
        assert not left, (level, left)
    assert levels.load_level(test.WALL_FOLDER.format("V035"))
    OUT.mkdir(parents=True, exist_ok=True)
    report["blueprints"] = blueprints
    (OUT / "VisualOff_V001.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
    unreal.log("Z1_COLLISION_VISUALOFF %s" % json.dumps({k.rsplit("/", 1)[1]: v["components_off"] for k, v in report.items()
                                                        if k != "blueprints"}))


if __name__ == "__main__":
    {"stats": stats, "export": export, "import": import_collision, "recheck": recheck, "layout": layout_v035,
     "check": check, "visualoff": visual_off}[sys.argv[1]](*sys.argv[2:])
