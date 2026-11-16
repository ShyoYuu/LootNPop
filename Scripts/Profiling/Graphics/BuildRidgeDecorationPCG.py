"""1-B5a 대표 구역 PCG 소수 배치, 1-B5b 바위 Blocker 승격, 1-B5d 규칙 확장(GX-015)의 규칙 에셋·PCG 그래프·WIP 변경을 만든다.

콘솔: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/BuildRidgeDecorationPCG.py" <모드>
  blocker-mesh: Sandstone 파생 메시를 만들고 기존 단순 컨벡스를 충돌로 쓰게 한다(원본은 ComplexAsSimple).
  rules: Ridge spec에서 구역·보호 형상을 다시 계산해 규칙 에셋을 만들거나 덮어쓴다(파생 데이터).
  graph: PCG 그래프(V004, 바위·키 큰 식생·지피별 겹침 정리, 키 큰 식생 > 바위 > 지피, Spawner 분리)가 없을 때만 만든다.
  place: 기존 WIP에 PCG 액터 1개를 만들고 생성한다.
  promote: 수동 바위5개를 Decoration에서 Static+Blocker로 바꾼다.
  regenerate: 기존 PCG 액터를 현재 그래프(V004)로 바꿔 다시 생성한다.
맵 저장은 사용자 확인 뒤 따로 한다. 원본 메시/재질·정식 Ridge는 수정하지 않는다. 결과는 Saved 아래 JSON으로 남긴다.
새 체크아웃: blocker-mesh → rules → graph → place → promote → 확인 뒤 WIP 저장.
"""
import hashlib
import json
from pathlib import Path
import shutil
import sys

import unreal

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "Scripts"))
sys.path.insert(0, str(HERE))
import RidgeOctantSpec as spec
from BuildRidgeSurfaceProbe import ZONE

LEVEL = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/V001/LVI_Ridge_SurfaceProbe"
FOLDER = "/Game/Graphics/Environment/ElectricDreams/RidgeDecoration_V001"
RULES = FOLDER + "/DA_RidgeDecorationRules"
# V001(단일 Decoration Spawner)은 1-B5a, V002(한 번에 겹침 정리)는 1-B5b, V003(바위 > 식생)은 1-B5d 첫 생성의
# 복구용으로 디스크에 남기고 쓰지 않는다.
GRAPH = FOLDER + "/PCG_RidgeDecoration_V004"
OUT = ROOT / "Saved/Profiling/Graphics/Phase01/RidgeDecorationExpand_20261116"
ACTOR_LABEL = "GraphicsDecorationPCG_V001"
CRUST_NAME = "SM_Octant_Ridge_00_V011_R30000"
SEED = 51001
DECORATION_TAG = "LNP.Surface.Decoration"
DECORATION_PROFILE = "LNPDecoration"
# GX-014: 바위는 Static+Blocker 지형 계약. Support는 ISM에서 막혀 있다(SSN D-058).
BLOCKER_TAGS = ["LNP.Surface.Static", "LNP.Surface.Blocker"]
BLOCKER_PROFILE = "LNPStaticBlocker"
# 승격한 수동 장식이 PCG 간격 대상에 남도록 하는 표식. 지형 계약 태그가 아니다(LNPDecorationSampler.cpp).
MANUAL_DECORATION_TAG = "LNP.Graphics.ManualDecoration"
SANDSTONE_SOURCE = "/Game/Megascans/3D_Assets/SandstoneBoulder/SM_SandstoneBoulder_01"
SANDSTONE_BLOCKER = FOLDER + "/SM_Ridge_SandstoneBoulder_01_Blocker"
# 수동 장식 중 승격할 바위(라벨 → 바꿀 메시, None이면 유지). 라벨은 BuildRidgeGroundProbe/RockProbe가 정했다.
MANUAL_BLOCKERS = {
    "GraphicsGroundProbe_Sandstone_00": SANDSTONE_BLOCKER,
    "GraphicsGroundProbe_Sandstone_01": SANDSTONE_BLOCKER,
    "GraphicsRockProbe_Moss_00": None,
    "GraphicsRockProbe_Moss_01": None,
    "GraphicsRockProbe_Cliff_02": None,
}

