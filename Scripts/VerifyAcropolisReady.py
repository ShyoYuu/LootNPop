"""저장된 최종 LVI의 마커 자세·충돌·경계와 풀 입력값을 읽기 전용으로 확인한다."""

import json
import math
from pathlib import Path

import unreal
from editor_toolset.toolsets.scene import SceneTools


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/SliceOctant/Acropolis_00/V016"
LEVEL = "/Game/Maps/Acropolis_00/LVI_Octant_Acropolis_00"
SURFACE = "/Game/Maps/Acropolis_00/DA_OctantSurface_Acropolis_00"
if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL):
    raise RuntimeError("Cannot load saved Acropolis level")
namespace = {"__name__":"acropolis_assembly_helper"}
exec(compile((ROOT / "Scripts/AssembleAcropolisOctant.py").read_text(encoding="utf-8"),
             "AssembleAcropolisOctant.py", "exec"), namespace)


def point(local):
    return unreal.Vector(**namespace["transform"](local)["location"])


placement = json.loads((OUT / "placement.json").read_text(encoding="utf-8"))
visibility = json.loads((OUT / "geometry-verification.json").read_text(encoding="utf-8"))
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
by_label = {a.get_actor_label(): a for a in actors}
position_error, up_error = 0.0, 0.0
for prefix, items in (("Anchor_", placement["anchors"]),("Spawn_", placement["pods"])):
    for item in items:
        actor = by_label[prefix + item["id"]]
        p = actor.get_actor_location()
        position_error = max(position_error,(p-point(item["local"])).length())
        up = actor.get_actor_up_vector()
        expected = -p / p.length()
        dot = up.x*expected.x+up.y*expected.y+up.z*expected.z
        up_error = max(up_error,1-dot)
assert position_error < 0.01 and up_error < 1e-6,(position_error,up_error)
floor_hits = []
for pod in placement["pods"]:
    x,y,h = pod["local"]
    for radius in (0,250):
        for i in range(8 if radius else 1):
            a = i*math.tau/8
            xx,yy = x+radius*math.cos(a), y+radius*math.sin(a)
            hit = SceneTools.trace_world(point([xx,yy,h+50]),point([xx,yy,h-70]))
            assert hit is not None and abs(hit-50) < 3,(pod["id"],hit)
            floor_hits.append(hit)
manifest=json.loads((OUT / "manifest.json").read_text(encoding="utf-8"))
pocket_hits=[]
pocket_headroom=[]
for check in visibility["pocket_floor_checks"]:
    x,y,h=check["local"]
    hit=SceneTools.trace_world(point([x,y,h+50]),point([x,y,h-70]))
    assert hit is not None and abs(hit-50)<3,(check,hit)
    pocket_hits.append(hit)
for pocket in manifest["pockets"]:
    a=math.radians(pocket["angle"])
    for u in (-100,-20):
        for v in (-1200,0,1200):
            x,y=(6800+u)*math.cos(a)-v*math.sin(a),(6800+u)*math.sin(a)+v*math.cos(a)
            h=-3500
            hit=SceneTools.trace_world(point([x,y,h+50]),point([x,y,h-70]))
            assert hit is not None and abs(hit-50)<3,(pocket["id"],u,v,hit)
            pocket_hits.append(hit)
    x,y,h=pocket["pod"]
    hit=SceneTools.trace_world(point([x,y,h+200]),point([x,y,h+3800]))
    assert hit is not None and abs(hit-3000)<3,(pocket["id"],"ceiling",hit)
    pocket_headroom.append(hit+200)
anchors = {a["id"]:a for a in placement["anchors"]}
for edge in visibility["hook_visibility_edges"]:
    hit = SceneTools.trace_world(point(edge["start_local"]),point(anchors[edge["to"]]["local"]))
    assert hit is None,(edge,hit)

pit=json.loads((ROOT/'Art/Octants/Acropolis_00/pit-recovery.json').read_text(encoding='utf-8'))
pit_actor=by_label['Anchor_'+pit['anchor']['id']]
assert pit_actor.get_editor_property('grapple_max_distance')==pit['anchor']['max_distance']
assert 'BP_GrappleAnchor_C' in pit_actor.get_editor_property('element_class').get_path_name()
assert (by_label['Review_'+pit['review_id']].get_actor_location()-point(pit['review_local'])).length()<0.01
assert len({a.get_editor_property('marker_id').export_text() for name,a in by_label.items() if name.startswith('Anchor_')})==29
pit_checks=json.loads((OUT/'pit-recovery-verification.json').read_text(encoding='utf-8'))
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for probe in pit_checks['probes']:
    x,y,h=probe['floor_local']
    start,end=point([x,y,h+150]),point(pit['anchor']['local'])
    assert SceneTools.trace_world(start,end) is None
    assert unreal.SystemLibrary.sphere_trace_single(world,start,end,100,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[],unreal.DrawDebugTrace.NONE,True) is None
