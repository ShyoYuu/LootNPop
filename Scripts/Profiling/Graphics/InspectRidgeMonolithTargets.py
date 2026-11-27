"""Phase 1-BR 모노리스 재해석 대상 특정(읽기 전용, GX-038).

UE Cmd: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/InspectRidgeMonolithTargets.py" <명령> [인자]
- state: 현재 레벨·PIE·미저장 패키지를 기록한다.
- open <레벨 경로>: 미저장 패키지가 없을 때만 레벨을 연다(저장하지 않는다).
- pick <요청 JSON>: 시점별 화면 좌표(u, v: 0~1, 오른쪽·아래가 +)에서 광선을 쏴 보이는 정적 메시 삼각형의 첫 교차를 찾는다.
  요청 형식 {"views": {"이름": {"location": {..}, "rotation": {..}, "fov": 가로 FOV, "aspect": 가로/세로, "points": [[u, v], ..]}}}.
  결과에 액터·컴포넌트·메시·재질·월드/옥탄트 로컬 좌표·이음매 거리(재질이 쓰는 메시 로컬 min(x, y, z))를 남긴다.
- describe <라벨 부분 문자열,...>: 라벨이 맞는 액터의 메시·재질·경계·충돌·태그·소속 레벨을 기록한다.
- heightmap <라벨> <x0> <x1> <y0> <y1> <칸>: 그 액터 메시 정점의 옥탄트 로컬 칸별 최소·최대 H를 기록한다.
- dump <라벨,...> <x0> <x1> <y0> <y1> <이름>: 옥탄트 로컬 상자와 경계가 겹치는 삼각형을 월드 좌표로 기록한다(Blender 몸체 입력).
- hide <라벨 부분 문자열,...> [1|0]: 에디터 임시 숨김(저장되지 않음). 화면 차이로 메시 영역을 가를 때 쓴다.
에셋·레벨은 바꾸지 않는다. 결과는 Saved/Profiling/Graphics/Phase01/Monolith_V001/ 아래 JSON이다.
"""
import datetime
import json
import math
from pathlib import Path
import sys

import unreal

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[2]
OUT = PROJECT / "Saved/Profiling/Graphics/Phase01/Monolith_V001"
# 큰 ISM은 인스턴스별 판정이 느려 경계 교차만 기록한다.
ISM_LIMIT = 3000

# 옥탄트 로컬 좌표계(BlueprintViewCamera.py와 같은 정의). 구면 Up = 중심 쪽.
R = 30000.0
LAT, AZ = math.radians(35.264389682754654), math.radians(45.0)
RADIAL = (math.cos(LAT)*math.cos(AZ), math.cos(LAT)*math.sin(AZ), math.sin(LAT))
TANGENT = (-math.sin(AZ), math.cos(AZ), 0.0)
BITAN = (-math.sin(LAT)*math.cos(AZ), -math.sin(LAT)*math.sin(AZ), math.cos(LAT))


def dot(a, b):
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]


def sub(a, b):
    return (a[0]-b[0], a[1]-b[1], a[2]-b[2])


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def vec(v):
    return (v.x, v.y, v.z)


def octant_local(p):
    """월드 점 → 옥탄트 로컬 (x, y, H). H는 반지름 30000에서 안쪽으로 잰 높이."""
    length = math.sqrt(dot(p, p))
    d = (p[0]/length, p[1]/length, p[2]/length)
    k = R/dot(d, RADIAL)
    return [round(k*dot(d, BITAN), 1), round(k*dot(d, TANGENT), 1), round(R-length, 1)]


def stamp():
    return datetime.datetime.now().strftime("%Y%m%d_%H%M%S")


def write(name, data):
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / name
    path.write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")
    return path


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def dirty():
    saving = unreal.EditorLoadingAndSavingUtils
    return [p.get_path_name() for p in list(saving.get_dirty_map_packages()) + list(saving.get_dirty_content_packages())]


def package(obj):
    return obj.get_path_name().split(".")[0] if obj else None


def state():
    report = {"level": package(world()), "pie": unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(),
              "dirty": dirty()}
    write("State_%s.json" % stamp(), report)
    unreal.log("RIDGE_MONOLITH_STATE " + json.dumps(report))


