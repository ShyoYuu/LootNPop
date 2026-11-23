"""UE Cmd에서 Z1 재현 원본(EDTerrainLayout_Z1.json)으로 베이크 가능한 Ridge WIP LVI를 만들고 굽는다(1-BR U6).

시험 레벨(Z1AssemblyWall_V0xx)은 조명·Sky가 들어 있고 마커가 없어 옥탄트 LVI로 쓸 수 없다. 이 스크립트는 JSON에서
조명·Sky를 뺀 구성으로 새 LVI를 만든다.
- 개별 메시: 렌더링 지각·V002 사본은 NoCollision 시각 메시, 충돌 지각·Z1 프록시는 숨긴 충돌 행(GX-031).
- 보존 바위(동굴·부유섬·석교): 시험 레벨에서는 충돌이 꺼져 있지만 LVI에서는 기본 WIP와 같이 Floor(Support+Blocker)·
  Shell(Blocker)로 되돌린다. 화면에도 보인다.
- 시각 Level Instance 15개: 베이커 수집기는 LVI 최상위 액터만 읽으므로 하위 레벨은 베이크 입력이 아니다. JSON 행에
  `cull`(GX-033)이 있으면 규칙으로 덜어낸 월드 좌표 사본 레벨을 원점에 놓는다. 런타임 중첩 로드는 PIE에서 확인했다.
- 마커: U3 재투영 마커(Saved/SliceOctant/Ridge_00/V012/placement.json) 45개. JSON `markers`에 있는 행은 그 값으로
  바꾼다. Pod는 베이커가 가장자리 여유 250cm·투영 100cm를 요구하므로 두 단계로 놓는다: Pod 없이 구운 판의 스폰 후보
  (Candidates_<판>.json)에서 원래 자리에 가장 가까운 유효 후보(가장자리 여유 260cm 이상·높이 차 4m 안)로 옮긴다.

인자:
- build <버전> [omit=<PodId,...>|all] [sites=<후보 판>] [discard=/Temp/Untitled_N]: 새 LVI를 만들고 저장한다(조명 원본
  레벨이 필요 없다). discard는 앞선 build 실패가 남긴 저장 안 된 임시 맵만 버린다. 결과 Saved .../Bake/Build_<버전>.json.
- check: 열린 LVI를 검사한다(build와 다른 Cmd 실행에서. 같은 실행 안에서는 Level Instance가 아직 로드되지 않는다).
- bake <버전>: TestMap03을 열고 `LNP.SurfaceNav.BakeOctant`로 굽고 수치·이음매·Pod 같은-Layer 후보를 보고한다.
  스폰 후보 전체는 Candidates_<버전>.json에 쓴다.
- nav <버전>: 저장된 SurfaceData에서 Z1 마커·동선 표본 지점의 Nav component를 `ExplainNavGap`으로 읽는다.
- survey <버전>: 열린 LVI에서 Z1 일대 3m 격자의 걷는 면 높이와 바로 위 Shell 막힘을 적는다(평면도 근거).
- pool <버전>: 정식 풀을 복제해 Ridge 정의 하나만 새 LVI·SurfaceData로 바꾼 검사용 풀을 만든다(정식 풀은 그대로).
검사용 풀의 임시 선택은 Python에서 `LNPSettings.OctantPool`이 보이지 않아 MCP `ObjectTools.set_properties`로 한다
(`/Script/LootNPop.Default__LNPSettings`, `{"octantPool": "<풀>.<풀>"}`, SaveConfig 안 함). 원래 값과 INI·정식 풀 해시는
먼저 Bake/SelectionState.json에 적고, PIE 뒤 되돌려 해시를 대조한다. 에디터를 다시 켜면 선택은 원래 값으로 돌아간다.
"""
import hashlib
import importlib
import json
import math
from pathlib import Path
import re
import struct
import sys

import unreal

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parents[1])]
import RidgeZ1Layout as layout

importlib.reload(layout)
test, spec = layout.test, layout.spec
import ExportRidgeEDCollision as collision

importlib.reload(collision)

