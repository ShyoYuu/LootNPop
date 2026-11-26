"""U5a-6 실제 검사/26비교를 대조하고 새 V007 검토본의 복구·해시를 마감한다."""
import ast
import hashlib
import json
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parents[3]
AUTHOR=ROOT/'Scripts/Profiling/Graphics/ClosedBodyTrial_V001'
UNIT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_6_V001'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')
def record(p):return {'path':p.relative_to(ROOT).as_posix(),'sha256':sha(p)}


def main():
    frozen=read(UNIT/'PreservationHashes.json')
    assert len(frozen)==len({r['path'].casefold() for r in frozen})==417
    assert not [r for r in frozen if sha(ROOT/r['path'])!=r['sha256']]
    native_path=sorted(UNIT.glob('NativeVerify_*.json'))[-1]
    native=read(native_path);full=read(ROOT/native['full_verify'])
    geometry=read(UNIT/'NativeGeometryCheck_V007.json');comparison=read(UNIT/'ComparisonValidation_V007.json')
    poses=read(UNIT/'ActualPoseExport_V007.json');config=read(AUTHOR/'Generation_V007.json')
    assert full['preservation_pass'] and not full['issues'] and not full['dirty']
    assert native['total_vertices']==73111 and native['total_triangles']==145806 and native['connection']['walking_vertices']==858
    assert not native['dirty'] and geometry['topology_pass']
    assert all(b['remaining_centered_crossing_pairs']==0 and b['topology']['boundary_edges']==b['topology']['nonmanifold_edges']==0 for b in geometry['bodies'])
    assert geometry['protected_spaces']==51 and geometry['samples']==16552 and geometry['other50_new_hits']==0
    assert sum(len(r['hits']) for r in geometry['overlaps'])==31
    assert geometry['connection_clearance']['samples']==13221 and geometry['connection_clearance']['pass']
    assert not geometry['connection_body_volume_hits'] and comparison['pass'] and comparison['camera_count']==26
    assert len(poses['actors'])==26 and max(v for r in poses['actors'] for v in r['source_transform_error'].values())==0
    lines=(ROOT/'Saved/Logs/LootNPop.log').read_text(encoding='utf-8',errors='replace').splitlines()
    start=read(UNIT/'NativeImportWindowStart.json')['log_lines']
    warnings=[{'line':i+1,'text':line} for i,line in enumerate(lines) if i>=start and (': Warning:' in line or ': Error:' in line)]
    tooling=[r for r in warnings if 'File is too large to read' in r['text']]
    rhi=[r for r in warnings if 'Total reserved resource allocated virtual size' in r['text']]
    legacy_source=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_2_V001/ImportWarnings.json'
    legacy_text=legacy_source.read_text(encoding='utf-8')
    legacy=[r for r in warnings if 'PIE: Warning: AttachTo:' in r['text'] and
            'Z1Cull_V001/ASM_Z1_WallNorth' in r['text'] and 'InstancedStaticMesh797' in r['text']]
    for r in legacy:
        suffix=r['text'].split('PIE: Warning: ',1)[1]
        assert suffix.replace("'","\\u0027") in legacy_text or suffix in legacy_text
    other=[r for r in warnings if r not in tooling+rhi+legacy]
    assert not other,other
    warning_report={'start_line':start,'end_line':len(lines),'warnings':warnings,
        'resolved_tool_payload_size_warnings':tooling,'fbx_or_material_warnings':other,
        'legacy_Z1_northwall_AttachTo':legacy,'legacy_warning_evidence':record(legacy_source),
        'RHI_reserved_virtual_budget_review':rhi,
        'RHI_status':'observed after native geometry verification; cause and packaged performance not investigated; verification files passed',
        'blender_export_notes':'relative brush path warnings and thumbnail cache write failure; new FBX/bin/Blend outputs exist and hashes verified; original files unchanged'}
    write(UNIT/'WarningReview_V007.json',warning_report)
    config['status']='native_preservation_geometry_and_same26comparison_complete; user_visual_review_pending'
    config['actual_pose_export']=record(UNIT/'ActualPoseExport_V007.json')
    config['validation']={'native_import':'pass; three new meshes; underbody and connection held',
        'full_live_fingerprint':record(ROOT/native['full_verify']),'native_vertices':record(native_path),
        'native_geometry':record(UNIT/'NativeGeometryCheck_V007.json'),'same26camera':record(UNIT/'ComparisonValidation_V007.json'),
        'warning_review':record(UNIT/'WarningReview_V007.json'),'all_dirty_packages':[],
        'accepted':False,'visual_review_pending':True,'collision_or_PIE_pass':False}
    write(AUTHOR/'Generation_V007.json',config)
    summary={'unit':'U5a-6','target_level':config['target_level'],'status':'implementation_and_actual_validation_complete; user_visual_review_pending',
        'source_preserved_files':417,'source_changed_files':[],
        'previous_accepted':'V006; Acceptance_V006 owns adoption; Generation_V006 production history unchanged',
        'original_candidate_pairs':[12,16,311,0],'additional_shared_vertex_crossings':[4,1,121,0],
        'repaired_crossing_face_pairs':465,'independent_defect_site_count':'not inferred from face pair count',
        'changed_body_vertices':[76,68,2010,0],'new_meshes':3,'kept_existing_meshes':['underbody','connection'],
        'source_uv_hashes_held':True,'native_remaining_crossing_pairs':[0,0,0,0],
        'native_body_vertices':73111,'native_body_triangles':145806,
        'maximum_native_vertex_error_cm':max(r['maximum_vertex_error_cm'] for r in native['bodies']),
        'connection_walking_vertices':858,'connection_maximum_vertex_error_cm':native['connection']['maximum_vertex_error_cm'],
        'preserved_counts':full['counts'],'inherited_fingerprint':full['inherited_fingerprint'],
        'actual_pose_count':26,'pose_error':0,'same_camera_count':26,'camera_error':0,'fov':90,'image_size':[1127,724],
        'protected_spaces':51,'protected_samples':16552,'other50_new_hits':0,'legacy_StairRamp3_review_samples':31,
        'fixed_route_samples':13221,'fixed_route_hits':0,'minimum_surface_distance_lower_bound_cm':geometry['connection_clearance']['minimum_surface_distance_lower_bound_cm'],
        'R01_status':'boundary sensitivity REVIEW retained; original points unchanged',
        'full_dirty_packages':[],'new_FBX_or_material_warnings':0,'resolved_tool_payload_size_warnings':len(tooling),
        'RHI_reserved_virtual_budget_review_warnings':len(rhi),
        'legacy_Z1_northwall_AttachTo_warnings':len(legacy),
        'accepted':False,'visual_review_pending':True,'collision_or_PIE_pass':False,
        'native_verify':record(native_path),'actual_geometry':record(UNIT/'NativeGeometryCheck_V007.json'),
        'comparison':record(UNIT/'ComparisonValidation_V007.json')}
    write(UNIT/'FinalValidation_V007.json',summary)
    scripts=[ROOT/'Scripts/Blender'/n for n in ['ReviewRidgeBodyIntersections.py','ExploreRidgeBodyIntersectionRepair.py',
        'ValidateRidgeBodyLocalRepair.py','RenderRidgeBodyIntersectionReview.py','ExportRidgeBodyIntersectionRepair.py','BuildRidgeBodyRepairComparison.py']]
    scripts += [ROOT/'Scripts/Profiling/Graphics'/n for n in ['InspectRidgeU5a6.py','BuildRidgeBodyIntersectionReview.py',
        'PrepareRidgeU5a6Review.py','PrepareRidgeBodyIntersectionImport.py','VerifyRidgeBodyIntersectionRepair.py','FinalizeRidgeBodyIntersectionRepair.py']]
    for p in scripts:ast.parse(p.read_text(encoding='utf-8'))
    docs=[ROOT/'.context/Graphics/analysis/Phase01_U5aSelfIntersectionReview.md',ROOT/'.context/Graphics/Current.md',
        ROOT/'.context/Graphics/README.md',ROOT/'.context/Graphics/analysis/Phase01_U5aBodyHandoff.md',
        ROOT/'.context/Graphics/phases/Phase01_RidgeReferencePatch.md',ROOT/'.context/Graphics/history/Phase01_Log.md',ROOT/'.gitattributes']
    maps=[ROOT/'Content'/(config['target_level'].removeprefix('/Game/')+'.umap')]
    maps += [ROOT/'Content'/(r['asset'].removeprefix('/Game/')+'.uasset') for r in config['new_outputs']]
    paths=maps+scripts+docs+[AUTHOR/'Generation_V007.json',AUTHOR/'Review_U5a6_V001.json']
    paths += [p for p in (UNIT/'Geometry_V001').iterdir() if p.is_file()]
    paths += list((UNIT/'NativeGeometry_V007').glob('*.bin'))
    paths += [UNIT/n for n in ['InitialEditorState.json','BeforeMutationLiveState.json','ReferencedV006LivePreflight.json',
        'PreservationHashes.json','PreservationAudit.json','CandidateClassification.json','CandidateReview.html','PairReviewData.json',
        'LocalRepairValidation.json','NativeImport_V007.json','NativeImportOrchestration.py','ActualPoseExport_V007.json',
        'NativeComponentState_V007.json','NativeGeometryCheck_V007.json','BeforeCaptureMetadata.json','AfterCaptureMetadata.json',
        'ComparisonValidation_V007.json','Comparison_V007.html','WarningReview_V007.json','FinalValidation_V007.json']]
    paths += [native_path,ROOT/native['full_verify'],legacy_source]+list(UNIT.glob('ComparisonSheet_*.png'))
    paths += list((UNIT/'Views_Before').glob('*.png'))+list((UNIT/'Views_After').glob('*.png'))
    backup=UNIT/'FinalReview_V007';rows=[]
    for p in dict.fromkeys(paths):
        destination=backup/p.relative_to(ROOT);destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(p,destination);assert sha(p)==sha(destination)
        rows.append(dict(record(p),backup=destination.relative_to(ROOT).as_posix()))
    write(UNIT/'FinalReviewHashes_V007.json',rows)
    assert not [r for r in frozen if sha(ROOT/r['path'])!=r['sha256']]
    print(json.dumps({'source_files':417,'new_recovery_files':len(rows),'actual_geometry_crossings':0,
        'same_camera_count':26,'native_vertex_error_cm':summary['maximum_native_vertex_error_cm'],
        'RHI_budget_review_warnings':len(rhi),'accepted':False}))


if __name__=='__main__':main()
