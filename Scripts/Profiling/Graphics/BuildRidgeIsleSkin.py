"""Phase 1-BR 부유섬·하늘 석교 ED 외피 시험 맵을 만든다(GX-037·GX-038).

UE Cmd: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/BuildRidgeIsleSkin.py" <명령> <판>
- build <판>: 채택 V008 맵을 IsleSkin_<판> 새 맵으로 복제하고, 보존 구조물(PreservedRocks Floor/Shell) 시각 재질을
  지각 재질(Surface V003)로 컴포넌트에서만 바꾼 뒤 IsleSkin_<판>.json 배치를 NoCollision으로 놓고 새 맵만 저장한다.
- apply <판>: 열린 IsleSkin_<판> 맵에서 IsleSkinTrial 태그 액터를 지우고 JSON으로 다시 놓는다(사용자 검토 전 반복용).
- verify <판>: 열린 맵의 실제 자세·충돌·재질·미저장 패키지를 JSON과 대조한다(읽기 전용).
기존 V008 맵·보존 메시 에셋·ED 원본은 바꾸지 않는다. 생성은 GenerateRidgeIsleSkin.py(오프라인)가 한다.
"""
import datetime
import json
from pathlib import Path
import sys

import unreal

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parents[1])]
import RidgeZ1Layout as layout  # noqa: E402

PROJECT = HERE.parents[2]
AUTHOR = HERE / "IsleSkin_V001"
OUT = PROJECT / "Saved/Profiling/Graphics/Phase01/IsleSkin_V001"
TAG = "IsleSkinTrial"
PRESERVED = ("SM_Ridge_00_V012_PreservedRocks_Floor", "SM_Ridge_00_V012_PreservedRocks_Shell")


def read(version):
    return json.loads((AUTHOR / ("IsleSkin_%s.json" % version)).read_text(encoding="utf-8"))


def stamp():
    return datetime.datetime.now().strftime("%Y%m%d_%H%M%S")


def write(name, data):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")


def top_actors():
    world = layout.test.world()
    prefix = world.get_path_name() + ":PersistentLevel."
    return [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
            if a.get_path_name().startswith(prefix)]


def preserved_material(data):
    material = unreal.load_asset(data["preserved_rock_material"])
    assert material, data["preserved_rock_material"]
    rows = []
    for actor in top_actors():
        if actor.get_actor_label() not in PRESERVED:
            continue
        comp = actor.static_mesh_component
        comp.modify()
        for i in range(comp.get_num_materials()):
            comp.set_material(i, material)
        rows.append(actor.get_actor_label())
    assert sorted(rows) == sorted(PRESERVED), rows
    return rows


def import_body(row):
    """Blender 몸체 FBX를 반입한다(이미 있으면 재사용하고 삼각형 수만 확인). U5a 반입 설정과 같다."""
    if not unreal.EditorAssetLibrary.does_asset_exist(row["asset"]):
        task = unreal.AssetImportTask()
        task.filename = str(PROJECT / row["file"])
        task.destination_path = row["asset"].rsplit("/", 1)[0]
        task.destination_name = row["asset"].rsplit("/", 1)[1]
        task.automated = True
        task.save = False
        task.factory = unreal.FbxFactory()
        options = unreal.FbxImportUI()
        options.automated_import_should_detect_type = False
        options.import_mesh = True
        options.import_as_skeletal = False
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        options.original_import_type = unreal.FBXImportType.FBXIT_STATIC_MESH
        options.import_materials = False
        options.import_textures = False
        options.import_animations = False
        options.static_mesh_import_data.combine_meshes = True
        options.static_mesh_import_data.remove_degenerates = False
        task.options = options
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(row["asset"])
    assert mesh, row["asset"]
    assert mesh.get_num_triangles(0) == row["triangles"], (row["asset"], mesh.get_num_triangles(0))
    material = unreal.load_asset(row["material"])
    assert material, row["material"]
    mesh.set_material(0, material)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    return mesh