PROJECT = HERE.parents[2]
FOLDER = layout.EDTERRAIN + "Z1Bake_{0}/"
LEVEL = FOLDER + "LVI_Ridge_EDTerrain_Z1Bake_{0}"
SURFACE = FOLDER + "DA_OctantSurface_LVI_Ridge_EDTerrain_Z1Bake_{0}"
POOL = FOLDER + "DA_GraphicsRidgeZ1BakePool_{0}"
PRODUCTION_POOL = "/Game/Maps/OctantPoolData"
TEST_MAP = "/Game/Maps/TestMap03"
MEADOW_SURFACE = "/Game/Maps/Meadow_00/DA_OctantSurface_Meadow_00"
PLACEMENT = PROJECT / "Saved/SliceOctant/Ridge_00/V012/placement.json"
OUT = test.OUT / "Bake"
PRESERVED = {"SM_Ridge_00_V012_PreservedRocks_Floor": "Floor", "SM_Ridge_00_V012_PreservedRocks_Shell": "Shell"}
# 충돌 역할(라벨 → 역할). 시험 레벨 3개 + 보존 바위 2개.
EXPECTED_ROLES = ["Crust", "Floor", "PreservedFloor", "PreservedShell", "Shell"]
WALKABLE_DOT = 0.71
POD_SEARCH_CM = (3000.0, 12000.0)
POD_CENTER_DOT = 0.9
POD_RING_CM = (150.0, 300.0)
POD_RING_STEP_CM = 90.0
POD_SEARCH_STEP_CM = 200
POD_MOVE_LIMIT_CM = 3000
POD_SITE_EDGE_CM = 260.0   # 베이커 Pod 가장자리 여유 250cm + 여유. 후보 하한이 약 260cm다
POD_SITE_DH_CM = 400.0
PROJECTION_LIMIT_CM = 100.0


