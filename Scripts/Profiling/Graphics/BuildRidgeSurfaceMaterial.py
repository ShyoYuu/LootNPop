"""1-B3b 사암/이끼 2레이어 시험용 원본 그래프 파생본을 만든다.

콘솔: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/BuildRidgeSurfaceMaterial.py"
에셋만 생성한다. 맵 전환·지각 적용·원본 저장은 하지 않는다.
"""
import json
from pathlib import Path
import sys

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from BuildRidgeSurfaceProbe import custom, node

ROOT = Path(__file__).resolve().parents[3]
BASE = "/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V001"
SOURCE = "/Game/Landscape/M_BGLandscape_Auto"
SOURCE_MI = "/Game/Landscape/MI_BGLandscape_Auto_01"
COORDINATES = "/Game/Graphics/Environment/ElectricDreams/RidgeProbe_V001/MF_RidgeSurfaceCoordinates"
ALIGNED = "/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture"
NORMALS = "/Engine/Functions/Engine_MaterialFunctions02/WorldAlignedNormals_HighQuality"
OUT = ROOT / "Saved/Profiling/Graphics/Phase01/RidgeSurfaceMaterial_20261114"
LIB = unreal.MaterialEditingLibrary


def connect(source, target, pin, output=""):
    assert LIB.connect_material_expressions(source, output, target, pin), pin


def call(material, function, x, y):
    result = node(material, unreal.MaterialExpressionMaterialFunctionCall, x, y)
    assert result.set_material_function(unreal.load_asset(function))
    return result


