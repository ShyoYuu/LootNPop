"""새 뿌리 덩어리의 경로/상부/절삭/지상 진입을 최종 외피에서 검사한다."""
import json
import math
import sys
from pathlib import Path
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Scripts'))
import GroveRootNetworkSpec as spec
sys.path.insert(0,str(ROOT/'Scripts/Blender'))
from BuildGroveRootNetwork import crust_ground_sampler
OUT=ROOT/'Saved/SliceOctant/Grove_00'/spec.VERSION
manifest=json.loads((OUT/'manifest.json').read_text(encoding='utf-8'))
bpy.ops.wm.open_mainfile(filepath=str(ROOT/('Art/Octants/Grove_00/Grove_00_'+manifest.get('geometry_version',spec.VERSION)+'.blend')))
support_component_max=max(abs(v) for e in manifest['exports'] if e['role'] in ('Floor','Crust') for values in e['bounds_cm'].values() for v in values)
assert support_component_max<32700,('Support exceeds replicated position margin',support_component_max)
assert len(manifest['roots'])==8 and not manifest['legacy_root_geometry'] and not manifest['legacy_branch_or_vault_geometry']
assert manifest['source_connected_components']==1
assert {r['tree'] for r in spec.BRANCH_RAMPS}=={t['id'] for t in spec.TREES}
approved=json.loads((OUT.parent/'V027/manifest.json').read_text(encoding='utf-8'))
for ramp in approved['branch_ramps']:
    assert ramp==next(r for r in manifest['branch_ramps'] if r['id']==ramp['id'])
surface=json.loads((OUT.parent/'V029/manifest.json').read_text(encoding='utf-8'))
for key in ('trees','roots','branch_ramps'):assert manifest[key]==surface[key],('Approved surface changed',key)
for old,new in zip(surface['crown_spaces'],manifest['crown_spaces']):
    if old['id']=='T4LowerBranch':
        assert {k:v for k,v in old.items() if k!='local'}=={k:v for k,v in new.items() if k!='local'}
        assert old['local'][:2]==new['local'][:2]
        lower=next(s for s in manifest['crown_spaces'] if s['id']=='T1LowerBranch')
        assert abs(new['local'][2]-lower['local'][2]-80)<.001
    else:assert old==new,('Approved crown changed',old['id'])
cave=json.loads((OUT.parent/'V039/manifest.json').read_text(encoding='utf-8'))
for key in ('underground','cave_cover'):assert manifest[key]==cave[key],('Approved underground changed',key)
approved_lights=json.loads((OUT.parent/'V039/underground-lights.json').read_text(encoding='utf-8'))
assert spec.cave_lights()[:len(approved_lights)]==approved_lights
for stage,count in (('BareTrees',0),('SolidRoots',8)):
    checkpoint=json.loads((OUT.parent/spec.BASELINE_VERSION/stage/'manifest.json').read_text(encoding='utf-8'))
    assert len(checkpoint['roots'])==count and checkpoint['trees']==manifest['trees']
    if count:assert checkpoint['roots']==manifest['roots']
meshes=[o for o in bpy.data.objects if o.type=='MESH' and o.name.startswith('SM_')]
vertices,faces=[],[]
for obj in meshes:
    obj.data.calc_loop_triangles()
    offset=len(vertices)
    vertices.extend(v.co.copy() for v in obj.data.vertices)
    faces.extend(tuple(offset+i for i in tri.vertices) for tri in obj.data.loop_triangles)
tree=BVHTree.FromPolygons(vertices,faces,all_triangles=True)
floor_vertices=[];floor_faces=[]
for obj in meshes:
    if not ('_Floor' in obj.name or obj.name.startswith('SM_Octant')):continue
    offset=len(floor_vertices);floor_vertices.extend(v.co.copy() for v in obj.data.vertices)
    floor_faces.extend(tuple(offset+i for i in tri.vertices) for tri in obj.data.loop_triangles)