def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(value, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")


def content_file(path, extension):
    return PROJECT / "Content" / (path[len("/Game/"):] + extension)


def open_level(path):
    if layout.world_package() != path:
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(path), path


def up_at(location):
    n = math.sqrt(location.x ** 2 + location.y ** 2 + location.z ** 2)
    return unreal.Vector(-location.x / n, -location.y / n, -location.z / n)


def walkable_hits(components, x, y, h, reach):
    # 옥탄트 로컬 (x, y)의 현지 Up 직선에서 h±reach 사이의 걷는 면 교차를 모두 모은다(위에서 아래로).
    top = unreal.Vector(*spec.ridge.point(x, y, h + reach))
    bottom = unreal.Vector(*spec.ridge.point(x, y, h - reach))
    down = bottom - top
    length = down.length()
    down = down * (1.0 / length)
    up = up_at(top)
    hits = []
    for comp in components:
        start = top
        for _ in range(64):
            # bool을 돌려주는 함수는 실패면 None, 성공이면 out 인자 튜플이다(버전에 따라 앞에 bool이 붙을 수 있다).
            result = comp.line_trace_component(start, bottom, True, False, False)
            if not result or (isinstance(result[0], bool) and not result[0]):
                break
            if isinstance(result[0], bool):
                result = result[1:]
            location, normal = result[0], result[1]
            if normal.dot(up) >= WALKABLE_DOT:
                hits.append({"h": spec.ridge.local(test.xyz(location))[2], "dot": round(normal.dot(up), 3),
                             "source": comp.get_owner().get_actor_label()})
            start = location + down * 2.0
            if (start - top).length() >= length:
                break
    return hits


def flat_site(components, x, y, h):
    # Pod 자리 조건(베이커 가장자리 여유 250cm·투영 100cm를 미리 거른다): 중심이 h±300cm 안의 완만한 걷는 면이고,
    # 반경 150·300cm 고리 12점이 모두 중심 높이 ±POD_RING_STEP_CM 안의 걷는 면이다. 만족하면 중심 hit를 돌려준다.
    hits = [hit for hit in walkable_hits(components, x, y, h, 300.0) if hit["dot"] >= POD_CENTER_DOT]
    if not hits:
        return None
    center = min(hits, key=lambda hit: abs(hit["h"] - h))
    for radius in POD_RING_CM:
        for k in range(12):
            a = 2 * math.pi * k / 12
            ring = walkable_hits(components, x + radius * math.cos(a), y + radius * math.sin(a), center["h"],
                                 POD_RING_STEP_CM)
            if not ring:
                return None
    return center


def project_pod(components, local):
    # 원래 (x, y)의 직선에서 원래 높이에 가장 가까운 걷는 면으로 내린다. 그 자리가 평탄 조건을 못 맞추면 같은 높이 근처에서
    # 원래 자리에 가장 가까운 평탄 지점을 나선으로 찾는다(최대 POD_MOVE_LIMIT_CM).
    x, y, h = local
    best = None
    for reach in POD_SEARCH_CM:
        hits = walkable_hits(components, x, y, h, reach)
        if hits:
            best = min(hits, key=lambda hit: abs(hit["h"] - h))
            break
    assert best, "no walkable surface for Pod at %s" % (local,)
    site = flat_site(components, x, y, best["h"])
    if site:
        return [x, y, site["h"]], site, 0.0
    for radius in range(POD_SEARCH_STEP_CM, POD_MOVE_LIMIT_CM + 1, POD_SEARCH_STEP_CM):
        count = max(8, int(2 * math.pi * radius / POD_SEARCH_STEP_CM))
        for k in range(count):
            a = 2 * math.pi * k / count
            cx, cy = x + radius * math.cos(a), y + radius * math.sin(a)
            site = flat_site(components, cx, cy, best["h"])
            if site:
                return [cx, cy, site["h"]], site, float(radius)
    raise AssertionError("no flat Pod site within %d cm of %s" % (POD_MOVE_LIMIT_CM, local))


def site_pod(components, local, sites):
    # 원래 (x, y)의 직선에서 원래 높이에 가장 가까운 걷는 면 높이를 잡고, 그 높이 ±POD_SITE_DH_CM 안의 베이크 스폰 후보 중
    # 가장자리 여유가 POD_SITE_EDGE_CM 이상인 가장 가까운 후보로 옮긴다. 후보 높이는 베이커가 붙인 Support 면이다.
    x, y, h = local
    best = None
    for reach in POD_SEARCH_CM:
        hits = walkable_hits(components, x, y, h, reach)
        if hits:
            best = min(hits, key=lambda hit: abs(hit["h"] - h))
            break
    assert best, "no walkable surface for Pod at %s" % (local,)
    rows = [c for c in sites["candidates"] if c["edge"] >= POD_SITE_EDGE_CM and abs(c["h"] - best["h"]) <= POD_SITE_DH_CM]
    assert rows, local
    pick = min(rows, key=lambda c: math.hypot(c["x"] - x, c["y"] - y))
    moved = math.hypot(pick["x"] - x, pick["y"] - y)
    assert moved <= POD_MOVE_LIMIT_CM, (local, moved)
    return [pick["x"], pick["y"], pick["h"]], {"source": "candidate layer %d" % pick["layer"], "dot": pick["slope"]},         round(moved, 1)


def markers(layout_data):
    placement = json.loads(PLACEMENT.read_text(encoding="utf-8"))
    overrides = layout_data.get("markers", {})
    result = {}
    for key in ("pods", "anchors", "nodes"):
        rows = []
        for item in placement[key]:
            item = dict(item)
            item.update(overrides.get(key, {}).get(item["id"], {}))
            rows.append(item)
        result[key] = rows
    return result


def build(version, *options):
    # options: omit=<PodId>[,<PodId>]|all (자리가 정해지지 않은 Pod를 뺀다)
    #          sites=<버전> (그 버전 베이크의 스폰 후보 중 가장 가까운 유효 후보로 Pod를 옮긴다)
    #          discard=/Temp/Untitled_N (이전 build가 실패하며 남긴 저장 안 된 임시 맵만 버린다. 다른 미저장 변경은 막는다)
    omit, sites, discard = set(), None, set()
    for option in options:
        key, _, value = option.partition("=")
        assert key in ("omit", "sites", "discard") and value, option
        if key == "omit":
            omit |= set(value.split(","))
        elif key == "discard":
            assert value.startswith("/Temp/Untitled"), value
            discard.add(value)
        else:
            sites = json.loads((OUT / ("Candidates_%s.json" % value)).read_text(encoding="utf-8"))
    level = LEVEL.format(version)
    data = json.loads(layout.LAYOUT.read_text(encoding="utf-8"))
    dirty = test.dirty()
    assert set(dirty) <= discard and (not discard or layout.world_package() in discard), dirty
    assert not unreal.EditorAssetLibrary.does_asset_exist(level), level
    for row in data["level_instances"]:
        source = row["cull"]["level"] if "cull" in row else row["level"]
        assert unreal.EditorAssetLibrary.does_asset_exist(source), source
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    saving = unreal.EditorLoadingAndSavingUtils
    assert saving.new_blank_map(False)

    traced = []
    for row in data["actors"]:
        t = layout.pose_transform(row)
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, t.translation, t.rotation.rotator())
        actor.set_actor_label(row["label"])
        actor.set_actor_scale3d(t.scale3d)
        comp = actor.static_mesh_component
        comp.set_static_mesh(unreal.load_asset(row["mesh"]))
        for slot, material in row["material_overrides"].items():
            comp.set_material(int(slot), unreal.load_asset(material))
        comp.set_editor_property("use_default_collision", False)
        role = row.get("collision_role") or PRESERVED.get(row["label"])
        if role:
            tags, profile = collision.ROLE_CONTRACT[role]
            comp.set_editor_property("component_tags", tags)
            comp.set_collision_profile_name(profile)
            comp.set_mobility(unreal.ComponentMobility.STATIC)
            # 충돌 지각·Z1 프록시는 숨기고(시각은 V010·ED 메시), 보존 바위는 시각 메시를 겸하므로 보인다.
            comp.set_visibility(row["label"] in PRESERVED)
            actor.set_folder_path("Collision" if row["label"] not in PRESERVED else "PreservedRocks")
            if role in ("Crust", "Floor"):
                traced.append(comp)
        else:
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            actor.set_folder_path("Visual")

    placed = []
    for row in data["level_instances"]:
        # cull(GX-033)이 있으면 규칙으로 덜어낸 월드 좌표 사본 레벨을 원점에 놓는다.
        source = row["cull"]["level"] if "cull" in row else row["level"]
        if row["role"] == "layer" or "cull" in row:
            location, rotation, scale = unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0), 1.0
        else:
            t = layout.instance_transform(row)
            location, rotation, scale = t.translation, t.rotation.rotator(), row["scale"]
        actor = actors.spawn_actor_from_class(unreal.LevelInstance, location, rotation)
        actor.set_actor_label(row["label"])
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        actor.set_world_asset(unreal.load_asset(source))
        actor.load_level_instance()
        assert test.package(actor.get_world_asset()) == source, row["label"]
        actor.set_folder_path("EDVisual")
        placed.append(row["label"])

    rows = markers(data)
    if "all" in omit:
        omit = {item["id"] for item in rows["pods"]}
    assert omit <= {item["id"] for item in rows["pods"]}, omit
    rows["pods"] = [item for item in rows["pods"] if item["id"] not in omit]
    pods = []
    for item in rows["pods"]:
        local, hit, moved = (site_pod(traced, item["local"], sites) if sites
                             else project_pod(traced, item["local"]))
        pods.append({"id": item["id"], "authored": [round(v, 1) for v in item["local"]],
                     "local": [round(v, 1) for v in local], "projected_h": round(local[2], 1),
                     "delta_cm": round(local[2] - item["local"][2], 1), "moved_cm": moved,
                     "source": hit["source"], "dot": hit["dot"]})
        item["local"] = local
    import AssembleRidgeOctant as assembly
    assembly.place_markers(actors, rows)
    assert saving.save_map(test.world(), level), level
    report = {"level": level, "layout_version": data["version"], "actors": len(data["actors"]),
              "level_instances": placed, "markers": {k: len(v) for k, v in rows.items()},
              "marker_overrides": data.get("markers", {}), "omitted_pods": sorted(omit), "pods": pods}
    write("Build_%s.json" % version, report)
    unreal.log("Z1_BAKE_LVI_BUILD_READY %s %s" % (level, json.dumps([(p["id"], p["delta_cm"], p["moved_cm"]) for p in pods])))


