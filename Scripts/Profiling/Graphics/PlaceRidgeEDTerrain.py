"""UE Cmd에서 Ridge ED 지형의 구역별 배치를 place, export, clear로 재현한다."""
import itertools
import json
import math
from pathlib import Path
import sys

import unreal

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parents[1])]
from AssembleRidgeEDBase import LEVEL, OUT, editor_world
from BuildRidgeDecorationPCG import HOOK_LINE_RADIUS, LANDING_RADIUS, PATH_HALF_SCALE, POD_CORE_RADIUS, ROUTE_HALF_WIDTH
import RidgeEDBaseSpec as base

LAYOUT = HERE / "EDTerrainPlacement_V001.json"
TAG = "LNP.Surface.Decoration"
PROFILE = "LNPDecoration"
SEAM_CLEARANCE_CM = 500
POSITION_TOLERANCE_CM = 0.01
ROTATION_VECTOR_TOLERANCE = 1e-5


def xyz(vector):
    return vector.x, vector.y, vector.z


def asset_path(asset):
    return asset.get_path_name().split(".")[0]


def frame(x, y, position):
    up = position * (-1.0 / position.length())
    direction = unreal.Vector(*base.ridge.point(x + 10, y, 0)) - unreal.Vector(*base.ridge.point(x, y, 0))
    rotation = unreal.MathLibrary.make_rot_from_xz(direction - up * up.dot(direction), up)
    return unreal.Transform(unreal.Vector(), rotation, unreal.Vector(1, 1, 1))


def transform(row, ground=base.base_ground):
    x, y = row["local_xy_cm"]
    position = unreal.Vector(*base.ridge.point(x, y, ground(x, y) + row["height_offset_cm"]))
    pitch, roll = row["tilt_deg"]
    rotation = unreal.MathLibrary.transform_rotation(frame(x, y, position),
                                                     unreal.Rotator(pitch=pitch, yaw=row["yaw_deg"], roll=roll))
    return unreal.Transform(position, rotation, unreal.Vector(*row["scale"]))


def capture(actor):
    position = actor.get_actor_location()
    x, y, h = base.ridge.local(xyz(position))
    offset = h - base.base_ground(x, y)
    rotation = unreal.MathLibrary.inverse_transform_rotation(frame(x, y, position), actor.get_actor_rotation())
    component = actor.static_mesh_component
    overrides = {str(slot): asset_path(material) for slot, material in
                 enumerate(component.get_editor_property("override_materials")) if material}
    return {"id": actor.get_actor_label(), "mesh": asset_path(component.static_mesh), "local_xy_cm": [x, y],
            "height_offset_cm": offset, "yaw_deg": rotation.yaw, "tilt_deg": [rotation.pitch, rotation.roll],
            "scale": list(xyz(actor.get_actor_scale3d())), "material_overrides": overrides}


def bounds_of(points):
    return tuple(map(min, zip(*points))), tuple(map(max, zip(*points)))


def overlaps_box(start, end, lower, upper):
    # 선분을 각 축의 상자 구간으로 자른다. 공통 구간이 남으면 상자에 닿는다.
    enter, exit = 0.0, 1.0
    for a, b, lo, hi in zip(start, end, lower, upper):
        delta = b - a
        if abs(delta) < 1e-8:
            if not lo <= a <= hi:
                return False
            continue
        t0, t1 = sorted(((lo - a) / delta, (hi - a) / delta))
        enter, exit = max(enter, t0), min(exit, t1)
        if enter > exit:
            return False
    return True


