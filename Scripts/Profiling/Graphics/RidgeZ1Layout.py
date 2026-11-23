"""UE Cmd에서 Z1 서쪽 산괴 시험 구성의 재현 원본(EDTerrainLayout_Z1.json)을 되읽고, 대조하고, 다시 만든다(GX-030).

재현 원본은 두 종류로 나뉜다.
- 손배치 층: 개별 StaticMeshActor(V002 사본·렌더링 지각)와 Level Instance(조립체·CR 산괴). 결과 transform을 옥탄트
  로컬(x, y, H, 현지 Up 기준 회전)로 담는다. 에디터에서 손으로 다듬은 배치는 export로 되읽는다.
- 인스턴스 층: 드레이프·띠·평면 묶음(합 약 14만 개). 생성기·버전·설정·시드와 입력(CR 구운 레벨 파일) 지문, 결과 지문
  (메시별 수·transform 해시)만 담는다. 인스턴스 손수정은 지원하지 않고 생성 규칙으로 고친다.

인자:
- export: 열린 Z1AssemblyWall 레벨을 JSON으로 쓴다(protection·crust_build는 기존 JSON 값을 유지한다).
- verify: 열린 Z1AssemblyWall 레벨을 JSON과 대조한다. 결과는 Saved Layout/Verify_<레벨 이름>.json.
- build <이름> [repro]: JSON에서 시험 레벨을 만든다. repro면 _Repro 폴더에 만들고 인스턴스 층도 _Repro 재생성본을 쓴다.
  조립체 시험 레벨(Z1AssemblyTest_V001)의 조명을 복사하므로 그 레벨을 열고(열려 있지 않으면 연다) 실행한다.
- regen <drape|band|flat> <버전>: 인스턴스 층을 _Repro 폴더에 다시 만들고 JSON 결과 지문과 대조한다.
충돌 지각·프록시(U5, GX-031)는 collision 역할 행으로 담는다(태그·프로필·가시성 포함, 원점 배치라 월드 자세).
같은 Cmd 실행 안에서 build 직후에는 Level Instance가 아직 로드되지 않으므로 verify는 다음 Cmd에서 돌린다.
"""
import hashlib
import importlib
import json
import math
from pathlib import Path
import re
import sys

import unreal

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parents[1])]
import BuildRidgeZ1AssemblyTest as test

importlib.reload(test)
spec = test.spec

LAYOUT = HERE / "EDTerrainLayout_Z1.json"
OUT = test.OUT / "Layout"
PROJECT = HERE.parents[2]
EDTERRAIN = "/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/"
REPRO_ROOT = EDTERRAIN + "_Repro/"
# export가 덮어쓰지 않고 기존 JSON에서 그대로 옮기는 항목(사람이 정한 결정·셸에서 잰 값).
PRESERVED = ("protection", "crust_build", "collision_build")
# V035·V036은 V034 시각 구성에 U5 충돌 지각·프록시만 더한 버전이라 조립체 높이 기준(WALLS)을 V034에서 읽는다.
WALLS_ALIAS = {"V035": "V034", "V036": "V034"}
LAYER_PATTERN = re.compile(r"/(Z1MassifDrape|Z1RimBand|Z1MassifFlat)_(V\d+)/")
LAYER_GENERATOR = {"Z1MassifDrape": "drape", "Z1RimBand": "band", "Z1MassifFlat": "flat"}
# 렌더링 지각 버전별 높이 함수(프록시 묻힘 판정용).
CRUST_GROUND = {"V010": spec.massif_flat_ground}
# 프록시 파생 제외 규칙: 메시 상자 꼭대기(현지 Up 방향 최고점)가 렌더링 지각보다 이만큼 넘게 아래면 묻힌 것으로 본다.
# 동굴 절개(spec.base.CUTS) 안은 지각이 없으므로 판정하지 않는다.
BURIED_MARGIN_CM = 50.0
CRUST_GRID_CM = 100.0
POSITION_TOLERANCE_CM = 0.05
AXIS_TOLERANCE = 1e-5


