"""ElectricDreams_PCGCloseRange의 PCG 생성 결과를 그룹별 일반 레벨로 굽는다(ElectricDreamsEnv 에디터 전용).

CloseRange는 World Partition 맵이라 Migrate 목록에 외부 액터가 실리지 않는다. 그래서 ED에서 PCG가 만든
인스턴스를 메시별 ISM 컴포넌트로 옮겨 담은 일반 레벨을 만들고, 그 레벨을 LootNPop으로 Migrate한다.
모든 그룹은 같은 원점(LargeAssembly 액터 위치)을 기준으로 저장하므로 같은 변환으로 놓으면 원래 배치가 된다.

사용법(ED 에디터 Cmd 입력창):
  py "<이 파일>" bake     CloseRange에서 읽어 /Game/LNPBake/CloseRange/CR_* 레벨 5개를 만든다.
  py "<이 파일>" verify   구운 레벨을 다시 열어 메시별 인스턴스 수를 원본 기록과 대조한다.
"""
import json
import sys
from pathlib import Path

import unreal

OUT = Path(r"D:/UnrealProjects/LootNPop/Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119")
CLOSE = "/Game/Levels/PCG/ElectricDreams_PCGCloseRange"
BAKE = "/Game/LNPBake/CloseRange/CR_{}"
# LargeAssembly 액터 위치(스플라인 높이). 굽힌 레벨의 원점이 된다.
ORIGIN = unreal.Vector(90250.0, -4360.0, 1660.0)
GROUPS = {
    "LargeAssembly": ("BP_PCG_LargeAssembly_C",),
    "SmallAssembly": ("BP_PCG_SmallAssembly_C",),
    "Ground": ("PCGDemo_GroundBP_C",),
    "Ditch": ("PCGDemo_DitchBP_C",),
    "Props": ("Rock_A_BP_C", "Rock_B_BP_C", "Rock_D_BP_C", "MI_Dirt_C", "MI_Leakage_8k_C"),
}
COPY_PROPS = ("cast_shadow", "instance_start_cull_distance", "instance_end_cull_distance",
              "world_position_offset_disable_distance", "evaluate_world_position_offset", "visible_in_ray_tracing",
              "affect_distance_field_lighting", "affect_dynamic_indirect_lighting", "receives_decals",
              "translucency_sort_priority", "cast_contact_shadow", "cast_far_shadow")
RECORD = OUT / "EDCloseRangeBake.json"


def pkg(obj):
    return obj.get_path_name().split(".")[0] if obj else None


def dirty():
    s = unreal.EditorLoadingAndSavingUtils
    return [p.get_path_name() for p in list(s.get_dirty_map_packages()) + list(s.get_dirty_content_packages())]


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def group_of(actor):
    cls = actor.get_class().get_name()
    for name, classes in GROUPS.items():
        if cls in classes:
            return name
    return None


def collect():
    # 그룹 → 컴포넌트 기록 목록. 인스턴스 변환은 ORIGIN 기준 상대 위치로 저장한다.
    groups = {name: {"components": [], "decals": []} for name in GROUPS}
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor):
        name = group_of(actor)
        if not name:
            continue
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            mesh = comp.static_mesh
            if not mesh or not comp.is_visible() or comp.get_editor_property("is_editor_only"):
                continue
            ism = isinstance(comp, unreal.InstancedStaticMeshComponent)
            poses = [comp.get_instance_transform(i, True) for i in range(comp.get_instance_count())] if ism \
                else [comp.get_world_transform()]
            if not poses:
                continue
            custom_count = comp.get_editor_property("num_custom_data_floats") if ism else 0
            custom = list(comp.get_editor_property("per_instance_sm_custom_data")) if custom_count else []
            props = {}
            for prop in COPY_PROPS:
                try:
                    props[prop] = comp.get_editor_property(prop)
                except Exception:
                    pass
            groups[name]["components"].append({
                "owner": actor.get_actor_label(), "component": comp.get_name(),
                "class": "HISM" if isinstance(comp, unreal.HierarchicalInstancedStaticMeshComponent) else "ISM",
                "mesh": mesh, "overrides": list(comp.get_editor_property("override_materials")),
                "collision_profile": str(comp.get_collision_profile_name()),
                "collision_enabled": comp.get_collision_enabled(), "props": props,
                "transforms": [unreal.Transform(p.translation - ORIGIN, p.rotation.rotator(), p.scale3d) for p in poses],
                "custom_count": custom_count, "custom": custom})
        for comp in actor.get_components_by_class(unreal.DecalComponent):
            t = comp.get_world_transform()
            groups[name]["decals"].append({"owner": actor.get_actor_label(), "material": comp.get_decal_material(),
                                           "size": comp.get_editor_property("decal_size"),
                                           "transform": unreal.Transform(t.translation - ORIGIN, t.rotation.rotator(), t.scale3d),
                                           "sort_order": comp.get_editor_property("sort_order")})
    return groups


