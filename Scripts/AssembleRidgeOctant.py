"""Ridge 전용 반입·조립. 독립 에디터 commandlet에서 실행하며 기존 맵/풀은 저장하지 않는다.

UnrealEditor-Cmd LootNPop.uproject -run=pythonscript -script=Scripts/AssembleRidgeOctant.py
출력: /Game/Maps/_WorkInProgress/Ridge_00/<VERSION>/ (LVI·메시·재질). 지각은 Meadow native 이음매 정점으로 맞춘다.
"""
import itertools
import json
import sys
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/"Scripts"))
import RidgeOctantSpec as spec

VERSION = spec.VERSION
OUT = ROOT/"Saved/SliceOctant/Ridge_00"/VERSION
BASE = "/Game/Maps/_WorkInProgress/Ridge_00/"+VERSION
LEVEL = BASE+"/LVI_Octant_Ridge_00"
manifest = json.loads((OUT/"manifest.json").read_text(encoding="utf-8"))
placement = json.loads((OUT/"placement.json").read_text(encoding="utf-8"))
helpers = {"__name__": "ridge_transform_helpers"}
exec(compile((ROOT/"Scripts/AssembleAcropolisOctant.py").read_text(encoding="utf-8"), "AssembleAcropolisOctant.py", "exec"), helpers)
# 프로토타입 지면 시작색(sRGB). 암반은 같은 계열의 어두운 색으로 지면과 구분한다.
GROUND_RGB = (0x93, 0x85, 0xA5)
ROCK_RGB = (0x6A, 0x62, 0x76)


def write(name, data):
    (OUT/name).write_text(json.dumps(data, indent=2), encoding="utf-8")


def save(asset):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


def native_mesh(asset):
    mesh = unreal.DynamicMesh()
    options = unreal.GeometryScriptCopyMeshFromAssetOptions()
    options.apply_build_settings = False
    options.request_tangents = False
    options.use_build_scale = False
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.lod_type = unreal.GeometryScriptLODType.SOURCE_MODEL
    mesh, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(asset, mesh, options, lod)
    assert outcome == unreal.GeometryScriptOutcomePins.SUCCESS
    mesh, positions, gaps = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False)
    assert not gaps
    return mesh, list(unreal.GeometryScript_List.convert_vector_list_to_array(positions))


def snap_seam(asset):
    """FBX 왕복 오차를 없애기 위해 이음매 띠 정점을 Meadow native 정점으로 정확히 맞춘다."""
    reference = native_mesh(unreal.load_asset("/Game/Maps/Meshes/SM_Octant_Meadow_00_R30000"))[1]
    mesh, positions = native_mesh(asset)

    def key(p):
        return tuple(round(v*10) for v in (p.x, p.y, p.z))
    lookup = {}
    for p in reference:
        if min(p.x, p.y, p.z) <= spec.SEAM_MARGIN+.1:
            lookup.setdefault(key(p), []).append(p)
    count, maximum = 0, 0
    for i, p in enumerate(positions):
        if min(p.x, p.y, p.z) > spec.SEAM_MARGIN+.01:
            continue
        k = key(p)
        candidates = [q for delta in itertools.product((-1, 0, 1), repeat=3)
                      for q in lookup.get(tuple(a+b for a, b in zip(k, delta)), [])]
        assert candidates, ("Missing native seam", p)
        q = min(candidates, key=lambda q: (p-q).length_squared())
        distance = (p-q).length()
        assert distance < .05, distance
        maximum = max(maximum, distance)
        positions[i] = q
        count += 1
    unreal.GeometryScript_MeshEdits.set_all_mesh_vertex_positions(mesh, unreal.GeometryScript_List.convert_array_to_vector_list(positions))
    options = unreal.GeometryScriptCopyMeshToAssetOptions()
    options.enable_recompute_normals = False
    options.enable_recompute_tangents = False
    options.use_build_scale = False
    mesh, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh, asset, options, unreal.GeometryScriptMeshWriteLOD())
    assert outcome == unreal.GeometryScriptOutcomePins.SUCCESS
    return {"vertices": count, "max_correction_cm": maximum}


def import_assets():
    report = []
    for item in manifest["exports"]:
        path = BASE+"/"+item["name"]
        assert not unreal.EditorAssetLibrary.does_asset_exist(path), "Use a new version for reimport"
        task = unreal.AssetImportTask()
        task.filename = item["file"]
        task.destination_path = BASE
        task.destination_name = item["name"]
        task.automated = True
        task.save = False
        task.factory = unreal.FbxFactory()
        options = unreal.FbxImportUI()
        options.automated_import_should_detect_type = False
        options.import_mesh = True
        options.import_as_skeletal = False
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        options.original_import_type = unreal.FBXImportType.FBXIT_STATIC_MESH
        options.import_materials = False
        options.import_textures = False
        options.import_animations = False
        options.static_mesh_import_data.combine_meshes = True
        options.static_mesh_import_data.remove_degenerates = False
        task.options = options
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        asset = unreal.load_asset(path)
        assert asset and asset.get_num_triangles(0) == item["triangles"], path
        seam = snap_seam(asset) if item["role"] == "Crust" else None
        body = asset.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        body.set_editor_property("double_sided_geometry", False)
        asset.set_editor_property("lod_for_collision", 0)
        save(asset)
        positions = native_mesh(asset)[1]
        error = max(abs(function(getattr(p, axis) for p in positions)-item["bounds_cm"][side][i])
                    for side, function in (("min", min), ("max", max)) for i, axis in enumerate(("x", "y", "z")))
        assert error < .05, (path, error)
        report.append({"path": path, "triangles": asset.get_num_triangles(0), "bounds_error_cm": error, "native_seam": seam})
    write("native-import.json", report)


