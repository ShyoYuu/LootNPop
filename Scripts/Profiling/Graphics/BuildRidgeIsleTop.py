"""Phase 1-BR 섬 윗면 단위(2, GX-042): 이전 IsleSkin 맵을 복제한 새 맵에 섬 윗면 변경을 반영한다.

동결 원본(V006·V007 Acceptance의 BuildRidgeIsleSkin.py·GenerateRidgeIsleSkin.py·BuildRidgeIsleBody.py)은 고치지 않는다.
BuildRidgeIsleSkin.import_body·spawn과 PlaceRidgeIsleSkinCollision.import_shell·mesh_check는 읽기 전용으로 불러 같은
반입·배치 설정을 쓴다. 원본 맵(V006~V010)과 공유 원본은 바꾸지 않는다.

판(VERSIONS)
- V010(2a): V009 + 섬 외피 몸체 2개의 메시만 덮개 재분할판(Geometry_V010)으로 교체.
- V011(2b): V010 + 엄폐 각기둥 시각 몸체 2개(섬마다 3개를 한 메시, NoCollision) + 엄폐 Shell 1개(숨김, LNPStaticBlocker).

실행: 에디터 상태 표시줄 Cmd에 py "<이 파일>" <명령> <맵 판>
- build <맵 판>: 필요한 메시를 반입하고 원본 맵을 IsleSkin_<맵 판>으로 복제해 변경을 반영한 뒤 새 맵만 저장한다.
  바꾸기 전 복제본의 최상위 액터 상태를 Top_<판>_Before.json으로 남기고, 저장 뒤 새 맵을 디스크에서 다시 연다.
- verify <맵 판>: (build 다음 Cmd) 바꾼·더한 액터(메시·원점·충돌·경계·재질·태그), 나머지 최상위 액터가 복제 직후와 같은지,
  충돌 컴포넌트 목록, 외피 충돌 0, Shell 역할 계약·메시 검사, 미저장·PIE를 검사한다(읽기 전용).
결과: Saved/Profiling/Graphics/Phase01/IsleSkin_V001/IsleTop/
"""
import importlib
import json
from pathlib import Path
import re
import sys
import time

import unreal

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path[:0] = [str(HERE), str(ROOT / "Scripts")]
import BuildRidgeIsleSkin as skin  # noqa: E402  (import_body·spawn만 읽기 전용으로 쓴다)
import PlaceRidgeIsleSkinCollision as shellio  # noqa: E402  (import_shell·mesh_check·Shell 계약 상수)

importlib.reload(skin)
importlib.reload(shellio)

OUT = ROOT / "Saved/Profiling/Graphics/Phase01/IsleSkin_V001/IsleTop"
SAVED = ROOT / "Saved/Profiling/Graphics/Phase01/IsleSkin_V001"
EDT = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/"
LEVEL = EDT + "IsleSkin_{0}/LVI_Ridge_EDTerrain_IsleSkin_{0}"
BODIES = EDT + "IsleSkin_V001/Bodies_{0}/"
ISLE_SURFACE = EDT + "IsleSkin_V001/Surface_V001/MI_Ridge_IsleSurface"
# 엄폐 덩어리 전용: IsleSurface 부모를 복제해 경사 판정을 0(절벽 무늬)으로 고정한 사본(material 명령).
# 각진 덩어리의 평평한 윗면이 섬 바닥과 같은 월드 정렬 지면 무늬로 그려져 바닥에 녹아 보였기 때문이다.
ISLE_ROCK_FOLDER = EDT + "IsleSkin_V001/Surface_V002/"
ISLE_ROCK = ISLE_ROCK_FOLDER + "MI_Ridge_IsleRock"
ISLE_ROCK_SCOPE = """
return float2(1.0, 0.0);
"""
TAG = "IsleSkinTrial"
TAGS = ["Decoration", "LNPDecoration", "IsleSkinTrial"]  # IsleSkin 외피와 같은 태그
# V009에 있는 충돌(§12 대조): 충돌 지각 V013·Z1 Floor/Shell(V003)·보존 구조물 Floor/Shell + 외피 Shell V002.
V009_COLLISION = ["SM_Octant_Ridge_00_V012_Z1MassifCover_V013_R30000", "SM_Ridge_00_V012_PreservedRocks_Floor",
                  "SM_Ridge_00_V012_PreservedRocks_Shell", "SM_Ridge_EDTerrain_V001_Z1_Floor",
                  "SM_Ridge_EDTerrain_V001_Z1_Shell", "SM_Ridge_IsleSkin_V002_Shell"]

