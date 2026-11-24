"""U4-0 읽기 전용 조사. 열린 V010의 자세·Cull 수·미저장 패키지를 새 보고서에 기록한다.

UE Cmd: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/InspectRidgeLargeLayout.py"
레벨 로드·저장·배치·Cull·베이크는 수행하지 않는다. 기존 보고서도 덮어쓰지 않는다.
"""
import datetime
import json
from pathlib import Path
import sys

import unreal

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parents[1])]
import RidgeZ1Layout as layout

PROJECT = HERE.parents[2]
LEVEL = layout.EDTERRAIN + "Z1Bake_V010/LVI_Ridge_EDTerrain_Z1Bake_V010"
OUT = PROJECT / "Saved/Profiling/Graphics/Phase01/U4_0_V001"


def inspect():
    """보존 기준과 현재 메모리 상태를 대조하고 로컬 근거만 추가한다."""
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    data = json.loads(layout.LAYOUT.read_text(encoding="utf-8"))
    top, nested = layout.level_actors()
    world = layout.test.world()
    dirty = layout.test.dirty()
    report = {"level": layout.world_package(), "expected_level": LEVEL,
              "layout_version": data["version"], "dirty": dirty, "issues": [],
              "actors": [], "level_instances": [], "counts": {}}
    loc, rot = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_level_viewport_camera_info()
    report["camera"] = {"location": layout.test.xyz(loc),
                        "rotation": [rot.pitch, rot.yaw, rot.roll]}
    if report["level"] != LEVEL:
        report["issues"].append("unexpected level; open preserved V010 before inspection")
    else:
        by_label = {actor.get_actor_label(): actor for actor in top}
        for row in data["actors"]:
            actor = by_label.get(row["label"])
            entry = {"label": row["label"], "role": row["role"]}
            if actor is None:
                report["issues"].append("missing actor " + row["label"])
            else:
                comp = actor.static_mesh_component
                errors = layout.pose_error(actor.get_actor_transform(), layout.pose_transform(row))
                entry.update({"pose": layout.local_pose(actor.get_actor_transform()),
                              "pose_error": errors, "mesh": layout.test.package(comp.static_mesh),
                              "profile": str(comp.get_collision_profile_name())})
                if errors[0] > 0.05 or errors[1] > 1e-5 or errors[2] > 1e-5:
                    report["issues"].append("actor pose mismatch " + row["label"])
                if entry["mesh"] != row["mesh"]:
                    report["issues"].append("actor mesh mismatch " + row["label"])
            report["actors"].append(entry)
        for row in data["level_instances"]:
            actor = by_label.get(row["label"])
            entry = {"label": row["label"], "role": row["role"]}
            if actor is None:
                report["issues"].append("missing level instance " + row["label"])
            else:
                source = row["cull"]["level"]
                entry.update({"level": layout.test.package(actor.get_world_asset()),
                              "pose": layout.local_pose(actor.get_actor_transform()),
                              "pose_error": layout.pose_error(actor.get_actor_transform(), unreal.Transform())})
                if entry["level"] != source or max(entry["pose_error"]) > 1e-5:
                    report["issues"].append("level instance source/pose mismatch " + row["label"])
                loaded = actor.get_loaded_level()
                children = nested.get(loaded.get_path_name(), []) if loaded else []
                counts = {}
                for child in children:
                    for comp in child.get_components_by_class(unreal.StaticMeshComponent):
                        if comp.static_mesh:
                            key = layout.test.package(comp.static_mesh)
                            count = comp.get_instance_count() if isinstance(comp, unreal.InstancedStaticMeshComponent) else 1
                            counts[key] = counts.get(key, 0) + count
                entry["mesh_counts"] = dict(sorted(counts.items()))
                entry["instances"] = sum(counts.values())
                entry["expected_instances"] = row["cull"]["instances"]
                if entry["instances"] != entry["expected_instances"]:
                    report["issues"].append("instance count mismatch " + row["label"])
            report["level_instances"].append(entry)
        report["counts"] = {"pods": sum(isinstance(a, unreal.LNPMassSpawnPoint) for a in top),
                            "anchors": sum(isinstance(a, unreal.LNPPlacementMarker) for a in top),
                            "nodes": sum(a.get_actor_label().startswith("Review_Ridge") for a in top),
                            "level_instances": sum(isinstance(a, unreal.LevelInstance) for a in top),
                            "nested_levels": len(nested)}
        if report["counts"] != {"pods": 10, "anchors": 6, "nodes": 27, "level_instances": 15, "nested_levels": 15}:
            report["issues"].append("unexpected marker/level counts")
        # 생성·재생성 없이 기존 읽기 전용 지문 함수를 재사용한다.
        report["visual_fingerprint"], report["visual_detail"] = layout.fingerprint(
            [a for group in nested.values() for a in group])
    report["dirty_after"] = layout.test.dirty()
    if dirty != report["dirty_after"]:
        report["issues"].append("dirty package set changed during inspection")
    report["pass"] = not report["issues"] and not dirty
    OUT.mkdir(parents=True, exist_ok=True)
    target = OUT / ("LiveSnapshot_" + stamp + ".json")
    with target.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=1)
    unreal.log("RIDGE_U4_0 %s %s" % ("PASS" if report["pass"] else "REVIEW", target))


if __name__ == "__main__":
    inspect()
