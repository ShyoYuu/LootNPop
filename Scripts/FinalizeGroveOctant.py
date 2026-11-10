"""승인된 V042를 정식 경로에 복사·베이크하고 라이브 풀 등록 입력을 준비한다."""
import hashlib
import gc
import json
import shutil
import struct
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/SliceOctant/Grove_00/Production'
BASE='/Game/Maps/Grove_00'
SOURCE='/Game/Maps/_WorkInProgress/Grove_00/'
LEVEL=BASE+'/LVI_Octant_Grove_00'
SURFACE=BASE+'/DA_OctantSurface_Grove_00'
POOL='/Game/Maps/OctantPoolData'
MAPPING={SOURCE+'V040/'+name:BASE+'/'+name for name in ('M_Grove_Ground','M_Grove_Bark','SM_Octant_Grove_00_V040_R30000')}
MAPPING.update({SOURCE+'V040/'+name:BASE+'/Authored/'+name for name in ('SM_Grove_00_V040_Forest_Floor','SM_Grove_00_V040_Forest_Shell')})
MAPPING[SOURCE+'V042/LVI_Octant_Grove_00']=LEVEL

def file(path):return ROOT/'Content'/Path(path.removeprefix('/Game/')).with_suffix('.umap' if '/LVI_' in path else '.uasset')
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def write(name,value):(OUT/name).write_text(json.dumps(value,indent=2),encoding='utf-8')
def save(asset):assert unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False),asset
def canon(value):
    if isinstance(value,unreal.Object):return canon(value.get_path_name())
    if isinstance(value,(list,tuple,unreal.Array)):return [canon(v) for v in value]
    text=value.export_text() if hasattr(value,'export_text') else str(value)
    for old,new in MAPPING.items():text=text.replace(old,new)
    return text
def props(obj,names):return {n:canon(obj.get_editor_property(n)) for n in names}
def snapshot():
    result={};counts={'geometry':0,'lights':0,'anchors':0,'pods':0,'reviews':0}
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        category=next((k for cls,k in ((unreal.StaticMeshActor,'geometry'),(unreal.PointLight,'lights'),(unreal.LNPPlacementMarker,'anchors'),(unreal.LNPMassSpawnPoint,'pods'),(unreal.TargetPoint,'reviews')) if isinstance(actor,cls)),None)
        if category is None:continue
        counts[category]+=1
        item={'class':actor.get_class().get_path_name(),'transform':canon(actor.get_actor_transform()),'tags':canon(actor.tags)}
        if category=='geometry':
            c=actor.static_mesh_component
            item['component']=props(c,['static_mesh','override_materials','mobility','component_tags','body_instance'])
        elif category=='lights':item['light']=props(actor.point_light_component,['mobility','intensity','attenuation_radius','intensity_units','light_color','cast_shadows','source_radius','use_inverse_squared_falloff'])
        elif category=='anchors':item['anchor']=props(actor,['marker_id','element_class','grapple_max_distance'])
        elif category=='pods':item['pod']=props(actor,['spawn_point_id'])
        label=actor.get_actor_label();assert label not in result,label
        result[label]=item
    return result,counts

