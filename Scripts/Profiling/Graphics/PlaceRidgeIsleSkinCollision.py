"""IsleSkin 외피 충돌 Shell(3b, GX-041)을 반입하고 V007을 복제한 새 맵에 놓는다.

Blender `Scripts/Blender/BuildRidgeIsleSkinCollision.py -- build <Shell 판>`의 결과를 쓴다. V007 맵·공유 원본·외피 에셋은 바꾸지 않는다.

실행: 에디터 상태 표시줄 Cmd에 py "<이 파일>" <명령> [인자]
- import <Shell 판>: Shell FBX를 IsleSkin_V001/Collision_<판>/에 반입(complex-as-simple 단일 trimesh, 자동 단순 도형 제거)하고 저장한다.
- build <맵 판> <Shell 판>: IsleSkin_V007 맵을 IsleSkin_<맵 판>으로 복제해 Shell 액터 하나(원점, 숨김, Shell 역할 태그·LNPStaticBlocker)를
  더하고 새 맵만 저장한다. 끝나면 새 맵을 디스크에서 다시 연다.
- verify <맵 판> <Shell 판>: (build 다음 Cmd) 열린 맵에서 충돌 컴포넌트가 기존 5개 + Shell 1개인지, Shell 역할 계약·메시 검사,
  외피(IsleSkinTrial 태그) 충돌 0, 미저장 패키지·PIE를 확인한다(읽기 전용).
결과: Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Collision/
"""
import hashlib
import json
from pathlib import Path
import sys

import unreal

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path[:0] = [str(HERE), str(ROOT / "Scripts")]
import AssembleRidgeOctant as assembly  # noqa: E402  (import_assets·native_mesh 재사용)

OUT = ROOT / "Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Collision"
EDT = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/"
SOURCE_LEVEL = EDT + "IsleSkin_V007/LVI_Ridge_EDTerrain_IsleSkin_V007"
LEVEL = EDT + "IsleSkin_{0}/LVI_Ridge_EDTerrain_IsleSkin_{0}"
FOLDER = EDT + "IsleSkin_V001/Collision_{0}"
# 옥탄트 제작 가이드 §4.2 역할 표의 Shell(ExportRidgeEDCollision.ROLE_CONTRACT와 같다).
SHELL_TAGS = ["LNP.Surface.Blocker", "LNP.Surface.Static"]
SHELL_PROFILE = "LNPStaticBlocker"
# V007에 이미 있는 충돌(3a Census): 충돌 지각 V013·Z1 Floor/Shell(V003)·보존 구조물 Floor/Shell.
EXISTING = ["SM_Octant_Ridge_00_V012_Z1MassifCover_V013_R30000", "SM_Ridge_00_V012_PreservedRocks_Floor",
            "SM_Ridge_00_V012_PreservedRocks_Shell", "SM_Ridge_EDTerrain_V001_Z1_Floor", "SM_Ridge_EDTerrain_V001_Z1_Shell"]
COMPONENT_LIMIT_CM = 32767.0


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def package(obj):
    return obj.get_path_name().split(".")[0] if obj else None


def dirty():
    saving = unreal.EditorLoadingAndSavingUtils
    return [p.get_path_name() for p in list(saving.get_dirty_map_packages()) + list(saving.get_dirty_content_packages())]


def pie():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()


def write(name, data):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")


def manifest(shell):
    return json.loads((OUT / ("Shell_%s" % shell) / "manifest.json").read_text(encoding="utf-8"))


def mesh_check(asset):
    """단순 도형 0·complex-as-simple·LOD 1개·정점이 옥탄트 안·성분 절댓값 32767 이하(ExportRidgeEDCollision.mesh_check와 같다)."""
    body = asset.get_editor_property("body_setup")
    geom = body.get_editor_property("agg_geom")
    simple = sum(len(geom.get_editor_property(k)) for k in ("box_elems", "convex_elems", "sphere_elems", "sphyl_elems",
                                                          "tapered_capsule_elems"))
    positions = assembly.native_mesh(asset)[1]
    low = min(min(p.x, p.y, p.z) for p in positions)
    high = max(max(abs(p.x), abs(p.y), abs(p.z)) for p in positions)
    complex_as_simple = body.get_editor_property("collision_trace_flag") == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
    nanite = asset.get_editor_property("nanite_settings").get_editor_property("enabled")
    return {"triangles": asset.get_num_triangles(0), "lods": asset.get_num_lods(), "simple_shapes": simple,
            "complex_as_simple": complex_as_simple, "nanite": nanite, "min_component_cm": round(low, 3),
            "abs_max_component_cm": round(high, 3),
            "pass": simple == 0 and complex_as_simple and low > -0.05 and high <= COMPONENT_LIMIT_CM
            and asset.get_num_lods() == 1}