def protection():
    ridge = base.ridge
    corridors = [("Path_" + path["id"], a[:2], b[:2], max(a[3], b[3]) * PATH_HALF_SCALE)
                 for path in ridge.active(ridge.PATHS) for a, b in zip(ridge.path_points(path), ridge.path_points(path)[1:])]
    corridors += [("Route_" + route["id"], a, b, ROUTE_HALF_WIDTH) for route in ridge.active(ridge.ROUTES)
                  if ridge.route_layer(route) == "surface" for a, b in zip(route["xy"], route["xy"][1:])]
    placement = json.loads((OUT / "placement.json").read_text(encoding="utf-8"))

    def point(local, lift=0):
        x, y, h = local
        return unreal.Vector(*ridge.point(x, y, h + lift))

    eyes = {row["id"]: point(row["local"], 170) for row in placement["nodes"]}
    pods = {"Pod_" + row["id"]: row for row in placement["pods"]}
    spheres = [(label, point(row["local"]), POD_CORE_RADIUS) for label, row in pods.items()]
    above = {row["id"]: row["above"] for row in ridge.active(ridge.ANCHORS)}
    lines = []
    for anchor in placement["anchors"]:
        end = point(anchor["local"])
        spheres.append(("Landing_" + anchor["id"], point(anchor["local"], -above[anchor["id"]]), LANDING_RADIUS))
        lines.extend((f"Hook_{origin}_{anchor['id']}", eyes[origin], end, HOOK_LINE_RADIUS) for origin in anchor["from"])

    landmark = next(rock for rock in ridge.ROCKS if rock["id"] == ridge.LANDMARK_ROCK)
    x, y, top = max(landmark["outline"], key=lambda vertex: vertex[2])
    targets = {name: point(row["local"], 150) for name, row in pods.items()}
    targets["LandmarkTop"] = point((x, y, ridge.rock_top(landmark, x, y, top)), 200)
    lines.extend((f"Sight_{origin}_{target}", eyes[origin], targets[target], 0)
                 for origin, target, visible in ridge.SIGHT_EXPECTATIONS if visible)
    return corridors, spheres, lines


def clearance(mesh, pose, protected):
    bounds = mesh.get_bounding_box()
    lower, upper = xyz(bounds.min), xyz(bounds.max)
    corners = [xyz(pose.transform_location(unreal.Vector(*corner))) for corner in itertools.product(*zip(lower, upper))]
    seam = min(min(point) for point in corners)
    if seam < SEAM_CLEARANCE_CM:
        return seam, ["Seam"]
    local_min, local_max = bounds_of([base.ridge.local(corner)[:2] for corner in corners])
    corridors, spheres, lines = protected
    hits = [label for label, a, b, radius in corridors if overlaps_box(
        a, b, [lo - radius for lo in local_min], [hi + radius for hi in local_max])]
    scale = xyz(pose.scale3d)
    for label, center, radius in spheres:
        local = xyz(pose.inverse_transform_location(center))
        delta = [(max(lo - value, value - hi, 0) * abs(s)) for value, lo, hi, s in zip(local, lower, upper, scale)]
        if sum(value * value for value in delta) < radius * radius:
            hits.append(label)
    for label, a, b, radius in lines:
        padding = [radius / abs(s) for s in scale]
        if overlaps_box(xyz(pose.inverse_transform_location(a)), xyz(pose.inverse_transform_location(b)),
                        [lo - pad for lo, pad in zip(lower, padding)], [hi + pad for hi, pad in zip(upper, padding)]):
            hits.append(label)
    return seam, hits


def require_editor(mode):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert not levels.is_in_play_in_editor(), "Stop PIE first."
    if asset_path(editor_world()) != LEVEL:
        saving = unreal.EditorLoadingAndSavingUtils
        assert not saving.get_dirty_map_packages() and not saving.get_dirty_content_packages(), "Review unsaved changes first."
        assert mode == "place", "Open the ED terrain work level first."
        assert levels.load_level(LEVEL), LEVEL
    assert asset_path(editor_world()) == LEVEL
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def zone_actors(actors, zone):
    found = [actor for actor in actors.get_all_level_actors() if str(actor.get_folder_path()) == "EDTerrain/" + zone]
    assert all(isinstance(actor, unreal.StaticMeshActor) and actor.static_mesh_component.static_mesh for actor in found)
    by_label = {actor.get_actor_label(): actor for actor in found}
    assert len(by_label) == len(found), "Duplicate zone actor labels."
    return by_label


def component_contract(actor):
    component = actor.static_mesh_component
    return (str(component.get_collision_profile_name()) == PROFILE
            and component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
            and [str(tag) for tag in component.get_editor_property("component_tags")] == [TAG]
            and component.get_editor_property("mobility") == unreal.ComponentMobility.STATIC)


def prepare_placements(rows, ground=base.base_ground, protected=None):
    assert rows and len({row["id"] for row in rows}) == len(rows), "Empty zone or duplicate actor labels."
    if protected is None:
        protected = protection()
    prepared, violations = [], {}
    for row in rows:
        assert all(math.isfinite(s) and abs(s) >= 0.001 for s in row["scale"]), row["id"]
        mesh = unreal.load_asset(row["mesh"])
        assert isinstance(mesh, unreal.StaticMesh), row["mesh"]
        pose = transform(row, ground)
        seam, hits = clearance(mesh, pose, protected)
        if hits:
            violations[row["id"]] = hits
        overrides = {int(slot): unreal.load_asset(path) for slot, path in row.get("material_overrides", {}).items()}
        assert all(0 <= slot < len(mesh.get_editor_property("static_materials")) and isinstance(value, unreal.MaterialInterface)
                   for slot, value in overrides.items()), row["id"]
        prepared.append((row, mesh, pose, overrides, seam))
    assert not violations, json.dumps(violations)
    return prepared


