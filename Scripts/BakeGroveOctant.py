"""Grove 대표 구역의 DA와 검사용 풀만 만든다. 실제 풀은 변경하지 않는다."""
import hashlib
import json
import math
import struct
import sys
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/"Scripts"))
import GroveRootNetworkSpec as spec
OUT = ROOT / "Saved/SliceOctant/Grove_00" / spec.VERSION
BASE = "/Game/Maps/_WorkInProgress/Grove_00/" + spec.VERSION
LEVEL = BASE + "/LVI_Octant_Grove_00"
SURFACE = BASE + "/DA_OctantSurface_Grove_00"
POOL = "/Game/Maps/OctantPoolData"
pool_file = ROOT / "Content/Maps/OctantPoolData.uasset"
before = hashlib.sha256(pool_file.read_bytes()).hexdigest()
unreal.AssetRegistryHelpers.get_asset_registry().search_all_assets(True)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
surface_file = ROOT / "Content" / SURFACE.removeprefix("/Game/")
surface_file = surface_file.with_suffix(".uasset")
previous = surface_file.stat().st_mtime_ns if surface_file.exists() else None
unreal.SystemLibrary.execute_console_command(world, "LNP.SurfaceNav.BakeOctant " + LEVEL)
assert surface_file.exists() and surface_file.stat().st_mtime_ns != previous, "Bake did not save SurfaceData"
data = unreal.load_asset(SURFACE)
header = data.get_editor_property("header")
streams = {name: {"elements": header.get_editor_property(name).get_editor_property("element_count"),
                  "bytes": header.get_editor_property(name).get_editor_property("uncompressed_size")}
           for name in ("support", "navigation", "traversal", "spawn", "conditional_patch")}
helpers = {"__name__": "grove_seam_helpers"}
exec(compile((ROOT / "Scripts/InspectAcropolisBake.py").read_text(encoding="utf-8"), "InspectAcropolisBake.py", "exec"), helpers)
reference = helpers["seams"]("/Game/Maps/Meadow_00/DA_OctantSurface_Meadow_00")
current = helpers["seams"](SURFACE)
assert reference == current, "Grove seam differs from Meadow"
anchors=helpers["spawn_anchors"](SURFACE)
hub=spec.point(*spec.UNDERGROUND["xy"],spec.UNDERGROUND["floor"])
cave_anchor=min(anchors,key=lambda a:math.dist(a["location"],hub))
assert math.dist(cave_anchor["location"],hub)<10
flyer=unreal.load_asset("/Game/Enemy/DA_Enemy_PureEntity_Flyer01")
flight=flyer.get_editor_property("flight_config")
required=max(flight.get_editor_property("idle_altitude_min"),flight.get_editor_property("idle_altitude_max"))+max(flyer.get_editor_property("capsule_radius"),flyer.get_editor_property("capsule_half_height"))+flight.get_editor_property("clearance")+200
assert cave_anchor["headroom"]>=4000 and cave_anchor["headroom"]>=required
payload=bytes(data.get_editor_property("spawn_payload"))
version,reserved,authored_count,random_count=struct.unpack_from("<HHII",payload)
assert version==3
offset=12
for _ in range(authored_count):
    offset+=16
    length=struct.unpack_from("<i",payload,offset)[0];offset+=4+length+28+16
    formations=struct.unpack_from("<I",payload,offset)[0];offset+=4
    for _ in range(formations):
        length=struct.unpack_from("<i",payload,offset)[0];offset+=4+length+16
spawn_config=unreal.load_asset("/Game/SpawnData/DA_MassSpawnConfig")
spawn_radius=spawn_config.get_editor_property("enemy_spawn_radius_around_pod")
candidates=[];maximum=0
for _ in range(random_count):
    value=struct.unpack_from("<I3fhhHBB4f",payload,offset);offset+=40
    maximum=max(maximum,*(abs(v) for v in value[1:4]))
    if value[6]==cave_anchor["layer"] and value[7]&2 and value[12]>=required and math.dist(value[1:4],hub)<=spawn_radius:
        candidates.append(value[0])
assert offset==len(payload) and maximum<32767
assert candidates,"No cavern candidate meets flyer headroom"
(OUT/"cavern-drone-bake.json").write_text(json.dumps({"anchor":cave_anchor,"flyer_required_headroom_cm":required,"enemy_radius_cm":spawn_radius,"eligible_enemy_candidates":len(candidates),"candidate_indices":candidates,"max_spawn_component_cm":maximum,"limits":["Baked capacity only; actual flight and combat require PIE"]},indent=2),encoding="utf-8")
pool = unreal.load_asset(POOL)
definitions = list(pool.get_editor_property("octant_definitions"))
definition = unreal.LNPOctantDefinition()
definition.set_editor_property("level_asset", unreal.load_asset(LEVEL))
definition.set_editor_property("surface_data", data)
definition.set_editor_property("allowed_slot_rotations", 255)
definition.set_editor_property("seam_signature", definitions[0].get_editor_property("seam_signature"))
for name, items in (("Only", [definition]), ("Mixed", definitions+[definition])):
    path = BASE + "/DA_Grove" + name + "ValidationPool"
    validation = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset(POOL, path)
    assert validation
    validation.set_editor_property("octant_definitions", items)
    assert unreal.EditorAssetLibrary.save_loaded_asset(validation, only_if_is_dirty=False)
assert hashlib.sha256(pool_file.read_bytes()).hexdigest() == before
(OUT / "bake-summary.json").write_text(json.dumps({"streams": streams, "matching_seam_samples": sum(map(len, current)),
    "level_asset": LEVEL, "surface_data": SURFACE, "production_pool_sha256": before,
    "limits": ["Runtime reference bake only; capsule, camera, NPC combat and PIE pending"]}, indent=2), encoding="utf-8")
unreal.log("GROVE_BAKE_READY " + json.dumps(streams))