# 여유 폭(cm)과 근거. 메시 전체 범위(회전·스케일·WPO 여유)에 적용하므로 이전 소수 시험의 bounds→spec 계수1.2는 쓰지 않는다.
CAPSULE_MARGIN = 50     # 플레이어 캡슐 반지름42cm를 올림. 통로·암반 외곽 끝에 선 캐릭터와 겹치지 않게 한다.
COVER_ROCK_MARGIN = 100  # 기존 엄폐 암반(BasinMonolith 등) 실루엣·판정 보존. 캡슐 지름84cm를 올림.
ROUTE_HALF_WIDTH = 200  # 폭이 없는 연결표 보행 중심선: 캡슐 2개가 나란히 지나가는 폭(168cm)+여유.
GATE_HALF_WIDTH = 500   # 대전당 남쪽 문 폭10m.
LANDING_RADIUS = 400    # 앵커 착지부 4m.
HOOK_LINE_RADIUS = 100  # 출발 검토점(지면+170)→앵커 직선의 조준·이동 공간.
CAMERA_RADIUS = 150     # 벤치마크 카메라 경로·검토 시점: 근거리 장식이 화면을 덮지 않게 한다.
ZONE_INSET = 100        # 임시 시험 재질 경계(이진 마스크) 안에 메시 전체를 둔다.
EXISTING_CLEARANCE = 30  # 수동 장식12개와 관통하지 않게 하는 최소 간격.
# GX-015 역할별 2D 여유(Margin 기준, 음수면 Margin 안까지): 지피는 동선 형상 안까지(형상의 지피 허용),
# 충돌 없는 Fern은 동선 반폭 가장자리까지(-50), 바위는 반폭+50(캡슐 여유만, 0).
FERN_PADDING = -CAPSULE_MARGIN
BLOCKER_PADDING = 0

# 역할: 1-B4에서 근경 통과한 메시만. 절벽 부분 보강은 수동 배치 역할이라 PCG에 넣지 않는다.
# 1-B5d 간격은 1-B5a 값(800/900/1000/1000)에 지피 ×0.35·나머지 ×0.5를 곱했다. 1-B5c 사용자 판정(훨씬 더 풍성하게)의 시험값이다.
# 지피와 Fern은 경사 0~40°로 넓혔다(1-B5d 판정: 바위 20~30%를 Fern으로. Fern은 바닥 범위가 커서 간격을 좁혀도 늘지 않았다).
# 스케일·매몰은 소수 시험 값 주변이다.
ROLES = [
    dict(id="Clover", mesh="/Game/Megascans/3D_Plants/CloverVarieties/SM_CloverVarieties_01", spacing=280, probability=1.0,
         scale=(0.85, 1.1), slope=(0, 40), sink=0.7, wind=0, max_float=8, max_sink=15, ground_cover=True, padding=0),
    dict(id="Sandstone", mesh=SANDSTONE_BLOCKER, spacing=450, probability=1.0, scale=(0.9, 1.1), slope=(0, 30), sink=2.0,
         wind=0, max_float=10, max_sink=25, blocker=True, padding=BLOCKER_PADDING),
    dict(id="Moss", mesh="/Game/Megascans/3D_Assets/MossyForestBoulder/SM_MossyForestBoulder_06",
         material="/Game/Megascans/3D_Assets/MossyForestBoulder/MI_MossyForestBoulder_09", spacing=500, probability=1.0,
         scale=(0.7, 0.9), slope=(0, 30), sink=3.0, wind=0, max_float=25, max_sink=30, blocker=True, padding=BLOCKER_PADDING),
    dict(id="Fern", mesh="/Game/Megascans/3D_Plants/Fern/SM_Fern_01",
         material="/Game/Graphics/Environment/ElectricDreams/RidgeFernWind_V001/MI_Ridge_Fern", spacing=500, probability=1.0,
         scale=(0.9, 1.1), slope=(0, 40), sink=1.5, wind=30, max_float=15, max_sink=25, padding=FERN_PADDING),
]


def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(value, indent=2, ensure_ascii=False), encoding="utf-8")


def vec(p):
    return unreal.Vector(*p)


