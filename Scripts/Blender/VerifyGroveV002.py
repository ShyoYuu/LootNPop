"""V002 실제 생성 형상의 렌더·보행 표본·개방 경계·Pod·훅을 확인한다."""
import json
import math
import sys
from pathlib import Path
import bpy
from mathutils import Matrix,Vector
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"Scripts"))
import GroveOctantSpecV002 as spec
OUT=ROOT/"Saved/SliceOctant/Grove_00"/spec.VERSION
bpy.ops.wm.open_mainfile(filepath=str(ROOT/f"Art/Octants/Grove_00/Grove_00_{spec.VERSION}.blend"))
meshes=[o for o in bpy.context.scene.objects if o.type=="MESH" and o.name.startswith("SM_")]

def bl(p):
    return Vector((p[0]/100,-p[1]/100,p[2]/100))

def ue(p):
    return Vector((p.x*100,-p.y*100,p.z*100))

def local(p):
    r=Vector((1,1,1)).normalized()
    b=Vector((-1,-1,2)).normalized()
    t=Vector((-1,1,0)).normalized()
    return [30000*p.dot(b)/p.dot(r),30000*p.dot(t)/p.dot(r),30000-p.length]

vertices,faces=[],[]
for obj in meshes:
    obj.data.calc_loop_triangles()
    start=len(vertices)
    vertices.extend(v.co.copy() for v in obj.data.vertices)
    faces.extend(tuple(start+i for i in f.vertices) for f in obj.data.loop_triangles)
bvh=BVHTree.FromPolygons(vertices,faces,all_triangles=True)
forest_vertices,forest_faces=[],[]
for obj in meshes:
    if "Forest" not in obj.name:
        continue
    start=len(forest_vertices)
    forest_vertices.extend(v.co.copy() for v in obj.data.vertices)
    forest_faces.extend(tuple(start+i for i in f.vertices) for f in obj.data.loop_triangles)
forest_bvh=BVHTree.FromPolygons(forest_vertices,forest_faces,all_triangles=True)

def inside_forest(local_point):
    p=bl(spec.point(*local_point))
    direction=-p.normalized()
    crossings=0
    for _ in range(30):
        result=forest_bvh.ray_cast(p,direction,400)
        if result[0] is None:
            return crossings%2==1
        crossings+=1
        p=result[0]+direction*.001
    raise AssertionError("Forest containment ray overflow")

def hit(a,b):
    a,b=bl(spec.point(*a)),bl(spec.point(*b))
    delta=b-a
    p,normal,_,distance=bvh.ray_cast(a,delta.normalized(),delta.length)
    return (ue(p),normal,distance*100) if p is not None else None

def floor(x,y,h=None):
    h=spec.ground(x,y) if h is None else h
    result=hit([x,y,h+2200],[x,y,h-600])
    assert result is not None,("Missing floor",x,y,h)
    p,normal,distance=result
    # 구멍 위 인방을 지상 보행 바닥으로 오인하지 않는다. 더 높은 기존 뿌리는 유지한다.
    for wall in spec.CONNECTING_WALLS:
        a,b=Vector(wall["from"]),Vector(wall["to"])
        d=b-a
        q=Vector((x,y))-a
        t=q.dot(d)/d.length_squared
        lateral=abs(q.x*d.y-q.y*d.x)/d.length
        if wall["style"]=="Arch" and lateral<wall["half_width"]+1 and any(left<t<right for left,right in wall["gates"]) and 200<local(p)[2]-h<350:
            below=hit([x,y,h+230],[x,y,h-600])
            # 기존 뿌리가 구멍을 채우는 곳은 뿌리 위를 걷는다. 몸체 밑면을 바닥으로 삼지 않는다.
            if below is not None and below[1].dot(bl(-below[0].normalized()).normalized())>=.71:
                p,normal,distance=below
                return {"local":local(p),"start":[x,y,h+230],"end":[x,y,h-600],"distance":distance,"slope":math.degrees(math.acos(max(-1,min(1,normal.dot(bl(-p.normalized()).normalized())))))}
    slope=math.degrees(math.acos(max(-1,min(1,normal.dot(bl(-p.normalized()).normalized())))))
    return {"local":local(p),"start":[x,y,h+2200],"end":[x,y,h-600],"distance":distance,"slope":slope}