def role_label(actor, mesh):
    label = actor.get_actor_label()
    if label in PRESERVED:
        return "Preserved" + PRESERVED[label]
    return collision.role_of(mesh)


def check():
    level = layout.world_package()
    assert re.search(r"/LVI_Ridge_EDTerrain_Z1Bake_V\d+$", level), level
    top, nested = layout.level_actors()
    rows, issues = [], []
    for actor in top:
        if actor.get_class().get_name() in test.LIGHT_CLASSES + ("SkyLight", "DirectionalLight"):
            issues.append("light " + actor.get_actor_label())
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        comp = actor.static_mesh_component
        if comp.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
            continue
        mesh = test.package(comp.static_mesh)
        role = role_label(actor, mesh)
        tags, profile = collision.ROLE_CONTRACT[role.replace("Preserved", "")]
        got = sorted(str(t) for t in comp.get_editor_property("component_tags"))
        row = {"label": actor.get_actor_label(), "role": role, "tags": got,
               "profile": str(comp.get_collision_profile_name()), "visible": comp.is_visible(),
               "mesh": collision.mesh_check(comp.static_mesh)}
        mesh_ok = row["mesh"]["pass"]
        if role.startswith("Preserved") and not mesh_ok:
            # U3 보존 바위는 FBX 반입이 만든 자동 상자 1개가 남아 있다(complex-as-simple이라 질의에는 안 쓰인다).
            # 기본 WIP와 공유하는 에셋이라 고치지 않고 기록만 한다(U3 조기 베이크도 이 상태로 통과).
            m = row["mesh"]
            mesh_ok = (m["complex_as_simple"] and m["lods"] == 1 and m["min_component_cm"] > -0.05
                       and m["abs_max_component_cm"] <= collision.COMPONENT_LIMIT_CM)
            row["note"] = "preserved rock keeps %d auto simple shape(s)" % m["simple_shapes"]
        row["pass"] = (got == sorted(tags) and row["profile"] == profile and mesh_ok
                       and comp.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS
                       and comp.mobility == unreal.ComponentMobility.STATIC)
        if not row["pass"]:
            issues.append(row["label"])
        rows.append(row)
    if sorted(r["role"] for r in rows) != EXPECTED_ROLES:
        issues.append("roles %s" % sorted(r["role"] for r in rows))
    visual = {}
    for level_actors in nested.values():
        for name, count in collision.colliding(level_actors).items():
            visual[name] = visual.get(name, 0) + count
    if visual:
        issues.append("visual collision %d" % sum(visual.values()))
    counts = {"pods": len([a for a in top if isinstance(a, unreal.LNPMassSpawnPoint)]),
              "anchors": len([a for a in top if isinstance(a, unreal.LNPPlacementMarker)]),
              "nodes": len([a for a in top if a.get_actor_label().startswith("Review_Ridge")]),
              "level_instances": len([a for a in top if isinstance(a, unreal.LevelInstance)]),
              "nested_levels": len(nested)}
    version = re.search(r"_(V\d+)$", level).group(1)
    expected_pods = 12 - len(json.loads((OUT / ("Build_%s.json" % version)).read_text(encoding="utf-8")).get("omitted_pods", []))
    if (counts["pods"] + 12 - expected_pods, counts["anchors"], counts["nodes"], counts["level_instances"]) != (12, 6, 27, 15):
        issues.append("counts %s" % counts)
    write("Check_%s.json" % version, {"level": level, "pass": not issues, "issues": issues, "actors": rows,
                                      "visual_collision": visual, "counts": counts})
    unreal.log("Z1_BAKE_LVI_CHECK %s %s" % ("PASS" if not issues else "FAIL", json.dumps(issues)))


