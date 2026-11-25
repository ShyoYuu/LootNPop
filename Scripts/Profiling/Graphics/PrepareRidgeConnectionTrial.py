"""새 연결로 한 개의 MCP 반입과 기존 검사기용5개 저작 기준을 준비한다."""
import hashlib
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
AUTHOR=ROOT/'Scripts/Profiling/Graphics/ClosedBodyTrial_V001'
UNIT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_3_V001'
GEO=UNIT/'Geometry_V008'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    path=AUTHOR/'Generation_V004.json';assert not path.exists(),'New manifest only'
    old=read(AUTHOR/'Generation_V003.json');build=read(GEO/'Build.json');check=read(GEO/'GeometryCheck.json')
    hashes=read(UNIT/'PreservationHashes.json')
    assert not [r['path'] for r in hashes if sha(ROOT/r['path'])!=r['sha256']]
    assert check['topology_pass'] and check['self_intersection_candidate_pairs']==0
    assert check['route_clearance']['pass'] and not check['route_body_volume_hits']
    assert check['walk_max_triangle_grade_deg']<45 and not check['new_interferences_other50']
    folder='/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/U5aBody_V004'
    row=build['outputs'][0]
    output=dict(row,asset=folder+'/ConnectionMeshes_V008/'+row['name'],actor_label='CONN_StairRamp3_V008',
        material=old['material'])
    assert sha(ROOT/output['file'])==output['sha256']
    assert not (ROOT/'Content'/(output['asset'].removeprefix('/Game/')+'.uasset')).exists()
    c={**old,'unit':'U5a-3','status':'connection_ready_for_native_MCP_import',
        'target_level':folder+'/LVI_Ridge_EDTerrain_U5aBody_V004','evidence_folder':UNIT.relative_to(ROOT).as_posix(),
        'asset_folder':folder+'/ConnectionMeshes_V008','outputs':old['outputs']+[output],
        'new_outputs':[output],'build_report':(GEO/'Build.json').relative_to(ROOT).as_posix(),
        'build_report_sha256':sha(GEO/'Build.json'),'geometry_check':(GEO/'GeometryCheck.json').relative_to(ROOT).as_posix(),
        'geometry_check_sha256':sha(GEO/'GeometryCheck.json'),'preservation_files':hashes,
        'previous_trial':{'map':old['target_level'],'generation':'Generation_V003.json',
            'status':'unadopted body REVIEW; preserve all4 poses, meshes and materials'},
        'method':{'new_geometry':'Only one new closed stone connection; 3m walking/5m visual width',
            'import':'StaticMeshTools.import_file for new_outputs only; do not call legacy import V004',
            'verify':'RidgeClosedBodyTrial.py verify V004 checks original Z1, accepted21 and all5 trial actors',
            'endpoints':'Retained Floor/Crust and structural native surface rays, approx5cm offset across3m',
            'protected_spaces':'Original51 retained; legacy StairRamp3 body28 and connection2 reported separately',
            'role':'visual NoCollision; no collision/crust/support/bake/PIE changes'},
        'review':{'accepted':False,'protected_pass':False,'route_sample_pass':True,
            'walk_max_triangle_grade_deg':check['walk_max_triangle_grade_deg'],
            'legacy_body_hits':28,'new_connection_legacy_hits':2,'other50_new_hits':0,
            'collision_or_PIE_pass':False},
        'validation':{'actual_native_import':'pending','full_live_fingerprint_verify':'pending','accepted':False},
        'deferred':['visual adoption','body cap/material refinements','collision derivation',
            'interior crust/support removal','bake','manual approach PIE','regional rollout']}
    path.write_text(json.dumps(c,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')
    (UNIT/'NewImport.json').write_text(json.dumps(output,indent=1),encoding='utf-8')
    print(json.dumps({'new_mesh':output['name'],'triangles':output['triangles'],'preserved':len(hashes),
        'grade':check['walk_max_triangle_grade_deg'],'route_samples':check['route_clearance']['samples'],
        'route_length_m':build['route_length_cm']/100,'seam_min_m':check['seam_min_cm']/100}))

if __name__=='__main__':main()
