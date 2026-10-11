"""V006 지각 절단 부피와 최종 Blender 벽의 접합을 독립 비교한다."""

import json
import math
from pathlib import Path

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree


def verify(repo, side_inset_cm=15):
    repo = Path(repo)
    namespace = {}
    path = repo / "Scripts/Blender/BuildHollowEastRim.py"
    exec(compile(path.read_text(encoding="utf-8"), str(path), "exec"), namespace)
    objects = [o for o in bpy.context.scene.objects if o.type == "MESH" and o.name in
               ("SM_Hollow_00_EastRimV006_Floor", "SM_Hollow_00_EastRimV006_Shell")]
    if len(objects) != 2:
        raise ValueError("V006 export meshes required")
    vertices, triangles = [], []
    for obj in objects:
        offset = len(vertices)
        vertices.extend(obj.matrix_world @ v.co for v in obj.data.vertices)
        triangles.extend(tuple(offset+i for i in p.vertices) for p in obj.data.polygons)
    tree = BVHTree.FromPolygons(vertices, triangles, all_triangles=True)
    radial = Vector((1, -1, 1)).normalized()
    tangent = Vector((-1, -1, 0)).normalized()
    bitangent = Vector((-1, 1, 2)).normalized()

    def point(x, y, h):
        return (radial*30000+bitangent*x+tangent*y).normalized() * ((30000-h)/100)

    faces = ((0,2,3),(0,3,1),(4,6,7),(4,7,5),(0,2,6),(0,6,4),
             (1,3,7),(1,7,5),(0,1,5),(0,5,4),(2,3,7),(2,7,6))
    checks = []
    for name, (a, b, width) in namespace["BRIDGES"].items():
        a, b = Vector(a), Vector(b)
        side = Vector((-(b.y-a.y), b.x-a.x)).normalized()
        steps = max(2, math.ceil(Vector((b.x-a.x,b.y-a.y)).length/(50 if name == "OuterEastBridge" else 200)))
        for step in range(steps):
            corners = []
            for top in (False, True):
                for t in (step/steps, (step+1)/steps):
                    p = a.lerp(b,t)
                    if name == "OuterEastBridge":
                        p.z = namespace["east_ramp_height"](p.x)
                    half = width*(.5 if top else .9)*30000/(30000-p.z)
                    for sign in (-1,1):
                        edge = Vector((p.x,p.y))+side*sign*half
                        corners.append(point(edge.x,edge.y,p.z if top else -2000))
            mid = sum(corners,Vector())/8
            planes = []
            for face_index,(i,j,k) in enumerate(faces):
                normal = (corners[j]-corners[i]).cross(corners[k]-corners[i]).normalized()
                if normal.dot(mid-corners[i]) > 0:
                    normal = -normal
                planes.append((normal,normal.dot(corners[i])-(side_inset_cm/100 if 4 <= face_index < 8 else 0)))
            p = a.lerp(b,(step+.5)/steps)
            for h in range(-1000,1001,100):
                for sign in (-1,1):
                    def candidate(offset):
                        q = Vector((p.x,p.y))+side*sign*offset
                        return q,point(q.x,q.y,h)
                    def inside(offset):
                        _,q = candidate(offset)
                        return max(n.dot(q)-d for n,d in planes) <= 0
                    if not inside(0):
                        continue
                    lo,hi = 0,width*1.2
                    if inside(hi):
                        raise ValueError("Missing side boundary")
                    for _ in range(35):
                        m = (lo+hi)/2
                        if inside(m): lo = m
                        else: hi = m
                    local,q = candidate((lo+hi)/2)
                    if not (-13950 < local.x < -7050 and 7850 < local.y < 11950):
                        continue
                    nearest,normal,index,distance = tree.find_nearest(q)
                    # 오목한 접합 모서리에서는 최근접 한 면의 법선 부호가 부피 안/밖을 나타내지 않는다.
                    votes = []
                    for direction in (radial, -radial, Vector((.312,.517,.797)).normalized()):
                        hit,hit_normal,hit_index,hit_distance = tree.ray_cast(q,direction)
                        if hit_normal is not None:
                            votes.append(hit_normal.dot(direction) < 0)
                    outside = sum(votes) > len(votes)/2
                    checks.append({"bridge":name,"local":[local.x,local.y,h],
                                   "signedDistanceCm":100*distance*(1 if outside else -1),
                                   "distanceCm":100*distance,"outsideVotes":sum(votes),"rays":len(votes)})
    worst = sorted(checks,key=lambda v:v["signedDistanceCm"],reverse=True)[:10]
    result = {"sideInsetCm":side_inset_cm,"samples":len(checks),
              "outsideOverOneCm":sum(c["signedDistanceCm"]>1 for c in checks),
              "disputedRays":sum(0<c["outsideVotes"]<c["rays"] for c in checks),
              "maximumOutsideCm":max(c["signedDistanceCm"] for c in checks),"worst":worst,
              "limits":"Sampled volume boundary versus final mesh; actual Crust assembly and Chaos collision require UE review"}
    folder = repo/"Saved/SliceOctant/Hollow_00/EastRimV006"
    (folder/f"ground-volume-inset{side_inset_cm}-verification.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
    return result