def spawn_candidates(payload):
    # LNPSpawnData 인코딩: 헤더, authored 앵커(가변 길이), random 후보(40B).
    version, _, authored, count = struct.unpack_from("<HHII", payload)
    offset, anchors = 12, []
    for _ in range(authored):
        offset += 16
        length = struct.unpack_from("<i", payload, offset)[0]
        offset += 4 + length
        location = struct.unpack_from("<fff", payload, offset)
        offset += 28
        layer, _, edge, capsule, headroom = struct.unpack_from("<HHfff", payload, offset)
        offset += 16
        formations = []
        n = struct.unpack_from("<I", payload, offset)[0]
        offset += 4
        for _ in range(n):
            # 편성: 이름, Count(int32), Offset(FVector2D double 2개), Radius(float) = 24B.
            length = struct.unpack_from("<i", payload, offset)[0]
            offset += 4 + length
            formations.append(struct.unpack_from("<iddf", payload, offset))
            offset += 24
        anchors.append({"location": location, "layer": layer, "edge": edge, "headroom": headroom,
                        "formations": formations})
    candidates = []
    for _ in range(count):
        _, px, py, pz, _, _, layer, allowed, _, slope, edge, capsule, headroom = struct.unpack_from(
            "<I3fhhHBBffff", payload, offset)
        candidates.append((px, py, pz, layer, edge, capsule, headroom, slope))
        offset += 40
    assert offset == len(payload), (offset, len(payload))
    return version, anchors, candidates


