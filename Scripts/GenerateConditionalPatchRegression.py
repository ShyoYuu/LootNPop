"""Phase 8의 영구 동적 회귀 옥탄트를 만든다. 기존 정적 oracle은 보존한다."""
import math

import unreal


FOLDER = "/Game/Maps/SurfaceNavigation/Fixtures"
SOURCE = FOLDER + "/LVI_Octant_Fixture_Regression"
LEVEL = FOLDER + "/LVI_Octant_Fixture_Conditional"
MAP = "/Game/Maps/SurfaceNavigation/L_ConditionalPatchRegression"


def frame(latitude, azimuth, radius, tangent_offset=0.0):
    lat, az = math.radians(latitude), math.radians(azimuth)
    radial = unreal.Vector(math.cos(lat) * math.cos(az), math.cos(lat) * math.sin(az), math.sin(lat))
    tangent = unreal.Vector(-math.sin(az), math.cos(az), 0.0)
    location = radial * radius + tangent * tangent_offset
    rotation = unreal.MathLibrary.make_rot_from_zy(radial * -1.0, tangent)
    return location, rotation


def place_playtest_starts(actors):
    # 생성된 게임 맵에서 바로 회전 운반을 확인한다. 캡슐은 보행면 위 120cm에서 낙하한다.
    location, rotation = frame(32.0, 18.0, 28150.0, 1150.0)
    base = unreal.Transform(location=location, rotation=rotation)
    standing = unreal.Transform(location=unreal.Vector(0.0, -500.0, 500.0),
                                rotation=unreal.Rotator(roll=30.0))
    starts = [actor for actor in actors.get_all_level_actors() if isinstance(actor, unreal.PlayerStart)]
    for index, actor in enumerate(starts):
        offset = (index - (len(starts) - 1) * 0.5) * 200.0
        point = standing.transform_location(unreal.Vector(offset, 0.0, 150.0))
        point = base.transform_location(point + unreal.Vector(0.0, 0.0, 120.0))
        actor.modify()
        actor.set_actor_location_and_rotation(point, rotation, False, True)
    # WP의 기존 Actor는 레벨 저장만으로 외부 패키지가 저장되지 않는다.
    packages = [actor.get_package() for actor in starts]
    if packages and not unreal.EditorLoadingAndSavingUtils.save_packages(packages, False):
        raise RuntimeError("Failed to save PlayerStart external packages.")


def mesh_actor(actors, mesh, label, location, rotation, scale, support):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
    actor.set_actor_label(label)
    actor.set_folder_path("ConditionalRegression")
    actor.set_actor_scale3d(scale)
    component = actor.get_editor_property("static_mesh_component")
    component.set_static_mesh(mesh)
    component.set_collision_profile_name("LNPStaticTerrain" if support else "LNPStaticBlocker")
    tags = ["LNP.Surface.Blocker", "LNP.Surface.Static"]
    if support:
        tags.append("LNP.Surface.Support")
    component.set_editor_property("component_tags", [unreal.Name(tag) for tag in tags])
    return actor


def source(mesh, scale, support, translation=unreal.Vector()):
    result = unreal.LNPConditionalPatchSource()
    result.set_editor_property("mesh", mesh)
    result.set_editor_property("transform", unreal.Transform(location=translation, scale=scale))
    result.set_editor_property("support", support)
    return result


def marker(actors, label, location, rotation, condition, sources):
    actor = actors.spawn_actor_from_class(unreal.LNPPlacementMarker, location, rotation)
    actor.set_actor_label(label)
    actor.set_folder_path("ConditionalRegression")
    actor.set_editor_property("element_class", unreal.LNPStatefulTerrain)
    actor.set_editor_property("patch_condition", condition)
    actor.set_editor_property("patch_sources", sources)
    return actor