def open_level(path):
    assert not dirty(), dirty()
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(path), path
    unreal.log("RIDGE_MONOLITH_OPEN " + path)


# ---- 광선 판정 ----
_TRIS = {}


def mesh_triangles(mesh):
    """LOD0(나나이트는 fallback) 섹션별 삼각형을 (슬롯, [(a, b, c), ..])로 캐시한다."""
    key = mesh.get_path_name()
    if key in _TRIS:
        return _TRIS[key]
    sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    sections = []
    for s in range(mesh.get_num_sections(0)):
        verts, tris = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, s)[:2]
        v = [vec(p) for p in verts]
        slot = sms.get_lod_material_slot(mesh, 0, s)
        sections.append((slot, [(v[tris[i]], v[tris[i+1]], v[tris[i+2]]) for i in range(0, len(tris), 3)]))
    _TRIS[key] = sections
    return sections


def ray_box(o, d, lo, hi):
    t0, t1 = -1e30, 1e30
    for i in range(3):
        if abs(d[i]) < 1e-12:
            if o[i] < lo[i] or o[i] > hi[i]:
                return None
            continue
        a, b = (lo[i]-o[i])/d[i], (hi[i]-o[i])/d[i]
        if a > b:
            a, b = b, a
        t0, t1 = max(t0, a), min(t1, b)
        if t0 > t1:
            return None
    return t0 if t1 >= 0 else None


def ray_tris(o, d, sections):
    best = None
    for slot, tris in sections:
        for a, b, c in tris:
            e1, e2 = sub(b, a), sub(c, a)
            pv = cross(d, e2)
            det = dot(e1, pv)
            if abs(det) < 1e-9:
                continue
            inv = 1.0/det
            tv = sub(o, a)
            u = dot(tv, pv)*inv
            if u < 0 or u > 1:
                continue
            qv = cross(tv, e1)
            w = dot(d, qv)*inv
            if w < 0 or u+w > 1:
                continue
            t = dot(e2, qv)*inv
            if t > 0 and (best is None or t < best[0]):
                best = (t, slot)
    return best


def all_actors():
    """EditorActorSubsystem은 지속 레벨만 돌려준다. Level Instance 하위 레벨까지 TActorIterator로 모은다."""
    return unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor)


def components():
    """에디터 월드의 모든 레벨(Level Instance 포함)에서 정적 메시 컴포넌트를 모은다."""
    rows = []
    for actor in all_actors():
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            if comp.static_mesh:
                rows.append((actor, comp))
    return rows


def visible(actor, comp):
    return comp.is_visible() and not actor.is_hidden_ed() and not actor.is_temporarily_hidden_in_editor()


def hit_row(actor, comp, t, slot, o, d, local_point, instance=None):
    p = (o[0]+d[0]*t, o[1]+d[1]*t, o[2]+d[2]*t)
    material = comp.get_material(slot) if slot is not None and slot >= 0 else None
    return {"distance_cm": round(t, 1), "actor": actor.get_actor_label(), "actor_path": actor.get_path_name(),
            "class": actor.get_class().get_name(), "component": comp.get_name(), "instance": instance,
            "mesh": package(comp.static_mesh), "slot": slot, "material": package(material),
            "visible": visible(actor, comp), "collision": str(comp.get_collision_enabled()),
            "tags": [str(x) for x in actor.tags], "world": [round(x, 1) for x in p],
            "octant_local_xyh": octant_local(p),
            "seam_world_min_cm": round(min(p), 1), "seam_mesh_local_min_cm": round(min(local_point), 1)}


