"""FBX 지각의 보존 띠를 Meadow native 정점 좌표로 복원한다.

기존 맵은 읽기만 한다. 작업용 반입본에서 Acropolis 지각만 갱신한다.
베이크 전 반드시 실행하며, 외부 프로세스에서 실행한 뒤 열린 에디터는 에셋을 재로드한다.
"""

import itertools
import json
import shutil
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/SliceOctant/Acropolis_00/V015"
TARGET = "/Game/Maps/Acropolis_00/SM_Octant_Acropolis_00_R30000"
RAW = "/Game/Maps/_WorkInProgress/Acropolis_00/V015/SM_AcropolisCrustRaw"


def extract(asset):
    mesh = unreal.DynamicMesh()
    options = unreal.GeometryScriptCopyMeshFromAssetOptions()
    options.apply_build_settings = False
    options.request_tangents = False
    options.use_build_scale = False
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.lod_type = unreal.GeometryScriptLODType.SOURCE_MODEL
    mesh, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(asset, mesh, options, lod)
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError("Cannot extract native mesh: " + asset.get_path_name())
    mesh, positions, gaps = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False)
    if gaps:
        raise RuntimeError("Unexpected vertex ID gaps")
    return mesh, list(unreal.GeometryScript_List.convert_vector_list_to_array(positions))


backup = OUT / "BeforeNativeSeamFix.uasset"
if not backup.exists():
    shutil.copy2(ROOT / "Content/Maps/Acropolis_00/SM_Octant_Acropolis_00_R30000.uasset", backup)
if not unreal.EditorAssetLibrary.does_asset_exist(RAW):
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
    task = unreal.AssetImportTask()
    task.filename = str(OUT / "SM_Octant_Acropolis_00_R30000.fbx")
    task.destination_path, task.destination_name = RAW.rsplit("/", 1)
    task.automated = True
    task.save = True
    task.factory = unreal.FbxFactory()
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
raw = unreal.load_asset(RAW)
target = unreal.load_asset(TARGET)
source = unreal.load_asset("/Game/Maps/Meshes/SM_Octant_Meadow_00_R30000")
source_mesh, reference = extract(source)
mesh, positions = extract(raw)

def key(p):
    return tuple(round(v*10) for v in (p.x,p.y,p.z))

lookup = {}
for p in reference:
    if min(p.x,p.y,p.z) <= 1500.1:
        lookup.setdefault(key(p), []).append(p)
count, maximum = 0, 0.0
for i,p in enumerate(positions):
    if min(p.x,p.y,p.z) > 1500.01:
        continue
    k = key(p)
    candidates = lookup.get(k, [])
    if not candidates:
        candidates = [q for d in itertools.product((-1,0,1), repeat=3)
                      for q in lookup.get(tuple(a+b for a,b in zip(k,d)), [])]
    if not candidates:
        raise RuntimeError("Seam vertex does not match a native source vertex: " + str(p))
    q = min(candidates, key=lambda q: (p-q).length_squared())
    distance = (p-q).length()
    if distance > 0.05:
        raise RuntimeError("Seam snap exceeds 0.05cm: " + str(distance))
    maximum = max(maximum,distance)
    positions[i] = q
    count += 1
new_positions = unreal.GeometryScript_List.convert_array_to_vector_list(positions)
unreal.GeometryScript_MeshEdits.set_all_mesh_vertex_positions(mesh, new_positions)
options = unreal.GeometryScriptCopyMeshToAssetOptions()
options.enable_recompute_normals = False
options.enable_recompute_tangents = False
options.use_build_scale = False
mesh, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(
    mesh,target,options,unreal.GeometryScriptMeshWriteLOD())
if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
    raise RuntimeError("Failed to publish native seam geometry")
body = target.get_editor_property("body_setup")
body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
target.set_editor_property("lod_for_collision", 0)
if not unreal.EditorAssetLibrary.save_loaded_asset(target, only_if_is_dirty=False):
    raise RuntimeError("Failed to save Acropolis crust")
report = {"snapped_vertices":count,"max_correction_cm":maximum,
          "target":TARGET,"native_seam_source":source.get_path_name(),
          "triangles":target.get_num_triangles(0)}
(OUT / "native-seam-fix.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
unreal.log("ACROPOLIS_NATIVE_SEAM_FIXED " + json.dumps(report))
