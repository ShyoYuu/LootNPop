"""1-B5c 대표 구역 밀도·원거리 확인: 규칙 간격·PCG 시드를 메모리에서만 바꿔 다시 생성하고 검사한다.

콘솔: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/ExploreRidgeDecorationDensity.py" <모드> [인자]
  state: 현재 레벨·PIE·dirty 패키지만 기록한다(어느 레벨에서나).
  preflight: WIP·PIE 꺼짐·dirty 없음을 확인하고 원래 역할 간격/확률·시드·보호 파일 해시·A/B/C 시점을 기록한다.
  variant <배율> <시드>: Preflight.json의 원래 간격 × 배율과 시드로 다시 생성한다. 저장하지 않는다.
  inspect <이름>: 생성 결과를 읽기 전용으로 기록한다(재생성과 다른 호출에서 실행해야 정리 대기 컴포넌트를 읽지 않는다).
  restore: Preflight.json의 원래 간격·시드로 되돌려 다시 생성한다. 저장하지 않는다.
  hashes <이름>: 보호 파일 해시만 다시 기록한다.
간격 배율은 모든 역할에 같게 적용하고 확률·보호 규칙·경사·스케일은 바꾸지 않는다. 기존 빌더(BuildRidgeDecorationPCG)는 실행하지 않는다.
WIP·규칙 에셋은 이 스크립트가 저장하지 않는다. 결과는 Saved 아래 JSON으로 남긴다.
"""
import hashlib
import json
from pathlib import Path
import sys

import unreal

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(HERE))
import InspectRidgeDecorationPCG as insp
from BuildRidgeDecorationPCG import ACTOR_LABEL, LEVEL, RULES, SEED

OUT = ROOT / "Saved/Profiling/Graphics/Phase01/RidgeDecorationDensity_20261116"
RULES_PACKAGE = RULES
# 시험 중 dirty가 허용되는 패키지: WIP 맵(PCG 시드·생성 결과)과 규칙 에셋(간격)뿐이다.
ALLOWED_DIRTY = (LEVEL, RULES_PACKAGE)
PROTECTED = [
    "Content/Maps/_WorkInProgress/Ridge_00/Graphics/V001/LVI_Ridge_SurfaceProbe.umap",
    "Content/Maps/_WorkInProgress/Ridge_00/Graphics/V001/DA_OctantSurface_LVI_Ridge_SurfaceProbe.uasset",
    "Content/Graphics/Environment/ElectricDreams/RidgeDecoration_V001/DA_RidgeDecorationRules.uasset",
    "Content/Graphics/Environment/ElectricDreams/RidgeDecoration_V001/PCG_RidgeDecoration_V002.uasset",
    "Content/Maps/OctantPoolData.uasset",
    "Config/DefaultGame.ini",
]
VIEWPOINTS = HERE / "RidgeViewpoints.json"


def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(value, indent=2, ensure_ascii=False), encoding="utf-8")


def read(name):
    return json.loads((OUT / name).read_text(encoding="utf-8"))


def dirty_packages():
    return [p.get_path_name() for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) +
            list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())]


def current_level():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    return world.get_path_name().split(".")[0] if world else None


def in_pie():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()


def file_hashes():
    result = {}
    for rel in PROTECTED:
        path = ROOT / rel
        result[rel] = hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None
    return result


def require_wip(allow_dirty):
    assert not in_pie(), "Stop PIE first."
    assert current_level() == LEVEL, "Open the existing WIP level first."
    dirty = [p for p in dirty_packages() if not (allow_dirty and p in ALLOWED_DIRTY)]
    assert not dirty, "Review existing unsaved changes first: %s" % dirty


def pcg_component():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    pcg = [a for a in actors if a.get_actor_label() == ACTOR_LABEL]
    assert len(pcg) == 1, "Expected the existing PCG actor."
    return pcg[0].get_component_by_class(unreal.PCGComponent)


def role_values(rules):
    return [{"id": str(r.get_editor_property("id")), "spacing": r.get_editor_property("spacing"),
             "probability": r.get_editor_property("probability")} for r in rules.get_editor_property("roles")]


