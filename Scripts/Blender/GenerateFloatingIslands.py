# Phase 3b 부유섬·경사로 greybox 생성기 (Blender 5.2).
#
# 실행: Blender MCP에서 exec(open(r"<이 파일>").read()) 후 generate(export_dir).
# 좌표 규약:
#   - 모든 정점은 옥탄트 로컬(UE, cm) 기준 월드 중심에서의 방향 + 반지름으로 정의한다.
#     윗면은 월드 구와 중심을 공유하는 구면 곡면이다(D-048).
#   - 메시 피벗은 섬 윗면 중심. 로컬 +Z = Up(월드 중심 방향), +X = 위도 증가(적도) 방향.
#   - UE 로컬 (x, y, z)cm -> Blender (x, -y, z)/100 m. FBX 임포트가 Y 반전과 100배를 되돌린다.
#   - 섬 하나는 닫힌 메시로 만들어 법선을 바깥으로 맞춘 뒤 Top(Support)과 Body(측벽·밑면, Blocker)로 나눈다.

import math
import os

import bmesh
import bpy

TAN_RAMP = math.tan(math.radians(25.0))

# (이름, θ(+Z에서의 각), φ, 윗면 반지름, 섬 반지름, 측벽 높이, 밑면 깊이, 윗면 링 수, 둘레 분할)
ISLANDS = [
    ("Big", 54.7356, 45.0, 25800.0, 3000.0, 300.0, 1500.0, 20, 72),
    ("A", 35.0, 20.0, 28580.0, 1000.0, 250.0, 700.0, 8, 48),
    ("B", 35.0, 70.0, 27430.0, 1000.0, 300.0, 1500.0, 8, 48),
]

# 섬 A의 극 쪽(-X) 경사로. 섬 중심에서 위도 방향 호 길이(cm).
# 적도 쪽은 지각이 경사로와 거의 나란히 내려가 4,500cm를 가야 묻혀서 극 쪽을 쓴다. 적도 쪽에는 Sculpt 언덕을 비교용으로 둔다.
RAMP_A = {
    "side": -1.0,
    "width": 500.0,
    "start": 975.0,  # 윗면 반지름 기준. 섬 가장자리 25cm 안쪽에서 시작
    "end": 4000.0,  # 크러스트 샘플 척도(반지름 30,000). 지각과 만나는 약 3,400cm에서 300cm 더 묻힌 지점
    "step": 100.0,
    "bottom_radius": 30370.0,  # 경사로 아래 최대 지각 반지름 + 300
    "lift": 1.0,  # 윗면과 z-fighting을 피하려고 1cm 위
    # 시험용 간이 동굴(D-035 동굴 키트가 아니다). 경사로 몸체에서 파낸다. s는 크러스트 샘플 척도 호 길이,
    # w는 경사로 폭 방향, r은 월드 중심에서의 반지름. r 바깥 끝을 경사로 바닥(bottom_radius)보다 깊게 두어
    # 바닥이 원래 지각이 되게 한다.
    "cutouts": [
        # A. 관통 터널: 지각(약 29,763) 위 400cm 천장, 경사로를 옆으로 관통
        {"s": (1900.0, 2300.0), "w": (-450.0, 450.0), "r": (29363.0, 30600.0)},
        # B. 방: 경사로 한쪽 옆면(+w)으로 열림, 지붕 두께 100cm 이상
        {"s": (1300.0, 1600.0), "w": (-150.0, 450.0), "r": (28930.0, 30600.0)},
        # B. 수직 구멍: 경사로 윗면에서 방까지
        {"s": (1400.0, 1600.0), "w": (-100.0, 100.0), "r": (28500.0, 28950.0)},
    ],
}
CRUST_SAMPLE_RADIUS = 30000.0


