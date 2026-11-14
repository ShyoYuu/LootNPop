"""1-B3a 좌표·경사·구역 진단 에셋만 만든다. 에디터에서 한 번 실행한다.

콘솔: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/BuildRidgeSurfaceProbe.py"
원본 그래프 파생/텍스처 투영 전 진단이다. 레벨 복제/전환/적용은 이 스크립트에서 하지 않는다.
"""
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[3]
BASE = "/Game/Graphics/Environment/ElectricDreams/RidgeProbe_V001"
OUT = ROOT / "Saved/Profiling/Graphics/Phase01/RidgeSurfaceProbe_20261114"
# 1-B0 §8.2의 반시계 방향 경계. 구면 호 길이가 아닌 spec 접평면 좌표(cm)다.
ZONE = ((-3200, -3400), (1200, -4300), (6500, -3600), (9000, -1500),
        (9000, 1800), (5500, 3800), (-2400, 3800), (-3200, 1800))
METRIC_CODE = """
const float radius = 30000.0;
const float3 radial = float3(1, 1, 1) / sqrt(3.0);
const float3 axisX = float3(-1, -1, 2) / sqrt(6.0);
const float3 axisY = float3(-1, 1, 0) / sqrt(2.0);
float depth = dot(PositionLocal, radial);
float2 xy = radius * float2(dot(PositionLocal, axisX), dot(PositionLocal, axisY)) / depth;
float upDot = clamp(dot(normalize(NormalLocal), -normalize(PositionLocal)), -1.0, 1.0);
const float2 boundary[8] = {BOUNDARY};
float inside = 1.0;
for (int i = 0; i < 8; ++i)
{
    float2 edge = boundary[(i + 1) % 8] - boundary[i];
    float2 offset = xy - boundary[i];
    inside *= step(0.0, edge.x * offset.y - edge.y * offset.x);
}
// 이음매 보존 15m와 그 바깥 전이 40m를 모두 제외한다.
inside *= step(5500.0, min(PositionLocal.x, min(PositionLocal.y, PositionLocal.z)));
return float4(xy, upDot, inside);
""".replace("BOUNDARY", ", ".join(f"float2({x}.0, {y}.0)" for x, y in ZONE))
DISPLAY_CODE = """
float3 outside = float3(0.08, 0.08, 0.08);
float3 color = float3(0.05, 0.6, 0.15);
if (Mode > 0.5 && Mode < 1.5)
{
    // 30/70도는 장식 진단 분류이며 Support/보행 판정이 아니다.
    color = Data.z >= 0.8660254 ? float3(0.05, 0.6, 0.15) :
           (Data.z >= 0.3420201 ? float3(0.8, 0.5, 0.02) : float3(0.8, 0.03, 0.02));
}
if (Mode >= 1.5)
{
    float2 uv = Data.xy / 1000.0;
    float2 distanceToLine = abs(frac(uv + 0.5) - 0.5);
    float2 width = max(fwidth(uv), float2(0.001, 0.001));
    float gridLine = 1.0 - min(saturate(distanceToLine.x / width.x), saturate(distanceToLine.y / width.y));
    color = lerp(float3(0.03, 0.08, 0.15), float3(0.8, 0.8, 0.8), gridLine);
}
return lerp(outside, color, Data.w);
"""


def node(owner, kind, x, y, **properties):
    create = (unreal.MaterialEditingLibrary.create_material_expression_in_function
              if isinstance(owner, unreal.MaterialFunction)
              else unreal.MaterialEditingLibrary.create_material_expression)
    result = create(owner, kind, x, y)
    assert result, kind
    for key, value in properties.items():
        result.set_editor_property(key, value)
    return result


def custom(owner, code, inputs, output_type, x, y):
    values = []
    for name in inputs:
        value = unreal.CustomInput()
        value.set_editor_property("input_name", name)
        values.append(value)
    return node(owner, unreal.MaterialExpressionCustom, x, y,
                code=code, inputs=values, output_type=output_type)