def render():
    scene=bpy.context.scene
    for obj in scene.objects:
        obj.hide_render=obj not in meshes
    for obj in meshes:
        obj.color=(.28,.38,.23,1) if "Octant" in obj.name else (.31,.22,.14,1) if "Forest" in obj.name else (.25,.29,.20,1)
    scene.render.engine="BLENDER_WORKBENCH"
    scene.display.shading.light="STUDIO"
    scene.display.shading.color_type="OBJECT"
    scene.display.shading.show_shadows=True
    scene.display.shading.show_cavity=True
    scene.display.shading.cavity_type="BOTH"
    scene.display.shading.background_type="WORLD"
    scene.world.color=(.14,.17,.20)
    scene.render.resolution_x,scene.render.resolution_y=1600,1100
    scene.render.resolution_percentage=100
    for name,xy in (("PlayerScale",[-12500,-2500]),("PlayerScaleB",[1000,-12000])):
        p=Vector(spec.point(*xy,spec.ground(*xy)+95))
        up=-p.normalized()
        bpy.ops.mesh.primitive_cylinder_add(vertices=8,radius=.24,depth=1.9,location=bl(p))
        obj=bpy.context.object
        obj.name=name
        obj.rotation_euler=bl(up).to_track_quat("Z","Y").to_euler()
        obj.color=(.95,.35,.07,1)
    camera=bpy.data.objects.new("GroveReview",bpy.data.cameras.new("GroveReview"))
    scene.collection.objects.link(camera)
    scene.camera=camera
    camera.data.clip_end=2000
    for name,eye,target,ortho,scale in (
        ("Overview",[0,-8000,24000],[1000,-4000,2000],True,470),
        ("ClearingA",[-12500,-2500,-700],[-5000,-7500,3000],False,0),
        ("ClearingB",[1000,-12000,-700],[0,0,4500],False,0),
        ("ClearingC",[4500,8500,-1100],[-4000,11000,3000],False,0),
        ("ClearingE",[-13500,15500,-600],[-10000,9000,2000],False,0),
        ("BossClearing",[10500,2000,-900],[17500,0,6000],False,0),
        ("Branch",[-5000,-8500,3200],[-2000,-4500,2800],False,0),
        ("RootZ",[-4500,6500,1500],[-500,6000,0],False,0),
        ("RootZSide",[-7700,4300,1700],[-750,5250,100],True,85),
        ("ConnectingWall",[-11000,-2500,-700],[-10000,0,-900],False,0),
        ("RootVault",[-9500,-9200,-4400],[-6700,-7800,-3700],False,0)):
        p=Vector(spec.point(*eye))
        position,aim=bl(p),bl(spec.point(*target))
        forward=(aim-position).normalized()
        up=bl(-p.normalized()).normalized()
        right=forward.cross(up).normalized()
        up=right.cross(forward).normalized()
        camera.location=position
        camera.rotation_euler=Matrix((right,up,-forward)).transposed().to_euler()
        camera.data.type="ORTHO" if ortho else "PERSP"
        camera.data.ortho_scale=scale if ortho else 300
        camera.data.lens=22
        scene.render.filepath=str(OUT/(name+".png"))
        bpy.ops.render.render(write_still=True)

