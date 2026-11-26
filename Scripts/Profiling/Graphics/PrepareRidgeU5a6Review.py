"""V006 채택 기준의 U5a-6 검토 manifest와 오프라인 복구본을 정리한다."""
import hashlib
import json
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parents[3]
AUTHOR=ROOT/'Scripts/Profiling/Graphics/ClosedBodyTrial_V001'
UNIT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_6_V001'


def read(path):return json.loads(path.read_text(encoding='utf-8'))
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def row(path):return {'path':path.relative_to(ROOT).as_posix(),'sha256':sha(path)}
def write(path,value):path.write_text(json.dumps(value,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')


def main():
    accepted=read(AUTHOR/'Acceptance_V006.json')
    generation=read(AUTHOR/'Generation_V006.json')
    preserved=read(UNIT/'PreservationHashes.json')
    assert len(preserved)==417 and len({r['path'].casefold() for r in preserved})==417
    assert not [r for r in preserved if sha(ROOT/r['path'])!=r['sha256']]
    actual=read(UNIT/'ReferencedV006LivePreflight.json')
    assert len(actual['poses'])==26 and not actual['issues'] and not actual['checked_asset_dirty']
    poses=accepted['poses']
    assert len(poses)==len({r['label'] for r in poses})==26
    evidence=['PreservationAudit.json','CandidateClassification.json','LocalRepairValidation.json',
              'ReferencedV006LivePreflight.json','PairReviewData.json','CandidateReview.html','PairRenderMetadata.json',
              'ConstrainedRepairExploration.json']
    classification=read(UNIT/'CandidateClassification.json')
    classification['source_map']=accepted['level']
    write(UNIT/'CandidateClassification.json',classification)
    manifest={'unit':'U5a-6','status':'classification_and_offline_local_proposal_ready; global dirty inspection pending',
      'previous_accepted':{'map':accepted['level'],'acceptance':row(AUTHOR/'Acceptance_V006.json'),
          'frozen_inputs':row(AUTHOR/'FrozenInputs_V006.json'),'generation':row(AUTHOR/'Generation_V006.json'),
          'actual_pose_export':accepted['actual_pose_export'],'poses':poses,
          'accepted_owner_count':21,'body_count':4,'connection_count':1,'outputs':generation['outputs']},
      'target':{'map_version':'U5aBody_V007','created':False,'generation_manifest_created':False},
      'classification':{'original_candidates':[12,16,311,0],'proper_crossings':[12,16,311,0],
          'additional_shared_vertex_crossings':[4,1,121,0],'total_crossing_face_pairs':465,
          'independent_defect_site_count':'not inferred from face pair count','coplanar_contact_or_degenerate_pairs':0,
          'methods':['float64 triangle supporting plane section intervals and strict barycentric midpoint',
                     'independent bidirectional Moller segment triangle intersections'],
          'self_check':'six geometric cases, both argument orders'},
      'proposal':{'method':'current crossing faces plus one ring local relaxation; resolved areas stop; original axis extrema fixed',
          'moved_vertices':[76,68,2010,0],'maximum_movement_cm':[5.833753715989666,6.44495560091409,7.257893441758019,0],
          'topology_unchanged':True,'remaining_centered_bvh_crossing_pairs':[0,0,0,0],
          'whole_voxel_remesh':False,'native_import':'pending','appearance_acceptance':'pending same26 camera native comparison'},
      'protected':{'spaces':51,'samples':16552,'other50_new_hits':0,'legacy_StairRamp3_review_samples':31,
          'fixed_route_samples':13221,'fixed_route_body_hits':0,'minimum_surface_distance_lower_bound_cm':252.32212829589844,
          'collision_or_PIE_pass':False},
      'live':{'active_level':actual['active_level'],'referenced_source_pose_error':0,
          'body_and_connection_components':'match accepted records','relevant_dirty_assets':[],
          'all_dirty_packages':'pending manual Python execution; first Cmd attempt produced no result'},
      'preservation_files':preserved,'evidence':[row(UNIT/name) for name in evidence],
      'source_policy':'V006/old maps/shared assets/Generation_V006 accepted=false and previous_trial history remain untouched'}
    target=AUTHOR/'Review_U5a6_V001.json'
    write(target,manifest)
    backup=UNIT/'OfflineReviewDraft'
    paths=[target]+[UNIT/name for name in evidence]+list(UNIT.glob('Pair_*.png'))+list(UNIT.glob('*_ConstrainedProposal.npz'))
    paths+=list((ROOT/'Scripts/Blender').glob('*RidgeBody*Review.py'))
    paths+=[ROOT/'Scripts/Blender'/name for name in ['ReviewRidgeBodyIntersections.py','ExploreRidgeBodyIntersectionRepair.py','ValidateRidgeBodyLocalRepair.py']]
    paths+=[ROOT/'Scripts/Profiling/Graphics'/name for name in ['PrepareRidgeU5a6Review.py','BuildRidgeBodyIntersectionReview.py','InspectRidgeU5a6.py']]
    rows=[]
    for path in dict.fromkeys(paths):
        destination=backup/path.relative_to(ROOT);destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(path,destination)
        assert sha(destination)==sha(path)
        rows.append(dict(row(path),backup=destination.relative_to(ROOT).as_posix()))
    write(UNIT/'OfflineReviewHashes.json',rows)
    print(json.dumps({'previous_accepted':'V006','preservation_files':417,'offline_recovery_files':len(rows),
                      'new_map_created':False,'native_work':'pending global dirty result'}))


if __name__=='__main__':main()
