"""1-B5e-2 같은 계열 변형 근경 시험. 기존 WIP 대분지(DeepBasin)에 변형 메시를 메모리에서만 놓는다.

콘솔: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/ProbeRidgeVariantCloseup.py" <모드>
  open: 저장된 WIP를 연다(현재 맵에 미저장 변경이 없을 때만).
  place: 변형을 하나씩 놓고 접지(모서리 뜸/매몰)를 기록한 뒤 카메라를 옮긴다. 저장하지 않는다.
  view <ID>: 해당 시험 메시 쪽으로 카메라를 옮기고 선택한다.
  clear: 시험 메시를 모두 지운다. WIP는 여전히 메모리에서만 바뀐 상태이므로 저장하지 않고 버린다.
배치 규칙은 LNPDecorationSampler와 같다(바닥 bounds 기준 접지·법선 정렬·바닥 4모서리+중심 접촉).
큰 바위 허용치는 크기에 비례시킨 시험값이다. 결과는 Saved 아래 JSON으로 남긴다.
"""
import json
import math
from pathlib import Path
import sys

import unreal

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from BuildRidgeGroundProbe import ROOT, LEVEL, xyz, spec, ZONE

OUT = ROOT / "Saved/Profiling/Graphics/Phase01/RidgeVariantCloseup_20261117"
PREFIX = "GraphicsVariantProbe_"
FOLDER = "Graphics/VariantProbe_V001"
INSPECTION = ROOT / "Saved/Profiling/Graphics/Phase01/RidgeDecorationExpand_20261116/Inspection_V004_FernSlope40.json"
CRUST_NAME = "SM_Octant_Ridge_00_V011_R30000"
CENTER = (3000.0, -300.0)  # spec DeepBasin 중심
FERN_MI = "/Game/Graphics/Environment/ElectricDreams/RidgeFernWind_V001/MI_Ridge_Fern"
SAND = "/Game/Megascans/3D_Assets/SandstoneBoulder/SM_SandstoneBoulder_%02d"
MOSS = "/Game/Megascans/3D_Assets/MossyForestBoulder/SM_MossyForestBoulder_%02d"
CLOVER = "/Game/Megascans/3D_Plants/CloverVarieties/SM_CloverVarieties_%02d"
FERN = "/Game/Megascans/3D_Plants/Fern/SM_Fern_%02d"

# ID, 메시, 종류. 큰 것부터 자리를 잡는다. *_Pivot은 bounds 바닥 대신 메시 원점을 지면에 둔 비교본이다
# (Clover_04·05는 bounds 바닥이 원점 아래 23·42cm라 샘플러 규칙대로면 깔개가 그만큼 뜰 수 있다).
ITEMS = [
    ("Sandstone_05", SAND % 5, "rock"), ("Moss_04", MOSS % 4, "rock"), ("Sandstone_04", SAND % 4, "rock"),
    ("Moss_03", MOSS % 3, "rock"), ("Clover_04", CLOVER % 4, "cover"), ("Clover_04_Pivot", CLOVER % 4, "cover"),
    ("Clover_05", CLOVER % 5, "cover"), ("Clover_05_Pivot", CLOVER % 5, "cover"),
    ("Moss_01", MOSS % 1, "rock"), ("Sandstone_03", SAND % 3, "rock"), ("Moss_05", MOSS % 5, "rock"),
    ("Moss_02", MOSS % 2, "rock"),
    ("Fern_05", FERN % 5, "fern"), ("Fern_04", FERN % 4, "fern"), ("Fern_02", FERN % 2, "fern"),
    ("Fern_06", FERN % 6, "fern"), ("Fern_03", FERN % 3, "fern"),
    ("Clover_03", CLOVER % 3, "cover"), ("Clover_02", CLOVER % 2, "cover"),
]
# 종류별 경사 상한·모서리 허용치(cm). 작은 메시는 현재 규칙 값, 큰 바위는 높이 비례(뜸5%·매몰20%).
KIND = {"rock": dict(slope=30, float_cm=10, sink_cm=25, sink=3.0),
        "cover": dict(slope=40, float_cm=8, sink_cm=15, sink=0.7),
        "fern": dict(slope=40, float_cm=15, sink_cm=25, sink=1.5)}