def build():
    dirty = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) +
             list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()))
    assert not dirty, "Save existing changes before building the surface material."
    assert not unreal.EditorAssetLibrary.list_assets(BASE, recursive=True, include_folder=False), "Version already exists."
    assert unreal.load_asset(COORDINATES), "Build the coordinate probe first."
    material = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, BASE + "/M_Ridge_Surface")
    instance = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MI, BASE + "/MI_Ridge_Surface")
    assert material and instance
    LIB.set_material_instance_parent(instance, material)
    material.set_editor_property("bTangentSpaceNormal", False)
    expressions = {value.get_name(): value for value in LIB.get_material_expressions(material)}
    assert len(expressions) == 137, "Unexpected source graph version."
    slab = expressions["MaterialExpressionStrataSlabBSDF_0"]
    cliff_color = expressions["MaterialExpressionMaterialFunctionCall_3"]
    cliff_masks = expressions["MaterialExpressionMaterialFunctionCall_4"]
    samples = [expressions["MaterialExpressionTextureSample_" + str(i)] for i in (3, 4, 5)]
    assert samples[0].texture.get_name() == "T_MossyRockyGround_01_BC"
    assert samples[1].texture.get_name() == "T_MossyRockyGround_01_N"
    assert samples[2].texture.get_name() == "T_MossyRockyGround_01_AoRDp"
    parameters = {str(value.get_editor_property("parameter_name")): value
                  for value in expressions.values() if isinstance(value, unreal.MaterialExpressionScalarParameter)}
    position = node(material, unreal.MaterialExpressionWorldPosition, -3200, -1400)
    local_position = node(material, unreal.MaterialExpressionTransformPosition, -3000, -1400,
                          transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
                          transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    normal = node(material, unreal.MaterialExpressionVertexNormalWS, -3200, -1150)
    local_normal = node(material, unreal.MaterialExpressionTransform, -3000, -1150,
                        transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                        transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    connect(position, local_position, "")
    connect(normal, local_normal, "")
    metrics = call(material, COORDINATES, -2700, -1400)
    connect(local_position, metrics, "PositionLocal")
    connect(local_normal, metrics, "NormalLocal")
    weights = custom(material, "return float2(Data.w, smoothstep(0.3420201, 0.8660254, Data.z));",
                     ("Data",), unreal.CustomMaterialOutputType.CMOT_FLOAT2, -2400, -1400)
    connect(metrics, weights, "Data")
    scope = node(material, unreal.MaterialExpressionComponentMask, -2150, -1500, r=True, g=False, b=False, a=False)
    ground_weight = node(material, unreal.MaterialExpressionComponentMask, -2150, -1300, r=False, g=True, b=False, a=False)
    connect(weights, scope, "")
    connect(weights, ground_weight, "")
    one = node(material, unreal.MaterialExpressionConstant3Vector, -3200, -850,
               constant=unreal.LinearColor(1, 1, 1, 1))
    sizes = []
    for index, name in enumerate(("Cliff Tiling", "Mud Tiling")):
        size = node(material, unreal.MaterialExpressionMultiply, -2900, -850 + index * 180)
        connect(parameters[name], size, "A")
        connect(one, size, "B")
        sizes.append(size)
    # 이 파생본의 Mud Tiling은 cm 단위로 쓰며 원본 MI의 색/거칠기 설정은 유지한다.
    LIB.set_material_instance_scalar_parameter_value(instance, "Mud Tiling", 400.0)
    assert LIB.get_material_instance_scalar_parameter_value(instance, "Mud Tiling") == 400.0
    for expression in (cliff_color, cliff_masks):
        connect(local_position, expression, "WorldPosition")
        connect(local_normal, expression, "World Space Normal")
        connect(sizes[0], expression, "TextureSize")
    connect(cliff_color, expressions["MaterialExpressionDesaturation_3"], "", "XYZ Texture")
    cliff_roughness = node(material, unreal.MaterialExpressionComponentMask, -2100, -850,
                           r=False, g=True, b=False, a=False)
    connect(cliff_masks, cliff_roughness, "", "XYZ Texture")
    connect(cliff_roughness, expressions["MaterialExpressionLinearInterpolate_3"], "Alpha")
    objects = []
    for index, sample in enumerate(samples):
        objects.append(node(material, unreal.MaterialExpressionTextureObject, -3200, -300 + index * 180,
                            texture=sample.texture, sampler_type=sample.get_editor_property("sampler_type")))
    ground_color = call(material, ALIGNED, -2700, -300)
    ground_masks = call(material, ALIGNED, -2700, 100)
    for expression, texture in ((ground_color, objects[0]), (ground_masks, objects[2])):
        connect(texture, expression, "TextureObject")
        connect(sizes[1], expression, "TextureSize")
        connect(local_position, expression, "WorldPosition")
        connect(local_normal, expression, "World Space Normal")
    connect(ground_color, expressions["MaterialExpressionDesaturation_1"], "", "XYZ Texture")
    ground_roughness = node(material, unreal.MaterialExpressionComponentMask, -2100, 100,
                            r=False, g=True, b=False, a=False)
    connect(ground_masks, ground_roughness, "", "XYZ Texture")
    connect(ground_roughness, expressions["MaterialExpressionLinearInterpolate_4"], "Alpha")
    negative = node(material, unreal.MaterialExpressionConstant, -3200, 500, r=-1.0)
    world_normals = []
    for index, texture in enumerate((expressions["MaterialExpressionTextureObject_2"], objects[1])):
        divisor = node(material, unreal.MaterialExpressionMultiply, -2900, 500 + index * 450)
        connect(sizes[index], divisor, "A")
        connect(negative, divisor, "B")
        projected = call(material, NORMALS, -2650, 500 + index * 450)
        connect(texture, projected, "Texture")
        connect(divisor, projected, "Negative World Position Divisor")
        connect(local_position, projected, "World Position")
        connect(local_normal, projected, "WorldNormal")
        world = node(material, unreal.MaterialExpressionTransform, -2350, 500 + index * 450,
                     transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                     transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        connect(projected, world, "", "World Space Normals XYZ")
        world_normals.append(world)
    # 월드→탄젠트→월드 왕복 없이 기하 노멀과 투영 노멀을 같은 공간에서 섞는다.
    strength = node(material, unreal.MaterialExpressionLinearInterpolate, -1900, 500)
    connect(normal, strength, "A")
    connect(world_normals[0], strength, "B")
    connect(parameters["Cliff Normal Strength"], strength, "Alpha")
    blended_normal = node(material, unreal.MaterialExpressionLinearInterpolate, -1600, 650)
    connect(strength, blended_normal, "A")
    connect(world_normals[1], blended_normal, "B")
    connect(ground_weight, blended_normal, "Alpha")
    for name in ("MaterialExpressionLinearInterpolate_0", "MaterialExpressionLinearInterpolate_2"):
        connect(ground_weight, expressions[name], "Alpha")
    flat_material = unreal.load_asset("/Game/Maps/Ridge_00/M_Ridge_Ground")
    flat_source = LIB.get_material_property_input_node(flat_material, unreal.MaterialProperty.MP_BASE_COLOR)
    assert isinstance(flat_source, unreal.MaterialExpressionConstant3Vector)
    flat_color = node(material, unreal.MaterialExpressionConstant3Vector, -1600, -850,
                      constant=flat_source.get_editor_property("constant"))
    # 범위 밖은 기존 단색 재질의 기본 입력을 재현한다. 조명 정합은 WIP에서 확인한다.
    outside_roughness = node(material, unreal.MaterialExpressionConstant, -1600, -500, r=0.5)
    outside_f0 = node(material, unreal.MaterialExpressionConstant, -1600, -300, r=0.04)
    outside_f90 = node(material, unreal.MaterialExpressionConstant, -1600, -100, r=1.0)
    outputs = (("Diffuse Albedo", expressions["MaterialExpressionLinearInterpolate_0"], flat_color),
               ("Roughness", expressions["MaterialExpressionLinearInterpolate_2"], outside_roughness),
               ("Normal", blended_normal, normal), ("F0", parameters["F0"], outside_f0),
               ("F90", parameters["F90"], outside_f90))
    for index, (pin, inside, outside) in enumerate(outputs):
        blend = node(material, unreal.MaterialExpressionLinearInterpolate, -1100, -800 + index * 250)
        connect(outside, blend, "A")
        connect(inside, blend, "B")
        connect(scope, blend, "Alpha")
        if pin == "Normal":
            normalized = node(material, unreal.MaterialExpressionNormalize, -850, -800 + index * 250)
            connect(blend, normalized, "")
            blend = normalized
        connect(blend, slab, pin)
    LIB.recompile_material(material)
    for asset in (material, instance):
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    OUT.mkdir(parents=True, exist_ok=True)
    report = {"status": "assets generated; compile and WIP validation pending", "source": SOURCE,
              "material": material.get_path_name(), "instance": instance.get_path_name(),
              "source_node_count": len(expressions), "layers": ["BeachCliff", "MossyRockyGround"],
              "ground_tile_cm": 400, "notes": ["Source color/roughness adjustments reused; roughness consumes G.",
              "Slope blend uses the 30/70 degree diagnostic thresholds for this first trial.",
              "Other source branches remain in the duplicated asset but do not feed the slab."]}
    (OUT / "Creation.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("Ridge surface material assets created. Apply only to the WIP terrain after compile verification.")


if __name__ == "__main__":
    build()
