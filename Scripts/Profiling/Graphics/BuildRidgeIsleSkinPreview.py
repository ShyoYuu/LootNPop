"""IsleSkin 외피 시험 맵의 Lit 미리보기 레벨(조명 있는 비교 레벨)을 만든다.

IsleSkin LVI 맵에는 조명이 없어 Lit 화면이 검다. Z1 조립체 시험과 같은 방식(BuildRidgeZ1AssemblyTest.build_wall_level)으로
TestMap03 조명 사본 + 시험 전용 균일 회색 Sky Light(GX-025, 세기 50) + IsleSkin LVI Level Instance(원점) 하나를 새 맵에 둔다.
원본 IsleSkin 맵·U5aBody V008·조명 원본·TestMap03은 바꾸지 않는다.

실행: 에디터 상태 표시줄 Cmd에 py "<이 파일>" <명령> [인자]
- build <V006|V007|V008|V010>: 조명 원본 Z1AssemblyTest_V001이 열린 상태(다음 Cmd)에서 IsleSkinLit_<판>을 만들어 저장한다.
- verify <V006|V007|V008|V010>: 열린 미리보기 레벨의 조명·Sky Light·Level Instance 구성과 dirty/PIE를 검사한다.
- view <시점>: 뷰포트를 IsleSkin_V001/PreviewViews_V001.json의 시점으로 옮긴다.
- sky <0|1>: Sky Light를 에디터에서만 임시로 숨기거나(0) 되돌린다(1). 패키지는 dirty가 되지 않는다.
결과: Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Preview/
"""
import importlib
import json
from pathlib import Path
import sys
import time

import unreal

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE)]
import BuildRidgeZ1AssemblyTest as assembly

# 에디터 Python 모듈 캐시를 피한다.
importlib.reload(assembly)

ROOT = HERE.parents[2]
OUT = ROOT / "Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Preview"
VIEWS = HERE / "IsleSkin_V001/PreviewViews_V001.json"
EDT = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/"
SOURCE = {"V006": EDT + "IsleSkin_V006/LVI_Ridge_EDTerrain_IsleSkin_V006",
          "V007": EDT + "IsleSkin_V007/LVI_Ridge_EDTerrain_IsleSkin_V007",
          "V008": EDT + "IsleSkin_V008/LVI_Ridge_EDTerrain_IsleSkin_V008",
          # 섬 윗면 단위(BuildRidgeIsleTop.py): V010 = V009 + 섬 덮개 재분할(2a).
          "V010": EDT + "IsleSkin_V010/LVI_Ridge_EDTerrain_IsleSkin_V010",
          # V011 = V010 + 섬 엄폐 각기둥 몸체·Shell(2b, GX-042).
          "V011": EDT + "IsleSkin_V011/LVI_Ridge_EDTerrain_IsleSkin_V011",
          "V012": EDT + "IsleSkin_V012/LVI_Ridge_EDTerrain_IsleSkin_V012",
          "V013": EDT + "IsleSkin_V013/LVI_Ridge_EDTerrain_IsleSkin_V013",
          "V014": EDT + "IsleSkin_V014/LVI_Ridge_EDTerrain_IsleSkin_V014",
          "V015": EDT + "IsleSkin_V015/LVI_Ridge_EDTerrain_IsleSkin_V015",
          "V016": EDT + "IsleSkin_V016/LVI_Ridge_EDTerrain_IsleSkin_V016"}
PREVIEW = EDT + "IsleSkinLit_V001/IsleSkinLit_{}"
# Z1AssemblyWall V021 이후 시험 레벨과 같은 Sky Light(TestMap03에는 없다, GX-025).
SKY = {"cubemap": "/Engine/EngineResources/GrayTextureCube", "intensity": 50.0}
SKY_LABEL = "SkyLight_UniformGray"


def write(name, data):
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / name
    path.write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")
    return path


def stamp():
    return time.strftime("%Y%m%d_%H%M%S")


def level_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def pie():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()


