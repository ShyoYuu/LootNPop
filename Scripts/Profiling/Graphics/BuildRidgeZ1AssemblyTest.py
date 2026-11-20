"""UE Cmd에서 Z1 ED 조립체 시험 레벨을 만든다.

인자:
- lights: 현재 레벨(TestMap03)의 조명 액터 목록을 읽는다.
- build: TestMap03 조명을 복사한 새 시험 레벨을 만들어 저장하고, Level Instance 배치 transform을 JSON으로 쓴다.
- instances: 시험 레벨에 V002와 ED 조립체 Level Instance를 넣고 저장한 뒤 비교 시점으로 옮긴다.
- view: 뷰포트를 비교 시점으로 옮긴다.
- wall <버전>: 시험 레벨이 열린 상태에서 V002 사본(서쪽 벽 제외)과 WALLS의 조립체로 Z1AssemblyWall_<버전>을 만든다.
- importcrust <버전>: Blender가 만든 산괴 고원 지각(Z1MassifCover)을 반입한다.
- drape <버전>: CloseRange 평면 그룹을 구면 지각에 다시 앉힌 Z1MassifDrape_<버전>을 만든다(V003부터 지면 타일을 경사에 맞춤).
- surface [버전]: 구역 마스크를 서쪽 고원까지 넓힌 지각 재질(Z1MassifSurface)을 만든다. V002는 V001에서 지면층 텍스처 세트만
  바꾼 후보, V003은 구역을 이음매 띠를 뺀 옥탄트 전체로 넓힌 후보다.
- puddle: ED 진흙 웅덩이 재질 사본에 Single Layer Water 출력 노드를 더한다(drape V006이 이 사본을 쓴다).
- band <버전>: CR_Ditch 둑 묶음을 분지 테두리 지각 표면에 옮겨 심은 Z1RimBand_<버전>을 만든다(GX-027).
- look / crflat / flatlook: 확인 시점·CloseRange 평면 비교 레벨.
자동 스크린샷(AutomationLibrary.take_high_res_screenshot)은 Megascans 재질 셰이더 assert로 에디터를 죽이므로 쓰지 않는다.
"""
import importlib
import json
import math
from pathlib import Path
import sys

import unreal

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parents[1])]
import RidgeZ1CompositionSpec as spec

# 에디터 Python은 모듈을 캐시하므로 spec 변경을 매 실행마다 다시 읽는다.
importlib.reload(spec)

OUT = Path(r"D:/UnrealProjects/LootNPop/Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119")
LIGHT_SOURCE = "/Game/Maps/TestMap03"
LEVEL = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1AssemblyTest_V001/Z1AssemblyTest_V001"
V002 = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1Composition_V002/LVI_Ridge_EDTerrain_Z1Composition_V002"
ASSEMBLY = "/Game/PCG/Assets/PCGAssemblies/Forest/PL_LargeCliff_01b"
LIGHT_CLASSES = ("PointLight", "PostProcessVolume")
# 서쪽 벽(x≈-1,600) 앞 분지 바닥. 원점은 조립체 바닥 중앙이다.
ASSEMBLY_LOCAL_XY = (0, -1600)
ASSEMBLY_YAW = 0.0
# 동쪽 분지에서 서쪽 벽과 조립체를 함께 보는 시점(눈높이 170cm).
VIEW_EYE = (2800, -1200, 170)
VIEW_TARGET = (-1600, -1400, 1500)


def xyz(v):
    return [round(v.x, 3), round(v.y, 3), round(v.z, 3)]


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def package(obj):
    return obj.get_path_name().split(".")[0]


def dirty():
    saving = unreal.EditorLoadingAndSavingUtils
    return [p.get_path_name() for p in list(saving.get_dirty_map_packages()) + list(saving.get_dirty_content_packages())]


def point(x, y, h_offset=0.0):
    return unreal.Vector(*spec.ridge.point(x, y, spec.west_cover_ground(x, y) + h_offset))


def frame_rotation(x, y, yaw):
    # 현지 Up은 구 중심 방향(-normalize(pos))이고, 현지 +X를 옥탄트 로컬 +X에 맞춘다.
    position = point(x, y)
    up = position * (-1.0 / position.length())
    direction = point(x + 10, y) - position
    frame = unreal.MathLibrary.make_rot_from_xz(direction - up * up.dot(direction), up)
    return unreal.MathLibrary.compose_rotators(unreal.Rotator(roll=0, pitch=0, yaw=yaw), frame)


def light_actors():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    return [a for a in actors if a.get_class().get_name() in LIGHT_CLASSES]


def lights():
    rows = []
    for actor in light_actors():
        rot = actor.get_actor_rotation()
        rows.append({"label": actor.get_actor_label(), "class": actor.get_class().get_name(),
                     "location": xyz(actor.get_actor_location()), "rotation": [rot.pitch, rot.yaw, rot.roll],
                     "folder": str(actor.get_folder_path())})
    (OUT / "TestMap03Lights.json").write_text(json.dumps({"world": world().get_path_name(), "actors": rows},
                                                         indent=1), encoding="utf-8")
    unreal.log("Z1_ASSEMBLY_LIGHTS_READY %d" % len(rows))


def build():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    saving = unreal.EditorLoadingAndSavingUtils
    assert package(world()) == LIGHT_SOURCE, package(world())
    assert not dirty(), dirty()
    assert not unreal.EditorAssetLibrary.does_asset_exist(LEVEL), LEVEL
    assert unreal.EditorAssetLibrary.does_asset_exist(V002) and unreal.EditorAssetLibrary.does_asset_exist(ASSEMBLY)

    # TestMap03 조명은 클립보드 복사로 모든 컴포넌트 설정을 그대로 옮긴다. TestMap03은 바꾸지 않는다.
    source = light_actors()
    expected = sorted((a.get_actor_label(), tuple(xyz(a.get_actor_location()))) for a in source)
    actors.set_selected_level_actors(source)
    # Python 서브시스템에 클립보드 API가 없어 에디터 Exec 명령을 쓴다.
    unreal.SystemLibrary.execute_console_command(world(), "EDIT COPY")
    actors.select_nothing()
    assert not dirty(), dirty()

    assert saving.new_blank_map(False)
    unreal.SystemLibrary.execute_console_command(world(), "EDIT PASTE")
    pasted = light_actors()
    got = sorted((a.get_actor_label(), tuple(xyz(a.get_actor_location()))) for a in pasted)
    assert [g[1] for g in got] == [e[1] for e in expected], (got, expected)
    for actor in pasted:
        actor.set_folder_path("Lighting_TestMap03")
    actors.select_nothing()
    assert saving.save_map(world(), LEVEL), LEVEL

    x, y = ASSEMBLY_LOCAL_XY
    location, rotation = point(x, y), frame_rotation(x, y, ASSEMBLY_YAW)
    eye = point(*VIEW_EYE)
    target = point(*VIEW_TARGET)
    up = eye * (-1.0 / eye.length())
    view = unreal.MathLibrary.make_rot_from_xz(target - eye, up)
    report = {"level": LEVEL, "lights": [g[0] for g in got],
              "assembly": {"level": ASSEMBLY, "local_xy_cm": [x, y], "yaw_deg": ASSEMBLY_YAW,
                           "location": xyz(location), "rotation": [rotation.pitch, rotation.yaw, rotation.roll]},
              "v002": {"level": V002},
              "view": {"location": xyz(eye), "rotation": [view.pitch, view.yaw, view.roll]}}
    (OUT / "AssemblyTestLevel.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
    unreal.log("Z1_ASSEMBLY_BUILD_READY")


def view():
    report = json.loads((OUT / "AssemblyTestLevel.json").read_text(encoding="utf-8"))
    pitch, yaw, roll = report["view"]["rotation"]
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
        unreal.Vector(*report["view"]["location"]), unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll))


