"""UAF 라이플 시험의 무기 부착 기준·위치와 장전 선택 사본을 재생성한다(Phase 5 §37).

PIE 종료 후 에디터 Python에서 실행한다. UAFSpike/WeaponBoneProbe의 리타기터·몽타주와
Retargeted/UAFWeaponProbe의 Idle/Reload, CHT_MontageRifleSpike만 생성·갱신/저장한다.
BP의 MontageChooser 연결은 보고서 확인 후 따로 적용한다. 원본/공유 VS·Chooser·맵은 저장하지 않는다.
기존 시퀀스는 재사용/검증하며 삭제·교체하지 않는다. 다른 값으로 다시 생성하려면 새 출력 경로를 정하고
AG 참조를 재연결/Compile해야 한다. 배치 overwrite는 기존 UObject를 강제 삭제해 UAF 참조를 무효화한다.
"""
import json
import math
from pathlib import Path
import unreal

if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
    raise RuntimeError("Stop PIE before updating UAF rifle attachment assets.")

AL = unreal.EditorAssetLibrary
FOLDER = "/Game/Characters/UAFSpike/WeaponBoneProbe"
GENERATED = "/Game/Characters/Retargeted/UAFWeaponProbe"
RTG = FOLDER + "/RTG_RifleAttachmentProbe"
MONTAGE = FOLDER + "/AM_RifleReloadAttachmentProbe"
CHOOSER = "/Game/Characters/UAFSpike/CHT_MontageRifleSpike"
BASE_RTG = "/Game/Characters/Retargeters/RTG_Lyra_to_UEFN_Offline"
BASE_MONTAGE = "/Game/Characters/Retargeted/Lyra/AM_MM_Rifle_Reload_UEFN"
OFFSET = unreal.Vector(-8, 3, -3)
NAMES = ("MM_Rifle_Idle_ADS", "MM_Rifle_Reload")
SOURCE_PATHS = ("/Game/Characters/Lyra_Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Idle_ADS",
                "/Game/Characters/Lyra_Mannequin/Animations/Actions/MM_Rifle_Reload")
sequence_paths = [GENERATED + "/" + name + "_AttachmentProbe" for name in NAMES]
existing = [AL.does_asset_exist(path) for path in sequence_paths]
if any(existing) and not all(existing):
    raise RuntimeError("Partial spike output exists. Use new output paths; refusing to replace existing sequence objects.")
sequence_objects_before = [unreal.load_asset(path) for path in sequence_paths] if all(existing) else []


def own_copy(path, source):
    asset = unreal.load_asset(path) if AL.does_asset_exist(path) else AL.duplicate_asset(source, path)
    if asset is None:
        raise RuntimeError("Could not load or create spike asset: " + path)
    return asset


def angle(a, b):
    av = [getattr(a.rotation, axis) for axis in ("x", "y", "z", "w")]
    bv = [getattr(b.rotation, axis) for axis in ("x", "y", "z", "w")]
    dot = abs(sum(x*y for x, y in zip(av, bv))) / math.sqrt(sum(x*x for x in av)*sum(y*y for y in bv))
    return math.degrees(2*math.acos(min(1.0, dot)))


retargeter = own_copy(RTG, BASE_RTG)
rc = unreal.IKRetargeterController.get_controller(retargeter)
pins = [rc.get_op_controller(i) for i in range(rc.get_num_retarget_ops())
        if hasattr(rc.get_op_controller(i), "get_all_bone_pairs")]
if not pins:
    pins = [rc.get_op_controller(rc.add_retarget_op("/Script/IKRig.IKRetargetPinBoneOp"))]
if len(pins) != 1:
    raise RuntimeError("Expected exactly one Pin Bones op in the spike retargeter.")
pin = pins[0]
retargeter.modify()
pin.set_bone_pair("hand_r", "weapon_r")
if {str(k): str(v) for k, v in pin.get_all_bone_pairs().items()} != {"weapon_r": "hand_r"}:
    raise RuntimeError("Unexpected spike attachment bone pairs.")
settings = pin.get_settings()
if settings.get_editor_property("skeleton_to_copy_from") != unreal.RetargetSourceOrTarget.TARGET:
    raise RuntimeError("Pin Bones must copy from the target skeleton.")
# 사용자가 PIE에서 정한 최종 상대 위치를 대상 손 기준으로 저장한다.
settings.set_editor_property("local_offset", unreal.Transform(OFFSET, unreal.Rotator(0, 0, 0), unreal.Vector(1, 1, 1)))
pin.set_settings(settings)
target_mesh = unreal.load_asset("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin")
created = []
if not any(existing):
    source_mesh = unreal.load_asset("/Game/Characters/Lyra_Mannequin/Meshes/SKM_Manny")
    inputs = unreal.IKRetargetBatchOperationInputs()
    for key, value in {"assets_to_retarget": [AL.find_asset_data(path) for path in SOURCE_PATHS],
                       "source_mesh": source_mesh, "target_mesh": target_mesh,
                       "ik_retarget_asset": retargeter, "target_path": GENERATED,
                       "use_source_path": False, "suffix": "_AttachmentProbe",
                       "include_referenced_assets": False, "overwrite_existing_files": False,
                       "retain_additive_flags": True}.items():
        inputs.set_editor_property(key, value)
    created = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    if len(created) != 2:
        raise RuntimeError("Expected exactly two newly generated UAF rifle sequences.")