VERSIONS = {
    "V010": {
        "source": "V009", "geometry": "V010",
        # 원본 맵의 몸체 액터 라벨 → Geometry 판의 섬
        "replace": {"BODY_IsleBody_ValleySkyIsle_V005": "ValleySkyIsle", "BODY_IsleBody_WestSkyIsle_V005": "WestSkyIsle"},
        "collision": V009_COLLISION,
        "note": "2a: island skin caps re-tessellated (fan -> concentric rings), cap hidden under PreservedRocks_Floor",
    },
    "V011": {
        "source": "V010", "cover": "V001", "shell": "Cover_V001",
        "collision": V009_COLLISION + ["SM_Ridge_IsleCover_V001_Shell"],
        "note": "2b: V011 island cover pillars restored as closed bodies (NoCollision) + derived Shell (GX-042)",
    },
    # V012: V011 각기둥 외관(규칙적인 띠)을 다듬은 Cover V002. 원본은 V011이 아니라 V010이다.
    "V012": {
        "source": "V010", "cover": "V002", "shell": "Cover_V002",
        "collision": V009_COLLISION + ["SM_Ridge_IsleCover_V002_Shell"],
        "note": "2b: cover pillars V002 (irregular strata, lobes, lean) + derived Shell (GX-042)",
    },
    # V013: 사용자 "더 다듬기"(각진 덩어리·2~3조각 균열·윗단 돌출/아랫단 파임·밑동 잔돌)의 Cover V003. 원본은 V010.
    "V013": {
        "source": "V010", "cover": "V003", "shell": "Cover_V003",
        "collision": V009_COLLISION + ["SM_Ridge_IsleCover_V003_Shell"],
        "note": "2b: faceted split sandstone cover blocks V003 + Shell from main hulls (rubble NoCollision only)",
    },
    # V014: Cover V004 = V003과 같은 형상, 삼각형 감는 방향을 조각 바깥으로 직접 맞춤(V003은 일부 면이 뒤집혀 빠져 보임).
    "V014": {
        "source": "V010", "cover": "V004", "shell": "Cover_V004",
        "collision": V009_COLLISION + ["SM_Ridge_IsleCover_V004_Shell"],
        "note": "2b: Cover V004 (V003 with per-triangle outward winding)",
    },
    # V015: Cover V004 + 엄폐 컴포넌트 재질만 MI_Ridge_IsleRock(전부 절벽 무늬). 메시 에셋 기본 재질은 그대로다.
    "V015": {
        "source": "V010", "cover": "V004", "shell": "Cover_V004", "cover_material": ISLE_ROCK,
        "collision": V009_COLLISION + ["SM_Ridge_IsleCover_V004_Shell"],
        "note": "2b: Cover V004 with rock-only material override (flat tops no longer blend into the island floor)",
    },
    # V016(GX-043): V015와 같은 시각(Cover V004 + IsleRock)에 Shell만 잔돌 포함판(Cover V005)으로 바꾼다.
    "V016": {
        "source": "V010", "cover": "V004", "shell": "Cover_V005", "cover_material": ISLE_ROCK,
        "collision": V009_COLLISION + ["SM_Ridge_IsleCover_V005_Shell"],
        "note": "2b: V015 visuals + Shell including base rubble >= 8cm (user PIE Q06)",
    },
}


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def package(obj):
    return obj.get_path_name().split(".")[0] if obj else None


def dirty():
    saving = unreal.EditorLoadingAndSavingUtils
    return [p.get_path_name() for p in list(saving.get_dirty_map_packages()) + list(saving.get_dirty_content_packages())]


def pie():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()


def stamp():
    return time.strftime("%Y%m%d_%H%M%S")


def write(name, data):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")


def top_actors():
    prefix = world().get_path_name() + ":PersistentLevel."
    return [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
            if a.get_path_name().startswith(prefix)]


def r4(values):
    return [round(v, 4) for v in values]


def world_asset_path(value):
    """Level Instance의 월드 에셋을 패키지 경로로 적는다(str()은 메모리 주소가 섞여 다시 열면 달라진다)."""
    if isinstance(value, str):
        match = re.search(r"'([^']+)'", value)
        return match.group(1).split(".")[0] if match else value
    return package(value)


