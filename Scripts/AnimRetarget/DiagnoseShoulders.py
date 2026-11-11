"""어깨 진단: 원본은 보존하고 임시 리타기터의 IK 유무·정렬 대조군을 측정한다.

에디터 Cmd: py "<프로젝트>/Scripts/AnimRetarget/DiagnoseShoulders.py"
결과: Saved/AnimRetarget/DiagnoseShoulders.json. 임시 에셋은 finally에서 제거한다.
WORLD는 AnimPose 안의 컴포넌트 공간이다. 쿼터니언 최단각과 상체 기준 팔 벌림각을 함께 잰다.
"""
import json
import importlib.util
import math
import traceback
from pathlib import Path
import unreal

RTG = '/Game/Characters/Retargeters/RTG_GASP_to_MS'
SRC = '/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin'
DST = '/Game/Characters/MoverSample_Mannequin/Meshes/SKM_Manny_Simple'
TEMP = '/Game/Temp_ShoulderDiagnosis_20261111'
CLIPS = [
    '/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Relaxed_Stand_Idle_Loop',
    '/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop',
    '/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Loop_F_Troy',
    '/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Arc_Wide_L',
]
BONES = ['pelvis', 'spine_05', 'clavicle_l', 'clavicle_r', 'upperarm_l', 'upperarm_r', 'lowerarm_l', 'lowerarm_r', 'hand_l', 'hand_r']
TWISTS = ['upperarm_twist_' + n + '_' + s for s in ('l', 'r') for n in ('01', '02')]
BONES += TWISTS
PE = unreal.AnimPoseExtensions
SPACE = unreal.AnimPoseSpaces.WORLD


def angle(a, b):
    dot = abs(sum(getattr(a, k) * getattr(b, k) for k in ('x', 'y', 'z', 'w')))
    na = math.sqrt(sum(getattr(a, k) ** 2 for k in ('x', 'y', 'z', 'w')))
    nb = math.sqrt(sum(getattr(b, k) ** 2 for k in ('x', 'y', 'z', 'w')))
    return math.degrees(2 * math.acos(min(1.0, dot / (na * nb))))


def stats(values):
    return {'mean': round(sum(values) / len(values), 3), 'min': round(min(values), 3), 'max': round(max(values), 3)}


def pose(anim, frame, mesh):
    options = unreal.AnimPoseEvaluationOptions(optional_skeletal_mesh=mesh)
    p = PE.get_anim_pose_at_frame(anim, frame, options)
    return {b: PE.get_bone_pose(p, b, SPACE) for b in BONES}


def arm_out(p, side):
    # 상체의 아래 방향과 상완(어깨→팔꿈치) 사이 각도. 0이면 몸 옆으로 내린 팔이다.
    v = p['lowerarm_' + side].translation - p['upperarm_' + side].translation
    down = p['pelvis'].translation - p['spine_05'].translation
    dot = sum(getattr(v, k) * getattr(down, k) for k in ('x', 'y', 'z'))
    return math.degrees(math.acos(max(-1.0, min(1.0, dot / (v.length() * down.length())))))


def compare(source, target, sm, tm):
    frames = range(0, unreal.AnimationLibrary.get_num_frames(source), 3)
    diffs = {b: [] for b in BONES}
    twist_errors = {b: [] for b in TWISTS}
    reference = PE.get_anim_pose_at_frame(target, 0, unreal.AnimPoseEvaluationOptions(optional_skeletal_mesh=tm))
    ref_positions = {b: PE.get_ref_bone_pose(reference, b, unreal.AnimPoseSpaces.LOCAL).translation for b in TWISTS}
    arms = {s: {'source': [], 'target': [], 'target_minus_source': []} for s in ('l', 'r')}
    for f in frames:
        a, b = pose(source, f, sm), pose(target, f, tm)
        for bone in BONES:
            diffs[bone].append(angle(a[bone].rotation, b[bone].rotation))
        for bone in TWISTS:
            local = unreal.AnimationLibrary.get_bone_pose_for_frame(target, bone, f, False)
            twist_errors[bone].append((local.translation - ref_positions[bone]).length())
        for s in arms:
            av, bv = arm_out(a, s), arm_out(b, s)
            for k, v in [('source', av), ('target', bv), ('target_minus_source', bv - av)]:
                arms[s][k].append(v)
    return {'samples': len(frames), 'component_rotation_deg': {b: stats(v) for b, v in diffs.items()},
            'twist_local_translation_error_cm': {b: stats(v) for b, v in twist_errors.items()},
            'arm_from_down_deg': {s: {k: stats(v) for k, v in row.items()} for s, row in arms.items()}}


