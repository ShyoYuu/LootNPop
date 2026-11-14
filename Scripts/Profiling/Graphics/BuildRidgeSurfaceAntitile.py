"""1-B3c 반복 완화 시험 에셋만 만든다. 맵 전환/적용은 하지 않는다.

콘솔: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/BuildRidgeSurfaceAntitile.py"
"""
import json
from pathlib import Path
import sys

import unreal
from editor_toolset.toolsets.material import MaterialTools

sys.path.insert(0, str(Path(__file__).resolve().parent))
from BuildRidgeSurfaceMaterial import ALIGNED, NORMALS, call, connect
from BuildRidgeSurfaceProbe import custom, node

ROOT = Path(__file__).resolve().parents[3]
SOURCE = "/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V001"
BASE = "/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V002"
BOMB = "/Engine/Functions/Engine_MaterialFunctions01/Texturing/Texture_Bombing"
OUT = ROOT / "Saved/Profiling/Graphics/Phase01/RidgeSurfaceAntitile_20261114"
LIB = unreal.MaterialEditingLibrary
MACRO_CODE = """
struct RidgeMacroNoise
{
    float hashValue(float3 p)
    {
        p = frac(p * 0.1031);
        p += dot(p, p.yzx + 33.33);
        return frac((p.x + p.y) * p.z);
    }
    float smoothValue(float3 p)
    {
        float3 cell = floor(p);
        float3 f = frac(p);
        f = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(lerp(hashValue(cell), hashValue(cell + float3(1,0,0)), f.x),
                         lerp(hashValue(cell + float3(0,1,0)), hashValue(cell + float3(1,1,0)), f.x), f.y),
                    lerp(lerp(hashValue(cell + float3(0,0,1)), hashValue(cell + float3(1,0,1)), f.x),
                         lerp(hashValue(cell + float3(0,1,1)), hashValue(cell + float3(1,1,1)), f.x), f.y), f.z);
    }
};
RidgeMacroNoise noise;
float3 p = PositionLocal / max(Size, 1.0);
float value = 0.65 * noise.smoothValue(p) + 0.35 * noise.smoothValue(p * 0.37 + float3(7,13,19));
return max(0.05, 1.0 + Strength * (2.0 * value - 1.0));
"""


def add_ground_macro(material):
    expressions = {e.get_name(): e for e in LIB.get_material_expressions(material)}
    size = node(material, unreal.MaterialExpressionScalarParameter, -2800, 1600,
                parameter_name="Ground Macro Size", default_value=1800.0, group="Repetition")
    strength = node(material, unreal.MaterialExpressionScalarParameter, -2800, 1800,
                    parameter_name="Ground Macro Strength", default_value=0.5, group="Repetition")
    macro = custom(material, MACRO_CODE, ("PositionLocal", "Size", "Strength"),
                   unreal.CustomMaterialOutputType.CMOT_FLOAT1, -2400, 1600)
    connect(expressions["MaterialExpressionTransformPosition_0"], macro, "PositionLocal")
    connect(size, macro, "Size")
    connect(strength, macro, "Strength")
    color = node(material, unreal.MaterialExpressionMultiply, -2050, 1600)
    connect(expressions["MaterialExpressionMaterialFunctionCall_1"], color, "A", "XYZ Texture")
    connect(macro, color, "B")
    connect(color, expressions["MaterialExpressionDesaturation_1"], "")
    roughness = custom(material, "return saturate(Roughness + (Macro - 1.0) * 0.2);",
                       ("Roughness", "Macro"), unreal.CustomMaterialOutputType.CMOT_FLOAT1, -2050, 1900)
    connect(expressions["MaterialExpressionSaturate_4"], roughness, "Roughness")
    connect(macro, roughness, "Macro")
    connect(roughness, expressions["MaterialExpressionLinearInterpolate_2"], "B")