def spawn(actors, label, mesh, transform, tags, folder):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, transform.translation, transform.rotation.rotator())
    actor.set_actor_label(label)
    actor.set_actor_scale3d(transform.scale3d)
    comp = actor.static_mesh_component
    comp.modify()
    comp.set_static_mesh(mesh)
    comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    comp.set_editor_property("use_default_collision", False)
    comp.set_collision_profile_name("NoCollision")
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property("can_ever_affect_navigation", False)
    comp.set_editor_property("generate_overlap_events", False)
    actor.tags = [unreal.Name(t) for t in tags]
    actor.set_folder_path(folder)
    return actor


def place(data):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    labels = {r["label"] for r in data["placements"] + data.get("bodies", [])}
    for actor in top_actors():
        # 태그가 붙기 전에 중단된 액터도 같은 라벨이면 지운다(V008에서 온 액터 라벨과는 겹치지 않는다).
        if TAG in [str(t) for t in actor.tags] or actor.get_actor_label() in labels:
            actors.destroy_actor(actor)
    count = 0
    for row in data["placements"]:
        mesh = unreal.load_asset(row["mesh"])
        assert mesh, row["mesh"]
        spawn(actors, row["label"], mesh, layout.pose_transform(row), data["tags"], "EDTerrain/IsleSkin/" + row["group"])
        count += 1
    identity = unreal.Transform(unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0), unreal.Vector(1, 1, 1))
    for row in data.get("bodies", []):
        spawn(actors, row["label"], import_body(row), identity, data["tags"], "EDTerrain/IsleSkin/" + row["group"])
        count += 1
    return count


def build(version):
    data = read(version)
    assert not layout.test.dirty(), layout.test.dirty()
    target = data["work_level"]
    assert not unreal.EditorAssetLibrary.does_asset_exist(target), "Use a new version: " + target
    assert unreal.EditorAssetLibrary.duplicate_asset(data["source_level"], target), target
    assert unreal.EditorAssetLibrary.save_asset(target, False), target
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(target), target
    materials = preserved_material(data)
    count = place(data)
    assert unreal.EditorLoadingAndSavingUtils.save_map(layout.test.world(), target), target
    write("Build_%s_%s.json" % (version, stamp()), {"level": target, "source": data["source_level"],
                                                  "placed": count, "material_overrides": materials})
    unreal.log("RIDGE_ISLE_SKIN_BUILD_READY %d; run verify in the next command" % count)


def apply(version):
    data = read(version)
    target = data["work_level"]
    assert layout.world_package() == target, layout.world_package()
    preserved_material(data)
    count = place(data)
    assert unreal.EditorLoadingAndSavingUtils.save_map(layout.test.world(), target), target
    write("Apply_%s_%s.json" % (version, stamp()), {"level": target, "placed": count})
    unreal.log("RIDGE_ISLE_SKIN_APPLY_READY %d" % count)


