"""총기 표현 세트의 왼손 그립 오프셋(ULNPWeaponVisualSet::LeftHandGripOffset)을 계산한다(애니메이션 개선 단위 2-0b).

LootNPop 에디터 Output Log의 Cmd 창에서 실행한다.
    py "<프로젝트>/Scripts/AnimRetarget/ComputeLeftHandGripOffsets.py" [apply]
전환 전 경로(Lyra 원본 애니 + Lyra와 비율이 같은 MoverSample 메시)에서 조준 아이들의 왼손(hand_l) 위치를
무기 메시 컴포넌트 공간으로 옮긴다. 무기 컴포넌트 변환 = 상대 오프셋(WeaponMeshRelative*) × 부착 소켓(MS 메시) × 소켓 부모 본.
같은 계산을 힙파이어 아이들로도 해서 차이를 보고한다(크면 고정 오프셋이 맞지 않는다는 뜻).
apply 인자를 주면 조준 아이들 값을 VS_Rifle·VS_Shotgun에 넣는다(저장하지 않는다). 권총은 리타깃만으로 0.3cm라 넣지 않는다.

견착 보정(ULNPWeaponVisualSet::RightHandCorrection)도 계산한다. UEFN 사본 조준 아이들을 런타임 리타기터 RTG_GASP_to_MS로
임시 폴더에 리타깃하고, 오른손의 spine_05 기준 위치를 Lyra 원본과 같게 만드는 이동량을 그 결과의 무기 공간으로 옮긴다.
런타임에서는 오른팔 IK가 손 회전을 유지한 채 손을 이만큼 옮긴다. apply면 세 총 모두에 넣는다.
결과는 Saved/AnimRetarget/ComputeLeftHandGripOffsets.json.
"""
import json, math, sys
from pathlib import Path
import unreal

MS_MESH = "/Game/Characters/MoverSample_Mannequin/Meshes/SKM_Manny_Simple"
UEFN_MESH = "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin"
RUNTIME_RTG = "/Game/Characters/Retargeters/RTG_GASP_to_MS"
TEMP = "/Game/Temp_GripOffsets"
LYRA_ANIM = "/Game/Characters/Lyra_Mannequin/Animations/Locomotion"
SETS = {
    "VS_Rifle": {"apply": True, "ads": LYRA_ANIM + "/Rifle/MM_Rifle_Idle_ADS", "hip": LYRA_ANIM + "/Rifle/MM_Rifle_Idle_Hipfire"},
    "VS_Shotgun": {"apply": True, "ads": LYRA_ANIM + "/Shotgun/MM_Shotgun_Idle_ADS", "hip": LYRA_ANIM + "/Shotgun/MM_Shotgun_Idle_Hipfire"},
    "VS_Pistol": {"apply": False, "ads": LYRA_ANIM + "/Pistol/MM_Pistol_Idle_ADS", "hip": LYRA_ANIM + "/Pistol/MM_Pistol_Idle_Hipfire"},
}
OUT = Path(unreal.Paths.project_saved_dir()).resolve() / "AnimRetarget" / "ComputeLeftHandGripOffsets.json"
ML = unreal.MathLibrary
PE = unreal.AnimPoseExtensions


def socket_world(pose, mesh, name):
    sock = mesh.find_socket(name)
    if sock is not None:
        parent = PE.get_bone_pose(pose, sock.get_editor_property("bone_name"), unreal.AnimPoseSpaces.WORLD)
        local = unreal.Transform(sock.get_editor_property("relative_location"), sock.get_editor_property("relative_rotation"),
                                 sock.get_editor_property("relative_scale"))
        return ML.compose_transforms(local, parent), "socket:" + str(sock.get_editor_property("bone_name"))
    return PE.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD), "bone"


def offset(vs, mesh, anim_path):
    anim = unreal.load_asset(anim_path)
    if anim is None:
        return None, "missing " + anim_path
    pose = PE.get_anim_pose_at_frame(anim, 0, unreal.AnimPoseEvaluationOptions())
    sock, kind = socket_world(pose, mesh, vs.get_editor_property("attach_socket_name"))
    rel = unreal.Transform(vs.get_editor_property("weapon_mesh_relative_location"), vs.get_editor_property("weapon_mesh_relative_rotation"),
                           unreal.Vector(1, 1, 1))
    weapon = ML.compose_transforms(rel, sock)
    hand_l = PE.get_bone_pose(pose, "hand_l", unreal.AnimPoseSpaces.WORLD).translation
    v = weapon.inverse_transform_location(hand_l)
    return [round(v.x, 2), round(v.y, 2), round(v.z, 2)], kind


