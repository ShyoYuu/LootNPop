# Phase 9 Vertical Slice 옥탄트의 유기 형태 메시 생성기 (Blender 5.2).
#
# 실행: Blender MCP에서 exec(open(r"<이 파일>").read()) 후 generate("<옥탄트 이름>", export_dir).
# 좌표 규약:
#   - 정점은 옥탄트 로컬(UE, cm) 좌표 그대로다. 피벗 = 옥탄트 원점이라 UE에서는 원점에 배치한다.
#   - 위치는 C++ LNPSliceOctant과 같은 프레임(옥탄트 중앙 위도 35.26°·방위 45°)의 (X, Y) 호 길이와
#     높이 H(지각 기준 반지름 30,000cm에서 구 중심 쪽 거리)로 정한다.
#   - UE (x, y, z)cm -> Blender (x, -y, z)/100 m. FBX 임포트가 Y 반전과 100배를 되돌린다(스태틱 메시 규약).
#   - 메시 하나를 닫힌 형태로 만들고 면 법선과 그 지점 Up의 내적이 0.71 이상이면 _Floor(Support), 아니면 _Shell(Blocker)로 나눈다.
#     이름 접미사가 UE 역할 태그를 정한다(LNPSliceOctant.cpp 머리 주석).

import math
import os
import random

import bmesh
import bpy

CRUST_RADIUS = 30000.0
WALKABLE_DOT = 0.71

CENTER_LAT = 35.264389682754654
CENTER_AZ = 45.0


