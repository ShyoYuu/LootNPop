"""UE Cmd에서 Z1 ED 시각 인스턴스를 규칙으로 덜어낸 사본 레벨을 만든다(1-BR U6 러프 PIE 지적 정리).

러프 PIE에서 다음 유형이 나왔다(.context/Graphics/history/Phase01_Log.md "U6 Z1 러프 PIE").
- A: 조립체·CR 산괴 꼭대기 덮개가 지형을 낮춘 뒤 공중에 남거나, 프랍끼리 받쳐 가지처럼 뻗었다.
- B: 드레이프·평면 묶음(평지 배치)이 좁은 봉우리에 통째로 얹혔다.
- C: 동굴 절개 안 인스턴스는 U5 충돌에서 빠져 보이기만 하고 통과된다.
- D: 얇은 판이 지면에 세워 꽂혔다.
판정은 Blender `Scripts/Blender/DecideRidgeZ1Cull.py`가 한다(numpy·BVH). 이 스크립트는 그 입력을 내보내고 결과로 레벨을 쓴다.

인자:
- export <버전>: 열린 LVI(Level Instance가 로드된 상태, LVI를 연 실행과 다른 Cmd)의 모든 시각 인스턴스를 소유 Level
  Instance·액터·컴포넌트·인스턴스 번호와 world transform, 메시 로컬 상자로 Saved .../Cull/Export_<버전>.json에 쓴다.
- build <버전>: 같은 LVI를 연 상태에서 Cull/Decide_<버전>.json의 제거 목록을 뺀 사본을 소유 Level Instance마다
  `EDTerrain_V001/Z1Cull_<버전>/<라벨>` 월드 좌표 레벨(ISM·데칼)로 저장한다. 원본 조립체·층 레벨은 바꾸지 않는다.
"""
import importlib
import json
from pathlib import Path
import sys

import unreal

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parents[1])]
import RidgeZ1Layout as layout

importlib.reload(layout)
test, spec = layout.test, layout.spec

OUT = test.OUT / "Cull"
FOLDER = layout.EDTERRAIN + "Z1Cull_{0}"
COPY = test.DRAPE_COPY


def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(value, separators=(",", ":")) + "\n", encoding="utf-8")


def owners():
    # 로드된 하위 레벨 경로 → 소유 Level Instance 라벨, 하위 레벨별 액터.
    top, nested = layout.level_actors()
    labels = {}
    for actor in top:
        if isinstance(actor, unreal.LevelInstance):
            loaded = actor.get_loaded_level()
            assert loaded, actor.get_actor_label()
            labels[loaded.get_path_name()] = actor.get_actor_label()
    assert set(labels) == set(nested), (sorted(labels), sorted(nested))
    return {labels[path]: actors for path, actors in nested.items()}


def components(actors):
    for actor in actors:
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            if comp.static_mesh:
                yield actor, comp


def pose(t):
    p, q, s = t.translation, t.rotation, t.scale3d
    return [round(p.x, 2), round(p.y, 2), round(p.z, 2), round(q.x, 7), round(q.y, 7), round(q.z, 7), round(q.w, 7),
            round(s.x, 5), round(s.y, 5), round(s.z, 5)]


def export(version):
    meshes, mesh_index, comps, rows, decals = [], {}, [], [], []
    for label, actors in sorted(owners().items()):
        for actor, comp in components(actors):
            path = test.package(comp.static_mesh)
            if path not in mesh_index:
                box = comp.static_mesh.get_bounding_box()
                mesh_index[path] = len(meshes)
                meshes.append([path, test.xyz(box.min), test.xyz(box.max)])
            cid = len(comps)
            comps.append([label, actor.get_name(), comp.get_name(), mesh_index[path]])
            if isinstance(comp, unreal.InstancedStaticMeshComponent):
                for i in range(comp.get_instance_count()):
                    rows.append([cid, i] + pose(comp.get_instance_transform(i, True)))
            else:
                rows.append([cid, -1] + pose(comp.get_world_transform()))
        for actor in actors:
            if isinstance(actor, unreal.DecalActor):
                size = actor.decal.get_editor_property("decal_size")
                decals.append([label, actor.get_name(), test.package(actor.decal.get_decal_material())]
                              + pose(actor.get_actor_transform()) + [size.x, size.y, size.z])
    write("Export_%s.json" % version, {"level": layout.world_package(), "meshes": meshes, "components": comps,
                                        "instances": rows, "decals": decals})
    unreal.log("Z1_CULL_EXPORT %d instances %d components %d meshes %d decals" % (len(rows), len(comps), len(meshes),
                                                                              len(decals)))


