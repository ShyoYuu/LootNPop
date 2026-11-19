"""기존 Z1 작업본의 저장본을 보존하고 별도 조형 비교 레벨을 만든다."""
import hashlib
import json
from pathlib import Path
import sys

import unreal

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parents[1])]
import AssembleRidgeOctant as assembly
from AssembleRidgeEDBase import FOLDER as SOURCE_FOLDER, LEVEL as SOURCE_LEVEL, OUT as BASE_OUT, content_file, editor_world
from PlaceRidgeEDTerrain import asset_path, place
import RidgeZ1CompositionSpec as spec

OUT = BASE_OUT.with_name(spec.VERSION)
FOLDER = SOURCE_FOLDER + "/Z1Composition_V001"
LEVEL = FOLDER + "/LVI_Ridge_EDTerrain_Z1Composition_V001"
GROUND_SOURCE = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/ReferencePatch_V001/MI_Ridge_ReferenceRiverbed_V001"
GROUND = FOLDER + "/MI_Ridge_Z1_Channel"

# Ridge 구면 로컬 좌표의 V012 비교본 하상 폭과 가장자리를 따른다.
CHANNEL_COORDS = """
const float x = Data.x;
const float mid = -3250.0 + 3200.0 * smoothstep(-200.0, 5400.0, x);
const float across = Data.y - mid;
const float width = lerp(680.0, 1090.0, smoothstep(700.0, 4750.0, x));
const float ripple = 48.0 * sin(x / 151.0 + across / 283.0) + 23.0 * sin(x / 53.0);
const float edge = 1.0 - smoothstep(width * 0.38, width, abs(across + ripple));
const float ends = smoothstep(-2980.0, -1270.0, x) * (1.0 - smoothstep(5220.0, 6730.0, x));
const float2 uv = float2((across + 17.0 * sin(x / 213.0)) / 161.0, x / 246.0);
return float3(uv, edge * ends * smoothstep(0.72, 0.94, Data.z) * Data.w);
"""


def channel_material():
    assets = unreal.EditorAssetLibrary
    if assets.does_asset_exist(GROUND):
        return unreal.load_asset(GROUND)
    edit = unreal.MaterialEditingLibrary
    old = unreal.load_asset(GROUND_SOURCE)
    assert old
    parent = old.get_editor_property("parent")
    new_parent = assets.duplicate_asset(asset_path(parent), FOLDER + "/M_Ridge_Z1_Channel")
    assert new_parent
    expression, = [node for node in edit.get_material_expressions(new_parent)
                   if isinstance(node, unreal.MaterialExpressionCustom) and "return float3(uv," in node.get_editor_property("code")]
    expression.set_editor_property("code", CHANNEL_COORDS)
    assert not edit.recompile_material(new_parent), "Channel material compilation failed."
    instance = assets.duplicate_asset(GROUND_SOURCE, GROUND)
    assert instance
    edit.set_material_instance_parent(instance, new_parent)
    edit.update_material_instance(instance)
    assembly.save(new_parent)
    assembly.save(instance)
    return instance


def view_outlet():
    x, y = 1170, -4200
    eye = unreal.Vector(*spec.ridge.point(x, y, spec.ground(x, y) + 235))
    target = unreal.Vector(*spec.ridge.point(1650, -1500, -3080))
    rotation = unreal.MathLibrary.make_rot_from_xz(target - eye, eye * -1.0)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(eye, rotation)


def run(discard_source_edits):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    saving = unreal.EditorLoadingAndSavingUtils
    assets = unreal.EditorAssetLibrary
    assert not levels.is_in_play_in_editor() and not saving.get_dirty_content_packages()
    dirty = {p.get_path_name() for p in saving.get_dirty_map_packages()}
    assert not dirty or (discard_source_edits and dirty == {SOURCE_LEVEL} and asset_path(editor_world()) == SOURCE_LEVEL), dirty
    assert not assets.does_asset_exist(LEVEL), LEVEL
    manifest = json.loads((OUT / "manifest.json").read_text(encoding="utf-8"))
    former = json.loads((BASE_OUT / "manifest.json").read_text(encoding="utf-8"))
    preserved = {"exports": [row for row in former["exports"] if row["role"] != "Crust"]}
    assert unreal.load_asset(GROUND_SOURCE)
    assert all(assets.does_asset_exist(SOURCE_FOLDER + "/" + row["name"]) for row in preserved["exports"])
    source_file = content_file(SOURCE_LEVEL, ".umap")
    source_hash = hashlib.sha256(source_file.read_bytes()).hexdigest()

    assert saving.new_blank_map(False)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assembly.import_assets(manifest, FOLDER, OUT)
    ground = channel_material()
    assembly.place_geometry(actors, manifest, FOLDER, ground, ground)
    assembly.place_geometry(actors, preserved, SOURCE_FOLDER, ground, ground)
    # 이 비교본은 Z1의 기존 동선과 사선을 고정하지 않고 새 구도를 검토한다.
    report = place(actors, "Z1", list(spec.placements()), spec.ground, protected=((), (), ()))
    view_outlet()
    assert saving.save_map(editor_world(), LEVEL), LEVEL
    assert hashlib.sha256(source_file.read_bytes()).hexdigest() == source_hash
    report.update(level=LEVEL, source_level_sha256=source_hash, ground_material=asset_path(ground))
    (OUT / "composition.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    unreal.log("RIDGE_Z1_COMPOSITION_READY " + json.dumps(report))


if __name__ == "__main__":
    assert sys.argv[1:] in ([], ["discard-z1-transforms"])
    run(sys.argv[1:] == ["discard-z1-transforms"])