floor_tree=BVHTree.FromPolygons(floor_vertices,floor_faces,all_triangles=True)
crust=next(o for o in meshes if o.name.startswith('SM_Octant'))
crust_tree=BVHTree.FromPolygons([v.co for v in crust.data.vertices],[tuple(t.vertices) for t in crust.data.loop_triangles],all_triangles=True)
surface_ground=crust_ground_sampler()

def bl(p):return Vector((p[0]/100,-p[1]/100,p[2]/100))
def ue(p):return Vector((p.x*100,-p.y*100,p.z*100))
def local(p):
    depth=p.dot(Vector((1,1,1)).normalized())
    return [30000*p.dot(Vector((-1,-1,2)).normalized())/depth,30000*p.dot(Vector((-1,1,0)).normalized())/depth,30000-p.length]
def hit(start,end):
    a,b=bl(spec.point(*start)),bl(spec.point(*end))
    delta=b-a
    p,n,index,distance=tree.ray_cast(a,delta.normalized(),delta.length)
    return (ue(p),n,distance*100) if p is not None else None

def ground_expected(x,y):
    # 보존 이음매/전이 띠는 원본 지각 높이를 쓴다. 내부 목표 높이로 판정하지 않는다.
    h=spec.ground(x,y)+10
    if min(spec.point(x,y,0))<5500:
        h=surface_ground(x,y)
        a,b=bl(spec.point(x,y,2200)),bl(spec.point(x,y,-4000))
        result=crust_tree.ray_cast(a,(b-a).normalized(),(b-a).length)
        if result[0] is not None:h=local(ue(result[0]))[2]
    return h

failures=[]
probes=[]
reports=[]
def check_floor(label,x,y,expected):
    start,end=[x,y,expected+100],[x,y,expected-180]
    result=hit(start,end)
    if result is None:
        failures.append({'kind':'MissingFloor','route':label,'local':[x,y,expected]});return None
    p,n,distance=result
    a,b=bl(spec.point(*start)),bl(spec.point(*end));delta=b-a
    floor_result=floor_tree.ray_cast(a,delta.normalized(),delta.length)
    if floor_result[0] is None or abs(floor_result[3]*100-distance)>.1:
        failures.append({'kind':'FloorRole','route':label,'local':[x,y,expected]})
    slope=math.degrees(math.acos(max(-1,min(1,n.dot(bl(-p.normalized()).normalized())))))
    if abs(distance-100)>30 or slope>=44.8:
        failures.append({'kind':'FloorHeightOrSlope','route':label,'local':[x,y,expected],'actual':local(p),'slope':slope})
    probes.append({'start':start,'end':end,'distance':distance,'kind':'Floor'})
    return {'local':local(p),'slope':slope}

def check_clear(label,a,b):
    result=hit(a,b)
    if result is not None:failures.append({'kind':'Clearance','route':label,'start':a,'end':b,'hit':local(result[0])})
    probes.append({'start':a,'end':b,'distance':None,'kind':'PassageClearance'})

def landing_expected(anchor,x,y):
    if anchor.get('landing_ground'):return ground_expected(x,y)
    if 'landing_root' in anchor:
        root=spec.ROOT_BY_ID[anchor['landing_root']]
        a,b,_,length=spec.basis(root)
        t=((x-a[0])*(b[0]-a[0])+(y-a[1])*(b[1]-a[1]))/length**2
        return spec.root_top(root,t)
    return anchor['landing'][2]

joint_checks=[]
for y in range(1400,2101,25):
    previous=None
    for x in range(-15300,-14399,20):
        result=check_floor('SurfaceJoint',x,y,surface_ground(x,y))
        if result:
            p=result['local'];joint_checks.append(result)
            if previous:
                check_clear('SurfaceJoint',[previous[0],previous[1],previous[2]+100],[p[0],p[1],p[2]+100])
            previous=p

