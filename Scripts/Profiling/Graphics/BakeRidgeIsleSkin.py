"""IsleSkin 충돌 맵(IsleSkin_V008, GX-041)을 Support Atlas로 굽고 검사용 풀을 만든다(3c-1).

BuildRidgeZ1BakeLVI.py(U6)의 bake·pool과 같은 절차를 IsleSkin LVI 경로로 쓴다. 비교 기준은 같은 충돌 5개·같은 마커로
구운 Z1Bake_V010이다(V007은 굽지 않는다. 그 폴더에 SurfaceData가 생기기 때문이다).

실행: 에디터 상태 표시줄 Cmd에 py "<이 파일>" <명령> [인자]
- open <레벨 경로>: 미저장 변경이 없을 때 그 레벨을 연다.
- dump <이름>: (open 다음 Cmd) 열린 LVI 최상위 액터의 클래스·라벨·자세와 충돌 액터의 메시·프로필·태그를 Top_<이름>.json에 쓴다(읽기 전용).
- bake <판>: TestMap03을 열고 `LNP.SurfaceNav.BakeOctant <IsleSkin_<판> LVI>`로 굽는다. 수치·이음매·Pod를 Bake_<판>.json에,
  스폰 후보를 Candidates_<판>.json에 쓰고 Z1Bake_V010(Bake_V010.json)과 비교한다.
- pool <판>: 정식 풀을 복제해 Ridge 정의 하나만 IsleSkin_<판> LVI·SurfaceData로 바꾼 검사용 풀을 만든다(정식 풀은 그대로).
결과: Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Bake/
검사용 풀의 임시 선택·복원은 BuildRidgeZ1BakeLVI.py 머리말과 같이 MCP ObjectTools로 한다(SaveConfig 안 함).
"""
import hashlib
import importlib
import json
import math
from pathlib import Path
import re
import struct
import sys

import unreal

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parents[1])]
import BuildRidgeZ1BakeLVI as z1bake  # noqa: E402  (spawn_candidates·last_bake_log·content_file 재사용)

importlib.reload(z1bake)
spec = z1bake.spec

ROOT = HERE.parents[2]
OUT = ROOT / "Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Bake"
EDT = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/"
FOLDER = EDT + "IsleSkin_{0}/"
LEVEL = FOLDER + "LVI_Ridge_EDTerrain_IsleSkin_{0}"
SURFACE = FOLDER + "DA_OctantSurface_LVI_Ridge_EDTerrain_IsleSkin_{0}"
POOL = FOLDER + "DA_GraphicsRidgeIsleSkinPool_{0}"
BASELINE = z1bake.OUT / "Bake_V010.json"


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def package(obj):
    return obj.get_path_name().split(".")[0] if obj else None


def dirty():
    saving = unreal.EditorLoadingAndSavingUtils
    return [p.get_path_name() for p in list(saving.get_dirty_map_packages()) + list(saving.get_dirty_content_packages())]


def pie():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()


def write(name, data):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(data, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")


def open_level(path):
    assert not pie()
    assert not dirty(), dirty()
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(path), path
    unreal.log("ISLESKIN_BAKE_OPEN " + path)


def top_actors():
    prefix = world().get_path_name() + ":PersistentLevel."
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor)
            if a.get_path_name().startswith(prefix)]


def dump(name):
    rows = []
    for actor in top_actors():
        loc, rot = actor.get_actor_location(), actor.get_actor_rotation()
        row = {"class": actor.get_class().get_name(), "label": actor.get_actor_label(),
               "location": [round(loc.x, 3), round(loc.y, 3), round(loc.z, 3)],
               "rotation": [round(rot.pitch, 4), round(rot.yaw, 4), round(rot.roll, 4)]}
        if isinstance(actor, unreal.StaticMeshActor):
            comp = actor.static_mesh_component
            if comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
                row["collision"] = {"mesh": package(comp.static_mesh), "profile": str(comp.get_collision_profile_name()),
                                    "enabled": str(comp.get_collision_enabled()), "visible": comp.is_visible(),
                                    "tags": sorted(str(t) for t in comp.get_editor_property("component_tags"))}
        if isinstance(actor, unreal.LevelInstance):
            asset = actor.get_world_asset()
            row["world_asset"] = package(asset) if asset else None
        rows.append(row)
    rows.sort(key=lambda r: (r["class"], r["label"]))
    write("Top_%s.json" % name, {"level": package(world()), "pie": pie(), "dirty": dirty(), "actors": rows})
    unreal.log("ISLESKIN_BAKE_DUMP %s %d" % (name, len(rows)))


