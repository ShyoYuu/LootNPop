"""경계 복귀로 아홉 곳의 폭·경사·접속부를 검사하고 엔진 검사 표본을 기록한다."""

import json
import math
from pathlib import Path

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree


ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Saved/SliceOctant/Acropolis_00/V015'
R=30000
RADIAL=Vector((1,1,1)).normalized()
BITANGENT=Vector((-1,-1,2)).normalized()
TANGENT=Vector((-1,1,0)).normalized()


def bl(p):
    return Vector((p.x/100,-p.y/100,p.z/100))


def ue(p):
    return Vector((p.x*100,-p.y*100,p.z*100))


def local(p):
    d=p.dot(RADIAL)
    return [R*p.dot(BITANGENT)/d,R*p.dot(TANGENT)/d,R-p.length]


def crust():
    return next(o for o in bpy.context.scene.objects if o.name.startswith('SM_Octant'))


previous_path=ROOT/'Saved/SliceOctant/Acropolis_00/V011/Acropolis_00_V011.blend'
if not previous_path.exists():
    previous_path=ROOT/'Art/Octants/Acropolis_00/Acropolis_00_V011.blend'
bpy.ops.wm.open_mainfile(filepath=str(previous_path))
previous=[]
for v in crust().data.vertices:
    p=ue(v.co)
    if min(p)>1500:
        previous.append(v.co.copy())
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'Art/Octants/Acropolis_00/Acropolis_00_V015.blend'))
obj=crust()
tree=BVHTree.FromPolygons([v.co for v in obj.data.vertices],[tuple(f.vertices) for f in obj.data.polygons])
manifest=json.loads((OUT/'manifest.json').read_text(encoding='utf-8'))
samples,reviews,summary,failures=[],[],[],[]
for route in manifest['exit_routes']:
    axis=route['axis'];others=[i for i in range(3) if i!=axis]
    slopes=[];centerline=[]
    end=16000 if axis==1 and route['angle_deg']==73 else 17350 if axis==2 and route['angle_deg']==78 else 11500
    for distance in range(100,end+1,150):
        for lateral in (-800,0,800):
            angle=math.radians(route['angle_deg'])+lateral/R
            p=Vector((0,0,0));p[axis]=distance
            other=math.sqrt(R*R-distance*distance)
            p[others[0]]=other*math.sin(angle);p[others[1]]=other*math.cos(angle)
            direction=bl(p).normalized()
            hit=tree.ray_cast(direction*290,direction,60)
            assert hit[0] is not None,(route['id'],distance,lateral)
            actual=ue(hit[0]);up=-actual.normalized()
            slope=math.degrees(math.acos(max(-1,min(1,hit[1].dot(bl(up).normalized())))))
            if slope>=42:
                failures.append({'id':route['id'],'station':distance,'lateral':lateral,'slope':slope})
            slopes.append(slope)
            sample={'id':route['id'],'lateral_cm':lateral,'station_cm':distance,'local':local(actual)}
            samples.append(sample)
            if lateral==0:
                centerline.append(sample)
    # 마지막은 내부 평지, 첫 표본은 보존된 옥탄트 경계 쪽이다.
    top,bottom=centerline[0]['local'],centerline[-1]['local']
    reviews.append({'id':'Acropolis'+route['id'],'local':[*bottom[:2],bottom[2]+200]})
    length=sum((Vector(a['local'][:2])-Vector(b['local'][:2])).length for a,b in zip(centerline,centerline[1:]))
    summary.append({**route,'top_local':top,'bottom_local':bottom,'max_slope_deg':max(slopes),
                    'sample_count':len(slopes),'local_parameter_length_cm':length})
# 동굴 외곽과 기존 지붕 접합은 같은 위치·높이로 남아 있어야 한다.
kd=KDTree(len(obj.data.vertices))
for i,v in enumerate(obj.data.vertices):
    kd.insert(v.co,i)
kd.balance()
protected=[]
for p in previous:
    x,y,h=local(ue(p))
    for pocket in manifest['pockets']:
        for a,b in zip(pocket['outline_xy'],pocket['outline_xy'][1:]+pocket['outline_xy'][:1]):
            aa,bb=Vector(a),Vector(b);d=bb-aa
            t=max(0,min(1,(Vector((x,y))-aa).dot(d)/d.length_squared))
            if (Vector((x,y))-aa-t*d).length<100:
                protected.append(kd.find(p)[2]*100)
                break
        else:
            continue
        break
assert max(protected)<0.02,('Cave perimeter changed',max(protected))
report={'routes':summary,'floor_samples':samples,'reviews':reviews,
        'walkable_core_width_cm':1600,'max_slope_deg':max(r['max_slope_deg'] for r in summary),
        'protected_cave_vertices':len(protected),'max_cave_vertex_error_cm':max(protected),
        'limits':['Floor rays and slopes; not Mover capsule traversal']}
(OUT/'exit-verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('ACROPOLIS_EXITS_VERIFIED',json.dumps({k:v for k,v in report.items() if k not in ('floor_samples','reviews')}))
assert not failures,sorted(failures,key=lambda f:-f['slope'])[:10]