render()
placement=spec.placement()
nodes={n["id"]:n["local"] for n in placement["nodes"]}
probes=[]
route_reports=[]
route_failures=[]
clearance_checks=[]
for route in spec.ROUTES:
    checks=[]
    for a,b in zip(route["xy"],route["xy"][1:]):
        a,b=Vector(a),Vector(b)
        delta=b-a
        side=Vector((-delta.y,delta.x)).normalized()
        count=math.ceil(delta.length/180)
        for i in range(count+1):
            center=a.lerp(b,i/count)
            for offset in (-route["half_width"],0,route["half_width"]):
                check=floor(*(center+offset*side))
                if check["slope"]>=44.8:
                    route_failures.append({"route":route["id"],**check})
                checks.append(check)
    # 바닥 존재와 별도로 낮은 벽/구멍 구간의 몸 높이 통과를 검사한다.
    for offset in range(3):
        lane=checks[offset::3]
        for previous,following in zip(lane,lane[1:]):
            a,b=previous["local"],following["local"]
            nearby=False
            for wall in spec.CONNECTING_WALLS:
                start,end=Vector(wall["from"]),Vector(wall["to"])
                d=end-start
                t=max(0,min(1,(Vector(a[:2])-start).dot(d)/d.length_squared))
                nearby|=(Vector(a[:2])-start.lerp(end,t)).length<500
            if nearby:
                assert hit([a[0],a[1],a[2]+100],[a[0],a[1],a[2]+240]) is None,("Passage headroom",route["id"],a)
                start,end=[a[0],a[1],a[2]+110],[b[0],b[1],b[2]+110]
                assert hit(start,end) is None,("Route body blocked",route["id"],start,end)
                clearance_checks.append({"start":start,"end":end,"distance":None,"kind":"PassageClearance"})
    probes.extend(checks)
    route_reports.append({"id":route["id"],"rays":len(checks),"max_slope":max(v["slope"] for v in checks)})
(OUT/"route-failures.json").write_text(json.dumps(route_failures,indent=2),encoding="utf-8")
assert not route_failures,("Route slope failures",len(route_failures),route_failures[:5])
switchbacks=[]
for wall in spec.ROOT_WALLS:
    if wall.get("style")!="Switchback":
        continue
    a,b=Vector(wall["from"]),Vector(wall["to"])
    side=Vector((-(b-a).y,(b-a).x)).normalized()
    checks=[]
    for sign in (-1,1):
        path=[(.96,1300,10),(.53,1300,wall["height"]*.5),(.5,1300,wall["height"]*.5),
              (.5,600,wall["height"]*.5),(.53,600,wall["height"]*.5),(.96,600,wall["height"]),
              (.99,600,wall["height"]),(.99,0,wall["height"])]
        for first,last in zip(path,path[1:]):
            count=max(1,math.ceil(math.hypot((last[0]-first[0])*(b-a).length,last[1]-first[1])/120))
            for i in range(count+1):
                t,lateral,h=(x+(y-x)*i/count for x,y in zip(first,last))
                xy=a.lerp(b,t)+sign*lateral*side
                expected=spec.ground(*xy)+h
                result=hit([*xy,expected+80],[*xy,expected-120])
                assert result is not None,("Z ramp missing floor",wall["id"],t,lateral)
                p,normal,distance=result
                assert abs(distance-80)<20,("Z ramp height",wall["id"],t,lateral,distance)
                slope=math.degrees(math.acos(max(-1,min(1,normal.dot(bl(-p.normalized()).normalized())))))
                assert slope<44.8,("Z ramp slope",wall["id"],slope,t,lateral)
                assert hit([*xy,expected+130],[*xy,expected+430]) is None,("Z ramp headroom",wall["id"],t,lateral)
                checks.append({"start":[*xy,expected+80],"end":[*xy,expected-120],"distance":distance,"slope":slope})
    probes.extend(checks)
    switchbacks.append({"id":wall["id"],"rays":len(checks),"max_slope":max(c["slope"] for c in checks)})
entries=[]
for axis in range(3):
    others=[i for i in range(3) if i!=axis]
    for step in range(1,20):
        a=step*math.pi/40
        # 단일 옥탄트 BVH의 정확한 열린 이음매 선은 반올림에 민감하므로 1m 안쪽부터 검사한다.
        # 이음매 선 자체의 일치는 원본 정점 복원과 베이크 seam 대조가 담당한다.
        for depth in (100,1000,2000,3000,4000):
            direction=Vector((0,0,0))
            direction[axis]=depth
            direction[others[0]]=30000*math.cos(a)
            direction[others[1]]=30000*math.sin(a)
            x,y,_=local(direction.normalized()*30000)
            check=floor(x,y)
            assert check["slope"]<35,("Boundary entry",axis,step,depth,check)
            entries.append(check)
