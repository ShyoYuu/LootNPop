"""1-B3f 바닥 삼각 격자3샘플 후보4개만 만든다. 맵은 변경하지 않는다.

콘솔: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/BuildRidgeSurfaceTriangle.py"
V004의 절벽2샘플·구면 좌표·노멀 프레임·구역/경사·macro를 유지한다.
"""
import json
from pathlib import Path
import sys

import unreal
from editor_toolset.toolsets.material import MaterialTools

sys.path.insert(0, str(Path(__file__).resolve().parent))
from BuildRidgeSurfaceMaterial import ALIGNED, NORMALS, connect
from BuildRidgeSurfaceProbe import custom, node
from BuildRidgeSurfaceAntitile import statistics

ROOT = Path(__file__).resolve().parents[3]
SOURCE = "/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V004"
GROUND = "/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V002"
BASE = "/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V005"
OUT = ROOT / "Saved/Profiling/Graphics/Phase01/RidgeSurfaceTriangle_20261116"
LIB = unreal.MaterialEditingLibrary
UV_CODE = """
struct RidgeTriangle
{
    float2 offset(float2 vertex)
    {
        return frac(sin(float2(dot(vertex, float2(127.1, 311.7)),
                               dot(vertex, float2(269.5, 183.3)))) * 43758.5453);
    }
};
RidgeTriangle noise;
float2 p = UV * 3.464101615;
float2 skewed = float2(p.x - 0.577350269 * p.y, 1.154700538 * p.y);
float2 cell = floor(skewed);
float2 f = frac(skewed);
float3 barycentric;
float2 vertexA, vertexB, vertexC;
if (f.x + f.y < 1.0)
{
    barycentric = float3(1.0 - f.x - f.y, f.x, f.y);
    vertexA = cell;
    vertexB = cell + float2(1, 0);
    vertexC = cell + float2(0, 1);
}
else
{
    barycentric = float3(f.x + f.y - 1.0, 1.0 - f.y, 1.0 - f.x);
    vertexA = cell + float2(1, 1);
    vertexB = cell + float2(1, 0);
    vertexC = cell + float2(0, 1);
}
// 단순 선형 혼합의 흐려짐을 줄이는 고정 가중치다. 히스토그램 보존식은 아니다.
float3 sharpened = barycentric * barycentric;
sharpened *= sharpened;
Weights = sharpened / max(dot(sharpened, float3(1, 1, 1)), 0.00001);
ThirdUV = UV + noise.offset(vertexC) * Offset;
return float4(UV + noise.offset(vertexA) * Offset,
              UV + noise.offset(vertexB) * Offset);
"""


