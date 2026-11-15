"""1-B4b 이끼 바위 2개와 절벽 부분 보강 1개를 기존 WIP에서 시험한다.

콘솔: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/BuildRidgeRockProbe.py"
기존 1-B4a 배치가 필요하다. 맵 전환·원본 편집·베이크는 하지 않는다.
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

OUT = ROOT / "Saved/Profiling/Graphics/Phase01/RidgeRockProbe_20261115"
PREFIX = "GraphicsRockProbe_"
MESHES = {
    "Moss": "/Game/Megascans/3D_Assets/MossyForestBoulder/SM_MossyForestBoulder_06",
    "Cliff": "/Game/SmartAssets/AssetMeshes/SM_Cliff_01",
}
MATERIALS = {
    "Moss": "/Game/Megascans/3D_Assets/MossyForestBoulder/MI_MossyForestBoulder_09",
    "Cliff": "/Game/SmartAssets/AssetMaterials/MI_Cliff_01",
}
# ID·spec XY(cm)·법선 주위 회전·균일 스케일. 절벽은 로컬 +Y를 노출 방향으로 삼는다.
POINTS = (("Moss_00", 2650, -2750, 37, .8), ("Moss_01", 2850, -2760, 163, .8),
          ("Cliff_02", 0, 0, 0, .15))


def write(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2), encoding="utf-8")


def build():
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first."
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name().split(".")[0] == LEVEL, "Open the existing WIP level first."
    dirty = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) +
             list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()))
    assert not dirty, "Save existing changes before placing the probe."
    editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = list(editor.get_all_level_actors())
    assert not any(a.get_actor_label().startswith(PREFIX) for a in existing), "Probe already exists."
    originals = [a for a in existing if isinstance(a, unreal.StaticMeshActor)]
    assert len(originals) == 11, "Expected original terrain and eight ground probes."
    terrain_actor = next(a for a in originals if a.static_mesh_component.static_mesh and
                         a.static_mesh_component.static_mesh.get_name() == "SM_Octant_Ridge_00_V011_R30000")
    terrain = terrain_actor.static_mesh_component
    assert max(abs(v) for v in xyz(terrain_actor.get_actor_location())) < .01
    assert max(abs(v - 1) for v in xyz(terrain_actor.get_actor_scale3d())) < .01
    r = terrain_actor.get_actor_rotation()
    assert max(abs(v) for v in (r.pitch, r.yaw, r.roll)) < .01, "Terrain must be canonical."
    assert terrain.get_material(0).get_path_name().split(".")[0] == (
        "/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V002/MI_Ridge_Surface")
    before = snapshot(originals)
    meshes = {kind: unreal.load_asset(path) for kind, path in MESHES.items()}
    for kind, mesh in meshes.items():
        assert mesh and mesh.get_num_sections(0) == 1
        assert mesh.get_material(0).get_path_name().split(".")[0] == MATERIALS[kind]
    plan, poses = [], []
    for point_id, x, y, angle, scale in POINTS:
        kind = point_id.split("_")[0]
        mesh = meshes[kind]
        bounds = mesh.get_bounding_box()
        analytic = unreal.Vector(*spec.point(x, y, spec.ground(x, y)))
        up = analytic * (-1.0 / analytic.length())
        hit, normal, component = trace(world, analytic, up)
        if normal.dot(up) < 0:
            normal *= -1.0
        slope = math.degrees(math.acos(max(-1, min(1, normal.dot(up)))))
        face_samples = []
        if kind == "Cliff":
            vertical = up - normal * up.dot(normal)
            vertical /= vertical.length()
            rotation = unreal.MathLibrary.make_rot_from_yz(normal, vertical)
            orientation = rotation.quaternion()
            center = (bounds.min + bounds.max) * .5
            # AABB 앞면의 중앙을 실제 지각보다 12cm 노출한다. 실제 메시 접촉의 증명은 아니다.
            local_front = unreal.Vector(center.x, bounds.max.y, center.z)
            location = hit + normal * 12 - orientation.rotate_vector(local_front * scale)
            for bx, bz in itertools.product((bounds.min.x, center.x, bounds.max.x),
                                             (bounds.min.z, center.z, bounds.max.z)):
                front = location + orientation.rotate_vector(unreal.Vector(bx, bounds.max.y, bz) * scale)
                sample_hit, _, sample_component = trace(world, front, normal)
                face_samples.append({"front": xyz(front), "terrain": xyz(sample_hit),
                                     "gap_cm": (front - sample_hit).dot(normal),
                                     "terrain_hit": sample_component == terrain})
        else:
            tangent = unreal.Vector(*spec.point(x, y + 10, spec.ground(x, y))) - analytic
            tangent -= normal * tangent.dot(normal)
            tangent /= tangent.length()
            a = math.radians(angle)
            tangent = tangent * math.cos(a) + normal.cross(tangent) * math.sin(a)
            rotation = unreal.MathLibrary.make_rot_from_zy(normal, tangent)
            orientation = rotation.quaternion()
            location = hit + normal * (-bounds.min.z * scale - 3)
        corners = [location + orientation.rotate_vector(unreal.Vector(bx, by, bz) * scale)
                   for bx, by, bz in itertools.product((bounds.min.x, bounds.max.x),
                       (bounds.min.y, bounds.max.y), (bounds.min.z, bounds.max.z))]
        radius = max(v.distance(hit) for v in corners)
        # spec 반폭+2m 및 bounds×1.2는 소수 시험 가드이며 최종 PCG 보호 규약이 아니다.
        path_clear = min(spec.path_profile(p, x, y)[0] - spec.path_profile(p, x, y)[2]
                         for p in spec.PATHS) - radius * 1.2 - 200
        pod_clear = min(hit.distance(unreal.Vector(*spec.point(*p["xy"], spec.ground(*p["xy"])))) -
                        p.get("formation_radius", spec.FORMATION_RADIUS) - radius
                        for p in spec.PODS if not p.get("layer"))
        guards = {"actual_terrain": component == terrain,
                  "slope": slope < 30 if kind == "Moss" else 70 < slope < 90,
                  "zone": spec.signed_distance(x, y, list(ZONE)) > radius * 1.2,
                  "seam": min(min(xyz(v)) for v in corners) > spec.SEAM_MARGIN + spec.SEAM_BAND,
                  "path_margin": path_clear > 0, "pod_margin": pod_clear > 0,
                  "rock_footprints": all(spec.signed_distance(x, y, spec.rock_outline(r)) < -radius * 1.2
                                         for r in spec.ROCKS),
                  "anchor_landings": all(hit.distance(unreal.Vector(*spec.point(*a["landing"],
                       spec.ground(*a["landing"])))) > radius + 400 for a in spec.ANCHORS),
                  "entrances": all(math.hypot(x - n["xy"][0], y - n["xy"][1]) > radius * 1.2 + 400
                                   for n in spec.NODES if "Gate" in n["id"]),
                  "cliff_front_proxy": all(s["terrain_hit"] and -10 <= s["gap_cm"] <= 30
                                           for s in face_samples)}
        plan.append({"label": PREFIX + point_id, "mesh": MESHES[kind], "xy": [x, y], "scale": scale,
                     "location": xyz(location), "rotation": [rotation.pitch, rotation.yaw, rotation.roll],
                     "slope_degrees": slope, "path_margin_cm": path_clear, "pod_margin_cm": pod_clear,
                     "radius_from_hit_cm": radius, "front_proxy": face_samples, "guards": guards})
        poses.append((mesh, location, rotation, scale))
    OUT.mkdir(parents=True, exist_ok=True)
    write("Plan.json", plan)
    assert all(all(p["guards"].values()) for p in plan), "Guard failed; see Plan.json. No actors spawned."
    backup = OUT / "BeforePlacement.umap"
    assert not backup.exists(), "Recovery point already exists; inspect before retrying."
    shutil.copy2(ROOT / ("Content" + LEVEL[5:] + ".umap"), backup)
    write("WipBefore.json", before)
    spawned = []
    try:
        for item, (mesh, location, rotation, scale) in zip(plan, poses):
            actor = editor.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
            assert actor
            spawned.append(actor)
            actor.set_actor_label(item["label"])
            actor.set_folder_path("Graphics/RockProbe_V001")
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
        assert before == snapshot(originals), "Existing mesh actors changed."
        write("Placement.json", {"existing_preserved": True, "actors": snapshot(spawned),
                                "status": "placed; manual exposure/contact review pending"})
    except Exception:
        for actor in reversed(spawned):
            editor.destroy_actor(actor)
        raise
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL)
    write("WipAfter.json", snapshot(originals + spawned))
    editor.set_selected_level_actors(spawned)
    unreal.log("Ridge rock probe placed and WIP saved; manual exposure/contact review pending.")


if __name__ == "__main__":
    build()