for route in spec.ROUTES:
    checks=[]
    for first,last in zip(route['xy'],route['xy'][1:]):
        a,b=Vector(first),Vector(last)
        d=b-a;side=Vector((-d.y,d.x)).normalized()
        count=math.ceil(d.length/180)
        lanes=[[],[],[]]
        for i in range(count+1):
            for lane,offset in enumerate((-200,0,200)):
                xy=a.lerp(b,i/count)+side*offset
                result=check_floor(route['id'],*xy,ground_expected(*xy))
                if result:
                    checks.append(result);lanes[lane].append(result)
        for lane in lanes:
            for first,last in zip(lane,lane[1:]):
                a,b=first['local'],last['local']
                if math.dist(a[:2],b[:2])>300:continue
                check_clear(route['id'],[a[0],a[1],a[2]+110],[b[0],b[1],b[2]+110])
    reports.append({'id':route['id'],'rays':len(checks),'max_slope':max((c['slope'] for c in checks),default=None)})

for root in spec.ROOTS:
    spans=[(.30,.70)]
    center=-400 if root['id']=='R1' else -100 if root['id']=='R7' else root['lean']
    if root['id']=='R6':spans=[(.30,.43),(.68,.73)]
    if root['id']=='R8':spans=[(.22,.38),(.61,.73)]
    checks=[]
    for t0,t1 in spans:
        count=math.ceil((t1-t0)*spec.basis(root)[3]/150)
        for i in range(count+1):
            t=t0+(t1-t0)*i/count
            for lateral in (center-400,center,center+400):
                xy=spec.root_xy(root,t,lateral)
                result=check_floor('Top_'+root['id'],*xy,spec.root_top(root,t))
                if result:checks.append(result)
    reports.append({'id':'Top_'+root['id'],'rays':len(checks),'max_slope':max((c['slope'] for c in checks),default=None)})

root=spec.ROOT_BY_ID['R1']
path=[(.70,1300,0),(.30,1300,.5),(.28,1300,.5),(.28,650,.5),(.30,650,.5),(.70,650,1),(.70,-400,1)]
z_checks=[]
for first,last in zip(path,path[1:]):
    count=max(1,math.ceil(math.hypot((last[0]-first[0])*spec.basis(root)[3],last[1]-first[1])/100))
    for i in range(count+1):
        t,lateral,f=(a+(b-a)*i/count for a,b in zip(first,last))
        for offset in (-150,0,150):
            # 두 종방향 구간은 폭, 반환 참은 길이 방향 여유를 확인한다.
            tt=t+offset/spec.basis(root)[3] if abs(first[1]-last[1])>1 else t
            ll=lateral if abs(first[1]-last[1])>1 else lateral+offset
            xy=spec.root_xy(root,tt,ll)
            result=check_floor('ZCarve',*xy,spec.cut_height(root,tt,ll,f))
            if result:z_checks.append(result)
reports.append({'id':'ZCarve','rays':len(z_checks),'max_slope':max((c['slope'] for c in z_checks),default=None)})

root=spec.ROOT_BY_ID['R3'];half=root['base_width']/2;top=root['top_width']/2
cross_checks=[]
for i in range(71):
    # 발 반경 42cm가 놓이는 중심을 검사한다. 두 바닥이 만나는 정확한 변은 피한다.
    lateral=(half-60)*(1-2*i/70)
    f=min(1,(half-abs(lateral))/(half-top))
    for along in (-200,0,200):
        t=.55+along/spec.basis(root)[3]
        xy=spec.root_xy(root,t,lateral)
        toe=spec.root_xy(root,t,half if lateral>=0 else -half)
        h=(spec.ground(*toe)+10)*(1-f)+spec.root_top(root,t)*f
        result=check_floor('CrossRamp',*xy,h)
        if result:cross_checks.append(result)
reports.append({'id':'CrossRamp','rays':len(cross_checks),'max_slope':max((c['slope'] for c in cross_checks),default=None)})

