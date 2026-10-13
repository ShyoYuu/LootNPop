"""저장 원본의 이음매·Pod 바닥·훅 시야를 검사하고 배치 원본과 검토 이미지를 만든다.

BVH 광선은 엔진 캡슐 검사와 실제 조작을 대신하지 않는다.
"""

import json
import math
from pathlib import Path

import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree


ROOT = Path(__file__).resolve().parents[2]
VERSION = "V015"
OUT = ROOT / "Saved/SliceOctant/Acropolis_00" / VERSION
R = 30000
RADIAL = Vector((1,1,1)).normalized()
TANGENT = Vector((-1,1,0)).normalized()
BITANGENT = Vector((-1,-1,2)).normalized()


def point(x,y,h):
    return (R*RADIAL+x*BITANGENT+y*TANGENT).normalized()*(R-h)


def bl(p):
    return Vector((p.x/100,-p.y/100,p.z/100))


def ue(p):
    return Vector((p.x*100,-p.y*100,p.z*100))


def tree(objects):
    vertices, faces = [], []
    for obj in objects:
        offset = len(vertices)
        vertices.extend(v.co.copy() for v in obj.data.vertices)
        faces.extend(tuple(offset+i for i in f.vertices) for f in obj.data.polygons)
    return BVHTree.FromPolygons(vertices,faces)


def clear(bvh,start,end):
    start,end = bl(start),bl(end)
    delta=end-start
    hit=bvh.ray_cast(start,delta.normalized(),delta.length-0.02)
    return hit[0] is None


