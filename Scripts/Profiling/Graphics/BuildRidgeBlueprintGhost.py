"""정식 Ridge V011을 대조하고 새 작업 레벨에 조립용 고스트를 만든다. 언리얼 에디터 Python에서 실행한다."""
from pathlib import Path
import sys

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from DrawRidgeDecorationKeepOuts import RULES_BR, draw
from ExportRidgeBlueprint import SOURCE_LEVEL, spec, verify

FOLDER = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001"
LEVEL = FOLDER + "/LVI_Ridge_EDTerrain_V001"
MATERIAL = FOLDER + "/M_Ridge_V011_Ghost"
SOURCE = SOURCE_LEVEL.rsplit("/", 1)[0]
MESHES = {
    "Crust": SOURCE + "/SM_Octant_Ridge_00_V011_R30000",
    "Rocks_Floor": SOURCE + "/Authored/SM_Ridge_00_V011_Rocks_Floor",
    "Rocks_Shell": SOURCE + "/Authored/SM_Ridge_00_V011_Rocks_Shell",
}


def ghost_material():
    assets = unreal.EditorAssetLibrary
    if assets.does_asset_exist(MATERIAL):
        return assets.load_asset(MATERIAL)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MATERIAL.rsplit("/", 1)[1], FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    assert material, MATERIAL
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    edit = unreal.MaterialEditingLibrary
    tint = edit.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, 0)
    tint.set_editor_property("constant", unreal.LinearColor(0.18, 0.5, 0.8, 1.0))
    fresnel = edit.create_material_expression(material, unreal.MaterialExpressionFresnel, -600, 160)
    fresnel.set_editor_property("base_reflect_fraction", 0.25)
    opacity = edit.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, 160)
    opacity.set_editor_property("const_b", 0.6)
    assert edit.connect_material_expressions(fresnel, "", opacity, "A")
    assert edit.connect_material_property(tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert edit.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    edit.recompile_material(material)
    assert assets.save_loaded_asset(material)
    return material


def place_ghosts(actors, meshes, material):
    for label, mesh in meshes.items():
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(), unreal.Rotator())
        actor.set_actor_label(label)
        actor.set_folder_path("Blueprint/V011_Ghost")
        actor.set_actor_hidden_in_game(True)
        actor.set_editor_property("is_editor_only_actor", True)
        component = actor.static_mesh_component
        component.set_static_mesh(mesh)
        # 반투명 재질은 Nanite 비지원이므로 이 컴포넌트에서만 fallback을 쓴다.
        component.set_editor_property("disallow_nanite", True)
        component.set_editor_property("component_tags", ["LNP.Surface.Decoration"])
        component.set_editor_property("use_default_collision", False)
        component.set_collision_profile_name("LNPDecoration")
        component.set_cast_shadow(False)
        component.set_mobility(unreal.ComponentMobility.STATIC)
        for slot in range(component.get_num_materials()):
            component.set_material(slot, material)


def verify_ghosts(level_actors):
    ghosts = {actor.get_actor_label(): actor for actor in level_actors
              if isinstance(actor, unreal.StaticMeshActor) and actor.get_actor_label().startswith("Ghost_V011_")}
    assert ghosts.keys() == {"Ghost_V011_" + label for label in MESHES}, "Expected three V011 ghost actors."
    for label, path in MESHES.items():
        actor = ghosts["Ghost_V011_" + label]
        component = actor.static_mesh_component
        assert actor.get_editor_property("hidden") and actor.get_editor_property("is_editor_only_actor"), label
        assert component.static_mesh.get_path_name().split(".")[0] == path, label
        assert [str(tag) for tag in component.get_editor_property("component_tags")] == ["LNP.Surface.Decoration"], label
        assert str(component.get_collision_profile_name()) == "LNPDecoration", label
        assert component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION, label
        for slot in range(component.get_num_materials()):
            assert component.get_material(slot).get_path_name().split(".")[0] == MATERIAL, label


def run():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    saving = unreal.EditorLoadingAndSavingUtils
    assert not levels.is_in_play_in_editor(), "Stop PIE first."
    dirty = list(saving.get_dirty_map_packages()) + list(saving.get_dirty_content_packages())
    assert not dirty, ("Review existing unsaved changes before switching maps.", [p.get_path_name() for p in dirty])
    assets = unreal.EditorAssetLibrary
    assert assets.does_asset_exist(RULES_BR), RULES_BR
    if not assets.does_asset_exist(LEVEL):
        assert levels.load_level(SOURCE_LEVEL), SOURCE_LEVEL
        verify()
        assert levels.new_level(LEVEL), LEVEL
    elif editor.get_editor_world().get_path_name().split(".")[0] != LEVEL:
        assert levels.load_level(LEVEL), LEVEL

    # new_level이 먼저 저장한 빈 맵도 같은 경로로 이어서 조립할 수 있다.
    empty_level = not actors.get_all_level_actors()
    if empty_level:
        meshes = {"Ghost_V011_" + label: assets.load_asset(path) for label, path in MESHES.items()}
        assert all(meshes.values()), MESHES
        place_ghosts(actors, meshes, ghost_material())
    verify_ghosts(actors.get_all_level_actors())
    eye, target = (unreal.Vector(*spec.point(*pos)) for pos in spec.VIEWS[0][1:3])
    rotation = unreal.MathLibrary.make_rot_from_xz(target - eye, -target)
    editor.set_level_viewport_camera_info(eye, rotation)
    if empty_level:
        assert levels.save_current_level(), LEVEL
    draw(full_ridge=True)
    unreal.log("RIDGE_BLUEPRINT_GHOST_READY " + LEVEL)


if __name__ == "__main__":
    run()
