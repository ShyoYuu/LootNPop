# 5-2 ⓓ 적 최소 UAF 시험 에셋 생성(새 경로에만 쓴다). 원본 BP·EntityConfig·스폰 설정은 저장하지 않는다.
# 1) BP_LNPEnemy → BP_EnemyUAFSpike(부모 ALNPUAFSpikeEnemyCharacter, 원본 공급층 ABP, 표시 메시 애니 끔)
# 2) DA_EnemyEntityConfig_ActorPromoted_Melee01 → DA_EnemyEntityConfig_UAFSpike_Melee01(템플릿 Actor 교체)
# 3) DA_MassSpawnConfig_UAFSpikeEnemy(Pod마다 UAF 적 3, 맵 저작 편성이 참조하는 다섯 종은 0명 행으로 둔다)
# UAF 컴포넌트(UAFLocomotion)는 사용자가 BP_PlayerUAFSpike에서 복사/붙여넣기한다(설정 그대로 옮기기 위해).
# 결과는 Saved/AnimationOverhaul/2026-11-03_UAF52EnemyPath/SetupAssets.json에 남긴다.
import json
import os

import unreal

OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "AnimationOverhaul", "2026-11-03_UAF52EnemyPath")
SPIKE_DIR = "/Game/Characters/UAFSpike"
SOURCE_BP = "/Game/Enemy/BP_LNPEnemy"
SPIKE_BP = SPIKE_DIR + "/BP_EnemyUAFSpike"
SOURCE_ENTITY = "/Game/Enemy/DA_EnemyEntityConfig_ActorPromoted_Melee01"
SPIKE_ENTITY = SPIKE_DIR + "/DA_EnemyEntityConfig_UAFSpike_Melee01"
SPIKE_SPAWN = SPIKE_DIR + "/DA_MassSpawnConfig_UAFSpikeEnemy"
SOURCE_ABP = SPIKE_DIR + "/ABP_SourceUAFSpike.ABP_SourceUAFSpike_C"
LOOT_POD = "/Game/LootPod/DA_LootPodEntityConfig_Bench01"
POD_SET_COUNT = 60
AUTHORED_ENEMIES = [
    SOURCE_ENTITY,
    "/Game/Enemy/DA_EnemyEntityConfig_PureEntity_Melee01",
    "/Game/Enemy/DA_EnemyEntityConfig_PureEntity_Ranged01",
    "/Game/Enemy/DA_EnemyEntityConfig_PureEntity_Crawler01",
    "/Game/Enemy/DA_EnemyEntityConfig_PureEntity_Flyer01",
]

report = {"steps": []}


def log(message):
    report["steps"].append(message)
    unreal.log("[SetupEnemyUAFSpike] " + message)


def duplicate(source, target):
    if unreal.EditorAssetLibrary.does_asset_exist(target):
        log("reuse " + target)
        return unreal.load_asset(target)
    asset = unreal.EditorAssetLibrary.duplicate_asset(source, target)
    log("duplicate %s -> %s : %s" % (source, target, asset is not None))
    return asset


def component_templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    result = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if obj is not None:
            result[str(unreal.SubobjectDataBlueprintFunctionLibrary.get_variable_name(data))] = obj
    return result


def setup_blueprint():
    # 이미 있으면 그대로 쓴다 — 사용자가 붙여넣은 UAFLocomotion 컴포넌트를 다시 컴파일·저장하지 않는다.
    if unreal.EditorAssetLibrary.does_asset_exist(SPIKE_BP):
        log("reuse " + SPIKE_BP)
        return unreal.load_asset(SPIKE_BP)
    blueprint = duplicate(SOURCE_BP, SPIKE_BP)
    parent = unreal.load_class(None, "/Script/LootNPop.LNPUAFSpikeEnemyCharacter")
    unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, parent)
    log("reparent -> " + parent.get_path_name())
    components = component_templates(blueprint)
    source = components["AnimSourceMesh"]
    source.set_editor_property("anim_class", unreal.load_class(None, SOURCE_ABP))
    source.set_editor_property("enable_animation", False)
    visual = components["VisualMesh"]
    visual.set_editor_property("enable_animation", False)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    log("blueprint saved; components=" + ",".join(sorted(components)))
    report["blueprint"] = {
        "AnimSourceMesh.anim_class": str(source.get_editor_property("anim_class")),
        "AnimSourceMesh.enable_animation": source.get_editor_property("enable_animation"),
        "VisualMesh.enable_animation": visual.get_editor_property("enable_animation"),
        "VisualMesh.anim_class": str(visual.get_editor_property("anim_class")),
    }
    return blueprint


