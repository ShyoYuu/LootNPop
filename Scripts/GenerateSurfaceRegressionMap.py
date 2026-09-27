import math
import os

import unreal


MAP_PATH = "/Game/Maps/SurfaceNavigation/L_SurfaceRegression"
GROUND_RADIUS = 30000.0
# 동적 사례만 남는다. 정적 사례는 LVI_Octant_Fixture_Regression(LNP.SurfaceNav.BuildRegressionFixture)이 소유한다.
# 옥탄트 내부(위도 25~40°)에 두어 이음매·꼭짓점에서 떨어뜨린다. (위도, 방위) 도.
CASE_DIRECTIONS = {
    "StatefulPillar": (30.0, 20.0),
    "MovingPanel": (30.0, 45.0),
    "DestructibleFloor": (30.0, 70.0),
}

TAG_SUPPORT = "LNP.Surface.Support"
TAG_BLOCKER = "LNP.Surface.Blocker"
TAG_STATIC = "LNP.Surface.Static"
TAG_DYNAMIC = "LNP.Surface.Dynamic"
TAG_STATEFUL = "LNP.Surface.StatefulTraversal"
TAG_DESTRUCTIBLE = "LNP.Surface.Destructible"
TAG_DECORATION = "LNP.Surface.Decoration"


def radial_basis(direction):
    lat = math.radians(direction[0])
    az = math.radians(direction[1])
    radial = unreal.Vector(math.cos(lat) * math.cos(az), math.cos(lat) * math.sin(az), math.sin(lat))
    tangent = unreal.Vector(-math.sin(az), math.cos(az), 0.0)
    return radial, tangent


def add_vectors(*vectors):
    return unreal.Vector(
        sum(vector.x for vector in vectors),
        sum(vector.y for vector in vectors),
        sum(vector.z for vector in vectors),
    )


def scaled(vector, value):
    return unreal.Vector(vector.x * value, vector.y * value, vector.z * value)


def case_tag(case_name):
    return "LNP.Regression.Case." + case_name


def spawn_mesh(
    actor_subsystem,
    mesh,
    label,
    case_name,
    direction,
    radius,
    dimensions,
    profile,
    semantic_tags,
    tangent_offset=0.0,
    movable=False,
):
    radial, tangent = radial_basis(direction)
    location = add_vectors(scaled(radial, radius), scaled(tangent, tangent_offset))
    # 로컬 Z = 방사, 로컬 Y = 방위 접선, 로컬 X = 위도 접선.
    rotation = unreal.MathLibrary.make_rot_from_zy(radial, tangent)
    actor = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
    if not actor:
        raise RuntimeError("Failed to spawn " + label)

    actor.set_actor_label(label)
    actor.set_folder_path(unreal.Name("SurfaceRegression/" + case_name))
    actor.set_editor_property(
        "tags",
        [unreal.Name(case_tag(case_name)), unreal.Name("LNP.Regression.Fixture")],
    )

    component = actor.get_editor_property("static_mesh_component")
    component.set_static_mesh(mesh)
    component.set_collision_profile_name(unreal.Name(profile))
    component.set_editor_property(
        "component_tags",
        [unreal.Name(tag) for tag in semantic_tags] + [unreal.Name(case_tag(case_name))],
    )
    if movable:
        component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)

    radial_size, tangent_size, latitude_size = dimensions
    actor.set_actor_scale3d(
        unreal.Vector(latitude_size / 100.0, tangent_size / 100.0, radial_size / 100.0)
    )
    return actor


def spawn_probe(actor_subsystem, sphere_mesh, case_name, radius):
    return spawn_mesh(
        actor_subsystem,
        sphere_mesh,
        "Probe_" + case_name,
        case_name,
        CASE_DIRECTIONS[case_name],
        radius,
        (30.0, 30.0, 30.0),
        "LNPDecoration",
        [TAG_DECORATION],
    )


def validate_map(actor_subsystem):
    expected_profile_tags = {
        "LNPStaticTerrain": {TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC},
        "LNPStaticBlocker": {TAG_BLOCKER, TAG_STATIC},
        "LNPDynamicTerrain": {TAG_SUPPORT, TAG_BLOCKER, TAG_DYNAMIC},
        "LNPStatefulTraversal": {TAG_BLOCKER, TAG_STATEFUL},
        "LNPDestructibleTerrain": {TAG_SUPPORT, TAG_BLOCKER, TAG_DESTRUCTIBLE},
        "LNPDecoration": {TAG_DECORATION},
    }
    expected_profile_counts = {
        "LNPStaticTerrain": 6,
        "LNPStaticBlocker": 0,
        "LNPDynamicTerrain": 1,
        "LNPStatefulTraversal": 1,
        "LNPDestructibleTerrain": 1,
        "LNPDecoration": 3,
    }

    actors = actor_subsystem.get_all_level_actors()
    fixtures = [
        actor
        for actor in actors
        if unreal.Name("LNP.Regression.Fixture") in actor.get_editor_property("tags")
    ]
    if len(fixtures) != 12:
        raise RuntimeError("Unexpected fixture actor count: " + str(len(fixtures)))

    profile_counts = {profile: 0 for profile in expected_profile_counts}
    found_cases = set()
    for actor in fixtures:
        component = actor.get_editor_property("static_mesh_component")
        profile = str(component.get_collision_profile_name())
        if profile not in expected_profile_tags:
            raise RuntimeError(actor.get_actor_label() + " has unexpected profile " + profile)

        component_tags = {str(tag) for tag in component.get_editor_property("component_tags")}
        if not expected_profile_tags[profile].issubset(component_tags):
            raise RuntimeError(actor.get_actor_label() + " has tags inconsistent with " + profile)
        profile_counts[profile] += 1

        actor_tags = {str(tag) for tag in actor.get_editor_property("tags")}
        case_tags = [tag for tag in actor_tags if tag.startswith("LNP.Regression.Case.")]
        if len(case_tags) != 1:
            raise RuntimeError(actor.get_actor_label() + " must have exactly one case tag")
        found_cases.add(case_tags[0])

    if profile_counts != expected_profile_counts:
        raise RuntimeError("Unexpected profile counts: " + str(profile_counts))

    expected_cases = {case_tag(case_name) for case_name in CASE_DIRECTIONS}
    if found_cases != expected_cases:
        raise RuntimeError("Regression case tags do not match the contract")

    unreal.log("LNP surface regression map validated: " + MAP_PATH)
    unreal.log("LNP surface regression fixture actors: " + str(len(fixtures)))


