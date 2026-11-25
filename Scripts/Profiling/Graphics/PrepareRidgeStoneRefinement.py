"""V004에서 고정한 동선의 새 석재 외형을 V005 사본에만 연결한다."""
import hashlib
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
AUTHOR=ROOT/'Scripts/Profiling/Graphics/ClosedBodyTrial_V001'
UNIT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_4_V001';GEO=UNIT/'Geometry_V001'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    target=AUTHOR/'Generation_V005.json';assert not target.exists(),'New manifest only'
    old=read(AUTHOR/'Generation_V004.json');b=read(GEO/'Build.json');q=read(GEO/'GeometryCheck.json')
    hashes=read(UNIT/'PreservationHashes.json')
    assert not [x['path'] for x in hashes if sha(ROOT/x['path'])!=x['sha256']]
    assert q['topology_pass'] and q['self_intersection_candidate_pairs']==0
    assert q['walking_surface_max_error_cm']<.01 and q['walk_max_triangle_grade_deg']<45
    assert q['route_clearance']['pass'] and not q['new_interferences_other50']
    folder='/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/U5aBody_V005'
    row=b['outputs'][0];new=dict(row,asset=folder+'/ConnectionStone_V001/'+row['name'],
        actor_label=old['new_outputs'][0]['actor_label'],material=old['material'],
        material_assignments={'RidgeWalk_Surface':old['material'],'RidgeStone_Sides':'/Game/SmartAssets/AssetMaterials/MI_Cliff_01'})
    assert not (ROOT/'Content'/(new['asset'].removeprefix('/Game/')+'.uasset')).exists()
    c={**old,'unit':'U5a-4','status':'stone_refinement_native_import_pending','target_level':folder+'/LVI_Ridge_EDTerrain_U5aBody_V005',
        'evidence_folder':UNIT.relative_to(ROOT).as_posix(),'asset_folder':folder+'/ConnectionStone_V001',
        'outputs':old['outputs'][:4]+[new],'new_outputs':[new],
        'build_report':(GEO/'Build.json').relative_to(ROOT).as_posix(),'build_report_sha256':sha(GEO/'Build.json'),
        'geometry_check':(GEO/'GeometryCheck.json').relative_to(ROOT).as_posix(),'geometry_check_sha256':sha(GEO/'GeometryCheck.json'),
        'preservation_files':hashes,'previous_trial':{'map':old['target_level'],'generation':'Generation_V004.json',
            'accepted_scope':'route and central3m walking surface; previous map/assets frozen; body and physical gates deferred'},
        'route_acceptance':(UNIT/'RouteAcceptance.json').relative_to(ROOT).as_posix(),
        'method':{'geometry':'Read frozen Connection_V011 binary; copy central3m face vertices; reshape only outside and below',
            'supports':'Four integrated corbels touch existing visible structural surfaces; no old actor changed',
            'materials':new['material_assignments'],'import':'Native MCP new mesh only; replace route mesh only in new V005',
            'verify':'RidgeClosedBodyTrial.py verify V005; no legacy import command',
            'role':'visual NoCollision; collision/crust/support/bake/PIE deferred'},
        'review':{'accepted':False,'route_kept':True,'walk_core_max_error_cm':q['walking_surface_max_error_cm'],
            'support_count':len(q['supports']),'protected_pass':False,'other50_new_hits':0,
            'connection_legacy_hits':sum(len(x['hits']) for x in q['new_connection_protection'] if x['id']=='StairRamp3CentralPassage'),
            'collision_or_PIE_pass':False},
        'validation':{'native_import':'pending','full_live_fingerprint':'pending','accepted':False}}
    target.write_text(json.dumps(c,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')
    (UNIT/'NewImport.json').write_text(json.dumps(new,indent=1),encoding='utf-8')
    print(json.dumps({'new_mesh':new['name'],'triangles':new['triangles'],'source_files':len(hashes),
        'core_error_cm':q['walking_surface_max_error_cm'],'supports':len(q['supports'])}))
if __name__=='__main__':main()