def cast(o, d, comps):
    hits, skipped = [], []
    for actor, comp in comps:
        origin, extent = unreal.SystemLibrary.get_component_bounds(comp)[:2]
        lo, hi = sub(vec(origin), vec(extent)), (origin.x+extent.x, origin.y+extent.y, origin.z+extent.z)
        if ray_box(o, d, lo, hi) is None:
            continue
        mesh = comp.static_mesh
        box = mesh.get_bounding_box()
        mlo, mhi = vec(box.min), vec(box.max)
        if isinstance(comp, unreal.InstancedStaticMeshComponent):
            count = comp.get_instance_count()
            if count > ISM_LIMIT:
                skipped.append({"actor": actor.get_actor_label(), "component": comp.get_name(), "instances": count,
                                "mesh": package(mesh), "box_t": round(ray_box(o, d, lo, hi), 1)})
                continue
            xforms = [(i, comp.get_instance_transform(i, True)) for i in range(count)]
        else:
            xforms = [(None, comp.get_world_transform())]
        for index, xf in xforms:
            lo_o = vec(xf.inverse_transform_location(unreal.Vector(*o)))
            far = vec(xf.inverse_transform_location(unreal.Vector(o[0]+d[0]*1e5, o[1]+d[1]*1e5, o[2]+d[2]*1e5)))
            ld = sub(far, lo_o)
            if ray_box(lo_o, ld, mlo, mhi) is None:
                continue
            got = ray_tris(lo_o, ld, mesh_triangles(mesh))
            if got:
                s = got[0]
                lp = (lo_o[0]+ld[0]*s, lo_o[1]+ld[1]*s, lo_o[2]+ld[2]*s)
                hits.append(hit_row(actor, comp, s*1e5, got[1], o, d, lp, index))
    hits.sort(key=lambda h: h["distance_cm"])
    return hits, skipped


def camera_basis(rotation):
    rot = unreal.Rotator(roll=rotation["roll"], pitch=rotation["pitch"], yaw=rotation["yaw"])
    return vec(rot.get_forward_vector()), vec(rot.get_right_vector()), vec(rot.get_up_vector())


def pick(request_path):
    request = json.loads(Path(request_path).read_text(encoding="utf-8"))
    comps = components()
    report = {"level": package(world()), "components": len(comps), "views": {}}
    for name, view in request["views"].items():
        o = (view["location"]["x"], view["location"]["y"], view["location"]["z"])
        f, r, u = camera_basis(view["rotation"])
        tx = math.tan(math.radians(view["fov"])/2)
        ty = tx/view["aspect"]
        rows = []
        for su, sv in view["points"]:
            a, b = (2*su-1)*tx, (1-2*sv)*ty
            d = (f[0]+r[0]*a+u[0]*b, f[1]+r[1]*a+u[1]*b, f[2]+r[2]*a+u[2]*b)
            n = math.sqrt(dot(d, d))
            d = (d[0]/n, d[1]/n, d[2]/n)
            hits, skipped = cast(o, d, comps)
            first = next((h for h in hits if h["visible"]), None)
            rows.append({"uv": [su, sv], "first_visible": first, "hits": hits[:6],
                         "large_ism_before_first": [s for s in skipped if first is None or s["box_t"] < first["distance_cm"]]})
        report["views"][name] = rows
    path = write("Pick_%s.json" % stamp(), report)
    unreal.log("RIDGE_MONOLITH_PICK " + str(path))


def describe(patterns):
    keys = [p.lower() for p in patterns.split(",")]
    rows = []
    for actor in all_actors():
        label = actor.get_actor_label()
        if not any(k in label.lower() for k in keys):
            continue
        comps = []
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            mesh = comp.static_mesh
            origin, extent = unreal.SystemLibrary.get_component_bounds(comp)[:2]
            comps.append({"component": comp.get_name(), "mesh": package(mesh),
                          "materials": [package(m) for m in comp.get_materials()],
                          "instances": comp.get_instance_count() if isinstance(comp, unreal.InstancedStaticMeshComponent) else None,
                          "bounds_origin": [round(x, 1) for x in vec(origin)], "bounds_extent": [round(x, 1) for x in vec(extent)],
                          "origin_octant_local_xyh": octant_local(vec(origin)),
                          "collision": str(comp.get_collision_enabled()), "profile": str(comp.get_collision_profile_name()),
                          "visible": visible(actor, comp),
                          "triangles": mesh.get_num_triangles(0) if mesh else None})
        rows.append({"label": label, "path": actor.get_path_name(), "class": actor.get_class().get_name(),
                     "location": [round(x, 1) for x in vec(actor.get_actor_location())],
                     "tags": [str(x) for x in actor.tags], "folder": str(actor.get_folder_path()), "components": comps})
    path = write("Describe_%s.json" % stamp(), {"level": package(world()), "patterns": keys, "actors": rows})
    unreal.log("RIDGE_MONOLITH_DESCRIBE %d %s" % (len(rows), path))


