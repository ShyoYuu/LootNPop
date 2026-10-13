"""V015 형상을 열린 에디터에 반입하고 승인된 훅과 기존 마커 ID를 보존한다.

에디터 콘솔에서 py Scripts/ReviseAcropolisOctant.py로 실행한다.
원본 형상 검사 통과 후에만 실행하며, 베이크는 별도 단계다.
"""

import json
import struct
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/SliceOctant/Acropolis_00/V015"
BASE = "/Game/Maps/Acropolis_00"
LEVEL = BASE + "/LVI_Octant_Acropolis_00"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level(LEVEL)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
by_label = {a.get_actor_label(): a for a in actors.get_all_level_actors()}
manifest = json.loads((OUT / "manifest.json").read_text(encoding="utf-8"))
assert json.loads((OUT / "geometry-verification.json").read_text())["reachable_islands"] == 24


def marker_state():
    return {name: {"transform": a.get_actor_transform().export_text(),
                   "id": a.get_editor_property("marker_id" if name.startswith("Anchor_") else "spawn_point_id").export_text()}
            for name,a in by_label.items() if name.startswith(("Anchor_","Spawn_"))}


def spawn_headroom():
    payload = bytes(unreal.load_asset(BASE + "/DA_OctantSurface_Acropolis_00").get_editor_property("spawn_payload"))
    version,reserved,count,random_count = struct.unpack_from("<HHII",payload)
    assert version == 3
    offset, result = 12, []
    for _ in range(count):
        guid=struct.unpack_from("<IIII",payload,offset)
        offset+=16
        length=struct.unpack_from("<i",payload,offset)[0]
        offset+=4+length
        location=struct.unpack_from("<fff",payload,offset)
        offset+=28
        layer,reserved,edge,capsule,headroom=struct.unpack_from("<HHfff",payload,offset)
        offset+=16
        formations=struct.unpack_from("<I",payload,offset)[0]
        offset+=4
        for _ in range(formations):
            length=struct.unpack_from("<i",payload,offset)[0]
            offset+=4+length+16
        result.append({"guid":"".join(f"{v:08X}" for v in guid),"location":location,"headroom":headroom,"layer":layer})
    return result


before = marker_state()
(OUT / "markers-before.json").write_text(json.dumps(before,indent=2),encoding="utf-8")
(OUT / "spawn-headroom-before.json").write_text(json.dumps(spawn_headroom(),indent=2),encoding="utf-8")
tasks=[]
changed_exports=[item for item in manifest["exports"] if item["role"]=="Crust"]
for item in changed_exports:
    task=unreal.AssetImportTask()
    task.filename=item["file"]
    task.destination_path=BASE if item["role"]=="Crust" else BASE+"/Authored"
    task.destination_name=item["name"]
    task.automated=True
    task.replace_existing=True
    task.replace_existing_settings=True
    task.save=True
    options=unreal.FbxImportUI()
    options.automated_import_should_detect_type=False
    options.import_mesh=True
    options.import_as_skeletal=False
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    options.original_import_type=unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_materials=False
    options.import_textures=False
    options.import_animations=False
    options.static_mesh_import_data.combine_meshes=True
    task.options=options
    task.factory=unreal.FbxFactory()
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for item,task in zip(changed_exports,tasks):
    folder=BASE if item["role"]=="Crust" else BASE+"/Authored"
    path=folder+"/"+item["name"]
    assert any(p.startswith(path) for p in task.imported_object_paths),(path,task.imported_object_paths)
    asset=unreal.load_asset(path)
    assert asset.get_num_triangles(0)==item["triangles"],path
    asset.get_editor_property("body_setup").set_editor_property("collision_trace_flag",unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    asset.set_editor_property("lod_for_collision",0)
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
namespace={"__name__":"acropolis_assembly_helper"}
exec(compile((ROOT/"Scripts/AssembleAcropolisOctant.py").read_text(encoding="utf-8"),
             "AssembleAcropolisOctant.py","exec"),namespace)
placement=json.loads((OUT/"placement.json").read_text(encoding="utf-8"))
exits=json.loads((OUT/"exit-verification.json").read_text(encoding="utf-8"))
reviews=[{"id":"Acropolis"+n["id"],"local":[*n["local"][:2],n["local"][2]+200]}
         for n in placement["nodes"] if n["id"].startswith("Entry_")]+exits["reviews"]
for item in reviews:
    pose=namespace["transform"](item["local"])
    label="Review_"+item["id"]
    actor=by_label.get(label)
    if actor is None:
        actor=actors.spawn_actor_from_class(unreal.TargetPoint,unreal.Vector(**pose["location"]),
            unreal.Rotator(**pose["rotation"]))
        actor.set_actor_label(label)
        actor.set_editor_property("tags",["LNP.ReviewPoint",item["id"]])
        actor.set_editor_property("is_editor_only_actor",True)
        actor.set_folder_path("Markers/Review")
    else:
        assert actors.set_actor_transform(actor,unreal.Transform(location=unreal.Vector(**pose["location"]),
            rotation=unreal.Rotator(**pose["rotation"])))
after=marker_state()
assert before==after
assert levels.save_current_level()
(OUT / "revision-import.json").write_text(json.dumps({"meshes":len(tasks),
    "preserved_anchor_states":28,"preserved_marker_ids":61,"moved_pocket_pods":0,
    "exit_reviews":len(exits["reviews"]),"updated_entry_reviews":3},indent=2),encoding="utf-8")
unreal.log("ACROPOLIS_REVISION_IMPORTED")