def verify_against(found, prepared):
    assert found.keys() == {row["id"] for row, *_ in prepared}, "Zone actor labels differ from the JSON."
    max_delta, max_axis_error = 0.0, 0.0
    for row, mesh, pose, overrides, _ in prepared:
        actor = found[row["id"]]
        component = actor.static_mesh_component
        assert component.static_mesh == mesh and component_contract(actor), row["id"]
        materials = {slot: value for slot, value in enumerate(component.get_editor_property("override_materials")) if value}
        assert materials == overrides, row["id"]
        actual = actor.get_actor_transform()
        delta = (actual.translation - pose.translation).length()
        max_delta = max(max_delta, delta)
        assert delta <= POSITION_TOLERANCE_CM and (actual.scale3d - pose.scale3d).length() < 1e-6, row["id"]
        for unit in (unreal.Vector(1, 0, 0), unreal.Vector(0, 1, 0), unreal.Vector(0, 0, 1)):
            axis_error = (actual.rotation.rotate_vector(unit) - pose.rotation.rotate_vector(unit)).length()
            max_axis_error = max(max_axis_error, axis_error)
            assert axis_error <= ROTATION_VECTOR_TOLERANCE, row["id"]
    return {"actors": len(found), "max_position_delta_cm": max_delta, "max_axis_error": max_axis_error,
            "min_seam_clearance_cm": min(seam for *_, seam in prepared)}


def place(actors, zone, rows, ground=base.base_ground, protected=None):
    assert not zone_actors(actors, zone), "Clear the current zone before placing."
    prepared = prepare_placements(rows, ground, protected)
    spawned = []
    with unreal.ScopedEditorTransaction("Place Ridge ED terrain " + zone):
        try:
            for row, mesh, pose, overrides, _ in prepared:
                actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, pose.translation, pose.rotation.rotator())
                assert actor, row["id"]
                spawned.append(actor)
                actor.set_actor_label(row["id"])
                actor.set_folder_path("EDTerrain/" + zone)
                actor.set_actor_scale3d(pose.scale3d)
                component = actor.static_mesh_component
                component.set_static_mesh(mesh)
                component.set_editor_property("component_tags", [TAG])
                component.set_editor_property("use_default_collision", False)
                component.set_collision_profile_name(PROFILE)
                component.set_mobility(unreal.ComponentMobility.STATIC)
                for slot, material in overrides.items():
                    component.set_material(slot, material)
            return verify_against(zone_actors(actors, zone), prepared)
        except Exception:
            for actor in reversed(spawned):
                actors.destroy_actor(actor)
            raise


def export(actors, zone, layout):
    found = zone_actors(actors, zone)
    rows = [capture(found[label]) for label in sorted(found)]
    result = verify_against(found, prepare_placements(rows))
    layout["zones"][zone] = rows
    temporary = LAYOUT.with_suffix(".tmp")
    temporary.write_text(json.dumps(layout, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    temporary.replace(LAYOUT)
    return result


def clear(actors, zone, rows):
    found = zone_actors(actors, zone)
    # JSON에 들어 있지 않은 수동 수정이 있으면 먼저 export한다.
    result = verify_against(found, prepare_placements(rows))
    assert not any(actor.get_attach_parent_actor() or actor.get_attached_actors() for actor in found.values()), \
        "Detach zone actors before replacing them."
    with unreal.ScopedEditorTransaction("Clear Ridge ED terrain " + zone):
        for actor in found.values():
            assert actors.destroy_actor(actor), actor.get_actor_label()
    assert not zone_actors(actors, zone)
    return result


def run(mode, zone):
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    assert layout["base_version"] == base.VERSION and zone in layout["zones"], zone
    actors = require_editor(mode)
    if mode == "export":
        result = export(actors, zone, layout)
    else:
        result = {"place": place, "clear": clear}[mode](actors, zone, layout["zones"][zone])
    unreal.log(f"RIDGE_ED_TERRAIN_{mode.upper()} {zone} {json.dumps(result)}")


if __name__ == "__main__":
    assert len(sys.argv) == 3 and sys.argv[1] in {"place", "export", "clear"}, "Use place|export|clear Z1."
    run(*sys.argv[1:])