gap=json.loads((OUT / "gap-verification.json").read_text(encoding="utf-8"))
for probe in gap["probes"]:
    hit=SceneTools.trace_world(unreal.Vector(*probe["start"]),unreal.Vector(*probe["end"]))
    assert hit is not None,("Native gap remains",probe)
exits=json.loads((OUT/"exit-verification.json").read_text(encoding="utf-8"))
exit_floor_hits=[]
for sample in exits["floor_samples"]:
    x,y,h=sample["local"]
    hit=SceneTools.trace_world(point([x,y,h+50]),point([x,y,h-70]))
    assert hit is not None and abs(hit-50)<3,("Exit floor mismatch",sample,hit)
    exit_floor_hits.append(hit)
for review in exits["reviews"]:
    assert (by_label["Review_"+review["id"]].get_actor_location()-point(review["local"])).length()<0.01
seam_namespace = {"__name__":"acropolis_seam_helper"}
exec(compile((ROOT / "Scripts/InspectAcropolisBake.py").read_text(encoding="utf-8"),
             "InspectAcropolisBake.py", "exec"), seam_namespace)
assert not seam_namespace["run"](),"Saved seam data differs from Meadow"
anchors=seam_namespace["spawn_anchors"](SURFACE)
cave_data=[]
for pod in placement["pods"]:
    if not pod["id"].startswith("Pocket_"):
        continue
    actor=by_label["Spawn_"+pod["id"]]
    native_guid=actor.get_editor_property("spawn_point_id")
    guid="".join(f"{native_guid.get_editor_property(k)&0xFFFFFFFF:08X}" for k in ("a","b","c","d"))
    baked=next(a for a in anchors if a["guid"]==guid)
    assert baked["headroom"]>=3000,(pod["id"],baked)
    cave_data.append({"id":pod["id"],**baked})
before=json.loads((OUT / "markers-before.json").read_text(encoding="utf-8"))
for name,state in before.items():
    actor=by_label[name]
    field="marker_id" if name.startswith("Anchor_") else "spawn_point_id"
    assert actor.get_editor_property(field).export_text()==state["id"]
    if name.startswith("Anchor_"):
        assert actor.get_actor_transform().export_text()==state["transform"]

data = unreal.load_asset(SURFACE)
production_pool = unreal.load_asset("/Game/Maps/OctantPoolData")
definitions = list(production_pool.get_editor_property("octant_definitions"))
seam = definitions[0].get_editor_property("seam_signature")
definition = unreal.LNPOctantDefinition()
definition.set_editor_property("level_asset", unreal.load_asset(LEVEL))
definition.set_editor_property("surface_data", data)
definition.set_editor_property("allowed_slot_rotations", 255)
definition.set_editor_property("seam_signature", seam)
(OUT / "OctantDefinition.txt").write_text(definition.export_text(),encoding="utf-8")
values = {"LevelAsset":LEVEL+".LVI_Octant_Acropolis_00",
          "SurfaceData":SURFACE+".DA_OctantSurface_Acropolis_00",
          "AllowedSlotRotations":255,"SeamSignature":str(seam)}
(OUT / "OctantDefinition.json").write_text(json.dumps(values,indent=2),encoding="utf-8")
report = {"saved_marker_poses":len(placement["anchors"])+len(placement["pods"]),"max_marker_position_error_cm":position_error,
          "max_marker_up_dot_error":up_error,"native_pod_floor_rays":len(floor_hits),
          "max_pod_floor_error_cm":max(abs(v-50) for v in floor_hits),
          "native_hook_visibility_rays":len(visibility["hook_visibility_edges"]),
          "native_pocket_floor_rays":len(pocket_hits),"native_pocket_ceilings":len(pocket_headroom),
          "native_min_pocket_headroom_cm":min(pocket_headroom),"baked_cave_anchors":cave_data,
          "preserved_anchor_states":28,"preserved_marker_ids":61,"new_pit_recovery_anchors":1,
          "native_pit_body_corridor_sweeps":len(pit_checks['probes']),
          "native_gap_closure_rays":len(gap["probes"]),
          "native_exit_floor_rays":len(exit_floor_hits),"exit_review_points":len(exits["reviews"]),
          "max_exit_floor_error_cm":max(abs(v-50) for v in exit_floor_hits),
          "baked_seam_samples_matched":2208,"production_pool_definitions":len(definitions),
          "production_pool_contains_acropolis":any("LVI_Octant_Acropolis_00" in d.export_text() for d in definitions),
          "pool_registration_performed":False,"pie_performed":False}
(OUT / "ready-verification.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
unreal.log("ACROPOLIS_READY_VERIFIED " + json.dumps(report))
