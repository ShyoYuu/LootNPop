"""Phase 5 §37: Lyra 장착 규약을 기존 리타깃 애니메이션과 함께 시험한다.

PIE 종료 후 LootNPop 에디터에서 실행한다. UAFSpike/LyraWeaponContract의 라이플
데이터/VisualSet 사본과 시험 BP만 저장한다. 소스·공유 에셋·맵은 변경하지 않는다.
시퀀스 생성/삭제/overwrite는 없다. AG Idle은 보고서 확인 후 기존 _UEFN으로 수동 연결한다.
"""
import json
import math
from pathlib import Path
import unreal

if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
    raise RuntimeError("Stop PIE before setting up the Lyra weapon contract trial.")

AL = unreal.EditorAssetLibrary
PE = unreal.AnimPoseExtensions
ML = unreal.MathLibrary
FOLDER = "/Game/Characters/UAFSpike/LyraWeaponContract"
BP_PATH = "/Game/Characters/UAFSpike/BP_PlayerUAFSpike"
BASE_VS = "/Game/ItemData/WeaponVisualSets/VS_Rifle"
BASE_WEAPON = "/Game/ItemData/Weapons/DA_Rifle"
VS_PATH = FOLDER + "/VS_RifleLyraContract"
WEAPON_PATH = FOLDER + "/DA_RifleLyraContract"
CHOOSER = "/Game/Characters/CHT_Montage"
SOURCE_ROOT = "/Game/Characters/Lyra_Mannequin/Animations"
TARGET_ROOT = "/Game/Characters/Retargeted/Lyra"
LYRA_ROTATION = unreal.Rotator(pitch=0.0, yaw=-90.0, roll=0.0)
LYRA_MOUNT = unreal.Transform(unreal.Vector(0, 0, 0), LYRA_ROTATION, unreal.Vector(1, 1, 1))


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing required asset: " + path)
    return asset


def own_copy(path, original):
    return load(path) if AL.does_asset_exist(path) else AL.duplicate_asset(original, path)


def vector(value):
    return [float(getattr(value, axis)) for axis in ("x", "y", "z")]


def angle(a, b):
    dot = sum(x*y for x, y in zip(vector(a), vector(b))) / (a.length()*b.length())
    return math.degrees(math.acos(max(-1.0, min(1.0, dot))))


source_mesh = load("/Game/Characters/Lyra_Mannequin/Meshes/SKM_Manny")
target_mesh = load("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin")
baseline_vs = load(BASE_VS)
source_options = unreal.AnimPoseEvaluationOptions()
source_options.set_editor_property("optional_skeletal_mesh", source_mesh)
source_options.set_editor_property("evaluation_type", unreal.AnimDataEvalType.COMPRESSED)
target_options = unreal.AnimPoseEvaluationOptions()
target_options.set_editor_property("optional_skeletal_mesh", target_mesh)
target_options.set_editor_property("evaluation_type", unreal.AnimDataEvalType.COMPRESSED)


def sample(sequence, time, options):
    pose = PE.get_anim_pose_at_time(sequence, time, options)
    bone = lambda name: PE.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD)
    weapon = ML.compose_transforms(LYRA_MOUNT, bone("weapon_r"))
    root = bone("root")
    return {
        "muzzleRootDirection": vector(root.rotation.unrotate_vector(weapon.rotation.rotate_vector(unreal.Vector(0, 1, 0)))),
        "rightHandInWeaponCm": vector(weapon.inverse_transform_location(bone("hand_r").translation)),
        "leftHandInWeaponCm": vector(weapon.inverse_transform_location(bone("hand_l").translation)),
        "weaponLocal": PE.get_bone_pose(pose, "weapon_r", unreal.AnimPoseSpaces.LOCAL).export_text(),
    }


