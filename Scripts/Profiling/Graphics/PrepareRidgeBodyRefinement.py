"""U5a-2 새 반입 저작본과 비교 카메라를 준비한다. UE 에셋은 수정하지 않는다."""
import hashlib
import json
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parents[3]
AUTHOR=ROOT/'Scripts/Profiling/Graphics/ClosedBodyTrial_V001'
OUT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_2_V001'
GEOM=ROOT/'Saved/Profiling/Graphics/Phase01/U5aBody_V007'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,data):p.write_text(json.dumps(data,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')

def main():
    destination=AUTHOR/'Generation_V003.json'
    assert not destination.exists(),'New manifest only; do not overwrite prior work'
    previous=read(AUTHOR/'Generation_V002.json')
    hashes={r['path']:r['sha256'] for r in previous['preservation_files']}
    hashes.update({r['path']:r['sha256'] for r in read(ROOT/'Saved/Profiling/Graphics/Phase01/U5a_1_V002/FinalReviewHashes.json')})
    for p,digest in hashes.items():assert sha(ROOT/p)==digest,p
    build=read(GEOM/'Build.json');check=read(GEOM/'GeometryCheck.json')
    assert check['topology_pass'] and check['protected_spaces']==51 and len(build['outputs'])==4
    assert not any(r['hits'] for r in check['overlaps'] if r['id']!='StairRamp3CentralPassage')
    for key in ('R03','R04'):
        assert any(r['inside'] and 'UnderSurface' in r['body'] for r in check['probes'][key]['bodies'])
    folder='/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/U5aBody_V003'
    material=folder+'/MI_Ridge_U5a_Body_V001'
    material_file=ROOT/'Content'/(material.removeprefix('/Game/')+'.uasset')
    assert material_file.exists(),'Prepare and save private material instance first'
    outputs=[]
    for old in build['outputs']:
        assert sha(ROOT/old['file'])==old['sha256']
        outputs.append(dict(old,asset=folder+'/TrialMeshes_V007/'+old['name'],
            actor_label=old['name'].replace('SM_Ridge_U5a_','BODY_'),material=material))
    config={**previous,'unit':'U5a-2','status':'boundary_refinement_ready_connection_manual',
        'target_level':folder+'/LVI_Ridge_EDTerrain_U5aBody_V003',
        'evidence_folder':OUT.relative_to(ROOT).as_posix(),'asset_folder':folder+'/TrialMeshes_V007',
        'material':material,'material_sha256':sha(material_file),'outputs':outputs,
        'build_report':(GEOM/'Build.json').relative_to(ROOT).as_posix(),'build_report_sha256':sha(GEOM/'Build.json'),
        'geometry_check':(GEOM/'GeometryCheck.json').relative_to(ROOT).as_posix(),'geometry_check_sha256':sha(GEOM/'GeometryCheck.json'),
        'preservation_files':[{'path':p,'sha256':d} for p,d in sorted(hashes.items())],
        'previous_trial':{'generation':'Generation_V002.json','map':previous['target_level'],
            'status':'unadopted REVIEW; preserved; not an accepted source'},
        'method':{'source':'Read frozen V006 binaries; previous generators not rerun',
            'scan_bodies':'Same saved V006 geometry; new V007 imports with private material slot',
            'under_surface':'125cm boundary taper, core radius225cm; outer toe ground+15cm; same closed lattice',
            'material':'Private copy of existing world/local projected MassifSurface_V003 instance; shared parent untouched',
            'role':'visual NoCollision; original51 protected spaces retained'},
        'review':{'accepted':False,'topology_pass':True,'protected_pass':check['protected_pass'],
            'conflicts':[{'id':r['id'],'hits':len(r['hits'])} for r in check['overlaps'] if r['hits']],
            'connection_status':'Not placed; manual landing selection before further route generation',
            'collision_or_PIE_pass':False},
        'deferred':['manual north landing selection','separate connection placement','visual adoption',
            'collision derivation','interior crust/support removal','bake','manual approach PIE','regional rollout']}
    OUT.mkdir(parents=True,exist_ok=True)
    # 읽기 요청 전에 새 카메라 파일을 먼저 준비한다. 이전 File-not-found 순서 오류를 반복하지 않는다.
    shutil.copyfile(ROOT/'Saved/Profiling/Graphics/Phase01/U5a_1_V002/ComparisonCameras.json',OUT/'ComparisonCameras.json')
    write(destination,config)
    write(OUT/'Preparation.json',{'status':'awaiting actual editor import',
        'previous_files_preserved':len(hashes),'new_material':material,
        'new_geometry_outputs':len(outputs),'triangles':sum(r['triangles'] for r in outputs),
        'body_refinements':build['body_refinements'],'protected_conflicts':config['review']['conflicts'],
        'before_images':'Saved/Profiling/Graphics/Phase01/U5a_1_V002/Views_Draft',
        'actual_import_verify_and_after_capture':'pending; do not claim PASS from preparation'})
    print(json.dumps({'manifest':str(destination),'previous_files':len(hashes),
        'outputs':len(outputs),'protected_conflicts':config['review']['conflicts']}))

if __name__=='__main__':main()