def ground_point(xy, lift=0.0):
    return spec.point(xy[0], xy[1], spec.ground(*xy) + lift)


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def require_wip_clean(allowed=()):
    """allowed: 이 스크립트가 직전 실패에서 메모리에 만든 자기 파생 에셋은 dirty여도 이어서 덮어쓴다."""
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first."
    assert editor_world().get_path_name().split(".")[0] == LEVEL, "Open the existing WIP level first."
    dirty = [p.get_path_name() for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) +
             list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())]
    dirty = [p for p in dirty if p not in allowed]
    assert not dirty, "Save or review existing changes first: %s" % dirty


def crust_mesh_path():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    found = [a.static_mesh_component.static_mesh for a in actors if isinstance(a, unreal.StaticMeshActor)
             and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name() == CRUST_NAME]
    assert len(found) == 1, "Expected exactly one crust actor."
    return found[0].get_path_name()


def keep_outs():
    """spec의 동선·Pod·훅·문·암반·카메라를 보호 형상으로 바꾼다. 범위 밖 형상도 넣어 두고 C++이 판정한다."""
    k2, k3 = [], []
    for path in spec.active(spec.PATHS):
        # GX-015: 동선 형상(Path·Route·Gate)은 지피를 안까지 허용한다. 암반 외곽은 허용하지 않는다.
        k2.append(dict(id="Path_" + path["id"], points=[(x, y, w) for x, y, _, w in spec.path_points(path)],
                       margin=CAPSULE_MARGIN, ground_cover=True))
    for route in spec.active(spec.ROUTES):
        if route.get("layer") or route.get("deck"):
            continue
        k2.append(dict(id="Route_" + route["id"], points=[(x, y, ROUTE_HALF_WIDTH) for x, y in route["xy"]],
                       margin=CAPSULE_MARGIN, ground_cover=True))
    for node in spec.active(spec.NODES):
        if "Gate" in node["id"] and not node.get("layer"):
            k2.append(dict(id="Gate_" + node["id"], points=[(*node["xy"], GATE_HALF_WIDTH)], margin=CAPSULE_MARGIN,
                           ground_cover=True))
    for rock in spec.active(spec.ROCKS):
        if rock.get("cut") is False:
            continue  # 석교·부유섬·섬 위 엄폐는 지면을 자르지 않는 공중 몸체다. 그 아래 지면은 실제 overlap 검사가 맡는다.
        margin = COVER_ROCK_MARGIN if rock["role"] == "Cover" else CAPSULE_MARGIN
        k2.append(dict(id="Rock_" + rock["id"], points=[(x, y, 0) for x, y in spec.rock_outline(rock)], closed=True,
                       margin=margin))
    for pod in spec.active(spec.PODS):
        if pod.get("layer"):
            continue
        # 낮은 지피만 Pod 3D 편성 공간에 허용한다(1-B0 Z3). 새 바위/식생 엄폐는 막는다.
        center = ground_point(pod["xy"])
        k3.append(dict(id="Pod_" + pod["id"], start=center, end=center,
                       radius=pod.get("formation_radius", spec.FORMATION_RADIUS), ground_cover=True))
    nodes = {n["id"]: n for n in spec.active(spec.NODES)}
    for anchor in spec.active(spec.ANCHORS):
        landing = ground_point(anchor["landing"])
        k3.append(dict(id="Landing_" + anchor["id"], start=landing, end=landing, radius=LANDING_RADIUS))
        hook = ground_point(anchor["landing"], anchor["above"])
        for origin in anchor["from"]:
            node = nodes.get(origin)
            if node and not node.get("layer"):
                k3.append(dict(id="Hook_%s_%s" % (origin, anchor["id"]), start=ground_point(node["xy"], 170), end=hook,
                               radius=HOOK_LINE_RADIUS))
    views = json.loads((HERE / "RidgeViewpoints.json").read_text(encoding="utf-8"))
    cameras = [p["camera"]["location"] for p in views["points"]]
    for i, a in enumerate(cameras):
        b = cameras[(i + 1) % len(cameras)]
        k3.append(dict(id="BenchmarkCamera_%d" % i, start=a, end=b, radius=CAMERA_RADIUS))
    for name, eye, _, _ in spec.VIEWS:
        if eye[2] is None:
            p = ground_point(eye[:2], 170)
            k3.append(dict(id="View_" + name, start=p, end=p, radius=CAMERA_RADIUS))
    return k2, k3


