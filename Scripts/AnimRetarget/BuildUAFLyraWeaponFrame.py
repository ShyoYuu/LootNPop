"""Phase 5 §37: Lyra 무기 본의 움직임을 보존하는 공통 손 기준축 후보를 생성한다.

PIE 종료 후 실행한다. 원본 리타기터 사본 끝에서 source hand_r의 애니메이션 회전만
target hand_r로 전달하고 자식을 함께 갱신한다. weapon_r의 원본 상대 운동을 보존한다.
새 폴더의 Idle/AO/장전/발사/대시 시퀀스를 생성·검증하며 BP/AG/Chooser는 바꾸지 않는다.
기존 산출물은 동일 설정일 때만 재사용한다. 배치 overwrite/기존 시퀀스 삭제는 금지한다.
"""
import json
import math
from pathlib import Path
import unreal

if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
    raise RuntimeError("Stop PIE before creating Lyra frame trial assets.")

AL = unreal.EditorAssetLibrary
PE = unreal.AnimPoseExtensions
ML = unreal.MathLibrary
BASE_RTG = "/Game/Characters/Retargeters/RTG_Lyra_to_UEFN_Offline"
RTG = "/Game/Characters/UAFSpike/LyraWeaponContract/RTG_LyraWeaponFrame"
DEST = "/Game/Characters/Retargeted/UAFLyraFrame"
SUFFIX = "_LyraFrame"
SOURCE_ROOT = "/Game/Characters/Lyra_Mannequin/Animations"
TARGET_ROOT = "/Game/Characters/Retargeted/Lyra"
INPUTS = [SOURCE_ROOT + "/" + name for name in (
    "Locomotion/Rifle/MM_Rifle_Idle_ADS", "AimOffsets/AO_MM_Rifle_Idle_ADS",
    "Actions/MM_Rifle_Reload", "Actions/MM_Rifle_Reload_Additive", "Actions/MM_Rifle_Fire",
    "Actions/MM_Dash_Forward", "Actions/MM_Dash_Backward", "Actions/MM_Dash_Left", "Actions/MM_Dash_Right")]


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing required asset: " + path)
    return asset


def package(asset):
    return asset.get_outermost().get_name()


def output(path):
    return DEST + "/" + path.rsplit("/", 1)[-1] + SUFFIX


def vec(value):
    return [float(getattr(value, axis)) for axis in ("x", "y", "z")]


def quat_angle(a, b):
    av = [getattr(a, axis) for axis in ("x", "y", "z", "w")]
    bv = [getattr(b, axis) for axis in ("x", "y", "z", "w")]
    dot = abs(sum(x*y for x, y in zip(av, bv))) / math.sqrt(sum(x*x for x in av)*sum(y*y for y in bv))
    return math.degrees(2*math.acos(min(1.0, dot)))


def angle(a, b):
    dot = sum(x*y for x, y in zip(vec(a), vec(b))) / (a.length()*b.length())
    return math.degrees(math.acos(max(-1.0, min(1.0, dot))))


sources = {}
queue = list(INPUTS)
while queue:
    asset = load(queue.pop())
    path = package(asset)
    if path in sources:
        continue
    sources[path] = asset
    if isinstance(asset, unreal.BlendSpace):
        queue.extend(package(sample.get_editor_property("animation")) for sample in asset.get_editor_property("sample_data")
                     if sample.get_editor_property("animation"))
    if isinstance(asset, unreal.AnimSequence) and asset.get_editor_property("ref_pose_seq"):
        queue.append(package(asset.get_editor_property("ref_pose_seq")))
if len({output(path) for path in sources}) != len(sources):
    raise RuntimeError("Source names collide in the candidate output folder.")
existing = [AL.does_asset_exist(output(path)) for path in sources]
if any(existing) and not all(existing):
    raise RuntimeError("Partial candidate outputs exist. Use new output paths; existing objects will not be overwritten.")