def view_poses():
    """RidgeViewpoints.json(옥탄트 로컬 = WIP 에디터 좌표)의 쿼터니언을 MCP 카메라용 Rotator로 바꾼다."""
    data = json.loads(VIEWPOINTS.read_text(encoding="utf-8"))
    poses = {}
    for p in data["points"]:
        x, y, z, w = p["camera"]["rotation_quaternion_xyzw"]
        r = unreal.Quat(x, y, z, w).rotator()
        loc = p["camera"]["location"]
        poses[p["id"]] = {"location": {"x": loc[0], "y": loc[1], "z": loc[2]},
                          "rotation": {"pitch": r.pitch, "yaw": r.yaw, "roll": r.roll}}
    return {"source": str(VIEWPOINTS.relative_to(ROOT)), "lens_fov": data["lens"]["fieldOfView"], "poses": poses}


def state():
    write("State_%s.json" % (sys.argv[2] if len(sys.argv) > 2 else "now"),
          {"level": current_level(), "pie": in_pie(), "dirty": dirty_packages()})


def preflight():
    require_wip(allow_dirty=False)
    assert not (OUT / "Preflight.json").exists(), "Preflight already recorded; originals must not be overwritten."
    rules = unreal.load_asset(RULES)
    component = pcg_component()
    seed = component.get_editor_property("seed")
    assert seed == SEED, "Unexpected PCG seed %s" % seed
    graph = component.get_graph()
    write("Preflight.json", {"level": LEVEL, "rules": RULES, "graph": graph.get_path_name() if graph else None,
                             "seed": seed, "roles": role_values(rules), "hashes": file_hashes()})
    write("ViewPoses.json", view_poses())
    unreal.log("Ridge decoration density preflight recorded.")


def apply(scale, seed):
    original = read("Preflight.json")
    by_id = {r["id"]: r for r in original["roles"]}
    rules = unreal.load_asset(RULES)
    roles = list(rules.get_editor_property("roles"))
    applied = []
    for role in roles:
        base = by_id[str(role.get_editor_property("id"))]
        # 확률은 원래 값 그대로 둔다(간격만 바꾼다).
        assert abs(role.get_editor_property("probability") - base["probability"]) < 1e-6
        role.set_editor_property("spacing", float(base["spacing"]) * scale)
        applied.append({"id": base["id"], "spacing": float(base["spacing"]) * scale})
    rules.set_editor_property("roles", roles)
    assert [round(r["spacing"], 3) for r in role_values(rules)] == [round(a["spacing"], 3) for a in applied]
    component = pcg_component()
    component.set_editor_property("seed", int(seed))
    assert component.get_editor_property("seed") == int(seed)
    component.generate(True)
    return applied


def variant():
    require_wip(allow_dirty=True)
    scale, seed = float(sys.argv[2]), int(sys.argv[3])
    applied = apply(scale, seed)
    write("Variant_s%s_seed%d.json" % (("%.2f" % scale).replace(".", "p"), seed),
          {"scale": scale, "seed": seed, "roles": applied, "dirty_after": dirty_packages()})
    unreal.log("Ridge decoration density variant scale=%.2f seed=%d requested; inspect in a separate call." % (scale, seed))


def restore():
    require_wip(allow_dirty=True)
    original = read("Preflight.json")
    applied = apply(1.0, original["seed"])
    write("Restore.json", {"seed": original["seed"], "roles": applied, "dirty_after": dirty_packages()})
    unreal.log("Ridge decoration density restored to preflight values; inspect in a separate call.")


def inspect():
    require_wip(allow_dirty=True)
    name = sys.argv[2]
    # 기존 검사 스크립트의 출력 위치만 이 단위 폴더로 바꾼다(검사 내용은 같다).
    insp.write = write
    insp.inspect("Inspection_%s.json" % name)
    data = read("Inspection_%s.json" % name)
    data["roles_now"] = role_values(unreal.load_asset(RULES))
    write("Inspection_%s.json" % name, data)


def hashes():
    write("Hashes_%s.json" % sys.argv[2], {"hashes": file_hashes(), "dirty": dirty_packages()})


if __name__ == "__main__":
    {"state": state, "preflight": preflight, "variant": variant, "inspect": inspect, "restore": restore,
     "hashes": hashes}[sys.argv[1]]()