def build_rules():
    # 규칙 에셋만 저장하고 맵은 저장하지 않으므로, 직전 재생성으로 dirty인 WIP는 허용한다.
    require_wip_clean(allowed=(RULES, LEVEL))
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    rules = unreal.load_asset(RULES)
    if rules is None:
        rules = tools.create_asset(RULES.split("/")[-1], FOLDER, unreal.LNPDecorationRuleData, None)
    assert rules
    k2, k3 = keep_outs()
    shapes2 = []
    for s in k2:
        shape = unreal.LNPDecorationKeepOut2D()
        shape.set_editor_property("id", s["id"])
        shape.set_editor_property("points", [unreal.Vector(float(x), float(y), float(w)) for x, y, w in s["points"]])
        shape.set_editor_property("closed_polygon", s.get("closed", False))
        shape.set_editor_property("margin", float(s["margin"]))
        shape.set_editor_property("allow_ground_cover", s.get("ground_cover", False))
        shapes2.append(shape)
    shapes3 = []
    for s in k3:
        shape = unreal.LNPDecorationKeepOut3D()
        shape.set_editor_property("id", s["id"])
        shape.set_editor_property("start", vec(s["start"]))
        shape.set_editor_property("end", vec(s["end"]))
        shape.set_editor_property("radius", float(s["radius"]))
        shape.set_editor_property("allow_ground_cover", s.get("ground_cover", False))
        shapes3.append(shape)
    roles = []
    for r in ROLES:
        role = unreal.LNPDecorationRole()
        role.set_editor_property("id", r["id"])
        role.set_editor_property("mesh", unreal.load_asset(r["mesh"]))
        if r.get("material"):
            role.set_editor_property("material_override", unreal.load_asset(r["material"]))
        role.set_editor_property("spacing", float(r["spacing"]))
        role.set_editor_property("probability", float(r["probability"]))
        role.set_editor_property("scale_range", unreal.Vector2D(*r["scale"]))
        role.set_editor_property("slope_range", unreal.Vector2D(*r["slope"]))
        role.set_editor_property("sink", float(r["sink"]))
        role.set_editor_property("wind_padding", float(r["wind"]))
        role.set_editor_property("max_corner_float", float(r["max_float"]))
        role.set_editor_property("max_corner_sink", float(r["max_sink"]))
        role.set_editor_property("ground_cover", r.get("ground_cover", False))
        role.set_editor_property("normal_alignment", 1.0)
        role.set_editor_property("terrain_blocker", r.get("blocker", False))
        # 바위는 막으므로 음수 여유를 쓰지 않는다(GX-015).
        assert not (r.get("blocker") and r["padding"] < 0), r["id"]
        role.set_editor_property("keep_out2d_padding", float(r["padding"]))
        assert abs(role.get_editor_property("keep_out2d_padding") - r["padding"]) < 1e-3
        roles.append(role)
    rules.set_editor_property("crust_mesh", unreal.load_asset(crust_mesh_path()))
    rules.set_editor_property("octant_radius", float(spec.RADIUS))
    rules.set_editor_property("zone", [unreal.Vector2D(float(x), float(y)) for x, y in ZONE])
    rules.set_editor_property("zone_inset", float(ZONE_INSET))
    rules.set_editor_property("seam_clearance", float(spec.SEAM_MARGIN + spec.SEAM_BAND))
    rules.set_editor_property("existing_decoration_clearance", float(EXISTING_CLEARANCE))
    rules.set_editor_property("open_sky_height", 2000.0)
    rules.set_editor_property("keep_out2d", shapes2)
    rules.set_editor_property("keep_out3d", shapes3)
    rules.set_editor_property("roles", roles)
    assert unreal.EditorAssetLibrary.save_loaded_asset(rules, False)
    write("Rules.json", {"asset": RULES, "seed": SEED, "keep_out_2d": k2, "keep_out_3d": k3, "roles": ROLES,
                         "margins_cm": {"capsule": CAPSULE_MARGIN, "cover_rock": COVER_ROCK_MARGIN,
                                        "route_half_width": ROUTE_HALF_WIDTH, "gate_half_width": GATE_HALF_WIDTH,
                                        "landing_radius": LANDING_RADIUS, "hook_line_radius": HOOK_LINE_RADIUS,
                                        "camera_radius": CAMERA_RADIUS, "zone_inset": ZONE_INSET,
                                        "existing_clearance": EXISTING_CLEARANCE, "fern_padding": FERN_PADDING,
                                        "blocker_padding": BLOCKER_PADDING}})
    unreal.log("Ridge decoration rules saved: %d 2D, %d 3D keep-outs, %d roles." % (len(k2), len(k3), len(ROLES)))


