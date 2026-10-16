"""파손부의 층간 왕복·두 층 하강과 모든 복층 홀의 상층 접근을 함께 설계한다."""
import json,math
from pathlib import Path
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[2]
VERSION='V058'
OUT=ROOT/'Saved/SliceOctant/Pyramid_00'/VERSION
ART=ROOT/'Art/Octants/Pyramid_00'
ns={'__file__':str(ROOT/'Scripts/Blender/PyramidOctantCommon.py'),'__name__':'pyramid_helpers'}
exec(compile(Path(ns['__file__']).read_text(encoding='utf-8'),ns['__file__'],'exec'),ns)
scene=bpy.data.scenes['Pyramid_00_'+VERSION]
body=scene.objects['ProjectedEnvelope']; body.data.calc_loop_triangles()
tree=BVHTree.FromPolygons([v.co for v in body.data.vertices],
                         [list(t.vertices) for t in body.data.loop_triangles],all_triangles=True)
placement=json.loads((OUT/'placement.json').read_text())
manifest=json.loads((OUT/'manifest.json').read_text())
baseline=json.loads((ART/'Placement.json').read_text())
old_anchors={a['id']:a for a in baseline['anchors']}
old_coverage=[dict(r,rupture_id=r['route_group']) for r in json.loads((ART/'Pyramid_00_V058_RouteProof.json').read_text())['coverage'] if r['region_kind']=='rupture']


def height(storey):
    return -5600+1400*storey


def point(local):
    return ns['to_blender'](ns['point'](*local))


def hit(a,b):
    a,b=point(a),point(b); delta=b-a
    if delta.length<0.00001:
        return None,None,None,None
    return tree.ray_cast(a,delta.normalized(),delta.length)


def floor_safe(local,clearance=250):
    x,y,h=local; scale=math.sqrt(30000**2+x*x+y*y)/(30000-h)
    for radius in (0,clearance*scale):
        for i in range(8 if radius else 1):
            angle=i*math.tau/8
            xx,yy=x+radius*math.cos(angle),y+radius*math.sin(angle)
            p,n,_,d=hit([xx,yy,h+60],[xx,yy,h-150])
            if p is None or abs(d*100-60)>9 or n.dot(-p.normalized())<0.71:
                return False
    return hit([x,y,h+15],[x,y,h+300])[0] is None


def segments(obj,h):
    result=[]; obj.data.calc_loop_triangles()
    for face in obj.data.loop_triangles:
        vs=[obj.data.vertices[i].co for i in face.vertices]; points=[]
        for a,b in zip(vs,vs[1:]+vs[:1]):
            if (a.z-h)*(b.z-h)<0:
                points.append(a.lerp(b,(h-a.z)/(b.z-a.z)))
        if len(points)==2 and (points[0]-points[1]).xy.length>5:
            result.append(points)
    return result


def positions(region,storey,root=False):
    h=height(storey); obj=region['object']
    plane=max(h+0.1,min(v.co.z for v in obj.data.vertices)+30)
    result=[]; seen=set()
    def add(xy,cost):
        key=(round(xy[0]/20),round(xy[1]/20))
        if key in seen:
            return
        seen.add(key); local=[*xy,h]
        if region['kind']=='facade' and not root:
            if max(-2*xy[0],xy[0]+math.sqrt(3)*xy[1],xy[0]-math.sqrt(3)*xy[1])<ns['radius'](h)+100:
                return
        if floor_safe(local):
            result.append({'landing':local,'cost':cost})
    seed=old_anchors.get(region['id']+'_Storey'+str(storey))
    if seed:
        add(seed['landing'][:2],0)
    if root:
        next_anchor=old_anchors.get(region['id']+'_Storey'+str(storey+1))
        if next_anchor:
            add(next_anchor['approach'][:2],0)
    for ledge in manifest.get('grapple_landings',[]):
        if ledge['route_group']==region['id'] and ledge['storey']==storey:
            center=Vector(ledge['landing'][:2]); direction=Vector(ledge['direction'])
            side=Vector((-direction.y,direction.x))
            scale=math.sqrt(30000**2+center.length_squared)/(30000-h)
            for forward in (-200,0,200):
                for lateral in (-200,0,200):
                    add(center+(direction*forward+side*lateral)*scale,-1)
    if root and region['kind']=='hall':
        center=Vector(region['center'])
        add(center,0)
        for vertex in region['outline']:
            for t in (0.2,0.4,0.6,0.8,0.95):
                add(center.lerp(Vector(vertex),t),0.1)
    for a,b in segments(obj,plane):
        tangent=(b-a).xy.normalized(); normal=Vector((-tangent.y,tangent.x))
        for t in (0.2,0.5,0.8):
            c=a.lerp(b,t).xy; scale=math.sqrt(30000**2+c.length_squared)/(30000-h)
            for distance in (300,500,800,1200,1800,2600):
                for sign in (-1,1):
                    add(c+normal*(sign*distance*scale),distance/1000)
    return result


