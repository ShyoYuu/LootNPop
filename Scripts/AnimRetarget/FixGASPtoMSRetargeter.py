"""런타임 리타기터 RTG_GASP_to_MS(플레이어 UEFN AnimSourceMesh → MoverSample VisualMesh)를 바로잡는다(단위 2-0b).

LootNPop 에디터 Output Log의 Cmd 창에서 실행한다.
    py "<프로젝트>/Scripts/AnimRetarget/FixGASPtoMSRetargeter.py" [verify]
2-0b 전에는 플레이어가 RTG_Lyra_to_MS를 써서 이 리타기터가 런타임에 쓰인 적이 없었다. 결함 세 가지를 고친다.
- 다리 IK 목표 불일치: MoverSample IK 리그(IK_Mannequin)는 다리 체인이 thigh→ball이고 목표가 ball에 있다.
  UEFN 리그는 thigh→foot에 목표가 foot이다. 이대로 IK를 돌리면 ball을 foot 자리로 끌어 다리가 프레임마다 100~170° 뒤집힌다
  (원본 20~30°). IK op를 끄면 팔 IK도 사라져 무기 왼손이 그립에서 벗어난다. 그래서 MS 리그를 LootNPop 구성 에셋
  IK_MS_FootGoal로 복제해(MoverSample 소스는 저장하지 않는다) 다리 체인 끝·목표를 foot으로 옮기고 대상 리그로 쓴다.
  ball은 체인 밖이 되어 아래 Copy Base Pose로 UEFN 발가락 회전을 받는다. 그래도 다리가 거의 펴진 클립에서 무릎이 뒤집혀
  (60~118°) 다리 목표는 솔버에서 떼어 다리는 FK만 쓰고, IK는 팔(무기 그립 위치)만 푼다.
- 자동 매핑이 원본(UEFN) IK 리그에 없는 대상 체인을 엉뚱한 체인에 이었다. 허벅지·종아리 트위스트 ← 손가락 중수골·쇄골,
  FootIK ← Toe, HandIK ← Index, HandGunIK ← LeftPinky. → 원본에 같은 이름 체인이 없는 대상 체인은 매핑을 해제한다.
- 체인 밖 본(트위스트·ik_*·ball)이 기준 포즈에 멈추지 않도록 스택 맨 앞에 Copy Base Pose op를 넣는다(본 이름으로 로컬 포즈 복사).
- 대상 리타깃 포즈는 **정렬하지 않은 DefaultPose**로 둔다. auto_align_all_bones로 정렬하면 견착 위치 오차는 줄지만
  (오른손의 spine_05 기준, Lyra 대비 라이플 9.5 → 4.2cm) 위팔·아래팔 기준 자세가 13°·22° 돌아 총구가 27° 숙여진다(세 총 모두,
  사용자 PIE에서 권총으로 발견). 정렬하지 않으면 총구 방향은 Lyra와 0°로 같다. 남는 위치 오차는 무기별 견착 보정
  (ULNPWeaponVisualSet::RightHandCorrection, 오른팔 IK — ComputeLeftHandGripOffsets.py가 계산)으로 메운다.
  예전 실행이 만든 "AutoAligned" 포즈가 있으면 지운다.
Root Motion op의 root·pelvis 본 지정도 다시 맞춘다. 저장하지 않는다. 확인 뒤 RTG_GASP_to_MS와 IK_MS_FootGoal만 저장한다.
verify 인자를 주면 클립을 임시 폴더에 배치 리타깃해 잰다(끝나면 지운다). ① 프레임 간 회전 튐(달리기 2개),
② 무기 그립 기준: 오른손 공간의 왼손 위치를 원본과 비교(조준 아이들, 전환 전 Lyra→MS 경로도 같은 방식으로 잰다).
결과는 Saved/AnimRetarget/FixGASPtoMSRetargeter.json.
"""
import json, math, sys
from pathlib import Path
import unreal

RTG = "/Game/Characters/Retargeters/RTG_GASP_to_MS"
MS_RIG = "/Game/Characters/MoverSample_Mannequin/Rigs/IK_Mannequin"
TGT_RIG = "/Game/Characters/Retargeters/IK_MS_FootGoal"
SRC_MESH = "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin"
TGT_MESH = "/Game/Characters/MoverSample_Mannequin/Meshes/SKM_Manny_Simple"
RUN_CLIPS = ["/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Loop_F_Troy",
             "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Arc_Wide_L"]
GRIP_CLIPS = ["/Game/Characters/Retargeted/Lyra/MM_Rifle_Idle_ADS_UEFN", "/Game/Characters/Retargeted/Lyra/MM_Pistol_Idle_ADS_UEFN",
              "/Game/Characters/Retargeted/Lyra/MM_Shotgun_Idle_ADS_UEFN"]
LYRA_GRIP = {"/Game/Characters/Lyra_Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Idle_ADS": "MM_Rifle_Idle_ADS_UEFN",
             "/Game/Characters/Lyra_Mannequin/Animations/Locomotion/Pistol/MM_Pistol_Idle_ADS": "MM_Pistol_Idle_ADS_UEFN",
             "/Game/Characters/Lyra_Mannequin/Animations/Locomotion/Shotgun/MM_Shotgun_Idle_ADS": "MM_Shotgun_Idle_ADS_UEFN"}
