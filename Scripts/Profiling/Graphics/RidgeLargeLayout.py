"""U4 전체 큰 배치의 새 사본 조립·결과 export·보존 대조.

UE Cmd: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/RidgeLargeLayout.py" export V001
verify <판>: 읽기 전용 대조. export <판>: 대조 통과 시 새 구역 JSON에 실제 자세를 되읽는다.
build <새 판>: 채택된 이전 맵(첫 단위는 V010)을 복제하고 고정되지 않은 새 소유자만 더한다.
기존 지각·Cull·충돌·층 생성기를 호출하지 않으며, 저장은 지정한 새 최상위 맵만 한다.
"""
import datetime
import hashlib
import json
from pathlib import Path
import sys

import unreal

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parents[1])]
import RidgeZ1Layout as layout

PROJECT = HERE.parents[2]
AUTHORING = HERE / "LargeLayout_V001"
MANIFEST = AUTHORING / "RidgeLayout.json"
OUT = PROJECT / "Saved/Profiling/Graphics/Phase01/U4_1_V001"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def asset_file(package):
    return PROJECT / "Content" / (package.removeprefix("/Game/") + ".umap")


def inputs():
    manifest = read(MANIFEST)
    zones = [(AUTHORING / name, read(AUTHORING / name)) for name in manifest["zones"]]
    baseline = manifest["baseline"]
    assert sha(PROJECT / baseline["layout"]) == baseline["layout_sha256"], "Baseline layout changed"
    assert sha(asset_file(baseline["level"])) == baseline["level_sha256"], "Baseline map changed"
    for _, zone in zones:
        for row in zone["level_instances"]:
            assert sha(asset_file(row["source_level"])) == row["source_sha256"], row["source_level"]
            assert sha(asset_file(row["level"])) == row["level_sha256"], row["level"]
    previous = manifest.get("previous_unit")
    if previous:
        assert sha(asset_file(previous["level"])) == previous["level_sha256"], "Accepted previous map changed"
        for row in previous["files"]:
            assert sha(PROJECT / row["path"]) == row["sha256"], row["path"]
    return manifest, zones


def evidence(name, report):
    OUT.mkdir(parents=True, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    target = OUT / (name + "_" + stamp + ".json")
    with target.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=1)
    unreal.log("RIDGE_LARGE_LAYOUT %s %s" % (name, target))


def owner_actors(top, nested):
    result = {}
    for actor in top:
        if isinstance(actor, unreal.LevelInstance):
            loaded = actor.get_loaded_level()
            assert loaded, "Wait for loaded level: " + actor.get_actor_label()
            result[actor.get_actor_label()] = nested.get(loaded.get_path_name(), [])
    return result


def corridor_boxes(actors, corridor):
    """보행 띠·교량·Pod의 명시 표본을 큰 암석의 메시 로컬 상자와 보수적으로 대조한다."""
    points = []
    offsets = corridor.get("height_offsets_cm", [100])
    def add(x, y, height):
        for offset in offsets:
            h = (layout.spec.massif_ramp_ground(x, y) if corridor.get("height_mode", "ground") == "ground"
                 else height) + offset
            points.append((x, y, h, unreal.Vector(*layout.spec.ridge.point(x, y, h))))
    if "disk_radius_cm" in corridor:
        import math
        x, y, height = corridor["points_xyh_cm"][0]
        add(x, y, height)
        radius = corridor["disk_radius_cm"]
        for ring in (radius * 0.5, radius):
            for i in range(24):
                angle = math.tau * i / 24
                add(x + ring * math.cos(angle), y + ring * math.sin(angle), height)
    else:
        authored = corridor["points_xyh_cm"]
        for index, ((ax, ay, ah), (bx, by, bh)) in enumerate(zip(authored, authored[1:])):
            dx, dy = bx - ax, by - ay
            length = (dx * dx + dy * dy) ** 0.5
            assert length > 0, corridor["id"]
            nx, ny = -dy / length, dx / length
            stations = max(1, round(length / 200))
            for i in range(stations + 1):
                t = i / stations
                half = corridor["half_width_cm"][index] * (1 - t) + corridor["half_width_cm"][index + 1] * t
                for offset in (-half, 0, half):
                    add(ax + dx * t + nx * offset, ay + dy * t + ny * offset, ah + (bh - ah) * t)
    hits, tested = [], 0
    for _, mesh, transform in layout.actor_instances(actors):
        name = mesh.get_path_name().lower()
        if not any(token in name for token in ("cliff", "boulder", "rock", "embankment", "terrain", "ground", "mesa")):
            continue
        box = mesh.get_bounding_box()
        size = box.max - box.min
        scale = transform.scale3d
        dims = sorted((abs(size.x * scale.x), abs(size.y * scale.y), abs(size.z * scale.z)))
        if dims[0] < 100 or dims[-1] < 300:
            continue
        tested += 1
        for x, y, h, point in points:
            local = unreal.MathLibrary.inverse_transform_location(transform, point)
            if all(low - 50 <= value <= high + 50 for low, value, high in (
                    (box.min.x, local.x, box.max.x), (box.min.y, local.y, box.max.y),
                    (box.min.z, local.z, box.max.z))):
                hits.append({"mesh": layout.test.package(mesh), "point_local_xyh_cm": [x, y, h]})
    return {"id": corridor["id"], "method": "mesh-local conservative box; 50cm padding; authored path or disk samples",
            "samples": len(points), "rock_instances_tested": tested, "hits": hits,
            "pass": not hits, "note": "Layout clearance only; no collision or PIE validation"}


