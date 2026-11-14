"""1-B4a의 고정된 소수 장식 시험을 현재 WIP에만 배치한다.

콘솔: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/BuildRidgeGroundProbe.py"
맵 전환/복제·원본 에셋 편집·베이크는 하지 않는다.
"""
import json
import math
from pathlib import Path
import shutil
import sys

import unreal
from editor_toolset.toolsets.object import ObjectTools

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "Scripts"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import RidgeOctantSpec as spec
from BuildRidgeSurfaceProbe import ZONE

LEVEL = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/V001/LVI_Ridge_SurfaceProbe"
OUT = ROOT / "Saved/Profiling/Graphics/Phase01/RidgeGroundProbe_20261115"
PREFIX = "GraphicsGroundProbe_"
MESHES = {
    "Sandstone": "/Game/Megascans/3D_Assets/SandstoneBoulder/SM_SandstoneBoulder_01",
    "Clover": "/Game/Megascans/3D_Plants/CloverVarieties/SM_CloverVarieties_01",
}
# 고정 ID·spec X/Y(cm)·현지 법선 주위 회전·균일 스케일. Riverbed_02/03은 외곽 모양 부적합으로 제외했다.
POINTS = (("Sandstone_00", 2700, -2700, 26, 1.0), ("Sandstone_01", 2900, -2710, 138, 1.0),
          ("Clover_04", 2850, -2180, 34, .9), ("Clover_05", 2970, -2270, 151, 1.05),
          ("Clover_06", 3070, -2370, 273, .85), ("Clover_07", 3270, -2320, 83, 1.1),
          ("Clover_08", 3410, -2270, 218, 1.0), ("Clover_09", 3450, -2430, 326, .95))
FIELDS = ["staticMesh", "overrideMaterials", "componentTags", "bodyInstance", "relativeLocation",
          "relativeRotation", "relativeScale3D", "bVisibleInRayTracing", "bAffectDistanceFieldLighting",
          "castShadow", "bCanEverAffectNavigation"]


def xyz(v):
    return [v.x, v.y, v.z]


def snapshot(actors):
    return {a.get_path_name(): {"transform": {"location": xyz(a.get_actor_location()),
            "rotation": [a.get_actor_rotation().pitch, a.get_actor_rotation().yaw, a.get_actor_rotation().roll],
            "scale": xyz(a.get_actor_scale3d())},
            "component": json.loads(ObjectTools.get_properties(a.static_mesh_component, FIELDS))}
            for a in actors}


def trace(world, p, up):
    hit = unreal.SystemLibrary.line_trace_single(world, p + up * 500, p - up * 500,
              unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, True)
    assert hit, "Ground trace missed."
    data = hit.to_dict()
    return data["location"], data["normal"], data["hit_component"]