def run():
    OUT.mkdir(parents=True,exist_ok=True)
    assert not (OUT/'integration.json').exists(),'Integration already completed; use saved audit'
    if any(unreal.EditorAssetLibrary.does_asset_exist(p) for p in MAPPING.values()):
        assert (OUT/'before.json').exists(),'Unowned production assets'
    pool_file=file(POOL);pool_before=digest(pool_file)
    config_before=digest(ROOT/'Config/DefaultGame.ini')
    backup=OUT/'Before';backup.mkdir(exist_ok=True)
    shutil.copy2(pool_file,backup/'OctantPoolData.uasset')
    before_hashes={}
    for old in MAPPING:
        source_file=file(old);before_hashes[old]=digest(source_file)
        shutil.copy2(source_file,backup/source_file.name)
    write('before.json',{'production_pool_sha256':pool_before,'config_sha256':config_before,'source_sha256':before_hashes})
    registry=unreal.AssetRegistryHelpers.get_asset_registry();registry.search_all_assets(True)
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert levels.load_level(SOURCE+'V042/LVI_Octant_Grove_00')
    original,counts=snapshot()
    assert counts['geometry']==3 and counts['lights']==47 and counts['anchors']==30 and counts['pods']==8,counts
    for old,new in MAPPING.items():
        copied=unreal.load_asset(new) if unreal.EditorAssetLibrary.does_asset_exist(new) else unreal.EditorAssetLibrary.duplicate_asset(old,new)
        assert copied,(old,new)
        if isinstance(copied,unreal.StaticMesh):
            source=unreal.load_asset(old)
            assert copied.get_num_triangles(0)==source.get_num_triangles(0)
            for index in range(copied.get_num_sections(0)):
                material=copied.get_material(index)
                if material and material.get_path_name().split('.')[0] in MAPPING:
                    copied.set_material(index,unreal.load_asset(MAPPING[material.get_path_name().split('.')[0]]))
        save(copied)
    copied=None
    gc.collect()
    unreal.SystemLibrary.collect_garbage()
    assert levels.load_level(LEVEL)
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if not isinstance(actor,unreal.StaticMeshActor):continue
        c=actor.static_mesh_component
        old=c.static_mesh.get_path_name().split('.')[0]
        assert old in MAPPING or old in MAPPING.values(),old
        if old in MAPPING:c.set_static_mesh(unreal.load_asset(MAPPING[old]))
        for index in range(c.get_num_materials()):
            material=c.get_material(index)
            if material and material.get_path_name().split('.')[0] in MAPPING:
                c.set_material(index,unreal.load_asset(MAPPING[material.get_path_name().split('.')[0]]))
    assert unreal.EditorAssetLibrary.save_asset(LEVEL,only_if_is_dirty=False)
    actual,after_counts=snapshot()
    write('actor-differences.json',{label:{'before':original.get(label),'after':actual.get(label)} for label in set(original)|set(actual) if original.get(label)!=actual.get(label)})
    assert counts==after_counts and original==actual,'Approved actor state changed'
    write('actor-preservation.json',{'counts':counts,'actors_compared':len(actual),'all_equal':True})
    registry.scan_paths_synchronous([BASE],True)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    # 재개 과정에서 같은 내용의 원본 패키지를 재저장해도 source hash는 다시 맞춘다.
    unreal.SystemLibrary.execute_console_command(world,'LNP.SurfaceNav.BakeOctant '+LEVEL)
    assert file(SURFACE).exists(),'Production bake did not save SurfaceData'
    data=unreal.load_asset(SURFACE);header=data.get_editor_property('header')
    helpers={'__name__':'grove_production_seam_helpers'}
    exec(compile((ROOT/'Scripts/InspectAcropolisBake.py').read_text(encoding='utf-8'),'InspectAcropolisBake.py','exec'),helpers)
    seams=helpers['seams'](SURFACE)
    assert seams==helpers['seams']('/Game/Maps/Meadow_00/DA_OctantSurface_Meadow_00')
    streams={n:{'elements':header.get_editor_property(n).get_editor_property('element_count'),'bytes':header.get_editor_property(n).get_editor_property('uncompressed_size')} for n in ('support','navigation','traversal','spawn','conditional_patch')}
    assert struct.unpack_from('<H',bytes(data.get_editor_property('support_payload')),2)[0]<=256
    write('bake-summary.json',{'streams':streams,'matching_seam_samples':sum(map(len,seams)),'level_asset':LEVEL,'surface_data':SURFACE})
    registry.scan_paths_synchronous([BASE],True)
    options=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True)
    pending=[LEVEL,SURFACE];visited=set()
    while pending:
        package=pending.pop()
        if package in visited:continue
        visited.add(package)
        for dependency in registry.get_dependencies(package,options):
            name=str(dependency)
            assert not name.startswith('/Game/Maps/_WorkInProgress/'),(package,name)
            if name.startswith('/Game/'):pending.append(name)
    assert all(digest(file(path))==value for path,value in before_hashes.items()),'Approved source files changed'
    assert digest(pool_file)==pool_before,'Pool changed concurrently; re-read before registering'
    pool=unreal.load_asset(POOL);definitions=list(pool.get_editor_property('octant_definitions'))
    old_definitions=[d.export_text() for d in definitions]
    assert all('Grove_00' not in value for value in old_definitions),'Grove already registered'
    definition=unreal.LNPOctantDefinition()
    definition.set_editor_property('level_asset',unreal.load_asset(LEVEL))
    definition.set_editor_property('surface_data',data)
    definition.set_editor_property('allowed_slot_rotations',255)
    definition.set_editor_property('seam_signature',definitions[0].get_editor_property('seam_signature'))
    assert digest(ROOT/'Config/DefaultGame.ini')==config_before
    art=ROOT/'Art/Octants/Grove_00'
    (art/'OctantDefinition.txt').write_text(definition.export_text(),encoding='utf-8')
    report={'source_version':'V042','geometry_version':'V040','counts':counts,'assets':list(MAPPING.values())+[SURFACE],
            'asset_mapping':MAPPING,'checked_dependency_packages':len(visited),'work_dependencies':[],
            'previous_definitions':old_definitions,'new_definition':definition.export_text(),'definition_count':len(definitions)+1,
            'production_pool_before_sha256':pool_before,'pool_registered':False,'default_game_ini_unchanged':True}
    write('integration-ready.json',report)
    unreal.log('GROVE_PRODUCTION_READY '+json.dumps({'definitions_after_registration':len(definitions)+1,'assets':7,'actors_preserved':len(actual),'lights':47,'anchors':30}))

run()