def v_add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def v_scale(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def v_dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def v_cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def v_norm(a):
    length = math.sqrt(v_dot(a, a))
    return v_scale(a, 1.0 / length)


def island_frame(theta_deg, phi_deg):
    th = math.radians(theta_deg)
    ph = math.radians(phi_deg)
    outward = (math.sin(th) * math.cos(ph), math.sin(th) * math.sin(ph), math.cos(th))
    x_axis = (math.cos(th) * math.cos(ph), math.cos(th) * math.sin(ph), -math.sin(th))
    z_axis = v_scale(outward, -1.0)
    y_axis = v_cross(z_axis, x_axis)
    return outward, x_axis, y_axis, z_axis


def direction(outward, x_axis, y_axis, angle, azimuth):
    tangent = v_add(v_scale(x_axis, math.cos(azimuth)), v_scale(y_axis, math.sin(azimuth)))
    return v_norm(v_add(v_scale(outward, math.cos(angle)), v_scale(tangent, math.sin(angle))))


def rock_noise(ring, seg, segments):
    # 결정론적 저주파 요철. 윗면 가장자리(측벽 첫 링까지)는 원형을 유지한다.
    a = 2.0 * math.pi * seg / segments
    return 1.0 + 0.07 * math.sin(3.0 * a + ring * 1.3) + 0.04 * math.sin(7.0 * a + ring * 2.1)


class LocalMesh:
    def __init__(self, frame, top_radius):
        outward, x_axis, y_axis, z_axis = frame
        self.axes = (x_axis, y_axis, z_axis)
        self.origin = v_scale(outward, top_radius)
        self.verts = []
        self.faces = []  # (정점 인덱스들, part) part: 0=Top, 1=Body

    def add(self, world_point):
        d = (world_point[0] - self.origin[0], world_point[1] - self.origin[1], world_point[2] - self.origin[2])
        local = tuple(v_dot(d, axis) for axis in self.axes)
        self.verts.append(local)
        return len(self.verts) - 1


def build_island(spec):
    name, theta, phi, top_r, radius, wall, depth, rings, segments = spec
    frame = island_frame(theta, phi)
    outward, x_axis, y_axis, _ = frame
    mesh = LocalMesh(frame, top_r)
    rim_angle = radius / top_r

    center = mesh.add(v_scale(outward, top_r))
    top_rings = []
    for k in range(1, rings + 1):
        angle = rim_angle * k / rings
        top_rings.append([mesh.add(v_scale(direction(outward, x_axis, y_axis, angle, 2 * math.pi * s / segments), top_r))
                          for s in range(segments)])

    for s in range(segments):
        mesh.faces.append(([center, top_rings[0][s], top_rings[0][(s + 1) % segments]], 0))
    for k in range(rings - 1):
        inner, outer = top_rings[k], top_rings[k + 1]
        for s in range(segments):
            n = (s + 1) % segments
            mesh.faces.append(([inner[s], outer[s], outer[n], inner[n]], 0))

    # 측벽: 가장자리 방향 그대로 wall만큼 바깥(아래)으로. 이후 밑면: 깊이가 늘수록 반경이 줄어 끝점으로 모인다.
    body_rings = [top_rings[-1]]
    taper_steps = 6
    levels = [(wall, 1.0)]
    for t in range(1, taper_steps + 1):
        f = t / (taper_steps + 1)
        levels.append((wall + (depth - wall) * f, (1.0 - f) ** 0.7))
    for index, (d, shrink) in enumerate(levels):
        ring = []
        for s in range(segments):
            noise = 1.0 if index == 0 else rock_noise(index, s, segments)
            angle = rim_angle * shrink * noise
            ring.append(mesh.add(v_scale(direction(outward, x_axis, y_axis, angle, 2 * math.pi * s / segments), top_r + d)))
        body_rings.append(ring)
    for k in range(len(body_rings) - 1):
        upper, lower = body_rings[k], body_rings[k + 1]
        for s in range(segments):
            n = (s + 1) % segments
            mesh.faces.append(([upper[s], upper[n], lower[n], lower[s]], 1))
    tip = mesh.add(v_scale(outward, top_r + depth))
    last = body_rings[-1]
    for s in range(segments):
        mesh.faces.append(([last[s], last[(s + 1) % segments], tip], 1))
    return mesh


def build_ramp(island_spec, ramp):
    name, theta, phi, top_r, *_ = island_spec
    frame = island_frame(theta, phi)
    outward, x_axis, y_axis, _ = frame
    mesh = LocalMesh(frame, top_r)
    half_w = ramp["width"] * 0.5

    # 섬 쪽 거리(start·rim)는 윗면 반지름 기준, 끝점은 크러스트 샘플 척도라 모두 각도로 바꿔 쓴다.
    rim_beta = island_spec[4] / top_r
    start_beta = ramp["start"] / top_r
    end_beta = ramp["end"] / CRUST_SAMPLE_RADIUS
    step_beta = ramp["step"] / top_r
    sections = []
    stations = []
    beta = start_beta
    while beta < end_beta:
        stations.append(beta)
        beta += step_beta
    stations.append(end_beta)
    for beta in stations:
        # 오르막 길이는 경사면 자체의 호 길이로 잰다(반지름이 커질수록 약간 길어지지만 25° 근사로 충분).
        rise = max(0.0, beta - rim_beta) * top_r * TAN_RAMP
        top = top_r - ramp["lift"] + rise
        section = []
        for w, r in ((-half_w, top), (half_w, top), (half_w, ramp["bottom_radius"]), (-half_w, ramp["bottom_radius"])):
            gamma = w / r
            d = v_norm(v_add(v_add(v_scale(outward, math.cos(beta) * math.cos(gamma)),
                                   v_scale(x_axis, ramp["side"] * math.sin(beta) * math.cos(gamma))),
                             v_scale(y_axis, math.sin(gamma))))
            section.append(mesh.add(v_scale(d, r)))
        sections.append(section)

    for a, b in zip(sections, sections[1:]):
        mesh.faces.append(([a[0], b[0], b[1], a[1]], 0))  # 윗면
        mesh.faces.append(([a[1], b[1], b[2], a[2]], 1))
        mesh.faces.append(([a[2], b[2], b[3], a[3]], 1))
        mesh.faces.append(([a[3], b[3], b[0], a[0]], 1))
    first, last = sections[0], sections[-1]
    mesh.faces.append(([first[0], first[1], first[2], first[3]], 1))
    mesh.faces.append(([last[3], last[2], last[1], last[0]], 1))
    return mesh


def build_cutter(island_spec, ramp, cut):
    """경사로와 같은 구면 좌표로 정의한 닫힌 상자. 경사로에서 빼낼 공간이다."""
    name, theta, phi, top_r, *_ = island_spec
    frame = island_frame(theta, phi)
    outward, x_axis, y_axis, _ = frame
    mesh = LocalMesh(frame, top_r)
    corners = {}
    for i, s in enumerate(cut["s"]):
        for j, w in enumerate(cut["w"]):
            for k, r in enumerate(cut["r"]):
                beta = s / CRUST_SAMPLE_RADIUS
                gamma = w / r
                d = v_norm(v_add(v_add(v_scale(outward, math.cos(beta) * math.cos(gamma)),
                                       v_scale(x_axis, ramp["side"] * math.sin(beta) * math.cos(gamma))),
                                 v_scale(y_axis, math.sin(gamma))))
                corners[(i, j, k)] = mesh.add(v_scale(d, r))
    c = corners
    for quad in (((0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)), ((0, 0, 1), (0, 1, 1), (1, 1, 1), (1, 0, 1)),
                 ((0, 0, 0), (0, 0, 1), (1, 0, 1), (1, 0, 0)), ((0, 1, 0), (1, 1, 0), (1, 1, 1), (0, 1, 1)),
                 ((0, 0, 0), (0, 1, 0), (0, 1, 1), (0, 0, 1)), ((1, 0, 0), (1, 0, 1), (1, 1, 1), (1, 1, 0))):
        mesh.faces.append(([c[q] for q in quad], 1))
    return mesh


def local_mesh_to_bmesh(mesh):
    bm = bmesh.new()
    bverts = [bm.verts.new((x / 100.0, -y / 100.0, z / 100.0)) for x, y, z in mesh.verts]
    bm.verts.ensure_lookup_table()
    part_layer = bm.faces.layers.int.new("part")
    for indices, part in mesh.faces:
        face = bm.faces.new([bverts[i] for i in indices])
        face[part_layer] = part
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return bm


def subtract_cutters(bm, cutters, top_r, collection):
    """Boolean(Exact)으로 동굴을 파내고, 윗면(월드 중심을 향한 면)만 part 0으로 다시 분류한다.

    Boolean이 만든 면은 천장(바깥을 향함)과 벽(옆을 향함)뿐이라 방향 하나로 윗면과 확실히 구분된다.
    """
    host_data = bpy.data.meshes.new("LNPCutHost")
    bm.to_mesh(host_data)
    host = bpy.data.objects.new("LNPCutHost", host_data)
    collection.objects.link(host)
    cutter_objects = []
    for index, cutter in enumerate(cutters):
        cbm = local_mesh_to_bmesh(cutter)
        cdata = bpy.data.meshes.new(f"LNPCutter{index}")
        cbm.to_mesh(cdata)
        cbm.free()
        cobj = bpy.data.objects.new(f"LNPCutter{index}", cdata)
        collection.objects.link(cobj)
        cutter_objects.append(cobj)
        modifier = host.modifiers.new(name=f"Cut{index}", type="BOOLEAN")
        modifier.operation = "DIFFERENCE"
        modifier.solver = "EXACT"
        modifier.object = cobj
    bpy.context.view_layer.update()
    evaluated = host.evaluated_get(bpy.context.evaluated_depsgraph_get())
    result_data = bpy.data.meshes.new_from_object(evaluated)
    for obj in cutter_objects + [host]:
        bpy.data.objects.remove(obj, do_unlink=True)

    result = bmesh.new()
    result.from_mesh(result_data)
    bpy.data.meshes.remove(result_data)
    result.normal_update()
    part_layer = result.faces.layers.int.get("part") or result.faces.layers.int.new("part")
    # 섬 로컬(UE) 원점이 윗면 중심이고 +Z가 Up이므로 월드 중심은 로컬 (0, 0, top_r) → Blender (0, 0, top_r/100).
    center = (0.0, 0.0, top_r / 100.0)
    for face in result.faces:
        c = face.calc_center_median()
        up = v_norm((center[0] - c.x, center[1] - c.y, center[2] - c.z))
        n = face.normal
        face[part_layer] = 0 if (n.x * up[0] + n.y * up[1] + n.z * up[2]) > 0.5 else 1
    bm.free()
    return result


def to_blender_objects(mesh, base_name, collection, cutters=None, top_r=0.0):
    bm = local_mesh_to_bmesh(mesh)
    if cutters:
        bm = subtract_cutters(bm, cutters, top_r, collection)

    objects = []
    for part, suffix in ((0, "Top"), (1, "Body")):
        copy = bm.copy()
        layer = copy.faces.layers.int["part"]
        bmesh.ops.delete(copy, geom=[f for f in copy.faces if f[layer] != part], context="FACES")
        loose = [v for v in copy.verts if not v.link_faces]
        bmesh.ops.delete(copy, geom=loose, context="VERTS")
        data = bpy.data.meshes.new(f"SM_{base_name}_{suffix}")
        copy.to_mesh(data)
        copy.free()
        obj = bpy.data.objects.new(f"SM_{base_name}_{suffix}", data)
        collection.objects.link(obj)
        objects.append(obj)
    bm.free()
    return objects


def placement(spec):
    """UE 배치용 월드(옥탄트 로컬) 위치와 축. 회전은 UE 쪽에서 축으로부터 계산한다."""
    name, theta, phi, top_r, *_ = spec
    outward, x_axis, y_axis, z_axis = island_frame(theta, phi)
    return {"location": v_scale(outward, top_r), "x": x_axis, "y": y_axis, "z": z_axis}


def generate(export_dir):
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    collection = bpy.context.scene.collection
    os.makedirs(export_dir, exist_ok=True)

    built = {}
    for spec in ISLANDS:
        built[f"Island_{spec[0]}"] = (build_island(spec), spec, None)
    ramp_cutters = [build_cutter(ISLANDS[1], RAMP_A, cut) for cut in RAMP_A.get("cutouts", [])]
    built["Ramp_A_Cave"] = (build_ramp(ISLANDS[1], RAMP_A), ISLANDS[1], ramp_cutters)

    report = {}
    for base_name, (mesh, spec, cutters) in built.items():
        for obj in to_blender_objects(mesh, base_name, collection, cutters, spec[3]):
            bpy.ops.object.select_all(action="DESELECT")
            obj.select_set(True)
            bpy.context.view_layer.objects.active = obj
            path = os.path.join(export_dir, obj.name + ".fbx")
            bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"},
                                     apply_scale_options="FBX_SCALE_NONE", global_scale=1.0,
                                     axis_forward="-Z", axis_up="Y", mesh_smooth_type="FACE",
                                     add_leaf_bones=False, bake_anim=False)
            report[obj.name] = {"fbx": path, "verts": len(obj.data.vertices), "faces": len(obj.data.polygons),
                                "placement": placement(spec)}
    return report