TEMP = "/Game/Temp_RTGVerify"
BONES = ["hand_l", "hand_r", "lowerarm_l", "lowerarm_r", "thigh_l", "calf_l", "foot_l", "ball_l", "thigh_twist_01_l",
         "calf_twist_01_l", "thigh_r", "calf_r", "foot_r", "ball_r", "upperarm_twist_01_l", "pelvis", "spine_01"]
OUT = Path(unreal.Paths.project_saved_dir()).resolve() / "AnimRetarget" / "FixGASPtoMSRetargeter.json"
AL = unreal.AnimationLibrary


def ang(q1, q2):
    d = abs(q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w)
    return math.degrees(2 * math.acos(min(1.0, d)))


def jumps(anim):
    """본별 프레임 간 로컬 회전 변화의 최댓값(도)."""
    n = AL.get_num_frames(anim)
    out = {}
    for b in BONES:
        prev, mx = None, 0.0
        for f in range(n):
            q = AL.get_bone_pose_for_frame(anim, b, f, False).rotation
            if prev is not None:
                mx = max(mx, ang(prev, q))
            prev = q
        out[b] = round(mx, 1)
    return out


def grip(anim):
    """첫 프레임에서 오른손 공간의 왼손 위치(cm)."""
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_frame(anim, 0, unreal.AnimPoseEvaluationOptions())
    r = unreal.AnimPoseExtensions.get_bone_pose(pose, "hand_r", unreal.AnimPoseSpaces.WORLD)
    l = unreal.AnimPoseExtensions.get_bone_pose(pose, "hand_l", unreal.AnimPoseSpaces.WORLD)
    v = r.inverse_transform_location(l.translation)
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


def dist(a, b):
    return round(math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b))), 1)


def batch(clips, src_mesh, rtg, dst):
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.set_editor_property("assets_to_retarget", [unreal.EditorAssetLibrary.find_asset_data(c) for c in clips])
    inputs.set_editor_property("source_mesh", unreal.load_asset(src_mesh))
    inputs.set_editor_property("target_mesh", unreal.load_asset(TGT_MESH))
    inputs.set_editor_property("ik_retarget_asset", unreal.load_asset(rtg))
    inputs.set_editor_property("target_path", dst)
    inputs.set_editor_property("use_source_path", False)
    inputs.set_editor_property("suffix", "")
    inputs.set_editor_property("include_referenced_assets", False)
    inputs.set_editor_property("overwrite_existing_files", True)
    created = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    return {str(a.package_name).rsplit("/", 1)[1]: unreal.load_asset(str(a.package_name)) for a in created}


def shoulder(anim):
    """오른손(=총)의 상체(spine_05) 기준 위치."""
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_frame(anim, 0, unreal.AnimPoseEvaluationOptions())
    r = unreal.AnimPoseExtensions.get_bone_pose(pose, "spine_05", unreal.AnimPoseSpaces.WORLD)
    h = unreal.AnimPoseExtensions.get_bone_pose(pose, "hand_r", unreal.AnimPoseSpaces.WORLD)
    v = r.inverse_transform_location(h.translation)
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


def measure(tag):
    out = {"jumps": {k: jumps(v) for k, v in batch(RUN_CLIPS, SRC_MESH, RTG, TEMP + "/" + tag + "Run").items()}}
    g = {}
    for k, v in batch(GRIP_CLIPS, SRC_MESH, RTG, TEMP + "/" + tag + "Grip").items():
        src = grip(unreal.load_asset("/Game/Characters/Retargeted/Lyra/" + k))
        lyra = [p for p in LYRA_GRIP if LYRA_GRIP[p] == k][0]
        g[k] = {"grip_source": src, "grip_target": grip(v), "grip_error_cm": dist(src, grip(v)),
                "shoulder_vs_lyra_cm": dist(shoulder(v), shoulder(unreal.load_asset(lyra)))}
    out["grip"] = g
    return out


def make_target_rig():
    if not unreal.EditorAssetLibrary.does_asset_exist(TGT_RIG):
        unreal.EditorAssetLibrary.duplicate_asset(MS_RIG, TGT_RIG)
    c = unreal.IKRigController.get_controller(unreal.load_asset(TGT_RIG))
    for side in ("l", "r"):
        chain = "LeftLeg" if side == "l" else "RightLeg"
        c.set_retarget_chain_end_bone(chain, "foot_" + side)
        c.set_goal_bone("foot_%s_Goal" % side, "foot_" + side)
        # 다리는 FK로 원본 회전을 그대로 받는다. 다리 목표가 솔버에 이어져 있으면 다리가 거의 펴진 클립에서
        # 무릎이 뒤집힌다(달리기 루프 실측 60~118°). 솔버는 팔 목표(무기 그립)만 푼다.
        for i in range(c.get_num_solvers()):
            if c.is_goal_connected_to_solver("foot_%s_Goal" % side, i):
                c.disconnect_goal_from_solver("foot_%s_Goal" % side, i)
    return [[str(x.get_editor_property("chain_name")), str(x.get_editor_property("end_bone").get_editor_property("bone_name")),
             str(x.get_editor_property("ik_goal_name"))] for x in c.get_retarget_chains()
            if str(x.get_editor_property("chain_name")) in ("LeftLeg", "RightLeg")]