entry_count=0
for axis in range(3):
    others=[i for i in range(3) if i!=axis]
    for step in range(1,20):
        for depth in (100,1000,2000,3000,4000):
            direction=Vector((0,0,0));direction[axis]=depth
            direction[others[0]]=30000*math.cos(step*math.pi/40);direction[others[1]]=30000*math.sin(step*math.pi/40)
            x,y,_=local(direction.normalized()*30000)
            result=check_floor('Boundary',x,y,ground_expected(x,y))
            entry_count+=1

placement=spec.placement();nodes={n['id']:n['local'] for n in placement['nodes']}
for entrance in spec.UNDERGROUND['entrances']:
    checks=[];count=math.ceil(entrance['length']/150)
    lanes=[[],[],[]]
    for i in range(count+1):
        t=i/count
        a,b=Vector(spec.underground_xy(entrance,max(0,t-.001))),Vector(spec.underground_xy(entrance,min(1,t+.001)))
        d=b-a;side=Vector((-d.y,d.x)).normalized()
        for lane,offset in enumerate((-250,0,250)):
            # 삼각형 경계의 부동소수점 hit 분기를 피하도록 새 지하 표본만 X방향 1cm 이동한다.
            xy=Vector(spec.underground_xy(entrance,t))+side*offset+Vector((1,0))
            expected=spec.underground_floor(entrance,t)
            # 문턱에서는 지각과 굴착 바닥 중 낮은 쪽이 나타난다.
            if entrance['type']=='Ramp' and t<.04:expected=min(expected,spec.ground(*xy)+10)
            result=check_floor('Underground_'+entrance['id'],*xy,expected)
            if result:
                checks.append(result);lanes[lane].append(result)
                h=result['local'][2]
                check_clear('UndergroundHead_'+entrance['id'],[*xy,h+110],[*xy,h+900])
    for lane in lanes:
        for first,last in zip(lane,lane[1:]):
            a,b=first['local'],last['local']
            check_clear('UndergroundBody_'+entrance['id'],[a[0],a[1],a[2]+110],[b[0],b[1],b[2]+110])
    reports.append({'id':'Underground_'+entrance['id'],'rays':len(checks),'max_slope':max((c['slope'] for c in checks),default=None)})

for entrance in spec.UNDERGROUND['entrances']:
    if entrance['type']!='Shaft':continue
    x,y,h=nodes['Underground'+entrance['id']+'Lip']
    check_floor('ShaftLip_'+entrance['id'],x,y,h)
    x,y,h=nodes['Underground'+entrance['id']+'Ledge']
    for i in range(9):
        r=300 if i<8 else 0;a=i*math.tau/8
        check_floor('ShaftLedge_'+entrance['id'],x+r*math.cos(a),y+r*math.sin(a),h)

cave_floor_count=0
for dx in range(-4500,4501,500):
    for dy in range(-4500,4501,500):
        if math.hypot(dx,dy)>4700:continue
        x,y=spec.UNDERGROUND['xy'][0]+dx,spec.UNDERGROUND['xy'][1]+dy
        if any(math.dist((x,y),c['xy'])<c['radius']+300 for c in spec.CAVE_COVER):continue
        result=check_floor('CavernFloor',x,y,spec.UNDERGROUND['floor'])
        if result:
            cave_floor_count+=1;h=result['local'][2]
            check_clear('CavernDroneHeadroom',[x,y,h+110],[x,y,h+3000])

for i in range(81):
    a=i*math.tau/80
    for radius in (3900,4100,4300):
        x,y=spec.UNDERGROUND['xy'][0]+radius*math.cos(a),spec.UNDERGROUND['xy'][1]+radius*math.sin(a)
        check_floor('CavernOrbit',x,y,spec.UNDERGROUND['floor'])

for cover in spec.CAVE_COVER:
    x,y=cover['xy'];reach=cover['radius']+500;h=spec.UNDERGROUND['floor']+160
    a,b=[x-reach,y,h],[x+reach,y,h];result=hit(a,b)
    if result is None:failures.append({'kind':'CoverMissing','id':cover['id']})
    else:probes.append({'start':a,'end':b,'distance':result[2],'kind':'Wall'})