def material(name, rgb):
    path = BASE+"/M_Ridge_"+name
    assert not unreal.EditorAssetLibrary.does_asset_exist(path)
    # 이미 사용 중인 단색 프로토타입 재질의 사본만 수정한다.
    result = unreal.EditorAssetLibrary.duplicate_asset("/Game/Maps/Acropolis_00/M_Acropolis_Ground", path)
    assert result
    expressions = unreal.MaterialEditingLibrary.get_material_expressions(result)
    constants = [e for e in expressions if isinstance(e, unreal.MaterialExpressionConstant3Vector)]
    assert len(constants) == 1, "Ground parent graph changed; inspect before editing"

    def linear(v):
        v /= 255
        return v/12.92 if v <= .04045 else ((v+.055)/1.055)**2.4
    constants[0].set_editor_property("constant", unreal.LinearColor(*(linear(v) for v in rgb), 1))
    unreal.MaterialEditingLibrary.recompile_material(result)
    save(result)
    return result


def assemble():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assert not unreal.EditorAssetLibrary.does_asset_exist(LEVEL)
    assert levels.new_level(LEVEL)
    ground = material("Ground", GROUND_RGB)
    rock = material("Rock", ROCK_RGB)

    def spawn(cls, label, local, folder):
        pose = helpers["transform"](local)
        actor = actors.spawn_actor_from_class(cls, unreal.Vector(**pose["location"]), unreal.Rotator(**pose["rotation"]))
        assert actor
        actor.set_actor_label(label)
        actor.set_folder_path(folder)
        return actor
    for item in manifest["exports"]:
        actor = spawn(unreal.StaticMeshActor, item["name"], [0, 0, 0], "Geometry")
        actor.set_actor_transform(unreal.Transform(), False, False)
        c = actor.static_mesh_component
        c.set_static_mesh(unreal.load_asset(BASE+"/"+item["name"]))
        support = item["role"] in ("Floor", "Crust")
        tags = ["LNP.Surface.Blocker", "LNP.Surface.Static"]
        if support:
            tags.append("LNP.Surface.Support")
        if item["role"] == "Crust":
            tags.append("LNP.Surface.CoarseSupport")
        c.set_editor_property("component_tags", tags)
        c.set_editor_property("use_default_collision", False)
        c.set_collision_profile_name("LNPStaticTerrain" if support else "LNPStaticBlocker")
        c.set_mobility(unreal.ComponentMobility.STATIC)
        c.set_material(0, ground if item["role"] == "Crust" else rock)
    for item in placement["pods"]:
        actor = spawn(unreal.LNPMassSpawnPoint, "Spawn_"+item["id"], item["local"], "Markers/Pods")
        actor.set_editor_property("target_spawn_set_id", "None")
        formations = []
        for values in item.get("enemy_formations", []):
            formation = unreal.LNPSpawnEnemyFormation()
            formation.set_editor_property("enemy_config_path", values["enemyConfigPath"])
            formation.set_editor_property("count", values["count"])
            formation.set_editor_property("offset", unreal.Vector2D(**values["offset"]))
            formation.set_editor_property("radius", values["radius"])
            formations.append(formation)
        actor.set_editor_property("enemy_formations", formations)
    anchor_class = unreal.load_class(None, "/Game/WorldDevice/BP_GrappleAnchor.BP_GrappleAnchor_C")
    for item in placement["anchors"]:
        actor = spawn(unreal.LNPPlacementMarker, "Anchor_"+item["id"], item["local"], "Markers/Grapple")
        actor.set_editor_property("element_class", anchor_class)
        actor.set_editor_property("grapple_max_distance", item["max_distance"])
        actor.set_editor_property("tags", ["LNP.Grapple.AuthoredOnly"])
    for item in placement["nodes"]:
        local = list(item["local"])
        local[2] += 200
        actor = spawn(unreal.TargetPoint, "Review_Ridge"+item["id"], local, "Markers/Review")
        actor.set_editor_property("tags", ["LNP.ReviewPoint", "Ridge"+item["id"]])
        actor.set_editor_property("is_editor_only_actor", True)
    assert levels.save_current_level()
    unreal.log("RIDGE_ASSEMBLY_READY")


if __name__ == "__main__":
    unreal.AssetRegistryHelpers.get_asset_registry().search_all_assets(True)
    import_assets()
    assemble()
