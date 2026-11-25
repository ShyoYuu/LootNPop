"""U5a-5 새4몸체 반입 목록을 만들고 유지한 연결을 명시한다."""
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
UNIT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_5_V001'
GEO=UNIT/'Geometry_V005'
AUTHOR=ROOT/'Scripts/Profiling/Graphics/ClosedBodyTrial_V001'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    path=AUTHOR/'Generation_V006.json'
    if path.exists():assert read(path)['unit']=='U5a-5' and read(path)['status']=='body_caps_native_import_pending'
    old=read(AUTHOR/'Generation_V005.json');build=read(GEO/'Build.json');geometry=read(GEO/'GeometryCheck.json')
    hashes=read(UNIT/'PreservationHashes.json')
    assert len(hashes)==333 and not [r['path'] for r in hashes if sha(ROOT/r['path'])!=r['sha256']]
    assert geometry['topology_pass'] and not geometry['new_interferences_other50']
    assert geometry['connection_clearance']['pass'] and not geometry['connection_body_volume_hits']
    assert geometry['probes']['R03']['inside_bodies'] and geometry['probes']['R04']['inside_bodies']
    assert geometry['self_intersection_candidate_pairs']<=340
    folder='/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/U5aBody_V006'
    material=folder+'/MI_Ridge_U5a_CapRock_V001'
    new=[dict(row,asset=folder+'/BodyCaps_V005/'+row['name'],actor_label=prior['actor_label'],material=material)
         for row,prior in zip(build['outputs'],old['outputs'][:4])]
    for row in new:assert not (ROOT/'Content'/(row['asset'].removeprefix('/Game/')+'.uasset')).exists()
    c={**old,'unit':'U5a-5','status':'body_caps_native_import_pending','target_level':folder+'/LVI_Ridge_EDTerrain_U5aBody_V006',
       'asset_folder':folder+'/BodyCaps_V005','evidence_folder':UNIT.relative_to(ROOT).as_posix(),
       'material':material,'outputs':new+[old['outputs'][4]],'new_outputs':new,
       'build_report':(GEO/'Build.json').relative_to(ROOT).as_posix(),'build_report_sha256':sha(GEO/'Build.json'),
       'geometry_check':(GEO/'GeometryCheck.json').relative_to(ROOT).as_posix(),'geometry_check_sha256':sha(GEO/'GeometryCheck.json'),
       'preservation_files':hashes,'previous_trial':{'map':old['target_level'],'generation':'Generation_V005.json',
            'accepted_scope':'Keep V005 connection mesh, two materials and26 transforms; previous body trial preserved as source'},
       'connection_acceptance':(UNIT/'ConnectionAcceptance.json').relative_to(ROOT).as_posix(),
       'kept_connection':old['outputs'][4],
       'method':{'geometry':'read frozen V007 bodies;25cm repair/tapered relief on two large caps; third shape held; lower outer underside top',
            'material':'duplicate MI_Cliff_01; neutral unique normal and unique AO zero;4m dominant-axis UV; shared parent unchanged',
            'verify':'RidgeBodyCapRefinement.py verify V006; never import V006',
            'role':'visual NoCollision; remaining self-intersection/ramp overlaps REVIEW; collision and PIE deferred'},
       'review':{'accepted':False,'connection_kept':True,'protected_pass':False,
            'old_self_intersection_candidates':1291,'new_self_intersection_candidates':geometry['self_intersection_candidate_pairs'],
            'connection_and_body_legacy_ramp_hits':31,'other50_new_hits':0,'collision_or_PIE_pass':False},
       'validation':{'native_import':'pending','full_live_fingerprint':'pending','accepted':False}}
    path.write_text(json.dumps(c,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')
    (UNIT/'NewImport.json').write_text(json.dumps(new,indent=1)+'\n',encoding='utf-8')
    print(json.dumps({'new_body_count':4,'triangles':sum(r['triangles'] for r in new),'preserved_files':333,
        'self_intersection_candidates':geometry['self_intersection_candidate_pairs'],'route_hits':0,'legacy_ramp_hits':31}))
if __name__=='__main__':main()
