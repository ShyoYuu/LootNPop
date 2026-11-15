"""1-B3d 축당 두 샘플을 쓰는 반복 완화 파생본4개만 만든다.

콘솔: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/BuildRidgeSurfaceReduced.py"
기존 V001/V002·원본·맵·장식은 수정하지 않는다. 생성 뒤 비교 검증을 거쳐 WIP에 적용한다.
"""
import json
from pathlib import Path
import sys

import unreal
from editor_toolset.toolsets.material import MaterialTools

sys.path.insert(0, str(Path(__file__).resolve().parent))
from BuildRidgeSurfaceMaterial import ALIGNED, NORMALS, connect
from BuildRidgeSurfaceProbe import node
from BuildRidgeSurfaceAntitile import statistics

ROOT = Path(__file__).resolve().parents[3]
SOURCE = "/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V002"
BASE = "/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V003"
OUT = ROOT / "Saved/Profiling/Graphics/Phase01/RidgeSurfaceReduced_20261115"
LIB = unreal.MaterialEditingLibrary
UV_CODE = """
struct RidgeTwoTap
{
    float hashValue(float2 p)
    {
        return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
    }
};
RidgeTwoTap noise;
float2 p = UV * 0.25;
float2 cellCoord = floor(p);
float2 f = frac(p);
f = f * f * (3.0 - 2.0 * f);
float value = lerp(lerp(noise.hashValue(cellCoord), noise.hashValue(cellCoord + float2(1,0)), f.x),
                   lerp(noise.hashValue(cellCoord + float2(0,1)), noise.hashValue(cellCoord + float2(1,1)), f.x), f.y);
float pattern = value * 8.0;
float bucket = floor(pattern);
Weight = smoothstep(0.25, 0.75, frac(pattern));
float2 offsetA = frac(sin((bucket + 1.0) * float2(17.13, 53.71)) * 43758.5453) * Offset;
float2 offsetB = frac(sin((bucket + 2.0) * float2(17.13, 53.71)) * 43758.5453) * Offset;
return float4(UV + offsetA, UV + offsetB);
"""


def replace_samples(function, is_normal):
    expressions = list(MaterialTools.get_expressions(function))
    samples = [e for e in expressions if isinstance(e, unreal.MaterialExpressionTextureSample)]
    assert len(samples) == 3, "Unexpected projection graph version."
    offset = node(function, unreal.MaterialExpressionScalarParameter, -2200, -1400,
                  parameter_name="Bomb Offset", default_value=1.0, group="Repetition")
    for index, original in enumerate(samples):
        incoming = {str(p.input_name): p for p in MaterialTools.get_expression_inputs(function, original)}
        consumers = [(e, p) for e in expressions for p in MaterialTools.get_expression_inputs(function, e)
                     if p.expression == original and str(p.output_name) not in ("A", "RGBA")]
        inputs = []
        for name in ("UV", "Offset"):
            value = unreal.CustomInput()
            value.set_editor_property("input_name", name)
            inputs.append(value)
        weight = unreal.CustomOutput()
        weight.set_editor_property("output_name", "Weight")
        weight.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        coords = node(function, unreal.MaterialExpressionCustom, -1800, index * 650,
                      inputs=inputs, additional_outputs=[weight], code=UV_CODE,
                      output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4,
                      desc="두 UV 이동 샘플과 공통 혼합 가중치")
        source = incoming["UVs"]
        connect(source.expression, coords, "UV", str(source.output_name))
        connect(offset, coords, "Offset")
        dx = node(function, unreal.MaterialExpressionDDX, -1800, index * 650 + 220)
        dy = node(function, unreal.MaterialExpressionDDY, -1800, index * 650 + 340)
        for derivative in (dx, dy):
            connect(source.expression, derivative, "", str(source.output_name))
        taps = []
        for tap_index in range(2):
            uv = node(function, unreal.MaterialExpressionComponentMask, -1400, index * 650 + tap_index * 220,
                      r=tap_index == 0, g=tap_index == 0, b=tap_index == 1, a=tap_index == 1)
            connect(coords, uv, "")
            sample = node(function, unreal.MaterialExpressionTextureSample, -1100, index * 650 + tap_index * 220,
                          sampler_type=original.get_editor_property("sampler_type"),
                          sampler_source=original.get_editor_property("sampler_source"),
                          mip_value_mode=unreal.TextureMipValueMode.TMVM_DERIVATIVE)
            connect(uv, sample, "UVs")
            texture = incoming["Tex"]
            connect(texture.expression, sample, "Tex", str(texture.output_name))
            # UV 오프셋 선택 경계가 ddx/ddy와 mip을 오염시키지 않도록 원래 UV 미분을 쓴다.
            connect(dx, sample, "DDX(UVs)")
            connect(dy, sample, "DDY(UVs)")
            taps.append(sample)
        blend = node(function, unreal.MaterialExpressionLinearInterpolate, -750, index * 650)
        connect(taps[0], blend, "A", "RGB")
        connect(taps[1], blend, "B", "RGB")
        connect(coords, blend, "Alpha", "Weight")
        result = blend
        if is_normal:
            result = node(function, unreal.MaterialExpressionNormalize, -550, index * 650)
            connect(blend, result, "")
        for consumer, pin in consumers:
            channels = str(pin.output_name) or "RGB"
            assert channels in ("RGB", "R", "G", "B"), channels
            mask = node(function, unreal.MaterialExpressionComponentMask, -300, index * 650,
                        r="R" in channels, g="G" in channels, b="B" in channels, a=False)
            connect(result, mask, "")
            connect(mask, consumer, str(pin.input_name))
    LIB.update_material_function(function)