GRID = 150          # 후보 격자(cm)
GAP = 150           # 시험 메시끼리 사이 여유: 캐릭터가 사이로 지나가며 볼 수 있게
EXISTING_GAP = 30   # 기존 바위·Fern·수동 장식과의 여유(EXISTING_CLEARANCE와 같은 값)
EXISTING_RADIUS = {"SM_Ridge_SandstoneBoulder_01_Blocker": 75, "SM_MossyForestBoulder_06": 75,
                   "SM_Fern_01": 125, "SM_CloverVarieties_01": 95}


def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(value, indent=2, ensure_ascii=False), encoding="utf-8")


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def dirty():
    return sorted(p.get_path_name() for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) +
                  list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()))


def probes():
    return [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
            if a.get_actor_label().startswith(PREFIX)]


def require_wip():
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first."
    assert world().get_path_name().split(".")[0] == LEVEL, "Open the existing WIP level first (mode: open)."


def crust_component():
    for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if isinstance(a, unreal.StaticMeshActor):
            mesh = a.static_mesh_component.static_mesh
            if mesh and mesh.get_name() == CRUST_NAME:
                return a.static_mesh_component
    raise AssertionError("Crust actor not found.")


def crust_hit(crust, p, up, reach):
    hit = unreal.SystemLibrary.line_trace_single(world(), p + up * reach, p - up * reach,
                                                 unreal.TraceTypeQuery.ECC_VISIBILITY, True, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    data = hit.to_dict()
    if data["hit_component"] != crust:  # 기존 장식·바위 위가 아니라 지각에 닿아야 한다.
        return None
    return data["location"], data["normal"]


def occupied():
    """기존 PCG 인스턴스와 수동 장식의 2D 원(spec 로컬 XY, 반지름, 지피 여부)."""
    discs = []
    data = json.loads(INSPECTION.read_text(encoding="utf-8"))
    for c in data["components"]:
        name = (c["mesh"] or "").split(".")[-1]
        for inst in c["instances"]:
            x, y, _ = spec.local(inst["location"])
            discs.append((x, y, EXISTING_RADIUS.get(name, 100) * inst["scale"][0], name == "SM_CloverVarieties_01"))
    for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if isinstance(a, unreal.StaticMeshActor) and not a.get_actor_label().startswith(PREFIX):
            mesh = a.static_mesh_component.static_mesh
            # 지각과 지형 암반(SM_Ridge_00_*_Rocks_Floor/Shell, bounds 반경 약170m)은 장식이 아니다.
            if mesh and mesh.get_name() != CRUST_NAME and not mesh.get_name().startswith("SM_Ridge_00_"):
                origin, extent = a.get_actor_bounds(False)
                x, y, _ = spec.local(xyz(origin))
                discs.append((x, y, math.hypot(extent.x, extent.y), False))
    return discs


def pose(crust, mesh, kind, item_id, x, y, angle):
    """샘플러와 같은 접지: 지각 hit에서 법선을 Up으로, bounds 바닥을 sink만큼 묻는다(*_Pivot은 원점 기준)."""
    box = mesh.get_bounding_box()
    height = box.max.z - box.min.z
    rule = dict(KIND[kind])
    if kind == "rock" and height > 120:
        rule.update(float_cm=max(rule["float_cm"], .05 * height), sink_cm=max(rule["sink_cm"], .2 * height),
                    sink=max(rule["sink"], .05 * height))
    analytic = unreal.Vector(*spec.point(x, y, spec.ground(x, y)))
    up = analytic * (-1.0 / analytic.length())
    hit = crust_hit(crust, analytic, up, 800)
    if not hit:
        return None, "no_crust"
    ground, normal = hit
    if normal.dot(up) < 0:
        normal *= -1.0
    slope = math.degrees(math.acos(max(-1.0, min(1.0, normal.dot(up)))))
    if slope > rule["slope"]:
        return None, "slope"
    tangent = unreal.Vector(*spec.point(x, y + 10, spec.ground(x, y))) - analytic
    tangent -= normal * tangent.dot(normal)
    tangent /= tangent.length()
    a = math.radians(angle)
    tangent = tangent * math.cos(a) + normal.cross(tangent) * math.sin(a)
    rotation = unreal.MathLibrary.make_rot_from_zy(normal, tangent)
    q = rotation.quaternion()
    bottom = 0.0 if item_id.endswith("_Pivot") else box.min.z
    location = ground + normal * (-bottom - rule["sink"])
    gaps = []
    center = (box.min + box.max) * .5
    for lx, ly in ((box.min.x, box.min.y), (box.max.x, box.min.y), (box.min.x, box.max.y),
                   (box.max.x, box.max.y), (center.x, center.y)):
        corner = location + q.rotate_vector(unreal.Vector(lx, ly, box.min.z))
        h = crust_hit(crust, corner, normal, 300 + rule["sink_cm"] + rule["float_cm"])
        if not h:
            return None, "contact_miss"
        gaps.append((corner - h[0]).dot(normal))  # 양수 뜸, 음수 묻힘
    result = {"location": location, "rotation": rotation, "slope": slope, "height_cm": height,
              "corner_gaps_cm": [round(g, 1) for g in gaps], "max_float_cm": round(max(gaps), 1),
              "max_sink_cm": round(max(0.0, -min(gaps)), 1), "rule": rule,
              "radius_cm": math.hypot(box.max.x - box.min.x, box.max.y - box.min.y) * .5}
    result["contact_pass"] = result["max_float_cm"] <= rule["float_cm"] and result["max_sink_cm"] <= rule["sink_cm"]
    if item_id.endswith("_Pivot"):  # 원점 기준 비교본은 bounds 바닥이 묻히는 것이 전제다. 뜸만 막고 수치는 기록한다.
        result["contact_pass"] = result["max_float_cm"] <= rule["float_cm"]
    return result, None


def place():
    require_wip()
    assert not probes(), "Probe actors already exist; run clear first."
    start_dirty = dirty()
    # clear 뒤 다시 놓을 때는 이 시험이 남긴 WIP dirty만 허용한다(다른 패키지 변경은 거부).
    assert set(start_dirty) <= {LEVEL}, "Unsaved changes outside the WIP; resolve them first: %s" % start_dirty
    crust = crust_component()
    discs = occupied()
    candidates = []
    steps = int(2600 / GRID)
    for i in range(-steps, steps + 1):
        for j in range(-steps, steps + 1):
            x, y = CENTER[0] + i * GRID, CENTER[1] + j * GRID
            if spec.inside(x, y, list(ZONE)):
                candidates.append((math.hypot(i, j), x, y))
    candidates.sort()
    editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    placed, report = [], []
    for index, (item_id, path, kind) in enumerate(ITEMS):
        mesh = unreal.load_asset(path)
        assert mesh, path
        box = mesh.get_bounding_box()
        radius = math.hypot(box.max.x - box.min.x, box.max.y - box.min.y) * .5
        angle = (index * 137.5) % 360.0  # 같은 계열이 같은 방향으로 늘어서지 않게 하는 결정적 회전
        rejects = {}
        chosen = None
        for _, x, y in candidates:
            if spec.signed_distance(x, y, list(ZONE)) < radius + 100:
                rejects["zone"] = rejects.get("zone", 0) + 1
                continue
            if any(spec.signed_distance(x, y, spec.rock_outline(r)) > -(radius + 100) for r in spec.ROCKS):
                rejects["terrain_rock"] = rejects.get("terrain_rock", 0) + 1
                continue
            if any(math.hypot(x - px, y - py) < radius + pr + GAP for px, py, pr in placed):
                rejects["probe_gap"] = rejects.get("probe_gap", 0) + 1
                continue
            # 작은 지피는 기존 지피와도 떨어뜨려 비교가 섞이지 않게 하고, 나머지(5m 깔개 포함)는 기존 지피 위를 허용한다.
            small_cover = kind == "cover" and radius < 200
            if any(math.hypot(x - dx, y - dy) < radius + dr + EXISTING_GAP
                   for dx, dy, dr, cover in discs if small_cover or not cover):
                rejects["existing"] = rejects.get("existing", 0) + 1
                continue
            result, why = pose(crust, mesh, kind, item_id, x, y, angle)
            if why:
                rejects[why] = rejects.get(why, 0) + 1
                continue
            if not result["contact_pass"]:
                rejects["contact"] = rejects.get("contact", 0) + 1
                if "best_failed" not in rejects or result["max_float_cm"] + result["max_sink_cm"] < \
                        rejects["best_failed"]["sum"]:
                    rejects["best_failed"] = {"sum": result["max_float_cm"] + result["max_sink_cm"], "xy": [x, y],
                                              "gaps": result["corner_gaps_cm"]}
                continue
            chosen = (x, y, result)
            break
        row = {"id": item_id, "mesh": path, "kind": kind, "radius_cm": round(radius, 1), "rejects": rejects}
        if chosen is None:
            row["placed"] = False
            report.append(row)
            continue
        x, y, result = chosen
        actor = editor.spawn_actor_from_class(unreal.StaticMeshActor, result["location"], result["rotation"])
        assert actor
        actor.set_actor_label(PREFIX + item_id)
        actor.set_folder_path(FOLDER)
        c = actor.static_mesh_component
        assert c.set_static_mesh(mesh)
        if kind == "fern":
            c.set_material(0, unreal.load_asset(FERN_MI))
        c.set_mobility(unreal.ComponentMobility.STATIC)
        c.set_editor_property("use_default_collision", False)
        c.set_editor_property("bCanEverAffectNavigation", False)
        if kind == "rock":  # GX-014와 같은 계약(재베이크는 하지 않으므로 Nav·SurfaceData에는 반영되지 않는다)
            c.set_editor_property("component_tags", [unreal.Name("LNP.Surface.Static"), unreal.Name("LNP.Surface.Blocker")])
            c.set_collision_profile_name("LNPStaticBlocker")
        else:
            c.set_editor_property("component_tags", [unreal.Name("LNP.Surface.Decoration")])
            c.set_collision_profile_name("LNPDecoration")
        placed.append((x, y, radius))
        row.update(placed=True, xy=[x, y], location=xyz(result["location"]),
                   slope_deg=round(result["slope"], 1), height_cm=round(result["height_cm"], 1),
                   corner_gaps_cm=result["corner_gaps_cm"], max_float_cm=result["max_float_cm"],
                   max_sink_cm=result["max_sink_cm"], rule=result["rule"],
                   distance_from_center_cm=round(math.hypot(x - CENTER[0], y - CENTER[1])),
                   profile=str(c.get_collision_profile_name()), collision=str(c.get_collision_enabled()))
        report.append(row)
    write("Placement.json", {"start_dirty": start_dirty, "items": report, "dirty_after": dirty(),
                             "status": "memory only; do not save the WIP"})
    view("Sandstone_05", overview=True)
    unreal.log("Ridge variant close-up probe placed: %d/%d. Memory only; do not save the WIP." %
               (sum(1 for r in report if r.get("placed")), len(report)))


def view(item_id, overview=False):
    require_wip()
    actors = {a.get_actor_label()[len(PREFIX):]: a for a in probes()}
    assert actors, "No probe actors."
    if overview:
        targets = list(actors.values())
        lx, ly = CENTER
    else:
        targets = [actors[item_id]]
        lx, ly, _ = spec.local(xyz(targets[0].get_actor_location()))
    center = unreal.Vector(*spec.point(lx, ly, spec.ground(lx, ly)))
    up = center * (-1.0 / center.length())
    origin, extent = targets[0].get_actor_bounds(False) if not overview else (center, unreal.Vector(1500, 1500, 0))
    distance = max(400.0, extent.length() * 2.2)
    side = unreal.Vector(*spec.point(lx - 10, ly, spec.ground(lx, ly))) - center
    side -= up * side.dot(up)
    side /= side.length()
    eye = origin + side * distance + up * distance * .55
    forward = origin - eye
    forward /= forward.length()
    rotation = unreal.MathLibrary.make_rot_from_xz(forward, up)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(eye, rotation)
    unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors(targets)


def clear():
    require_wip()
    editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    removed = [a.get_actor_label() for a in probes()]
    for a in probes():
        editor.destroy_actor(a)
    write("Clear.json", {"removed": removed, "remaining": [a.get_actor_label() for a in probes()],
                         "dirty_after": dirty(), "note": "Discard WIP changes; do not save."})
    unreal.log("Ridge variant close-up probe cleared: %d actor(s). Do not save the WIP." % len(removed))


def open_wip():
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first."
    assert not dirty(), "Current map has unsaved changes; resolve them first."
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL)
    unreal.log("Opened WIP level for the variant close-up probe.")


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode == "open":
        open_wip()
    elif mode == "place":
        place()
    elif mode == "view":
        view(sys.argv[2])
    elif mode == "clear":
        clear()
    else:
        raise SystemExit("Usage: open | place | view <ID> | clear")
