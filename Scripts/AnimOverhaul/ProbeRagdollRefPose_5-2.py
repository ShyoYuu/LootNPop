# 5-2 ⓓ 읽기 전용. 랙돌 중인 UAF 시험 적 VisualMesh의 현재 본(컴포넌트 공간)이 참조 포즈와 얼마나 다른지 잰다.
# 편차가 0에 가까우면 본 버퍼 자체가 참조 포즈다(물리 블렌드가 안 들어감). 호스트·게스트를 함께 비교한다.
import json
import os
import time

import unreal

OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "AnimationOverhaul", "2026-11-03_UAF52EnemyPath")
BONES = ("pelvis", "spine_03", "head", "upperarm_r", "lowerarm_r", "hand_r", "upperarm_l", "hand_l", "calf_r", "foot_r", "foot_l")


def ref_component_transform(comp, bone):
    # 참조 포즈 로컬 변환을 부모 쪽으로 누적해 컴포넌트 공간을 만든다.
    result = unreal.Transform()
    name = unreal.Name(bone)
    while str(name) != "None":
        index = comp.get_bone_index(name)
        local = comp.get_ref_pose_transform(index)
        result = result.multiply(local) if False else unreal.MathLibrary.compose_transforms(result, local)
        name = comp.get_parent_bone(name)
    return result


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    result = []
    for actor in unreal.ObjectIterator(unreal.Actor):
        path = actor.get_path_name()
        if "BP_EnemyUAFSpike_C" not in actor.get_name() or "UEDPIE" not in path:
            continue
        comps = [c for c in actor.get_components_by_class(unreal.SkeletalMeshComponent) if c.get_name() == "VisualMesh"]
        if not comps or str(comps[0].get_collision_profile_name()) != "Ragdoll":
            continue
        comp = comps[0]
        pelvis_now = comp.get_socket_transform("pelvis", unreal.RelativeTransformSpace.RTS_COMPONENT).translation
        pelvis_ref = ref_component_transform(comp, "pelvis").translation
        bones = {}
        for bone in BONES:
            now = comp.get_socket_transform(bone, unreal.RelativeTransformSpace.RTS_COMPONENT).translation - pelvis_now
            ref = ref_component_transform(comp, bone).translation - pelvis_ref
            bones[bone] = {"now": [round(now.x), round(now.y), round(now.z)], "ref": [round(ref.x), round(ref.y), round(ref.z)],
                           "deltaCm": round((now - ref).length(), 1)}
        result.append({"actor": path, "world": "guest" if "UEDPIE_1" in path else "host",
                       "maxDeltaCm": max(b["deltaCm"] for b in bones.values()), "bones": bones})
    out_path = os.path.join(OUT_DIR, "RefPoseProbe_%s.json" % time.strftime("%H%M%S"))
    with open(out_path, "w", encoding="utf-8") as file:
        json.dump(result, file, ensure_ascii=False, indent=1)
    unreal.log("[UAFRefPoseProbe] actors=%d out=%s" % (len(result), out_path))


main()
