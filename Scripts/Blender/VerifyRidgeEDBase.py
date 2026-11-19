"""V012의 경계·보행로·Pod·착지를 광선으로 검증하고 실제 메시 위 마커 위치를 기록한다."""
import json
import math
from pathlib import Path
import sys

import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, str(Path(__file__).resolve().parent))
from BuildRidgeEDBase import OUT, base, builder

ridge = base.ridge
ENTRY_DEPTHS = (100, 1000, 2500, 4000, 5500)
WALK_SLOPE_DEG = 44.8


def build_bvh(meshes):
    vertices, faces, owners = [], [], []
    for obj in meshes:
        obj.data.calc_loop_triangles()
        start = len(vertices)
        vertices.extend(obj.matrix_world @ v.co for v in obj.data.vertices)
        for triangle in obj.data.loop_triangles:
            faces.append(tuple(start + i for i in triangle.vertices))
            owners.append(obj.name)
    return BVHTree.FromPolygons(vertices, faces, all_triangles=True), owners


def verify():
    bpy.ops.wm.open_mainfile(filepath=str(OUT / "Ridge_00_V012.blend"))
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH" and obj.name.startswith("SM_")]
    bvh, owners = build_bvh(meshes)
    failures = []

    def ray(start, end):
        origin, target = (builder.bl(ridge.point(*p)) for p in (start, end))
        delta = target - origin
        hit, normal, index, _ = bvh.ray_cast(origin, delta.normalized(), delta.length)
        if hit is None:
            return None
        slope = math.degrees(math.acos(min(1, abs(normal.dot(-hit.normalized())))))
        return ridge.local(tuple(builder.ue(hit)))[2], slope, owners[index]

    def floor(label, x, y, layer, max_slope=WALK_SLOPE_DEG):
        expected = base.base_ground(x, y) if layer == "surface" else ridge.layer_height(x, y, layer)
        hit = ray((x, y, expected + 300), (x, y, expected - 300))
        if hit is None or hit[1] > max_slope or hit[2].endswith("_Shell"):
            failures.append({"test": "floor", "id": label, "local": [x, y, expected], "hit": hit})
        return hit if hit is not None else (expected, 0.0, None)

    def clear(label, start, end):
        hit = ray(start, end)
        if hit:
            failures.append({"test": "clearance", "id": label, "from": start, "to": end, "hit": hit})

    routes = {}
    for route in ridge.active(ridge.ROUTES):
        label, layer = route["id"], ridge.route_layer(route)
        centerline, slopes, shifts = [], [], []
        for (ax, ay), (bx, by) in zip(route["xy"], route["xy"][1:]):
            length = math.hypot(bx - ax, by - ay)
            nx, ny = -(by - ay) / length, (bx - ax) / length
            count = max(1, int(length / 200))  # V011 검사와 같은 표본을 사용한다.
            for i in range(count):
                x, y = ax + (bx - ax) * i / count, ay + (by - ay) * i / count
                for offset in (-200, 0, 200):
                    px, py = x + nx * offset, y + ny * offset
                    h, slope, _ = floor(label, px, py, layer)
                    slopes.append(slope)
                    shifts.append(abs(h - ridge.layer_height(px, py, layer)))
                    if offset == 0:
                        centerline.append((x, y, h))
        for (x0, y0, h0), (x1, y1, h1) in zip(centerline, centerline[1:]):
            for lift in (60, 180):
                clear(label, (x0, y0, h0 + lift), (x1, y1, h1 + lift))
        routes[label] = {"floor_rays": len(slopes), "max_slope_deg": max(slopes), "max_spec_height_delta_cm": max(shifts)}

    corners = [ridge.local(p)[:2] for p in ((0, 0, ridge.RADIUS), (ridge.RADIUS, 0, 0), (0, ridge.RADIUS, 0))]
    entry_rays = 0
    for edge, (a, b) in enumerate(zip(corners, corners[1:] + corners[:1])):
        for i in range(1, 20):
            ex, ey = (a[0] + (b[0] - a[0]) * i / 20, a[1] + (b[1] - a[1]) * i / 20)
            for depth in ENTRY_DEPTHS:
                low, high = 0.0, 1.0
                for _ in range(25):
                    t = (low + high) / 2
                    if min(ridge.point(ex * (1 - t), ey * (1 - t), 0)) < depth:
                        low = t
                    else:
                        high = t
                x, y = ex * (1 - high), ey * (1 - high)
                hit = ray((x, y, 1500), (x, y, -1500))
                if hit is None or hit[1] > WALK_SLOPE_DEG or hit[2].endswith("_Shell"):
                    failures.append({"test": "entry", "edge": edge, "local_xy": [x, y], "hit": hit})
                entry_rays += 1

    placement = ridge.placement()
    pods, landings, marker_shifts = {}, {}, {}
    for pod in placement["pods"]:
        label = "Pod_" + pod["id"]
        x, y, before = pod["local"]
        sampled = []
        for radius, count in ((0, 1), (250, 8), (500, 12)):
            for i in range(count):
                a = i * math.tau / count
                sampled.append(floor(label, x + radius * math.cos(a), y + radius * math.sin(a), pod["layer"], 30))
        height = sampled[0][0]
        pod["local"][2] = height
        marker_shifts[label] = height - before
        clearance = 2700 if pod["flyers"] else 600
        for radius, count in ((0, 1), (300, 8)):
            for i in range(count):
                a = i * math.tau / count
                px, py = x + radius * math.cos(a), y + radius * math.sin(a)
                h = floor(label, px, py, pod["layer"], 30)[0]
                clear(label, (px, py, h + 100), (px, py, h + clearance))
        pods[pod["id"]] = {"max_slope_deg": max(slope for _, slope, _ in sampled), "headroom_cm": clearance}

    for anchor in placement["anchors"]:
        label = "Anchor_" + anchor["id"]
        x, y = anchor["landing"]
        layer = anchor["layer"]
        h, slope, _ = floor(label, x, y, layer)
        shift = h - ridge.layer_height(x, y, layer)
        anchor["local"][2] += shift
        marker_shifts[label] = shift
        slopes = [slope]
        for i in range(8):
            a = i * math.tau / 8
            slopes.append(floor(label, x + 400 * math.cos(a), y + 400 * math.sin(a), layer)[1])
        landings[anchor["id"]] = {"max_slope_deg": max(slopes), "landing_h_cm": h}

    nodes = {n["id"]: n for n in ridge.active(ridge.NODES)}
    for node in placement["nodes"]:
        x, y, before = node["local"]
        # 검토점은 발밑이 가파른 자리도 있으므로 투영할 바닥만 확인한다.
        h, _, _ = floor("Review_" + node["id"], x, y, nodes[node["id"]].get("layer", "surface"), 90)
        node["local"][2] = h
        marker_shifts["Review_" + node["id"]] = h - before

    crust = next(obj for obj in meshes if obj.name.startswith("SM_Octant_"))
    bm = bmesh.new()
    bm.from_mesh(crust.data)
    topology = builder.stats(bm, closed=False)
    seam_edges = cut_edges = 0
    for edge in (e for e in bm.edges if e.is_boundary):
        points = [tuple(builder.ue(v.co)) for v in edge.verts]
        if any(all(abs(p[axis]) <= 1 for p in points) for axis in range(3)):
            seam_edges += 1
            continue
        local = [ridge.local(p) for p in points]
        if any(all(abs(ridge.signed_distance(x, y, cut)) < 5 for x, y, _ in local) for cut in base.CUTS):
            cut_edges += 1
        else:
            failures.append({"test": "open_boundary", "local": local})
    bm.free()
    highest = max(ridge.RADIUS - (builder.ue(p).length)
                  for obj in meshes if not obj.name.endswith("_Shell")
                  for p in [*(v.co for v in obj.data.vertices), *(f.center for f in obj.data.polygons)])
    if highest > 7800:
        failures.append({"test": "height_cap", "max_h_cm": highest})

    report = {"version": base.VERSION, "entry_rays": entry_rays, "routes": routes, "pods": pods,
              "landings": landings, "marker_height_shifts_cm": marker_shifts, "crust_topology": topology,
              "seam_edges": seam_edges, "cave_cut_edges": cut_edges, "max_walkable_h_cm": highest,
              "failures": failures}
    (OUT / "geometry-verification.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    assert not failures, (len(failures), failures[:8])
    (OUT / "placement.json").write_text(json.dumps(placement, indent=2) + "\n", encoding="utf-8")
    print("RIDGE_ED_BASE_GEOMETRY_VERIFIED", json.dumps({"entry_rays": entry_rays, "routes": len(routes),
                                                        "pods": len(pods), "landings": len(landings),
                                                        "max_walkable_h_cm": highest}))


if __name__ == "__main__":
    verify()