def instances():
    # build가 저장한 조명 전용 레벨에 V002와 조립체를 Level Instance로 넣고 같은 실행에서 저장한다.
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if package(world()) != LEVEL:
        assert not dirty(), dirty()
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL), LEVEL
    labels = {"LI_Z1Composition_V002", "LI_ED_PL_LargeCliff_01b"}
    existing = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.LevelInstance)]
    assert all(a.get_actor_label() in labels for a in existing), [a.get_actor_label() for a in existing]
    # 저장 전에 중단된 이전 실행이 남긴 이 시험의 Level Instance만 지운다.
    for actor in existing:
        actors.destroy_actor(actor)
    report = json.loads((OUT / "AssemblyTestLevel.json").read_text(encoding="utf-8"))
    pitch, yaw, roll = report["assembly"]["rotation"]
    rows = (("LI_Z1Composition_V002", V002, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0)),
            ("LI_ED_PL_LargeCliff_01b", ASSEMBLY, unreal.Vector(*report["assembly"]["location"]),
             unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll)))
    placed = []
    for label, level, location, rotation in rows:
        actor = actors.spawn_actor_from_class(unreal.LevelInstance, location, rotation)
        actor.set_actor_label(label)
        actor.set_world_asset(unreal.load_asset(level))
        actor.load_level_instance()
        # 로드는 다음 틱 이후 끝나므로 여기서는 연결된 레벨만 확인한다.
        assert package(actor.get_world_asset()) == level, label
        placed.append({"label": label, "level": level, "location": xyz(actor.get_actor_location())})
    assert unreal.EditorLoadingAndSavingUtils.save_map(world(), LEVEL), LEVEL
    view()
    report["instances"] = placed
    (OUT / "AssemblyTestLevel.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
    unreal.log("Z1_ASSEMBLY_INSTANCES_READY")


CR = "/Game/LNPBake/CloseRange/CR_"
WALL_FOLDER = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1AssemblyWall_{0}/Z1AssemblyWall_{0}"
ASSEMBLIES = "/Game/PCG/Assets/PCGAssemblies/"
# V002의 서쪽 벽과 왕관은 조립체로 대체하므로 옮기지 않는다.
WALL_REPLACED = ("ED_Z1_West", "ED_Z1_NorthCrown")
# (이름, 조립체, 로컬 x, y, 현지 Up 기준 yaw, 높이 오프셋[, 균등 스케일, 높이 기준]).
# 높이 기준 "floor"는 서쪽 벽 발치 분지 바닥, "local"은 그 위치의 V002 기반면이다.
FRONT_02 = (
    ("ASM_Z1_WallSouth", "Forest/PL_LargeCliff_02c", -2000, -2700, 0, -100),
    ("ASM_Z1_WallCenter", "Forest/PL_LargeCliff_02e", -2100, -200, 0, -150),
    ("ASM_Z1_WallNorth", "Forest/PL_LargeCliff_02d", -2000, 2300, 0, -100),
)
WALLS = {
    # 01b·01c는 암석 85개를 공유하므로 yaw로 다른 면을 보인다. 결과: 독립 기둥 4개로 읽혀 벽이 되지 않았다.
    "V001": (
        ("ASM_Z1_WallSouth", "Forest/PL_LargeCliff_01b", -1800, -3000, 0, -50),
        ("ASM_Z1_WallSouthCenter", "Forest/PL_LargeCliff_01c", -1750, -1150, 90, -50),
        ("ASM_Z1_WallNorthCenter", "Forest/PL_LargeCliff_01b", -1750, 700, 180, -50),
        ("ASM_Z1_WallNorth", "Forest/PL_LargeCliff_01c", -1800, 2550, 270, -50),
    ),
    # 폭 약 35m의 02 계열을 25m 간격으로 약 10m 겹치고, 02b를 뒤쪽 높은 곳에 두어 기반면 사면을 가린다.
    # 결과: 이어진 벽으로 읽혔지만 사용자는 덩치가 미묘하다고 판정했다.
    "V002": FRONT_02 + (("ASM_Z1_WallBack", "Forest/PL_LargeCliff_02b", -3100, -1000, 180, 600),),
    # 앞은 02 계열 벽, 뒤는 원경용 배경 조립체를 줄여 산괴와 하늘선을 만든다(ED의 근경·원경 층 구조).
    "V003": FRONT_02 + (
        ("BG_Z1_Hoodoo", "Background/Asmbl_BG_Hoodoo_01b_SmartAsset", -5200, -600, 0, -1000, 0.4, "local"),
        ("BG_Z1_Mountain", "Background/Asmbl_BG_Mountain_04b_SmartAsset", -5500, 3500, 0, -1000, 0.3, "local"),
    ),
    # ED CloseRange의 PCG 산괴(구운 레벨)를 벽 자리에 원래 크기로 놓는다. ED 바닥은 원점 +4m라 -400으로 분지 바닥에 맞춘다.
    # 결과: 서쪽 사면이 분지 바닥에서 x -2,000~-3,000 사이에 58m까지 솟아 산괴가 기반면 속에 묻혔다.
    "V004": (("CR_Z1_LargeAssembly", "/Game/LNPBake/CloseRange/CR_LargeAssembly", -3500, -200, 0, -400),),
    # ED 구조대로 가파른 면은 02 절벽 조립체, 고원 위는 CloseRange 산괴(숲 왕관)로 덮는다.
    # 산괴 중심 기반면은 7,439cm(분지 기준)이고 동쪽 가장자리 고원은 약 5,900cm라 -1,900으로 동쪽에 맞춘다.
    "V005": FRONT_02 + (("CR_Z1_Crown", "/Game/LNPBake/CloseRange/CR_LargeAssembly", -5500, -200, 0, -1900, 1.0, "local"),),
    # 기반면을 산괴에 맞춘다. 02 벽 뒤를 분지 기준 +46m 고원으로 깎은 Z1MassifCover 지각 위에 산괴를 앉힌다.
    # ED 바닥은 산괴 원점 +4m이므로 원점은 고원 -4m다.
    # 결과: 02 벽 바로 뒤 봉우리(약 +57m)가 남아 정면·원경 모두 산괴를 가렸다.
    "V006": FRONT_02 + (("CR_Z1_Plateau", "/Game/LNPBake/CloseRange/CR_LargeAssembly", spec.MASSIF_CENTER[0],
                         spec.MASSIF_CENTER[1], 0, spec.MASSIF_PLATEAU_CM - 400),),
    # 고원을 02 벽 안쪽부터 깎은 Z1MassifCover V002 지각을 쓴다.
    # 결과: 벽 뒤 봉우리가 사라졌다. 고원 위 산괴 둘레 맨 지면이 넓게 남았다. CloseRange 바닥 그룹이 원점 ±3m라 -4m는 잘못이었다.
    "V007": FRONT_02 + (("CR_Z1_Plateau", "/Game/LNPBake/CloseRange/CR_LargeAssembly", spec.MASSIF_CENTER[0],
                         spec.MASSIF_CENTER[1], 0, spec.MASSIF_PLATEAU_CM - 400),),
    # 원점을 고원 높이에 두고 CloseRange 바닥·작은 무더기·바위를 같은 원점에 함께 놓는다(원래 상대 배치).
    "V008": FRONT_02 + tuple(("CR_Z1_" + group, CR + group, spec.MASSIF_CENTER[0], spec.MASSIF_CENTER[1], 0,
                              spec.MASSIF_PLATEAU_CM) for group in ("LargeAssembly", "Ground", "SmallAssembly", "Props")),
    # 결과: 바닥 그룹이 곡률(중심에서 d만큼 떨어지면 d²/2R 아래)로 대부분 묻혔다.
    # 산괴는 강체로 두고, 바닥·작은 무더기·바위는 인스턴스마다 구면 지각에 다시 앉힌 Z1MassifDrape 레벨을 쓴다.
    "V009": FRONT_02 + (("CR_Z1_LargeAssembly", CR + "LargeAssembly", spec.MASSIF_CENTER[0], spec.MASSIF_CENTER[1], 0,
                         spec.MASSIF_PLATEAU_CM),
                        ("CR_Z1_Drape", "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1MassifDrape_V001/"
                         "Z1MassifDrape_V001", 0, 0, 0, 0, 1.0, "world")),
    # Ditch(지면 덮개·하천 둑)까지 드레이프한 레벨을 쓴다. 하상은 ED 지면 아래라 고원 속에 묻힌다.
    "V010": FRONT_02 + (("CR_Z1_LargeAssembly", CR + "LargeAssembly", spec.MASSIF_CENTER[0], spec.MASSIF_CENTER[1], 0,
                         spec.MASSIF_PLATEAU_CM),
                        ("CR_Z1_Drape", "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1MassifDrape_V002/"
                         "Z1MassifDrape_V002", 0, 0, 0, 0, 1.0, "world")),
}
# V010과 같은 배치에 지각 재질 구역을 서쪽 산괴 고원까지 넓힌 재질(Z1MassifSurface)을 입힌다.
WALLS["V011"] = WALLS["V010"]
MASSIF_FOLDER = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1MassifCover_{}"
MASSIF_OUT = Path(r"D:/UnrealProjects/LootNPop/Saved/SliceOctant/Ridge_00")


def massif_crust(version):
    return MASSIF_FOLDER.format(version) + "/SM_Octant_Ridge_00_%s_R30000" % spec.MASSIF_COVER_VERSIONS[version][0]


# 버전별로 V002 서쪽 덮개 지각을 다른 지각 메시로 바꾼다.
WALL_CRUST = {"V006": massif_crust("V001"), "V007": massif_crust("V002"), "V008": massif_crust("V002"), "V009": massif_crust("V002"), "V010": massif_crust("V002"), "V011": massif_crust("V002")}
SURFACE_FOLDER = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1MassifSurface_V001"
SURFACE_SOURCE = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1Composition_V001/"
# 하상 재질(V005 절벽·지면 + 하상)은 1-B0 대표 구역(x -3,200 동쪽) 밖을 평지색으로 칠한다.
# 사본에서 구역 마스크에 서쪽 산괴 고원과 그 남북 사면을 더한다. 하상 가중치는 원래 구역 마스크를 그대로 쓴다.
SURFACE_SCOPE = """
const float west = step(-14000.0, Data.x) * step(Data.x, -1500.0) * step(-8000.0, Data.y) * step(Data.y, 8000.0);
return float2(max(Data.w, west), smoothstep(0.3420201, 0.8660254, Data.z));
"""
WALL_MATERIAL = {"V011": SURFACE_FOLDER + "/MI_Ridge_Z1_MassifSurface"}
# 지각 재질 후보(V002~). V001 사본에서 지면층 텍스처 세트(MossyRockyGround BC·N·AoRDp)만 바꾸고 MI 값을 덮어쓴다.
# CloseRange 바닥 느낌은 SM_ForestGround_01 등의 Megascans 3D 스캔 재질(M_MS_Default_Material_VT_DynamicLayering)에서
# 나오지만, 그 텍스처는 메시 고유 UV 아틀라스라 지각에 반복 투영할 수 없다. 그래서 같은 갈색 낙엽·잔가지 흙에 가까운
# 타일링 표면으로 지면층을 바꾸고, 구면 좌표·삼각 격자 반복 완화·경사/구역 마스크·절벽층(BeachCliff)·하상은 그대로 둔다.
SURFACE_VERSIONS = {
    "V002": {"ground": "DriedGrassOnDampSoil", "scalar": {}, "vector": {}},
}
for _version in SURFACE_VERSIONS:
    WALL_MATERIAL["Surface" + _version] = (SURFACE_FOLDER.rsplit("_", 1)[0] + "_" + _version
                                           + "/MI_Ridge_Z1_MassifSurface")
# V011과 같고 드레이프만 사면 지면 타일을 법선으로 눕히고 급경사에서 뺀 Z1MassifDrape_V003(V012)·V004(V013)로 바꾼다.
for _wall, _drape in (("V012", "V003"), ("V013", "V004")):
    WALLS[_wall] = WALLS["V011"][:-1] + (("CR_Z1_Drape", "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/"
                                          "Z1MassifDrape_{0}/Z1MassifDrape_{0}".format(_drape), 0, 0, 0, 0, 1.0, "world"),)
    WALL_CRUST[_wall] = WALL_CRUST["V011"]
    WALL_MATERIAL[_wall] = WALL_MATERIAL["V011"]
# V013과 같은 배치(드레이프 높이는 V002 고원)에 CloseRange 하상 띠를 50cm 판 지각 Z1MassifCover V003만 바꿔 입힌다.
WALLS["V014"] = WALLS["V013"]
WALL_CRUST["V014"] = massif_crust("V003")
WALL_MATERIAL["V014"] = WALL_MATERIAL["V013"]
# V014에서 드레이프만 고원 밖으로 튀어나온 지면 타일을 뺀 Z1MassifDrape_V005로 바꾼다.
WALLS["V015"] = WALLS["V014"][:-1] + (("CR_Z1_Drape", "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/"
                                       "Z1MassifDrape_V005/Z1MassifDrape_V005", 0, 0, 0, 0, 1.0, "world"),)
WALL_CRUST["V015"] = WALL_CRUST["V014"]
WALL_MATERIAL["V015"] = WALL_MATERIAL["V014"]
# V015 결과: 타일 4모서리 꺼짐으로 211개를 뺐지만 동쪽 테두리는 그대로였다. ED 섬 바깥 둑 고리 전체가 원점 +3m 단이라
# 테두리 타일은 고원에 닿는 자리가 원래 없다. V016은 V014(Drape V004)에 섬 지지면을 따라 올린 지각 Z1MassifCover V004를 입힌다.
WALLS["V016"] = WALLS["V014"]
WALL_CRUST["V016"] = massif_crust("V004")
WALL_MATERIAL["V016"] = WALL_MATERIAL["V014"]
# V016 결과: 허공 느낌은 사라졌지만 바깥 테두리 둥근 타일 가장자리가 지각 위로 버섯 갓처럼 튀어나와 그림자가 졌다.
# V017은 테두리 밖을 지지면 +10cm로 1m 넓힌 지각 Z1MassifCover V005로 바꾼다.
WALLS["V017"] = WALLS["V016"]
WALL_CRUST["V017"] = massif_crust("V005")
WALL_MATERIAL["V017"] = WALL_MATERIAL["V016"]
# V018은 V017에서 지각 재질만 지면층을 DriedGrassOnDampSoil로 바꾼 Z1MassifSurface V002로 바꾼다.
WALLS["V018"] = WALLS["V017"]
WALL_CRUST["V018"] = WALL_CRUST["V017"]
WALL_MATERIAL["V018"] = WALL_MATERIAL["SurfaceV002"]
# 지각 재질 구역 후보(V003~). V001 사본에서 구역 Custom 노드만 바꾼다. 하상 가중치는 원래 1-B0 구역 그대로다.
# V001 구역(1-B0 다각형 + 서쪽 직사각형) 밖의 V012 지각은 재질 상수 평지색(보라 0.29, 0.23, 0.38)으로 칠해진다.
# GX-021의 교체 범위는 경계 전이부를 뺀 Ridge 지표 전체이므로, 구역을 원래 좌표 함수의 이음매 제외(세 이음매 평면까지
# 메시 로컬 거리 5,500cm 미만 = 이음매 보존 15m + 전이 40m)만 남긴 옥탄트 전체로 넓힌다. 이음매 띠는 평지색 그대로 둔다.
SURFACE_SCOPE_VERSIONS = {
    "V003": """
const float seam = min(PositionLocal.x, min(PositionLocal.y, PositionLocal.z));
return float2(step(5500.0, seam), smoothstep(0.3420201, 0.8660254, Data.z));
""",
}
for _version in SURFACE_SCOPE_VERSIONS:
    WALL_MATERIAL["Surface" + _version] = (SURFACE_FOLDER.rsplit("_", 1)[0] + "_" + _version
                                           + "/MI_Ridge_Z1_MassifSurface")
# TestMap03 조명은 구 중심 점광원과 보조 점광원 4개뿐이다. 광원이 모두 구 안쪽이라 어느 지표에서나 거의 머리 위에 있고
# (Z1 02 벽 발치에서 현지 Up과의 cos 0.93~1.00), 분지를 보는 02 벽 정면(현지 동쪽)은 cos +0.27 이하의 스치는 빛만 받는다.
# Sky Light가 없어 앰비언트·Lumen 반사 실패 시 하늘값이 0이다. 시험 레벨에만 균일 회색 큐브맵 Sky Light를 더한다.
# 균일 큐브맵은 방향과 무관해 구면에서도 쓸 수 있다. 아래 반구 검정(기본 true)은 월드 -Z 쪽을 지우는데, Ridge의 현지 Up은
# 월드 -Z 성분을 가지므로 반드시 끈다.
WALL_SKY = {}
# V019는 V017에 Sky Light만 더한다(02 벽 정면 그늘). V020은 V019에서 지각 재질만 구역 V003으로 바꾼다.
WALLS["V019"] = WALLS["V017"]
WALL_CRUST["V019"] = WALL_CRUST["V017"]
WALL_MATERIAL["V019"] = WALL_MATERIAL["V017"]
# 세기는 분지 정면 시점에서 0·8·30·50·100을 비교해 정했다. 8은 거의 차이가 없고, 50에서 02 벽 정면이 읽히며
# 바닥 밝기는 약 2배가 된다(고정 노출 EV100 6.0 그대로).
WALL_SKY["V019"] = {"cubemap": "/Engine/EngineResources/GrayTextureCube", "intensity": 50.0}
WALLS["V020"] = WALLS["V019"]
WALL_CRUST["V020"] = WALL_CRUST["V019"]
WALL_MATERIAL["V020"] = WALL_MATERIAL["SurfaceV003"]
WALL_SKY["V020"] = WALL_SKY["V019"]
# V021은 V020에서 드레이프만 웅덩이 재질을 고친 Z1MassifDrape_V006으로 바꾼다.
WALLS["V021"] = WALLS["V020"][:-1] + (("CR_Z1_Drape", "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/"
                                       "Z1MassifDrape_V006/Z1MassifDrape_V006", 0, 0, 0, 0, 1.0, "world"),)
WALL_CRUST["V021"] = WALL_CRUST["V020"]
WALL_MATERIAL["V021"] = WALL_MATERIAL["V020"]
WALL_SKY["V021"] = WALL_SKY["V020"]
# 분지 저시점(눈높이 170cm)에서 02 벽 3개가 같은 "기둥 두 개 + 세로 홈" 리듬과 같은 꼭대기 높이로 일렬 담장처럼 읽히고,
# 기둥 꼭대기 사이 V자 틈으로 하늘이 보였다. V022는 세 벽에 yaw·높이 차를 주고, 두 접합부 뒤 고원(분지 기준 +46m)에
# 01 계열 기둥을 묻어 세워 왕관 틈을 막는다. 01b·01c는 암석을 공유하므로 yaw를 달리해 다른 면을 보인다.
FRONT_02_V022 = (
    ("ASM_Z1_WallSouth", "Forest/PL_LargeCliff_02c", -2000, -2700, 10, -100),
    ("ASM_Z1_WallCenter", "Forest/PL_LargeCliff_02e", -2150, -200, -8, 250),
    ("ASM_Z1_WallNorth", "Forest/PL_LargeCliff_02d", -2000, 2300, 6, -350),
)
CROWN_V022 = (
    ("ASM_Z1_CrownSouth", "Forest/PL_LargeCliff_01b", -3200, -1450, 20, spec.MASSIF_PLATEAU_CM - 1500),
    ("ASM_Z1_CrownNorth", "Forest/PL_LargeCliff_01c", -3200, 1050, -25, spec.MASSIF_PLATEAU_CM - 1500),
)
WALLS["V022"] = FRONT_02_V022 + CROWN_V022 + WALLS["V021"][len(FRONT_02):]
WALL_CRUST["V022"] = WALL_CRUST["V021"]
WALL_MATERIAL["V022"] = WALL_MATERIAL["V021"]
WALL_SKY["V022"] = WALL_SKY["V021"]
# V022 결과: 01 기둥 두 개가 벽 위 왕관을 이뤄 하늘 틈이 줄었지만, 가운데 벽 뒤(두 기둥 사이)에 틈이 남았다.
# 가운데 벽을 2.5m 올리자 발치 잔돌 판 아래가 떠 검은 빈 공간이 생겼다. 벽은 올리지 않고 북쪽 벽만 더 묻어 높이 차를 낸다.
# V023은 가운데 벽 뒤 고원에 02b를 더 낮게 묻어 틈을 막고, 왕관 셋의 높이를 북 > 남 > 가운데로 다르게 한다.
FRONT_02_V023 = (
    ("ASM_Z1_WallSouth", "Forest/PL_LargeCliff_02c", -2000, -2700, 10, -100),
    ("ASM_Z1_WallCenter", "Forest/PL_LargeCliff_02e", -2150, -200, -8, -150),
    ("ASM_Z1_WallNorth", "Forest/PL_LargeCliff_02d", -2000, 2300, 6, -450),
)
CROWN_V023 = (
    ("ASM_Z1_CrownSouth", "Forest/PL_LargeCliff_01b", -3200, -1450, 20, spec.MASSIF_PLATEAU_CM - 1500),
    ("ASM_Z1_CrownCenter", "Forest/PL_LargeCliff_02b", -3600, -300, 170, spec.MASSIF_PLATEAU_CM - 2500),
    ("ASM_Z1_CrownNorth", "Forest/PL_LargeCliff_01c", -3200, 1050, -25, spec.MASSIF_PLATEAU_CM - 1000),
)
WALLS["V023"] = FRONT_02_V023 + CROWN_V023 + WALLS["V021"][len(FRONT_02):]
WALL_CRUST["V023"] = WALL_CRUST["V021"]
WALL_MATERIAL["V023"] = WALL_MATERIAL["V021"]
WALL_SKY["V023"] = WALL_SKY["V021"]
# 분지 남북 테두리와 고원 남북 끝 능선은 맨 V012 지각이다. 분지는 남북 폭이 약 75m라 저시점에서 테두리가 화면 절반을 넘고,
# 부감에서는 고원 남북 끝 능선(+50~57m)이 각진 기둥으로 산괴 양옆을 차지한다. V024는 지각을 바꾸지 않고 02 계열을
# 테두리 발치에 세우고(앞면이 분지를 보게 yaw 남 +90°·북 -90° 기준 ±6~10°), 능선 면에는 고원 높이에 묻어 세운다.
# 높이 기준 "abs"는 현지 반지름 기준 절대 높이 H(cm)다(분지 바닥 약 -4,000, 고원 약 +600).
# 북 테두리 동쪽 조각은 위 NorthLedge 동선 아래에 들도록 12m 묻는다. 동굴 입구(HallFromBasin, x≈2,500)는 비운다.
RIM_V024 = (
    ("ASM_Z1_RimSouthWest", "Forest/PL_LargeCliff_02d", 700, -5600, 96, -3900, 1.0, "abs"),
    ("ASM_Z1_RimSouth", "Forest/PL_LargeCliff_02e", 3200, -5700, 82, -3500, 1.0, "abs"),
    ("ASM_Z1_RimSouthEast", "Forest/PL_LargeCliff_02b", 5600, -5600, 98, -3400, 1.0, "abs"),
    ("ASM_Z1_CanyonWestBank", "Forest/PL_LargeCliff_01b", -3000, -4900, 60, -3000, 1.0, "abs"),
    ("ASM_Z1_RidgeSouth", "Forest/PL_LargeCliff_02c", -3800, -4600, 45, 500, 1.0, "abs"),
    ("ASM_Z1_RimNorth", "Forest/PL_LargeCliff_02c", 200, 3800, -84, -4000, 1.0, "abs"),
    ("ASM_Z1_RimNorthCrown", "Forest/PL_LargeCliff_01c", -200, 4700, -110, 900, 1.0, "abs"),
    ("ASM_Z1_RidgeNorth", "Forest/PL_LargeCliff_02e", -4300, 3500, -70, 600, 1.0, "abs"),
    ("ASM_Z1_RimNorthEast", "Forest/PL_LargeCliff_02b", 4500, 2300, -95, -5000, 1.0, "abs"),
)
WALLS["V024"] = WALLS["V023"] + RIM_V024
WALL_CRUST["V024"] = WALL_CRUST["V023"]
WALL_MATERIAL["V024"] = WALL_MATERIAL["V023"]
WALL_SKY["V024"] = WALL_SKY["V023"]
# V024 결과: 남 테두리(30m)보다 조립체(49m)가 높아 사면을 덮지 못하고 따로 선 기둥 3개로 읽혔다. 능선 기둥은 아래만 가려졌다.
# V025는 지각을 Z1MassifCover V006(능선 상한 + 테두리 면 물리기, spec RIM_FACES)으로 바꾸고, 조립체를 앞면 선에 맞춘다.
# 남 테두리: 4개를 약 19m 간격으로 겹치고, 암석 꼭대기가 테두리 꼭대기(약 -300)에 오도록 분지 바닥보다 10~15m 묻는다.
# 북 테두리: 벽(분지 바닥) + 꼭대기 북쪽 대지에 묻은 왕관 2단. NorthLedge 내리막(x 0~2,200)은 비운다.
# 깎은 능선 자리: 고원 남쪽 모서리·협곡 서안과 북쪽 단(+6→+20m)에 묻어 세워 하늘선을 만든다.
# 같은 계열은 3개씩 쓰고 yaw를 달리한다. V002 하구 조각 3개(Outlet*)는 남 테두리 조립체로 대체한다.
RIM_V025 = (
    ("ASM_Z1_RimSouthWest", "Forest/PL_LargeCliff_02d", 400, -5350, 96, -5000, 1.0, "abs"),
    ("ASM_Z1_RimSouth", "Forest/PL_LargeCliff_02b", 2300, -5300, 82, -5200, 1.0, "abs"),
    ("ASM_Z1_RimSouthEast", "Forest/PL_LargeCliff_02c", 4200, -5250, 100, -4700, 1.0, "abs"),
    ("ASM_Z1_RimSouthEnd", "Forest/PL_LargeCliff_02e", 5900, -5200, 84, -4900, 1.0, "abs"),
    ("ASM_Z1_CanyonWestBank", "Forest/PL_LargeCliff_02b", -3300, -4900, -30, -3500, 1.0, "abs"),
    ("ASM_Z1_PlateauSouthCrown", "Forest/PL_LargeCliff_01c", -4600, -4400, 30, 300, 1.0, "abs"),
    ("ASM_Z1_RimNorth", "Forest/PL_LargeCliff_02d", 200, 3550, -84, -4000, 1.0, "abs"),
    ("ASM_Z1_RimNorthCrown", "Forest/PL_LargeCliff_01b", -900, 4300, -130, 200, 1.0, "abs"),
    ("ASM_Z1_PlateauNorthStep", "Forest/PL_LargeCliff_02c", -4300, 3500, -75, -1000, 1.0, "abs"),
    ("ASM_Z1_RimNorthEast", "Forest/PL_LargeCliff_02e", 4500, 2300, -95, -5000, 1.0, "abs"),
)
WALLS["V025"] = WALLS["V023"] + RIM_V025
WALL_CRUST["V025"] = massif_crust("V006")
WALL_MATERIAL["V025"] = WALL_MATERIAL["V023"]
WALL_SKY["V025"] = WALL_SKY["V023"]
# 버전별로 V002 사본에서 더 빼는 개별 메시(라벨 접두어).
WALL_REPLACED_EXTRA = {"V025": ("ED_Z1_OutletWestBank", "ED_Z1_OutletEastBank", "ED_Z1_OutletShelf")}
# V025 사용자 판정: 지각 V006(능선 깎기·테두리 물리기)과 능선 자리 단독 조립체는 괜찮다. 남북 테두리에 같은 계열 조립체를
# 줄지어 세운 긴 절벽(RimSouth*·RimNorth*)은 확실히 어색하다(조립체는 가끔 변칙으로만 쓸 수단이다).
# V026은 테두리 조립체 7개를 빼고, ED CloseRange 둑 묶음을 테두리 지각 표면에 옮겨 심은 띠(Z1RimBand, band)를 넣는다.
RIM_ACCENTS_V025 = tuple(row for row in RIM_V025 if not row[0].startswith(("ASM_Z1_RimSouth", "ASM_Z1_RimNorth")))
WALLS["V026"] = WALLS["V023"] + RIM_ACCENTS_V025 + (
    ("CR_Z1_RimBand", "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1RimBand_V001/Z1RimBand_V001",
     0, 0, 0, 0, 1.0, "world"),)
WALL_CRUST["V026"] = WALL_CRUST["V025"]
WALL_MATERIAL["V026"] = WALL_MATERIAL["V025"]
WALL_SKY["V026"] = WALL_SKY["V025"]
WALL_REPLACED_EXTRA["V026"] = WALL_REPLACED_EXTRA["V025"]
# V027은 V026에서 지각을 남 테두리 3단 둑 V007로, 띠를 그 위에 얹은 Z1RimBand V002로 바꾼다.
WALLS["V027"] = WALLS["V026"][:-1] + (
    ("CR_Z1_RimBand", "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1RimBand_V002/Z1RimBand_V002",
     0, 0, 0, 0, 1.0, "world"),)
WALL_CRUST["V027"] = massif_crust("V007")
WALL_MATERIAL["V027"] = WALL_MATERIAL["V026"]
WALL_SKY["V027"] = WALL_SKY["V026"]
WALL_REPLACED_EXTRA["V027"] = WALL_REPLACED_EXTRA["V026"]
# V028은 V027에서 띠만 밀도를 올리고 큰 바위 층을 더한 Z1RimBand V003으로 바꾼다.
WALLS["V028"] = WALLS["V027"][:-1] + (
    ("CR_Z1_RimBand", "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1RimBand_V003/Z1RimBand_V003",
     0, 0, 0, 0, 1.0, "world"),)
for _table in (WALL_CRUST, WALL_MATERIAL, WALL_SKY, WALL_REPLACED_EXTRA):
    _table["V028"] = _table["V027"]
# V029는 V028에서 띠만 판 조각 법선 정렬·재안착 규칙을 더한 Z1RimBand V004로 바꾼다.
WALLS["V029"] = WALLS["V028"][:-1] + (
    ("CR_Z1_RimBand", "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1RimBand_V004/Z1RimBand_V004",
     0, 0, 0, 0, 1.0, "world"),)
for _table in (WALL_CRUST, WALL_MATERIAL, WALL_SKY, WALL_REPLACED_EXTRA):
    _table["V029"] = _table["V028"]
# V030(북 테두리 B안, 사용자 결정 2026-11-20): 지각을 북 둑 2단을 덧댄 V008로 바꾸고, 띠를 남 V005(= V004 남쪽) + 북 둑 V006으로
# 나눈다. 둑 맨 위 뒤 급사면(약 35m)은 서쪽 x -1,300에 01b 하나를 묻어 세워 덮는다(GX-027 단독 강조). 동굴 홀(y 4,200~, 천장
# -1,200)과 NorthLedge(x -300 동쪽)를 피해 서쪽에 둔다. 둑 발치에 묻히는 V002 NorthTalus는 뺀다.
NORTH_BANK_V030 = (
    ("CR_Z1_RimBand", "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1RimBand_V005/Z1RimBand_V005",
     0, 0, 0, 0, 1.0, "world"),
    ("CR_Z1_RimBandNorth", "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1RimBand_V006/Z1RimBand_V006",
     0, 0, 0, 0, 1.0, "world"),
    ("ASM_Z1_NorthRimCrown", "Forest/PL_LargeCliff_01b", -1300, 3950, -100, -2300, 1.0, "abs"),
)
WALLS["V030"] = WALLS["V029"][:-1] + NORTH_BANK_V030
WALL_CRUST["V030"] = massif_crust("V008")
WALL_MATERIAL["V030"] = WALL_MATERIAL["V029"]
WALL_SKY["V030"] = WALL_SKY["V029"]
WALL_REPLACED_EXTRA["V030"] = WALL_REPLACED_EXTRA["V029"] + ("ED_Z1_NorthTalus",)
# V030 결과: 01b는 기둥형이라 북쪽 단(+20m)에 대부분 묻혀 뾰족한 끝만 보였다. V031은 벽형 02c를 북 테두리 서쪽(x -1,600)에
# 앞면이 분지를 보게(yaw -86) 둑 2단 아래까지 묻어 세우고(꼭대기 ≈ 북쪽 단 높이), 북 띠를 V007로 바꾼다.
# 동쪽 끝(x 약 160)은 홀 북서 모서리·NorthLedge 시작점에 조금 걸칠 수 있다(시각 시험, 동굴 내부는 범위 밖).
WALLS["V031"] = WALLS["V030"][:-2] + (
    ("CR_Z1_RimBandNorth", "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1RimBand_V007/Z1RimBand_V007",
     0, 0, 0, 0, 1.0, "world"),
    ("ASM_Z1_NorthRimWall", "Forest/PL_LargeCliff_02c", -1600, 3800, -86, -2900, 1.0, "abs"),
)
for _table in (WALL_CRUST, WALL_MATERIAL, WALL_SKY, WALL_REPLACED_EXTRA):
    _table["V031"] = _table["V030"]
# V031 결과: 02c 동쪽 끝 돌출부가 동굴 지붕 가장자리 위에 떠 보였고, 띠가 걷히며 V007부터 있던 V012 바위 돌출(+30.8m)이 드러났다.
# V032는 지각을 V009(돌출 깎음)로, 북 띠를 V008로 바꾸고, 02c를 서쪽 x -1,950으로 옮긴다(동쪽 끝 x 약 -190).
WALLS["V032"] = WALLS["V031"][:-2] + (
    ("CR_Z1_RimBandNorth", "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1RimBand_V008/Z1RimBand_V008",
     0, 0, 0, 0, 1.0, "world"),
    ("ASM_Z1_NorthRimWall", "Forest/PL_LargeCliff_02c", -1950, 3800, -86, -2900, 1.0, "abs"),
)
for _table in (WALL_MATERIAL, WALL_SKY, WALL_REPLACED_EXTRA):
    _table["V032"] = _table["V031"]
WALL_CRUST["V032"] = massif_crust("V009")
# V033은 V032에서 북 띠만 V009(큰 바위 덩어리형·축소, 동굴 입구 옆 언덕 제외)로 바꾼다.
WALLS["V033"] = WALLS["V032"][:-2] + (
    ("CR_Z1_RimBandNorth", "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1RimBand_V009/Z1RimBand_V009",
     0, 0, 0, 0, 1.0, "world"),) + WALLS["V032"][-1:]
for _table in (WALL_CRUST, WALL_MATERIAL, WALL_SKY, WALL_REPLACED_EXTRA):
    _table["V033"] = _table["V032"]
WEST_COVER_LABEL = "SM_Octant_Ridge_00_V012_Z1WestCover_R30000"
WALL_VIEW_EYE = (4500, -400, 170)
WALL_VIEW_TARGET = (-1700, -300, 1800)


def asset_path(name):
    # PCGAssemblies 상대 이름 또는 /Game/ 전체 경로(LNPBake 구운 레벨)를 받는다.
    return name if name.startswith("/Game/") else ASSEMBLIES + name


def floor_point(x, y, offset, base="floor"):
    # floor는 서쪽 벽 발치 분지 바닥, local은 해당 위치의 V002 기반면 높이, abs는 offset을 절대 높이로 쓴다.
    ground = {"floor": lambda: spec.ground(-1250, y), "local": lambda: spec.west_cover_ground(x, y),
              "abs": lambda: 0.0}[base]()
    return unreal.Vector(*spec.ridge.point(x, y, ground + offset))


def wall(version):
    # 조립체 시험 레벨이 열린 상태에서 실행한다. V002 Level Instance 안의 배치를 읽어 서쪽 벽만 뺀 사본을 만든다.
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    saving = unreal.EditorLoadingAndSavingUtils
    assert package(world()) == LEVEL, package(world())
    assert not dirty(), dirty()
    wall_level = WALL_FOLDER.format(version)
    rows = WALLS[version]
    assert not unreal.EditorAssetLibrary.does_asset_exist(wall_level), wall_level
    for _, name, *_ in rows:
        assert unreal.EditorAssetLibrary.does_asset_exist(asset_path(name)), name

    kept, skipped, owners = [], [], set()
    v002_name = V002.rsplit("/", 1)[1]
    # EditorActorSubsystem은 Level Instance 안의 액터를 돌려주지 않아 월드 전체에서 찾는다.
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.StaticMeshActor):
        owner = actor.get_level().get_outermost().get_name()
        owners.add(owner)
        if v002_name not in owner:
            continue
        label = actor.get_actor_label()
        if label.startswith(WALL_REPLACED + WALL_REPLACED_EXTRA.get(version, ())):
            skipped.append(label)
            continue
        comp = actor.static_mesh_component
        t = actor.get_actor_transform()
        kept.append({"label": label, "mesh": package(comp.static_mesh), "loc": xyz(t.translation),
                     "rot": [t.rotation.rotator().pitch, t.rotation.rotator().yaw, t.rotation.rotator().roll],
                     "scale": xyz(t.scale3d),
                     "overrides": {i: package(m) for i, m in enumerate(comp.get_editor_property("override_materials"))
                                   if m}})
    assert kept and skipped, (len(kept), skipped, sorted(owners))
    if version in WALL_CRUST:
        cover, = [row for row in kept if row["label"] == WEST_COVER_LABEL]
        assert unreal.EditorAssetLibrary.does_asset_exist(WALL_CRUST[version]), WALL_CRUST[version]
        cover["mesh"] = WALL_CRUST[version]
        cover["label"] = WALL_CRUST[version].rsplit("/", 1)[1]
        if version in WALL_MATERIAL:
            assert unreal.EditorAssetLibrary.does_asset_exist(WALL_MATERIAL[version]), WALL_MATERIAL[version]
            cover["overrides"] = {0: WALL_MATERIAL[version]}

    source = light_actors()
    expected = sorted(tuple(xyz(a.get_actor_location())) for a in source)
    actors.set_selected_level_actors(source)
    unreal.SystemLibrary.execute_console_command(world(), "EDIT COPY")
    actors.select_nothing()
    assert not dirty(), dirty()

    assert saving.new_blank_map(False)
    unreal.SystemLibrary.execute_console_command(world(), "EDIT PASTE")
    pasted = light_actors()
    assert sorted(tuple(xyz(a.get_actor_location())) for a in pasted) == expected
    for actor in pasted:
        actor.set_folder_path("Lighting_TestMap03")
    actors.select_nothing()
    sky = None
    if version in WALL_SKY:
        sky = dict(WALL_SKY[version])
        cubemap = unreal.load_asset(sky["cubemap"])
        assert cubemap, sky["cubemap"]
        # 지정 큐브맵이라 위치는 영향이 없다. 조명 사본과 같은 폴더 규칙으로 따로 둔다.
        light = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        light.set_actor_label("SkyLight_UniformGray")
        light.set_folder_path("Lighting_Sky")
        comp = light.light_component
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        comp.set_editor_property("cubemap", cubemap)
        comp.set_editor_property("real_time_capture", False)
        comp.set_editor_property("lower_hemisphere_is_black", False)
        comp.set_editor_property("intensity", sky["intensity"])
        comp.recapture_sky()
        assert not comp.get_editor_property("lower_hemisphere_is_black")
        assert abs(comp.get_editor_property("intensity") - sky["intensity"]) < 1e-4

    for row in kept:
        rot = unreal.Rotator(pitch=row["rot"][0], yaw=row["rot"][1], roll=row["rot"][2])
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*row["loc"]), rot)
        actor.set_actor_label(row["label"])
        actor.set_actor_scale3d(unreal.Vector(*row["scale"]))
        comp = actor.static_mesh_component
        comp.set_static_mesh(unreal.load_asset(row["mesh"]))
        for slot, material in row["overrides"].items():
            comp.set_material(slot, unreal.load_asset(material))
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        actor.set_folder_path("V002_WithoutWestWall")

    placed = []
    for label, name, x, y, yaw, offset, *extra in rows:
        scale, base = (list(extra) + [1.0, "floor"][len(extra):])[:2]
        if base == "world":
            # 월드 좌표로 저장한 레벨(구면 드레이프)은 원점에 그대로 둔다.
            location, rotation = unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0)
        else:
            location, rotation = floor_point(x, y, offset, base), frame_rotation(x, y, yaw)
        actor = actors.spawn_actor_from_class(unreal.LevelInstance, location, rotation)
        actor.set_actor_label(label)
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        actor.set_world_asset(unreal.load_asset(asset_path(name)))
        actor.load_level_instance()
        assert package(actor.get_world_asset()) == asset_path(name), label
        actor.set_folder_path("EDAssemblies")
        placed.append({"label": label, "level": asset_path(name), "local_xy_cm": [x, y], "yaw_deg": yaw,
                       "height_offset_cm": offset, "height_base": base, "scale": scale, "location": xyz(location)})
    assert saving.save_map(world(), wall_level), wall_level

    eye, target = point(*WALL_VIEW_EYE), point(*WALL_VIEW_TARGET)
    view_rot = unreal.MathLibrary.make_rot_from_xz(target - eye, eye * (-1.0 / eye.length()))
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(eye, view_rot)
    report = {"level": wall_level, "kept_v002": kept, "replaced_v002": skipped, "assemblies": placed, "sky": sky,
              "view": {"location": xyz(eye), "rotation": [view_rot.pitch, view_rot.yaw, view_rot.roll]}}
    (OUT / ("AssemblyWallLevel_%s.json" % version)).write_text(json.dumps(report, indent=1), encoding="utf-8")
    unreal.log("Z1_ASSEMBLY_WALL_READY %d kept %d replaced" % (len(kept), len(skipped)))