def inspect(version, export=False):
    manifest, zones = inputs()
    expected_level = manifest["work_level_pattern"].format(version)
    assert layout.world_package() == expected_level, "Open " + expected_level
    assert not layout.test.dirty(), layout.test.dirty()
    baseline = read(PROJECT / manifest["baseline"]["layout"])
    snapshot = read(PROJECT / manifest["baseline"]["snapshot"])
    top, nested = layout.level_actors()
    # SceneTools.create_level_instance는 참조만 지정하며 내부 로드를 요청하지 않는다.
    # 게임 스레드에서 로드 완료를 기다리지 않고, 다음 Cmd에서 검사하도록 명시한다.
    pending = [actor for actor in top if isinstance(actor, unreal.LevelInstance) and not actor.get_loaded_level()]
    if pending:
        labels = [actor.get_actor_label() for actor in pending]
        for actor in pending:
            actor.load_level_instance()
        evidence("LoadRequested", {"level": expected_level, "pending": labels, "exported": False})
        unreal.log_warning("RIDGE_LARGE_LAYOUT_LOAD_REQUESTED %s; run the same command again after loading" %
                           json.dumps(labels))
        return
    owners = owner_actors(top, nested)
    by_label = {actor.get_actor_label(): actor for actor in top}
    issues, actors, inherited, additions = [], [], [], []
    for expected in baseline["actors"]:
        actor = by_label.get(expected["label"])
        assert actor, expected["label"]
        errors = layout.pose_error(actor.get_actor_transform(), layout.pose_transform(expected))
        comp = actor.static_mesh_component
        if errors[0] > 0.05 or errors[1] > 1e-5 or errors[2] > 1e-5:
            issues.append("Inherited actor pose changed: " + expected["label"])
        if layout.test.package(comp.static_mesh) != expected["mesh"]:
            issues.append("Inherited mesh changed: " + expected["label"])
        old = next(row for row in snapshot["actors"] if row["label"] == expected["label"])
        if str(comp.get_collision_profile_name()) != old["profile"]:
            issues.append("Inherited collision profile changed: " + expected["label"])
        overrides = {str(i): layout.test.package(mat) for i, mat in enumerate(
            comp.get_editor_property("override_materials")) if mat}
        if overrides != expected["material_overrides"]:
            issues.append("Inherited materials changed: " + expected["label"])
        actors.append({"label": expected["label"], "pose_error": errors})
    for expected in baseline["level_instances"]:
        actor = by_label.get(expected["label"])
        assert actor, expected["label"]
        if layout.test.package(actor.get_world_asset()) != expected["cull"]["level"]:
            issues.append("Inherited Cull reference changed: " + expected["label"])
        if max(layout.pose_error(actor.get_actor_transform(), unreal.Transform())) > 1e-5:
            issues.append("Inherited Cull pose changed: " + expected["label"])
        inherited.extend(owners[expected["label"]])
    fingerprint, _ = layout.fingerprint(inherited)
    if fingerprint != snapshot["visual_fingerprint"]:
        issues.append("Inherited visual fingerprint changed")
    exports = []
    locked = manifest.get("previous_unit", {}).get("locked_owners", {})
    for path, zone in zones:
        for expected in zone["level_instances"]:
            actor = by_label.get(expected["label"])
            assert actor, expected["label"]
            if layout.test.package(actor.get_world_asset()) != expected["level"]:
                issues.append("New assembly reference changed: " + expected["label"])
            actual = dict(expected, **layout.local_pose(actor.get_actor_transform()))
            fingerprint_new, _ = layout.fingerprint(owners[expected["label"]])
            colliding = []
            for child in owners[expected["label"]]:
                for comp in child.get_components_by_class(unreal.StaticMeshComponent):
                    if comp.static_mesh and comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
                        colliding.append(comp.get_path_name())
            if colliding:
                issues.append("New visual has collision")
            if fingerprint_new["instances"] != expected["expected_instances"]:
                issues.append("New assembly instance count mismatch")
            if expected["label"] in locked:
                old = locked[expected["label"]]
                errors = layout.pose_error(actor.get_actor_transform(), layout.pose_transform(old["pose"]))
                if errors[0] > 0.05 or errors[1] > 1e-5 or errors[2] > 1e-5:
                    issues.append("Accepted assembly pose changed: " + expected["label"])
                if fingerprint_new != old["fingerprint"] or expected["level"] != old["level"]:
                    issues.append("Accepted assembly content changed: " + expected["label"])
            corridors = zone.get("corridors", [zone["corridor"]] if "corridor" in zone else [])
            checks = [corridor_boxes(owners[expected["label"]], corridor) for corridor in corridors
                      if not (corridor.get("check_owners") == "new_only" and expected["label"] in locked)]
            if any(not check["pass"] for check in checks):
                issues.append("Assembly overlaps protected boxes: " + expected["label"])
            additions.append({"label": expected["label"], "pose": layout.local_pose(actor.get_actor_transform()),
                              "fingerprint": fingerprint_new, "colliding": colliding,
                              "corridor": checks[0] if checks else {}, "corridors": checks,
                              "previously_accepted": expected["label"] in locked})
            exports.append((path, zone, expected, actual))
    expected_labels = {row["label"] for row in baseline["level_instances"]}
    expected_labels.update(row["label"] for _, zone in zones for row in zone["level_instances"])
    if set(owners) != expected_labels:
        issues.append("Unexpected level instances")
    counts = {"pods": sum(isinstance(a, unreal.LNPMassSpawnPoint) for a in top),
              "anchors": sum(isinstance(a, unreal.LNPPlacementMarker) for a in top),
              "nodes": sum(a.get_actor_label().startswith("Review_Ridge") for a in top),
              "level_instances": len(owners), "nested_levels": len(nested)}
    if (counts["pods"], counts["anchors"], counts["nodes"]) != (10, 6, 27):
        issues.append("Inherited marker count changed")
    preserved = read(PROJECT / manifest["baseline"]["recovery_manifest"])
    changed = [row["path"] for row in preserved["files"] if sha(PROJECT / row["path"]) != row["sha256"]]
    if changed:
        issues.append("Preserved files changed")
    report = {"pass": not issues, "issues": issues, "level": expected_level, "counts": counts,
              "actors": actors, "inherited_fingerprint": fingerprint, "additions": additions,
              "preserved_files_changed": changed, "dirty": layout.test.dirty(), "exported": False}
    if export and not issues:
        for path, zone, expected, actual in exports:
            expected.update(actual)
            with path.open("w", encoding="utf-8") as stream:
                json.dump(zone, stream, ensure_ascii=False, indent=1)
                stream.write("\n")
        report["exported"] = True
    evidence("Export" if export else "Verify", report)
    unreal.log("RIDGE_LARGE_LAYOUT_CHECK %s %s" % ("PASS" if not issues else "REVIEW", json.dumps(issues)))


