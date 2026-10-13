"""승인된 V015 지형에 구덩이 탈출 훅 하나와 재검토 지점을 추가한다."""

import hashlib
import json
from pathlib import Path

import unreal
from editor_toolset.toolsets.scene import SceneTools


ROOT=Path(unreal.Paths.project_dir()).resolve()
ART=ROOT/'Art/Octants/Acropolis_00'
OUT=ROOT/'Saved/SliceOctant/Acropolis_00/V016'
OUT.mkdir(parents=True,exist_ok=True)
LEVEL='/Game/Maps/Acropolis_00/LVI_Octant_Acropolis_00'
spec=json.loads((ART/'pit-recovery.json').read_text(encoding='utf-8'))
namespace={'__name__':'acropolis_assembly_helper'}
exec((ROOT/'Scripts/AssembleAcropolisOctant.py').read_text(encoding='utf-8'),namespace)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level(LEVEL)
assert levels.save_current_level()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
by_label={a.get_actor_label():a for a in actors.get_all_level_actors()}


def point(local):
    return unreal.Vector(**namespace['transform'](local)['location'])


def marker_state():
    return {name:{'transform':a.get_actor_transform().export_text(),
                  'id':a.get_editor_property('marker_id' if name.startswith('Anchor_') else 'spawn_point_id').export_text()}
            for name,a in by_label.items() if name.startswith(('Anchor_','Spawn_')) and name!='Anchor_'+spec['anchor']['id']}


before=marker_state()
assert len(before)==61
(OUT/'markers-before.json').write_text(json.dumps(before,indent=2),encoding='utf-8')
mesh_hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest()
             for p in (ROOT/'Content/Maps/Acropolis_00').rglob('SM_*.uasset')}
anchor_spec=spec['anchor']
anchor=point(anchor_spec['local'])
floor_samples=[]
for y in range(-26000,-23499,250):
    for x in range(-14500,-10999,250):
        hit=SceneTools.trace_world(point([x,y,1500]),point([x,y,-5000]))
        assert hit is not None,('Missing pit floor',x,y)
        if 1500-hit<-2000:
            floor_samples.append([x,y,1500-hit])
probes=[]
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for x,y,h in floor_samples:
    start=point([x,y,h+150])
    assert SceneTools.trace_world(start,anchor) is None,(x,y,'Blocked sightline')
    assert unreal.SystemLibrary.sphere_trace_single(world,start,anchor,100,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[],unreal.DrawDebugTrace.NONE,True) is None,(x,y,'Blocked body corridor')
    distance=(anchor-start).length()
    assert distance<anchor_spec['max_distance']
    probes.append({'floor_local':[x,y,h],'distance_cm':distance})
assert len(probes)==45
landing=[]
for dx in (-500,0,500):
    for dy in (-500,0,500):
        x,y=anchor_spec['local'][0]+dx,anchor_spec['local'][1]+dy
        hit=SceneTools.trace_world(point([x,y,200]),point([x,y,-200]))
        assert hit is not None and abs(hit-200)<5,('Landing floor',x,y,hit)
        landing.append([x,y,200-hit])

pose=namespace['transform'](anchor_spec['local'])
label='Anchor_'+anchor_spec['id']
actor=by_label.get(label)
if actor is None:
    actor=actors.spawn_actor_from_class(unreal.LNPPlacementMarker,unreal.Vector(**pose['location']),unreal.Rotator(**pose['rotation']))
    actor.set_actor_label(label)
else:
    assert actors.set_actor_transform(actor,unreal.Transform(location=unreal.Vector(**pose['location']),rotation=unreal.Rotator(**pose['rotation'])))
actor.set_editor_property('element_class',unreal.load_class(None,'/Game/WorldDevice/BP_GrappleAnchor.BP_GrappleAnchor_C'))
actor.set_editor_property('grapple_max_distance',anchor_spec['max_distance'])
actor.set_editor_property('tags',['LNP.Grapple.AuthoredOnly'])
actor.set_folder_path('Markers/Grapple')
pose=namespace['transform'](spec['review_local'])
label='Review_'+spec['review_id']
review=by_label.get(label)
if review is None:
    review=actors.spawn_actor_from_class(unreal.TargetPoint,unreal.Vector(**pose['location']),unreal.Rotator(**pose['rotation']))
    review.set_actor_label(label)
review.set_editor_property('tags',['LNP.ReviewPoint',spec['review_id']])
review.set_editor_property('is_editor_only_actor',True)
review.set_folder_path('Markers/Review')
assert marker_state()==before
assert levels.save_current_level()
placement=json.loads((ART/'placement.json').read_text(encoding='utf-8'))
placement['anchors']=[a for a in placement['anchors'] if a['id']!=anchor_spec['id']]+[anchor_spec]
assert len(placement['anchors'])==29
for folder in (ART,OUT):
    (folder/'placement.json').write_text(json.dumps(placement,indent=2),encoding='utf-8')
assert mesh_hashes=={p.name:hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in (ROOT/'Content/Maps/Acropolis_00').rglob('SM_*.uasset')}
report={'revision':'V016','geometry_revision':'V015','anchor':anchor_spec,
        'marker_id':actor.get_editor_property('marker_id').export_text(),
        'preserved_marker_ids':61,'preserved_mesh_assets':len(mesh_hashes),
        'visibility_rays':len(probes),'sphere_radius_cm':100,'sphere_start_above_floor_cm':150,
        'body_corridor_sweeps':len(probes),'max_distance_cm':max(p['distance_cm'] for p in probes),
        'landing_floor_samples':landing,'probes':probes,
        'limits':['Authored collision corridor and landing floor; actual Mover flight requires PIE review']}
(OUT/'pit-recovery-verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('ACROPOLIS_PIT_RECOVERY_SAVED')