def build_graph():
    existing = unreal.load_asset(GRAPH)
    if existing is not None:
        # 디스크에 저장된 그래프는 덮지 않는다. 직전 실패로 메모리에만 남은 미완성 그래프만 지우고 다시 만든다.
        assert not unreal.EditorAssetLibrary.does_asset_exist(GRAPH) or not (
            ROOT / ("Content" + GRAPH[5:] + ".uasset")).exists(), "Graph already saved; inspect it instead of rebuilding."
        assert unreal.EditorAssetLibrary.delete_loaded_asset(existing)
    rules = unreal.load_asset(RULES)
    assert rules, "Build rules first."
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    graph = tools.create_asset(GRAPH.split("/")[-1], FOLDER, unreal.PCGGraph, unreal.PCGGraphFactory())
    assert graph
    sampler_node, sampler = graph.add_node_of_type(unreal.LNPDecorationSamplerSettings)
    sampler.set_editor_property("rules", rules)
    blocker_filter_node = add_attribute_filter(graph, "TerrainBlocker")
    cover_filter_node = add_attribute_filter(graph, "GroundCover")
    # 겹침 정리는 계약·등급별로 따로 한다(V002처럼 한 번에 하면 바닥 면적이 큰 지피가 바위를 지웠다, 1-B5c).
    # 기본값 LargeToSmall·시드 기반 무작위 동률 처리를 그대로 쓴다. 점 범위에는 WPO 여유가 들어 있다.
    prune_blocker_node, _ = graph.add_node_of_type(unreal.PCGSelfPruningSettings)
    prune_tall_node, _ = graph.add_node_of_type(unreal.PCGSelfPruningSettings)
    prune_cover_node, _ = graph.add_node_of_type(unreal.PCGSelfPruningSettings)
    # 우선순위: 키 큰 식생(Fern) > 바위 > 지피. V003은 바위가 Fern을 지워 Fern 7→1이었다(1-B5d 사용자: 바위 20~30%를 Fern으로).
    rock_difference_node = add_difference(graph)
    cover_difference_node = add_difference(graph)
    blocker_node = add_spawner(graph, BLOCKER_TAGS, BLOCKER_PROFILE)
    decoration_node = add_spawner(graph, [DECORATION_TAG], DECORATION_PROFILE)
    graph.add_edge(sampler_node, "Out", blocker_filter_node, "In")
    graph.add_edge(blocker_filter_node, "InsideFilter", prune_blocker_node, "In")
    graph.add_edge(blocker_filter_node, "OutsideFilter", cover_filter_node, "In")
    graph.add_edge(cover_filter_node, "InsideFilter", prune_cover_node, "In")
    graph.add_edge(cover_filter_node, "OutsideFilter", prune_tall_node, "In")
    graph.add_edge(prune_blocker_node, "Out", rock_difference_node, "Source")
    graph.add_edge(prune_tall_node, "Out", rock_difference_node, "Differences")
    graph.add_edge(rock_difference_node, "Out", blocker_node, "In")
    graph.add_edge(prune_cover_node, "Out", cover_difference_node, "Source")
    graph.add_edge(rock_difference_node, "Out", cover_difference_node, "Differences")
    graph.add_edge(cover_difference_node, "Out", decoration_node, "In")
    graph.add_edge(prune_tall_node, "Out", decoration_node, "In")
    graph.add_edge(blocker_node, "Out", graph.get_output_node(), "Out")
    graph.add_edge(decoration_node, "Out", graph.get_output_node(), "Out")
    layout = [(sampler_node, 0, 0), (blocker_filter_node, 300, 0), (cover_filter_node, 600, 150),
              (prune_blocker_node, 600, -150), (prune_cover_node, 900, 300), (prune_tall_node, 900, 100),
              (rock_difference_node, 1200, -150), (cover_difference_node, 1500, 300),
              (blocker_node, 1800, -150), (decoration_node, 1800, 200)]
    for node, x, y in layout:
        node.set_node_position(x, y)
    assert unreal.EditorAssetLibrary.save_loaded_asset(graph, False)
    unreal.log("Ridge decoration PCG graph %s saved." % GRAPH)