sequences = [unreal.load_asset(path) for path in sequence_paths]
if any(sequence is None for sequence in sequences):
    raise RuntimeError("Expected exactly two UAF rifle sequences.")
if sequence_objects_before and any(before != after for before, after in zip(sequence_objects_before, sequences)):
    raise RuntimeError("Existing sequence object identity changed unexpectedly.")
pe = unreal.AnimPoseExtensions
options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property("optional_skeletal_mesh", target_mesh)
options.set_editor_property("evaluation_type", unreal.AnimDataEvalType.COMPRESSED)
checks = []
for sequence in sequences:
    count = int(round(sequence.get_play_length()*30)) + 1
    max_rotation = max_position = 0.0
    for step in range(count):
        pose = pe.get_anim_pose_at_time(sequence, sequence.get_play_length()*step/(count-1), options)
        hand = pe.get_bone_pose(pose, "hand_r", unreal.AnimPoseSpaces.WORLD)
        weapon = pe.get_bone_pose(pose, "weapon_r", unreal.AnimPoseSpaces.WORLD)
        max_rotation = max(max_rotation, angle(weapon, hand))
        max_position = max(max_position, (weapon.translation-hand.transform_location(OFFSET)).length())
    checks.append({"sequence": sequence.get_path_name(), "samples": count,
                   "maxRotationErrorDeg": max_rotation, "maxPositionErrorCm": max_position})
    if max_rotation > 0.01 or max_position > 0.01:
        raise RuntimeError("Existing or generated offset does not match: " + sequence.get_name() + ". Use new output paths and rebind the graph; existing sequences will not be overwritten.")
montage = own_copy(MONTAGE, BASE_MONTAGE)
montage.modify()
tracks = montage.get_editor_property("slot_anim_tracks")
changed = 0
for index, track in enumerate(tracks):
    if str(track.get_editor_property("slot_name")) != "UpperBody":
        continue
    anim_track = track.get_editor_property("anim_track")
    segments = anim_track.get_editor_property("anim_segments")
    if len(segments) != 1:
        raise RuntimeError("Unexpected spike UpperBody track layout.")
    segment = segments[0]
    segment.set_editor_property("anim_reference", sequences[1])
    segments[0] = segment
    anim_track.set_editor_property("anim_segments", segments)
    track.set_editor_property("anim_track", anim_track)
    tracks[index] = track
    changed += 1
if changed != 1:
    raise RuntimeError("Expected one spike UpperBody track.")
montage.set_editor_property("slot_anim_tracks", tracks)
base = [track for track in montage.get_editor_property("slot_anim_tracks")
        if str(track.get_editor_property("slot_name")) == "UpperBody"][0]
if base.get_editor_property("anim_track").get_editor_property("anim_segments")[0].get_editor_property("anim_reference") != sequences[1]:
    raise RuntimeError("Spike montage reference validation failed.")
chooser = own_copy(CHOOSER, "/Game/Characters/CHT_Montage")
unreal.SystemLibrary.execute_console_command(None, "LNP.Editor.ReplaceAssetRefs " + CHOOSER + " " + BASE_MONTAGE + "=" + MONTAGE)
for asset in [retargeter, montage, chooser] + sequences:
    if not AL.save_loaded_asset(asset):
        raise RuntimeError("Could not save spike asset " + asset.get_path_name())
registry = unreal.AssetRegistryHelpers.get_asset_registry()
chooser_file = Path(unreal.Paths.project_content_dir()).resolve() / (CHOOSER.removeprefix("/Game/") + ".uasset")
registry.scan_files_synchronous([str(chooser_file)], True)
dependencies = [str(path) for path in (registry.get_dependencies(CHOOSER, unreal.AssetRegistryDependencyOptions(include_hard_package_references=True)) or [])]
if MONTAGE not in dependencies or BASE_MONTAGE in dependencies:
    raise RuntimeError("Spike chooser reload reference validation failed; BP was not changed.")
report = {"offsetHandSpaceCm": [-8, 3, -3], "pinSettings": pin.get_settings().export_text(),
          "reusedExistingSequences": all(existing), "newSequenceCount": len(created),
          "reusedSequenceObjectsUnchanged": bool(sequence_objects_before),
          "saved": [asset.get_path_name() for asset in [retargeter, montage, chooser] + sequences],
          "chooserDependencies": dependencies, "checks": checks,
          "scope": "Spike assets only; shared VS/chooser, source assets, BP and map unchanged"}
output = Path(unreal.Paths.project_saved_dir()).resolve() / "AnimationOverhaul/2026-10-29_UAF52RifleOverlay/RifleAttachmentPersistent_2026-11-01.json"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, indent=2), encoding="utf-8")
print("[UAFRifleAttachment] Saved " + str(output))