def build(version):
    decide = json.loads((OUT / ("Decide_%s.json" % version)).read_text(encoding="utf-8"))
    removed = {label: {key: set(indices) for key, indices in table.items()} for label, table in decide["remove"].items()}
    removed_decals = {label: set(names) for label, names in decide["remove_decals"].items()}
    assert not test.dirty(), test.dirty()
    # 원본(로드된 하위 레벨)을 모두 읽은 뒤 새 맵을 만든다. 새 맵을 열면 원본 액터는 사라지지만 메시·재질 객체는 남는다.
    sources = {}
    for label, actors in sorted(owners().items()):
        rows, decals = [], []
        for actor, comp in components(actors):
            gone = removed.get(label, {}).get(actor.get_name() + "|" + comp.get_name(), set())
            k = comp.get_editor_property("num_custom_data_floats") if isinstance(
                comp, unreal.InstancedStaticMeshComponent) else 0
            custom = list(comp.get_editor_property("per_instance_sm_custom_data")) if k else []
            if isinstance(comp, unreal.InstancedStaticMeshComponent):
                keep = [i for i in range(comp.get_instance_count()) if i not in gone]
                transforms = [comp.get_instance_transform(i, True) for i in keep]
                custom = [v for i in keep for v in custom[i * k:(i + 1) * k]]
            else:
                transforms = [] if -1 in gone else [comp.get_world_transform()]
            if not transforms:
                continue
            props = {}
            for prop in COPY:
                try:
                    props[prop] = comp.get_editor_property(prop)
                except Exception:
                    pass
            rows.append({"mesh": comp.static_mesh, "overrides": list(comp.get_editor_property("override_materials")),
                         "hism": isinstance(comp, unreal.HierarchicalInstancedStaticMeshComponent), "props": props,
                         "custom_count": k, "custom": custom, "transforms": transforms})
        for actor in actors:
            if isinstance(actor, unreal.DecalActor) and actor.get_name() not in removed_decals.get(label, set()):
                decals.append({"material": actor.decal.get_decal_material(), "transform": actor.get_actor_transform(),
                               "size": actor.decal.get_editor_property("decal_size"),
                               "sort_order": actor.decal.get_editor_property("sort_order")})
        sources[label] = (rows, decals)
    actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    saving = unreal.EditorLoadingAndSavingUtils
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary

    def add(parent, cls):
        handle, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls))
        assert not str(fail), fail
        return handle, lib.get_object(lib.get_data(handle))

    report = {}
    for label, (rows, decals) in sources.items():
        level = "%s/%s" % (FOLDER.format(version), label)
        assert not unreal.EditorAssetLibrary.does_asset_exist(level), level
        assert saving.new_blank_map(False)
        actor = actors_sub.spawn_actor_from_class(unreal.Actor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        actor.set_actor_label(label + "_Cull_" + version)
        root_handle, root = add(sds.k2_gather_subobject_data_for_instance(actor)[0], unreal.SceneComponent)
        root.set_mobility(unreal.ComponentMobility.STATIC)
        count = 0
        for row in rows:
            _, comp = add(root_handle, unreal.HierarchicalInstancedStaticMeshComponent if row["hism"]
                          else unreal.InstancedStaticMeshComponent)
            comp.set_mobility(unreal.ComponentMobility.STATIC)
            comp.set_static_mesh(row["mesh"])
            for i, material in enumerate(row["overrides"]):
                if material:
                    comp.set_material(i, material)
            for prop, value in row["props"].items():
                comp.set_editor_property(prop, value)
            # GX-031: 시각 메시는 충돌이 없다. 이름 있는 프로필이 로드 때 충돌을 되살리지 않게 프로필부터 끈다.
            comp.set_collision_profile_name("NoCollision")
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            comp.add_instances(row["transforms"], False, True)
            k = row["custom_count"]
            if k:
                comp.set_num_custom_data_floats(k)
                for i in range(len(row["transforms"])):
                    for j in range(k):
                        comp.set_custom_data_value(i, j, row["custom"][i * k + j], False)
            assert comp.get_instance_count() == len(row["transforms"])
            count += len(row["transforms"])
        actor.set_folder_path("Cull")
        for row in decals:
            t = row["transform"]
            decal = actors_sub.spawn_actor_from_class(unreal.DecalActor, t.translation, t.rotation.rotator())
            decal.set_actor_scale3d(t.scale3d)
            decal.decal.set_decal_material(row["material"])
            decal.decal.set_editor_property("decal_size", row["size"])
            decal.decal.set_editor_property("sort_order", row["sort_order"])
            decal.set_folder_path("Cull")
        assert saving.save_map(test.world(), level), level
        report[label] = {"level": level, "instances": count, "components": len(rows), "decals": len(decals)}
        unreal.log("Z1_CULL_LEVEL %s %d" % (label, count))
    write("Build_%s.json" % version, report)
    unreal.log("Z1_CULL_BUILD_READY %s" % json.dumps({k: v["instances"] for k, v in report.items()}))


if __name__ == "__main__":
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first."
    {"export": export, "build": build}[sys.argv[1]](*sys.argv[2:])