def add_attribute_filter(graph, attribute):
    """attribute > 0(기본 상수 Double 0)이면 InsideFilter, 아니면 OutsideFilter로 보낸다(샘플러가 0/1로 남긴다)."""
    node, filtering = graph.add_node_of_type(unreal.PCGAttributeFilteringSettings)
    filtering.set_editor_property("operator", unreal.PCGAttributeFilterOperator.GREATER)
    filtering.set_editor_property("use_constant_threshold", True)
    # Python에서 set_attribute_name은 ref 인자를 바꾸지 않는다(UE5.8 실측). 엔진 export 형식으로 import한다.
    target = filtering.get_editor_property("target_attribute")
    target.import_text("PCGBegin(%s)PCGEnd" % attribute)
    filtering.set_editor_property("target_attribute", target)
    name = filtering.get_editor_property("target_attribute").get_name()
    assert str(name) == attribute, "Filter target attribute was not set: %s" % name
    return node


def add_difference(graph):
    """Source에서 Differences 점 범위와 조금이라도 겹치는 점을 뺀다(Binary: 밀도0, Discrete: 점으로 내보내며 밀도0 제거)."""
    node, difference = graph.add_node_of_type(unreal.PCGDifferenceSettings)
    difference.set_editor_property("density_function", unreal.PCGDifferenceDensityFunction.BINARY)
    difference.set_editor_property("mode", unreal.PCGDifferenceMode.DISCRETE)
    return node


def add_spawner(graph, tags, profile):
    """메시·재질은 샘플러 속성에서 읽고, 계약(태그·충돌 프로필)은 Spawner마다 고정한다."""
    node, spawner = graph.add_node_of_type(unreal.PCGStaticMeshSpawnerSettings)
    spawner.set_mesh_selector_type(unreal.PCGMeshSelectorByAttribute)
    selector = spawner.get_editor_property("mesh_selector_parameters")
    selector.set_editor_property("attribute_name", "MeshPath")
    selector.set_editor_property("use_attribute_material_overrides", True)
    selector.set_editor_property("material_override_attributes", ["MaterialPath"])
    descriptor = selector.get_editor_property("template_descriptor")
    descriptor.set_editor_property("component_tags", tags)
    descriptor.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    descriptor.set_editor_property("can_ever_affect_navigation", False)
    descriptor.set_editor_property("evaluate_world_position_offset", True)
    descriptor.set_editor_property("body_instance", body_for_profile(profile))
    selector.set_editor_property("template_descriptor", descriptor)
    return node


def body_for_profile(profile):
    """BodyInstance의 프로필 이름만 Python으로 바꾸면 충돌 활성·응답이 로드되지 않는다(Blocker ISM이 NoCollision으로 나옴).
    프로필을 실제로 적용하는 set_collision_profile_name을 임시 컴포넌트에 호출해 BodyInstance 전체를 복사한다."""
    probe = unreal.StaticMeshComponent()
    probe.set_collision_profile_name(profile)
    body = probe.get_editor_property("body_instance")
    assert str(body.get_editor_property("collision_profile_name")) == profile
    expected = unreal.CollisionEnabled.NO_COLLISION if profile == DECORATION_PROFILE else \
        unreal.CollisionEnabled.QUERY_AND_PHYSICS
    assert probe.get_collision_enabled() == expected, "Profile %s did not apply." % profile
    return body