objects_before = {path: load(output(path)) for path in sources} if all(existing) else {}
source_mesh = load("/Game/Characters/Lyra_Mannequin/Meshes/SKM_Manny")
target_mesh = load("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin")
retargeter = load(RTG) if AL.does_asset_exist(RTG) else AL.duplicate_asset(BASE_RTG, RTG)
if retargeter is None:
    raise RuntimeError("Could not duplicate the candidate retargeter.")
rc = unreal.IKRetargeterController.get_controller(retargeter)
if quat_angle(rc.get_rotation_offset_for_retarget_pose_bone("hand_r", unreal.RetargetSourceOrTarget.TARGET),
              unreal.Quat(x=0.0, y=0.0, z=0.0, w=1.0)) > 0.001:
    raise RuntimeError("Unexpected target hand retarget-pose offset in this candidate.")
pins = [rc.get_op_controller(i) for i in range(rc.get_num_retarget_ops())
        if hasattr(rc.get_op_controller(i), "get_all_bone_pairs")]
if not pins and not any(existing):
    pins = [rc.get_op_controller(rc.add_retarget_op("/Script/IKRig.IKRetargetPinBoneOp"))]
if len(pins) != 1:
    raise RuntimeError("Expected exactly one candidate hand rotation transfer op.")
pin = pins[0]
rules = {"skeleton_to_copy_from": unreal.RetargetSourceOrTarget.SOURCE,
         "copy_translation": False, "copy_rotation": True, "copy_scale": False,
         "propagate_to_children": True,
         "rotation_mode": unreal.PinBoneRotationMode.COPY_GLOBAL_ROTATION}
if not any(existing):
    pin.clear_all_bone_pairs()
    pin.set_bone_pair("hand_r", "hand_r")
    settings = pin.get_settings()
    for key, value in rules.items():
        settings.set_editor_property(key, value)
    pin.set_settings(settings)
settings = pin.get_settings()
if ({str(k): str(v) for k, v in pin.get_all_bone_pairs().items()} != {"hand_r": "hand_r"}
        or any(settings.get_editor_property(key) != value for key, value in rules.items())):
    raise RuntimeError("Candidate rotation transfer differs. Use new output paths; existing sequences will not be replaced.")

created = []
if not any(existing):
    inputs = unreal.IKRetargetBatchOperationInputs()
    for key, value in {"assets_to_retarget": [AL.find_asset_data(path) for path in INPUTS],
                       "source_mesh": source_mesh, "target_mesh": target_mesh,
                       "ik_retarget_asset": retargeter, "target_path": DEST,
                       "use_source_path": False, "suffix": SUFFIX, "include_referenced_assets": True,
                       "overwrite_existing_files": False, "retain_additive_flags": True}.items():
        inputs.set_editor_property(key, value)
    created = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
copies = {path: load(output(path)) for path in sources}
if objects_before and any(copies[path] != objects_before[path] for path in sources):
    raise RuntimeError("Unexpected existing candidate object identity change.")

# retain_additive_flags가 되돌린 가산 기준 참조를 새 출력끼리 연결한다.
fixed = []
for path, asset in copies.items():
    if isinstance(asset, unreal.BlendSpace):
        if any(not package(sample.get_editor_property("animation")).startswith(DEST + "/")
               for sample in asset.get_editor_property("sample_data") if sample.get_editor_property("animation")):
            raise RuntimeError("Candidate AO sample points outside the candidate folder.")
    if not isinstance(asset, unreal.AnimSequence):
        continue
    reference = asset.get_editor_property("ref_pose_seq")
    if reference and package(reference) in copies:
        asset.modify()
        asset.set_editor_property("ref_pose_seq", copies[package(reference)])
        fixed.append(path)
    reference = asset.get_editor_property("ref_pose_seq")
    if reference and not package(reference).startswith(DEST + "/"):
        raise RuntimeError("Candidate additive reference points outside the candidate folder.")