DRAPE_LEVEL = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1MassifDrape_{0}/Z1MassifDrape_{0}"
# 넓은 평면 그룹만 다시 앉힌다. LargeAssembly(산괴 암석·둔덕)는 강체 Level Instance로 둔다.
# V001 결과: 바닥 그룹만으로는 덮개가 듬성했다. CloseRange 지면 덮개(SM_ForestGround 등)는 Ditch 그룹에 있다.
DRAPE_VERSIONS = {"V001": ("V002", ("Ground", "SmallAssembly", "Props")),
                  "V002": ("V002", ("Ground", "SmallAssembly", "Props", "Ditch"))}
# V002 결과: 고원 밖 남북 오름 사면(70~85°)의 지면 타일이 구면 Up에 맞춰 수평으로 놓여 층층이 선반처럼 보였다.
# V003부터 납작한 인스턴스(지면 타일·데칼)는 그 자리 지각 법선으로 눕히고, 경사가 상한을 넘는 자리에서는 뺀다.
# 납작함은 인스턴스 스케일을 곱한 메시 상자 높이 / 수평 최대 폭이다. 식생·바위는 그대로 현지 Up에 세운다.
DRAPE_VERSIONS["V003"] = DRAPE_VERSIONS["V002"]
# (지면 타일 최대 경사°, 납작함 상한[, 모든 인스턴스 최대 경사°]). 지각 재질은 30°까지 지면, 70°부터 절벽이다.
# V003 결과: 북 사면 선반·회색 데칼은 사라졌지만, 60° 넘는 남 사면 협곡에 세운 암석·식생(3,616개)이 층층이 붙고
# 꼭대기 판이 하늘로 튀어나왔다. 납작함에 인스턴스 회전을 반영해 보았으나 ED가 기울여 둔 둑 조각이 오히려 빠져 기각했다.
# V004는 V003에 더해 경사가 60°를 넘는 자리의 인스턴스를 종류와 관계없이 뺀다(ED 평지 배치에 대응하는 자리가 없음).
# V004 Lit 결과: 고원 동쪽 가장자리의 지면 타일이 고원 밖 허공으로 튀어나와 큰 그림자를 드리웠다.
# V005는 넷째 값(cm)을 더해, 납작한 타일의 상자 바닥 네 모서리 아래 지각이 중심 접평면보다 그만큼 넘게 꺼지면 뺀다.
DRAPE_SLOPE = {"V003": (45.0, 0.35), "V004": (45.0, 0.35, 60.0), "V005": (45.0, 0.35, 60.0, 100.0)}
DRAPE_VERSIONS["V004"] = DRAPE_VERSIONS["V002"]
DRAPE_VERSIONS["V005"] = DRAPE_VERSIONS["V002"]
# V004 Lit 수직 부감에서 진흙 웅덩이(SM_WaterPlane_01 × MI_MudPuddle_01)가 어두운 판으로 보였다. 원인은 반사가 아니라
# 재질 컴파일 실패다. M_MudPuddle_01_PCG는 Substrate Single Layer Water BSDF만 쓰고 `Single Layer Water Material`
# 출력 노드가 없다. r.RayTracing=True면 레이 트레이싱 히트 셰이더가 GetSingleLayerWaterMaterialOutput0~2를 찾다가 실패해
# 재질 전체가 기본 재질(회색 체커)로 대체된다. ED 원본도 같은 오류라 밝은 햇빛 아래 체커가 자갈처럼 보였을 뿐이다.
# V006은 V004 규칙 그대로, 웅덩이 재질만 출력 노드를 더한 프로젝트 사본(PUDDLE_FOLDER)으로 바꾼다.
DRAPE_SLOPE["V006"] = DRAPE_SLOPE["V004"]
DRAPE_VERSIONS["V006"] = DRAPE_VERSIONS["V004"]
PUDDLE_SOURCE = "/Game/PCG/Assets/Materials/"
PUDDLE_FOLDER = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1Puddle_V001/"
DRAPE_MATERIAL_SWAP = {"V006": {PUDDLE_SOURCE + "MI_MudPuddle_01": PUDDLE_FOLDER + "MI_MudPuddle_01"}}
# 지각 법선 유한 차분 격자(cm). 지각 높이 함수가 호출당 약 1ms라 격자 점을 캐시한다.
DRAPE_NORMAL_GRID = 100.0
DRAPE_COPY = ("cast_shadow", "instance_start_cull_distance", "instance_end_cull_distance",
              "world_position_offset_disable_distance", "evaluate_world_position_offset", "visible_in_ray_tracing",
              "affect_distance_field_lighting", "affect_dynamic_indirect_lighting", "receives_decals",
              "translucency_sort_priority", "cast_contact_shadow", "cast_far_shadow")


