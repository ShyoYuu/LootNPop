# 5-2 ⓓ 읽기 전용. PIE 모든 월드(호스트 UEDPIE_0·게스트 UEDPIE_1)에서 랙돌 프로필인 UAF 시험 적의
# VisualMesh 상태(애니 평가·인스턴스·물리 시뮬·블렌드·URO)와 본/바디 위치를 Saved JSON으로 남긴다.
# 게스트 랙돌 T포즈 원인 판별용: 렌더 본(소켓)과 물리 바디가 어긋나는지, 손 간격이 T포즈(약 150cm 이상)인지 본다.
import json
import os
import sys
import time

import unreal

OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "AnimationOverhaul", "2026-11-03_UAF52EnemyPath")
PROPS = ("enable_animation", "b_blend_physics", "visibility_based_anim_tick_option",
         "enable_update_rate_optimizations", "physics_blend_weight", "hidden_in_game", "visible")


def read_props(comp):
    data = {}
    for prop in PROPS:
        try:
            data[prop] = str(comp.get_editor_property(prop))
        except Exception as error:
            data[prop] = "ERR " + str(error)[:60]
    return data


def probe_mesh(comp):
    data = read_props(comp)
    data["animInstance"] = str(comp.get_anim_instance())
    data["collision"] = str(comp.get_collision_enabled())
    data["profile"] = str(comp.get_collision_profile_name())
    data["simulatingRoot"] = comp.is_simulating_physics()
    data["componentLocation"] = str(comp.get_world_location())
    data["attachParent"] = str(comp.get_attach_parent())
    hands = []
    for bone in ("pelvis", "spine_03", "head", "hand_r", "hand_l"):
        socket = comp.get_socket_location(bone)
        item = {"socket": str(socket)}
        try:
            item["simulating"] = comp.is_simulating_physics(bone)
            item["velocity"] = str(comp.get_physics_linear_velocity(bone))
        except Exception as error:
            item["physics"] = "ERR " + str(error)[:60]
        data[bone] = item
        if bone.startswith("hand_"):
            hands.append(socket)
    data["handSpanCm"] = (hands[0] - hands[1]).length()
    return data


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    result = []
    for actor in unreal.ObjectIterator(unreal.Actor):
        path = actor.get_path_name()
        if "BP_EnemyUAFSpike_C" not in actor.get_name() or "UEDPIE" not in path:
            continue
        meshes = {c.get_name(): c for c in actor.get_components_by_class(unreal.SkeletalMeshComponent)}
        visual = meshes.get("VisualMesh")
        if visual is None:
            continue
        # 인자 all이면 랙돌 진입 여부와 상관없이 전부 읽는다(랙돌 진입이 누락된 시체 판별용).
        if "all" not in sys.argv and str(visual.get_collision_profile_name()) != "Ragdoll":
            continue
        entry = {"actor": path, "world": "guest" if "UEDPIE_1" in path else "host",
                 "role": str(actor.get_editor_property("role")),
                 "actorTickEnabled": actor.is_actor_tick_enabled(),
                 "actorLocation": str(actor.get_actor_location()),
                 "VisualMesh": probe_mesh(visual)}
        source = meshes.get("AnimSourceMesh")
        if source is not None:
            entry["AnimSourceMesh"] = read_props(source)
        for comp in actor.get_components_by_class(unreal.ActorComponent):
            if comp.get_class().get_name() == "UAFComponent":
                entry["uafActive"] = comp.is_active()
        result.append(entry)
    out_path = os.path.join(OUT_DIR, "RagdollProbe_%s.json" % time.strftime("%H%M%S"))
    with open(out_path, "w", encoding="utf-8") as file:
        json.dump(result, file, ensure_ascii=False, indent=1)
    unreal.log("[UAFEnemyRagdollProbe] actors=%d out=%s" % (len(result), out_path))


main()