def add_component(actor, cls, parent_handle):
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    params = unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=cls, blueprint_context=None)
    handle, fail = sds.add_new_subobject(params)
    assert not str(fail), fail
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    return handle, lib.get_object(lib.get_data(handle))


def build_group(name, data):
    path = BAKE.format(name)
    assert not unreal.EditorAssetLibrary.does_asset_exist(path), path
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level_editor.new_level(path, False), path
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    counts = {}
    if data["components"]:
        actor = actors.spawn_actor_from_class(unreal.Actor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        actor.set_actor_label("CR_" + name)
        actor_handle = sds.k2_gather_subobject_data_for_instance(actor)[0]
        root_handle, root = add_component(actor, unreal.SceneComponent, actor_handle)
        root.set_mobility(unreal.ComponentMobility.STATIC)
        for row in data["components"]:
            cls = unreal.HierarchicalInstancedStaticMeshComponent if row["class"] == "HISM" else unreal.InstancedStaticMeshComponent
            _, comp = add_component(actor, cls, root_handle)
            comp.set_mobility(unreal.ComponentMobility.STATIC)
            comp.set_static_mesh(row["mesh"])
            for i, material in enumerate(row["overrides"]):
                if material:
                    comp.set_material(i, material)
            for prop, value in row["props"].items():
                comp.set_editor_property(prop, value)
            comp.set_collision_profile_name(row["collision_profile"])
            comp.set_collision_enabled(row["collision_enabled"])
            comp.add_instances(row["transforms"], False, False)
            if row["custom_count"]:
                comp.set_num_custom_data_floats(row["custom_count"])
                k = row["custom_count"]
                for i in range(len(row["transforms"])):
                    for j in range(k):
                        comp.set_custom_data_value(i, j, row["custom"][i * k + j], False)
            assert comp.get_instance_count() == len(row["transforms"]), row["component"]
            key = pkg(row["mesh"])
            counts[key] = counts.get(key, 0) + len(row["transforms"])
    for row in data["decals"]:
        t = row["transform"]
        decal = actors.spawn_actor_from_class(unreal.DecalActor, t.translation, t.rotation.rotator())
        decal.set_actor_scale3d(t.scale3d)
        decal.decal.set_decal_material(row["material"])
        decal.decal.set_editor_property("decal_size", row["size"])
        decal.decal.set_editor_property("sort_order", row["sort_order"])
        decal.set_actor_label("CR_Decal_" + row["owner"])
        key = "decal:" + pkg(row["material"])
        counts[key] = counts.get(key, 0) + 1
    assert level_editor.save_current_level(), path
    return {"level": path, "components": len(data["components"]), "decals": len(data["decals"]),
            "instances": sum(len(r["transforms"]) for r in data["components"]), "by_mesh": counts}


def bake():
    assert Path(unreal.Paths.project_dir()).resolve().name == "ElectricDreamsEnv"
    if pkg(world()) != CLOSE:
        assert not dirty(), dirty()
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(CLOSE)
    assert not dirty(), dirty()
    groups = collect()
    source = {}
    for name, data in groups.items():
        by_mesh = {}
        for row in data["components"]:
            key = pkg(row["mesh"])
            by_mesh[key] = by_mesh.get(key, 0) + len(row["transforms"])
        for row in data["decals"]:
            key = "decal:" + pkg(row["material"])
            by_mesh[key] = by_mesh.get(key, 0) + 1
        source[name] = by_mesh
    assert not dirty(), dirty()
    built = {name: build_group(name, data) for name, data in groups.items()}
    report = {"source_map": CLOSE, "origin_cm": [ORIGIN.x, ORIGIN.y, ORIGIN.z], "source": source, "built": built}
    RECORD.write_text(json.dumps(report, indent=1), encoding="utf-8")
    unreal.log("ED_CLOSE_BAKE_READY " + " ".join("%s=%d" % (n, b["instances"]) for n, b in built.items()))


def verify():
    report = json.loads(RECORD.read_text(encoding="utf-8"))
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    result = {}
    for name in GROUPS:
        path = BAKE.format(name)
        assert not dirty(), dirty()
        assert level_editor.load_level(path), path
        counts = {}
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor):
            for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
                key = pkg(comp.static_mesh)
                counts[key] = counts.get(key, 0) + comp.get_instance_count()
            if isinstance(actor, unreal.DecalActor):
                key = "decal:" + pkg(actor.decal.get_decal_material())
                counts[key] = counts.get(key, 0) + 1
        result[name] = {"match": counts == report["source"][name], "instances": sum(counts.values()),
                        "diff": {k: [report["source"][name].get(k), counts.get(k)]
                                 for k in set(counts) | set(report["source"][name])
                                 if counts.get(k) != report["source"][name].get(k)}}
    report["verify"] = result
    RECORD.write_text(json.dumps(report, indent=1), encoding="utf-8")
    unreal.log("ED_CLOSE_VERIFY_READY " + " ".join("%s=%s" % (n, r["match"]) for n, r in result.items()))


if __name__ == "__main__":
    {"bake": bake, "verify": verify}[sys.argv[1]]()