def world_package():
    return test.package(test.world())


def rounded(values, digits=6):
    # +0.0은 -0.0을 0.0으로 바꿔 JSON 차이를 없앤다.
    return [round(float(v), digits) + 0.0 for v in values]


def sha(data):
    return hashlib.sha256(json.dumps(data, separators=(",", ":")).encode()).hexdigest()


# ---- 옥탄트 로컬 자세 ------------------------------------------------------------------------------------------

def local_pose(transform):
    # 위치는 옥탄트 로컬 (x, y, H), 회전은 그 자리 현지 좌표계(test.frame_rotation, yaw 0) 기준 pitch·yaw·roll이다.
    p = transform.translation
    if p.length() < 1.0:
        # 옥탄트 메시(지각·보존 바위)는 정점이 월드 좌표라 원점에 둔다. 원점에서는 로컬 좌표가 정의되지 않아 월드 자세로 쓴다.
        r = transform.rotation.rotator()
        return {"world_cm": rounded(test.xyz(p), 4), "world_rotation_deg": rounded((r.pitch, r.yaw, r.roll), 6),
                "scale": rounded(test.xyz(transform.scale3d), 6)}
    x, y, h = spec.ridge.local((p.x, p.y, p.z))
    frame = unreal.Transform(unreal.Vector(0, 0, 0), test.frame_rotation(x, y, 0), unreal.Vector(1, 1, 1))
    rel = unreal.MathLibrary.inverse_transform_rotation(frame, transform.rotation.rotator())
    return {"local_xyh_cm": rounded((x, y, h), 4), "rotation_deg": rounded((rel.pitch, rel.yaw, rel.roll), 6),
            "scale": rounded(test.xyz(transform.scale3d), 6)}


def world_rotation(x, y, rotation_deg):
    pitch, yaw, roll = rotation_deg
    return unreal.MathLibrary.compose_rotators(unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll), test.frame_rotation(x, y, 0))


def pose_transform(pose):
    if "world_cm" in pose:
        pitch, yaw, roll = pose["world_rotation_deg"]
        return unreal.Transform(unreal.Vector(*pose["world_cm"]), unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll),
                                unreal.Vector(*pose["scale"]))
    x, y, h = pose["local_xyh_cm"]
    return unreal.Transform(unreal.Vector(*spec.ridge.point(x, y, h)), world_rotation(x, y, pose["rotation_deg"]),
                            unreal.Vector(*pose["scale"]))


def base_height(base, x, y):
    return {"floor": lambda: spec.ground(-1250, y), "local": lambda: spec.west_cover_ground(x, y),
            "abs": lambda: 0.0}[base]()


def pose_error(a, b):
    delta = (a.translation - b.translation).length()
    axis = max((a.rotation.rotate_vector(u) - b.rotation.rotate_vector(u)).length()
               for u in (unreal.Vector(1, 0, 0), unreal.Vector(0, 1, 0), unreal.Vector(0, 0, 1)))
    return delta, axis, (a.scale3d - b.scale3d).length()


# ---- 지문 --------------------------------------------------------------------------------------------------------

def instance_row(t):
    # 0.01cm 위치, 1e-4 축 성분, 1e-4 스케일. 쿼터니언 부호 모호성을 피하려고 회전은 X·Z 축 벡터로 쓴다.
    p, s = t.translation, t.scale3d
    ax = t.rotation.rotate_vector(unreal.Vector(1, 0, 0))
    az = t.rotation.rotate_vector(unreal.Vector(0, 0, 1))
    return (round(p.x * 100), round(p.y * 100), round(p.z * 100), round(ax.x * 1e4), round(ax.y * 1e4), round(ax.z * 1e4),
            round(az.x * 1e4), round(az.y * 1e4), round(az.z * 1e4), round(s.x * 1e4), round(s.y * 1e4), round(s.z * 1e4))


def component_key(comp):
    overrides = [test.package(m) if m else "" for m in comp.get_editor_property("override_materials")]
    kind = "ism" if isinstance(comp, unreal.InstancedStaticMeshComponent) else "sm"
    return "%s:%s|%s" % (kind, test.package(comp.static_mesh), ",".join(overrides))