# 배치의 참조 교체가 원본 메모리를 건드리는 엔진 경로에 대비해 원본 패키지만 재로드한다.
if created:
    packages = [unreal.find_package(path) for path in sources]
    unreal.EditorLoadingAndSavingUtils.reload_packages([item for item in packages if item], unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
sources = {path: load(path) for path in sources}
for asset in [retargeter] + list(copies.values()):
    if not AL.save_loaded_asset(asset):
        raise RuntimeError("Could not save candidate asset: " + asset.get_path_name())

options = {}
for name, mesh in (("source", source_mesh), ("target", target_mesh)):
    options[name] = unreal.AnimPoseEvaluationOptions()
    options[name].set_editor_property("optional_skeletal_mesh", mesh)
    options[name].set_editor_property("evaluation_type", unreal.AnimDataEvalType.COMPRESSED)
    options[name].set_editor_property("retrieve_additive_as_full_pose", True)
mount = unreal.Transform(unreal.Vector(0, 0, 0), unreal.Rotator(pitch=0, yaw=-90, roll=0), unreal.Vector(1, 1, 1))
grip_bones = ("hand_r", "index_01_r", "index_03_r", "middle_01_r", "thumb_02_r", "hand_l")


def pose_data(sequence, time, role):
    pose = PE.get_anim_pose_at_time(sequence, time, options[role])
    bone = lambda name: PE.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD)
    weapon = ML.compose_transforms(mount, bone("weapon_r"))
    return {"direction": bone("root").rotation.unrotate_vector(weapon.rotation.rotate_vector(unreal.Vector(0, 1, 0))),
            "grips": {name: weapon.inverse_transform_location(bone(name).translation) for name in grip_bones},
            "handPosition": bone("hand_r").translation}


checks = []
for path, source in sources.items():
    if not isinstance(source, unreal.AnimSequence):
        continue
    candidate = copies[path]
    baseline = load(TARGET_ROOT + "/" + source.get_name() + "_UEFN")
    direction_errors, right_grip_changes, hand_changes, left_grip_errors = [], [], [], []
    for fraction in (0.0, 0.25, 0.5, 0.75, 1.0):
        time = min(source.get_play_length(), candidate.get_play_length())*fraction
        original = pose_data(source, time, "source")
        current = pose_data(candidate, time, "target")
        before = pose_data(baseline, time, "target")
        direction_errors.append(angle(original["direction"], current["direction"]))
        right_grip_changes.extend((current["grips"][name]-before["grips"][name]).length() for name in grip_bones if name != "hand_l")
        hand_changes.append((current["handPosition"]-before["handPosition"]).length())
        left_grip_errors.append((current["grips"]["hand_l"]-original["grips"]["hand_l"]).length())
    checks.append({"sequence": candidate.get_path_name(), "samples": 5,
                   "maxSourceBarrelDifferenceDeg": max(direction_errors),
                   "maxRightGripChangeVsPreviousCm": max(right_grip_changes),
                   "maxRightHandPositionChangeCm": max(hand_changes),
                   "maxLeftHandVsSourceWeaponSpaceCm": max(left_grip_errors)})
report = {"scope": "Owned retargeter and new animation outputs only; BP/AG/chooser and source assets not saved or rebound",
          "retargeter": RTG, "rotationTransferSettings": settings.export_text(),
          "newAssetCount": len(created), "reusedObjects": bool(objects_before), "additiveReferencesFixed": fixed,
          "saved": [asset.get_path_name() for asset in [retargeter] + list(copies.values())], "checks": checks,
          "pending": "Review numerical direction and grip changes before connecting graph or montages; no visual/runtime pass yet"}
result_path = Path(unreal.Paths.project_saved_dir()).resolve() / "AnimationOverhaul/2026-10-29_UAF52RifleOverlay/LyraWeaponFrameCandidate_2026-11-01.json"
result_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
print("[UAFLyraWeaponFrame] Saved " + str(result_path))
