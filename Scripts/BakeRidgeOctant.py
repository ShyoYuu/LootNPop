"""Ridge 작업본의 DA를 기존 베이커로 굽고 Meadow seam 일치·Pod 앵커 머리 공간을 확인한 뒤 검사용 풀만 만든다.

실제 풀(`/Game/Maps/OctantPoolData`)은 변경하지 않으며 전후 SHA256을 비교한다.
"""
import hashlib
import json
import struct
import math
import sys
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/"Scripts"))
import RidgeOctantSpec as spec
OUT = ROOT/"Saved/SliceOctant/Ridge_00"/spec.VERSION
BASE = "/Game/Maps/_WorkInProgress/Ridge_00/"+spec.VERSION
LEVEL = BASE+"/LVI_Octant_Ridge_00"
SURFACE = BASE+"/DA_OctantSurface_Ridge_00"
POOL = "/Game/Maps/OctantPoolData"
pool_file = ROOT/"Content/Maps/OctantPoolData.uasset"
before = hashlib.sha256(pool_file.read_bytes()).hexdigest()
unreal.AssetRegistryHelpers.get_asset_registry().search_all_assets(True)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
surface_file = (ROOT/"Content"/SURFACE.removeprefix("/Game/")).with_suffix(".uasset")
previous = surface_file.stat().st_mtime_ns if surface_file.exists() else None
unreal.SystemLibrary.execute_console_command(world, "LNP.SurfaceNav.BakeOctant "+LEVEL)
assert surface_file.exists() and surface_file.stat().st_mtime_ns != previous, "Bake did not save SurfaceData"
data = unreal.load_asset(SURFACE)
header = data.get_editor_property("header")
streams = {name: {"elements": header.get_editor_property(name).get_editor_property("element_count"),
                  "bytes": header.get_editor_property(name).get_editor_property("uncompressed_size")}
           for name in ("support", "navigation", "traversal", "spawn", "conditional_patch")}
helpers = {"__name__": "ridge_seam_helpers"}
exec(compile((ROOT/"Scripts/InspectAcropolisBake.py").read_text(encoding="utf-8"), "InspectAcropolisBake.py", "exec"), helpers)
reference = helpers["seams"]("/Game/Maps/Meadow_00/DA_OctantSurface_Meadow_00")
current = helpers["seams"](SURFACE)
assert reference == current, "Ridge seam differs from Meadow"


def spawn_anchors(path):
    """authored 앵커 목록. 편성 레코드(이름 + count·offset·radius 24B)를 건너뛴다(Acropolis 헬퍼는 편성 없는 옛 형식)."""
    payload = bytes(unreal.load_asset(path).get_editor_property("spawn_payload"))
    version, _, count, _ = struct.unpack_from("<HHII", payload)
    assert version == 3
    offset, result = 12, []
    for _ in range(count):
        offset += 16
        length = struct.unpack_from("<i", payload, offset)[0]
        offset += 4+length
        location = struct.unpack_from("<fff", payload, offset)
        offset += 28
        layer, _, _, _, headroom = struct.unpack_from("<HHfff", payload, offset)
        offset += 16
        formations = struct.unpack_from("<I", payload, offset)[0]
        offset += 4
        for _ in range(formations):
            length = struct.unpack_from("<i", payload, offset)[0]
            offset += 4+length+24
        result.append({"location": location, "headroom": headroom, "layer": layer, "formations": formations})
    return result


anchors = spawn_anchors(SURFACE)
assert sorted(a["formations"] for a in anchors) == sorted(len(p["enemy_formations"]) for p in json.loads((OUT/"placement.json").read_text(encoding="utf-8"))["pods"]), "Pod formations missing from bake"
pods = json.loads((OUT/"placement.json").read_text(encoding="utf-8"))["pods"]
flyer = unreal.load_asset("/Game/Enemy/DA_Enemy_PureEntity_Flyer01")
flight = flyer.get_editor_property("flight_config")
required = max(flight.get_editor_property("idle_altitude_min"), flight.get_editor_property("idle_altitude_max"))+max(flyer.get_editor_property("capsule_radius"), flyer.get_editor_property("capsule_half_height"))+flight.get_editor_property("clearance")+200
pod_report = []
for pod in pods:
    target = spec.point(*pod["local"])
    nearest = min(anchors, key=lambda a: math.dist(a["location"], target))
    # 베이크 앵커는 실제 지각(4m 삼각형)에 붙으므로 해석 높이와 수직으로만 조금 다를 수 있다.
    lx, ly, lh = spec.local(nearest["location"])
    horizontal = math.hypot(lx-pod["local"][0], ly-pod["local"][1])
    assert horizontal < 10 and abs(lh-pod["local"][2]) < 100, (pod["id"], horizontal, lh-pod["local"][2])
    # 비행형이 없는 지하 동굴 편성은 낮은 천장을 허용한다.
    assert nearest["headroom"] >= required or not pod["flyers"], (pod["id"], nearest["headroom"], required)
    pod_report.append({"id": pod["id"], "headroom_cm": nearest["headroom"], "layer": nearest["layer"],
                       "horizontal_cm": horizontal, "height_delta_cm": lh-pod["local"][2]})
pool = unreal.load_asset(POOL)
definitions = list(pool.get_editor_property("octant_definitions"))
definition = unreal.LNPOctantDefinition()
definition.set_editor_property("level_asset", unreal.load_asset(LEVEL))
definition.set_editor_property("surface_data", data)
definition.set_editor_property("allowed_slot_rotations", 255)
definition.set_editor_property("seam_signature", definitions[0].get_editor_property("seam_signature"))
for name, items in (("Only", [definition]), ("Mixed", definitions+[definition])):
    path = BASE+"/DA_Ridge"+name+"ValidationPool"
    validation = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset(POOL, path)
    assert validation
    validation.set_editor_property("octant_definitions", items)
    assert unreal.EditorAssetLibrary.save_loaded_asset(validation, only_if_is_dirty=False)
assert hashlib.sha256(pool_file.read_bytes()).hexdigest() == before
(OUT/"bake-summary.json").write_text(json.dumps({"streams": streams, "matching_seam_samples": sum(map(len, current)),
    "pods": pod_report, "flyer_required_headroom_cm": required, "level_asset": LEVEL, "surface_data": SURFACE,
    "production_pool_sha256": before, "production_definitions": len(definitions),
    "limits": ["Runtime reference bake; capsule, camera, NPC combat and PIE pending"]}, indent=2), encoding="utf-8")
unreal.log("RIDGE_BAKE_READY "+json.dumps(streams))
