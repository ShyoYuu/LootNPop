"""1-B4c 원본 Fern 1개를 기존 WIP에서 시험한다.

콘솔: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/BuildRidgeFernProbe.py"
1-B4a/b 배치가 필요하다. 원본 메시/MI/MPC·맵 전환·베이크는 수정하지 않는다.
"""
import itertools
import json
import math
from pathlib import Path
import shutil
import sys

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from BuildRidgeGroundProbe import ROOT, LEVEL, snapshot, trace, xyz, spec, ZONE

OUT = ROOT / "Saved/Profiling/Graphics/Phase01/RidgeFernProbe_20261115"
LABEL = "GraphicsFernProbe_Original_00"
MESH = "/Game/Megascans/3D_Plants/Fern/SM_Fern_01"
MI = "/Game/Megascans/3D_Plants/Fern/MI_Fern_01"
X, Y, ANGLE = 3000, -2700, 67
# 최대 변위 측정값이 아닌, 첫 소수 시험에서 두는 여유(cm)다.
WIND_PADDING = 30


def write(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2), encoding="utf-8")


def build():
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first."
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name().split(".")[0] == LEVEL, "Open the existing WIP level first."
    assert not (list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) +
                list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())), "Save existing changes first."
    editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = list(editor.get_all_level_actors())
    assert not any(a.get_actor_label().startswith("GraphicsFernProbe_") for a in existing), "Probe already exists."
    originals = [a for a in existing if isinstance(a, unreal.StaticMeshActor)]
    assert len(originals) == 14, "Expected original terrain and eleven ground/rock probes."
    terrain_actor = next(a for a in originals if a.static_mesh_component.static_mesh and
                         a.static_mesh_component.static_mesh.get_name() == "SM_Octant_Ridge_00_V011_R30000")
    terrain = terrain_actor.static_mesh_component
    r = terrain_actor.get_actor_rotation()
    assert max(abs(v) for v in xyz(terrain_actor.get_actor_location())) < .01
    assert max(abs(v - 1) for v in xyz(terrain_actor.get_actor_scale3d())) < .01
    assert max(abs(v) for v in (r.pitch, r.yaw, r.roll)) < .01
    assert terrain.get_material(0).get_path_name().split(".")[0] == (
        "/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V002/MI_Ridge_Surface")
    before = snapshot(originals)
    mesh = unreal.load_asset(MESH)
    assert mesh and mesh.get_material(0).get_path_name().split(".")[0] == MI
    bounds = mesh.get_bounding_box()
    analytic = unreal.Vector(*spec.point(X, Y, spec.ground(X, Y)))
    up = analytic * (-1 / analytic.length())
    hit, normal, component = trace(world, analytic, up)
    if normal.dot(up) < 0:
        normal *= -1
    slope = math.degrees(math.acos(max(-1, min(1, normal.dot(up)))))
    tangent = unreal.Vector(*spec.point(X, Y + 10, spec.ground(X, Y))) - analytic
    tangent -= normal * tangent.dot(normal)
    tangent /= tangent.length()
    angle = math.radians(ANGLE)
    tangent = tangent * math.cos(angle) + normal.cross(tangent) * math.sin(angle)
    rotation = unreal.MathLibrary.make_rot_from_zy(normal, tangent)
    location = hit + normal * (-bounds.min.z - 1.5)
    corners = [location + rotation.quaternion().rotate_vector(unreal.Vector(x, y, z))
               for x, y, z in itertools.product((bounds.min.x, bounds.max.x),
                    (bounds.min.y, bounds.max.y), (bounds.min.z, bounds.max.z))]
    radius = max(v.distance(hit) for v in corners) + WIND_PADDING
    path_clear = min(spec.path_profile(p, X, Y)[0] - spec.path_profile(p, X, Y)[2]
                     for p in spec.PATHS) - radius * 1.2 - 200
    pod_clear = min(hit.distance(unreal.Vector(*spec.point(*p["xy"], spec.ground(*p["xy"])))) -
                    p.get("formation_radius", spec.FORMATION_RADIUS) - radius
                    for p in spec.PODS if not p.get("layer"))
    guards = {"actual_terrain": component == terrain, "slope": slope < 30,
              "zone": spec.signed_distance(X, Y, list(ZONE)) > radius * 1.2,
              "seam": min(xyz(hit)) - radius > spec.SEAM_MARGIN + spec.SEAM_BAND,
              "path_margin": path_clear > 0, "pod_margin": pod_clear > 0,
              "rock_footprints": all(spec.signed_distance(X, Y, spec.rock_outline(r)) < -radius * 1.2
                                     for r in spec.ROCKS),
              "anchor_landings": all(hit.distance(unreal.Vector(*spec.point(*a["landing"],
                  spec.ground(*a["landing"])))) > radius + 400 for a in spec.ANCHORS),
              "entrances": all(math.hypot(X - n["xy"][0], Y - n["xy"][1]) > radius * 1.2 + 400
                               for n in spec.NODES if "Gate" in n["id"])}
    plan = {"label": LABEL, "mesh": MESH, "material": MI, "xy": [X, Y], "scale": 1,
            "location": xyz(location), "rotation": [rotation.pitch, rotation.yaw, rotation.roll],
            "hit": xyz(hit), "normal": xyz(normal), "slope_degrees": slope,
            "trial_wind_padding_cm": WIND_PADDING, "padded_radius_cm": radius,
            "path_margin_cm": path_clear, "pod_margin_cm": pod_clear, "guards": guards}
    OUT.mkdir(parents=True, exist_ok=True)
    write("Plan.json", plan)
    assert all(guards.values()), "Guard failed; see Plan.json. No actor spawned."
    backup = OUT / "BeforePlacement.umap"
    assert not backup.exists(), "Recovery point already exists; inspect before retrying."
    shutil.copy2(ROOT / ("Content" + LEVEL[5:] + ".umap"), backup)
    write("WipBefore.json", before)
    actor = editor.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
    assert actor
    try:
        actor.set_actor_label(LABEL)
        actor.set_folder_path("Graphics/FernProbe_V001")
        c = actor.static_mesh_component
        assert c.set_static_mesh(mesh)
        c.set_editor_property("component_tags", [unreal.Name("LNP.Surface.Decoration")])
        c.set_editor_property("use_default_collision", False)
        c.set_collision_profile_name("LNPDecoration")
        c.set_editor_property("bCanEverAffectNavigation", False)
        c.set_editor_property("bEvaluateWorldPositionOffset", True)
        c.set_mobility(unreal.ComponentMobility.STATIC)
        assert c.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
        assert str(c.get_collision_profile_name()) == "LNPDecoration"
        assert before == snapshot(originals), "Existing mesh actors changed."
        write("Placement.json", {"existing_preserved": True, "actor": snapshot([actor]),
            "wpo_enabled": c.get_editor_property("bEvaluateWorldPositionOffset"),
            "wpo_disable_distance": c.get_editor_property("world_position_offset_disable_distance"),
            "status": "placed; manual motion/contact review pending; original wind uses world axes"})
    except Exception:
        editor.destroy_actor(actor)
        raise
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL)
    write("WipAfter.json", snapshot(originals + [actor]))
    editor.set_selected_level_actors([actor])
    unreal.log("Ridge fern probe placed and WIP saved; manual motion/contact review pending.")


if __name__ == "__main__":
    build()