def bank(region,storey,root=False):
    plane=max(height(storey)+0.1,min(v.co.z for v in region['object'].data.vertices)+30)
    cross_section=segments(region['object'],plane)
    total=sum((b-a).length for a,b in cross_section)
    center=sum(((a+b).xy/2*(b-a).length for a,b in cross_section),Vector((0,0)))/total if total else Vector(region['center'])
    positions_by_sector={}
    for p in positions(region,storey,root):
        x,y,_=p['landing']; cx,cy=center
        sector=int((math.atan2(y-cy,x-cx)+math.pi)/math.tau*12)%12
        positions_by_sector.setdefault(sector,[]).append(p)
    selected=[]
    for sector,options in sorted(positions_by_sector.items()):
        options.sort(key=lambda p:p['cost'])
        diverse=[options[0],options[len(options)//2],options[-1]]
        for p in diverse:
            x,y,h=p['landing']
            offsets=[200] if root else [500,700,350,950,1100]
            seed=old_anchors.get(region['id']+'_Storey'+str(storey))
            if seed and not root:
                offsets.append(seed['local'][2]-h)
            for offset in sorted(set(offsets),key=lambda v:abs(v-500)):
                anchor=[x,y,h+offset]
                if hit([x,y,h+15],[x,y,h+offset+100])[0] is None:
                    selected.append({'id':region['id']+('_Entry' if root else '_Storey'+str(storey)),
                        'storey':storey,'local':anchor,'landing':p['landing'],
                        'cost':p['cost']+(0 if offset==500 else 0.3),'route_group':region['id'],
                        'region_kind':region['kind'],'opening_center':list(center)})
    return selected


def corridor(source,target,radius=100):
    a,b=point(source),point(target); delta=b-a; length=delta.length
    if length<0.01 or length>120:
        return False
    side=delta.normalized().cross(-b.normalized()).normalized()*radius/100
    up=-b.normalized()*radius/100*0.7
    for offset in (Vector((0,0,0)),side,-side,up,-up):
        if tree.ray_cast(a+offset,delta.normalized(),length)[0] is not None:
            return False
    return True


departures_cache={}; edge_cache={}
def departures(candidate,center):
    key=(candidate['route_group'],tuple(candidate['landing']))
    if key in departures_cache:
        return departures_cache[key]
    x,y,h=candidate['landing']; scale=math.sqrt(30000**2+x*x+y*y)/(30000-h)
    inward=(Vector(candidate['opening_center'])-Vector((x,y))).normalized()
    side=Vector((-inward.y,inward.x)); result=[]
    for front,lateral in ((0,0),(100,0),(200,0),(350,0),(150,150),(150,-150),(300,150),(300,-150),
                          (-100,0),(-150,0),(-175,0),(-150,100),(-150,-100),(0,150),(0,-150),(0,250),(0,-250)):
        xy=Vector((x,y))+(inward*front+side*lateral)*scale
        foot=[*xy,h]
        if not floor_safe(foot,75):
            continue
        walk=[]
        for i in range(5):
            q=Vector((x,y)).lerp(xy,i/4)
            local=[*q,h]
            if not floor_safe(local,75):
                break
            walk.append(local)
        moved=(xy-Vector((x,y))).length>1
        if len(walk)!=5 or (moved and not corridor([x,y,h+110],[*xy,h+110],70)):
            continue
        result.append({'origin':[*xy,h+200],'walk':walk})
    departures_cache[key]=result
    return result


def edge(a,b,region):
    key=(a['route_group'],tuple(a['local']),tuple(b['local']))
    if key in edge_cache:
        return edge_cache[key]
    result=None
    if a['id'].endswith('_Entry') or hit(a['local'],b['local'])[0] is None:
        for departure in departures(a,region['center']):
            distance=(point(departure['origin'])-point(b['local'])).length*100
            if distance>=1000 and corridor(departure['origin'],b['local']):
                result={'from':a['id'],'to':b['id'],'source_landing':a['landing'],
                        'origin':departure['origin'],'walk':departure['walk'],'target':b['local'],
                        'distance_cm':distance}
                break
    edge_cache[key]=result
    return result


def solve(region):
    floors=region['floors']
    if len(floors)<2:
        return [],[]
    roots=bank(region,floors[0],True)
    banks=[bank(region,i) for i in floors[1:]]
    print('PYRAMID_ROUTE_BANK '+json.dumps({'region':region['id'],'counts':[len(roots)]+[len(b) for b in banks]}),flush=True)
    assert roots and all(banks),('Missing landing bank',region['id'],[len(roots)]+[len(b) for b in banks])
    states=[]
    for target in banks[0]:
        entry=next((edge(source,target,region) for source in roots if edge(source,target,region)),None)
        if entry:
            states.append((target['cost'],[target],[entry]))
    states.sort(key=lambda s:s[0])
    assert states,('No entry route',region['id'])
    for bank_index,options in enumerate(banks[1:],1):
        next_states={}
        stats={'up':0,'down':0,'both':0,'anchor_los':0,'source_states':len(states)}
        for cost,path,routes in states:
            previous=path[-1]
            for target in options:
                up,down=edge(previous,target,region),edge(target,previous,region)
                stats['up']+=bool(up); stats['down']+=bool(down)
                stats['anchor_los']+=hit(previous['local'],target['local'])[0] is None
                if not up or not down:
                    continue
                stats['both']+=1
                skip=edge(target,path[-2],region) if len(path)>1 else None
                # 첫 중간층 세 곳은 4층→2층 직접 하강을 보장한다. 상부는 왕복을 우선한다.
                if len(path)==2 and not skip:
                    continue
                links=[up,down]+([skip] if skip else [])
                new_cost=cost+target['cost']+sum(r['distance_cm'] for r in links)/100000+(10 if len(path)>1 and not skip else 0)
                key=(tuple(previous['local']),tuple(target['local']))
                if key not in next_states or new_cost<next_states[key][0]:
                    next_states[key]=(new_cost,path+[target],routes+links)
        states=list(next_states.values())
        if not states:
            (OUT/(region['id']+'_failed-pair.json')).write_text(json.dumps({'stats':stats,
                'previous':banks[bank_index-1],'next':options},indent=2),encoding='utf-8')
        assert states,('No bidirectional/skip-floor route',region['id'],floors[bank_index:bank_index+2])
    _,anchors,routes=min(states,key=lambda s:s[0])
    pairs={(r['from'],r['to']) for r in routes}
    for source in anchors:
        for target in anchors:
            if source is target or (source['id'],target['id']) in pairs:
                continue
            direct=edge(source,target,region)
            if direct:
                routes.append(direct); pairs.add((source['id'],target['id']))
    for source in anchors:
        reachable={source['id']}
        for _ in anchors:
            reachable|={b for a,b in pairs if a in reachable}
        assert {a['id'] for a in anchors}<=reachable,('Upper storeys require a lowest-floor detour',region['id'],source['id'])
    for a in anchors:
        a['max_distance']=math.ceil(max(r['distance_cm'] for r in routes if r['to']==a['id'])/500)*500+500
        a['approach']=next(r['origin'] for r in routes if r['to']==a['id'])
        a.pop('cost',None)
        if region['kind']=='rupture':
            a['rupture_id']=region['id']
        if region['kind']=='hall':
            a['hall_id']=region['id']
    return anchors,routes


regions=[]
for record in old_coverage:
    obj=scene.objects[record['rupture_id']]
    center=next(v['center'][:2] for v in manifest['volumetric_damage'] if v['id']==obj.name)
    regions.append({'id':obj.name,'kind':'rupture','object':obj,'center':center,'floors':record['affected_storeys']})
for hall in manifest['double_height_halls']:
    obj=ns['prism']('RouteVoid_'+hall['id'],hall['outline'],hall['bottom']-0.1,hall['top'])
    regions.append({'id':hall['id'],'kind':'hall','object':obj,'center':hall['center'][:2],
                    'outline':hall['outline'],'floors':[hall['storey'],hall['storey']+1]})

chosen=[]; links=[]; coverage=[]; failures=[]
for region in regions:
    try:
        anchors,routes=solve(region)
        chosen.extend(anchors); links.extend(routes)
        coverage.append({'route_group':region['id'],'region_kind':region['kind'],
                         'affected_storeys':region['floors'],'anchors':[a['id'] for a in anchors],
                         'routes':routes})
        print('PYRAMID_REGION_ROUTE_READY '+region['id']+' '+str(len(routes)),flush=True)
    except AssertionError as error:
        failures.append({'region':region['id'],'error':str(error)})
        print('PYRAMID_REGION_ROUTE_FAILED '+region['id']+' '+str(error),flush=True)
(OUT/'route-diagnostics.json').write_text(json.dumps({'failures':failures,'coverage':coverage},indent=2),encoding='utf-8')
assert not failures,('Incomplete route groups',failures)
placement['anchors']=[a for a in placement['anchors'] if not a.get('rupture_id') and not a.get('route_group')]+chosen
(OUT/'placement.json').write_text(json.dumps(placement,indent=2),encoding='utf-8')
(OUT/'rupture-anchor-verification.json').write_text(json.dumps({'coverage':coverage,'anchors':chosen,'routes':links,
    'landing_radius_cm':250,'approach_corridor_radius_cm':100,
    'limits':['Standing departures, short repositioning and LOS corridors do not replace user grapple momentum tests']},indent=2),encoding='utf-8')
print('PYRAMID_ROUTE_ANCHORS_READY '+str(len(chosen))+' routes='+str(len(links)),flush=True)
