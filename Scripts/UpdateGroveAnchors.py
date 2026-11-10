"""확정된 Grove LVI의 사본에서 앵커만 통합·이동한다. 지형/조명/Pod는 유지한다."""
import json
import sys
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
import GroveRootNetworkSpec as spec
OUT=ROOT/'Saved/SliceOctant/Grove_00'/spec.VERSION
SOURCE_VERSION=getattr(spec,'ANCHOR_BASE_VERSION',spec.GEOMETRY_VERSION)
SOURCE='/Game/Maps/_WorkInProgress/Grove_00/'+SOURCE_VERSION+'/LVI_Octant_Grove_00'
TARGET='/Game/Maps/_WorkInProgress/Grove_00/'+spec.VERSION+'/LVI_Octant_Grove_00'
placement=json.loads((OUT/'placement.json').read_text(encoding='utf-8'))
helpers={'__name__':'grove_transform_helpers'}
exec(compile((ROOT/'Scripts/AssembleAcropolisOctant.py').read_text(encoding='utf-8'),'AssembleAcropolisOctant.py','exec'),helpers)

unreal.AssetRegistryHelpers.get_asset_registry().search_all_assets(True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not unreal.EditorAssetLibrary.does_asset_exist(TARGET),'Use a new anchor version'
assert unreal.EditorAssetLibrary.duplicate_asset(SOURCE,TARGET)
assert levels.load_level(TARGET)
all_actors=actors.get_all_level_actors()
markers={a.get_actor_label().removeprefix('Anchor_'):a for a in all_actors if isinstance(a,unreal.LNPPlacementMarker)}
previous=json.loads((OUT.parent/SOURCE_VERSION/'placement.json').read_text(encoding='utf-8'))
assert len(markers)==len(previous['anchors'])
def snapshot(actor):
    return {'transform':str(actor.get_actor_transform()),'guid':str(actor.get_editor_property('actor_guid'))}
protected={a.get_actor_label():snapshot(a) for a in all_actors if not isinstance(a,unreal.LNPPlacementMarker)}
geometry={a.get_actor_label():a.static_mesh_component.static_mesh.get_path_name() for a in all_actors if isinstance(a,unreal.StaticMeshActor)}
rows=[]
retained=set()
for item in placement['anchors']:
    actor=markers[item['id']];retained.add(item['id'])
    old=snapshot(actor);marker_id=str(actor.get_editor_property('marker_id'))
    pose=helpers['transform'](item['local'])
    position=unreal.Vector(**pose['location'])
    actor.set_actor_location_and_rotation(position,unreal.Rotator(**pose['rotation']),False,False)
    assert (actor.get_actor_location()-position).length()<.01
    actor.set_editor_property('grapple_max_distance',item['max_distance'])
    assert str(actor.get_editor_property('marker_id'))==marker_id
    rows.append({'id':item['id'],'marker_id':marker_id,'before':old,'after':snapshot(actor),'merged_ids':item['merged_ids']})
removed=[]
for name,actor in markers.items():
    if name in retained:continue
    assert actors.destroy_actor(actor);removed.append(name)
remaining=actors.get_all_level_actors()
assert len([a for a in remaining if isinstance(a,unreal.LNPPlacementMarker)])==len(placement['anchors'])
assert protected=={a.get_actor_label():snapshot(a) for a in remaining if not isinstance(a,unreal.LNPPlacementMarker)}
assert geometry=={a.get_actor_label():a.static_mesh_component.static_mesh.get_path_name() for a in remaining if isinstance(a,unreal.StaticMeshActor)}
assert levels.save_current_level()
(OUT/'anchor-application.json').write_text(json.dumps({'source':SOURCE,'target':TARGET,'retained':rows,'removed':removed,'protected_actors':len(protected),'geometry_refs':geometry},indent=2),encoding='utf-8')
unreal.log('GROVE_ANCHORS_UPDATED')