def build(version):
    assert assembly.package(assembly.world()) == assembly.LEVEL, assembly.package(assembly.world())
    assert not pie()
    assert not assembly.dirty(), assembly.dirty()
    dest = PREVIEW.format(version)
    row = {"label": "LI_IsleSkin_%s" % version, "level": SOURCE[version],
           "location": unreal.Vector(0, 0, 0), "rotation": unreal.Rotator(0, 0, 0), "scale": 1.0,
           "meta": {"note": "IsleSkin LVI is authored in world coordinates; placed at origin"}}
    placed = assembly.build_wall_level(dest, [], [row], dict(SKY))
    report = {"level": dest, "source": SOURCE[version], "light_source": assembly.LEVEL, "sky": SKY,
              "instances": placed, "dirty_after": assembly.dirty()}
    write("Build_%s.json" % version, report)
    unreal.log("RIDGE_ISLESKIN_PREVIEW_BUILD " + json.dumps({"level": dest, "dirty": report["dirty_after"]}))


def verify(version):
    dest = PREVIEW.format(version)
    checks = {}
    checks["level"] = assembly.package(assembly.world()) == dest
    expected = json.loads((assembly.OUT / "TestMap03Lights.json").read_text(encoding="utf-8"))["actors"]
    actors = level_actors()
    lights = [a for a in actors if a.get_class().get_name() in assembly.LIGHT_CLASSES]
    checks["lights"] = sorted(tuple(assembly.xyz(a.get_actor_location())) for a in lights) == \
        sorted(tuple(r["location"]) for r in expected)
    skies = [a for a in actors if isinstance(a, unreal.SkyLight)]
    sky_ok = len(skies) == 1
    if sky_ok:
        comp = skies[0].light_component
        cube = comp.get_editor_property("cubemap")
        sky_ok = (abs(comp.get_editor_property("intensity") - SKY["intensity"]) < 1e-4
                  and cube is not None and assembly.package(cube) == SKY["cubemap"]
                  and not comp.get_editor_property("lower_hemisphere_is_black"))
    checks["sky"] = sky_ok
    instances = [a for a in actors if isinstance(a, unreal.LevelInstance)]
    li_rows = []
    for a in instances:
        asset = a.get_world_asset()
        li_rows.append({"label": a.get_actor_label(), "level": assembly.package(asset) if asset else None,
                        "location": assembly.xyz(a.get_actor_location()),
                        "rotation": [a.get_actor_rotation().pitch, a.get_actor_rotation().yaw, a.get_actor_rotation().roll],
                        "scale": assembly.xyz(a.get_actor_scale3d())})
    checks["instance"] = (len(li_rows) == 1 and li_rows[0]["level"] == SOURCE[version]
                          and li_rows[0]["location"] == [0, 0, 0] and li_rows[0]["rotation"] == [0, 0, 0]
                          and li_rows[0]["scale"] == [1, 1, 1])
    # 이 레벨 자신은 조명·Sky·Level Instance 외 액터를 갖지 않는다(WorldSettings 등 기본 액터 제외).
    own = [a for a in actors if a.get_path_name().startswith(dest + ".")]
    allowed = set(assembly.LIGHT_CLASSES) | {"SkyLight", "LevelInstance", "WorldSettings", "Brush", "DefaultPhysicsVolume",
                                            "GameplayDebuggerPlayerManager", "WorldDataLayers", "AbstractNavData"}
    extra = sorted({a.get_class().get_name() for a in own} - allowed)
    checks["no_extra_actors"] = not extra
    checks["pie_false"] = not pie()
    checks["dirty_none"] = not assembly.dirty()
    report = {"level": dest, "checks": checks, "pass": all(checks.values()), "lights": len(lights),
              "instances": li_rows, "extra_classes": extra, "dirty": assembly.dirty()}
    write("Verify_%s.json" % version, report)
    unreal.log("RIDGE_ISLESKIN_PREVIEW_VERIFY " + json.dumps({"pass": report["pass"], "checks": checks}))


def view(name):
    row = json.loads(VIEWS.read_text(encoding="utf-8"))["views"][name]
    pitch, yaw, roll = row["rotation"]
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
        unreal.Vector(*row["location"]), unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll))
    unreal.log("RIDGE_ISLESKIN_PREVIEW_VIEW " + name)


def sky(value):
    hits = [a for a in level_actors() if isinstance(a, unreal.SkyLight) and a.get_actor_label() == SKY_LABEL]
    assert len(hits) == 1, len(hits)
    hits[0].set_is_temporarily_hidden_in_editor(value == "0")
    unreal.log("RIDGE_ISLESKIN_PREVIEW_SKY %s %s" % (value, json.dumps({"dirty": assembly.dirty()})))


if __name__ == "__main__":
    command, *args = sys.argv[1:]
    {"build": build, "verify": verify, "view": view, "sky": sky}[command](*args)
