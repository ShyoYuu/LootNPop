"""UE Cmd에서 Z1 ED 조립체 시험 레벨을 만든다.

인자:
- lights: 현재 레벨(TestMap03)의 조명 액터 목록을 읽는다.
- build: TestMap03 조명을 복사한 새 시험 레벨을 만들어 저장하고, Level Instance 배치 transform을 JSON으로 쓴다.
- instances: 시험 레벨에 V002와 ED 조립체 Level Instance를 넣고 저장한 뒤 비교 시점으로 옮긴다.
- view: 뷰포트를 비교 시점으로 옮긴다.
- wall <버전>: 시험 레벨이 열린 상태에서 V002 사본(서쪽 벽 제외)과 WALLS의 조립체로 Z1AssemblyWall_<버전>을 만든다.
- importcrust <버전>: Blender가 만든 산괴 고원 지각(Z1MassifCover)을 반입한다.
- drape <버전>: CloseRange 평면 그룹을 구면 지각에 다시 앉힌 Z1MassifDrape_<버전>을 만든다(V003부터 지면 타일을 경사에 맞춤).
- surface [버전]: 구역 마스크를 서쪽 고원까지 넓힌 지각 재질(Z1MassifSurface)을 만든다. V002부터는 V001에서 지면층 텍스처 세트만 바꾼 후보다.
- look / crflat / flatlook: 확인 시점·CloseRange 평면 비교 레벨.
자동 스크린샷(AutomationLibrary.take_high_res_screenshot)은 Megascans 재질 셰이더 assert로 에디터를 죽이므로 쓰지 않는다.
"""
import importlib
import json
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
WEST_COVER_LABEL = "SM_Octant_Ridge_00_V012_Z1WestCover_R30000"
WALL_VIEW_EYE = (4500, -400, 170)
WALL_VIEW_TARGET = (-1700, -300, 1800)


def asset_path(name):
    # PCGAssemblies 상대 이름 또는 /Game/ 전체 경로(LNPBake 구운 레벨)를 받는다.
    return name if name.startswith("/Game/") else ASSEMBLIES + name


def floor_point(x, y, offset, base="floor"):
    # floor는 서쪽 벽 발치 분지 바닥, local은 해당 위치의 V002 기반면 높이를 쓴다.
    ground = spec.ground(-1250, y) if base == "floor" else spec.west_cover_ground(x, y)
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
        if label.startswith(WALL_REPLACED):
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
    report = {"level": wall_level, "kept_v002": kept, "replaced_v002": skipped, "assemblies": placed,
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
              "groups": built, **summary}
    (OUT / ("MassifDrape_%s.json" % version)).write_text(json.dumps(report, indent=1), encoding="utf-8")
    unreal.log("Z1_MASSIF_DRAPE_READY " + json.dumps(built))


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
    # V002~: V001 부모·MI를 복제해 지면층 텍스처 세트와 MI 값만 바꾼다(SURFACE_VERSIONS).
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
     "surface": surface}[sys.argv[1]](*sys.argv[2:])