def actor_instances(actors):
    # (키, 메시, 월드 transform) 목록. ISM은 인스턴스마다, 일반 StaticMeshComponent는 컴포넌트 하나가 한 개다.
    for actor in actors:
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            if not comp.static_mesh:
                continue
            key = component_key(comp)
            if isinstance(comp, unreal.InstancedStaticMeshComponent):
                for i in range(comp.get_instance_count()):
                    yield key, comp.static_mesh, comp.get_instance_transform(i, True)
            else:
                yield key, comp.static_mesh, comp.get_world_transform()


def fingerprint(actors):
    by_key, decals = {}, []
    for key, _, t in actor_instances(actors):
        by_key.setdefault(key, []).append(instance_row(t))
    for actor in actors:
        if isinstance(actor, unreal.DecalActor):
            size = actor.decal.get_editor_property("decal_size")
            decals.append([test.package(actor.decal.get_decal_material()), list(instance_row(actor.get_actor_transform())),
                           round(size.x * 100), round(size.y * 100), round(size.z * 100),
                           actor.decal.get_editor_property("sort_order")])
    detail = {key: [len(rows), sha(sorted(rows))] for key, rows in sorted(by_key.items())}
    decals.sort()
    if decals:
        detail["decal"] = [len(decals), sha(decals)]
    return {"instances": sum(len(rows) for rows in by_key.values()), "decals": len(decals), "keys": len(detail),
            "sha256": sha(sorted(detail.items()))}, detail


