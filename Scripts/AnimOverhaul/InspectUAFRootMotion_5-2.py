"""Phase 5 §42: 에셋 이동량과 현재 시험 폰 연결을 읽는 인계용 진단.

에디터 Output Log에서 py "D:/UnrealProjects/LootNPop/Scripts/AnimOverhaul/InspectUAFRootMotion_5-2.py"로 실행한다.
PIE가 있으면 현재 시험 폰도 읽는다. 에셋 저장/몽타주 재생/캐릭터 이동은 하지 않는다.
Inspection.json은 실행마다 덮어쓰므로 비교할 Idle/몽타주 결과는 별도 사본으로 보존한다.
"""
import json
from pathlib import Path
import unreal

report = {"scope": "Read-only section42 investigation; not a movement gate verdict"}
folder = "/Game/Characters/UAFSpike/LyraWeaponContract/"
paths = [folder + "AM_MM_Dash_" + name + "_LyraFrame" for name in ("Forward", "Backward", "Left", "Right")]
paths.append("/Game/Characters/Retargeted/Lyra/AM_SW_Attack_01_UEFN")
rows = []
for path in paths:
    montage = unreal.load_asset(path)
    if montage is None:
        raise RuntimeError("Missing montage: " + path)
    length = montage.get_play_length()
    maximum = {"distanceCm": 0.0, "start": 0.0, "end": 0.0}
    for index in range(int(length * 30) + 1):
        start = min(index / 30.0, length)
        end = min(start + 0.2, length)
        delta = unreal.MotionWarpingUtilities.extract_root_motion_from_animation(montage, start, end)
        distance = delta.translation.length()
        if distance > maximum["distanceCm"]:
            maximum = {"distanceCm": distance, "start": start, "end": end}
    total = unreal.MotionWarpingUtilities.extract_root_motion_from_animation(montage, 0.0, length)
    rows.append({"montage": path, "length": length, "netTranslationCm": total.translation.length(),
                 "maximumWindow0_2s": maximum,
                 "slots": [str(track.get_editor_property("slot_name")) for track in montage.get_editor_property("slot_anim_tracks")]})
report["montages"] = rows
module = unreal.load_asset("/Game/Characters/UAFSpike/S_LocomotionSpike")
graphs = []
for graph in unreal.ObjectIterator(unreal.RigVMGraph):
    if graph.get_path_name().startswith(module.get_path_name() + ":"):
        units = [node.get_script_struct().get_name() for node in graph.get_nodes()
                 if isinstance(node, unreal.RigVMUnitNode) and node.get_script_struct()]
        graphs.append({"graph": graph.get_path_name(), "units": units})
report["moduleGraphs"] = graphs
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
report["runtime"] = []
if world:
    tag = unreal.GameplayTag()
    tag.import_text('(TagName="Mover.AnimRootMotion.MeshAttribute")')
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.LNPUAFSpikePlayerCharacter):
        mover = actor.get_editor_property("mover_component")
        primary = mover.get_primary_visual_component()
        source = actor.get_editor_property("anim_source_mesh")
        anim = source.get_anim_instance()
        active = anim.get_current_active_montage()
        report["runtime"].append({"actor": actor.get_path_name(), "primaryVisual": primary.get_path_name(),
                                  "sourceMesh": source.get_path_name(), "sameMesh": primary == source,
                                  "backend": mover.get_editor_property("backend_class").get_path_name(),
                                  "rootMotionAttributeFeature": mover.has_features_with_tag(tag, True),
                                  "activeMontage": active.get_path_name() if active else None,
                                  "montagePlaying": anim.montage_is_playing(active) if active else False,
                                  "montagePosition": anim.montage_get_position(active) if active else None,
                                  "defaultSlotActive": anim.is_slot_active("DefaultSlot"),
                                  "fullBodySlotActive": anim.is_slot_active("FullBody"),
                                  "rootTranslationMode": str(anim.get_offset_root_translation_mode()),
                                  "rootRotationMode": str(anim.get_offset_root_rotation_mode())})
output = Path(unreal.Paths.project_saved_dir()) / "AnimationOverhaul/2026-11-02_UAF52RootMotion/Inspection.json"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, indent=2), encoding="utf-8")
print("[UAFRootMotionInspection] " + str(output))