probes.extend(entries)
pod_checks=[]
for pod in placement["pods"]:
    x,y,h=pod["local"]
    for i in range(17):
        r=0 if i==16 else 300
        a=i*math.tau/16
        result=hit([x+r*math.cos(a),y+r*math.sin(a),h+70],[x+r*math.cos(a),y+r*math.sin(a),h-150])
        assert result is not None,("Pod floor",pod["id"],i)
        p,normal,distance=result
        assert abs(distance-70)<12,("Pod height",pod["id"],distance)
        check={"start":[x+r*math.cos(a),y+r*math.sin(a),h+70],"end":[x+r*math.cos(a),y+r*math.sin(a),h-150],"distance":distance}
        probes.append(check)
        pod_checks.append(check)
    assert hit([x,y,h+150],[x,y,h+3150]) is None,("Pod headroom",pod["id"])
hook_checks=[]
for anchor in placement["anchors"]:
    a=nodes[anchor["from"]]
    start=[a[0],a[1],a[2]+190] if anchor["from"]!="VaultInner" else a
    end=anchor["local"]
    distance=(Vector(spec.point(*start))-Vector(spec.point(*end))).length
    assert distance<anchor["max_distance"],("Hook range",anchor["id"],distance)
    assert hit(start,end) is None,("Hook visibility",anchor["id"],start,end)
    hook_checks.append({"id":anchor["id"],"distance_cm":distance,"start":start,"end":end})
field_heights=[spec.ground(x,y) for x in range(-16000,25001,1000) for y in range(-20000,20001,1000) if min(spec.point(x,y,0))>9000]
floor_count=len(probes)
wall_checks=[]
embedded_wall_samples=0
for wall in spec.CONNECTING_WALLS:
    a,b=Vector(wall["from"]),Vector(wall["to"])
    side=Vector((-(b-a).y,(b-a).x)).normalized()
    for t in (.2,.35,.5,.65,.8):
        if any(left-.02<t<right+.02 for left,right in wall["gates"]):
            continue
        xy=a.lerp(b,t)
        first,last=xy-350*side,xy+350*side
        h=spec.ground(*xy)+180
        start,end=[*first,h],[*last,h]
        if inside_forest(start) or inside_forest(end):
            embedded_wall_samples+=1
            continue
        result=hit(start,end)
        assert result is not None,("Connecting wall gap",wall["id"],t)
        wall_checks.append({"start":start,"end":end,"distance":result[2],"kind":"Wall"})
probes.extend(wall_checks+clearance_checks)
report={"routes":route_reports,"open_boundary_entry_rays":len(entries),"pod_floor_rays":len(pod_checks),
    "switchbacks":switchbacks,
    "trees":len(spec.TREES),"root_walls":len(spec.ROOT_WALLS),"ground_field_height_range_cm":[min(field_heights),max(field_heights)],
    "connecting_walls":len(spec.CONNECTING_WALLS),"wall_blocker_rays":len(wall_checks),"embedded_wall_samples":embedded_wall_samples,"passage_clearance_rays":len(clearance_checks),
    "hooks":hook_checks,"floor_rays":floor_count,"limits":["Blender BVH rays, not Chaos capsules or Mover; hook inertia and combat need PIE"]}
(OUT/"placement.json").write_text(json.dumps(placement,indent=2),encoding="utf-8")
(OUT/"geometry-verification.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
chunks=[]
for offset in range(0,len(probes),200):
    name=f"native-probes-{offset//200:02d}.json"
    (OUT/name).write_text(json.dumps([[p["start"],p["end"],p["distance"],p.get("kind","Floor")] for p in probes[offset:offset+200]],separators=(",",":")),encoding="utf-8")
    chunks.append(name)
(OUT/"native-probe-index.json").write_text(json.dumps(chunks),encoding="utf-8")
print("GROVE_GEOMETRY_VERIFIED",json.dumps(report))
