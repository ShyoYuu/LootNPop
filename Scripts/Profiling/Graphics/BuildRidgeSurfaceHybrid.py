"""1-B3d 바닥은 V002, 절벽은 V003인 파생 부모/MI2개를 만든다.

콘솔: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/BuildRidgeSurfaceHybrid.py"
V003 바닥 원경 실패를 보존하고, 원본/기존 후보/맵은 수정하지 않는다.
"""
import json
from pathlib import Path
import sys

import unreal
from editor_toolset.toolsets.material import MaterialTools

sys.path.insert(0, str(Path(__file__).resolve().parent))
from BuildRidgeSurfaceAntitile import statistics

ROOT = Path(__file__).resolve().parents[3]
SOURCE = "/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V003"
GROUND = "/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V002"
BASE = "/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V004"
OUT = ROOT / "Saved/Profiling/Graphics/Phase01/RidgeSurfaceReduced_20261115"
LIB = unreal.MaterialEditingLibrary


def build():
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first."
    assert not (list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) +
                list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())), "Save existing changes first."
    assert not unreal.EditorAssetLibrary.list_assets(BASE, recursive=True, include_folder=False), "Version already exists."
    original = unreal.load_asset(GROUND + "/MI_Ridge_Surface")
    failed = unreal.load_asset(SOURCE + "/MI_Ridge_Surface")
    assert original and failed
    baseline_stats = statistics(original)
    failed_stats = statistics(failed)
    material = unreal.EditorAssetLibrary.duplicate_asset(SOURCE + "/M_Ridge_Surface", BASE + "/M_Ridge_Surface")
    assert material
    replacements = {SOURCE + "/MF_RidgeTwoTapTexture": GROUND + "/MF_RidgeBombAlignedTexture",
                    SOURCE + "/MF_RidgeTwoTapNormal": GROUND + "/MF_RidgeBombAlignedNormal"}
    restored = []
    retained = []
    for expression in LIB.get_material_expressions(material):
        if not isinstance(expression, unreal.MaterialExpressionMaterialFunctionCall):
            continue
        path = expression.get_editor_property("material_function").get_path_name().split(".")[0]
        if path not in replacements:
            continue
        pins = {str(p.input_name): p for p in MaterialTools.get_expression_inputs(material, expression)}
        texture = pins["TextureObject" if path.endswith("Texture") else "Texture"].expression
        name = texture.get_editor_property("texture").get_name()
        if name.startswith("T_MossyRockyGround_"):
            assert expression.set_material_function(unreal.load_asset(replacements[path]))
            restored.append({"expression": expression.get_name(), "texture": name, "function": replacements[path]})
        else:
            retained.append({"expression": expression.get_name(), "texture": name, "function": path})
    assert len(restored) == 3 and len(retained) == 3, (restored, retained)
    MaterialTools.recompile(material)
    instance = unreal.EditorAssetLibrary.duplicate_asset(SOURCE + "/MI_Ridge_Surface", BASE + "/MI_Ridge_Surface")
    assert instance
    LIB.set_material_instance_parent(instance, material)
    LIB.update_material_instance(instance)
    candidate_stats = statistics(instance)
    assert 0 < candidate_stats["num_virtual_texture_samples"] < baseline_stats["num_virtual_texture_samples"]
    assert 0 < candidate_stats["num_pixel_shader_instructions"] < baseline_stats["num_pixel_shader_instructions"]
    for asset in (material, instance):
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    report = {"status": "hybrid assets saved; visual and neutral-normal review pending; map unchanged",
              "baseline_statistics": baseline_stats, "failed_v003_statistics": failed_stats,
              "candidate_statistics": candidate_stats, "ground_calls_restored": restored,
              "cliff_calls_retained": retained,
              "notes": ["V003 ground failed the user's distant checker-pattern review; V003 assets preserved.",
                        "Ground color/masks/normal reuse V002; cliff uses V003 two-tap projection.",
                        "Original slope/scope/ground macro and MI parameters retained.",
                        "Editor estimates are not packaged GPU timings."]}
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "HybridCreation.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("Ridge hybrid material assets saved; validate before applying to WIP terrain.")


if __name__ == "__main__":
    build()