def verify(version):
    data = read(version)
    by_label = {a.get_actor_label(): a for a in top_actors()}
    issues, worst = [], [0.0, 0.0, 0.0]
    for row in data["placements"]:
        actor = by_label.get(row["label"])
        if not actor:
            issues.append("missing " + row["label"])
            continue
        comp = actor.static_mesh_component
        if layout.test.package(comp.static_mesh) != row["mesh"]:
            issues.append("mesh " + row["label"])
        if comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION or \
                str(comp.get_collision_profile_name()) != "NoCollision" or comp.get_editor_property("use_default_collision"):
            issues.append("collision " + row["label"])
        err = layout.pose_error(actor.get_actor_transform(), layout.pose_transform(row))
        worst = [max(a, b) for a, b in zip(worst, err)]
    body_bounds = 0.0
    for row in data.get("bodies", []):
        actor = by_label.get(row["label"])
        if not actor:
            issues.append("missing " + row["label"])
            continue
        comp = actor.static_mesh_component
        if layout.test.package(comp.static_mesh) != row["asset"]:
            issues.append("mesh " + row["label"])
        if comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION or \
                str(comp.get_collision_profile_name()) != "NoCollision" or comp.get_editor_property("use_default_collision"):
            issues.append("collision " + row["label"])
        if actor.get_actor_location().length() > 0.01:
            issues.append("not at origin " + row["label"])
        box = comp.static_mesh.get_bounding_box()
        got = [[box.min.x, box.min.y, box.min.z], [box.max.x, box.max.y, box.max.z]]
        body_bounds = max(body_bounds, max(abs(a - b) for g, e in zip(got, row["bounds_cm"]) for a, b in zip(g, e)))
    if body_bounds > 0.1:
        issues.append("body bounds %.4f" % body_bounds)
    extra = [a.get_actor_label() for a in top_actors() if TAG in [str(t) for t in a.tags]
             and a.get_actor_label() not in {r["label"] for r in data["placements"] + data.get("bodies", [])}]
    mats = {}
    for actor in top_actors():
        if actor.get_actor_label() in PRESERVED:
            comp = actor.static_mesh_component
            mats[actor.get_actor_label()] = {
                "materials": [layout.test.package(m) for m in comp.get_materials()],
                "collision": str(comp.get_collision_enabled())}
    report = {"level": layout.world_package(), "expected": data["work_level"], "placements": len(data["placements"]),
              "issues": issues, "extra": extra, "max_position_cm": worst[0], "max_axis": worst[1],
              "max_scale": worst[2], "bodies": len(data.get("bodies", [])), "body_bounds_error_cm": body_bounds,
              "preserved": mats, "dirty": layout.test.dirty(),
              "pie": unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()}
    report["pass"] = (report["level"] == report["expected"] and not issues and not extra
                      and worst[0] < 0.05 and worst[1] < 1e-4)
    write("Verify_%s_%s.json" % (version, stamp()), report)
    unreal.log("RIDGE_ISLE_SKIN_VERIFY %s" % json.dumps({k: report[k] for k in ("pass", "issues", "extra", "dirty")}))


SURFACE_SOURCE = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/Z1MassifSurface_V003/"
ISLE_SURFACE = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/IsleSkin_V001/Surface_{0}/"
# 지각 재질(Surface V003)은 메시 위치로 이음매 거리를 재서 55m 안을 평지색(보라)으로 남긴다(GX-025).
# 부유섬·석교·동굴은 이음매 지각이 아니므로 구역 판정만 1로 두고 경사 판정은 그대로 쓴다.
ISLE_SCOPE = """
return float2(1.0, smoothstep(0.3420201, 0.8660254, Data.z));
"""


def material(version):
    """Surface V003 부모·MI를 복제해 구역 Custom 노드 코드만 바꾼 내부 구조물용 재질을 만든다."""
    assets, edit = unreal.EditorAssetLibrary, unreal.MaterialEditingLibrary
    assert not layout.test.dirty(), layout.test.dirty()
    folder = ISLE_SURFACE.format(version)
    instance_path = folder + "MI_Ridge_IsleSurface"
    assert not assets.does_asset_exist(instance_path), instance_path
    parent = assets.duplicate_asset(SURFACE_SOURCE + "M_Ridge_Z1_MassifSurface", folder + "M_Ridge_IsleSurface")
    assert parent
    scope, = [node for node in edit.get_material_expressions(parent) if isinstance(node, unreal.MaterialExpressionCustom)
              and "step(5500.0, seam)" in node.get_editor_property("code")]
    scope.set_editor_property("code", ISLE_SCOPE)
    assert not edit.recompile_material(parent), "Isle surface material compilation failed."
    instance = assets.duplicate_asset(SURFACE_SOURCE + "MI_Ridge_Z1_MassifSurface", instance_path)
    assert instance
    edit.set_material_instance_parent(instance, parent)
    edit.update_material_instance(instance)
    assert assets.save_loaded_asset(parent, False) and assets.save_loaded_asset(instance, False)
    write("Material_%s_%s.json" % (version, stamp()), {"instance": instance_path, "scope_code": ISLE_SCOPE,
                                                     "source": SURFACE_SOURCE})
    unreal.log("RIDGE_ISLE_SURFACE_READY " + instance_path)


if __name__ == "__main__":
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first"
    command, version = sys.argv[1:]
    {"build": build, "apply": apply, "verify": verify, "material": material}[command](version)
