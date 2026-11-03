# 5-2 ⓓ 적 최소 UAF 경로 준비용 읽기 전용 조회.
# 플레이어 시험 BP와 승격 적 BP의 컴포넌트 구성, 적 Config·EntityConfig·스폰 설정을 덤프한다.
# 아무것도 저장하지 않으며 Saved/AnimationOverhaul/2026-11-03_UAF52EnemyPath/Setup.json만 쓴다.
import json
import os

import unreal

OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "AnimationOverhaul", "2026-11-03_UAF52EnemyPath")
BLUEPRINTS = [
    "/Game/Characters/UAFSpike/BP_PlayerUAFSpike",
    "/Game/Enemy/BP_LNPEnemy",
]
ASSETS = [
    "/Game/Enemy/DA_Enemy_ActorPromoted_Melee01",
    "/Game/Enemy/DA_EnemyEntityConfig_ActorPromoted_Melee01",
    "/Game/SpawnData/DA_MassSpawnConfig",
]
COMPONENT_PROPS = [
    "skeletal_mesh_asset", "anim_class", "enable_animation", "visibility_based_anim_tick_option",
    "disable_post_process_blueprint", "hidden_in_game", "visible", "primary_component_tick",
    "system", "auto_activate", "relative_location", "relative_rotation",
]


def to_data(value, depth=0):
    if depth > 6:
        return str(value)
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, (list, tuple, unreal.Array)):
        return [to_data(v, depth + 1) for v in value]
    if isinstance(value, unreal.Map):
        return {str(k): to_data(v, depth + 1) for k, v in value.items()}
    if isinstance(value, (unreal.Object,)):
        return value.get_path_name()
    if isinstance(value, unreal.StructBase):
        result = {"_type": type(value).__name__}
        for name in dir(value):
            if name.startswith("_"):
                continue
            try:
                field = value.get_editor_property(name)
            except Exception:
                continue
            result[name] = to_data(field, depth + 1)
        return result
    return str(value)


def dump_object(obj, names=None):
    result = {"_class": obj.get_class().get_path_name()}
    for name in names if names is not None else dir(obj):
        if name.startswith("_"):
            continue
        try:
            value = obj.get_editor_property(name)
        except Exception:
            continue
        result[name] = to_data(value)
    return result


def dump_blueprint(path):
    blueprint = unreal.load_asset(path)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    components = []
    for handle in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if obj is None:
            continue
        entry = {"name": str(unreal.SubobjectDataBlueprintFunctionLibrary.get_variable_name(data)),
                 "class": obj.get_class().get_path_name(), "path": obj.get_path_name()}
        entry.update(dump_object(obj, COMPONENT_PROPS))
        components.append(entry)
    return {"components": components}


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    report = {"blueprints": {}, "assets": {}}
    for path in BLUEPRINTS:
        try:
            report["blueprints"][path] = dump_blueprint(path)
        except Exception as error:
            report["blueprints"][path] = {"error": str(error)}
    for path in ASSETS:
        try:
            report["assets"][path] = dump_object(unreal.load_asset(path))
        except Exception as error:
            report["assets"][path] = {"error": str(error)}
    with open(os.path.join(OUT_DIR, "Setup.json"), "w", encoding="utf-8") as file:
        json.dump(report, file, ensure_ascii=False, indent=1)
    unreal.log("[InspectEnemyUAFSetup] wrote " + os.path.join(OUT_DIR, "Setup.json"))


main()