def bake(version):
    level, surface = LEVEL.format(version), SURFACE.format(version)
    assert not pie()
    assert not dirty(), dirty()
    pods_local = {}
    top = json.loads((OUT / ("Top_IsleSkin_%s.json" % version)).read_text(encoding="utf-8"))
    assert top["level"] == level, top["level"]
    for row in top["actors"]:
        if row["class"] == "LNPMassSpawnPoint":
            pods_local[row["label"].replace("Spawn_", "")] = spec.ridge.local(tuple(row["location"]))
    z1bake.open_level(z1bake.TEST_MAP)
    surface_file = z1bake.content_file(surface, ".uasset")
    prior = surface_file.stat().st_mtime_ns if surface_file.exists() else None
    unreal.SystemLibrary.execute_console_command(world(), "LNP.SurfaceNav.BakeOctant " + level)
    lines = z1bake.last_bake_log(level)
    if not surface_file.exists() or surface_file.stat().st_mtime_ns == prior:
        write("Bake_%s.json" % version, {"level": level, "failed": True, "log": lines})
        unreal.log_error("ISLESKIN_BAKE_FAILED " + " | ".join(lines))
        return
    data = unreal.load_asset(surface)
    header = data.get_editor_property("header")
    packages = sorted(str(r.get_editor_property("package_name")) for r in header.get_editor_property("source_manifest"))
    from InspectAcropolisBake import seams
    seam_match = seams(z1bake.MEADOW_SURFACE) == seams(surface)
    support = bytes(data.get_editor_property("support_payload"))
    _, layers = struct.unpack_from("<HH", support)
    _, anchors, candidates = z1bake.spawn_candidates(bytes(data.get_editor_property("spawn_payload")))
    pods = []
    for anchor in anchors:
        ax, ay, az = anchor["location"]
        lx, ly, lh = spec.ridge.local((ax, ay, az))
        pid = min(pods_local, key=lambda k: math.hypot(pods_local[k][0] - lx, pods_local[k][1] - ly))
        radius = max(f[3] for f in anchor["formations"]) if anchor["formations"] else 0
        same = [c for c in candidates if c[3] == anchor["layer"]]
        near = lambda r: sum(1 for c in same if math.dist(c[:3], (ax, ay, az)) <= r)
        pods.append({"id": pid, "local": [round(lx), round(ly), round(lh)], "layer": anchor["layer"],
                     "edge_cm": round(anchor["edge"]), "headroom_cm": round(anchor["headroom"]),
                     "formation_count": sum(f[0] for f in anchor["formations"]), "formation_radius_cm": radius,
                     "same_layer_in_1500": near(1500), "same_layer_in_radius": near(radius),
                     "layer_candidates": len(same)})
    streams = {name: header.get_editor_property(name).get_editor_property("element_count")
               for name in ("support", "navigation", "traversal", "spawn", "conditional_patch")}
    summary = lines[0].split(" | ", 1)[-1]
    numbers = {key: int(value) for key, value in re.findall(r"(components|portals|cells|authored|noNavRejected)=(\d+)",
                                                              summary)}
    base = json.loads(BASELINE.read_text(encoding="utf-8"))
    base_pods = {p["id"]: p for p in base["pods"]}
    compare = {"support_layers": [base["support_layers"], layers],
               "spawn_candidates": [base["spawn_candidates"], len(candidates)],
               "summary": {k: [base["summary"].get(k), numbers.get(k)] for k in numbers},
               "streams": {k: [base["streams"].get(k), streams[k]] for k in streams},
               "pods": {p["id"]: {k: [base_pods.get(p["id"], {}).get(k), p[k]]
                                  for k in ("edge_cm", "headroom_cm", "layer_candidates", "same_layer_in_radius")}
                        for p in pods}}
    report = {"level": level, "surface_data": surface, "data_version": header.get_editor_property("data_version"),
              "support_layers": layers, "streams": streams, "seam_matches_meadow": seam_match,
              "spawn_candidates": len(candidates), "pods": pods, "summary": numbers,
              "source_packages": packages, "baseline": str(BASELINE.relative_to(ROOT)).replace("\\", "/"),
              "compare_to_baseline": compare, "log": lines, "dirty_after": dirty()}
    write("Bake_%s.json" % version, report)
    rows = []
    for c in candidates:
        lx, ly, lh = spec.ridge.local(c[:3])
        rows.append({"x": round(lx, 1), "y": round(ly, 1), "h": round(lh, 1), "layer": c[3], "edge": round(c[4], 1),
                     "capsule": round(c[5], 1), "headroom": round(c[6], 1), "slope": round(c[7], 3)})
    write("Candidates_%s.json" % version, {"surface_data": surface, "candidates": rows})
    unreal.log("ISLESKIN_BAKE_READY layers=%d seam=%s %s" % (layers, seam_match, json.dumps(numbers)))


