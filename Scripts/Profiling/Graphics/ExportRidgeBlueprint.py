"""V011 설계도를 UE 없이 내보낸다. UE에서는 verify 인자로 실제 마커 좌표를 대조한다."""
import json
import math
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1]))
import RidgeOctantSpec as spec

OUTPUT = HERE / "RidgeBlueprint_V001.json"
SOURCE_LEVEL = "/Game/Maps/Ridge_00/LVI_Octant_Ridge_00"
REVIEW_LIFT_CM = 200


def plate_cliff_edges(plate):
    """원래 단 외곽 선분과 중점에서 실제 V011 노면 양쪽을 샘플링한 단차를 기록한다."""
    polygon = plate["polygon"]
    segments = list(zip(polygon, polygon[1:] + polygon[:1]))
    winding = math.copysign(1, sum(ax * by - bx * ay for (ax, ay), (bx, by) in segments))
    # edge 전이 폭과 V011 edge_jitter 최대 진폭(170+90cm)을 모두 지난 곳에서 잰다.
    offset = plate["edge"] + 260
    result = []
    for a, b in segments:
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = math.hypot(dx, dy)
        nx, ny = winding * dy / length, -winding * dx / length
        x, y = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
        inner = round(spec.ground(x - nx * offset, y - ny * offset), 3)
        outer = round(spec.ground(x + nx * offset, y + ny * offset), 3)
        result.append({"start_xy": a, "end_xy": b, "sample_offset_cm": offset,
                       "inner_h": inner, "outer_h": outer, "drop_cm": round(inner - outer, 3)})
    return result


def blueprint():
    placement = spec.placement()
    for key, prefix in (("pods", "Spawn_"), ("anchors", "Anchor_"), ("nodes", "Review_Ridge")):
        for marker in placement[key]:
            x, y, h = marker["local"]
            marker["actor_label"] = prefix + marker["id"]
            marker["actor_world_cm"] = spec.point(x, y, h + (REVIEW_LIFT_CM if key == "nodes" else 0))
    for marker in placement["anchors"]:
        marker["landing_local"] = [*marker["landing"], spec.layer_height(*marker["landing"], marker["layer"])]
    return {
        "source_version": spec.VERSION,
        "source_level": SOURCE_LEVEL,
        "radius_cm": spec.RADIUS,
        "local_coordinates": "Octant tangent X/Y, radial H positive towards the center, all in cm",
        "review_marker_lift_cm": REVIEW_LIFT_CM,
        "plates": [dict(plate, cliff_edges=plate_cliff_edges(plate)) for plate in spec.active(spec.PLATES)],
        "basins": spec.active(spec.BASINS),
        "path_point_fields": ["x", "y", "h", "half_width"],
        "paths": [dict(path, points=spec.path_points(path)) for path in spec.active(spec.PATHS)],
        **placement,
        "views": [{"id": name, "eye_local": eye if eye[2] is not None else spec.ground_point(eye[:2], 170),
                   "target_local": target, "lens_mm": lens} for name, eye, target, lens in spec.VIEWS],
        "sight_expectations": [{"from": origin, "target": target, "visible": visible}
                               for origin, target, visible in spec.SIGHT_EXPECTATIONS],
        "preserve": {
            "caves": [{"id": cave["id"], "outline": cave["outline"], "bottom_abs": cave["bottom_abs"]}
                      for cave in spec.active(spec.CAVES)],
            "islands": spec.active(spec.ISLANDS),
            "bridges": [rock for rock in spec.active(spec.ROCKS) if rock["role"] == "Bridge"],
        },
    }


def verify():
    import unreal

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name().split(".")[0] == SOURCE_LEVEL, "Open the production Ridge V011 level first."
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    data = json.loads(OUTPUT.read_text(encoding="utf-8"))
    summary = {}
    for key, cls in (("pods", unreal.LNPMassSpawnPoint), ("anchors", unreal.LNPPlacementMarker),
                     ("nodes", unreal.TargetPoint)):
        found = {actor.get_actor_label(): actor.get_actor_location() for actor in actors if isinstance(actor, cls)}
        expected = {marker["actor_label"]: unreal.Vector(*marker["actor_world_cm"]) for marker in data[key]}
        assert len(found) == len(expected) and found.keys() == expected.keys(), (key, found.keys(), expected.keys())
        maximum = max((found[label] - pos).length() for label, pos in expected.items())
        assert maximum <= 0.05, (key, maximum)
        summary[key] = {"count": len(found), "max_delta_cm": maximum}
    unreal.log("RIDGE_BLUEPRINT_VERIFIED " + json.dumps(summary))


if __name__ == "__main__":
    if sys.argv[1:] == ["verify"]:
        verify()
    else:
        assert not sys.argv[1:], "Usage: ExportRidgeBlueprint.py [verify]"
        OUTPUT.write_text(json.dumps(blueprint(), indent=2, allow_nan=False) + "\n", encoding="utf-8")