def repair_graph_bodies():
    """이미 저장된 그래프(GRAPH)의 Spawner BodyInstance를 body_for_profile로 다시 채운다(메시·속성 선택은 그대로)."""
    graph = unreal.load_asset(GRAPH)
    assert graph, "Build the graph first."
    fixed = []
    for node in graph.get_editor_property("nodes"):
        settings = node.get_settings()
        if not isinstance(settings, unreal.PCGStaticMeshSpawnerSettings):
            continue
        selector = settings.get_editor_property("mesh_selector_parameters")
        descriptor = selector.get_editor_property("template_descriptor")
        tags = [str(t) for t in descriptor.get_editor_property("component_tags")]
        profile = BLOCKER_PROFILE if tags == BLOCKER_TAGS else DECORATION_PROFILE
        assert tags in (BLOCKER_TAGS, [DECORATION_TAG]), "Unexpected spawner tags: %s" % tags
        descriptor.set_editor_property("body_instance", body_for_profile(profile))
        selector.set_editor_property("template_descriptor", descriptor)
        fixed.append(profile)
    assert sorted(fixed) == sorted([BLOCKER_PROFILE, DECORATION_PROFILE]), fixed
    assert unreal.EditorAssetLibrary.save_loaded_asset(graph, False)
    unreal.log("Ridge decoration PCG graph spawner bodies repaired: %s" % fixed)


