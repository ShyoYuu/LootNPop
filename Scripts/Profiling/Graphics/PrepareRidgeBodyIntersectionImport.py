"""V006 채택본을 기준으로 새 U5aBody_V007 반입 manifest만 작성한다."""
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
AUTHOR=ROOT/'Scripts/Profiling/Graphics/ClosedBodyTrial_V001'
UNIT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_6_V001'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    target=AUTHOR/'Generation_V007.json'
    assert not target.exists(),'New manifest only'
    old=read(AUTHOR/'Generation_V006.json');accepted=read(AUTHOR/'Acceptance_V006.json')
    review=read(AUTHOR/'Review_U5a6_V001.json');build=read(UNIT/'Geometry_V001/Build.json')
    before=read(UNIT/'BeforeMutationLiveState.json')
    assert not before['initial_global_dirty']['all_dirty_packages'] and before['level']==accepted['level']
    hashes=read(UNIT/'PreservationHashes.json')
    assert len(hashes)==417 and not [r for r in hashes if sha(ROOT/r['path'])!=r['sha256']]
    geometry=read(UNIT/'Geometry_V001/GeometryCheck.json')
    assert geometry['topology_pass'] and not geometry['new_interferences_other50']
    assert geometry['connection_clearance']['samples']==13221 and geometry['connection_clearance']['pass']
    folder='/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/U5aBody_V007'
    new=[dict(row,asset=folder+'/IntersectionRepair_V001/'+row['name'],
         actor_label=prior['actor_label'],material=prior['material']) for row,prior in zip(build['outputs'],old['outputs'][:3])]
    config={'unit':'U5a-6','status':'native_import_pending','target_level':folder+'/LVI_Ridge_EDTerrain_U5aBody_V007',
        'asset_folder':folder+'/IntersectionRepair_V001','evidence_folder':UNIT.relative_to(ROOT).as_posix(),
        'material':old['material'],'input_config_sha256':old['input_config_sha256'],
        'outputs':new+old['outputs'][3:],'new_outputs':new,'body_outputs':new+[old['outputs'][3]],
        'kept_body':old['outputs'][3],'kept_connection':old['outputs'][4],
        'build_report':(UNIT/'Geometry_V001/Build.json').relative_to(ROOT).as_posix(),
        'build_report_sha256':sha(UNIT/'Geometry_V001/Build.json'),
        'geometry_check':(UNIT/'Geometry_V001/GeometryCheck.json').relative_to(ROOT).as_posix(),
        'geometry_check_sha256':sha(UNIT/'Geometry_V001/GeometryCheck.json'),
        'preservation_files':hashes,'previous_accepted':review['previous_accepted'],
        'actual_pose_baseline':accepted['actual_pose_export'],
        'method':{'geometry':'three sparse local derivative meshes; original UV and triangle indices held; no remesh',
            'kept':'underbody, connection mesh/two materials/core858 vertices,21 owners and26 world poses',
            'role':'visual Static identity NoCollision; collision/crust/Support/bake/PIE deferred'},
        'review':{'accepted':False,'original_candidates':339,'additional_shared_vertex_crossings':126,
            'exported_remaining_crossings':0,'legacy_StairRamp3_review_samples':31,'collision_or_PIE_pass':False},
        'validation':{'native_import':'pending','full_live_fingerprint':'pending','native_vertices':'pending','same26camera':'pending'}}
    assert len(config['outputs'])==5 and len({r['actor_label'] for r in config['outputs']})==5
    target.write_text(json.dumps(config,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')
    print(json.dumps({'new_meshes':len(new),'previous_accepted':accepted['level'],'preserved':len(hashes)}))


if __name__=='__main__':main()