def drape_frame():
    # V008의 CR Level Instance와 같은 원점·회전(고원 높이, 현지 Up 기준 yaw 0)이다.
    x, y = spec.MASSIF_CENTER
    return floor_point(x, y, spec.MASSIF_PLATEAU_CM), frame_rotation(x, y, 0)


def tilt_between(rotation, up_from, up_to):
    # up_from을 up_to로 돌리는 최소 회전을 rotation 뒤에 적용한다.
    axis = up_from.cross(up_to)
    if axis.length() <= 1e-9:
        return rotation
    angle = unreal.MathLibrary.radians_to_degrees(unreal.MathLibrary.acos(max(-1.0, min(1.0, up_from.dot(up_to)))))
    tilt = unreal.MathLibrary.rotator_from_axis_and_angle(axis * (1.0 / axis.length()), angle)
    return unreal.MathLibrary.compose_rotators(rotation, tilt)


_crust_points = {}


def crust_point(ix, iy, crust):
    key = (ix, iy, crust)
    if key not in _crust_points:
        x, y = ix * DRAPE_NORMAL_GRID, iy * DRAPE_NORMAL_GRID
        _crust_points[key] = unreal.Vector(*spec.ridge.point(x, y, spec.massif_cover_ground(x, y, crust)))
    return _crust_points[key]


