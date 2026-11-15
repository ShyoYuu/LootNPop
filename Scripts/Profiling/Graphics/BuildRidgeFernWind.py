"""1-B4d Fern 바람의 기하 계산을 구면 접선 프레임으로 옮긴 파생본3개를 만든다.

콘솔: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/BuildRidgeFernWind.py"
원본/공유 함수/MPC·맵·액터는 수정하지 않는다. 적용/회전 관찰은 생성·컴파일 확인 뒤 진행한다.
"""
import json
from pathlib import Path
import shutil
import sys

import unreal
from editor_toolset.toolsets.material import MaterialTools

sys.path.insert(0, str(Path(__file__).resolve().parent))
from BuildRidgeGroundProbe import ROOT, LEVEL, snapshot
from BuildRidgeSurfaceProbe import custom, node
from BuildRidgeSurfaceMaterial import connect

BASE = "/Game/Graphics/Environment/ElectricDreams/RidgeFernWind_V001"
SOURCE = "/Game/Custom/CustomMaterials/Materials/Master_Materials/Custom_MS_Master/M_MS_Foliage_Overwrite/M_MS_CustomFoliage"
SOURCE_MI = "/Game/Megascans/3D_Plants/Fern/MI_Fern_01"
SOURCE_WIND = "/Game/MSPresets/MS_Foliage_Material_LATEST/MaterialFunctions/Foliage/MF_SimpleWind"
OUT = ROOT / "Saved/Profiling/Graphics/Phase01/RidgeFernWind_20261115"
LIB = unreal.MaterialEditingLibrary
# Z는 Up이 아니라 행성의 공통 방향 기준이다. 극점 가까이에서는 X 기준으로 바꾼다.
FRAME_CODE = """
float3 frameUp = -normalize(PivotWS);
float3 referenceAxis = abs(frameUp.z) < 0.95 ? float3(0,0,1) : float3(1,0,0);
float3 frameX = normalize(cross(referenceAxis, frameUp));
float3 frameY = cross(frameUp, frameX);
"""
POSITION_CODE = FRAME_CODE + """
float3 relativePosition = PositionWS - PivotWS;
return float3(dot(relativePosition, frameX), dot(relativePosition, frameY), dot(relativePosition, frameUp));
"""
OFFSET_CODE = FRAME_CODE + """
return OffsetFrame.x * frameX + OffsetFrame.y * frameY + OffsetFrame.z * frameUp;
"""


def build():
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first."
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name().split(".")[0] == LEVEL, "Open the existing WIP level first."
    assert not (list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) +
                list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())), "Save existing changes first."
    assert not unreal.EditorAssetLibrary.list_assets(BASE, recursive=True, include_folder=False), "Version already exists."
    actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
    meshes = [a for a in actors if isinstance(a, unreal.StaticMeshActor)]
    assert len(meshes) == 15, "Expected terrain and completed ground/rock/fern probes."
    ferns = [a for a in meshes if a.get_actor_label() == "GraphicsFernProbe_Original_00"]
    assert len(ferns) == 1
    assert ferns[0].static_mesh_component.get_material(0).get_path_name().split(".")[0] == SOURCE_MI
    before = snapshot(meshes)
    OUT.mkdir(parents=True, exist_ok=True)
    backup = OUT / "BeforeMaterialApplication.umap"
    assert not backup.exists(), "Recovery point already exists; inspect before retrying."
    shutil.copy2(ROOT / ("Content" + LEVEL[5:] + ".umap"), backup)
    (OUT / "WipBefore.json").write_text(json.dumps(before, indent=2), encoding="utf-8")

    wind = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_WIND, BASE + "/MF_Ridge_FernWind")
    assert wind
    expressions = {e.get_name(): e for e in MaterialTools.get_expressions(wind)}
    assert len(expressions) == 71, "Unexpected source wind graph version."
    rotate = expressions["MaterialExpressionRotateAboutAxis_2"]
    position = expressions["MaterialExpressionWorldPosition_4"]
    pivot = expressions["MaterialExpressionMaterialFunctionCall_10"]
    tree = expressions["MaterialExpressionFunctionOutput_1"]
    incoming = {str(p.input_name): p for p in MaterialTools.get_expression_inputs(wind, rotate)}
    assert incoming["Position"].expression == position and incoming["PivotPoint"].expression == pivot
    assert str(tree.get_editor_property("output_name")) == "WPO Tree (V3)"
    output = MaterialTools.get_expression_inputs(wind, tree)[0]
    assert output.expression == expressions["MaterialExpressionCustom_3"]
    position.set_editor_property("world_position_shader_offset",
                                unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    frame_position = custom(wind, POSITION_CODE, ("PositionWS", "PivotWS"),
                            unreal.CustomMaterialOutputType.CMOT_FLOAT3, -2000, -1800)
    frame_position.set_editor_property("desc", "구면 접선 프레임의 피벗 기준 위치")
    connect(position, frame_position, "PositionWS", "XYZ")
    connect(pivot, frame_position, "PivotWS", "Mesh Particle Pivot Location")
    connect(frame_position, rotate, "Position")
    zero = node(wind, unreal.MaterialExpressionConstant3Vector, -2000, -1500,
                constant=unreal.LinearColor(0, 0, 0, 0))
    connect(zero, rotate, "PivotPoint")
    world_offset = custom(wind, OFFSET_CODE, ("OffsetFrame", "PivotWS"),
                          unreal.CustomMaterialOutputType.CMOT_FLOAT3, 1200, -1800)
    world_offset.set_editor_property("desc", "접선 프레임의 변위 벡터를 월드 공간으로 복귀")
    connect(output.expression, world_offset, "OffsetFrame", str(output.output_name))
    connect(pivot, world_offset, "PivotWS", "Mesh Particle Pivot Location")
    connect(world_offset, tree, "")
    LIB.update_material_function(wind)

    material = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, BASE + "/M_Ridge_Fern")
    instance = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MI, BASE + "/MI_Ridge_Fern")
    assert material and instance
    calls = [e for e in LIB.get_material_expressions(material)
             if isinstance(e, unreal.MaterialExpressionMaterialFunctionCall) and
             e.get_editor_property("material_function") == unreal.load_asset(SOURCE_WIND)]
    assert len(calls) == 1
    assert calls[0].set_material_function(wind)
    errors = LIB.recompile_material(material)
    assert not errors, errors
    LIB.set_material_instance_parent(instance, material)
    LIB.update_material_instance(instance)
    stats = LIB.get_statistics(instance)
    assert stats.get_editor_property("num_vertex_shader_instructions") > 0
    assert stats.get_editor_property("num_pixel_shader_instructions") > 0
    for asset in (wind, material, instance):
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    assert before == snapshot(meshes), "Existing actors changed."
    report = {"status": "assets generated; compile verification and WIP application pending",
              "assets": [a.get_path_name() for a in (wind, material, instance)],
              "original_wind_nodes": len(expressions), "added_nodes": 3,
              "position_code": POSITION_CODE, "offset_code": OFFSET_CODE,
              "existing_actors_preserved": True,
              "notes": ["Original scalar world-space gust/noise phase and MI parameters preserved.",
                        "Canonical geometric wind uses radial Up at the primitive pivot.",
                        "Planet Z is a direction reference; X is used near its poles.",
                        "Only the Tree output used by this Fern parent is adapted.",
                        "Ray-traced branch, original assets, MPC, mesh, and actors are unchanged."]}
    (OUT / "Creation.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("Ridge fern wind assets saved; compile and WIP application verification pending.")


if __name__ == "__main__":
    build()
