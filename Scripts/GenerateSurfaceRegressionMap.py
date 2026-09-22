import math
import os

import unreal


MAP_PATH = "/Game/Maps/SurfaceNavigation/L_SurfaceRegression"
GROUND_RADIUS = 25000.0
CASE_ANGLES = {
    "BasicCrust": 5.0,
    "FloatingIslandOne": 13.0,
    "FloatingIslandTwo": 21.0,
    "IslandEdge": 29.0,
    "SimpleCave": 37.0,
    "StaticProps": 45.0,
    "StatefulPillar": 53.0,
    "MovingPanel": 61.0,
    "DestructibleFloor": 69.0,
    "OctantSeam": 90.0,
}

TAG_SUPPORT = "LNP.Surface.Support"
TAG_BLOCKER = "LNP.Surface.Blocker"
TAG_STATIC = "LNP.Surface.Static"
TAG_DYNAMIC = "LNP.Surface.Dynamic"
TAG_STATEFUL = "LNP.Surface.StatefulTraversal"
TAG_DESTRUCTIBLE = "LNP.Surface.Destructible"
TAG_DECORATION = "LNP.Surface.Decoration"


def radial_basis(angle_degrees):
    radians = math.radians(angle_degrees)
    radial = unreal.Vector(math.cos(radians), math.sin(radians), 0.0)
    tangent = unreal.Vector(-math.sin(radians), math.cos(radians), 0.0)
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
    angle,
    radius,
    dimensions,
    profile,
    semantic_tags,
    tangent_offset=0.0,
    vertical_offset=0.0,
    movable=False,
):
    radial, tangent = radial_basis(angle)
    location = add_vectors(
        scaled(radial, radius),
        scaled(tangent, tangent_offset),
        unreal.Vector(0.0, 0.0, vertical_offset),
    )
    rotation = unreal.Rotator(pitch=90.0, yaw=angle, roll=0.0)
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

    radial_size, tangent_size, vertical_size = dimensions
    actor.set_actor_scale3d(
        unreal.Vector(vertical_size / 100.0, tangent_size / 100.0, radial_size / 100.0)
    )
    return actor