def flush_log():
    unreal.SystemLibrary.execute_console_command(test.world(), "FLUSHLOG")


def last_bake_log(level):
    flush_log()
    log = (PROJECT / "Saved/Logs/LootNPop.log").read_text(encoding="utf-8", errors="replace").splitlines()
    marker = "[BakeOctant] " + level
    starts = [i for i, line in enumerate(log) if marker in line]
    assert starts, "bake log line not found"
    i = starts[-1]
    lines = [log[i]]
    for line in log[i + 1:]:
        if line.startswith("  "):
            lines.append(line)
        else:
            break
    return lines


def bake(version):
    level, surface = LEVEL.format(version), SURFACE.format(version)
    assert not test.dirty(), test.dirty()
    open_level(TEST_MAP)
    surface_file = content_file(surface, ".uasset")
    prior = surface_file.stat().st_mtime_ns if surface_file.exists() else None
    unreal.SystemLibrary.execute_console_command(test.world(), "LNP.SurfaceNav.BakeOctant " + level)
    lines = last_bake_log(level)
    if not surface_file.exists() or surface_file.stat().st_mtime_ns == prior:
        write("Bake_%s.json" % version, {"level": level, "failed": True, "log": lines})
        unreal.log_error("Z1_BAKE_FAILED " + " | ".join(lines))
        return
    data = unreal.load_asset(surface)
    header = data.get_editor_property("header")
    packages = sorted(str(r.get_editor_property("package_name")) for r in header.get_editor_property("source_manifest"))
    from InspectAcropolisBake import seams
    seam_match = seams(MEADOW_SURFACE) == seams(surface)
    support = bytes(data.get_editor_property("support_payload"))
    _, layers = struct.unpack_from("<HH", support)
    _, anchors, candidates = spawn_candidates(bytes(data.get_editor_property("spawn_payload")))
    placement = json.loads((OUT / ("Build_%s.json" % version)).read_text(encoding="utf-8"))["pods"]
    pods = []
    for anchor in anchors:
        ax, ay, az = anchor["location"]
        lx, ly, lh = spec.ridge.local((ax, ay, az))
        pod = min(placement, key=lambda p: math.hypot(p["local"][0] - lx, p["local"][1] - ly))
        radius = max(f[3] for f in anchor["formations"]) if anchor["formations"] else 0
        same = [c for c in candidates if c[3] == anchor["layer"]]
        near = lambda r: sum(1 for c in same if math.dist(c[:3], (ax, ay, az)) <= r)
        pods.append({"id": pod["id"], "local": [round(lx), round(ly), round(lh)], "layer": anchor["layer"],
                     "edge_cm": round(anchor["edge"]), "headroom_cm": round(anchor["headroom"]),
                     "formation_count": sum(f[0] for f in anchor["formations"]), "formation_radius_cm": radius,
                     "same_layer_in_1500": near(1500), "same_layer_in_radius": near(radius),
                     "layer_candidates": len(same)})
    streams = {name: header.get_editor_property(name).get_editor_property("element_count")
               for name in ("support", "navigation", "traversal", "spawn", "conditional_patch")}
    summary = lines[0].split(" | ", 1)[-1]
    numbers = {key: int(value) for key, value in re.findall(r"(components|portals|cells|authored|noNavRejected)=(\d+)",
                                                              summary)}
    report = {"level": level, "surface_data": surface, "data_version": header.get_editor_property("data_version"),
              "support_layers": layers, "streams": streams, "seam_matches_meadow": seam_match,
              "spawn_candidates": len(candidates), "pods": pods, "summary": numbers,
              "source_packages": packages, "log": lines}
    write("Bake_%s.json" % version, report)
    rows = []
    for c in candidates:
        lx, ly, lh = spec.ridge.local(c[:3])
        rows.append({"x": round(lx, 1), "y": round(ly, 1), "h": round(lh, 1), "layer": c[3], "edge": round(c[4], 1),
                     "capsule": round(c[5], 1), "headroom": round(c[6], 1), "slope": round(c[7], 3)})
    write("Candidates_%s.json" % version, {"surface_data": surface, "candidates": rows})
    unreal.log("Z1_BAKE_READY layers=%d seam=%s %s" % (layers, seam_match, json.dumps(numbers)))