def fix(rc):
    log = {"target_rig_legs": make_target_rig()}
    rig = unreal.load_asset(TGT_RIG)
    if rc.get_ik_rig(unreal.RetargetSourceOrTarget.TARGET) != rig:
        rc.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, rig)
    # op 설정도 대상 IK 리그를 따로 들고 있다. 리타기터의 대상 리그만 바꾸면 op는 옛 MS 리그로 계속 푼다
    # (op 설정 구조체의 ik_rig_asset을 set_settings로 바꾸는 것은 무시된다).
    rc.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, rig)
    # 대상 리타깃 포즈는 정렬하지 않은 기본 포즈로 둔다(정렬하면 총구가 27° 숙여진다 — 위 설명).
    tgt = unreal.RetargetSourceOrTarget.TARGET
    rc.set_current_retarget_pose("Default Pose", tgt)  # 기본 포즈 이름에는 공백이 있다
    if "AutoAligned" in [str(p) for p in rc.get_retarget_poses(tgt)]:
        rc.remove_retarget_pose("AutoAligned", tgt)
    log["target_retarget_pose"] = str(rc.get_current_retarget_pose_name(tgt))
    src_rig = unreal.IKRigController.get_controller(rc.get_ik_rig(unreal.RetargetSourceOrTarget.SOURCE))
    tgt_rig = unreal.IKRigController.get_controller(rig)
    src_names = {str(c.get_editor_property("chain_name")) for c in src_rig.get_retarget_chains()}
    cleared, mapped = [], []
    for c in tgt_rig.get_retarget_chains():
        t = str(c.get_editor_property("chain_name"))
        cur = str(rc.get_source_chain(t))
        if t not in src_names:
            if cur != "None":
                rc.set_source_chain("None", t)
                cleared.append([t, cur])
        elif cur != t:
            rc.set_source_chain(t, t)
            mapped.append([t, cur])
    log["cleared"] = cleared
    log["mapped_same_name"] = mapped
    for i in range(rc.get_num_retarget_ops()):
        name = str(rc.get_op_name(i))
        oc = rc.get_op_controller(i)
        if name == "Root Motion":
            oc.set_source_root_bone("root")
            oc.set_target_root_bone("root")
            oc.set_target_pelvis_bone("pelvis")
        if name in ("Run IK Rig", "FK Chains"):
            log["rig_" + name] = str(oc.get_settings().get_editor_property("ik_rig_asset").get_path_name())
        if name == "Run IK Rig":
            rc.set_retarget_op_enabled(i, True)
    names = [str(rc.get_op_name(i)) for i in range(rc.get_num_retarget_ops())]
    if "Copy Base Pose" not in names:
        rc.add_retarget_op("/Script/IKRig.IKRetargetCopyBasePoseOp")
        rc.move_retarget_op_in_stack(rc.get_num_retarget_ops() - 1, 0)
    log["ops"] = [[str(rc.get_op_name(i)), rc.get_retarget_op_enabled(i)] for i in range(rc.get_num_retarget_ops())]
    log["mapping"] = [[str(c.get_editor_property("chain_name")), str(rc.get_source_chain(str(c.get_editor_property("chain_name"))))]
                      for c in tgt_rig.get_retarget_chains()]
    return log


res = {}
try:
    verify = len(sys.argv) > 1 and sys.argv[1] == "verify"
    rc = unreal.IKRetargeterController.get_controller(unreal.load_asset(RTG))
    if verify:
        res["source_jumps"] = {c.rsplit("/", 1)[1]: jumps(unreal.load_asset(c)) for c in RUN_CLIPS}
        old = {}
        for k, v in batch(list(LYRA_GRIP), "/Game/Characters/Lyra_Mannequin/Meshes/SKM_Manny",
                          "/Game/Characters/Retargeters/RTG_Lyra_to_MS", TEMP + "/LyraGrip").items():
            src = grip(unreal.load_asset([p for p in LYRA_GRIP if p.endswith("/" + k)][0]))
            old[k] = {"source": src, "target": grip(v), "error_cm": dist(src, grip(v))}
        res["old_lyra_to_ms_grip"] = old
        res["before"] = measure("Before")
    res["fix"] = fix(rc)
    if verify:
        res["after"] = measure("After")
        unreal.EditorAssetLibrary.delete_directory(TEMP)
    res["dirty"] = sorted(str(p.get_name()) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
except Exception:
    import traceback
    res["error"] = traceback.format_exc()
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(res, indent=1, ensure_ascii=False), encoding="utf-8")
unreal.log("FixGASPtoMSRetargeter: report written to " + str(OUT))
