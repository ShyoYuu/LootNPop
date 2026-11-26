"""U5a-7 실제 형상·보존·27비교와 최종 V008 복구본을 대조한다."""
import ast
import base64
import hashlib
import json
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parents[3]
AUTHOR=ROOT/'Scripts/Profiling/Graphics/ClosedBodyTrial_V001'
UNIT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_7_V001'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')
def record(p):return {'path':p.relative_to(ROOT).as_posix(),'sha256':sha(p)}


def main():
    frozen=read(UNIT/'PreservationHashes.json')
    assert len(frozen)==len({r['path'].casefold() for r in frozen})==534
    assert all(sha(ROOT/r['path'])==r['sha256'] for r in frozen)
    for row in read(ROOT/'Saved/Profiling/Graphics/Phase01/U5a_6_V001/AcceptedHandoffHashes_V007.json'):
        assert sha(ROOT/row['backup'])==row['sha256']
    native_path=sorted(UNIT.glob('NativeVerify_*.json'))[-1];native=read(native_path)
    full=read(ROOT/native['full_verify']);g=read(UNIT/'NativeGeometryCheck_V008.json')
    compare=read(UNIT/'ComparisonValidation_V008.json');poses=read(UNIT/'ActualPoseExport_V008.json')
    imported=read(UNIT/'NativeImport_V008.json');state=read(UNIT/'FinalEditorState_V008.json')
    config=read(AUTHOR/'Generation_V008.json');reg=g['regression']
    assert full['preservation_pass'] and not full['issues'] and not full['dirty'] and not native['dirty']
    assert native['total_vertices']==73273 and native['total_triangles']==146126
    assert native['connection']['walking_vertices']==858 and len(native['connection']['materials'])==2
    assert native['held_body_count']==4 and g['held4_actual_binary_hashes_match_V007']
    assert len(g['actual_bodies'])==5 and all(b['actual_self_intersection_pairs']==0 and b['topology']['boundary_edges']==b['topology']['nonmanifold_edges']==0 for b in g['actual_bodies'])
    assert g['target_inside'] and g['user_camera_outside_new_body'] and not g['outside_original_Cliff09_hull_vertices']
    assert reg['spaces']==51 and reg['samples']==16552 and reg['other50_new_hits']==0 and reg['legacy_review_samples']==31
    assert sum(len(r['hits']) for r in reg['overlaps'])==31
    assert reg['fixed_connection_clearance']['samples']==13221 and reg['fixed_connection_clearance']['pass'] and not reg['fixed_connection_body_volume_hits']
    assert g['original_R01_grid']['before_misses']==9 and g['original_R01_grid']['after_union_misses']==0
    assert compare['pass'] and compare['camera_count']==27 and len(compare['comparisons'])==27
    assert max(v for row in compare['comparisons'] for v in row['camera_errors'].values())==0
    assert poses['source_pose_count']==26 and poses['new_pose_count']==1 and poses['actor_count']==27
    assert max(v for row in poses['actors'] if row['kind']=='held_V007' for v in row['source_transform_error'].values())==0
    assert not poses['map_dirty'] and not poses['pie'] and not state['IsPIERunning'] and not any(state['relevant_dirty'].values())
    assert state['GetSelectedActors']==[imported['new_actor']]
    assert imported['component_properties']['bodyInstance']['collisionEnabled']=='NoCollision'
    assert config['previous_accepted']['map']==read(AUTHOR/'Acceptance_V007.json')['level']
    assert len(config['outputs'])==6 and len(config['body_outputs'])==5 and len(config['new_outputs'])==1
    assert config['new_outputs'][0]['name']=='SM_Ridge_U5a_R01RockBackfill_V006'
    assert sha(ROOT/config['build_report'])==config['build_report_sha256'] and sha(ROOT/config['geometry_check'])==config['geometry_check_sha256']
    for row in native['bodies']:assert sha(ROOT/row['binary'])==row['sha256']
    # 로그에는 외부 통계 URL/식별자를 복사하지 않고 종류와 줄 번호만 남긴다.
    lines=(ROOT/'Saved/Logs/LootNPop.log').read_text(encoding='utf-8',errors='replace').splitlines()
    start=read(UNIT/'FullVerifyWindowStart_V008.json')['log_lines']
    bounded=[(i+1,s) for i,s in enumerate(lines) if i>=start and (': Warning:' in s or ': Error:' in s)]
    resolved_tool_errors=[i for i,s in bounded if "Toolset 'EditorAppToolset' not found" in s or "Toolset 'SceneTools' not found" in s]
    resolved_path_warnings=[i for i,s in bounded if 'LogScript: Warning: Path ' in s and 'finaleditorstate_v008.json' in s and 'not within an allowed root' in s]
    errors=[i for i,s in bounded if ': Error:' in s and i not in resolved_tool_errors]
    legacy=[i for i,s in bounded if 'PIE: Warning: AttachTo:' in s and 'ASM_Z1_WallNorth' in s and 'InstancedStaticMesh797' in s]
    http=[i for i,s in bounded if 'LogHttp: Warning:' in s]
    other=[{'line':i,'category':s.split(']')[-1].split(':')[0]} for i,s in bounded if i not in errors+legacy+http+resolved_tool_errors+resolved_path_warnings]
    assert not errors and not other,(errors,other)
    legacy_path=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_2_V001/ImportWarnings.json'
    assert 'InstancedStaticMesh797' in legacy_path.read_text(encoding='utf-8')
    warn={'full_verify_start_line':start,'end_line':len(lines),'full_verify_error_lines':errors,
        'legacy_Z1_AttachTo_lines':legacy,'legacy_evidence':record(legacy_path),'http_warning_lines':http,
        'later_resolved_toolset_lookup_error_lines':resolved_tool_errors,'later_resolved_relative_path_warning_lines':resolved_path_warnings,
        'http_status':'Epic editor analytics upload libcurl65 after native PASS; no mesh validation failure; external identifiers omitted',
        'new_FBX_or_material_warnings_in_full_verify':0,'other_full_verify_warnings':other,
        'production_setup':'earlier MCP orchestration assertion, input SyntaxError and toolset short-name lookup errors resolved; V003/V004 rejected geometry drafts retained; collision explicitly disabled after default-collision flag',
        'existing_render_reviews':['V007 RHI reserved virtual258GB /256GB budget','Lumen surface cache atlas oversubscribed visible in before captures; no CVar changes'],
        'geometry_checker_correction':'native UE winding sign compared to frozen V007; authoring/UE sign difference is not a mesh repair'}
    write(UNIT/'WarningReview_V008.json',warn)
    config['status']='native_preservation_geometry_and_same27comparison_complete; user_visual_review_pending'
    config['actual_pose_export']=record(UNIT/'ActualPoseExport_V008.json')
    config['review']['R01_user_report']='visible opening confirmed; user camera in front of rock retained outside; backfill target368.165cm along view inside exactCliff09'
    config['review']['R01_original_lower_grid']='historical9 misses retained; actual final bounded81ray union misses0; whole R01/access REVIEW'
    config['validation']={'native_import':record(UNIT/'NativeImport_V008.json'),'full_native_verify':record(ROOT/native['full_verify']),
        'native_vertices':record(native_path),'actual_geometry':record(UNIT/'NativeGeometryCheck_V008.json'),
        'same27camera':record(UNIT/'ComparisonValidation_V008.json'),'warning_review':record(UNIT/'WarningReview_V008.json'),
        'all_dirty_packages_at_native_verify':[],'accepted':False,'visual_review_pending':True,'collision_or_PIE_pass':False}
    write(AUTHOR/'Generation_V008.json',config)
    summary={'unit':'U5a-7','level':config['target_level'],'previous_accepted':'V007 Acceptance/FrozenInputs; production history preserved',
        'source_preserved_files':534,'source_changed_files':[],'held_body_binary_hashes_equal':4,'new_bodies':1,
        'new_vertices':162,'new_triangles':320,'actual_new_volume_m3':g['actual_backfill_volume_m3'],
        'native_body_vertices':73273,'native_body_triangles':146126,'actual_body_self_crossing_pairs':[0]*5,
        'native_maximum_vertex_error_cm':max(b['maximum_vertex_error_cm'] for b in native['bodies']),
        'connection_walking_vertices':858,'existing_pose_count':26,'new_pose_count':1,'existing_pose_error':0,
        'same_camera_count':27,'camera_error':0,'protected_spaces':51,'protected_samples':16552,'other50_new_hits':0,
        'legacy_StairRamp3_review_samples':31,'fixed_route_samples':13221,'fixed_route_hits':0,
        'minimum_surface_distance_lower_bound_cm':reg['fixed_connection_clearance']['minimum_surface_distance_lower_bound_cm'],
        'original_R01_grid_misses_before_after':[9,0],'whole_R01_and_original_path_status':'REVIEW; finite ray evidence does not establish access or entire closure',
        'full_dirty_packages_at_native_verify':[],'preserved_counts':full['counts'],'inherited_fingerprint':full['inherited_fingerprint'],
        'full_verify_error_count':0,'legacy_AttachTo_warnings':len(legacy),'analytics_HTTP_warning_rows':len(http),
        'accepted':False,'visual_review_pending':True,'collision_or_PIE_pass':False,
        'native_verify':record(native_path),'actual_geometry':record(UNIT/'NativeGeometryCheck_V008.json'),'comparison':record(UNIT/'ComparisonValidation_V008.json')}
    write(UNIT/'FinalValidation_V008.json',summary)
    for name in ['UserGapView','UserGapAfter_V006','R01WestPreview_V006']:
        p=UNIT/(name+'.txt');p.with_suffix('.png').write_bytes(base64.b64decode(p.read_text(encoding='utf-8-sig')))
    scripts=[ROOT/'Scripts/Blender'/n for n in ['ReviewRidgeBodyBoundary.py','BuildRidgeR01JoinBody.py','BuildRidgeR01LocalClosure.py','RefineRidgeR01ClosureHull.py','BuildRidgeR01RockBackfill.py','ValidateRidgeR01Backfill.py','BuildRidgeR01Comparison.py']]
    scripts += [ROOT/'Scripts/Profiling/Graphics'/n for n in ['BuildRidgeBodyBoundaryReview.py','PrepareRidgeR01Closure.py','VerifyRidgeR01Backfill.py','FinalizeRidgeR01Backfill.py']]
    for p in scripts:ast.parse(p.read_text(encoding='utf-8'))
    docs=[ROOT/'.context/Graphics'/n for n in ['Current.md','README.md','analysis/Phase01_U5aBodyHandoff.md','analysis/Phase01_U5aSelfIntersectionReview.md','analysis/Phase01_U5aBoundaryReview.md','phases/Phase01_RidgeReferencePatch.md','history/Phase01_Log.md']]+[ROOT/'.gitattributes']
    paths=[ROOT/'Content'/(config['target_level'].removeprefix('/Game/')+'.umap')]
    paths += [ROOT/'Content'/(o['asset'].removeprefix('/Game/')+'.uasset') for o in config['new_outputs']]
    paths += scripts+docs+[AUTHOR/'Generation_V008.json']
    # 초안 복구본도 보존하고 최종 복구 폴더 자체는 중복 수집하지 않는다.
    paths += [p for p in UNIT.rglob('*') if p.is_file() and not any(x.startswith('FinalReview') for x in p.relative_to(UNIT).parts) and p.suffix not in {'.txt','.blend1'} and '__pycache__' not in p.parts]
    backup=UNIT/'FinalReview_V008';rows=[]
    for p in dict.fromkeys(paths):
        dest=backup/p.relative_to(ROOT);dest.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(p,dest);assert sha(p)==sha(dest)
        rows.append(dict(record(p),backup=dest.relative_to(ROOT).as_posix()))
    write(UNIT/'FinalReviewHashes_V008.json',rows)
    assert all(sha(ROOT/r['path'])==r['sha256'] for r in frozen)
    assert all(sha(ROOT/r['path'])==sha(ROOT/r['backup'])==r['sha256'] for r in rows)
    print(json.dumps({'source_preserved':534,'recovery_files':len(rows),'native_vertices':73273,'same_cameras':27,'accepted':False}))


if __name__=='__main__':main()