def place():
    require_wip_clean()
    graph = unreal.load_asset(GRAPH)
    assert graph, "Build the graph first."
    editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = list(editor.get_all_level_actors())
    assert not any(a.get_actor_label() == ACTOR_LABEL for a in existing), "PCG actor already exists."
    meshes = [a for a in existing if isinstance(a, unreal.StaticMeshActor)]
    assert len(meshes) == 15, "Expected crust/rock 3 + manual decoration 12."
    backup = OUT / "BeforePCG.umap"
    source = ROOT / ("Content" + LEVEL[5:] + ".umap")
    if backup.exists():
        # 저장하지 않고 버린 이전 시도의 복구점이면 디스크 WIP와 같아야 한다. 다르면 사람이 먼저 확인한다.
        assert hashlib.sha256(backup.read_bytes()).digest() == hashlib.sha256(source.read_bytes()).digest(), \
            "Recovery point differs from the saved WIP; inspect before retrying."
    else:
        OUT.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, backup)
    actor = editor.spawn_actor_from_class(unreal.PCGVolume, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    assert actor
    actor.set_actor_label(ACTOR_LABEL)
    actor.set_folder_path("Graphics/DecorationPCG_V001")
    # 볼륨 브러시는 기본이 QueryOnly다. 계약 태그 없는 충돌은 SurfaceData 수집기가 거부하고 게임 trace도 맞을 수 있다.
    brush = actor.get_component_by_class(unreal.BrushComponent)
    brush.set_collision_profile_name("NoCollision")
    assert brush.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    component = actor.get_component_by_class(unreal.PCGComponent)
    component.set_graph(graph)
    component.set_editor_property("seed", SEED)
    component.set_editor_property("generation_trigger", unreal.PCGComponentGenerationTrigger.GENERATE_ON_DEMAND)
    component.generate(True)
    unreal.log("Ridge decoration PCG generation requested; inspect, then save the WIP after review.")


def build_blocker_mesh():
    """원본 Sandstone은 ComplexAsSimple(렌더 삼각형7,166개)이다. 삼각형 sweep 대신 기존 생성 컨벡스를 충돌로 쓰는 파생본."""
    assert not unreal.EditorAssetLibrary.does_asset_exist(SANDSTONE_BLOCKER), "Derived mesh already exists; inspect it."
    source = unreal.load_asset(SANDSTONE_SOURCE)
    source_body = source.get_editor_property("body_setup")
    assert source_body.get_editor_property("collision_trace_flag") == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
    mesh = unreal.EditorAssetLibrary.duplicate_asset(SANDSTONE_SOURCE, SANDSTONE_BLOCKER)
    assert mesh
    body = mesh.get_editor_property("body_setup")
    convex = len(body.get_editor_property("agg_geom").get_editor_property("convex_elems"))
    assert convex >= 1, "Derived mesh has no simple convex collision."
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    assert body.get_editor_property("collision_trace_flag") == unreal.CollisionTraceFlag.CTF_USE_DEFAULT
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    assert source_body.get_editor_property("collision_trace_flag") == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
    write("BlockerMesh.json", {"source": SANDSTONE_SOURCE, "derived": SANDSTONE_BLOCKER, "convex_elems": convex,
                               "trace_flag": "CTF_UseDefault"})
    unreal.log("Sandstone blocker mesh saved with %d convex element(s)." % convex)


def manual_blocker_actors():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    found = {}
    for label in MANUAL_BLOCKERS:
        matches = [a for a in actors if a.get_actor_label() == label]
        assert len(matches) == 1 and isinstance(matches[0], unreal.StaticMeshActor), "Expected one actor: " + label
        found[label] = matches[0]
    return found


def transform_values(t):
    q = t.rotation
    return [round(v, 4) for v in (t.translation.x, t.translation.y, t.translation.z, q.x, q.y, q.z, q.w,
                                  t.scale3d.x, t.scale3d.y, t.scale3d.z)]


def component_state(component):
    return {"mesh": component.static_mesh.get_path_name(), "tags": [str(t) for t in component.component_tags],
            "profile": str(component.get_collision_profile_name()), "collision": str(component.get_collision_enabled()),
            "mobility": str(component.mobility),
            "affects_navigation": component.get_editor_property("can_ever_affect_navigation")}


def promote():
    require_wip_clean()
    assert unreal.EditorAssetLibrary.does_asset_exist(SANDSTONE_BLOCKER), "Run blocker-mesh first."
    backup = OUT / "BeforeBlocker.umap"
    source = ROOT / ("Content" + LEVEL[5:] + ".umap")
    if backup.exists():
        assert hashlib.sha256(backup.read_bytes()).digest() == hashlib.sha256(source.read_bytes()).digest(), \
            "Recovery point differs from the saved WIP; inspect before retrying."
    else:
        OUT.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, backup)
    actors = manual_blocker_actors()
    # 모두 확인한 뒤에만 바꿔 일부만 승격된 상태를 남기지 않는다.
    for label, actor in actors.items():
        state = component_state(actor.static_mesh_component)
        assert state["tags"] == [DECORATION_TAG] and state["profile"] == DECORATION_PROFILE, "Unexpected state: " + label
    record = {}
    for label, actor in actors.items():
        component = actor.static_mesh_component
        before = component_state(component)
        transform = actor.get_actor_transform()
        actor.modify()
        component.modify()
        if MANUAL_BLOCKERS[label]:
            assert before["mesh"].split(".")[0] == SANDSTONE_SOURCE
            assert component.set_static_mesh(unreal.load_asset(MANUAL_BLOCKERS[label]))
        component.set_editor_property("component_tags", BLOCKER_TAGS + [MANUAL_DECORATION_TAG])
        component.set_collision_profile_name(BLOCKER_PROFILE)
        after = component_state(component)
        assert after["profile"] == BLOCKER_PROFILE and component.get_collision_enabled() == \
            unreal.CollisionEnabled.QUERY_AND_PHYSICS, "Promotion did not apply: " + label
        assert transform_values(actor.get_actor_transform()) == transform_values(transform), "Transform changed: " + label
        record[label] = {"before": before, "after": after}
    write("Promotion.json", record)
    unreal.log("Promoted %d manual rocks to Static+Blocker; review, then save the WIP." % len(record))


def regenerate():
    # promote가 남긴 WIP 변경만 허용한다. 다른 미저장 변경은 먼저 사람이 확인한다.
    require_wip_clean(allowed=(LEVEL,))
    for label, actor in manual_blocker_actors().items():
        assert MANUAL_DECORATION_TAG in [str(t) for t in actor.static_mesh_component.component_tags], \
            "Run promote first: " + label
    graph = unreal.load_asset(GRAPH)
    assert graph, "Build the graph first."
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    pcg = [a for a in actors if a.get_actor_label() == ACTOR_LABEL]
    assert len(pcg) == 1, "Expected the existing PCG actor."
    component = pcg[0].get_component_by_class(unreal.PCGComponent)
    assert component.get_editor_property("seed") == SEED
    component.set_graph(graph)
    component.generate(True)
    unreal.log("Ridge decoration PCG regeneration with %s requested; inspect, then save the WIP after review." % GRAPH)


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    {"blocker-mesh": build_blocker_mesh, "rules": build_rules, "graph": build_graph, "place": place,
     "promote": promote, "regenerate": regenerate, "repair-graph": repair_graph_bodies}[mode]()