def replace_samples(function, is_normal):
    expressions = list(MaterialTools.get_expressions(function))
    samples = [e for e in expressions if isinstance(e, unreal.MaterialExpressionTextureSample)]
    assert len(samples) == 3, "Unexpected projection function version."
    offset = node(function, unreal.MaterialExpressionScalarParameter, -1800, -1000,
                  parameter_name="Bomb Offset", default_value=1.0, group="Repetition")
    one = node(function, unreal.MaterialExpressionConstant, -1800, -800, r=1.0)
    disabled = node(function, unreal.MaterialExpressionStaticBool, -1800, -600, value=False)
    normal_flag = node(function, unreal.MaterialExpressionStaticBool, -1800, -400, value=is_normal)
    for index, sample in enumerate(samples):
        incoming = {str(p.input_name): p for p in MaterialTools.get_expression_inputs(function, sample)}
        consumers = [(e, p) for e in expressions
                     for p in MaterialTools.get_expression_inputs(function, e) if p.expression == sample]
        bombing = call(function, BOMB, -1200, index * 600)
        for old_pin, new_pin in (("UVs", "UVs"), ("Tex", "Texture Object")):
            source = incoming[old_pin]
            connect(source.expression, bombing, new_pin, str(source.output_name))
        connect(offset, bombing, "Offset")
        connect(one, bombing, "Tiling")
        # 색/거칠기/노멀에 같은 블렌드 가중치를 쓴다. 높이 텍스처는 추가하지 않는다.
        connect(disabled, bombing, "Enable Height Lerp")
        connect(normal_flag, bombing, "Is Normalmap")
        for consumer, pin in consumers:
            channels = str(pin.output_name)
            # 미사용 ExportFloat4 분기는 원래 샘플을 유지한다.
            if channels in ("A", "RGBA"):
                continue
            assert channels in ("", "RGB", "R", "G", "B"), channels
            mask = node(function, unreal.MaterialExpressionComponentMask, -700, index * 600,
                        r="R" in (channels or "RGB"), g="G" in (channels or "RGB"),
                        b="B" in (channels or "RGB"), a=False)
            connect(bombing, mask, "")
            connect(mask, consumer, str(pin.input_name))
    LIB.update_material_function(function)


def statistics(material):
    stats = LIB.get_statistics(material)
    fields = ("num_vertex_shader_instructions", "num_pixel_shader_instructions", "num_samplers",
              "num_vertex_texture_samples", "num_pixel_texture_samples", "num_virtual_texture_samples")
    return {field: stats.get_editor_property(field) for field in fields}


def build():
    dirty = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) +
             list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()))
    assert not dirty, "Save existing changes before building the anti-tiling material."
    assert not unreal.EditorAssetLibrary.list_assets(BASE, recursive=True, include_folder=False), "Version already exists."
    baseline = unreal.load_asset(SOURCE + "/MI_Ridge_Surface")
    assert baseline
    baseline_stats = statistics(baseline)
    functions = []
    for source, name, normal in ((ALIGNED, "MF_RidgeBombAlignedTexture", False),
                                 (NORMALS, "MF_RidgeBombAlignedNormal", True)):
        function = unreal.EditorAssetLibrary.duplicate_asset(source, BASE + "/" + name)
        assert function
        replace_samples(function, normal)
        functions.append(function)
    material = unreal.EditorAssetLibrary.duplicate_asset(SOURCE + "/M_Ridge_Surface", BASE + "/M_Ridge_Surface")
    instance = unreal.EditorAssetLibrary.duplicate_asset(SOURCE + "/MI_Ridge_Surface", BASE + "/MI_Ridge_Surface")
    assert material and instance
    LIB.set_material_instance_parent(instance, material)
    replacements = {ALIGNED: functions[0], NORMALS: functions[1]}
    counts = {ALIGNED: 0, NORMALS: 0}
    for expression in LIB.get_material_expressions(material):
        if isinstance(expression, unreal.MaterialExpressionMaterialFunctionCall):
            source = expression.get_editor_property("material_function").get_path_name().split(".")[0]
            if source in replacements:
                assert expression.set_material_function(replacements[source])
                counts[source] += 1
    assert list(counts.values()) == [4, 2], counts
    add_ground_macro(material)
    # 컴파일 오류가 나면 저장/적용하지 않고 미완성 시험 에셋만 남긴다.
    MaterialTools.recompile(material)
    candidate_stats = statistics(instance)
    for asset in functions + [material, instance]:
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    OUT.mkdir(parents=True, exist_ok=True)
    report = {"status": "assets generated; WIP visual validation pending", "source": SOURCE,
              "candidate": BASE, "replaced_calls": counts, "baseline_statistics": baseline_stats,
              "candidate_statistics": candidate_stats,
              "notes": ["Engine Texture_Bombing, height lerp disabled, normal-map correction enabled for normals.",
                        "Ground-only macro color/roughness variation uses local position and adds no texture lookups.",
                        "Original projection frames, world normal output and layer parameters retained.",
                        "Editor shader estimates are not packaged GPU timings."]}
    (OUT / "Creation.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("Ridge anti-tiling assets created. Validate before applying to the WIP terrain.")


if __name__ == "__main__":
    build()
