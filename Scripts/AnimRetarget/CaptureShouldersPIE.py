"""PIE 원본/표시 어깨를 0.1초마다 읽어 저장한다. 에셋·인스턴스 값을 변경하지 않는다.

에디터 Cmd에서 실행 후 PIE에서 맨손·무기별 정지·달리기를 한다. PIE 종료 또는 300초 뒤 자동 해제된다.
결과: Saved/AnimRetarget/CaptureShouldersPIE.json. 승격 적은 각자의 원본/표시 쌍을 비교한다.
"""
import json
import datetime
import math
import time
import traceback
from pathlib import Path
import unreal

_shoulder_rows = []
_shoulder_errors = []
_shoulder_started = None
_shoulder_last = 0.0
_shoulder_last_save = 0.0
_shoulder_capture_id = datetime.datetime.now(datetime.timezone.utc).isoformat()
_shoulder_out = Path(unreal.Paths.project_saved_dir()).resolve() / 'AnimRetarget/CaptureShouldersPIE.json'
_shoulder_space = unreal.RelativeTransformSpace.RTS_COMPONENT
_shoulder_bones = ['root', 'pelvis', 'spine_05', 'clavicle_l', 'clavicle_r', 'upperarm_l', 'upperarm_r', 'lowerarm_l', 'lowerarm_r', 'hand_l', 'hand_r']
_shoulder_bones += [b + '_' + s for s in ('l', 'r') for b in ('upperarm_twist_01', 'upperarm_twist_02')]
_shoulder_equipment_class = unreal.load_class(None, '/Script/LootNPop.LNPEquipmentComponent')
_shoulder_weapon_defs = [unreal.load_asset('/Game/ItemData/Weapons/DA_' + n) for n in ('Pistol', 'Rifle', 'Shotgun', 'Launcher', 'LongSword', 'Flamethrower')]


def _shoulder_save():
    _shoulder_out.write_text(json.dumps({'capture_id': _shoulder_capture_id, 'rows': _shoulder_rows, 'errors': _shoulder_errors}, indent=1), encoding='utf-8')


def _shoulder_pose(mesh):
    result = {}
    for b in _shoulder_bones:
        if mesh.get_bone_index(b) < 0:
            raise RuntimeError('Missing bone: ' + b)
        t = mesh.get_bone_transform(b, _shoulder_space)
        result[b] = {'q': [getattr(t.rotation, k) for k in ('x', 'y', 'z', 'w')],
                     'p': [getattr(t.translation, k) for k in ('x', 'y', 'z')]}
    return result