def build(version):
    """채택된 이전 맵을 새 맵으로 복제하고 현재 구역 JSON의 새 소유자만 더한다."""
    manifest, zones = inputs()
    assert not layout.test.dirty(), layout.test.dirty()
    target = manifest["work_level_pattern"].format(version)
    assert not unreal.EditorAssetLibrary.does_asset_exist(target), "Use a new version: " + target
    previous = manifest.get("previous_unit", {})
    source = previous.get("level", manifest["baseline"]["level"])
    assert unreal.EditorAssetLibrary.duplicate_asset(source, target), target
    assert unreal.EditorAssetLibrary.save_asset(target, False), target
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(target), target
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for _, zone in zones:
        for row in zone["level_instances"]:
            if row["label"] in previous.get("locked_owners", {}):
                continue
            transform = layout.pose_transform(row)
            actor = actors.spawn_actor_from_class(unreal.LevelInstance, transform.translation, transform.rotation.rotator())
            actor.set_actor_label(row["label"])
            actor.set_actor_scale3d(transform.scale3d)
            actor.set_world_asset(unreal.load_asset(row["level"]))
            actor.load_level_instance()
            actor.set_folder_path("EDTerrain/" + zone["zone"])
    assert unreal.EditorLoadingAndSavingUtils.save_map(layout.test.world(), target), target
    evidence("Build", {"level": target, "baseline": manifest["baseline"], "zones": manifest["zones"]})
    unreal.log("RIDGE_LARGE_LAYOUT_BUILD_READY; run verify in the next command after levels load")


if __name__ == "__main__":
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first"
    command, version = sys.argv[1:]
    AUTHORING = HERE / ("LargeLayout_" + version)
    MANIFEST = AUTHORING / "RidgeLayout.json"
    OUT = PROJECT / read(MANIFEST).get("evidence_folder", "Saved/Profiling/Graphics/Phase01/U4_1_V001")
    if command == "build":
        build(version)
    else:
        assert command in ("verify", "export"), command
        inspect(version, export=command == "export")