def wire(source, target, pin):
    assert unreal.MaterialEditingLibrary.connect_material_expressions(source, "", target, pin), pin


def build():
    # 사용자 변경을 임의로 저장하지 않고, 미저장 변경이 있으면 생성 전에 중단한다.
    dirty = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) +
             list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()))
    assert not dirty, "Save your existing changes before running the probe builder."
    assert not unreal.EditorAssetLibrary.list_assets(BASE, recursive=True, include_folder=False), "Probe version already exists."
    OUT.mkdir(parents=True, exist_ok=True)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    function = tools.create_asset("MF_RidgeSurfaceCoordinates", BASE, unreal.MaterialFunction,
                                  unreal.MaterialFunctionFactoryNew())
    assert function
    position = node(function, unreal.MaterialExpressionFunctionInput, -700, -100,
                    input_name="PositionLocal", input_type=unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3,
                    sort_priority=0)
    normal = node(function, unreal.MaterialExpressionFunctionInput, -700, 150,
                  input_name="NormalLocal", input_type=unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3,
                  sort_priority=1)
    metrics = custom(function, METRIC_CODE, ("PositionLocal", "NormalLocal"),
                     unreal.CustomMaterialOutputType.CMOT_FLOAT4, -350, 0)
    output = node(function, unreal.MaterialExpressionFunctionOutput, 0, 0, output_name="SurfaceData")
    wire(position, metrics, "PositionLocal")
    wire(normal, metrics, "NormalLocal")
    wire(metrics, output, "")
    unreal.MaterialEditingLibrary.update_material_function(function)
    material = tools.create_asset("M_RidgeSurfaceProbe", BASE, unreal.Material, unreal.MaterialFactoryNew())
    assert material
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    world_position = node(material, unreal.MaterialExpressionWorldPosition, -1000, -150)
    local_position = node(material, unreal.MaterialExpressionTransformPosition, -750, -150,
                          transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
                          transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    world_normal = node(material, unreal.MaterialExpressionVertexNormalWS, -1000, 150)
    local_normal = node(material, unreal.MaterialExpressionTransform, -750, 150,
                        transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                        transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    call = node(material, unreal.MaterialExpressionMaterialFunctionCall, -500, 0)
    assert call.set_material_function(function)
    mode = node(material, unreal.MaterialExpressionScalarParameter, -500, 300,
                parameter_name="ProbeMode", default_value=1.0, group="Probe")
    display = custom(material, DISPLAY_CODE, ("Data", "Mode"),
                     unreal.CustomMaterialOutputType.CMOT_FLOAT3, -150, 0)
    for source, target, pin in ((world_position, local_position, ""),
                                (world_normal, local_normal, ""),
                                (local_position, call, "PositionLocal"),
                                (local_normal, call, "NormalLocal"),
                                (call, display, "Data"), (mode, display, "Mode")):
        wire(source, target, pin)
    assert unreal.MaterialEditingLibrary.connect_material_property(
        display, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    assets = [function, material]
    for name, value in (("Scope", 0.0), ("Slope", 1.0), ("Grid", 2.0)):
        instance = tools.create_asset("MI_RidgeProbe_" + name, BASE, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
        assert instance
        unreal.MaterialEditingLibrary.set_material_instance_parent(instance, material)
        # UE 5.8 setter는 설정 후에도 false를 반환하므로 실제 값을 재조회한다.
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(instance, "ProbeMode", value)
        assert unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(instance, "ProbeMode") == value
        assets.append(instance)
    for asset in assets:
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    report = {"status": "assets created; WIP Save As and application pending",
              "assets": [asset.get_path_name() for asset in assets], "zone_cm": ZONE}
    (OUT / "Creation.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("Ridge surface probe assets created. Create the WIP map with Save As before applying the probe.")


if __name__ == "__main__":
    build()