def _shoulder_tick(delta):
    global _shoulder_started, _shoulder_last, _shoulder_last_save
    now = time.monotonic()
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if world is None:
            if _shoulder_started is not None:
                _shoulder_save()
                unreal.unregister_slate_post_tick_callback(_shoulder_handle)
                unreal.log('CaptureShouldersPIE: finished ' + str(_shoulder_out))
            return
        if _shoulder_started is None:
            _shoulder_started = now
        if now - _shoulder_started > 300:
            _shoulder_save()
            unreal.unregister_slate_post_tick_callback(_shoulder_handle)
            unreal.log('CaptureShouldersPIE: finished ' + str(_shoulder_out))
            return
        if now - _shoulder_last < 0.1:
            return
        _shoulder_last = now
        player = unreal.GameplayStatics.get_player_pawn(world, 0)
        pawns = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn))
        # 플레이어와 근처 승격 적 두 개까지. 대규모 적 전체를 매 샘플 측정하지 않는다.
        pairs = []
        for a in sorted(pawns, key=lambda a: 0 if a == player else 1):
            meshes = {m.get_name(): m for m in a.get_components_by_class(unreal.SkeletalMeshComponent)}
            if 'AnimSourceMesh' in meshes and 'VisualMesh' in meshes:
                pairs.append((a, meshes))
            if len(pairs) >= 3:
                break
        for a, meshes in pairs:
            src, dst = meshes['AnimSourceMesh'], meshes['VisualMesh']
            sai, dai = src.get_anim_instance(), dst.get_anim_instance()
            if sai is None or dai is None:
                continue
            weapon = meshes.get('WeaponMesh')
            definition = None
            if a == player:
                ps = a.get_editor_property('player_state')
                equipment = ps.get_component_by_class(_shoulder_equipment_class) if ps else None
                if equipment:
                    definition = next((d for d in _shoulder_weapon_defs if equipment.is_equipped(d)), None)
            else:
                config = a.get_editor_property('enemy_config')
                if config:
                    definition = config.get_editor_property('weapon_data')
            row = {'t': round(now - _shoulder_started, 3), 'actor': a.get_path_name(), 'player': a == player,
                   'game_time': unreal.GameplayStatics.get_time_seconds(world),
                   'source_mesh': src.get_skeletal_mesh_asset().get_path_name(),
                   'visual_mesh': dst.get_skeletal_mesh_asset().get_path_name(),
                   'source_abp': sai.get_class().get_path_name(), 'visual_abp': dai.get_class().get_path_name(),
                   'speed': sai.get_editor_property('ground_speed'),
                   'right_alpha': dai.get_editor_property('right_hand_ik_alpha'),
                   'left_alpha': dai.get_editor_property('left_hand_ik_alpha'),
                   'physics_alpha': dai.get_editor_property('physics_alpha'),
                   'weapon_def': definition.get_path_name() if definition else None,
                   'weapon': str(weapon.get_skeletal_mesh_asset()) if weapon else None,
                   'source': _shoulder_pose(src), 'visual': _shoulder_pose(dst)}
            if a == player:
                retargeter = dai.get_editor_property('iKRetargeter')
                row['retarget_asset'] = retargeter.get_path_name() if retargeter else None
                row['retarget_pose'] = str(unreal.IKRetargeterController.get_controller(retargeter).get_current_retarget_pose_name(unreal.RetargetSourceOrTarget.TARGET)) if retargeter else None
            if dai.get_editor_property('has_left_hand_grip'):
                anchor = dst.get_socket_bone_name(weapon.get_attach_socket_name())
                goal = dst.get_bone_transform(anchor, _shoulder_space).transform_location(dai.get_editor_property('left_hand_grip_in_hand'))
                actual = dst.get_bone_transform('hand_l', _shoulder_space).translation
                row['left_grip_error_cm'] = (actual - goal).length()
            aim = dai.get_editor_property('rifle_aim')
            row['aim_alpha'] = aim.get_editor_property('alpha')
            if row['aim_alpha'] > 0 and weapon.does_socket_exist('Muzzle') and weapon.does_socket_exist('Barrel'):
                muzzle = weapon.get_socket_location('Muzzle')
                barrel = muzzle - weapon.get_socket_location('Barrel')
                target = aim.get_editor_property('aim_target') - muzzle
                dot = sum(getattr(barrel, k) * getattr(target, k) for k in ('x', 'y', 'z'))
                row['barrel_target_deg'] = math.degrees(math.acos(max(-1, min(1, dot / (barrel.length() * target.length())))))
            _shoulder_rows.append(row)
        if now - _shoulder_last_save >= 5:
            _shoulder_save()
            _shoulder_last_save = now
    except Exception:
        _shoulder_errors.append(traceback.format_exc())
        _shoulder_save()
        unreal.unregister_slate_post_tick_callback(_shoulder_handle)
        unreal.log_error('CaptureShouldersPIE: failed; see ' + str(_shoulder_out))


_shoulder_handle = unreal.register_slate_post_tick_callback(_shoulder_tick)
_shoulder_save()
unreal.log('CaptureShouldersPIE: armed; start PIE and test unarmed plus all weapons. Stops after 300 seconds or EndPIE.')