def actor_state(actor):
    t = actor.get_actor_transform()
    rot = t.rotation.rotator()
    row = {"label": actor.get_actor_label(), "class": actor.get_class().get_name(),
           "location": r4([t.translation.x, t.translation.y, t.translation.z]), "rotation": r4([rot.pitch, rot.yaw, rot.roll]),
           "scale": r4([t.scale3d.x, t.scale3d.y, t.scale3d.z]), "hidden": actor.is_hidden_ed(),
           "tags": sorted(str(x) for x in actor.tags), "folder": str(actor.get_folder_path()), "components": []}
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        row["components"].append({"mesh": package(comp.static_mesh),
                                  "materials": [package(m) for m in comp.get_materials()],
                                  "collision": str(comp.get_collision_enabled()),
                                  "profile": str(comp.get_collision_profile_name()), "visible": comp.is_visible()})
    if isinstance(actor, unreal.LevelInstance):
        row["world_asset"] = world_asset_path(actor.get_editor_property("world_asset"))
    return row


def visual_rows(version):
    """바꾸거나 더할 시각 몸체 행: {키: {label, file, asset, triangles, bounds_cm, material}}."""
    cfg = VERSIONS[version]
    if "geometry" in cfg:
        build = json.loads((SAVED / ("Geometry_%s/Build.json" % cfg["geometry"])).read_text(encoding="utf-8"))
        folder, key = BODIES.format(cfg["geometry"]), "isle"
    else:
        build = json.loads((SAVED / ("Cover_%s/Build.json" % cfg["cover"])).read_text(encoding="utf-8"))
        build["outputs"] = build["visual"]
        folder, key = BODIES.format("Cover_" + cfg["cover"]), "isle"
    assert build["pass"], "Blender build did not pass"
    return {o[key]: {"label": "BODY_" + o["name"].replace("SM_Ridge_", ""), "file": o["file"], "asset": folder + o["name"],
                     "triangles": o["triangles"], "bounds_cm": o["bounds_cm"], "material": ISLE_SURFACE}
            for o in build["outputs"]}


def shell_item(version):
    cfg = VERSIONS[version]
    item, = shellio.manifest(cfg["shell"])["exports"]
    return item, shellio.FOLDER.format(cfg["shell"]) + "/" + item["name"]


def spawn_shell(mesh, label):
    """PlaceRidgeIsleSkinCollision.build와 같은 Shell 액터(원점·숨김·Shell 역할 태그·LNPStaticBlocker·Static)."""
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    actor.set_actor_label(label)
    comp = actor.static_mesh_component
    comp.modify()
    comp.set_static_mesh(mesh)
    comp.set_editor_property("component_tags", [unreal.Name(t) for t in shellio.SHELL_TAGS])
    comp.set_editor_property("use_default_collision", False)
    comp.set_collision_profile_name(shellio.SHELL_PROFILE)
    comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    comp.set_mobility(unreal.ComponentMobility.STATIC)
    comp.set_visibility(False)
    actor.set_folder_path("Collision")
    return actor