def crust_normal(x, y, crust):
    # 가장 가까운 격자 점에서 중심 차분으로 지각 법선을 구하고 현지 Up 쪽으로 향하게 한다.
    ix, iy = round(x / DRAPE_NORMAL_GRID), round(y / DRAPE_NORMAL_GRID)
    tx = crust_point(ix + 1, iy, crust) - crust_point(ix - 1, iy, crust)
    ty = crust_point(ix, iy + 1, crust) - crust_point(ix, iy - 1, crust)
    n = tx.cross(ty)
    n = n * (1.0 / n.length())
    center = crust_point(ix, iy, crust)
    return n * -1.0 if n.dot(center) > 0 else n


def drape_flat(size, scale, rule):
    # 인스턴스 스케일을 곱한 메시 상자가 납작하면 지면 타일로 본다. size가 None이면 데칼이다.
    if size is None:
        return True
    sx, sy, sz = abs(size.x * scale.x), abs(size.y * scale.y), abs(size.z * scale.z)
    return sz < rule[1] * max(sx, sy)


def drape_overhang(transform, box, center, normal, crust):
    # 타일 상자 바닥 네 모서리 아래 지각 점이 중심 지각 점의 접평면에서 가장 많이 꺼진 깊이(cm, 양수가 꺼짐).
    deepest = 0.0
    for bx in (box.min.x, box.max.x):
        for by in (box.min.y, box.max.y):
            w = unreal.MathLibrary.transform_location(transform, unreal.Vector(bx, by, box.min.z))
            cx, cy, _ = spec.ridge.local((w.x, w.y, w.z))
            point = crust_point(round(cx / DRAPE_NORMAL_GRID), round(cy / DRAPE_NORMAL_GRID), crust)
            deepest = max(deepest, -(point - center).dot(normal))
    return deepest