def heightmap(label, x0, x1, y0, y1, cell):
    """라벨이 같은 액터 메시 정점을 옥탄트 로컬로 바꿔 칸별 최대·최소 H를 기록한다(형상 발자국 확인용)."""
    x0, x1, y0, y1, cell = (float(v) for v in (x0, x1, y0, y1, cell))
    actor = next(a for a in all_actors() if a.get_actor_label() == label)
    comp = actor.get_component_by_class(unreal.StaticMeshComponent)
    xf = comp.get_world_transform()
    grid = {}
    for _, tris in mesh_triangles(comp.static_mesh):
        for tri in tris:
            for v in tri:
                lx, ly, h = octant_local(vec(xf.transform_location(unreal.Vector(*v))))
                if not (x0 <= lx < x1 and y0 <= ly < y1):
                    continue
                key = (int((lx-x0)//cell), int((ly-y0)//cell))
                lo, hi = grid.get(key, (1e9, -1e9))
                grid[key] = (min(lo, h), max(hi, h))
    nx, ny = int((x1-x0)//cell), int((y1-y0)//cell)
    rows = [[None if (i, j) not in grid else [round(grid[(i, j)][0]), round(grid[(i, j)][1])] for j in range(ny)]
            for i in range(nx)]
    path = write("Height_%s_%s.json" % (label, stamp()), {"label": label, "mesh": package(comp.static_mesh),
                                                          "x0": x0, "y0": y0, "cell": cell, "rows_by_x": rows})
    unreal.log("RIDGE_MONOLITH_HEIGHT " + str(path))


def dump(labels, x0, x1, y0, y1, name):
    """라벨 액터들의 삼각형 중 정점 하나라도 옥탄트 로컬 상자 안인 것을 월드 좌표(UE cm)로 기록한다(Blender 입력)."""
    x0, x1, y0, y1 = (float(v) for v in (x0, x1, y0, y1))
    by_label = {a.get_actor_label(): a for a in all_actors()}
    rows = {}
    for label in labels.split(","):
        comp = by_label[label].get_component_by_class(unreal.StaticMeshComponent)
        xf = comp.get_world_transform()
        tris = []
        for _, section in mesh_triangles(comp.static_mesh):
            for tri in section:
                w = [vec(xf.transform_location(unreal.Vector(*v))) for v in tri]
                ls = [octant_local(p) for p in w]
                # 정점이 상자 밖이어도 삼각형 경계가 상자와 겹치면 넣는다(큰 지각 삼각형 누락 방지).
                if min(l[0] for l in ls) <= x1 and max(l[0] for l in ls) >= x0 and \
                        min(l[1] for l in ls) <= y1 and max(l[1] for l in ls) >= y0:
                    tris.append([[round(c, 3) for c in p] for p in w])
        rows[label] = {"mesh": package(comp.static_mesh), "triangles": tris}
    path = write("Dump_%s.json" % name, {"level": package(world()), "box_local_xy_cm": [x0, x1, y0, y1], "actors": rows})
    unreal.log("RIDGE_MONOLITH_DUMP %s %s" % (path, {k: len(v["triangles"]) for k, v in rows.items()}))


def hide(patterns, value="1"):
    """라벨에 부분 문자열이 맞는 액터를 에디터에서만 임시로 숨기거나(1) 되돌린다(0). 패키지는 dirty가 되지 않는다."""
    keys = [p.lower() for p in patterns.split(",")]
    hits = [a for a in all_actors() if any(k in a.get_actor_label().lower() for k in keys)]
    for actor in hits:
        actor.set_is_temporarily_hidden_in_editor(value == "1")
    unreal.log("RIDGE_MONOLITH_HIDE %s %d %s" % (value, len(hits), json.dumps({"dirty": dirty()})))


if __name__ == "__main__":
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first"
    command, args = sys.argv[1], sys.argv[2:]
    {"state": state, "open": open_level, "pick": pick, "describe": describe, "heightmap": heightmap, "hide": hide, "dump": dump}[command](*args)