def build():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name().split(".")[0] == LEVEL, "Open the existing WIP level first."
    dirty = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) +
             list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()))
    assert not dirty, "Save existing changes before placing the probe."
    editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = list(editor.get_all_level_actors())
    assert not any(a.get_actor_label().startswith(PREFIX) for a in existing), "Probe already exists."
    originals = [a for a in existing if isinstance(a, unreal.StaticMeshActor)]
    assert len(originals) == 3, "Unexpected existing mesh actors."
    for actor in originals:
        rotation = actor.get_actor_rotation()
        assert max(abs(v) for v in xyz(actor.get_actor_location())) < .01
        assert max(abs(v - 1) for v in xyz(actor.get_actor_scale3d())) < .01
        assert max(abs(v) for v in (rotation.pitch, rotation.yaw, rotation.roll)) < .01
    before = snapshot(originals)
    terrain = next(a.static_mesh_component for a in originals
                   if a.static_mesh_component.static_mesh.get_name() == "SM_Octant_Ridge_00_V011_R30000")
    meshes = {key: unreal.load_asset(path) for key, path in MESHES.items()}
    assert all(meshes.values())
    plan = []
    poses = []
    for point_id, x, y, angle, scale in POINTS:
        kind = point_id.split("_")[0]
        mesh = meshes[kind]
        bounds = mesh.get_bounding_box()
        radius = math.sqrt(sum(max(abs(a), abs(b)) ** 2 for a, b in zip(xyz(bounds.min), xyz(bounds.max)))) * scale
        analytic = unreal.Vector(*spec.point(x, y, spec.ground(x, y)))
        up = analytic * (-1.0 / analytic.length())
        hit, normal, hit_component = trace(world, analytic, up)
        if normal.dot(up) < 0:
            normal *= -1.0
        slope = math.degrees(math.acos(max(-1, min(1, normal.dot(up)))))
        tangent = unreal.Vector(*spec.point(x, y + 10, spec.ground(x, y))) - analytic
        tangent -= normal * tangent.dot(normal)
        tangent /= tangent.length()
        a = math.radians(angle)
        tangent = tangent * math.cos(a) + normal.cross(tangent) * math.sin(a)
        rotation = unreal.MathLibrary.make_rot_from_zy(normal, tangent)
        sink = .7 if kind == "Clover" else 2.0
        location = hit + normal * (-bounds.min.z * scale - sink)
        path_clear = min(spec.path_profile(p, x, y)[0] - spec.path_profile(p, x, y)[2]
                         for p in spec.PATHS) - radius * 1.2 - 200
        pod_clear = min(math.dist(xyz(hit), spec.point(*p["xy"], spec.ground(*p["xy"]))) -
                        p.get("formation_radius", spec.FORMATION_RADIUS) - radius
                        for p in spec.PODS if not p.get("layer"))
        guards = {"zone": spec.signed_distance(x, y, list(ZONE)) > radius * 1.2,
                  "seam": min(xyz(hit)) - radius > spec.SEAM_MARGIN + spec.SEAM_BAND,
                  "path_margin": path_clear > 0, "rock_pod_margin": kind != "Sandstone" or pod_clear > 0,
                  "rock_footprints": all(spec.signed_distance(x, y, spec.rock_outline(r)) < -radius * 1.2
                                         for r in spec.ROCKS),
                  "slope": slope < 30, "actual_terrain_component": hit_component == terrain}
        guards["anchor_landings"] = all(math.dist(xyz(hit), spec.point(*a["landing"], spec.ground(*a["landing"])))
                                        > radius + 400 for a in spec.ANCHORS)
        guards["entrances"] = all(math.hypot(x - n["xy"][0], y - n["xy"][1]) > radius * 1.2 + 400
                                  for n in spec.NODES if "Gate" in n["id"])
        plan.append({"label": PREFIX + point_id, "kind": kind, "xy": [x, y],
                     "mesh": MESHES[kind], "location": xyz(location), "normal": xyz(normal),
                     "rotation": {"pitch": rotation.pitch, "yaw": rotation.yaw, "roll": rotation.roll},
                     "scale": scale, "sink_cm": sink, "bound_radius_cm": radius, "slope_degrees": slope,
                     "hit_component": hit_component.get_path_name(), "analytic_distance_cm": hit.distance(analytic),
                     "path_margin_cm": path_clear, "pod_margin_cm": pod_clear, "guards": guards})
        poses.append((mesh, location, rotation, scale))
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "Plan.json").write_text(json.dumps(plan, indent=2), encoding="utf-8")
    assert all(all(p["guards"].values()) for p in plan), "Placement guard failed; see Plan.json. No actors spawned."
    shutil.copy2(ROOT / ("Content" + LEVEL[5:] + ".umap"), OUT / "BeforePlacement.umap")
    (OUT / "WipBefore.json").write_text(json.dumps(before, indent=2), encoding="utf-8")
    spawned = []
    for item, (mesh, location, rotation, scale) in zip(plan, poses):
        actor = editor.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
        assert actor
        spawned.append(actor)
        actor.set_actor_label(item["label"])
        actor.set_folder_path("Graphics/GroundProbe_V001")
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        c = actor.static_mesh_component
        assert c.set_static_mesh(mesh)
        c.set_editor_property("component_tags", [unreal.Name("LNP.Surface.Decoration")])
        c.set_editor_property("use_default_collision", False)
        c.set_collision_profile_name("LNPDecoration")
        c.set_editor_property("bCanEverAffectNavigation", False)
        c.set_mobility(unreal.ComponentMobility.STATIC)
        assert str(c.get_collision_profile_name()) == "LNPDecoration"
        assert c.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    after = snapshot(originals)
    assert before == after, "Existing terrain actors changed."
    (OUT / "Placement.json").write_text(json.dumps({"originals_preserved": before == after,
        "actors": snapshot(spawned), "plan": plan, "status": "placed; manual contact and visual review pending"}, indent=2), encoding="utf-8")
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL)
    editor.set_selected_level_actors(spawned)
    unreal.log("Ridge ground probe placed and WIP saved; manual contact review pending.")


if __name__ == "__main__":
    build()