def file_fingerprint(level):
    path = PROJECT / "Content" / (level[len("/Game/"):] + ".umap")
    data = path.read_bytes()
    return {"level": level, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


# ---- 열린 레벨 읽기 ----------------------------------------------------------------------------------------------

def level_actors():
    # 영속 레벨 액터와 Level Instance가 로드한 레벨별 액터를 나눈다.
    top, nested = [], {}
    persistent = world_package()
    for actor in unreal.GameplayStatics.get_all_actors_of_class(test.world(), unreal.Actor):
        level = actor.get_level()
        if level.get_outermost().get_name() == persistent:
            top.append(actor)
        else:
            nested.setdefault(level.get_path_name(), []).append(actor)
    return top, nested


def layer_info(level):
    match = LAYER_PATTERN.search(level)
    if not match:
        return None
    generator, version = LAYER_GENERATOR[match.group(1)], match.group(2)
    if generator == "drape":
        crust, groups = test.DRAPE_VERSIONS[version]
        config = {"crust": crust, "groups": list(groups), "slope_rule": test.DRAPE_SLOPE.get(version),
                  "material_swap": test.DRAPE_MATERIAL_SWAP.get(version, {})}
        sources = list(groups)
    elif generator == "band":
        config = test.BAND_VERSIONS[version]
        sources = [config["source"]]
    else:
        config = test.FLAT_VERSIONS[version]
        sources = list(config["groups"])
    return {"generator": generator, "version": version, "command": "%s %s" % (generator, version),
            "config": json.loads(json.dumps(config)), "seed": config.get("seed"),
            "source": [file_fingerprint(test.CR + group) for group in sources]}


class CrustHeight:
    # 렌더링 지각 높이를 1m 격자 점에 캐시하고 쌍선형 보간한다(높이 함수 호출이 느리다).
    def __init__(self, ground):
        self.ground, self.cache = ground, {}

    def at(self, i, j):
        if (i, j) not in self.cache:
            self.cache[(i, j)] = self.ground(i * CRUST_GRID_CM, j * CRUST_GRID_CM)
        return self.cache[(i, j)]

    def __call__(self, x, y):
        fx, fy = x / CRUST_GRID_CM, y / CRUST_GRID_CM
        i, j = math.floor(fx), math.floor(fy)
        u, v = fx - i, fy - j
        return ((self.at(i, j) * (1 - u) + self.at(i + 1, j) * u) * (1 - v)
                + (self.at(i, j + 1) * (1 - u) + self.at(i + 1, j + 1) * u) * v)


def buried(actors, crust):
    # 메시 상자 꼭대기(현지 Up 쪽 최고점)가 지각보다 BURIED_MARGIN_CM 넘게 아래인 인스턴스. 상자 중심이 지각보다 위면 건너뛴다.
    rows, boxes = [], {}
    for key, mesh, t in actor_instances(actors):
        name = mesh.get_path_name()
        if name not in boxes:
            boxes[name] = mesh.get_bounding_box()
        box = boxes[name]
        center = unreal.MathLibrary.transform_location(t, (box.min + box.max) * 0.5)
        x, y, h = spec.ridge.local((center.x, center.y, center.z))
        ground = crust(x, y)
        if h > ground - BURIED_MARGIN_CM:
            continue
        # signed_distance는 다각형 안쪽이 양수다.
        if any(spec.ridge.signed_distance(x, y, cut) > 0 for cut in spec.base.CUTS):
            continue
        top = max(spec.ridge.local(test.xyz(unreal.MathLibrary.transform_location(t, unreal.Vector(bx, by, bz))))[2]
                  for bx in (box.min.x, box.max.x) for by in (box.min.y, box.max.y) for bz in (box.min.z, box.max.z))
        if top < ground - BURIED_MARGIN_CM:
            rows.append({"key": key, "local_xyh_cm": rounded((x, y, h), 1), "top_below_crust_cm": round(ground - top, 1)})
    return rows


def collect(with_buried=True):
    # 열린 Z1AssemblyWall 레벨을 재현 원본 형태로 읽는다. 상세(지문 키별 해시·묻힘 목록)는 따로 돌려준다.
    level = world_package()
    match = re.search(r"Z1AssemblyWall_(V\d+)", level)
    assert match, level
    version = match.group(1)
    walls = {row[0]: test.wall_instance(row)["meta"] for row in test.WALLS.get(WALLS_ALIAS.get(version, version), ())}
    top, nested = level_actors()
    layout = {"schema": 1, "version": version, "level": level.replace("/_Repro/", "/"),
              "lighting": {"source_level": test.LEVEL, "lights": [], "sky": None},
              "actors": [], "level_instances": [], "layers": {}, "assemblies": {}}
    detail = {"layers": {}, "assemblies": {}, "buried": {}}
    crust_ground = None
    for actor in sorted(top, key=lambda a: a.get_actor_label()):
        cls = actor.get_class().get_name()
        if cls in test.LIGHT_CLASSES:
            layout["lighting"]["lights"].append({"label": actor.get_actor_label(), "class": cls,
                                                 "location": rounded(test.xyz(actor.get_actor_location()), 3)})
        elif isinstance(actor, unreal.SkyLight):
            comp = actor.light_component
            layout["lighting"]["sky"] = {"cubemap": test.package(comp.get_editor_property("cubemap")),
                                         "intensity": round(comp.get_editor_property("intensity"), 4),
                                         "lower_hemisphere_is_black": comp.get_editor_property("lower_hemisphere_is_black")}
        elif isinstance(actor, unreal.StaticMeshActor) and \
                actor.static_mesh_component.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
            # U5 충돌 지각·프록시(GX-031). 태그·프로필·가시성까지 재현 원본에 담는다.
            comp = actor.static_mesh_component
            mesh = test.package(comp.static_mesh)
            role = "Floor" if mesh.endswith("_Floor") else "Shell" if mesh.endswith("_Shell") else "Crust"
            layout["actors"].append({"label": actor.get_actor_label(), "role": "collision", "collision_role": role,
                                     "mesh": mesh, "material_overrides": {},
                                     "tags": [str(t) for t in comp.get_editor_property("component_tags")],
                                     "profile": str(comp.get_collision_profile_name()), "visible": comp.is_visible(),
                                     **local_pose(actor.get_actor_transform())})
        elif isinstance(actor, unreal.StaticMeshActor):
            comp = actor.static_mesh_component
            mesh = test.package(comp.static_mesh)
            assert comp.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION, actor.get_actor_label()
            crust = re.search(r"/Z1MassifCover_(V\d+)/", mesh)
            row = {"label": actor.get_actor_label(), "role": "render_crust" if crust else "v002", "mesh": mesh,
                   "material_overrides": {str(i): test.package(m) for i, m in
                                          enumerate(comp.get_editor_property("override_materials")) if m},
                   **local_pose(actor.get_actor_transform())}
            if crust:
                row["crust_version"] = crust.group(1)
                crust_ground = CRUST_GROUND[crust.group(1)]
            layout["actors"].append(row)
        elif isinstance(actor, unreal.LevelInstance):
            label = actor.get_actor_label()
            level_path = test.package(actor.get_world_asset())
            canonical = level_path.replace("/_Repro/", "/")
            t = actor.get_actor_transform()
            loaded = actor.get_loaded_level()
            assert loaded, "Level Instance not loaded (run in a separate Cmd after opening the level): " + label
            members = nested.get(loaded.get_path_name(), [])
            info = layer_info(canonical)
            if info:
                r = t.rotation.rotator()
                assert t.translation.length() < 1e-3 and max(abs(r.pitch), abs(r.yaw), abs(r.roll)) < 1e-4, label
                row = {"label": label, "role": "layer", "level": canonical, "height_base": "world"}
                summary, detail["layers"][label] = fingerprint(members)
                layout["layers"][label] = dict(info, level=canonical, result=summary)
            else:
                pose = local_pose(t)
                x, y, h = pose["local_xyh_cm"]
                base = walls.get(label, {}).get("height_base", "abs")
                pitch, yaw, roll = pose["rotation_deg"]
                row = {"label": label, "role": "massif" if canonical.startswith(test.CR) else "assembly",
                       "level": canonical, "local_xy_cm": [x, y], "yaw_deg": rounded([yaw], 4)[0],
                       "tilt_deg": rounded((pitch, roll), 4), "height_base": base,
                       "height_offset_cm": rounded([h - base_height(base, x, y)], 3)[0], "scale": pose["scale"][0]}
                assert max(pose["scale"]) - min(pose["scale"]) < 1e-6, label
                summary, detail["assemblies"][label] = fingerprint(members)
                layout["assemblies"][label] = summary
                if with_buried:
                    detail["buried"][label] = members
            layout["level_instances"].append(row)
    assert crust_ground, "render crust actor not found"
    if with_buried:
        crust = CrustHeight(crust_ground)
        excluded = {}
        for label, members in list(detail["buried"].items()):
            rows = buried(members, crust)
            detail["buried"][label] = rows
            if rows:
                # 메시별 목록은 Saved(ProxyExclude_<버전>.json)에 두고 JSON에는 Level Instance별 수만 담는다.
                excluded[label] = len(rows)
        layout["proxy"] = {"exclude_rule": {"kind": "buried", "crust": "render", "margin_cm": BURIED_MARGIN_CM,
                                            "skip_inside_cuts": True, "scope": "assemblies+massif"},
                           "excluded_total": sum(len(r) for r in detail["buried"].values()),
                           "excluded": excluded,
                           "list": str((OUT / ("ProxyExclude_%s.json" % version)).relative_to(PROJECT)).replace("\\", "/")}
    return layout, detail


# ---- 명령 --------------------------------------------------------------------------------------------------------

def export():
    assert not test.dirty(), test.dirty()
    OUT.mkdir(parents=True, exist_ok=True)
    layout, detail = collect()
    old = json.loads(LAYOUT.read_text(encoding="utf-8")) if LAYOUT.exists() else {}
    for key in PRESERVED:
        if key in old:
            layout[key] = old[key]
    temporary = LAYOUT.with_suffix(".tmp")
    temporary.write_text(json.dumps(layout, indent=1, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")
    temporary.replace(LAYOUT)
    (OUT / ("ProxyExclude_%s.json" % layout["version"])).write_text(json.dumps(detail["buried"], indent=1), encoding="utf-8")
    (OUT / ("Fingerprint_%s.json" % layout["version"])).write_text(
        json.dumps({"layers": detail["layers"], "assemblies": detail["assemblies"]}, indent=1), encoding="utf-8")
    unreal.log("Z1_LAYOUT_EXPORT %s actors %d instances %d layers %d excluded %d" % (
        layout["version"], len(layout["actors"]), len(layout["level_instances"]), len(layout["layers"]),
        layout["proxy"]["excluded_total"]))


def verify():
    OUT.mkdir(parents=True, exist_ok=True)
    expected = json.loads(LAYOUT.read_text(encoding="utf-8"))
    actual, detail = collect()
    issues = []

    def check(ok, what):
        if not ok:
            issues.append(what)

    check(actual["version"] == expected["version"], "version %s != %s" % (actual["version"], expected["version"]))
    check(actual["lighting"] == expected["lighting"], "lighting")
    got = {row["label"]: row for row in actual["actors"]}
    check(set(got) == {row["label"] for row in expected["actors"]}, "actor labels")
    worst = {"actor_cm": 0.0, "actor_axis": 0.0, "instance_cm": 0.0, "instance_axis": 0.0}
    for row in expected["actors"]:
        other = got.get(row["label"])
        if not other:
            continue
        check(other["mesh"] == row["mesh"] and other["material_overrides"] == row["material_overrides"], row["label"] + " mesh")
        if row["role"] == "collision":
            check(other["role"] == "collision" and sorted(other["tags"]) == sorted(row["tags"])
                  and other["profile"] == row["profile"] and other["visible"] == row["visible"], row["label"] + " collision")
        delta, axis, scale = pose_error(pose_transform(row), pose_transform(other))
        worst["actor_cm"], worst["actor_axis"] = max(worst["actor_cm"], delta), max(worst["actor_axis"], axis)
        check(delta <= POSITION_TOLERANCE_CM and axis <= AXIS_TOLERANCE and scale < 1e-5, row["label"] + " pose")
    got = {row["label"]: row for row in actual["level_instances"]}
    check(set(got) == {row["label"] for row in expected["level_instances"]}, "level instance labels")
    for row in expected["level_instances"]:
        other = got.get(row["label"])
        if not other:
            continue
        check(other["level"] == row["level"] and other["role"] == row["role"], row["label"] + " level")
        if row["role"] == "layer":
            continue
        delta, axis, scale = pose_error(instance_transform(row), instance_transform(other))
        worst["instance_cm"], worst["instance_axis"] = max(worst["instance_cm"], delta), max(worst["instance_axis"], axis)
        check(delta <= POSITION_TOLERANCE_CM and axis <= AXIS_TOLERANCE and scale < 1e-5, row["label"] + " pose")
    for label, layer in expected["layers"].items():
        check(actual["layers"].get(label, {}).get("result") == layer["result"], label + " layer fingerprint")
    check(actual["assemblies"] == expected["assemblies"], "assembly fingerprints")
    check(actual["proxy"]["excluded"] == expected["proxy"]["excluded"], "proxy excluded")
    name = world_package().rsplit("/", 1)[1] + ("_Repro" if "/_Repro/" in world_package() else "")
    report = {"level": world_package(), "layout": str(LAYOUT), "pass": not issues, "issues": issues, "worst": worst,
              "layers": {label: actual["layers"][label]["result"] for label in actual["layers"]},
              "assemblies": actual["assemblies"], "proxy_excluded_total": actual["proxy"]["excluded_total"]}
    (OUT / ("Verify_%s.json" % name)).write_text(json.dumps(report, indent=1), encoding="utf-8")
    unreal.log("Z1_LAYOUT_VERIFY %s %s" % ("PASS" if not issues else "FAIL", json.dumps(issues)))


def instance_transform(row):
    x, y = row["local_xy_cm"]
    h = base_height(row["height_base"], x, y) + row["height_offset_cm"]
    pitch, roll = row["tilt_deg"]
    s = row["scale"]
    return unreal.Transform(unreal.Vector(*spec.ridge.point(x, y, h)), world_rotation(x, y, (pitch, row["yaw_deg"], roll)),
                            unreal.Vector(s, s, s))


def repro_level(level):
    assert level.startswith(EDTERRAIN), level
    return REPRO_ROOT + level[len(EDTERRAIN):]


def build(name, mode=""):
    assert mode in ("", "repro"), mode
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    assert not test.dirty(), test.dirty()
    if world_package() != test.LEVEL:
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(test.LEVEL), test.LEVEL
    level = test.WALL_FOLDER.format(name)
    if mode == "repro":
        level = repro_level(level)
    kept = []
    for row in layout["actors"]:
        t = pose_transform(row)
        rot = t.rotation.rotator()
        kept.append({"label": row["label"], "mesh": row["mesh"], "loc": test.xyz(t.translation),
                     "rot": [rot.pitch, rot.yaw, rot.roll], "scale": row["scale"],
                     "overrides": {int(slot): path for slot, path in row["material_overrides"].items()},
                     "collision": {k: row[k] for k in ("tags", "profile", "visible")} if row["role"] == "collision"
                     else None})
    instances = []
    for row in layout["level_instances"]:
        if row["role"] == "layer":
            target = repro_level(row["level"]) if mode == "repro" else row["level"]
            instances.append({"label": row["label"], "level": target, "location": unreal.Vector(0, 0, 0),
                              "rotation": unreal.Rotator(0, 0, 0), "scale": 1.0, "meta": {"height_base": "world"}})
        else:
            t = instance_transform(row)
            instances.append({"label": row["label"], "level": row["level"], "location": t.translation,
                              "rotation": t.rotation.rotator(), "scale": row["scale"],
                              "meta": {k: row[k] for k in ("local_xy_cm", "yaw_deg", "tilt_deg", "height_offset_cm",
                                                           "height_base")}})
    sky = layout["lighting"]["sky"]
    expected_lights = sorted(tuple(light["location"]) for light in layout["lighting"]["lights"])
    assert sorted(tuple(rounded(test.xyz(a.get_actor_location()), 3)) for a in test.light_actors()) == expected_lights
    placed = test.build_wall_level(level, kept, instances, {"cubemap": sky["cubemap"], "intensity": sky["intensity"]}
                                   if sky else None)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / ("Build_%s%s.json" % (name, "_Repro" if mode else ""))).write_text(
        json.dumps({"level": level, "actors": len(kept), "level_instances": placed}, indent=1), encoding="utf-8")
    unreal.log("Z1_LAYOUT_BUILD_READY " + level)


def regen(generator, version):
    # 생성기의 출력 레벨·보고서 경로만 _Repro로 바꿔 돌린다(규칙·시드·입력은 그대로).
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    expected = [layer for layer in layout["layers"].values()
                if layer["generator"] == generator and layer["version"] == version]
    assert len(expected) == 1, (generator, version)
    for source in expected[0]["source"]:
        assert file_fingerprint(source["level"]) == source, source["level"]
    test.OUT = OUT / "Repro"
    test.OUT.mkdir(parents=True, exist_ok=True)
    test.DRAPE_LEVEL = repro_level(test.DRAPE_LEVEL)
    test.BAND_LEVEL = repro_level(test.BAND_LEVEL)
    test.FLAT_LEVEL = repro_level(test.FLAT_LEVEL)
    {"drape": test.drape, "band": test.band, "flat": test.flat}[generator](version)
    assert "/_Repro/" in world_package(), world_package()
    top, _ = level_actors()
    summary, detail = fingerprint(top)
    match = summary == expected[0]["result"]
    (OUT / ("Regen_%s_%s.json" % (generator, version))).write_text(
        json.dumps({"level": world_package(), "match": match, "result": summary, "expected": expected[0]["result"],
                    "detail": detail}, indent=1), encoding="utf-8")
    unreal.log("Z1_LAYOUT_REGEN %s %s %s" % (generator, version, "MATCH" if match else "MISMATCH"))


if __name__ == "__main__":
    {"export": export, "verify": verify, "build": build, "regen": regen}[sys.argv[1]](*sys.argv[2:])
