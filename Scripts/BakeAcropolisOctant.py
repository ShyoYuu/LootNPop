"""Acropolis DA를 굽고 실제 풀을 변경하지 않는 검사용 풀 사본을 만든다."""

import hashlib
import json
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/SliceOctant/Acropolis_00/V016"
LEVEL = "/Game/Maps/Acropolis_00/LVI_Octant_Acropolis_00"
SURFACE = "/Game/Maps/Acropolis_00/DA_OctantSurface_Acropolis_00"
POOL = "/Game/Maps/OctantPoolData"
VALIDATION = "/Game/Maps/_WorkInProgress/Acropolis_00/Validation"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


pool_file = ROOT / "Content/Maps/OctantPoolData.uasset"
before = digest(pool_file)
surface_file = ROOT / "Content/Maps/Acropolis_00/DA_OctantSurface_Acropolis_00.uasset"
previous_save = surface_file.stat().st_mtime_ns if surface_file.exists() else None
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not levels.load_level(LEVEL):
    raise RuntimeError("Failed to initialize source level components")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world, "LNP.SurfaceNav.BakeOctant " + LEVEL)
if not surface_file.exists() or surface_file.stat().st_mtime_ns == previous_save:
    raise RuntimeError("Bake did not save a new SurfaceData revision; inspect the log")
data = unreal.load_asset(SURFACE)
if not data:
    raise RuntimeError("Acropolis SurfaceData was not produced; inspect Bake.log")
header = data.get_editor_property("header")
if header.get_editor_property("data_version") != 7:
    raise RuntimeError("Unexpected SurfaceData version")
streams = {}
for name in ("support", "navigation", "traversal", "spawn", "conditional_patch"):
    descriptor = header.get_editor_property(name)
    payload = data.get_editor_property(name + "_payload")
    size = descriptor.get_editor_property("uncompressed_size")
    if len(payload) != size or not size:
        raise RuntimeError("Invalid payload size: " + name)
    streams[name] = {"elements": descriptor.get_editor_property("element_count"), "bytes": size}

pool = unreal.load_asset(POOL)
definitions = list(pool.get_editor_property("octant_definitions"))
seam = definitions[0].get_editor_property("seam_signature")
definition = unreal.LNPOctantDefinition()
definition.set_editor_property("level_asset", unreal.load_asset(LEVEL))
definition.set_editor_property("surface_data", data)
definition.set_editor_property("allowed_slot_rotations", 255)
definition.set_editor_property("seam_signature", seam)
mixed_path = VALIDATION + "/DA_AcropolisMixedValidationPool"
if unreal.EditorAssetLibrary.does_asset_exist(mixed_path):
    mixed = unreal.load_asset(mixed_path)
else:
    mixed = unreal.EditorAssetLibrary.duplicate_asset(POOL, mixed_path)
if not mixed:
    raise RuntimeError("Failed to create isolated validation pool")
mixed.set_editor_property("octant_definitions", [d for d in definitions
    if "LVI_Octant_Acropolis_00" not in d.export_text()] + [definition])
if not unreal.EditorAssetLibrary.save_loaded_asset(mixed):
    raise RuntimeError("Failed to save isolated validation pool")
single_path = VALIDATION + "/DA_AcropolisOnlyValidationPool"
if unreal.EditorAssetLibrary.does_asset_exist(single_path):
    single = unreal.load_asset(single_path)
else:
    single = unreal.EditorAssetLibrary.duplicate_asset(POOL, single_path)
single.set_editor_property("octant_definitions", [definition])
if not unreal.EditorAssetLibrary.save_loaded_asset(single):
    raise RuntimeError("Failed to save isolated Acropolis-only pool")
assert digest(pool_file) == before, "Production pool changed"
report = {"level_asset": LEVEL, "surface_data": SURFACE, "allowed_slot_rotations": 255,
          "seam_signature": str(seam), "streams": streams, "production_pool_sha256": before,
          "mixed_validation_pool": mixed_path, "only_validation_pool": single_path}
(OUT / "bake-summary.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("ACROPOLIS_BAKE_READY " + json.dumps(report))