flight_checks=[]
for altitude in (500,1000,1300,2300,3500):
    for axis in (0,1):
        h=spec.UNDERGROUND['floor']+altitude
        a=Vector(spec.point(-12000,1000,h) if axis==0 else spec.point(-9000,-2500,h))
        b=Vector(spec.point(-6000,1000,h) if axis==0 else spec.point(-9000,4500,h))
        d=(b-a).normalized();side=d.cross(a.normalized()).normalized();up=d.cross(side).normalized()
        for offset in (Vector((0,0,0)),side*130,side*-130,up*130,up*-130):
            check_clear('DroneFlightBand',local(a+offset),local(b+offset))
        flight_checks.append({'altitude_cm':altitude,'axis':axis})

for space in spec.CROWN_SPACES:
    x,y,h=space['local'];checks=[]
    # 중심뿐 아니라 플레이 가능한 상면 내부를 5m 간격으로 검사한다.
    for dx in range(-1000,1001,500):
        for dy in range(-1000,1001,500):
            if math.hypot(dx,dy)>space['radius']-400:continue
            result=check_floor(space['id'],x+dx,y+dy,h)
            if result:checks.append(result)
    reports.append({'id':space['id'],'rays':len(checks),'max_slope':max((c['slope'] for c in checks),default=None)})

junction_lines=[]
low=next(s['local'][2] for s in spec.CROWN_SPACES if s['id']=='T1LowerBranch')
high=next(s['local'][2] for s in spec.CROWN_SPACES if s['id']=='T4LowerBranch')
for y in (-4700,-4900,-5100,-5300):
    line=[]
    for x in range(-400,1001,50):
        a,b=[x,y,high+150],[x,y,low-100];result=hit(a,b)
        if result is None:
            failures.append({'kind':'BranchJunctionMissingFloor','local':[x,y,low]});continue
        p,n,d=result;h=local(p)[2]
        expected=low if abs(h-low)<abs(h-high) else high
        check_floor('BranchJunction',x,y,expected)
        line.append(h)
    assert len(line)==29
    step=max(abs(a-b) for a,b in zip(line,line[1:]))
    if not 40<step<110:failures.append({'kind':'BranchJunctionStep','y':y,'step':step})
    check_clear('BranchJunctionJumpHead',[-400,y,low+335],[1000,y,low+335])
    junction_lines.append({'y':y,'max_step_cm':step,'rays':len(line)})

for ramp in spec.BRANCH_RAMPS:
    checks=[];count=math.ceil(math.dist(ramp['start'],ramp['end'])/100);lanes=[[],[],[]]
    for i in range(count+1):
        for lane,offset in enumerate((-200,0,200)):
            x,y,h=spec.branch_ramp_point(ramp,i/count,offset)
            h=max(h,ramp['cap']) if i/count<.3 else h
            result=check_floor('BranchRamp_'+ramp['tree'],x,y,h)
            if result:checks.append(result);lanes[lane].append(result)
    for lane in lanes:
        for a,b in zip(lane,lane[1:]):
            a,b=a['local'],b['local']
            for height in (100,200):check_clear('BranchRampBody_'+ramp['tree'],[a[0],a[1],a[2]+height],[b[0],b[1],b[2]+height])
    reports.append({'id':'BranchRamp_'+ramp['tree'],'rays':len(checks),'max_slope':max((c['slope'] for c in checks),default=None)})

for pod in placement['pods']:
    x,y,h=pod['local']
    for i in range(17):
        r=300 if i<16 else 0;a=i*math.tau/16
        result=check_floor('Pod_'+pod['id'],x+r*math.cos(a),y+r*math.sin(a),h)
        if result and abs(result['local'][2]-h)>15:
            failures.append({'kind':'PodPad','id':pod['id'],'local':result['local'],'expected_h':h})
    check_clear('Head_'+pod['id'],[x,y,h+110],[x,y,h+pod.get('headroom',3000)])