def build(version):
    assert not pie(), "Stop PIE first"
    assert not dirty(), dirty()
    cfg = VERSIONS[version]
    source, target = LEVEL.format(cfg["source"]), LEVEL.format(version)
    assert not unreal.EditorAssetLibrary.does_asset_exist(target), "Use a new version: " + target
    rows = visual_rows(version)
    meshes = {key: skin.import_body(row) for key, row in rows.items()}
    shell = None
    if "shell" in cfg:
        item, path = shell_item(version)
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            shellio.import_shell(cfg["shell"])
        shell = unreal.load_asset(path)
        assert shell and shellio.mesh_check(shell)["pass"], path
    assert not dirty(), dirty()
    assert unreal.EditorAssetLibrary.duplicate_asset(source, target), target
    assert unreal.EditorAssetLibrary.save_asset(target, False), target
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(target), target
    before = [actor_state(a) for a in top_actors()]
    write("Top_%s_Before.json" % version, {"level": target, "source": source, "actors": before})
    by_label = {a.get_actor_label(): a for a in top_actors()}
    changes = []
    for label, key in cfg.get("replace", {}).items():
        actor = by_label[label]
        comp = actor.static_mesh_component
        comp.modify()
        comp.set_static_mesh(meshes[key])
        actor.set_actor_label(rows[key]["label"])
        changes.append({"replace": label, "to": rows[key]["label"], "mesh": rows[key]["asset"]})
    if "cover" in cfg:
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        identity = unreal.Transform(unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0), unreal.Vector(1, 1, 1))
        override = unreal.load_asset(cfg["cover_material"]) if cfg.get("cover_material") else None
        if cfg.get("cover_material"):
            assert override, cfg["cover_material"]
        for key, row in rows.items():
            actor = skin.spawn(actors, row["label"], meshes[key], identity, TAGS, "EDTerrain/IsleSkin/%s/Cover" % key)
            if override:
                actor.static_mesh_component.set_material(0, override)
            changes.append({"add": row["label"], "mesh": row["asset"], "material": cfg.get("cover_material")})
    if shell:
        spawn_shell(shell, shell_item(version)[0]["name"])
        changes.append({"add": shell_item(version)[0]["name"], "mesh": package(shell)})
    assert unreal.EditorLoadingAndSavingUtils.save_map(world(), target), target
    write("Build_%s_%s.json" % (version, stamp()), {"level": target, "source": source, "changes": changes,
                                                  "config": cfg, "dirty": dirty()})
    # 디스크 저장값으로 검사하려고 새 맵을 다시 연다(Python 충돌 설정이 저장 때 되돌아가는 함정 대비).
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(target), target
    unreal.log("RIDGE_ISLE_TOP_BUILD_READY %s; run verify in the next command" % target)


def check_visual(actor, row, issues):
    comp = actor.static_mesh_component
    label = row["label"]
    if package(comp.static_mesh) != row["asset"]:
        issues.append("mesh " + label)
    if comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION or \
            str(comp.get_collision_profile_name()) != "NoCollision" or comp.get_editor_property("use_default_collision"):
        issues.append("collision " + label)
    if actor.get_actor_location().length() > 0.01:
        issues.append("not at origin " + label)
    if [package(m) for m in comp.get_materials()] != [row["material"]]:
        issues.append("material " + label)
    if comp.static_mesh.get_num_triangles(0) != row["triangles"]:
        issues.append("triangles " + label)
    box = comp.static_mesh.get_bounding_box()
    got = [[box.min.x, box.min.y, box.min.z], [box.max.x, box.max.y, box.max.z]]
    err = max(abs(a - b) for g, e in zip(got, row["bounds_cm"]) for a, b in zip(g, e))
    if err > 0.1:
        issues.append("bounds %s %.4f" % (label, err))
    if TAG not in [str(t) for t in actor.tags]:
        issues.append("tag " + label)