def build():
    assets = unreal.EditorAssetLibrary
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    # 재실행은 저장된 authoring을 덮어쓰지 않는다. 수정을 원하면 기존 마커를 직접 편집한다.
    if assets.does_asset_exist(LEVEL):
        raise RuntimeError("Conditional regression already exists; refusing to overwrite authoring.")
    if not assets.does_asset_exist(MAP):
        if not levels.new_level_from_template(MAP, "/Game/Maps/TestMap03"):
            raise RuntimeError("Failed to create gameplay regression map.")
        place_playtest_starts(actors)
        if not levels.save_current_level():
            raise RuntimeError("Failed to save playtest starts.")
    # UWorld를 일반 에셋 복제 후 LoadLevel로 열면 standalone 복제본이 GC에 남는다.
    # 레벨 전용 template API가 월드 교체·복제·저장을 한 번에 소유하게 한다.
    if not levels.new_level_from_template(LEVEL, SOURCE):
        raise RuntimeError("Failed to create conditional fixture from template.")
    top = unreal.load_asset(FOLDER + "/SM_FixtureBoxTop")
    body = unreal.load_asset(FOLDER + "/SM_FixtureBoxBody")
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    if not all((top, body, cube)):
        raise RuntimeError("Missing fixture meshes.")

    # 기존 섬의 끝과 새 독립 섬 사이를 patch가 잇는다. top은 단위 box의 +Z 면(z=50)이다.
    for label, lat, az, radius, center, width, endpoint, condition in (
        ("CP_Bridge", 32.0, 18.0, 28000.0, 1150.0, 1000.0, 2150.0, unreal.LNPConditionalPatchCondition.BRIDGE),
        ("CP_Floor", 32.0, 30.0, 28200.0, 1225.0, 1050.0, 2250.0, unreal.LNPConditionalPatchCondition.INTACT),
    ):
        location, rotation = frame(lat, az, radius + 150.0, endpoint)
        for mesh, suffix, support in ((top, "Top", True), (body, "Body", False)):
            mesh_actor(actors, mesh, label + "Endpoint_" + suffix, location, rotation,
                       unreal.Vector(10.0, 10.0, 3.0), support)
        location, rotation = frame(lat, az, radius + 150.0, center)
        sources = [source(top, unreal.Vector(10.0, width / 100.0, 3.0), True),
                   source(body, unreal.Vector(10.0, width / 100.0, 3.0), False)]
        placed = marker(actors, label, location, rotation, condition, sources)
        if condition == unreal.LNPConditionalPatchCondition.BRIDGE:
            placed.set_editor_property("standing_transform", unreal.Transform(
                location=unreal.Vector(0.0, -500.0, 500.0), rotation=unreal.Rotator(roll=30.0)))
            placed.set_editor_property("transition_seconds", 8.0)

    location, rotation = frame(30.0, 8.0, 30000.0)
    marker(actors, "CP_Blocker", location, rotation, unreal.LNPConditionalPatchCondition.INTACT,
           [source(cube, unreal.Vector(3.0, 3.0, 3.0), False, unreal.Vector(0.0, 0.0, 150.0))])
    location, rotation = frame(32.0, 42.0, 27000.0)
    panel = actors.spawn_actor_from_class(unreal.LNPPlacementMarker, location, rotation)
    panel.set_actor_label("CP_MovingPanel")
    panel.set_folder_path("ConditionalRegression")
    panel.set_editor_property("element_class", unreal.LNPMovingPanel)
    path = panel.get_editor_property("path")
    path.set_location_at_spline_point(0, unreal.Vector(0.0, -800.0, 0.0), unreal.SplineCoordinateSpace.LOCAL)
    path.set_location_at_spline_point(1, unreal.Vector(0.0, 800.0, 0.0), unreal.SplineCoordinateSpace.LOCAL)
    panel.set_editor_property("path_speed", 200.0)
    if not levels.save_current_level():
        raise RuntimeError("Failed to save conditional fixture.")
    unreal.log("[ConditionalRegression] Authoring saved: " + LEVEL)
    unreal.log("[ConditionalRegression] Bake with LNP.SurfaceNav.BakeOctant " + LEVEL)


if __name__ == "__main__":
    build()