def drape_transform(rel, origin, frame, crust, rule=None, size=None, stats=None, box=None):
    # 평면 배치 위치를 옥탄트 로컬 x, y로 되돌린 뒤 그 자리 지각 높이 + ED 지면 위 높이(dz)로 다시 앉힌다.
    # rule이 있으면 납작한 인스턴스를 지각 법선으로 눕히고, 경사가 상한을 넘으면 None(제외)을 돌려준다.
    planar = unreal.MathLibrary.compose_transforms(rel, unreal.Transform(origin, frame, unreal.Vector(1, 1, 1)))
    p = planar.translation
    x, y, _ = spec.ridge.local((p.x, p.y, p.z))
    ground = spec.massif_cover_ground(x, y, crust)
    target = unreal.Vector(*spec.ridge.point(x, y, ground + rel.translation.z))
    up_from, up_to = origin * (-1.0 / origin.length()), target * (-1.0 / target.length())
    rotation = tilt_between(planar.rotation.rotator(), up_from, up_to)
    if rule:
        normal = crust_normal(x, y, crust)
        slope = unreal.MathLibrary.radians_to_degrees(unreal.MathLibrary.acos(max(-1.0, min(1.0, up_to.dot(normal)))))
        flat = drape_flat(size, planar.scale3d, rule)
        band = "%s_%02d" % ("flat" if flat else "other", min(int(slope // 15) * 15, 75))
        stats[band] = stats.get(band, 0) + 1
        if len(rule) > 2 and slope > rule[2]:
            return None
        if flat:
            if slope > rule[0]:
                return None
            rotation = tilt_between(rotation, up_to, normal)
            if len(rule) > 3 and box is not None:
                center = unreal.Vector(*spec.ridge.point(x, y, ground))
                draped = unreal.Transform(target, rotation, planar.scale3d)
                if drape_overhang(draped, box, center, normal, crust) > rule[3]:
                    stats["flat_overhang"] = stats.get("flat_overhang", 0) + 1
                    return None
    return unreal.Transform(target, rotation, planar.scale3d)


def drape(version, mode=""):
    # CloseRange 평면 그룹을 인스턴스마다 Z1MassifCover 구면 지각 위에 다시 앉힌 레벨을 만든다(월드 좌표 저장).
    # mode "dry"는 레벨을 만들지 않고 경사 대역·제외 집계만 MassifDrape_<버전>_dry.json에 쓴다.
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    saving = unreal.EditorLoadingAndSavingUtils
    level = DRAPE_LEVEL.format(version)
    crust, drape_groups = DRAPE_VERSIONS[version]
    assert not dirty(), dirty()
    assert mode == "dry" or not unreal.EditorAssetLibrary.does_asset_exist(level), level
    rule = DRAPE_SLOPE.get(version)
    swap, swapped = DRAPE_MATERIAL_SWAP.get(version, {}), {}
    for target in swap.values():
        assert unreal.EditorAssetLibrary.does_asset_exist(target), target
    origin, frame = drape_frame()
    groups, source, slopes, dropped, steep = {}, {}, {}, {}, {}
    for group in drape_groups:
        assert levels.load_level(CR + group), group
        rows, decals, counts, stats, drops = [], [], {}, {}, {}
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor):
            for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
                k = comp.get_editor_property("num_custom_data_floats")
                rels = [comp.get_instance_transform(i, False) for i in range(comp.get_instance_count())]
                props = {}
                for prop in DRAPE_COPY:
                    try:
                        props[prop] = comp.get_editor_property(prop)
                    except Exception:
                        pass
                box = comp.static_mesh.get_bounding_box()
                size = box.max - box.min
                custom = list(comp.get_editor_property("per_instance_sm_custom_data")) if k else []
                kept, kept_custom, bands = [], [], {}
                for i, t in enumerate(rels):
                    draped = drape_transform(t, origin, frame, crust, rule, size, bands, box)
                    if draped is None:
                        continue
                    kept.append(draped)
                    kept_custom += custom[i * k:(i + 1) * k]
                key = package(comp.static_mesh)
                for band, n in bands.items():
                    stats[band] = stats.get(band, 0) + n
                    # 60° 넘는 사면에 세워 둔 인스턴스는 메시별로 남겨 다음 규칙의 근거로 쓴다.
                    if band in ("other_60", "other_75"):
                        row = steep.setdefault(key.rsplit("/", 1)[1], [0, round(size.z / max(size.x, size.y), 2)])
                        row[0] += n
                if len(kept) < len(rels):
                    drops[key] = drops.get(key, 0) + len(rels) - len(kept)
                if not kept:
                    continue
                rows.append({"hism": isinstance(comp, unreal.HierarchicalInstancedStaticMeshComponent),
                             "mesh": comp.static_mesh, "overrides": list(comp.get_editor_property("override_materials")),
                             "profile": comp.get_collision_profile_name(), "collision": comp.get_collision_enabled(),
                             "props": props, "custom_count": k, "custom": kept_custom, "transforms": kept})
                counts[key] = counts.get(key, 0) + len(kept)
            if isinstance(actor, unreal.DecalActor):
                key = "decal:" + package(actor.decal.get_decal_material())
                draped = drape_transform(actor.get_actor_transform(), origin, frame, crust, rule, None, stats)
                if draped is None:
                    drops[key] = drops.get(key, 0) + 1
                    continue
                decals.append({"material": actor.decal.get_decal_material(), "size": actor.decal.get_editor_property("decal_size"),
                               "sort_order": actor.decal.get_editor_property("sort_order"), "transform": draped})
                counts[key] = counts.get(key, 0) + 1
        groups[group], source[group], slopes[group], dropped[group] = (rows, decals), counts, stats, drops
        assert not dirty(), dirty()

    summary = {"slope_rule": rule, "slope_bands": slopes,
               "dropped": {g: dict(sorted(d.items(), key=lambda kv: -kv[1])) for g, d in dropped.items()},
               "steep_upright": dict(sorted(steep.items(), key=lambda kv: -kv[1][0]))}
    if mode == "dry":
        (OUT / ("MassifDrape_%s_dry.json" % version)).write_text(json.dumps(summary, indent=1), encoding="utf-8")
        unreal.log("Z1_MASSIF_DRAPE_DRY " + version)
        return
    assert saving.new_blank_map(False)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary

    def add(parent, cls):
        handle, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls))
        assert not str(fail), fail
        return handle, lib.get_object(lib.get_data(handle))

    built = {}
    for group, (rows, decals) in groups.items():
        counts = {}
        if rows:
            actor = actors.spawn_actor_from_class(unreal.Actor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
            actor.set_actor_label("Drape_" + group)
            root_handle, root = add(sds.k2_gather_subobject_data_for_instance(actor)[0], unreal.SceneComponent)
            root.set_mobility(unreal.ComponentMobility.STATIC)
            for row in rows:
                cls = unreal.HierarchicalInstancedStaticMeshComponent if row["hism"] else unreal.InstancedStaticMeshComponent
                _, comp = add(root_handle, cls)
                comp.set_mobility(unreal.ComponentMobility.STATIC)
                comp.set_static_mesh(row["mesh"])
                for i, material in enumerate(row["overrides"]):
                    if material:
                        comp.set_material(i, material)
                # 메시 기본 재질이든 덮어쓴 재질이든 실제로 쓰는 재질이 교체 표에 있으면 사본으로 바꾼다.
                for i in range(comp.get_num_materials()):
                    used = comp.get_material(i)
                    target = swap.get(package(used)) if used else None
                    if target:
                        comp.set_material(i, unreal.load_asset(target))
                        swapped[target] = swapped.get(target, 0) + len(row["transforms"])
                for prop, value in row["props"].items():
                    comp.set_editor_property(prop, value)
                comp.set_collision_profile_name(row["profile"])
                comp.set_collision_enabled(row["collision"])
                comp.add_instances(row["transforms"], False, True)
                if row["custom_count"]:
                    k = row["custom_count"]
                    comp.set_num_custom_data_floats(k)
                    for i in range(len(row["transforms"])):
                        for j in range(k):
                            comp.set_custom_data_value(i, j, row["custom"][i * k + j], False)
                assert comp.get_instance_count() == len(row["transforms"])
                key = package(row["mesh"])
                counts[key] = counts.get(key, 0) + len(row["transforms"])
            actor.set_folder_path("Drape_" + group)
        for row in decals:
            t = row["transform"]
            decal = actors.spawn_actor_from_class(unreal.DecalActor, t.translation, t.rotation.rotator())
            decal.set_actor_scale3d(t.scale3d)
            decal.decal.set_decal_material(row["material"])
            decal.decal.set_editor_property("decal_size", row["size"])
            decal.decal.set_editor_property("sort_order", row["sort_order"])
            decal.set_folder_path("Drape_" + group)
            key = "decal:" + package(row["material"])
            counts[key] = counts.get(key, 0) + 1
        built[group] = {"match": counts == source[group], "instances": sum(counts.values())}
    assert all(b["match"] for b in built.values()), built
    assert saving.save_map(world(), level), level
    report = {"level": level, "crust": crust, "origin": xyz(origin), "frame": [frame.pitch, frame.yaw, frame.roll],
              "groups": built, "material_swap": swapped, **summary}
    assert set(swapped) == set(swap.values()), swapped
    (OUT / ("MassifDrape_%s.json" % version)).write_text(json.dumps(report, indent=1), encoding="utf-8")
    unreal.log("Z1_MASSIF_DRAPE_READY " + json.dumps(built))


# 긴 테두리 사면 띠(V025 판정: 같은 계열 조립체를 줄지어 세운 긴 절벽은 어색하다).
# ED CloseRange의 긴 둑(Ditch, 둘레 약 270m)은 큰 조립체가 아니라 높이 1~10m 암석 조각(MassiveSandstoneCliff_04 313개,
# HugeSandstoneCliff_07 245개, HugeSandstoneCliff_04 187개 등)과 둑 메시·식생을 하상을 따라 촘촘히 깐 것이다.
# band는 CR_Ditch의 둑 경사면에서 반경 patch 안의 인스턴스 묶음(암석 + 식생)을 떼어, 테두리 지각 표면의 표본점마다
# 표면 좌표계(선 접선 T, 표면 법선 n, 오르막 B = n × T)째로 옮겨 심는다. 원본 좌표계는 하상 중심선 접선과 ED 섬 지지면
# (CloseRangeSupport V002 + 50cm) 법선이고, 오르막은 하상에서 멀어지는 쪽, 목표 오르막은 분지에서 멀어지는 쪽이다.
# 암석·지면 조각은 표면을 따라 기울고, 식생(3D_Plants)은 같은 자리에서 현지 Up으로 다시 세운다.
BAND_LEVEL = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1RimBand_{0}/Z1RimBand_{0}"
BAND_CRUST = {"V006": spec.massif_rim_ground, "V007": spec.massif_terrace_ground, "V008": spec.massif_bank_ground,
              "V009": spec.massif_spur_ground}
# lines: spec.RIM_FACES 이름, band: 앞면 선 기준 뒤로 들어간 거리 범위(cm), spacing: 표면 호 길이 표본 간격(cm),
# patch: 원본 묶음 반경(cm), min_slope: 표본점 최소 경사(°), anchor_slope: 원본 묶음 중심의 최소 지지면 경사(°).
BAND_VERSIONS = {
    "V001": {"crust": "V006", "source": "Ditch", "lines": ("South", "North"), "band": (-300, 2600), "spacing": 450,
             "patch": 300, "min_slope": 25.0, "anchor_slope": 15.0, "seed": 27001},
    # V001 결과: 원본 묶음 경사 중앙값 38°, 목표 면 중앙값 73°라 판·돌무더기가 벽에 붙은 것처럼 보였다.
    # V002는 남 테두리를 약 50° 둑 3단으로 바꾼 지각 V007에 얹고, 원본 묶음도 지지면 경사 35° 이상만 쓴다.
    "V002": {"crust": "V007", "source": "Ditch", "lines": ("South", "North"), "band": (-1300, 3200), "spacing": 450,
             "patch": 300, "min_slope": 25.0, "anchor_slope": 35.0, "seed": 27002},
}
# V002 사용자 판정(2026-11-20): 둑 3단 + 띠 방향은 가능성이 있다. 큰 바위를 섞고 밀도를 더 올린다.
# V003은 묶음 표본 간격을 320cm로 줄이고, 큰 바위 층(boulders)을 따로 깐다. 큰 바위는 원본 둑의 큰 암석 인스턴스 하나를
# 같은 표면 좌표계로 옮기되 scale배로 키우고, 키운 높이의 sink 비율만큼 표면 법선 반대로 묻는다.
BAND_VERSIONS["V003"] = dict(BAND_VERSIONS["V002"], spacing=320, seed=27003, boulders={
    "meshes": ("SM_MassiveSandstoneCliff_0", "SM_HugeSandstoneCliff_04", "SM_SandstoneCliff_0", "SM_SandstoneBoulder_05",
               "SM_MossyForestBoulder_04"),
    "spacing": 1100, "scale": (1.6, 2.4), "sink": 0.25})
# V003 사용자 판정: 밀도는 충분하다. 얇은 판 조각이 벽에 동전처럼 꽂히고, 한쪽 면만 있는 조각이 세워져 꽂힌 것이 어색하다.
# V004는 V003과 같은 표본에서 seat 규칙(band_seater)으로 판을 지각 법선에 눕히고 덩어리·식생을 실제 지각 높이에 다시 앉힌다.
BAND_VERSIONS["V004"] = dict(BAND_VERSIONS["V003"], seat=True, plate_ratio=0.4, plate_tilt=6.0, plate_embed=0.3,
                             seat_grid=100.0)
# 북 테두리 B안(지각 V008): 북 앞면 띠(V006 급사면 위, 돌무더기)는 빼고 덧댄 둑 2단에 새 띠를 깐다.
# V005는 V004와 표본·묶음·난수 흐름을 그대로 두고 남 테두리(place_lines)만 놓는다 → 남 띠는 V004와 같다.
# V006은 지각 V008의 북 둑 선(spec.RIM_BANK_LINES)에 V004 규칙으로 깐 띠다. 둑 맨 위 뒤 급사면(65° 초과)은 표본에서 뺀다.
BAND_VERSIONS["V005"] = dict(BAND_VERSIONS["V004"], place_lines=("South",))
BAND_VERSIONS["V006"] = dict(BAND_VERSIONS["V004"], crust="V008", lines=("NorthBank",), band=(-2400, 100), max_slope=65.0,
                             seed=27006)
# V006 결과: 둑 맨 위(d 0 근처) 묶음이 반경 3m만큼 뒤 급사면으로 번져 돌 조각이 흩어졌다. V007은 표본을 둑 맨 위 3m 앞까지로 줄인다.
BAND_VERSIONS["V007"] = dict(BAND_VERSIONS["V006"], band=(-2400, -300), seed=27007)
# V008은 V007과 같고 지각만 V009(돌출 깎음)다(둑 구역 높이는 같으므로 표본도 같다).
BAND_VERSIONS["V008"] = dict(BAND_VERSIONS["V007"], crust="V009")
# V008 결과: 북 둑은 한 단 12m라 큰 바위 판(MassiveSandstoneCliff 04 x2.36)을 눕히면 둑 밖 허공으로 아치처럼 삐져나왔고,
# 동굴 입구 옆 언덕(x 약 1,400, 47~63°) 표본이 돌무더기가 됐다. V009는 북 띠 큰 바위를 덩어리형·1.4~2.0배로 줄이고
# x 1,100 동쪽 표본을 뺀다(x_range). 남은 표본 위치는 V008과 같지만 표본마다 고르는 묶음은 난수 순서가 밀려 달라진다.
BAND_VERSIONS["V009"] = dict(BAND_VERSIONS["V008"], x_range=(-2400, 1100), boulders={
    "meshes": ("SM_HugeSandstoneCliff_04", "SM_SandstoneBoulder_05", "SM_MossyForestBoulder_04"),
    "spacing": 1100, "scale": (1.4, 2.0), "sink": 0.25})
# 묶음 중심으로 쓰는 원본 암석·둑 메시(이름 접두어).
BAND_ANCHORS = ("SM_MassiveSandstoneCliff_0", "SM_HugeSandstoneCliff_04", "SM_SandstoneCliff_0", "SM_Embankment_01",
                "SM_SandstoneBoulder_0")
BAND_SKIP = ("SM_WaterPlane",)


def band_ground_source(cx, cy):
    # ED 섬 지지면 높이(CR 원점 기준 cm). 높이장은 '지지면 - 50cm'를 담는다.
    return spec.closerange_support_offset(cx, cy, "V002") + 50.0


def band_frame(origin, tangent, normal, uphill):
    # T를 법선에 수직으로 만들고 B = n × T가 오르막을 향하게 한다. X=T, Y=B, Z=n 회전을 돌려준다.
    t = tangent - normal * normal.dot(tangent)
    t = t * (1.0 / t.length())
    b = normal.cross(t)
    if b.dot(uphill) < 0:
        t, b = t * -1.0, b * -1.0
    return unreal.Transform(origin, unreal.MathLibrary.make_rot_from_xy(t, b), unreal.Vector(1, 1, 1))


def band_targets(cfg, ground_at, rng):
    # 앞면 선을 따라 열을 두고, 열마다 뒤쪽으로 지각 단면을 걸어 표면 호 길이 spacing마다 표본점을 낸다.
    toe, back = cfg["band"]
    spacing = cfg["spacing"]
    targets = []
    for name in cfg["lines"]:
        line = spec.rim_line(name)
        for (ax, ay), (bx, by) in zip(line, line[1:]):
            dx, dy = bx - ax, by - ay
            length = (dx * dx + dy * dy) ** 0.5
            tx, ty = dx / length, dy / length
            nx, ny = -ty, tx  # 왼쪽 법선 = 분지 쪽
            s = rng.uniform(0, spacing)
            while s < length:
                cx, cy = ax + tx * s, ay + ty * s
                prev, arc, next_at = None, 0.0, rng.uniform(0, spacing)
                d = toe
                while d <= back:
                    x, y = cx - nx * d, cy - ny * d
                    h = ground_at(x, y)
                    if prev is not None:
                        step = ((d - prev[0]) ** 2 + (h - prev[1]) ** 2) ** 0.5
                        arc += step
                        slope = math.degrees(math.atan2(abs(h - prev[1]), d - prev[0]))
                        if arc >= next_at:
                            next_at = arc + spacing * rng.uniform(0.75, 1.25)
                            x_lo, x_hi = cfg.get("x_range", (-1e9, 1e9))
                            if cfg["min_slope"] <= slope <= cfg.get("max_slope", 90.0) and x_lo <= x <= x_hi:
                                targets.append({"line": name, "x": x, "y": y, "h": h, "t": (tx, ty), "back": (-nx, -ny),
                                                "slope": round(slope, 1), "d": d})
                    prev = (d, h)
                    d += 25.0
                s += spacing * rng.uniform(0.75, 1.25)
    return targets


def band_target_frame(target, ground_at):
    x, y, h = target["x"], target["y"], target["h"]
    point = lambda px, py: unreal.Vector(*spec.ridge.point(px, py, ground_at(px, py)))
    tx, ty = target["t"]
    bx, by = target["back"]
    origin = unreal.Vector(*spec.ridge.point(x, y, h))
    along = point(x + tx * 50, y + ty * 50) - point(x - tx * 50, y - ty * 50)
    uphill = point(x + bx * 50, y + by * 50) - point(x - bx * 50, y - by * 50)
    normal = along.cross(uphill)
    normal = normal * (1.0 / normal.length())
    up = origin * (-1.0 / origin.length())
    if normal.dot(up) < 0:
        normal = normal * -1.0
    return band_frame(origin, along, normal, uphill), up


def band_source_normal(cx, cy):
    g = band_ground_source
    gx = (g(cx + 50, cy) - g(cx - 50, cy)) / 100.0
    gy = (g(cx, cy + 50) - g(cx, cy - 50)) / 100.0
    normal = unreal.Vector(-gx, -gy, 1.0)
    return normal * (1.0 / normal.length())


def band_source_frame(cx, cy):
    # 하상 중심선 접선과 지지면 법선. 오르막은 하상에서 멀어지는 쪽이다.
    g = band_ground_source
    normal = band_source_normal(cx, cy)
    best, tangent, away = float("inf"), None, None
    pts = spec.CLOSERANGE_CHANNEL_CR
    for (ax, ay), (bx, by) in zip(pts, pts[1:] + pts[:1]):
        dx, dy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((cx - ax) * dx + (cy - ay) * dy) / (dx * dx + dy * dy or 1.0)))
        px, py = ax + t * dx, ay + t * dy
        dist = ((cx - px) ** 2 + (cy - py) ** 2) ** 0.5
        if dist < best:
            best, tangent, away = dist, unreal.Vector(dx, dy, 0), unreal.Vector(cx - px, cy - py, 0)
    slope = math.degrees(math.acos(max(-1.0, min(1.0, normal.z))))
    return band_frame(unreal.Vector(cx, cy, g(cx, cy)), tangent, normal, away), slope