NAV_POINTS_RADIUS_CM = 150
NAV_HEIGHT_WINDOW_CM = 300.0


def nav(version):
    # 마커(Pod·검토점)와 동선 꼭짓점의 Nav component. 결과는 로그를 읽어 정리한다.
    level = LEVEL.format(version)
    rows = markers(json.loads(layout.LAYOUT.read_text(encoding="utf-8")))
    build_pods = {p["id"]: p for p in json.loads((OUT / ("Build_%s.json" % version)).read_text(encoding="utf-8"))["pods"]}
    points = []
    for item in rows["nodes"]:
        x, y, h = item["local"]
        if item["id"] in build_pods:
            h = build_pods[item["id"]]["projected_h"]
        points.append(("node", item["id"], x, y, h))
    blueprint = json.loads((HERE / "RidgeBlueprint_V001.json").read_text(encoding="utf-8"))
    for route in blueprint["routes"]:
        for index, (x, y) in enumerate(route["xy"]):
            points.append(("route", "%s[%d]" % (route["id"], index), x, y, None))
    log_file = PROJECT / "Saved/Logs/LootNPop.log"
    results = []
    for kind, name, x, y, h in points:
        if h is None:
            h = spec.massif_flat_ground(x, y)
        p = spec.ridge.point(x, y, h)
        radius = math.sqrt(sum(v * v for v in p))
        flush_log()
        start = len(log_file.read_text(encoding="utf-8", errors="replace").splitlines())
        unreal.SystemLibrary.execute_console_command(
            test.world(), "LNP.SurfaceNav.ExplainNavGap %s %.1f %.1f %.1f %d" % (level, p[0], p[1], p[2], NAV_POINTS_RADIUS_CM))
        flush_log()
        lines = log_file.read_text(encoding="utf-8", errors="replace").splitlines()[start:]
        comps = {}
        ground = None
        for line in lines:
            m = re.search(r"\[ExplainNavGap\] layer=(\d+) IJ=.*? (Node\w*) .*?atlasR=([\d.]+).*component=(\d+) componentNodes=(\d+)",
                          line)
            # 같은 방향의 다른 층(동굴 바닥·섬)은 빼고 지점 높이 ±NAV_HEIGHT_WINDOW_CM 안의 node만 센다.
            if m and abs(float(m.group(3)) - radius) <= NAV_HEIGHT_WINDOW_CM:
                comps.setdefault((int(m.group(1)), int(m.group(4))), int(m.group(5)))
            g = re.search(r"\[ExplainNavGap\] ground hit=(\d) radius=([\d.]+) dot=([-\d.]+) source=(\S+)", line)
            if g:
                ground = {"hit": g.group(1) == "1", "dot": float(g.group(3)), "source": g.group(4)}
        results.append({"kind": kind, "id": name, "local": [round(x), round(y), round(h)], "ground": ground,
                        "components": [{"layer": k[0], "component": k[1], "nodes": v} for k, v in sorted(comps.items())]})
    write("Nav_%s.json" % version, {"level": level, "radius_cm": NAV_POINTS_RADIUS_CM, "points": results})
    unreal.log("Z1_BAKE_NAV_READY %d points" % len(results))