def build_map():
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not level_subsystem.load_level(MAP_PATH):
            raise RuntimeError("Failed to load existing regression map")
        if os.environ.get("LNP_REBUILD_SURFACE_REGRESSION_MAP") != "1":
            validate_map(actor_subsystem)
            return
        if not actor_subsystem.destroy_actors(actor_subsystem.get_all_level_actors()):
            raise RuntimeError("Failed to clear existing regression map")
    else:
        if not level_subsystem.new_level(MAP_PATH):
            raise RuntimeError("Failed to create regression map")

    cube_mesh = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    sphere_mesh = unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere")
    cylinder_mesh = unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")
    if not cube_mesh or not sphere_mesh or not cylinder_mesh:
        raise RuntimeError("Failed to load Engine basic shape meshes")

    # 지면 판은 반지름 R+50 중심, 두께 100이라 안쪽 면이 R에 온다.
    ground = GROUND_RADIUS + 50.0
    terrain_tags = [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC]

    direction = CASE_DIRECTIONS["StatefulPillar"]
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_07_GroundLeft", "StatefulPillar", direction, ground,
               (100.0, 1100.0, 1500.0), "LNPStaticTerrain", terrain_tags, tangent_offset=-800.0)
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_07_GroundRight", "StatefulPillar", direction, ground,
               (100.0, 1100.0, 1500.0), "LNPStaticTerrain", terrain_tags, tangent_offset=800.0)
    spawn_mesh(actor_subsystem, cylinder_mesh, "SSN_07_StatefulPillar", "StatefulPillar", direction, GROUND_RADIUS - 500.0,
               (1000.0, 180.0, 180.0), "LNPStatefulTraversal", [TAG_BLOCKER, TAG_STATEFUL], movable=True)
    spawn_probe(actor_subsystem, sphere_mesh, "StatefulPillar", GROUND_RADIUS - 800.0)

    direction = CASE_DIRECTIONS["MovingPanel"]
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_08_GroundLeft", "MovingPanel", direction, ground,
               (100.0, 1100.0, 1500.0), "LNPStaticTerrain", terrain_tags, tangent_offset=-800.0)
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_08_GroundRight", "MovingPanel", direction, ground,
               (100.0, 1100.0, 1500.0), "LNPStaticTerrain", terrain_tags, tangent_offset=800.0)
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_08_MovingPanel", "MovingPanel", direction, GROUND_RADIUS - 750.0,
               (100.0, 800.0, 800.0), "LNPDynamicTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_DYNAMIC], movable=True)
    spawn_probe(actor_subsystem, sphere_mesh, "MovingPanel", GROUND_RADIUS - 1200.0)

    direction = CASE_DIRECTIONS["DestructibleFloor"]
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_09_GroundLeft", "DestructibleFloor", direction, ground,
               (100.0, 1200.0, 1500.0), "LNPStaticTerrain", terrain_tags, tangent_offset=-950.0)
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_09_GroundRight", "DestructibleFloor", direction, ground,
               (100.0, 1200.0, 1500.0), "LNPStaticTerrain", terrain_tags, tangent_offset=950.0)
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_09_DestructibleFloor", "DestructibleFloor", direction, ground,
               (100.0, 700.0, 1500.0), "LNPDestructibleTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_DESTRUCTIBLE], movable=True)
    spawn_probe(actor_subsystem, sphere_mesh, "DestructibleFloor", GROUND_RADIUS - 500.0)

    light = actor_subsystem.spawn_actor_from_class(
        unreal.DirectionalLight,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(pitch=-35.0, yaw=-45.0, roll=0.0),
    )
    light.set_actor_label("SSN_Lighting_Directional")
    light.set_folder_path(unreal.Name("SurfaceRegression/Lighting"))

    skylight = actor_subsystem.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(), unreal.Rotator())
    skylight.set_actor_label("SSN_Lighting_Sky")
    skylight.set_folder_path(unreal.Name("SurfaceRegression/Lighting"))

    if not level_subsystem.save_current_level():
        raise RuntimeError("Failed to save regression map")

    unreal.log("LNP surface regression map generated: " + MAP_PATH)
    validate_map(actor_subsystem)


build_map()
