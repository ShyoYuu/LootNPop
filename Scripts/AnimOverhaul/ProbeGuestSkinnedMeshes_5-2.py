# 5-2 ⓓ 읽기 전용. 게스트 T자 시체의 정체를 찾기 위해, PIE 월드별로 보이는 모든 스킨드 메시 컴포넌트를
# 소유 Actor와 상관없이 나열한다(ISKM 인스턴스 수 포함). 플레이어 카메라 기준 거리도 함께 남긴다.
# 결과: Saved/AnimationOverhaul/2026-11-03_UAF52EnemyPath/SkinnedProbe_<시각>.json
import json
import os
import time

import unreal

OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "AnimationOverhaul", "2026-11-03_UAF52EnemyPath")


def world_of(path):
    return "guest" if "UEDPIE_1" in path else "host" if "UEDPIE_0" in path else None


def vector(value):
    return [round(value.x), round(value.y), round(value.z)]


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    cameras = {}
    for controller in unreal.ObjectIterator(unreal.PlayerController):
        world = world_of(controller.get_path_name())
        manager = controller.get_editor_property("player_camera_manager")
        if world and manager and controller.is_local_controller():
            cameras[world] = manager.get_camera_location()
    result = []
    for comp in unreal.ObjectIterator(unreal.SkinnedMeshComponent):
        path = comp.get_path_name()
        world = world_of(path)
        if world is None or not comp.is_visible() or comp.get_editor_property("hidden_in_game"):
            continue
        owner = comp.get_owner()
        location = comp.get_world_location()
        entry = {"world": world, "component": comp.get_name(), "class": comp.get_class().get_name(),
                 "owner": owner.get_name() if owner else None,
                 "ownerClass": owner.get_class().get_name() if owner else None,
                 "location": vector(location)}
        for prop in ("skeletal_mesh_asset", "skinned_asset"):
            try:
                asset = comp.get_editor_property(prop)
                entry["mesh"] = asset.get_name() if asset else None
                break
            except Exception:
                continue
        if world in cameras:
            entry["cameraDistance"] = round((location - cameras[world]).length())
        if isinstance(comp, unreal.SkeletalMeshComponent):
            entry["enableAnimation"] = comp.get_editor_property("enable_animation")
            entry["animInstance"] = str(comp.get_anim_instance())
            entry["profile"] = str(comp.get_collision_profile_name())
            entry["simulating"] = comp.is_simulating_physics()
        else:
            try:
                entry["instances"] = comp.get_instance_count()
            except Exception as error:
                entry["instances"] = "ERR " + str(error)[:60]
        result.append(entry)
    out_path = os.path.join(OUT_DIR, "SkinnedProbe_%s.json" % time.strftime("%H%M%S"))
    with open(out_path, "w", encoding="utf-8") as file:
        json.dump({"cameras": {k: vector(v) for k, v in cameras.items()}, "components": result},
                  file, ensure_ascii=False, indent=1)
    unreal.log("[UAFSkinnedProbe] components=%d out=%s" % (len(result), out_path))


main()