def v_add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def v_scale(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def v_dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def v_norm(a):
    length = math.sqrt(v_dot(a, a))
    return v_scale(a, 1.0 / length)


def center_frame():
    lat = math.radians(CENTER_LAT)
    az = math.radians(CENTER_AZ)
    radial = (math.cos(lat) * math.cos(az), math.cos(lat) * math.sin(az), math.sin(lat))
    tangent = (-math.sin(az), math.cos(az), 0.0)
    bitangent = (-math.sin(lat) * math.cos(az), -math.sin(lat) * math.sin(az), math.cos(lat))
    return radial, tangent, bitangent


def radial_at(x, y):
    radial, tangent, bitangent = center_frame()
    return v_norm(v_add(v_add(v_scale(radial, CRUST_RADIUS), v_scale(bitangent, x)), v_scale(tangent, y)))


def point(x, y, h):
    """로컬 (X, Y) 호 길이 위치의 높이 H 점(UE 옥탄트 로컬 cm)."""
    return v_scale(radial_at(x, y), CRUST_RADIUS - h)


def to_blender(p):
    return (p[0] / 100.0, -p[1] / 100.0, p[2] / 100.0)


# 바위 메사: 묻힌 바닥에서 솟은 불규칙 기둥과 완만한 윗면. 그래플로만 오른다.
# (이름, 중심 X, Y, 반경, 바닥 H, 윗면 H, 둘레 분할, 높이 링 수, 윗면 링 수, 둘레 요철 비율, 시드)
MESAS = {
    # Bastion_00은 거인의 성채 개편(2026-10-07)으로 메사 자리가 안뜰이 되어 비웠다.
    "Bastion_00": [],
    "Hollow_00": [],
}


def build_mesa(spec):
    name, cx, cy, radius, bottom_h, top_h, segments, rings, cap_rings, roughness, seed = spec
    rng = random.Random(seed)
    # 둘레 요철은 각도별 고정 계수로 두어 높이 방향으로 이어지게 한다. 위로 갈수록 조금 좁아진다.
    # 저주파 사인 합이라 둘레가 톱니가 아니라 완만한 바위 윤곽이 된다.
    waves = [(k, rng.random() * 2.0 * math.pi, rng.random()) for k in (2, 3, 5)]
    norm = sum(w for _, _, w in waves)
    wobble = [1.0 + roughness * sum(w * math.sin(k * 2.0 * math.pi * s / segments + p) for k, p, w in waves) / norm
              for s in range(segments)]
    bm = bmesh.new()
    rows = []
    for ring in range(rings + 1):
        t = ring / rings
        h = bottom_h + (top_h - bottom_h) * t
        taper = 1.0 - 0.15 * t
        row = []
        for s in range(segments):
            a = 2.0 * math.pi * s / segments
            r = radius * taper * wobble[s] * (1.0 + 0.04 * (rng.random() * 2.0 - 1.0))
            row.append(bm.verts.new(to_blender(point(cx + r * math.cos(a), cy + r * math.sin(a), h))))
        rows.append(row)
    for ring in range(rings):
        for s in range(segments):
            n = (s + 1) % segments
            bm.faces.new((rows[ring][s], rows[ring][n], rows[ring + 1][n], rows[ring + 1][s]))
    # 윗면: 가장자리 링에서 안쪽으로 완만하게 솟는 돔(최대 150cm).
    top_row = rows[-1]
    edge_r = radius * 0.85
    for cap in range(1, cap_rings + 1):
        t = cap / cap_rings
        inner = []
        for s in range(segments):
            a = 2.0 * math.pi * s / segments
            r = edge_r * wobble[s] * (1.0 - t)
            h = top_h + 150.0 * math.sin(0.5 * math.pi * t) + 20.0 * (rng.random() * 2.0 - 1.0) * (1.0 - t)
            inner.append(bm.verts.new(to_blender(point(cx + r * math.cos(a), cy + r * math.sin(a), h))) if cap < cap_rings else None)
        if cap < cap_rings:
            for s in range(segments):
                n = (s + 1) % segments
                bm.faces.new((top_row[s], top_row[n], inner[n], inner[s]))
            top_row = inner
        else:
            apex = bm.verts.new(to_blender(point(cx, cy, top_h + 150.0)))
            for s in range(segments):
                n = (s + 1) % segments
                bm.faces.new((top_row[s], top_row[n], apex))
    # 바닥 뚜껑(지각 아래라 보이지 않지만 닫힌 메시로 둔다).
    bm.faces.new(list(reversed(rows[0])))
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return name, bm


def split_and_link(bm, base_name, collection):
    """면 법선 · 그 지점 Up(구 중심 방향) 내적으로 Floor/Shell을 나눈다. Blender 좌표에서 구 중심은 원점이다."""
    bm.normal_update()
    floor_faces = set()
    for face in bm.faces:
        c = face.calc_center_median()
        up = v_norm((-c.x, -c.y, -c.z))
        n = face.normal
        if n.x * up[0] + n.y * up[1] + n.z * up[2] >= WALKABLE_DOT:
            floor_faces.add(face.index)
    objects = []
    for suffix, keep_floor in (("Floor", True), ("Shell", False)):
        copy = bm.copy()
        copy.faces.ensure_lookup_table()
        drop = [f for f in copy.faces if (f.index in floor_faces) != keep_floor]
        bmesh.ops.delete(copy, geom=drop, context="FACES")
        loose = [v for v in copy.verts if not v.link_faces]
        bmesh.ops.delete(copy, geom=loose, context="VERTS")
        if not copy.faces:
            copy.free()
            continue
        data = bpy.data.meshes.new(f"SM_{base_name}_{suffix}")
        copy.to_mesh(data)
        copy.free()
        obj = bpy.data.objects.new(data.name, data)
        collection.objects.link(obj)
        objects.append(obj)
    return objects


def generate(octant, export_dir):
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    # 남은 메시 데이터가 있으면 새 이름에 .001이 붙어 FBX·UE 에셋 이름이 바뀐다.
    for mesh in list(bpy.data.meshes):
        bpy.data.meshes.remove(mesh)
    collection = bpy.context.scene.collection
    os.makedirs(export_dir, exist_ok=True)

    report = {}
    for spec in MESAS.get(octant, []):
        name, bm = build_mesa(spec)
        for obj in split_and_link(bm, f"{octant}_{name}", collection):
            bpy.ops.object.select_all(action="DESELECT")
            obj.select_set(True)
            bpy.context.view_layer.objects.active = obj
            path = os.path.join(export_dir, obj.name + ".fbx")
            bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"},
                                     apply_scale_options="FBX_SCALE_NONE", global_scale=1.0,
                                     axis_forward="-Z", axis_up="Y", mesh_smooth_type="FACE",
                                     add_leaf_bones=False, bake_anim=False)
            report[obj.name] = {"fbx": path, "verts": len(obj.data.vertices), "faces": len(obj.data.polygons)}
        bm.free()
    return report