def weapon_world(pose, vs, mesh):
    sock, _ = socket_world(pose, mesh, vs.get_editor_property("attach_socket_name"))
    rel = unreal.Transform(vs.get_editor_property("weapon_mesh_relative_location"), vs.get_editor_property("weapon_mesh_relative_rotation"),
                           unreal.Vector(1, 1, 1))
    return ML.compose_transforms(rel, sock)


def runtime_retarget(ads_path):
    """UEFN 사본(<이름>_UEFN)을 런타임 리타기터로 MS 메시에 리타깃한 임시 시퀀스."""
    copy = "/Game/Characters/Retargeted/Lyra/" + ads_path.rsplit("/", 1)[1] + "_UEFN"
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.set_editor_property("assets_to_retarget", [unreal.EditorAssetLibrary.find_asset_data(copy)])
    inputs.set_editor_property("source_mesh", unreal.load_asset(UEFN_MESH))
    inputs.set_editor_property("target_mesh", unreal.load_asset(MS_MESH))
    inputs.set_editor_property("ik_retarget_asset", unreal.load_asset(RUNTIME_RTG))
    inputs.set_editor_property("target_path", TEMP)
    inputs.set_editor_property("use_source_path", False)
    inputs.set_editor_property("suffix", "")
    inputs.set_editor_property("include_referenced_assets", False)
    inputs.set_editor_property("overwrite_existing_files", True)
    created = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    return unreal.load_asset(str(created[0].package_name))


def shoulder_correction(vs, mesh, ads_path):
    lyra_pose = PE.get_anim_pose_at_frame(unreal.load_asset(ads_path), 0, unreal.AnimPoseEvaluationOptions())
    ms_pose = PE.get_anim_pose_at_frame(runtime_retarget(ads_path), 0, unreal.AnimPoseEvaluationOptions())
    lyra_rel = PE.get_bone_pose(lyra_pose, "spine_05", unreal.AnimPoseSpaces.WORLD).inverse_transform_location(
        PE.get_bone_pose(lyra_pose, "hand_r", unreal.AnimPoseSpaces.WORLD).translation)
    ms_spine = PE.get_bone_pose(ms_pose, "spine_05", unreal.AnimPoseSpaces.WORLD)
    ms_hand = PE.get_bone_pose(ms_pose, "hand_r", unreal.AnimPoseSpaces.WORLD).translation
    delta = ms_spine.transform_location(lyra_rel) - ms_hand
    v = weapon_world(ms_pose, vs, mesh).rotation.unrotate_vector(delta)
    return [round(v.x, 2), round(v.y, 2), round(v.z, 2)], round(delta.length(), 2)


res = {}
try:
    apply = len(sys.argv) > 1 and sys.argv[1] == "apply"
    mesh = unreal.load_asset(MS_MESH)
    for name, cfg in SETS.items():
        vs = unreal.load_asset("/Game/ItemData/WeaponVisualSets/" + name)
        ads, kind = offset(vs, mesh, cfg["ads"])
        hip, _ = offset(vs, mesh, cfg["hip"])
        row = {"socket": str(vs.get_editor_property("attach_socket_name")), "kind": kind, "ads": ads, "hipfire": hip}
        if ads and hip:
            row["ads_vs_hip_cm"] = round(math.sqrt(sum((a - b) ** 2 for a, b in zip(ads, hip))), 2)
        corr, corr_len = shoulder_correction(vs, mesh, cfg["ads"])
        row["right_hand_correction"] = corr
        row["right_hand_correction_len_cm"] = corr_len
        if apply and cfg["apply"] and ads:
            vs.modify()
            vs.set_editor_property("use_left_hand_grip_offset", True)
            vs.set_editor_property("left_hand_grip_offset", unreal.Vector(*ads))
            row["applied"] = True
        if apply:
            vs.modify()
            vs.set_editor_property("right_hand_correction", unreal.Vector(*corr))
            row["applied_correction"] = True
        res[name] = row
except Exception:
    import traceback
    res["error"] = traceback.format_exc()
try:
    unreal.EditorAssetLibrary.delete_directory(TEMP)
except Exception:
    pass
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(res, indent=1, ensure_ascii=False), encoding="utf-8")
unreal.log("ComputeLeftHandGripOffsets: report written to " + str(OUT))