rows = []
for relative in ("Locomotion/Rifle/MM_Rifle_Idle_ADS", "Actions/MM_Rifle_Reload",
                 "Actions/MM_Dash_Forward", "Actions/MM_Dash_Backward", "Actions/MM_Dash_Left", "Actions/MM_Dash_Right"):
    source = load(SOURCE_ROOT + "/" + relative)
    target = load(TARGET_ROOT + "/" + relative.rsplit("/", 1)[-1] + "_UEFN")
    samples = []
    for fraction in (0.0, 0.25, 0.5, 0.75, 1.0):
        time = min(source.get_play_length(), target.get_play_length()) * fraction
        source_row = sample(source, time, source_options)
        target_row = sample(target, time, target_options)
        samples.append({"time": time, "source": source_row, "target": target_row,
                        "muzzleDirectionDifferenceDeg": angle(unreal.Vector(*source_row["muzzleRootDirection"]),
                                                              unreal.Vector(*target_row["muzzleRootDirection"]))})
    rows.append({"source": source.get_path_name(), "target": target.get_path_name(), "samples": samples})

# 원본 장착 변환은 WID_Rifle에서 직접 읽었다. 루트 SkeletalMesh는 사용자 GUI 확인 근거다.
visual_set = own_copy(VS_PATH, BASE_VS)
weapon_data = own_copy(WEAPON_PATH, BASE_WEAPON)
if visual_set is None or weapon_data is None:
    raise RuntimeError("Could not create isolated rifle data copies.")
visual_set.modify()
visual_set.set_editor_property("attach_socket_name", "weapon_r")
visual_set.set_editor_property("weapon_mesh_relative_location", unreal.Vector(0, 0, 0))
visual_set.set_editor_property("weapon_mesh_relative_rotation", LYRA_ROTATION)
rotation = visual_set.get_editor_property("weapon_mesh_relative_rotation")
if abs(rotation.pitch) > 0.001 or abs(rotation.yaw + 90.0) > 0.001 or abs(rotation.roll) > 0.001:
    raise RuntimeError("Unexpected serialized mount rotation; expected pitch=0, yaw=-90, roll=0.")
# 기존 그립 IK 설정은 유지하고 원본/대상 위치 차이를 보고한다.
weapon_data.modify()
weapon_data.set_editor_property("visual_set", visual_set)
bp = load(BP_PATH)
cdo = unreal.get_default_object(unreal.load_class(None, BP_PATH + ".BP_PlayerUAFSpike_C"))
weapons = list(cdo.get_editor_property("test_weapon_list"))
indices = [index for index, item in enumerate(weapons) if item in (load(BASE_WEAPON), weapon_data)]
if len(indices) != 1:
    raise RuntimeError("Expected exactly one rifle entry in the spike weapon list.")
previous = {"testWeapons": [item.get_path_name() if item else None for item in weapons],
            "chooser": cdo.get_editor_property("montage_chooser").get_path_name()}
weapons[indices[0]] = weapon_data
cdo.modify()
cdo.set_editor_property("test_weapon_list", weapons)
cdo.set_editor_property("montage_chooser", load(CHOOSER))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = unreal.get_default_object(unreal.load_class(None, BP_PATH + ".BP_PlayerUAFSpike_C"))
if list(cdo.get_editor_property("test_weapon_list"))[indices[0]] != weapon_data or cdo.get_editor_property("montage_chooser") != load(CHOOSER):
    raise RuntimeError("Spike BP reference validation failed.")
for asset in (visual_set, weapon_data, bp):
    if not AL.save_loaded_asset(asset):
        raise RuntimeError("Could not save trial asset: " + asset.get_path_name())

report = {"scope": "Trial VS/weapon data and spike BP only; no sequence creation or source/shared/map edits",
          "previousBP": previous, "saved": [asset.get_path_name() for asset in (visual_set, weapon_data, bp)],
          "mount": LYRA_MOUNT.export_text(), "leftGripPreserved": vector(visual_set.get_editor_property("left_hand_grip_offset")),
          "chooser": CHOOSER, "samples": rows,
          "pending": "Rebind AG Idle to MM_Rifle_Idle_ADS_UEFN; compile/save AG; test first/re-equip, aim/move, reload and all dash directions. Offline samples do not prove visual grip quality."}
output = Path(unreal.Paths.project_saved_dir()).resolve() / "AnimationOverhaul/2026-10-29_UAF52RifleOverlay/LyraWeaponContractTrial_2026-11-01.json"
output.write_text(json.dumps(report, indent=2), encoding="utf-8")
print("[UAFLyraWeaponContract] Saved " + str(output))