def import_shell(shell):
    assert not pie()
    assert not dirty(), dirty()
    data = manifest(shell)
    item, = data["exports"]
    path = Path(item["file"])
    assert hashlib.sha256(path.read_bytes()).hexdigest() == item["sha256"], path
    # import_assets는 min/max 사전 형식의 bounds_cm와 새 경로를 요구하고 반입 뒤 저장한다(새 에셋만).
    folder = FOLDER.format(shell)
    rows = assembly.import_assets(data, folder, OUT / ("Shell_%s" % shell))
    asset = unreal.load_asset(folder + "/" + item["name"])
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(asset)
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    check = mesh_check(asset)
    write("Import_%s.json" % shell, {"asset": package(asset), "import": rows, "check": check, "dirty": dirty()})
    unreal.log("ISLESKIN_SHELL_IMPORT " + json.dumps(check))


def build(version, shell):
    assert not pie()
    assert not dirty(), dirty()
    target = LEVEL.format(version)
    assert not unreal.EditorAssetLibrary.does_asset_exist(target), "Use a new version: " + target
    item, = manifest(shell)["exports"]
    mesh = unreal.load_asset(FOLDER.format(shell) + "/" + item["name"])
    assert mesh and mesh_check(mesh)["pass"], item["name"]
    assert unreal.EditorAssetLibrary.duplicate_asset(SOURCE_LEVEL, target), target
    assert unreal.EditorAssetLibrary.save_asset(target, False), target
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(target), target
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    actor.set_actor_label(item["name"])
    comp = actor.static_mesh_component
    comp.modify()
    comp.set_static_mesh(mesh)
    comp.set_editor_property("component_tags", [unreal.Name(t) for t in SHELL_TAGS])
    comp.set_editor_property("use_default_collision", False)
    comp.set_collision_profile_name(SHELL_PROFILE)
    comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    comp.set_mobility(unreal.ComponentMobility.STATIC)
    comp.set_visibility(False)
    actor.set_folder_path("Collision")
    assert unreal.EditorLoadingAndSavingUtils.save_map(world(), target), target
    write("Build_%s.json" % version, {"level": target, "source": SOURCE_LEVEL, "shell": package(mesh), "dirty": dirty()})
    # 디스크 저장값으로 검사하려고 새 맵을 다시 연다(Python 충돌 설정이 저장 때 되돌아가는 함정 대비).
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(target), target
    unreal.log("ISLESKIN_SHELL_BUILD_READY %s; run verify in the next command" % target)


def verify(version, shell):
    target = LEVEL.format(version)
    item, = manifest(shell)["exports"]
    shell_mesh = FOLDER.format(shell) + "/" + item["name"]
    rows, visual = [], []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor):
        trial = "IsleSkinTrial" in [str(t) for t in actor.tags]
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            on = comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION
            if trial and on:
                visual.append(actor.get_actor_label())
            if on:
                rows.append({"level": actor.get_level().get_outermost().get_name(), "actor": actor.get_actor_label(),
                             "mesh": package(comp.static_mesh), "profile": str(comp.get_collision_profile_name()),
                             "enabled": str(comp.get_collision_enabled()), "visible": comp.is_visible(),
                             "use_default": comp.get_editor_property("use_default_collision"),
                             "mobility": str(comp.mobility), "location": [actor.get_actor_location().x,
                                                                          actor.get_actor_location().y,
                                                                          actor.get_actor_location().z],
                             "tags": sorted(str(t) for t in comp.get_editor_property("component_tags"))})
    shells = [r for r in rows if r["mesh"] == shell_mesh]
    others = sorted(r["mesh"].rsplit("/", 1)[-1] for r in rows if r["mesh"] != shell_mesh)
    checks = {"level": package(world()) == target, "existing_five": others == sorted(EXISTING),
              "one_shell": len(shells) == 1, "visual_collision_zero": not visual, "pie_false": not pie(),
              "dirty_none": not dirty()}
    if shells:
        s = shells[0]
        checks["shell_contract"] = (s["tags"] == sorted(SHELL_TAGS) and s["profile"] == SHELL_PROFILE
                                    and s["enabled"] == str(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
                                    and not s["use_default"] and not s["visible"]
                                    and s["mobility"] == str(unreal.ComponentMobility.STATIC)
                                    and max(abs(c) for c in s["location"]) < 0.01)
        checks["shell_mesh"] = mesh_check(unreal.load_asset(shell_mesh))["pass"]
    report = {"level": package(world()), "checks": checks, "pass": all(checks.values()), "colliding": rows,
              "visual_collision": visual, "dirty": dirty()}
    write("Verify_%s.json" % version, report)
    unreal.log("ISLESKIN_SHELL_VERIFY " + json.dumps({"pass": report["pass"], "checks": checks}))


if __name__ == "__main__":
    command, *args = sys.argv[1:]
    {"import": import_shell, "build": build, "verify": verify}[command](*args)
