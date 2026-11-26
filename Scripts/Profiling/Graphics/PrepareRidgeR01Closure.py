"""채택 V007을 기준으로 국소 R01 접합1개를 추가하는 V008 manifest를 만든다."""
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
AUTHOR=ROOT/'Scripts/Profiling/Graphics/ClosedBodyTrial_V001'
UNIT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_7_V001'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def record(p):return {'path':p.relative_to(ROOT).as_posix(),'sha256':sha(p)}
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')


def main():
    target=AUTHOR/'Generation_V008.json';assert not target.exists()
    accepted=read(AUTHOR/'Acceptance_V007.json');old=read(AUTHOR/'Generation_V007.json')
    frozen=read(UNIT/'PreservationHashes.json');assert len(frozen)==534
    assert not [r for r in frozen if sha(ROOT/r['path'])!=r['sha256']]
    build=read(UNIT/'Geometry_V006/Build.json');check=read(UNIT/'Geometry_V006/GeometryCheck.json')
    assert check['target_inside'] and not check['user_camera_inside_new_body'] and check['self_intersection_pairs']==0
    assert check['regression']['other50_new_hits']==0 and check['regression']['fixed_connection_clearance']['pass']
    folder='/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/U5aBody_V008'
    new=dict(build['outputs'][0],asset=folder+'/R01Closure_V006/'+build['outputs'][0]['name'],
        actor_label='BODY_R01RockBackfill_V006',material=old['material'])
    config={'unit':'U5a-7','status':'native_local_closure_import_pending',
        'target_level':folder+'/LVI_Ridge_EDTerrain_U5aBody_V008','asset_folder':folder+'/R01Closure_V006',
        'evidence_folder':UNIT.relative_to(ROOT).as_posix(),'material':old['material'],
        'input_config_sha256':old['input_config_sha256'],'outputs':old['outputs']+[new],
        'body_outputs':old['body_outputs']+[new],'new_outputs':[new],'kept_outputs':old['outputs'],
        'kept_connection':old['kept_connection'],'preservation_files':frozen,
        'build_report':record(UNIT/'Geometry_V006/Build.json')['path'],'build_report_sha256':sha(UNIT/'Geometry_V006/Build.json'),
        'geometry_check':record(UNIT/'Geometry_V006/GeometryCheck.json')['path'],'geometry_check_sha256':sha(UNIT/'Geometry_V006/GeometryCheck.json'),
        'previous_accepted':{'map':accepted['level'],'acceptance':record(AUTHOR/'Acceptance_V007.json'),
            'generation':record(AUTHOR/'Generation_V007.json'),'frozen_inputs':record(AUTHOR/'FrozenInputs_V007.json'),
            'actual_pose_export':accepted['actual_pose_export'],'poses':accepted['poses'],
            'accepted_owner_count':21,'body_count':4,'connection_count':1,'outputs':old['outputs']},
        'actual_pose_baseline':accepted['actual_pose_export'],'user_gap_camera':record(UNIT/'UserGapCamera.json'),
        'method':{'geometry':'one new local rock backfill; accepted4 bodies and connection held; no whole asset closure/remesh',
            'surface_fit':'inside exact adjacentCliff09 hull;5cm behind original ED skin; camera and natural air gap retained',
            'role':'Static identity NoCollision; existing26 world poses retained plus one new identity actor'},
        'review':{'accepted':False,'legacy_StairRamp3_review_samples':31,'R01_user_report':'visible opening confirmed; user camera captured; camera remains outside; target lies between exactCliff09 hull entry and first ED surface',
            'R01_original_lower_grid':'9 initial misses retained historically; final bounded ray check0; no full R01 closure guarantee','collision_or_PIE_pass':False},
        'validation':{'native_import':'pending','full_native_verify':'pending','same27camera':'pending','visual_review_pending':True}}
    assert len(config['outputs'])==len({r['actor_label'] for r in config['outputs']})==6
    write(target,config)
    write(UNIT/'NativeImportConfig_V008.json',{k:config[k] for k in ['target_level','asset_folder','new_outputs','kept_outputs']})
    print(json.dumps({'source_map':'V007','new_body_count':1,'baseline_poses':26,'target_actor_poses':27,'preserved_files':534}))


if __name__=='__main__':main()