def band_seater(cfg, ground_at, comps, insts, rng):
    # 옮긴 인스턴스를 목표 지각에 다시 앉히는 함수를 돌려준다(V004~).
    # - 판(가장 얇은 축 / 가장 긴 축 < plate_ratio, 지면 타일·둑 판·얇은 사암 판·한쪽 면 스캔 표면): 얇은 축을 그 자리 지각
    #   법선에 맞춰 눕힌다. 원본에서 그 축이 지면 법선과 하상 쪽을 향하던 면을 바깥으로 한다. 회전 변화는 법선 둘레 자유 회전과
    #   plate_tilt 이하 기울기만 주고, 두께의 plate_embed만큼 묻는다.
    # - 덩어리 바위·식생: 옮긴 수평 위치의 지각 높이 + 원본 지면 위 높이(dz)로 다시 앉힌다. 식생은 현지 Up으로 세운다.
    step = cfg.get("seat_grid", 100.0)
    heights, info = {}, {}
    axes = (unreal.MathLibrary.get_forward_vector, unreal.MathLibrary.get_right_vector, unreal.MathLibrary.get_up_vector)

    def h_at(i, j):
        if (i, j) not in heights:
            heights[(i, j)] = ground_at(i * step, j * step)
        return heights[(i, j)]

    def height(x, y):
        fx, fy = x / step, y / step
        i, j = math.floor(fx), math.floor(fy)
        u, v = fx - i, fy - j
        return ((h_at(i, j) * (1 - u) + h_at(i + 1, j) * u) * (1 - v)
                + (h_at(i, j + 1) * (1 - u) + h_at(i + 1, j + 1) * u) * v)

    def point(a, b):
        return unreal.Vector(*spec.ridge.point(a * step, b * step, h_at(a, b)))

    def normal(x, y):
        i, j = round(x / step), round(y / step)
        n = (point(i + 1, j) - point(i - 1, j)).cross(point(i, j + 1) - point(i, j - 1))
        n = n * (1.0 / n.length())
        return n * -1.0 if n.dot(point(i, j)) > 0 else n

    def sizes(box, sc):
        return [abs((box.max.x - box.min.x) * sc.x), abs((box.max.y - box.min.y) * sc.y),
                abs((box.max.z - box.min.z) * sc.z)]

    def source(idx):
        # 원본 지면 위 높이와, 판이면 (얇은 축 번호, 바깥 부호).
        if idx not in info:
            ci, t, x, y, _ = insts[idx]
            dz = t.translation.z - band_ground_source(x, y)
            size = sizes(comps[ci]["box"], t.scale3d)
            plate = None
            if not comps[ci]["plant"] and min(size) < cfg["plate_ratio"] * max(size):
                a = size.index(min(size))
                axis = axes[a](t.rotation.rotator())
                frame, _ = band_source_frame(x, y)
                # 원본 좌표계 Y(B)는 오르막(하상에서 멀어지는 쪽)이므로 하상 쪽은 -B다.
                toward = unreal.MathLibrary.get_right_vector(frame.rotation.rotator()) * -1.0
                n = band_source_normal(x, y)
                plate = (a, 1.0 if axis.dot(n + toward * 0.5) >= 0 else -1.0)
            info[idx] = (dz, plate)
        return info[idx]

    def seat(idx, moved, k=1.0):
        ci = insts[idx][0]
        dz, plate = source(idx)
        x, y, _ = spec.ridge.local((moved.translation.x, moved.translation.y, moved.translation.z))
        h = height(x, y)
        surface = unreal.Vector(*spec.ridge.point(x, y, h))
        up = surface * (-1.0 / surface.length())
        if plate is None:
            rotation = moved.rotation.rotator()
            if comps[ci]["plant"]:
                rotation = tilt_between(rotation, unreal.MathLibrary.get_up_vector(rotation), up)
            return unreal.Transform(unreal.Vector(*spec.ridge.point(x, y, h + dz * k)), rotation, moved.scale3d)
        a, sign = plate
        n = normal(x, y)
        ref = n.cross(up)
        if ref.length() < 1e-3:
            ref = n.cross(unreal.Vector(1, 0, 0))
        ref = ref * (1.0 / ref.length())
        tilt = cfg["plate_tilt"]
        n_j = unreal.MathLibrary.rotate_angle_axis(n, rng.uniform(-tilt, tilt), ref)
        u = unreal.MathLibrary.rotate_angle_axis(ref, rng.uniform(0, 360), n_j)
        face = n_j * sign
        if a == 2:
            rotation = unreal.MathLibrary.make_rot_from_xz(u, face)
        elif a == 1:
            rotation = unreal.MathLibrary.make_rot_from_xy(u, face)
        else:
            rotation = unreal.MathLibrary.make_rot_from_xz(face, u)
        box, sc = comps[ci]["box"], moved.scale3d
        offset = unreal.MathLibrary.transform_location(unreal.Transform(unreal.Vector(0, 0, 0), rotation, sc),
                                                       (box.min + box.max) * 0.5)
        thickness = sizes(box, sc)[a]
        return unreal.Transform(surface - n * (cfg["plate_embed"] * thickness) - offset, rotation, sc)

    seat.is_plate = lambda idx: source(idx)[1] is not None
    return seat