def verify(version):
    cfg = VERSIONS[version]
    target = LEVEL.format(version)
    rows = visual_rows(version)
    renamed = {label: rows[key]["label"] for label, key in cfg.get("replace", {}).items()}
    added = [row["label"] for row in rows.values()] if "cover" in cfg else []
    shell_label = shell_item(version)[0]["name"] if "shell" in cfg else None
    if shell_label:
        added.append(shell_label)
    before = {r["label"]: r for r in json.loads((OUT / ("Top_%s_Before.json" % version)).read_text(encoding="utf-8"))["actors"]}
    for row in before.values():
        if "world_asset" in row:
            row["world_asset"] = world_asset_path(row["world_asset"])
    now = {a.get_actor_label(): a for a in top_actors()}
    issues = []
    # 1) 바꾸거나 더한 시각 몸체
    for row in rows.values():
        actor = now.get(row["label"])
        if not actor:
            issues.append("missing " + row["label"])
            continue
        check_visual(actor, dict(row, material=cfg.get("cover_material") or row["material"]), issues)
    # 2) Shell 계약(PlaceRidgeIsleSkinCollision.verify와 같은 항목)
    shell_ok = None
    if shell_label:
        actor = now.get(shell_label)
        _, path = shell_item(version)
        if not actor:
            issues.append("missing " + shell_label)
        else:
            comp = actor.static_mesh_component
            loc = actor.get_actor_location()
            shell_ok = (package(comp.static_mesh) == path
                        and sorted(str(t) for t in comp.get_editor_property("component_tags")) == sorted(shellio.SHELL_TAGS)
                        and str(comp.get_collision_profile_name()) == shellio.SHELL_PROFILE
                        and comp.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS
                        and not comp.get_editor_property("use_default_collision") and not comp.is_visible()
                        and comp.mobility == unreal.ComponentMobility.STATIC
                        and max(abs(loc.x), abs(loc.y), abs(loc.z)) < 0.01
                        and shellio.mesh_check(comp.static_mesh)["pass"])
            if not shell_ok:
                issues.append("shell contract " + shell_label)
    # 3) 나머지 최상위 액터는 복제 직후와 같다(바꾼 몸체는 메시·라벨만 다르다).
    changed = []
    expect = sorted([renamed.get(k, k) for k in before] + added)
    if sorted(now) != expect:
        issues.append("actor labels differ: +%s -%s" % (sorted(set(now) - set(expect)), sorted(set(expect) - set(now))))
    for label, row in before.items():
        actor = now.get(renamed.get(label, label))
        if not actor:
            continue
        got = actor_state(actor)
        if label in renamed:
            changed += [{"label": label, "key": k} for k in ("location", "rotation", "scale", "hidden", "tags", "folder")
                        if got[k] != row[k]]
            continue
        got["label"] = label
        if got != row:
            changed.append({"label": label, "diff": [k for k in row if row[k] != got.get(k)]})
    if changed:
        issues.append("unexpected changes %d" % len(changed))
    # 4) 충돌 컴포넌트(하위 Level Instance 포함)
    colliding, visual = [], []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor):
        trial = TAG in [str(t) for t in actor.tags]
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            if comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
                colliding.append(package(comp.static_mesh).rsplit("/", 1)[-1])
                if trial:
                    visual.append(actor.get_actor_label())
    checks = {"level": package(world()) == target, "actors": not issues,
              "collision_expected": sorted(colliding) == sorted(cfg["collision"]),
              "visual_collision_zero": not visual, "pie_false": not pie(), "dirty_none": not dirty()}
    report = {"level": package(world()), "expected": target, "checks": checks, "issues": issues, "changed": changed,
              "added": added, "shell_contract": shell_ok, "colliding": sorted(colliding), "visual_collision": visual,
              "dirty": dirty(), "pass": all(checks.values())}
    write("Verify_%s_%s.json" % (version, stamp()), report)
    unreal.log("RIDGE_ISLE_TOP_VERIFY " + json.dumps({"pass": report["pass"], "checks": checks, "issues": issues}))


def material(version):
    """IsleSurface 부모·MI를 복제해 경사 Custom 노드 코드만 절벽 고정으로 바꾼 엄폐용 재질을 만든다(원본은 그대로)."""
    assets, edit = unreal.EditorAssetLibrary, unreal.MaterialEditingLibrary
    assert not dirty(), dirty()
    assert version == "Rock", version
    assert not assets.does_asset_exist(ISLE_ROCK), ISLE_ROCK
    source = ISLE_SURFACE.rsplit("/", 1)[0] + "/"
    parent = assets.duplicate_asset(source + "M_Ridge_IsleSurface", ISLE_ROCK_FOLDER + "M_Ridge_IsleRock")
    assert parent
    scope, = [node for node in edit.get_material_expressions(parent) if isinstance(node, unreal.MaterialExpressionCustom)
              and "smoothstep(0.3420201" in node.get_editor_property("code")]
    scope.set_editor_property("code", ISLE_ROCK_SCOPE)
    assert not edit.recompile_material(parent), "Isle rock material compilation failed."
    instance = assets.duplicate_asset(ISLE_SURFACE, ISLE_ROCK)
    assert instance
    edit.set_material_instance_parent(instance, parent)
    edit.update_material_instance(instance)
    assert assets.save_loaded_asset(parent, False) and assets.save_loaded_asset(instance, False)
    write("Material_Rock_%s.json" % stamp(), {"instance": ISLE_ROCK, "parent": package(parent), "scope_code": ISLE_ROCK_SCOPE,
                                              "source": ISLE_SURFACE, "dirty": dirty()})
    unreal.log("RIDGE_ISLE_ROCK_MATERIAL_READY " + ISLE_ROCK)


if __name__ == "__main__":
    command, version = sys.argv[1:]
    {"build": build, "verify": verify, "material": material}[command](version)