def build():
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first."
    assert not (list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) +
                list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())), "Save existing changes first."
    assert not unreal.EditorAssetLibrary.list_assets(BASE, recursive=True, include_folder=False), "Version already exists."
    baseline = unreal.load_asset(SOURCE + "/MI_Ridge_Surface")
    assert baseline
    baseline_stats = statistics(baseline)
    functions = []
    for source, name, normal in ((ALIGNED, "MF_RidgeTwoTapTexture", False),
                                 (NORMALS, "MF_RidgeTwoTapNormal", True)):
        function = unreal.EditorAssetLibrary.duplicate_asset(source, BASE + "/" + name)
        assert function
        replace_samples(function, normal)
        functions.append(function)
    material = unreal.EditorAssetLibrary.duplicate_asset(SOURCE + "/M_Ridge_Surface", BASE + "/M_Ridge_Surface")
    assert material
    old_functions = (SOURCE + "/MF_RidgeBombAlignedTexture", SOURCE + "/MF_RidgeBombAlignedNormal")
    replacements = dict(zip(old_functions, functions))
    counts = dict.fromkeys(old_functions, 0)
    for expression in LIB.get_material_expressions(material):
        if isinstance(expression, unreal.MaterialExpressionMaterialFunctionCall):
            source = expression.get_editor_property("material_function").get_path_name().split(".")[0]
            if source in replacements:
                assert expression.set_material_function(replacements[source])
                counts[source] += 1
    assert list(counts.values()) == [4, 2], counts
    MaterialTools.recompile(material)
    instance = unreal.EditorAssetLibrary.duplicate_asset(SOURCE + "/MI_Ridge_Surface", BASE + "/MI_Ridge_Surface")
    assert instance
    LIB.set_material_instance_parent(instance, material)
    LIB.update_material_instance(instance)
    candidate_stats = statistics(instance)
    assert 0 < candidate_stats["num_virtual_texture_samples"] < baseline_stats["num_virtual_texture_samples"]
    assert 0 < candidate_stats["num_pixel_shader_instructions"] < baseline_stats["num_pixel_shader_instructions"]
    for asset in functions + [material, instance]:
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    OUT.mkdir(parents=True, exist_ok=True)
    report = {"status": "assets generated; visual and neutral-normal validation pending; map unchanged",
              "source": SOURCE, "candidate": BASE, "replaced_calls": counts, "uv_code": UV_CODE,
              "baseline_statistics": baseline_stats, "candidate_statistics": candidate_stats,
              "notes": ["Two translated samples per projection axis; no random UV rotation or mirroring.",
                        "Same coordinate weights for color, masks and normals; normal blend normalized.",
                        "Original UV derivatives supplied explicitly; no noise texture added.",
                        "V002 local projection, slope/scope, shading and ground macro preserved.",
                        "Editor statistics are not packaged GPU timings."]}
    (OUT / "Creation.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("Ridge reduced-sampling material assets saved; visual and normal validation pending.")


if __name__ == "__main__":
    build()