def band(version):
    # CR 둑 묶음을 테두리 지각 표면에 옮겨 심은 Z1RimBand_<버전>(월드 좌표) 레벨을 만든다.
    import random
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    saving = unreal.EditorLoadingAndSavingUtils
    cfg = BAND_VERSIONS[version]
    level = BAND_LEVEL.format(version)
    ground_at = BAND_CRUST[cfg["crust"]]
    assert not dirty(), dirty()
    assert not unreal.EditorAssetLibrary.does_asset_exist(level), level
    rng = random.Random(cfg["seed"])

    # 원본: 컴포넌트 설정과 인스턴스(월드 = CR 원점 기준 좌표)를 읽는다.
    assert levels.load_level(CR + cfg["source"]), cfg["source"]
    comps, insts = [], []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor):
        for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
            path = package(comp.static_mesh)
            if any(s in path for s in BAND_SKIP):
                continue
            k = comp.get_editor_property("num_custom_data_floats")
            custom = list(comp.get_editor_property("per_instance_sm_custom_data")) if k else []
            props = {}
            for prop in DRAPE_COPY:
                try:
                    props[prop] = comp.get_editor_property(prop)
                except Exception:
                    pass
            ci = len(comps)
            comps.append({"mesh": comp.static_mesh, "path": path, "overrides": list(comp.get_editor_property("override_materials")),
                          "hism": isinstance(comp, unreal.HierarchicalInstancedStaticMeshComponent), "props": props,
                          "custom_count": k, "plant": "/3D_Plants/" in path,
                          "height": comp.static_mesh.get_bounding_box().max.z - comp.static_mesh.get_bounding_box().min.z,
                          "box": comp.static_mesh.get_bounding_box()})
            for i in range(comp.get_instance_count()):
                t = comp.get_instance_transform(i, True)
                insts.append((ci, t, t.translation.x, t.translation.y, custom[i * k:(i + 1) * k]))
    assert not dirty(), dirty()

    # 묶음 중심: 지지면 경사가 anchor_slope 이상인 자리의 암석·둑 인스턴스.
    anchors = []
    for ci, t, x, y, _ in insts:
        name = comps[ci]["path"].rsplit("/", 1)[1]
        if name.startswith(BAND_ANCHORS):
            frame, slope = band_source_frame(x, y)
            if slope >= cfg["anchor_slope"]:
                anchors.append((x, y, frame, slope))
    assert anchors, "no anchors"
    radius = cfg["patch"]
    cell = {}
    for idx, (_, _, x, y, _) in enumerate(insts):
        cell.setdefault((int(x // radius), int(y // radius)), []).append(idx)

    def patch(x, y):
        ix, iy = int(x // radius), int(y // radius)
        for gx in (ix - 1, ix, ix + 1):
            for gy in (iy - 1, iy, iy + 1):
                for idx in cell.get((gx, gy), ()):
                    _, _, px, py, _ = insts[idx]
                    if (px - x) ** 2 + (py - y) ** 2 <= radius * radius:
                        yield idx

    # 판 회전 난수는 따로 두어, 표본점·묶음 선택이 seat 없는 버전(V003)과 같게 한다.
    seat = band_seater(cfg, ground_at, comps, insts, random.Random(cfg["seed"] + 1)) if cfg.get("seat") else None
    targets = band_targets(cfg, ground_at, rng)
    placed = {}
    # place_lines가 있으면 그 선의 묶음·큰 바위만 놓는다(표본·난수·seat 호출은 모든 선에서 그대로 해 결과를 이전 버전과 맞춘다).
    keep = set(cfg.get("place_lines", cfg["lines"]))
    per_line = {}
    for target in targets:
        ax, ay, src_frame, src_slope = anchors[rng.randrange(len(anchors))]
        dst_frame, up = band_target_frame(target, ground_at)
        # 원본 묶음을 표면 좌표계 그대로 옮긴다: inst * inverse(src) * dst.
        relay = unreal.MathLibrary.compose_transforms(unreal.MathLibrary.invert_transform(src_frame), dst_frame)
        target["anchor"] = [round(ax), round(ay), round(src_slope, 1)]
        for idx in patch(ax, ay):
            ci, t, _, _, custom = insts[idx]
            moved = unreal.MathLibrary.compose_transforms(t, relay)
            if seat:
                moved = seat(idx, moved)
            elif comps[ci]["plant"]:
                # 식생은 옮긴 자리에서 현지 Up으로 다시 세운다(줄기 방향만 바로잡고 방위는 유지).
                z = unreal.MathLibrary.get_up_vector(moved.rotation.rotator())
                moved = unreal.Transform(moved.translation, tilt_between(moved.rotation.rotator(), z, up), moved.scale3d)
            if target["line"] not in keep:
                continue
            per_line[target["line"]] = per_line.get(target["line"], 0) + 1
            row = placed.setdefault(ci, ([], []))
            row[0].append(moved)
            row[1].extend(custom)

    boulders = []
    if "boulders" in cfg:
        bcfg = cfg["boulders"]
        sources = []
        for idx, (ci, t, x, y, _) in enumerate(insts):
            if comps[ci]["path"].rsplit("/", 1)[1].startswith(bcfg["meshes"]):
                frame, slope = band_source_frame(x, y)
                if slope >= cfg["anchor_slope"]:
                    sources.append((idx, frame))
        assert sources, "no boulder sources"
        for target in band_targets(dict(cfg, spacing=bcfg["spacing"]), ground_at, rng):
            idx, src_frame = sources[rng.randrange(len(sources))]
            ci, t, _, _, custom = insts[idx]
            dst_frame, _ = band_target_frame(target, ground_at)
            relay = unreal.MathLibrary.compose_transforms(unreal.MathLibrary.invert_transform(src_frame), dst_frame)
            k = rng.uniform(*bcfg["scale"])
            moved = unreal.MathLibrary.compose_transforms(unreal.Transform(t.translation, t.rotation.rotator(), t.scale3d * k),
                                                          relay)
            normal = unreal.MathLibrary.get_up_vector(dst_frame.rotation.rotator())
            sink = bcfg["sink"] * comps[ci]["height"] * abs(moved.scale3d.z)
            if seat:
                # 재안착은 원본 지면 위 높이(키운 배수)를 따르고, 덩어리는 그 뒤에 sink만큼 더 묻는다. 판은 seat가 묻는다.
                moved = seat(idx, moved, k)
                if seat.is_plate(idx):
                    sink = 0.0
            moved = unreal.Transform(moved.translation - normal * sink, moved.rotation.rotator(), moved.scale3d)
            if target["line"] not in keep:
                continue
            per_line[target["line"] + "_boulders"] = per_line.get(target["line"] + "_boulders", 0) + 1
            row = placed.setdefault(ci, ([], []))
            row[0].append(moved)
            row[1].extend(custom)
            boulders.append({"line": target["line"], "x": round(target["x"]), "y": round(target["y"]),
                             "mesh": comps[ci]["path"].rsplit("/", 1)[1], "scale": round(k, 2)})

    assert saving.new_blank_map(False)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary

    def add(parent, cls):
        handle, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls))
        assert not str(fail), fail
        return handle, lib.get_object(lib.get_data(handle))

    actor = actors.spawn_actor_from_class(unreal.Actor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    actor.set_actor_label("RimBand_" + version)
    root_handle, root = add(sds.k2_gather_subobject_data_for_instance(actor)[0], unreal.SceneComponent)
    root.set_mobility(unreal.ComponentMobility.STATIC)
    counts = {}
    for ci, (transforms, custom) in placed.items():
        src = comps[ci]
        _, comp = add(root_handle, unreal.HierarchicalInstancedStaticMeshComponent if src["hism"]
                      else unreal.InstancedStaticMeshComponent)
        comp.set_mobility(unreal.ComponentMobility.STATIC)
        comp.set_static_mesh(src["mesh"])
        for i, material in enumerate(src["overrides"]):
            if material:
                comp.set_material(i, material)
        for prop, value in src["props"].items():
            comp.set_editor_property(prop, value)
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.add_instances(transforms, False, True)
        k = src["custom_count"]
        if k:
            comp.set_num_custom_data_floats(k)
            for i in range(len(transforms)):
                for j in range(k):
                    comp.set_custom_data_value(i, j, custom[i * k + j], False)
        assert comp.get_instance_count() == len(transforms)
        counts[src["path"]] = counts.get(src["path"], 0) + len(transforms)
    actor.set_folder_path("RimBand")
    assert saving.save_map(world(), level), level
    report = {"level": level, "config": cfg, "anchors": len(anchors), "targets": targets, "boulders": boulders,
              "instances": sum(counts.values()), "by_line": per_line, "by_mesh": dict(sorted(counts.items(), key=lambda kv: -kv[1]))}
    (OUT / ("RimBand_%s.json" % version)).write_text(json.dumps(report, indent=1), encoding="utf-8")
    unreal.log("Z1_RIM_BAND_READY %d targets %d instances %s" % (len(targets), report["instances"], json.dumps(per_line)))


def importcrust(version):
    # Blender가 구운 Z1MassifCover 지각 FBX를 새 폴더로 반입한다(경계 정점은 Meadow native에 맞춘다).
    import AssembleRidgeOctant as assembly
    assert not dirty(), dirty()
    out = MASSIF_OUT / spec.MASSIF_COVER_VERSIONS[version][0]
    manifest = json.loads((out / "manifest.json").read_text(encoding="utf-8"))
    report = assembly.import_assets(manifest, MASSIF_FOLDER.format(version), out)
    unreal.log("Z1_MASSIF_CRUST_READY " + json.dumps(report))


def surface(version="V001"):
    # V001: 하상 재질 부모·MI를 복제해 구역 마스크만 바꾼다. 원본 V005·하상 재질은 수정하지 않는다.
    # V002: V001 부모·MI를 복제해 지면층 텍스처 세트와 MI 값만 바꾼다(SURFACE_VERSIONS).
    # V003~: V001 부모·MI를 복제해 구역 노드만 바꾼다(SURFACE_SCOPE_VERSIONS).
    if version in SURFACE_SCOPE_VERSIONS:
        return surface_scope(version)
    if version != "V001":
        return surface_variant(version)
    assets, edit = unreal.EditorAssetLibrary, unreal.MaterialEditingLibrary
    assert not dirty(), dirty()
    instance_path = WALL_MATERIAL["V011"]
    assert not assets.does_asset_exist(instance_path), instance_path
    parent = assets.duplicate_asset(SURFACE_SOURCE + "M_Ridge_Z1_Channel", SURFACE_FOLDER + "/M_Ridge_Z1_MassifSurface")
    assert parent
    scope, = [node for node in edit.get_material_expressions(parent) if isinstance(node, unreal.MaterialExpressionCustom)
              and node.get_editor_property("code").strip().startswith("return float2(Data.w,")]
    scope.set_editor_property("code", SURFACE_SCOPE)
    assert not edit.recompile_material(parent), "Massif surface material compilation failed."
    instance = assets.duplicate_asset(SURFACE_SOURCE + "MI_Ridge_Z1_Channel", instance_path)
    assert instance
    edit.set_material_instance_parent(instance, parent)
    edit.update_material_instance(instance)
    assert assets.save_loaded_asset(parent, False) and assets.save_loaded_asset(instance, False)
    unreal.log("Z1_MASSIF_SURFACE_READY " + instance_path)


GROUND_SOURCE = "T_MossyRockyGround_01_"
GROUND_MAPS = ("BC", "N", "AoRDp")


def surface_variant(version):
    assets, edit = unreal.EditorAssetLibrary, unreal.MaterialEditingLibrary
    assert not dirty(), dirty()
    row = SURFACE_VERSIONS[version]
    instance_path = WALL_MATERIAL["Surface" + version]
    folder = instance_path.rsplit("/", 1)[0]
    assert not assets.does_asset_exist(instance_path), instance_path
    ground = "/Game/Megascans/Surfaces/{0}/T_{0}_01_".format(row["ground"])
    textures = {GROUND_SOURCE + m: unreal.load_asset(ground + m) for m in GROUND_MAPS}
    assert all(textures.values()), textures
    parent = assets.duplicate_asset(SURFACE_FOLDER + "/M_Ridge_Z1_MassifSurface", folder + "/M_Ridge_Z1_MassifSurface")
    assert parent
    # 지면층 텍스처는 삼각 격자 함수에 들어가는 TextureObject와 원본 파생에 남은 TextureSample 모두에 있다. 함께 바꾼다.
    swapped = {}
    for node in edit.get_material_expressions(parent):
        if not isinstance(node, unreal.MaterialExpressionTextureBase):
            continue
        texture = node.get_editor_property("texture")
        if texture and texture.get_name() in textures:
            node.set_editor_property("texture", textures[texture.get_name()])
            swapped[texture.get_name()] = swapped.get(texture.get_name(), 0) + 1
    assert set(swapped) == set(textures), swapped
    assert not edit.recompile_material(parent), "Massif surface variant compilation failed."
    instance = assets.duplicate_asset(SURFACE_FOLDER + "/MI_Ridge_Z1_MassifSurface", instance_path)
    assert instance
    edit.set_material_instance_parent(instance, parent)
    for name, value in row["scalar"].items():
        edit.set_material_instance_scalar_parameter_value(instance, name, value)
        assert abs(edit.get_material_instance_scalar_parameter_value(instance, name) - value) < 1e-4, name
    for name, value in row["vector"].items():
        edit.set_material_instance_vector_parameter_value(instance, name, unreal.LinearColor(*value))
    edit.update_material_instance(instance)
    assert assets.save_loaded_asset(parent, False) and assets.save_loaded_asset(instance, False)
    report = {"version": version, "instance": instance_path, "ground": row["ground"], "swapped_nodes": swapped,
              "scalar": row["scalar"], "vector": row["vector"]}
    (OUT / ("MassifSurface_%s.json" % version)).write_text(json.dumps(report, indent=1), encoding="utf-8")
    unreal.log("Z1_MASSIF_SURFACE_READY " + instance_path)


def surface_scope(version):
    # V001 부모·MI를 복제해 구역 Custom 노드 코드만 바꾼다. 이음매 거리에 메시 로컬 위치가 필요해 입력을 하나 더한다.
    assets, edit = unreal.EditorAssetLibrary, unreal.MaterialEditingLibrary
    assert not dirty(), dirty()
    instance_path = WALL_MATERIAL["Surface" + version]
    folder = instance_path.rsplit("/", 1)[0]
    assert not assets.does_asset_exist(instance_path), instance_path
    parent = assets.duplicate_asset(SURFACE_FOLDER + "/M_Ridge_Z1_MassifSurface", folder + "/M_Ridge_Z1_MassifSurface")
    assert parent
    scope, = [node for node in edit.get_material_expressions(parent) if isinstance(node, unreal.MaterialExpressionCustom)
              and "const float west" in node.get_editor_property("code")]
    coordinates, = edit.get_inputs_for_material_expression(parent, scope)
    assert isinstance(coordinates, unreal.MaterialExpressionMaterialFunctionCall), coordinates
    # 좌표 함수(MF_RidgeSurfaceCoordinates)가 받는 메시 로컬 위치 노드를 그대로 쓴다.
    position = edit.get_inputs_for_material_expression(parent, coordinates)[0]
    assert isinstance(position, unreal.MaterialExpressionTransformPosition), position
    inputs = []
    for name in ("Data", "PositionLocal"):
        value = unreal.CustomInput()
        value.set_editor_property("input_name", name)
        inputs.append(value)
    scope.set_editor_property("inputs", inputs)
    scope.set_editor_property("code", SURFACE_SCOPE_VERSIONS[version])
    assert edit.connect_material_expressions(coordinates, "", scope, "Data")
    assert edit.connect_material_expressions(position, "", scope, "PositionLocal")
    assert [n.get_name() for n in edit.get_inputs_for_material_expression(parent, scope)] == \
        [coordinates.get_name(), position.get_name()]
    assert not edit.recompile_material(parent), "Massif surface scope compilation failed."
    instance = assets.duplicate_asset(SURFACE_FOLDER + "/MI_Ridge_Z1_MassifSurface", instance_path)
    assert instance
    edit.set_material_instance_parent(instance, parent)
    edit.update_material_instance(instance)
    assert assets.save_loaded_asset(parent, False) and assets.save_loaded_asset(instance, False)
    report = {"version": version, "instance": instance_path, "scope_code": SURFACE_SCOPE_VERSIONS[version],
              "inputs": [coordinates.get_name(), position.get_name()]}
    (OUT / ("MassifSurface_%s.json" % version)).write_text(json.dumps(report, indent=1), encoding="utf-8")
    unreal.log("Z1_MASSIF_SURFACE_READY " + instance_path)


def puddle():
    # ED 진흙 웅덩이 재질의 프로젝트 사본에 `Single Layer Water Material` 출력 노드를 더한다(원본은 수정하지 않는다).
    # Substrate에서는 입력을 비워도 되며 레이 트레이싱 히트 셰이더용 기본값(산란·흡수 0, PhaseG 0)이 들어간다.
    # 래스터 셰이딩은 기존 Substrate Single Layer Water BSDF가 그대로 맡는다.
    assets, edit = unreal.EditorAssetLibrary, unreal.MaterialEditingLibrary
    assert not dirty(), dirty()
    instance_path = PUDDLE_FOLDER + "MI_MudPuddle_01"
    assert not assets.does_asset_exist(instance_path), instance_path
    parent = assets.duplicate_asset(PUDDLE_SOURCE + "M_MudPuddle_01_PCG", PUDDLE_FOLDER + "M_MudPuddle_01_PCG")
    assert parent
    nodes = edit.get_material_expressions(parent)
    assert not [n for n in nodes if isinstance(n, unreal.MaterialExpressionSingleLayerWaterMaterialOutput)]
    edit.create_material_expression(parent, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, 400, 600)
    assert not edit.recompile_material(parent), "Puddle material compilation failed."
    instance = assets.duplicate_asset(PUDDLE_SOURCE + "MI_MudPuddle_01", instance_path)
    assert instance
    edit.set_material_instance_parent(instance, parent)
    edit.update_material_instance(instance)
    assert assets.save_loaded_asset(parent, False) and assets.save_loaded_asset(instance, False)
    report = {"parent": parent.get_path_name(), "instance": instance_path,
              "added": "MaterialExpressionSingleLayerWaterMaterialOutput (inputs unconnected)"}
    (OUT / "Puddle_V001.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
    unreal.log("Z1_PUDDLE_READY " + instance_path)


CR_GROUPS = ("LargeAssembly", "SmallAssembly", "Ground", "Ditch", "Props")
CR_FLAT = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/CloseRangeBake_V001/CloseRangeBake_V001"


def crflat():
    # ED에서 구운 CloseRange 5그룹을 평면 맵 원점에 그대로 놓아 굽기 결과를 ED 원본과 비교한다.
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    saving = unreal.EditorLoadingAndSavingUtils
    assert not dirty(), dirty()
    assert not unreal.EditorAssetLibrary.does_asset_exist(CR_FLAT), CR_FLAT
    assert saving.new_blank_map(False)
    # ED CloseRange와 같은 종류의 하늘 조명(태양·스카이라이트·대기·안개)을 기본값으로 둔다.
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 3000), unreal.Rotator(pitch=-35, yaw=30, roll=0))
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 3000), unreal.Rotator(0, 0, 0))
    sky.light_component.set_editor_property("real_time_capture", True)
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    for group in CR_GROUPS:
        actor = actors.spawn_actor_from_class(unreal.LevelInstance, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        actor.set_actor_label("CR_" + group)
        actor.set_world_asset(unreal.load_asset(CR + group))
        actor.load_level_instance()
        assert package(actor.get_world_asset()) == CR + group, group
    assert saving.save_map(world(), CR_FLAT), CR_FLAT
    flatlook(-5500, -3500, 2000, 0, 0, 1200)
    unreal.log("CR_FLAT_READY")


def flatlook(ex, ey, ez, tx, ty, tz):
    # 평면 맵 시점. 좌표는 월드 cm다.
    eye, target = unreal.Vector(float(ex), float(ey), float(ez)), unreal.Vector(float(tx), float(ty), float(tz))
    rot = unreal.MathLibrary.find_look_at_rotation(eye, target)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(eye, rot)


def look(ex, ey, eh, tx, ty, th):
    # 시험 화면 확인용 시점. 인자는 옥탄트 로컬 x, y와 그 위치 V002 기반면 위 높이(cm)다.
    eye, target = point(float(ex), float(ey), float(eh)), point(float(tx), float(ty), float(th))
    view_rot = unreal.MathLibrary.make_rot_from_xz(target - eye, eye * (-1.0 / eye.length()))
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(eye, view_rot)


if __name__ == "__main__":
    {"lights": lights, "build": build, "instances": instances, "view": view, "wall": wall,
     "look": look, "crflat": crflat, "flatlook": flatlook, "importcrust": importcrust, "drape": drape,
     "surface": surface, "puddle": puddle, "band": band}[sys.argv[1]](*sys.argv[2:])