def pool(version):
    level, surface, path = LEVEL.format(version), SURFACE.format(version), POOL.format(version)
    assert not pie()
    production = z1bake.content_file(z1bake.PRODUCTION_POOL, ".uasset")
    before = hashlib.sha256(production.read_bytes()).hexdigest()
    source = unreal.load_asset(z1bake.PRODUCTION_POOL)
    definitions = list(source.get_editor_property("octant_definitions"))
    ridge = [i for i, d in enumerate(definitions)
             if "/Ridge_00/" in d.get_editor_property("level_asset").get_path_name()]
    assert len(ridge) == 1, ridge
    definition = definitions[ridge[0]]
    definition.set_editor_property("level_asset", unreal.load_asset(level))
    definition.set_editor_property("surface_data", unreal.load_asset(surface))
    assert not unreal.EditorAssetLibrary.does_asset_exist(path), "Use a new version: " + path
    target = unreal.EditorAssetLibrary.duplicate_asset(z1bake.PRODUCTION_POOL, path)
    assert target
    target.set_editor_property("octant_definitions", definitions)
    assert unreal.EditorAssetLibrary.save_loaded_asset(target, only_if_is_dirty=False)
    # 구조체 배열은 사본이라 원본 풀은 바뀌지 않아야 한다. 다시 불러 확인한다.
    check = list(unreal.load_asset(z1bake.PRODUCTION_POOL).get_editor_property("octant_definitions"))[ridge[0]]
    assert "/Ridge_00/" in check.get_editor_property("level_asset").get_path_name()
    assert not any(z1bake.PRODUCTION_POOL in p for p in dirty()), dirty()
    after = hashlib.sha256(production.read_bytes()).hexdigest()
    assert after == before
    saved = list(unreal.load_asset(path).get_editor_property("octant_definitions"))[ridge[0]]
    write("Pool_%s.json" % version, {"pool": path, "ridge_index": ridge[0], "definitions": len(definitions),
                                     "level": saved.get_editor_property("level_asset").get_path_name(),
                                     "surface_data": saved.get_editor_property("surface_data").get_path_name(),
                                     "production_sha256": before, "dirty": dirty()})
    unreal.log("ISLESKIN_BAKE_POOL_READY " + path)


if __name__ == "__main__":
    command, *args = sys.argv[1:]
    {"open": open_level, "dump": dump, "bake": bake, "pool": pool}[command](*args)