hook_checks=[]
hook_probe_start=len(probes)
for anchor,source in ((a,n) for a in placement['anchors'] for n in [a['from']]+a.get('also_from',[])):
    a=list(nodes[source])
    if source=='EastGround':
        # 이 검토점은 이음매 보간 지각 위다. 플레이어가 서는 실제 바닥 높이를 쓴다.
        floor=hit([a[0],a[1],a[2]+200],[a[0],a[1],a[2]-200])
        assert floor is not None
        a[2]=local(floor[0])[2]
    takeoff=list(a)
    start=[a[0],a[1],a[2]+190];end=anchor['local']
    distance=math.dist(spec.point(*start),spec.point(*end))
    if distance>=anchor['max_distance']:failures.append({'kind':'HookRange','id':anchor['id'],'distance':distance})
    check_clear('Hook_'+anchor['id'],start,end)
    if 'landing' in anchor:
        a=Vector(spec.point(a[0],a[1],a[2]+96));b=Vector(spec.point(*end));direction=(b-a).normalized()
        side=direction.cross(a.normalized()).normalized();other=direction.cross(side).normalized()
        for offset in (Vector((0,0,0)),side*60,side*-60,other*60,other*-60):
            check_clear('HookBody_'+anchor['id'],local(a+offset),local(b+offset))
        stop=b-direction*150
        x,y,_=local(stop);h=landing_expected(anchor,x,y)
        check_clear('HookDrop_'+anchor['id'],[x,y,local(stop)[2]],[x,y,h+220])
        p=Vector(spec.point(x,y,h));up=p.normalized();u=up.cross(Vector((0,0,1))).normalized();v=up.cross(u).normalized()
        for i in range(8):
            q=(p+(u*math.cos(i*math.tau/8)+v*math.sin(i*math.tau/8))*400).normalized()*p.length
            xx,yy,_=local(q)
            check_floor('HookLanding_'+anchor['id'],xx,yy,landing_expected(anchor,xx,yy))
    hook_checks.append({'id':anchor['id'],'from':source,'takeoff_local':takeoff,'distance_cm':distance})

(OUT/'geometry-failures.json').write_text(json.dumps(failures,indent=2),encoding='utf-8')
report={'routes':reports,'boundary_floor_rays':entry_count,'hooks':hook_checks,'failure_count':len(failures),
    'native_scope':'anchors_only_frozen_geometry' if 'geometry_version' in manifest else 'full_geometry',
    'branch_junction_lines':junction_lines,
    'surface_joint_floor_rays':len(joint_checks),'surface_joint_max_slope':max(c['slope'] for c in joint_checks),
    'floor_rays':sum(p['kind']=='Floor' for p in probes),'clearance_rays':sum(p['distance'] is None for p in probes),
    'trees':len(spec.TREES),'roots':len(spec.ROOTS),'checkpoints':manifest['checkpoints'],
    'cavern_floor_rays':cave_floor_count,'flight_bands':flight_checks,'cover_count':len(spec.CAVE_COVER),'support_max_component_cm':support_component_max,'underground_sample_offset_xy_cm':[1,0],
    'limits':['Point rays, not Chaos capsule sweeps, Mover, hook inertia, combat or design approval']}
(OUT/'geometry-verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
assert not failures,('Fresh Grove geometry failures',len(failures),failures[:5])
chunks=[]
native_probes=probes[hook_probe_start:] if 'geometry_version' in manifest else probes
for offset in range(0,len(native_probes),200):
    name='native-probes-'+str(offset//200).zfill(2)+'.json'
    (OUT/name).write_text(json.dumps([[p['start'],p['end'],p['distance'],p['kind']] for p in native_probes[offset:offset+200]],separators=(',',':')),encoding='utf-8');chunks.append(name)
(OUT/'native-probe-index.json').write_text(json.dumps(chunks),encoding='utf-8')
print('GROVE_FRESH_VERIFIED',json.dumps(report))
