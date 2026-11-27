"""IsleSkin 외피 충돌 측정(3a)의 UE 쪽 입력을 만든다. 읽기 전용이다(레벨·에셋을 바꾸거나 저장하지 않는다).

열린 레벨(Level Instance 하위 레벨 포함)에서 충돌이 켜진 StaticMeshComponent를 모두 세고(census),
외피 몸체 묶음 주변 상자와 겹치는 충돌 삼각형을 월드 좌표(UE cm) float32로 덤프한다(dump).
Blender 측정은 Scripts/Blender/MeasureRidgeIsleSkinCollision.py가 이 덤프와 몸체 FBX를 읽는다.

실행: 에디터 상태 표시줄 Cmd에 py "<이 파일>" <명령> [인자]
- census: 충돌 컴포넌트 목록 → Census_<레벨 끝 이름>.json
- dump <배치 판> [이름]: IsleSkin_<판>.json의 몸체 경계 묶음 + 여유 3,000cm 상자와 겹치는 충돌 삼각형 → Dump_<이름|판>.bin/.json
  (Shell을 더한 맵은 배치 판 V007 + 이름 V008처럼 준다)
결과: Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Collision/
"""
import hashlib
import json
from pathlib import Path
import struct
import sys

import unreal

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUT = ROOT / "Saved/Profiling/Graphics/Phase01/IsleSkin_V001/Collision"
MARGIN_CM = 3000.0
# 몸체 묶음: 같은 자리를 차지하는 몸체끼리 한 상자로 덤프한다.
CLUSTERS = {"ValleySkyIsle": ("ValleySkyIsle/",), "WestSkyIsle": ("WestSkyIsle/",), "SkyBridge": ("SkyBridge/",),
            "Monolith": ("Monolith/",)}


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def package(obj):
    return obj.get_path_name().split(".")[0] if obj else None


def dirty():
    saving = unreal.EditorLoadingAndSavingUtils
    return [p.get_path_name() for p in list(saving.get_dirty_map_packages()) + list(saving.get_dirty_content_packages())]


def pie():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()


def colliding():
    """(액터, 컴포넌트) 중 충돌이 켜진 것. Level Instance 하위 레벨까지 모은다."""
    rows = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor):
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            if comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
                rows.append((actor, comp))
    return rows


def describe(actor, comp):
    n = comp.get_instance_count() if isinstance(comp, unreal.InstancedStaticMeshComponent) else 1
    return {"level": actor.get_level().get_outermost().get_name(), "actor": actor.get_actor_label(),
            "mesh": package(comp.static_mesh), "class": comp.get_class().get_name(),
            "profile": str(comp.get_collision_profile_name()), "enabled": str(comp.get_collision_enabled()),
            "tags": sorted(str(t) for t in comp.get_editor_property("component_tags")),
            "visible": comp.is_visible(), "instances": n}


def write(name, data):
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / name
    path.write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")
    return path


def census():
    rows = [describe(a, c) for a, c in colliding()]
    name = package(world()).rsplit("/", 1)[-1]
    write("Census_%s.json" % name, {"world": package(world()), "pie": pie(), "dirty": dirty(), "colliding": rows})
    unreal.log("ISLESKIN_COLLISION_CENSUS %d" % len(rows))


def mesh_arrays(mesh):
    # Nanite fallback(RenderData LOD0)의 정점(메시 로컬 cm)과 삼각형(ExportRidgeEDCollision.mesh_arrays와 같은 읽기).
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.set_editor_property("lod_type", unreal.GeometryScriptLODType.RENDER_DATA)
    lod.set_editor_property("lod_index", 0)
    dm, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(
        mesh, unreal.DynamicMesh(), unreal.GeometryScriptCopyMeshFromAssetOptions(), lod)
    assert outcome == unreal.GeometryScriptOutcomePins.SUCCESS, mesh.get_path_name()
    queries, lists = unreal.GeometryScript_MeshQueries, unreal.GeometryScript_List
    _, positions, gaps = queries.get_all_vertex_positions(dm, False)
    _, triangles, tri_gaps = queries.get_all_triangle_indices(dm, False)
    assert not gaps and not tri_gaps, mesh.get_path_name()
    verts = [(v.x, v.y, v.z) for v in lists.convert_vector_list_to_array(positions)]
    tris = [(t.x, t.y, t.z) for t in lists.convert_triangle_list_to_array(triangles)]
    return verts, tris


def boxes(version):
    spec = json.loads((HERE / ("IsleSkin_V001/IsleSkin_%s.json" % version)).read_text(encoding="utf-8"))
    out = {}
    for name, prefixes in CLUSTERS.items():
        rows = [b for b in spec["bodies"] if b["group"].startswith(prefixes)]
        assert rows, name
        lo = [min(b["bounds_cm"][0][i] for b in rows) - MARGIN_CM for i in range(3)]
        hi = [max(b["bounds_cm"][1][i] for b in rows) + MARGIN_CM for i in range(3)]
        out[name] = {"lo": lo, "hi": hi, "bodies": [b["label"] for b in rows]}
    return out


def dump(version, name=None):
    name = name or version
    assert not pie(), "Stop PIE first"
    box = boxes(version)
    los = [b["lo"] for b in box.values()]
    his = [b["hi"] for b in box.values()]
    meta, blob = [], bytearray()
    for actor, comp in colliding():
        row = describe(actor, comp)
        assert row["class"] == "StaticMeshComponent", row
        xf = comp.get_world_transform()
        verts, tris = mesh_arrays(comp.static_mesh)
        identity = (xf.translation.length() < 1e-3 and abs(xf.rotation.w) > 1 - 1e-9
                    and (xf.scale3d - unreal.Vector(1, 1, 1)).length() < 1e-6)
        if not identity:
            verts = [(lambda w: (w.x, w.y, w.z))(xf.transform_location(unreal.Vector(*v))) for v in verts]
        kept = 0
        for a, b, c in tris:
            p, q, r = verts[a], verts[b], verts[c]
            tlo = [min(p[i], q[i], r[i]) for i in range(3)]
            thi = [max(p[i], q[i], r[i]) for i in range(3)]
            if any(all(tlo[i] <= hi[i] and thi[i] >= lo[i] for i in range(3)) for lo, hi in zip(los, his)):
                blob += struct.pack("<9f", *p, *q, *r)
                kept += 1
        row.update({"identity_transform": identity, "mesh_triangles": len(tris), "dumped_triangles": kept})
        meta.append(row)
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / ("Dump_%s.bin" % name)
    path.write_bytes(bytes(blob))
    report = {"world": package(world()), "version": version, "name": name, "margin_cm": MARGIN_CM, "boxes": box,
              "layout": "float32 little-endian, 9 per triangle (world UE cm), meshes in 'meshes' order",
              "meshes": meta, "bin": path.relative_to(ROOT).as_posix(),
              "bin_sha256": hashlib.sha256(bytes(blob)).hexdigest(), "pie": pie(), "dirty": dirty()}
    write("Dump_%s.json" % name, report)
    unreal.log("ISLESKIN_COLLISION_DUMP " + json.dumps({m["actor"]: m["dumped_triangles"] for m in meta}))


if __name__ == "__main__":
    command, *args = sys.argv[1:]
    {"census": census, "dump": dump}[command](*args)