def build():
    bpy.ops.wm.open_mainfile(filepath=str(ROOT / f"Art/Octants/Acropolis_00/Acropolis_00_{VERSION}.blend"))
    manifest=json.loads((OUT / "manifest.json").read_text(encoding="utf-8"))
    meshes=[o for o in bpy.context.scene.objects if o.type=="MESH"]
    bvh=tree(meshes)
    crust=next(o for o in meshes if o.name.startswith("SM_Octant"))
    crust_tree=tree([crust])
    kd=KDTree(len(crust.data.vertices))
    for i,v in enumerate(crust.data.vertices):
        kd.insert(v.co,i)
    kd.balance()
    source=json.loads((OUT / "seam-source.json").read_text())
    seam_error=max(kd.find(bl(Vector(p)))[2]*100 for p in source)
    assert seam_error<0.02, seam_error
    nodes=[]
    anchors=[]
    pods=[]
    for island in manifest["islands"]:
        x,y,h=(island[k] for k in ("x","y","h"))
        extra=2500 if island["id"] in ("Outer_01","Outer_05","Outer_09") else 1800
        anchors.append({"id":island["id"],"local":[x,y,h+extra],"max_distance":10500})
        nodes.append({"id":island["id"],"local":[x,y,h],"radius":island["radius_xy"]})
        pods.append({"id":island["id"],"local":[x,y,h]})
    previous=json.loads((ROOT / "Art/Octants/Acropolis_00/placement.json").read_text(encoding="utf-8"))
    for i,a in enumerate((0,120,240)):
        a=math.radians(a)
        x,y=20500*math.cos(a),20500*math.sin(a)
        direction=bl(point(x,y,0)).normalized()
        hit=crust_tree.ray_cast(direction*100,direction,400)
        assert hit[0] is not None
        h=R-ue(hit[0]).length
        nodes.append({"id":f"Entry_{i+1}","local":[x,y,h],"radius":0})
        x,y=7400*math.cos(a),7400*math.sin(a)
        anchors.append({"id":f"Recovery_{i+1}","local":[x,y,-1700],"max_distance":10500})
    anchors.append({"id":"BossReturn","local":[0,0,400],"max_distance":14000})
    recovery_path=ROOT / "Art/Octants/Acropolis_00/pit-recovery.json"
    if recovery_path.exists():
        anchors.append(json.loads(recovery_path.read_text(encoding="utf-8"))["anchor"])
    assert anchors==previous["anchors"], "Approved grapple placement changed"
    nodes.append({"id":"Boss","local":[0,0,-1800],"radius":0})
    for pocket in manifest["pockets"]:
        pods.append({"id":pocket["id"],"local":pocket["pod"]})
    for i,deg in enumerate((54,126,198,270)):
        a=math.radians(deg)
        pods.append({"id":f"Ground_{i+1}","local":[7200*math.cos(a),7200*math.sin(a),-3500]})
    edges=[]
    for src in nodes:
        for dst in anchors[:24]:
            if src["id"]==dst["id"]:
                continue
            x,y,h=src["local"]
            dx,dy=dst["local"][0]-x,dst["local"][1]-y
            d=math.hypot(dx,dy)
            if src["radius"]:
                x+=dx/d*src["radius"]*0.65
                y+=dy/d*src["radius"]*0.65
            island=next((s for s in manifest["islands"] if s["id"]==src["id"]),None)
            if island and island["shape"]=="Terrace":
                a=math.atan2(island["y"],island["x"])+math.pi/2
                u=((x-island["x"])*math.cos(a)+(y-island["y"])*math.sin(a))/island["radius_xy"]
                h+=700*min(1,max(0,(u-0.25)/0.8))
            start=point(x,y,h+160)
            end=point(*dst["local"])
            distance=(end-start).length
            if distance<=dst["max_distance"] and clear(bvh,start,end):
                edges.append({"from":src["id"],"to":dst["id"],"distance_cm":distance,
                              "start_local":[x,y,h+160]})
    reached={n["id"] for n in nodes if n["id"].startswith("Entry_")}
    while True:
        extended=reached|{e["to"] for e in edges if e["from"] in reached}
        if extended==reached:
            break
        reached=extended
    missing=[s["id"] for s in manifest["islands"] if s["id"] not in reached]
    assert not missing, ("Unreachable islands",missing)
    pod_probes=[]
    for pod in pods:
        x,y,h=pod["local"]
        up=-point(x,y,h).normalized()
        for radius in (0,250):
            for i in range(8 if radius else 1):
                a=i*math.tau/8
                p=point(x+radius*math.cos(a),y+radius*math.sin(a),h)
                hit=bvh.ray_cast(bl(p+up*50),bl(-up).normalized(),1.2)
                assert hit[0] is not None,("Missing pod floor",pod["id"])
                slope=math.degrees(math.acos(max(-1,min(1,hit[1].dot(bl(up).normalized())))))
                assert slope<44.8,(pod["id"],slope)
                pod_probes.append({"id":pod["id"],"slope":slope})
    intersections=[]
    island_trees=[(s["id"],tree([o for o in meshes if "_"+s["id"]+"_" in o.name])) for s in manifest["islands"]]
    for i,(name,a) in enumerate(island_trees):
        for other,b in island_trees[i+1:]:
            if a.overlap(b):
                intersections.append([name,other])
    assert not intersections,("Intersecting islands",intersections)
    pocket_probes=[]
    pocket_floor_checks=[]
    for pocket in manifest["pockets"]:
        sections=pocket["sections"]
        pocket_tree=tree([o for o in meshes if "_"+pocket["id"]+"_Floor" in o.name])
        for first,last in zip(sections,sections[1:]):
            for t in (0.02,0.25,0.5,0.75,0.98):
                center=Vector(first["center"]).lerp(Vector(last["center"]),t)
                offset=Vector(first["offset"]).lerp(Vector(last["offset"]),t)
                h=first["h"]+(last["h"]-first["h"])*t
                for v in (-1200,0,1200):
                    xy=center+v*offset
                    p=point(*xy,h)
                    hit=pocket_tree.find_nearest(bl(p))
                    assert hit[0] is not None and hit[3]*100<500,(pocket["id"],xy,hit)
                    p=ue(hit[0])
                    up=-p.normalized()
                    slope=math.degrees(math.acos(max(-1,min(1,hit[1].dot(bl(up).normalized())))))
                    assert slope<44.8,(pocket["id"],xy,slope)
                    pocket_probes.append(slope)
                    d=p.dot(RADIAL)
                    local=[R*p.dot(BITANGENT)/d,R*p.dot(TANGENT)/d,R-p.length]
                    pocket_floor_checks.append({"id":pocket["id"],"local":local})
        p=point(*pocket["pod"])
        hit=bvh.ray_cast(bl(p-p.normalized()*200),bl(-p.normalized()).normalized(),40)
        assert hit[0] is not None
        headroom=(ue(hit[0])-p).length
        assert headroom>=3000,(pocket["id"],headroom)
    report={"seam_samples":len(source),"max_seam_vertex_error_cm":seam_error,
            "hook_visibility_edges":edges,"reachable_islands":24,"pod_floor_probes":len(pod_probes),
            "max_pod_floor_slope_deg":max(p["slope"] for p in pod_probes),
            "preserved_grapple_anchors":len(anchors),"island_intersections":intersections,
            "pocket_floor_probes":len(pocket_probes),"max_pocket_slope_deg":max(pocket_probes),
            "pocket_floor_checks":pocket_floor_checks,
            "limits":["Ray visibility only; no swept capsule, inertia landing, bake or PIE"]}
    (OUT / "geometry-verification.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    (OUT / "placement.json").write_text(json.dumps({"anchors":anchors,"pods":pods,"nodes":nodes},indent=2),encoding="utf-8")
    # 작업용 뷰는 외피 색상으로 역할을 구별하며 게임용 조명을 추가하지 않는다.
    for obj in meshes:
        color=(0.19,0.24,0.22,1) if obj==crust else (0.48,0.43,0.32,1)
        if "AirCover" in obj.name:
            color=(0.13,0.19,0.24,1)
        obj.color=color
    scene=bpy.context.scene
    scene.render.engine="BLENDER_WORKBENCH"
    scene.display.shading.light="STUDIO"
    scene.display.shading.color_type="OBJECT"
    scene.display.shading.show_shadows=True
    scene.display.shading.show_cavity=True
    scene.display.shading.cavity_type="BOTH"
    scene.display.shading.background_type="WORLD"
    scene.world=bpy.data.worlds.new("ReviewWorld")
    scene.world.color=(0.055,0.065,0.08)
    scene.render.resolution_x=1800
    scene.render.resolution_y=1400
    scene.render.resolution_percentage=100
    camera_data=bpy.data.cameras.new("ReviewCamera")
    camera=bpy.data.objects.new("ReviewCamera",camera_data)
    scene.collection.objects.link(camera)
    scene.camera=camera
    for label,eye,target,scale in (
        ("Overview",Vector((0,0,0)),point(0,0,0),560),
        ("AirRoute",point(-21000,-18000,16000),point(0,0,1000),370)):
        camera.location=bl(eye)
        camera.rotation_euler=(bl(target)-camera.location).to_track_quat("-Z","Y").to_euler()
        camera_data.type="ORTHO"
        camera_data.ortho_scale=scale
        camera_data.clip_end=2000
        scene.render.filepath=str(OUT / f"{label}.png")
        bpy.ops.render.render(write_still=True)
    pocket=manifest["pockets"][1]
    eye=point(*Vector(pocket["sections"][-2]["center"]).lerp(Vector(pocket["sections"][-1]["center"]),0.85),-6800)
    target=point(*pocket["sections"][2]["center"],-5100)
    camera.location=bl(eye)
    forward=(bl(target)-camera.location).normalized()
    up=bl(-eye.normalized()).normalized()
    right=forward.cross(up).normalized()
    up=right.cross(forward).normalized()
    camera.rotation_euler=Matrix((right,up,-forward)).transposed().to_euler()
    camera_data.type="PERSP"
    camera_data.lens=22
    scene.render.filepath=str(OUT / "PocketInterior.png")
    bpy.ops.render.render(write_still=True)
    print("ACROPOLIS_GEOMETRY_VERIFIED",json.dumps({k:v for k,v in report.items() if k not in ("hook_visibility_edges","pocket_floor_checks")}))


if __name__=="__main__":
    build()