def spawn_probe(actor_subsystem, sphere_mesh, case_name, angle, radius, tangent_offset=0.0, vertical_offset=0.0):
    return spawn_mesh(
        actor_subsystem,
        sphere_mesh,
        "Probe_" + case_name,
        case_name,
        angle,
        radius,
        (30.0, 30.0, 30.0),
        "LNPDecoration",
        [TAG_DECORATION],
        tangent_offset,
        vertical_offset,
        False,
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
        "LNPStaticTerrain": 17,
        "LNPStaticBlocker": 5,
        "LNPDynamicTerrain": 1,
        "LNPStatefulTraversal": 1,
        "LNPDestructibleTerrain": 1,
        "LNPDecoration": 12,
    }

    actors = actor_subsystem.get_all_level_actors()
    fixtures = [
        actor
        for actor in actors
        if unreal.Name("LNP.Regression.Fixture") in actor.get_editor_property("tags")
    ]
    if len(fixtures) != 37:
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

    expected_cases = {case_tag(case_name) for case_name in CASE_ANGLES}
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

    angle = CASE_ANGLES["BasicCrust"]
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_01_BasicCrust", "BasicCrust", angle, 25050.0,
               (100.0, 1800.0, 1800.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC])
    spawn_probe(actor_subsystem, sphere_mesh, "BasicCrust", angle, 24500.0)

    angle = CASE_ANGLES["FloatingIslandOne"]
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_02_Crust", "FloatingIslandOne", angle, 25050.0,
               (100.0, 1800.0, 1800.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC])
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_02_Island", "FloatingIslandOne", angle, 23050.0,
               (100.0, 1300.0, 1300.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC])
    spawn_probe(actor_subsystem, sphere_mesh, "FloatingIslandOne", angle, 22500.0)

    angle = CASE_ANGLES["FloatingIslandTwo"]
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_03_Crust", "FloatingIslandTwo", angle, 25050.0,
               (100.0, 1800.0, 1800.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC])
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_03_IslandOuter", "FloatingIslandTwo", angle, 23250.0,
               (100.0, 1400.0, 1400.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC])
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_03_IslandInner", "FloatingIslandTwo", angle, 21450.0,
               (100.0, 1100.0, 1100.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC])
    spawn_probe(actor_subsystem, sphere_mesh, "FloatingIslandTwo", angle, 20800.0)

    angle = CASE_ANGLES["IslandEdge"]
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_04_EdgeIsland", "IslandEdge", angle, 23050.0,
               (100.0, 1200.0, 1600.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC])
    spawn_probe(actor_subsystem, sphere_mesh, "IslandEdge", angle, 22500.0, tangent_offset=550.0)
    spawn_probe(actor_subsystem, sphere_mesh, "IslandEdge", angle, 22500.0, tangent_offset=750.0)

    angle = CASE_ANGLES["SimpleCave"]
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_05_CaveFloor", "SimpleCave", angle, 25050.0,
               (100.0, 1800.0, 1400.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC])
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_05_CaveCeiling", "SimpleCave", angle, 24150.0,
               (100.0, 1800.0, 1400.0), "LNPStaticBlocker", [TAG_BLOCKER, TAG_STATIC])
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_05_CaveWallLeft", "SimpleCave", angle, 24600.0,
               (900.0, 100.0, 1400.0), "LNPStaticBlocker", [TAG_BLOCKER, TAG_STATIC], tangent_offset=-850.0)
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_05_CaveWallRight", "SimpleCave", angle, 24600.0,
               (900.0, 100.0, 1400.0), "LNPStaticBlocker", [TAG_BLOCKER, TAG_STATIC], tangent_offset=850.0)
    spawn_probe(actor_subsystem, sphere_mesh, "SimpleCave", angle, 24600.0)

    angle = CASE_ANGLES["StaticProps"]
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_06_PropGround", "StaticProps", angle, 25050.0,
               (100.0, 2000.0, 1600.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC])
    spawn_mesh(actor_subsystem, cylinder_mesh, "SSN_06_TreeBlocker", "StaticProps", angle, 24650.0,
               (700.0, 180.0, 180.0), "LNPStaticBlocker", [TAG_BLOCKER, TAG_STATIC], tangent_offset=-350.0)
    spawn_mesh(actor_subsystem, sphere_mesh, "SSN_06_RockBlocker", "StaticProps", angle, 24800.0,
               (400.0, 450.0, 450.0), "LNPStaticBlocker", [TAG_BLOCKER, TAG_STATIC], tangent_offset=350.0)
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_06_Decoration", "StaticProps", angle, 24600.0,
               (200.0, 250.0, 500.0), "LNPDecoration", [TAG_DECORATION], tangent_offset=700.0)
    spawn_probe(actor_subsystem, sphere_mesh, "StaticProps", angle, 24400.0)

    angle = CASE_ANGLES["StatefulPillar"]
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_07_GroundLeft", "StatefulPillar", angle, 25050.0,
               (100.0, 1100.0, 1500.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC], tangent_offset=-800.0)
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_07_GroundRight", "StatefulPillar", angle, 25050.0,
               (100.0, 1100.0, 1500.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC], tangent_offset=800.0)
    spawn_mesh(actor_subsystem, cylinder_mesh, "SSN_07_StatefulPillar", "StatefulPillar", angle, 24500.0,
               (1000.0, 180.0, 180.0), "LNPStatefulTraversal", [TAG_BLOCKER, TAG_STATEFUL], movable=True)
    spawn_probe(actor_subsystem, sphere_mesh, "StatefulPillar", angle, 24200.0)

    angle = CASE_ANGLES["MovingPanel"]
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_08_GroundLeft", "MovingPanel", angle, 25050.0,
               (100.0, 1100.0, 1500.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC], tangent_offset=-800.0)
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_08_GroundRight", "MovingPanel", angle, 25050.0,
               (100.0, 1100.0, 1500.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC], tangent_offset=800.0)
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_08_MovingPanel", "MovingPanel", angle, 24250.0,
               (100.0, 800.0, 800.0), "LNPDynamicTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_DYNAMIC], movable=True)
    spawn_probe(actor_subsystem, sphere_mesh, "MovingPanel", angle, 23800.0)

    angle = CASE_ANGLES["DestructibleFloor"]
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_09_GroundLeft", "DestructibleFloor", angle, 25050.0,
               (100.0, 1200.0, 1500.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC], tangent_offset=-950.0)
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_09_GroundRight", "DestructibleFloor", angle, 25050.0,
               (100.0, 1200.0, 1500.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC], tangent_offset=950.0)
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_09_DestructibleFloor", "DestructibleFloor", angle, 25050.0,
               (100.0, 700.0, 1500.0), "LNPDestructibleTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_DESTRUCTIBLE], movable=True)
    spawn_probe(actor_subsystem, sphere_mesh, "DestructibleFloor", angle, 24500.0)

    angle = CASE_ANGLES["OctantSeam"]
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_10_SeamA", "OctantSeam", angle, 25050.0,
               (100.0, 1000.0, 1600.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC], tangent_offset=-500.0)
    spawn_mesh(actor_subsystem, cube_mesh, "SSN_10_SeamB", "OctantSeam", angle, 25050.0,
               (100.0, 1000.0, 1600.0), "LNPStaticTerrain", [TAG_SUPPORT, TAG_BLOCKER, TAG_STATIC], tangent_offset=500.0)
    spawn_probe(actor_subsystem, sphere_mesh, "OctantSeam", angle, 24500.0)

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