res = {'project': unreal.Paths.get_project_file_path(), 'variants': {}}
try:
    if unreal.EditorAssetLibrary.does_directory_exist(TEMP):
        raise RuntimeError('Temporary diagnosis directory already exists; inspect it before retrying.')
    sm, tm = unreal.load_asset(SRC), unreal.load_asset(DST)
    spec = importlib.util.spec_from_file_location('shoulder_fix', str(Path(unreal.Paths.project_dir()) / 'Scripts/AnimRetarget/FixGASPtoMSRetargeter.py'))
    fix = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fix)
    res['dirty_before'] = sorted(str(p.get_name()) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    for tag in ('baseline', 'ik_off', 'aligned_ik_off', 'aligned_ik_on', 'aligned_hands', 'legacy_twist_copy'):
        path = TEMP + '/RTG_' + tag
        rtg = unreal.EditorAssetLibrary.duplicate_asset(RTG, path)
        if rtg is None:
            raise RuntimeError('Failed to duplicate retargeter: ' + path)
        rc = unreal.IKRetargeterController.get_controller(rtg)
        side = unreal.RetargetSourceOrTarget.TARGET
        # 수정된 에셋에서도 정렬하지 않은 이전 구성을 같은 조건으로 재현한다.
        rc.set_current_retarget_pose('Default Pose', side)
        if tag in ('aligned_hands', 'legacy_twist_copy'):
            fix.align_arm_pose(rc)
            if tag == 'legacy_twist_copy':
                for side in ('Left', 'Right'):
                    for number in ('01', '02'):
                        rc.set_source_chain('None', side + 'UpperArmTwist' + number)
        elif 'aligned' in tag:
            rc.create_retarget_pose('DiagnosticAligned', side)
            rc.set_current_retarget_pose('DiagnosticAligned', side)
            rc.auto_align_all_bones(side)
        for i in range(rc.get_num_retarget_ops()):
            if str(rc.get_op_name(i)) == 'Run IK Rig':
                rc.set_retarget_op_enabled(i, not tag.endswith('ik_off'))
        inputs = unreal.IKRetargetBatchOperationInputs()
        inputs.set_editor_properties({'assets_to_retarget': [unreal.EditorAssetLibrary.find_asset_data(c) for c in CLIPS],
            'source_mesh': sm, 'target_mesh': tm, 'ik_retarget_asset': rtg, 'target_path': TEMP + '/' + tag,
            'use_source_path': False, 'include_referenced_assets': False, 'overwrite_existing_files': False})
        created = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
        targets = {str(a.package_name).rsplit('/', 1)[1]: unreal.load_asset(str(a.package_name)) for a in created}
        if len(targets) != len(CLIPS):
            raise RuntimeError('Incomplete batch: ' + str(list(targets)))
        res['variants'][tag] = {c.rsplit('/', 1)[1]: compare(unreal.load_asset(c), targets[c.rsplit('/', 1)[1]], sm, tm) for c in CLIPS}
except Exception:
    res['error'] = traceback.format_exc()
finally:
    # 이번 실행이 만든 전용 경로만 제거한다. 기존 경로 충돌 시에는 건드리지 않는다.
    if 'dirty_before' in res:
        res['temporary_cleanup'] = unreal.EditorAssetLibrary.delete_directory(TEMP)
    res['dirty_after'] = sorted(str(p.get_name()) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    out = Path(unreal.Paths.project_saved_dir()).resolve() / 'AnimRetarget/DiagnoseShoulders.json'
    out.write_text(json.dumps(res, indent=2), encoding='utf-8')
    unreal.log('DiagnoseShoulders: ' + str(out))