SURVEY_BOX_CM = (-12000, 8000, -9000, 10000)
SURVEY_STEP_CM = 300
SURVEY_REACH_CM = 9000.0
SURVEY_HEADROOM_CM = 200.0


def survey(version):
    # 열린 LVI에서 Z1 일대 격자 점마다 현지 Up 직선의 걷는 면(충돌 지각·Floor·보존 바위 Floor)을 모두 적고, 각 면 바로 위
    # SURVEY_HEADROOM_CM 안을 Shell이 막는지 표시한다. 평면도(동선 재정의 근거)용이다.
    level = layout.world_package()
    assert level == LEVEL.format(version), level
    top, _ = layout.level_actors()
    support, shells = [], []
    for actor in top:
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        comp = actor.static_mesh_component
        if comp.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
            continue
        tags = [str(t) for t in comp.get_editor_property("component_tags")]
        (support if "LNP.Surface.Support" in tags else shells).append(comp)
    x0, x1, y0, y1 = SURVEY_BOX_CM
    rows = []
    for x in range(x0, x1 + 1, SURVEY_STEP_CM):
        for y in range(y0, y1 + 1, SURVEY_STEP_CM):
            h0 = spec.massif_flat_ground(x, y)
            hits = walkable_hits(support, x, y, h0 + 3000.0, SURVEY_REACH_CM)
            cells = []
            for hit in sorted(hits, key=lambda hit: -hit["h"]):
                a = unreal.Vector(*spec.ridge.point(x, y, hit["h"] + 20.0))
                b = unreal.Vector(*spec.ridge.point(x, y, hit["h"] + SURVEY_HEADROOM_CM))
                blocked = any(comp.line_trace_component(a, b, True, False, False) for comp in shells)
                cells.append([round(hit["h"]), hit["dot"], hit["source"][-6:], bool(blocked)])
            rows.append([x, y, cells])
    write("Survey_%s.json" % version, {"level": level, "box_cm": SURVEY_BOX_CM, "step_cm": SURVEY_STEP_CM, "rows": rows})
    unreal.log("Z1_BAKE_SURVEY_READY %d points" % len(rows))


def pool(version):
    level, surface, path = LEVEL.format(version), SURFACE.format(version), POOL.format(version)
    production = content_file(PRODUCTION_POOL, ".uasset")
    before = hashlib.sha256(production.read_bytes()).hexdigest()
    source = unreal.load_asset(PRODUCTION_POOL)
    definitions = list(source.get_editor_property("octant_definitions"))
    ridge = [i for i, d in enumerate(definitions)
             if "/Ridge_00/" in d.get_editor_property("level_asset").get_path_name()]
    assert len(ridge) == 1, ridge
    definition = definitions[ridge[0]]
    definition.set_editor_property("level_asset", unreal.load_asset(level))
    definition.set_editor_property("surface_data", unreal.load_asset(surface))
    target = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) \
        else unreal.EditorAssetLibrary.duplicate_asset(PRODUCTION_POOL, path)
    assert target
    target.set_editor_property("octant_definitions", definitions)
    assert unreal.EditorAssetLibrary.save_loaded_asset(target, only_if_is_dirty=False)
    # duplicate·편집은 원본 객체를 바꾸지 않아야 한다(구조체 배열은 사본이다). 다시 불러 확인한다.
    check = list(unreal.load_asset(PRODUCTION_POOL).get_editor_property("octant_definitions"))[ridge[0]]
    assert "/Ridge_00/" in check.get_editor_property("level_asset").get_path_name()
    assert not any(PRODUCTION_POOL in p for p in test.dirty()), test.dirty()
    assert hashlib.sha256(production.read_bytes()).hexdigest() == before
    write("Pool_%s.json" % version, {"pool": path, "ridge_index": ridge[0], "definitions": len(definitions),
                                     "production_sha256": before})
    unreal.log("Z1_BAKE_POOL_READY " + path)


if __name__ == "__main__":
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first."
    {"build": build, "check": check, "bake": bake, "nav": nav, "survey": survey, "pool": pool}[sys.argv[1]](*sys.argv[2:])
