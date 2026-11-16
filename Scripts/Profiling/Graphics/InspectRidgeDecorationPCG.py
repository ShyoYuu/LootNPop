"""1-B5a/1-B5b PCG 생성 결과를 읽기 전용으로 검사해 Saved JSON으로 남긴다.

콘솔: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/InspectRidgeDecorationPCG.py" [이름]
PCG 액터가 만든 ISM 컴포넌트의 계약(태그·충돌·내비·Static)과 인스턴스 변환, 그래프, 승격 대상 수동 바위의 계약을 기록한다.
맵·에셋을 수정하거나 저장하지 않는다.
"""
import json
import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from BuildRidgeDecorationPCG import ACTOR_LABEL, LEVEL, OUT, component_state, manual_blocker_actors, write


def xyz(v):
    return [v.x, v.y, v.z]


def inspect(name):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name().split(".")[0] == LEVEL, "Open the existing WIP level first."
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    pcg = [a for a in actors if a.get_actor_label() == ACTOR_LABEL]
    result = {"pcg_actor_count": len(pcg), "components": [],
              "dirty": [p.get_path_name() for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) +
                        list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())]}
    for actor in pcg:
        pcg_component = actor.get_component_by_class(unreal.PCGComponent)
        graph = pcg_component.get_graph()
        result["pcg"] = {"graph": graph.get_path_name() if graph else None,
                         "seed": pcg_component.get_editor_property("seed"),
                         "trigger": str(pcg_component.get_editor_property("generation_trigger")),
                         "generated": pcg_component.get_editor_property("generated")}
        brush = actor.get_component_by_class(unreal.BrushComponent)
        if brush:
            result["volume_brush"] = {"profile": str(brush.get_collision_profile_name()),
                                      "collision": str(brush.get_collision_enabled())}
        for c in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
            instances = []
            for i in range(c.get_instance_count()):
                t = c.get_instance_transform(i, True)
                r = t.rotation.rotator()
                instances.append({"location": xyz(t.translation), "rotation": [r.pitch, r.yaw, r.roll],
                                  "scale": xyz(t.scale3d)})
            result["components"].append({
                "name": c.get_name(), "mesh": c.static_mesh.get_path_name() if c.static_mesh else None,
                "materials": [m.get_path_name() if m else None for m in c.get_materials()],
                "tags": [str(t) for t in c.component_tags], "profile": str(c.get_collision_profile_name()),
                "collision": str(c.get_collision_enabled()), "mobility": str(c.mobility),
                "affects_navigation": c.get_editor_property("can_ever_affect_navigation"),
                "evaluate_wpo": c.get_editor_property("evaluate_world_position_offset"),
                "instances": instances})
    result["counts"] = {c["mesh"].split(".")[-1]: len(c["instances"]) for c in result["components"] if c["mesh"]}
    result["manual_rocks"] = {label: component_state(actor.static_mesh_component)
                              for label, actor in manual_blocker_actors().items()}
    write(name, result)
    unreal.log("Ridge decoration PCG inspection written: %s" % result["counts"])


if __name__ == "__main__":
    inspect(sys.argv[1] if len(sys.argv) > 1 else "Inspection.json")
