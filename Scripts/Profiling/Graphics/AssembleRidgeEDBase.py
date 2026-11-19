"""V012를 U1 작업 레벨에 반입하거나 TestMap03에서 조기 SurfaceData를 굽는다."""
import json
import math
from pathlib import Path
import shutil
import struct
import sys

import unreal

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parents[1])]
import AssembleRidgeOctant as assembly
from BuildRidgeBlueprintGhost import FOLDER, LEVEL, verify_ghosts
from InspectAcropolisBake import seams
import RidgeEDBaseSpec as base

OUT = assembly.OUT.with_name(base.VERSION)
SURFACE = FOLDER + "/DA_OctantSurface_LVI_Ridge_EDTerrain_V001"
TEST_MAP = "/Game/Maps/TestMap03"
GROUND = "/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V005/MI_Ridge_Surface"
ROCK = "/Game/Maps/Ridge_00/M_Ridge_Rock"


def read(name):
    return json.loads((OUT / name).read_text(encoding="utf-8"))


def write(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def content_file(path, extension):
    return (assembly.ROOT / "Content" / path.removeprefix("/Game/")).with_suffix(extension)


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def open_level(levels, path):
    if editor_world().get_path_name().split(".")[0] != path:
        assert levels.load_level(path), path


def verify_placement(actors, manifest, placement):
    level_actors = actors.get_all_level_actors()
    verify_ghosts(level_actors)
    by_label = {actor.get_actor_label(): actor for actor in level_actors}
    for item in manifest["exports"]:
        component = by_label[item["name"]].static_mesh_component
        assert component.static_mesh.get_path_name().split(".")[0] == FOLDER + "/" + item["name"]
        if item["role"] == "Crust":
            assert component.get_material(0).get_path_name().split(".")[0] == GROUND
            profile = "LNPStaticTerrain"
        else:
            profile = "LNPStaticTerrain" if item["role"] == "Floor" else "LNPStaticBlocker"
        assert str(component.get_collision_profile_name()) == profile, item["name"]
    marker_report = {}
    for key, prefix, lift in (("pods", "Spawn_", 0), ("anchors", "Anchor_", 0), ("nodes", "Review_Ridge", 200)):
        rows = placement[key]
        deltas = []
        for item in rows:
            x, y, h = item["local"]
            location = by_label[prefix + item["id"]].get_actor_location()
            deltas.append(math.dist((location.x, location.y, location.z), base.ridge.point(x, y, h + lift)))
        assert max(deltas) <= .05, (key, max(deltas))
        marker_report[key] = {"count": len(rows), "max_world_delta_cm": max(deltas)}
    return marker_report


def assemble(levels):
    manifest, placement = read("manifest.json"), read("placement.json")
    assert manifest["version"] == base.VERSION and not read("geometry-verification.json")["failures"]
    open_level(levels, LEVEL)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    current = actors.get_all_level_actors()
    verify_ghosts(current)
    expected = {item["name"] for item in manifest["exports"]}
    installed = expected & {actor.get_actor_label() for actor in current}
    if not installed:
        assert len(current) == 3, "Review current WIP actors before adding V012."
        ground, rock = unreal.load_asset(GROUND), unreal.load_asset(ROCK)
        assert ground and rock
        backup = OUT / "BeforeV012.umap"
        if not backup.exists():
            shutil.copy2(content_file(LEVEL, ".umap"), backup)
        assembly.import_assets(manifest, FOLDER, OUT)
        assembly.place_geometry(actors, manifest, FOLDER, ground, rock)
        assembly.place_markers(actors, placement)
    else:
        assert installed == expected, "Review the partial WIP assembly before retrying."
    write("native-assembly.json", verify_placement(actors, manifest, placement))
    for actor in current:
        if actor.get_actor_label().startswith("Ghost_V011_"):
            actor.set_is_temporarily_hidden_in_editor(True)
    if not installed:
        assert levels.save_current_level(), LEVEL
    unreal.log("RIDGE_ED_BASE_ASSEMBLY_READY " + LEVEL)


def bake(levels):
    open_level(levels, TEST_MAP)
    surface_file = content_file(SURFACE, ".uasset")
    prior_mtime = surface_file.stat().st_mtime_ns if surface_file.exists() else None
    unreal.SystemLibrary.execute_console_command(editor_world(), "LNP.SurfaceNav.BakeOctant " + LEVEL)
    assert surface_file.exists() and surface_file.stat().st_mtime_ns != prior_mtime, "SurfaceData was not saved."
    data = unreal.load_asset(SURFACE)
    header = data.get_editor_property("header")
    manifest = read("manifest.json")
    packages = [str(row.get_editor_property("package_name")) for row in header.get_editor_property("source_manifest")]
    assert set(packages) == {LEVEL, *(FOLDER + "/" + item["name"] for item in manifest["exports"])}, packages
    reference, current = seams("/Game/Maps/Meadow_00/DA_OctantSurface_Meadow_00"), seams(SURFACE)
    assert reference == current, "Baked V012 seam differs from Meadow."
    _, layers = struct.unpack_from("<HH", bytes(data.get_editor_property("support_payload")))
    _, _, authored, candidates = struct.unpack_from("<HHII", bytes(data.get_editor_property("spawn_payload")))
    assert authored == len(read("placement.json")["pods"]), authored
    streams = {name: header.get_editor_property(name).get_editor_property("element_count")
               for name in ("support", "navigation", "traversal", "spawn", "conditional_patch")}
    report = {"level": LEVEL, "surface_data": SURFACE, "data_version": header.get_editor_property("data_version"),
              "matching_seam_samples": sum(map(len, current)), "max_seam_radius_delta_cm": 0,
              "layers": layers, "authored_spawn_anchors": authored, "spawn_candidates": candidates,
              "streams": streams, "source_packages": packages}
    write("early-bake.json", report)
    unreal.log("RIDGE_ED_BASE_BAKE_READY " + json.dumps(report))


if __name__ == "__main__":
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    saving = unreal.EditorLoadingAndSavingUtils
    assert not levels.is_in_play_in_editor(), "Stop PIE first."
    dirty = list(saving.get_dirty_map_packages()) + list(saving.get_dirty_content_packages())
    assert not dirty, ("Review the unsaved changes first.", [p.get_path_name() for p in dirty])
    assert len(sys.argv) == 2 and sys.argv[1] in {"assemble", "bake"}
    {"assemble": assemble, "bake": bake}[sys.argv[1]](levels)