def replace_samples(function, is_normal):
    expressions = list(MaterialTools.get_expressions(function))
    samples = [e for e in expressions if isinstance(e, unreal.MaterialExpressionTextureSample)]
    assert len(samples) == 3, "Unexpected projection graph version."
    offset = node(function, unreal.MaterialExpressionScalarParameter, -2400, -1400,
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
        outputs = []
        for name, kind in (("ThirdUV", unreal.CustomMaterialOutputType.CMOT_FLOAT2),
                           ("Weights", unreal.CustomMaterialOutputType.CMOT_FLOAT3)):
            value = unreal.CustomOutput()
            value.set_editor_property("output_name", name)
            value.set_editor_property("output_type", kind)
            outputs.append(value)
        y = index * 900
        coords = node(function, unreal.MaterialExpressionCustom, -2000, y,
                      inputs=inputs, additional_outputs=outputs, code=UV_CODE,
                      output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4,
                      desc="삼각 격자 UV3개와 공통 혼합 가중치")
        source = incoming["UVs"]
        connect(source.expression, coords, "UV", str(source.output_name))
        connect(offset, coords, "Offset")
        dx = node(function, unreal.MaterialExpressionDDX, -2000, y + 300)
        dy = node(function, unreal.MaterialExpressionDDY, -2000, y + 450)
        for derivative in (dx, dy):
            connect(source.expression, derivative, "", str(source.output_name))
        taps = []
        for tap_index in range(3):
            uv = coords
            uv_output = "ThirdUV"
            if tap_index < 2:
                uv = node(function, unreal.MaterialExpressionComponentMask, -1600, y + tap_index * 250,
                          r=tap_index == 0, g=tap_index == 0, b=tap_index == 1, a=tap_index == 1)
                connect(coords, uv, "")
                uv_output = ""
            sample = node(function, unreal.MaterialExpressionTextureSample, -1300, y + tap_index * 250,
                          sampler_type=original.get_editor_property("sampler_type"),
                          sampler_source=original.get_editor_property("sampler_source"),
                          mip_value_mode=unreal.TextureMipValueMode.TMVM_DERIVATIVE)
            connect(uv, sample, "UVs", uv_output)
            texture = incoming["Tex"]
            connect(texture.expression, sample, "Tex", str(texture.output_name))
            # 모든 이동 샘플에 원래 UV 미분을 공급해 선택 경계의 mip 오염을 피한다.
            connect(dx, sample, "DDX(UVs)")
            connect(dy, sample, "DDY(UVs)")
            taps.append(sample)
        blend = custom(function, "return A * Weights.x + B * Weights.y + C * Weights.z;",
                       ("A", "B", "C", "Weights"), unreal.CustomMaterialOutputType.CMOT_FLOAT3, -850, y)
        for name, sample in zip(("A", "B", "C"), taps):
            connect(sample, blend, name, "RGB")
        connect(coords, blend, "Weights", "Weights")
        result = blend
        if is_normal:
            result = node(function, unreal.MaterialExpressionNormalize, -600, y)
            connect(blend, result, "")
        for consumer, pin in consumers:
            channels = str(pin.output_name) or "RGB"
            assert channels in ("RGB", "R", "G", "B"), channels
            mask = node(function, unreal.MaterialExpressionComponentMask, -350, y,
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
    for source, name, normal in ((ALIGNED, "MF_RidgeTriangleTexture", False),
                                 (NORMALS, "MF_RidgeTriangleNormal", True)):
        function = unreal.EditorAssetLibrary.duplicate_asset(source, BASE + "/" + name)
        assert function
        replace_samples(function, normal)
        functions.append(function)
    material = unreal.EditorAssetLibrary.duplicate_asset(SOURCE + "/M_Ridge_Surface", BASE + "/M_Ridge_Surface")
    assert material
    replacements = dict(zip((GROUND + "/MF_RidgeBombAlignedTexture", GROUND + "/MF_RidgeBombAlignedNormal"), functions))
    replaced = []
    retained = []
    for expression in LIB.get_material_expressions(material):
        if not isinstance(expression, unreal.MaterialExpressionMaterialFunctionCall):
            continue
        path = expression.get_editor_property("material_function").get_path_name().split(".")[0]
        if path in replacements:
            pins = {str(p.input_name): p for p in MaterialTools.get_expression_inputs(material, expression)}
            texture = pins["TextureObject" if path.endswith("Texture") else "Texture"].expression
            assert texture.get_editor_property("texture").get_name().startswith("T_MossyRockyGround_")
            assert expression.set_material_function(replacements[path])
            replaced.append(expression.get_name())
        elif path.startswith("/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V003/MF_RidgeTwoTap"):
            retained.append(expression.get_name())
    assert len(replaced) == 3 and len(retained) == 3, (replaced, retained)
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
    report = {"status": "assets saved; visual and neutral-normal validation pending; map unchanged",
              "source": SOURCE, "candidate": BASE, "ground_calls_replaced": replaced,
              "cliff_calls_retained": retained, "uv_code": UV_CODE,
              "baseline_statistics": baseline_stats, "candidate_statistics": candidate_stats,
              "notes": ["Three translated samples per axis on a triangular grid; no rotation or mirroring.",
                        "Shared fourth-power barycentric weights for color/masks/normals; normals normalized.",
                        "Original UV derivatives supplied explicitly; no noise texture or histogram LUT.",
                        "This prototype is not histogram-preserving; contrast and blur require visual review.",
                        "V004 cliff path, spherical coordinates, slope/scope and macro retained.",
                        "Editor estimates are not packaged GPU timings."]}
    (OUT / "Creation.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("Ridge triangular sampling candidate saved; validate before applying to WIP terrain.")


if __name__ == "__main__":
    build()
