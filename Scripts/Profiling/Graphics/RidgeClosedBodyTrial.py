"""새 U5a 사본에만 생성 몸체를 반입하고 채택 배치 보존을 검사한다.

UE Cmd: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/RidgeClosedBodyTrial.py" import V002
반입 뒤 보존 검사를 자동 실행한다. verify V002는 읽기 전용 재검사다.
기존 V011/공유 에셋·충돌·지각·베이크는 수정하지 않는다.
"""
import datetime
import hashlib
import json
from pathlib import Path
import sys

import unreal

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
sys.path[:0]=[str(HERE),str(ROOT/'Scripts')]
import RidgeZ1Layout as layout

AUTHOR=HERE/'ClosedBodyTrial_V001'
CONFIG=AUTHOR/('Generation_'+(sys.argv[2] if len(sys.argv)>2 else 'V001')+'.json')
OUT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_1_V001'

def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def asset_file(p):return ROOT/'Content'/(p.removeprefix('/Game/')+'.umap')
def evidence(kind,data):
    OUT.mkdir(parents=True,exist_ok=True)
    path=OUT/(kind+'_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'.json')
    path.write_text(json.dumps(data,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')
    unreal.log('CLOSED_BODY_TRIAL '+kind+' '+str(path))

def inputs():
    global OUT
    c=read(CONFIG);body=read(AUTHOR/'BodyTrial.json')
    OUT=ROOT/c.get('evidence_folder','Saved/Profiling/Graphics/Phase01/U5a_1_V001')
    assert sha(AUTHOR/'BodyTrial.json')==c['input_config_sha256'],'Frozen input config changed'
    assert sha(ROOT/body['accepted_report'])==body['accepted_report_sha256'],'Accepted export changed'
    assert sha(asset_file(body['source_level']))==body['source_level_sha256'],'Accepted source map changed'
    for row in body['authoring_files']+body['previous_authoring_files']+c['preservation_files']:
        assert sha(ROOT/row['path'])==row['sha256'],row['path']
    assert sha(ROOT/c['build_report'])==c['build_report_sha256']
    assert sha(ROOT/c['geometry_check'])==c['geometry_check_sha256']
    for row in c['outputs']:assert sha(ROOT/row['file'])==row['sha256'],row['file']
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(),'Stop PIE first'
    assert layout.world_package()==c['target_level'],'Open '+c['target_level']
    return c,body

def import_trial():
    c,body=inputs()
    assert not layout.test.dirty(),layout.test.dirty()
    top,_=layout.level_actors();labels={a.get_actor_label() for a in top}
    assert not any(r['actor_label'] in labels for r in c['outputs']),'New trial only; do not rerun import'
    for row in c['outputs']:
        assert not unreal.EditorAssetLibrary.does_asset_exist(row['asset']),'Use a new version'
    material=unreal.load_asset(c['material'])
    assert material,c['material']
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    imported=[]
    for row in c['outputs']:
        task=unreal.AssetImportTask()
        task.filename=str(ROOT/row['file']);task.destination_path=c['asset_folder']
        task.destination_name=row['name'];task.automated=True;task.save=False;task.factory=unreal.FbxFactory()
        options=unreal.FbxImportUI()
        options.automated_import_should_detect_type=False;options.import_mesh=True;options.import_as_skeletal=False
        options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
        options.original_import_type=unreal.FBXImportType.FBXIT_STATIC_MESH
        options.import_materials=False;options.import_textures=False;options.import_animations=False
        options.static_mesh_import_data.combine_meshes=True
        options.static_mesh_import_data.remove_degenerates=False
        task.options=options
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        mesh=unreal.load_asset(row['asset']);assert mesh,row['asset']
        assert mesh.get_num_triangles(0)==row['triangles'],row['name']
        used_material=unreal.load_asset(row.get('material',c['material']));assert used_material
        mesh.set_material(0,used_material)
        actor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(),unreal.Rotator())
        assert actor,row['name']
        actor.set_actor_label(row['actor_label']);actor.set_folder_path('EDTerrain/U5aTrial')
        actor.set_editor_property('tags',['Decoration','LNPDecoration','U5aBodyTrial'])
        comp=actor.static_mesh_component
        comp.set_static_mesh(mesh);comp.set_editor_property('mobility',unreal.ComponentMobility.STATIC)
        comp.set_collision_profile_name('NoCollision');comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        assert comp.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        # 동일 FBX 반입 규약을 쓴 메시의 UE 원시 정점으로 축/단위/범위를 확인한다.
        import AssembleRidgeOctant as assembly
        _,positions=assembly.native_mesh(mesh)
        bounds=[[f(getattr(p,axis) for p in positions) for axis in ('x','y','z')] for f in (min,max)]
        error=max(abs(a-b) for pair in zip(bounds,row['bounds_cm']) for a,b in zip(*pair))
        assert error<.05,(row['name'],error)
        imported.append({'actor':actor.get_path_name(),'label':row['actor_label'],'asset':row['asset'],
                         'triangles':mesh.get_num_triangles(0),'bounds_error_cm':error,'collision':'NoCollision',
                         'material':used_material.get_path_name()})
    allowed=[c['target_level']]+[r['asset'] for r in c['outputs']]
    dirty=layout.test.dirty()
    assert all(p in allowed for p in dirty),dirty
    for path in [r['asset'] for r in c['outputs']]:assert unreal.EditorAssetLibrary.save_asset(path,False),path
    assert unreal.EditorLoadingAndSavingUtils.save_map(layout.test.world(),c['target_level'])
    assert not layout.test.dirty(),layout.test.dirty()
    evidence('Import',{'level':c['target_level'],'imported':imported,'dirty':layout.test.dirty(),
       'source_level':body['source_level'],'status':'visual trial imported; no collision or PIE gate'})
    unreal.log('CLOSED_BODY_TRIAL_IMPORT_READY; preservation verify follows')

def verify():
    c,body=inputs()
    assert not layout.test.dirty(),layout.test.dirty()
    top,nested=layout.level_actors()
    pending=[a for a in top if isinstance(a,unreal.LevelInstance) and not a.get_loaded_level()]
    if pending:
        for a in pending:a.load_level_instance()
        evidence('LoadRequested',{'labels':[a.get_actor_label() for a in pending]})
        unreal.log_warning('CLOSED_BODY_TRIAL_LOAD_REQUESTED; run verify again after load')
        return
    by_label={a.get_actor_label():a for a in top};owners={}
    for a in top:
        if isinstance(a,unreal.LevelInstance):owners[a.get_actor_label()]=nested.get(a.get_loaded_level().get_path_name(),[])
    baseline=read(ROOT/body['z1_baseline']['layout'])
    snapshot=read(ROOT/body['z1_baseline']['snapshot']);issues=[];preserved_actors=[]
    for expected in baseline['actors']:
        actor=by_label.get(expected['label']);assert actor,expected['label']
        comp=actor.static_mesh_component;errors=layout.pose_error(actor.get_actor_transform(),layout.pose_transform(expected))
        if errors[0]>.05 or max(errors[1:])>1e-5:issues.append('Inherited actor transform: '+expected['label'])
        if layout.test.package(comp.static_mesh)!=expected['mesh']:issues.append('Inherited mesh: '+expected['label'])
        overrides={str(i):layout.test.package(mat) for i,mat in enumerate(comp.get_editor_property('override_materials')) if mat}
        if overrides!=expected['material_overrides']:issues.append('Inherited material: '+expected['label'])
        old=next(x for x in snapshot['actors'] if x['label']==expected['label'])
        if str(comp.get_collision_profile_name())!=old['profile']:issues.append('Inherited profile: '+expected['label'])
        preserved_actors.append({'label':expected['label'],'pose_error':errors})
    inherited=[]
    for row in baseline['level_instances']:
        a=by_label[row['label']]
        if layout.test.package(a.get_world_asset())!=row['cull']['level']:issues.append('Inherited Cull reference')
        if max(layout.pose_error(a.get_actor_transform(),unreal.Transform()))>1e-5:issues.append('Inherited Cull transform')
        inherited.extend(owners[row['label']])
    fingerprint,_=layout.fingerprint(inherited)
    if fingerprint!=snapshot['visual_fingerprint']:issues.append('Inherited Z1 fingerprint')
    additions=[]
    for label,expected in body['accepted_additions'].items():
        actor=by_label[label];errors=layout.pose_error(actor.get_actor_transform(),layout.pose_transform(expected['pose']))
        fp,_=layout.fingerprint(owners[label])
        if errors[0]>.05 or max(errors[1:])>1e-5:issues.append('Accepted transform: '+label)
        if fp!=expected['fingerprint'] or layout.test.package(actor.get_world_asset())!=expected['level']:
            issues.append('Accepted content: '+label)
        additions.append({'label':label,'pose_error':errors,'fingerprint':fp})
    trials=[]
    for row in c['outputs']:
        a=by_label[row['actor_label']];comp=a.static_mesh_component
        errors=layout.pose_error(a.get_actor_transform(),unreal.Transform())
        if max(errors)>1e-5 or layout.test.package(comp.static_mesh)!=row['asset']:issues.append('Trial pose or mesh: '+row['name'])
        if comp.get_collision_enabled()!=unreal.CollisionEnabled.NO_COLLISION:issues.append('Trial collision enabled')
        if comp.static_mesh.get_num_triangles(0)!=row['triangles']:issues.append('Trial triangle count mismatch')
        if layout.test.package(comp.static_mesh.get_material(0))!=row.get('material',c['material']):issues.append('Trial material mismatch')
        trials.append({'label':row['actor_label'],'mesh':row['asset'],'pose':layout.local_pose(a.get_actor_transform()),
           'world_identity_pose_error':errors,'collision':str(comp.get_collision_enabled()),'triangles':comp.static_mesh.get_num_triangles(0)})
    counts={'pods':sum(isinstance(a,unreal.LNPMassSpawnPoint) for a in top),
      'anchors':sum(isinstance(a,unreal.LNPPlacementMarker) for a in top),
      'nodes':sum(a.get_actor_label().startswith('Review_Ridge') for a in top),'level_instances':len(owners),'nested_levels':len(nested)}
    if counts!={'pods':10,'anchors':6,'nodes':27,'level_instances':36,'nested_levels':36}:issues.append('Inherited counts changed')
    geometry=read(ROOT/c['geometry_check'])
    report={'preservation_pass':not issues,'issues':issues,'level':c['target_level'],'counts':counts,
      'inherited_fingerprint':fingerprint,'actors':preserved_actors,'accepted_additions':additions,'trials':trials,
      'dirty':layout.test.dirty(),'geometry_topology_pass':geometry['topology_pass'],
      'protected_pass':geometry['protected_pass'],'protected_conflicts':[
          {'id':r['id'],'hits':len(r['hits'])} for r in geometry['overlaps'] if r['hits']],
      'status':'REVIEW_existing_ramp_overlap' if not geometry['protected_pass'] else 'pending_visual_and_manual_validation',
      'collision_or_PIE_pass':False,'accepted':False}
    evidence('Verify',report)
    unreal.log('CLOSED_BODY_TRIAL_VERIFY '+('PRESERVATION_PASS' if not issues else 'PRESERVATION_REVIEW'))

if __name__=='__main__':
    command=sys.argv[1]
    assert command in ('import','verify'),command
    if command=='import':
        import_trial()
        verify()
    else:verify()