def setup_entity_config(blueprint):
    if unreal.EditorAssetLibrary.does_asset_exist(SPIKE_ENTITY):
        log("reuse " + SPIKE_ENTITY)
        return unreal.load_asset(SPIKE_ENTITY)
    entity = duplicate(SOURCE_ENTITY, SPIKE_ENTITY)
    actor_class = blueprint.generated_class()
    for trait in entity.get_editor_property("config").get_editor_property("traits"):
        if isinstance(trait, unreal.MassCrowdVisualizationTrait):
            trait.set_editor_property("high_res_template_actor", actor_class)
            trait.set_editor_property("low_res_template_actor", actor_class)
            log("visualization trait -> " + actor_class.get_path_name())
    unreal.EditorAssetLibrary.save_loaded_asset(entity)
    return entity


def setup_spawn_config(entity):
    if unreal.EditorAssetLibrary.does_asset_exist(SPIKE_SPAWN):
        spawn = unreal.load_asset(SPIKE_SPAWN)
        log("reuse " + SPIKE_SPAWN)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.load_class(None, "/Script/LootNPop.LNPMassSpawnConfig"))
        spawn = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "DA_MassSpawnConfig_UAFSpikeEnemy", SPIKE_DIR, None, factory)
        log("create " + SPIKE_SPAWN)
    def entry(config, count):
        result = unreal.LNPEnemySpawnEntry()
        result.set_editor_property("enemy_entity_config", config)
        result.set_editor_property("count", count)
        return result

    # 맵의 저작 앵커 편성이 참조하는 적 종류는 모두 행이 있어야 스폰 계획이 통과한다(0명이어도 된다).
    # 기존 ABP 승격 적도 0명이다 — 화면에서 두 경로를 구분할 수 없어 육안 판정은 UAF 적만 늘린다.
    entries = [entry(entity, 3)] + [entry(unreal.load_asset(path), 0) for path in AUTHORED_ENEMIES]
    pod_set = unreal.LNPLootPodSpawnEntry()
    pod_set.set_editor_property("spawn_set_id", "Bench01")
    pod_set.set_editor_property("loot_pod_entity_config", unreal.load_asset(LOOT_POD))
    pod_set.set_editor_property("associated_enemies", entries)
    pod_set.set_editor_property("pod_set_count", POD_SET_COUNT)
    spawn.set_editor_property("loot_pod_spawn_sets", [pod_set])
    spawn.set_editor_property("min_distance_between_pods", 2000.0)
    spawn.set_editor_property("enemy_spawn_radius_around_pod", 1500.0)
    unreal.EditorAssetLibrary.save_loaded_asset(spawn)
    log("spawn config saved: pods=%d uaf=3 base=0" % POD_SET_COUNT)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    try:
        blueprint = setup_blueprint()
        entity = setup_entity_config(blueprint)
        setup_spawn_config(entity)
        report["result"] = "ok"
    except Exception as error:
        report["result"] = "error: " + str(error)
        unreal.log_error("[SetupEnemyUAFSpike] " + str(error))
    with open(os.path.join(OUT_DIR, "SetupAssets.json"), "w", encoding="utf-8") as file:
        json.dump(report, file, ensure_ascii=False, indent=1)


main()
