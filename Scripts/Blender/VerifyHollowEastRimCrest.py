"""실패한 Nav edge에서 캡슐 하단 구의 바닥 여유를 수치로 비교한다.

50cm Support 샘플과 양방향 고정 Up을 베이커와 같이 재구성한다.
BVH 최근접 거리의 촘촘한 샘플 검사이며 Chaos의 연속 캡슐 sweep을 대신하지 않는다.
"""

import json
import math
from pathlib import Path

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree


def verify(folder, edges_file):
    folder = Path(folder)
    version = folder.name.removeprefix("EastRim")
    obj = bpy.data.objects[f"SM_Hollow_00_EastRim{version}_Floor"]
    tree = BVHTree.FromPolygons([v.co for v in obj.data.vertices],
                               [list(p.vertices) for p in obj.data.polygons], all_triangles=True)
    radial = Vector((1, 1, 1)).normalized()
    bitangent = Vector((-1, -1, 2)).normalized()
    tangent = Vector((-1, 1, 0)).normalized()

    def hit_direction(direction):
        p, normal, _, _ = tree.ray_cast(Vector((0, 0, 0)), direction, 400)
        if p is None:
            raise ValueError("Crest edge has no floor")
        return p, normal

    def hit(xy):
        direction = (radial*30000+bitangent*xy[0]+tangent*xy[1]).normalized()
        return hit_direction(Vector((direction.x, -direction.y, direction.z)))

    records = []
    for edge in json.loads(Path(edges_file).read_text(encoding="utf-8")):
        first, _ = hit(edge["from"])
        last, _ = hit(edge["to"])
        steps = max(2, math.ceil((last-first).length/.5))
        feet = [hit_direction(first.lerp(last, i/steps).normalized()) for i in range(steps+1)]
        minimum = math.inf
        for (a, an), (b, bn) in zip(feet, feet[1:]):
            au, bu = -a.normalized(), -b.normalized()
            ac = a+au*(.88+.5*(1/an.dot(au)-1)+.01)
            bc = b+bu*(.88+.5*(1/bn.dot(bu)-1)+.01)
            for up in (au, bu):
                for i in range(101):
                    sphere = ac.lerp(bc, i/100)-up*.38
                    _, _, _, distance = tree.find_nearest(sphere)
                    minimum = min(minimum, distance*100)
        records.append({**edge, "minimum_bottom_sphere_distance_cm": minimum})
    result = {"version": version, "edges": len(records), "radius_cm": 50,
              "minimum_cm": min(r["minimum_bottom_sphere_distance_cm"] for r in records),
              "penetrating_edges": sum(r["minimum_bottom_sphere_distance_cm"] < 50 for r in records),
              "records": records,
              "scope": "Sampled Blender floor BVH diagnostic; final UE bake and capsule review still required"}
    (folder/f"crest-{Path(edges_file).stem}-verification.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    return {k: v for k, v in result.items() if k != "records"}


if __name__ == "__main__":
    import sys
    args = sys.argv[sys.argv.index("--")+1:]
    print(json.dumps(verify(*args), indent=2))
