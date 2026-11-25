"""U5a-2: 보존한 V006 파생 몸체의 경계 보정과 별도 연결 조사.

이전 생성기를 재실행하지 않고 저장된 이진 형상만 읽는다. 원본/채택 에셋은 수정하지 않는다.
Blender --background --factory-startup --python <파일> -- survey|build V007
"""
import importlib.util
import json
import math
from pathlib import Path
import sys

import bpy
import numpy as np
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(Path(__file__).parent))
import BuildRidgeClosedBodyTrial as base

OUT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_2_V001'
GEOM=ROOT/'Saved/Profiling/Graphics/Phase01/U5aBody_V007'
PRIOR=ROOT/'Saved/Profiling/Graphics/Phase01/U5aBody_V006'
read=base.read
helper=ROOT/'Saved/Profiling/Graphics/Phase01/U4_2_V002/DiagnosePlacement.py'
env={};exec(compile(helper.read_text(encoding='utf-8').split('\nraw =')[0],str(helper),'exec'),env)

def write(name,data):
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/name).write_text(json.dumps(data,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')

def binary(path):
    data=path.read_bytes();nv,nt=np.frombuffer(data[:8],np.int32)
    return (np.frombuffer(data[8:8+nv*12],np.float32).reshape(-1,3).astype(float),
            np.frombuffer(data[8+nv*12:],np.int32).reshape(-1,3))

def protected_inputs():
    previous=read(ROOT/'Scripts/Profiling/Graphics/ClosedBodyTrial_V001/Generation_V002.json')
    hashes={r['path']:r['sha256'] for r in previous['preservation_files']}
    for row in read(ROOT/'Saved/Profiling/Graphics/Phase01/U5a_1_V002/FinalReviewHashes.json'):
        hashes[row['path']]=row['sha256']
    for path,digest in hashes.items():assert base.sha(ROOT/path)==digest,path
    return hashes

def polyline(points,spacing=50):
    stations=[]
    for a,b in zip(np.asarray(points,float),np.asarray(points,float)[1:]):
        count=max(1,math.ceil(np.linalg.norm(b[:2]-a[:2])/spacing))
        for j in range(count):stations.append(a+(b-a)*j/count)
    return np.asarray(stations+[np.asarray(points[-1],float)])

def candidate_points():
    g=base.spec.massif_ramp_ground
    start=[-8500.,-3600.,g(-8500,-3600)]
    end=[-7000.,2400.,g(-7000,2400)]
    # 먼저 짧은 서쪽 연결, 실패하면 기존 Ramp2를 잇는 넓은 우회를 함께 비교한다.
    result=[]
    for x in (-8700,-9200,-9700,-10200):
        for roof in (1200,1800,2400):
            result.append({'id':f'West_{abs(x)}_H{roof}','points':[start,[x,-1700,roof+1000],
                [x,2200,roof],[x+500,3400,1000],[-7500,3400,650],end]})
    result.append({'id':'ViaExistingRamp2','points':[start,[-9200,-3200,g(-9200,-3200)],
        [-10700,2600,g(-10700,2600)],[-10700,4300,1000],[-7600,4300,800],end]})
    for x in (-8750,-9000,-9250):
        result.append({'id':f'HighShoulder_{abs(x)}','points':[start,[x,-1700,4000],
            [x,-500,3000],[x,2100,1000],[-8000,2400,850],end]})
    for y in (1200,1700,2100):
        result.append({'id':f'Ramp2Inner_Y{y}','points':[start,[-9200,-3200,g(-9200,-3200)],
            [-10400,900,1400],[-10300,y,1400],[-9000,y,1400],[-8500,2400,1300],[-7800,2400,1000],end]})
    for x in (-8500,-8750,-9000):
        result.append({'id':f'UpperNorthLoop_{abs(x)}','points':[start,[x,-1600,4200],
            [x,1800,3200],[-8300,3100,2500],[-6800,3000,1400],end]})
    result.append({'id':'EastInner','points':[start,[-7100,-3400,4800],[-6600,-1800,4800],
        [-6250,-500,3200],[-6400,1600,1400],end]})
    for x in (-8750,-9000):
        result.append({'id':f'UpperEastReturn_{abs(x)}','points':[start,[x,-1600,4200],
            [x,1600,3200],[-8500,2800,2500],[-7400,2900,1900],[-6200,2400,1100],
            [-5700,1400,650],[-6400,1300,650],end]})
    for h in (800,900,1000):
        result.append({'id':f'InnerNorthReturn_H{h}','points':[start,[-8500,-1600,4300],
            [-8500,1800,3500],[-8100,3000,2700],[-7000,3100,2000],[-6100,3100,1400],
            [-6250,2400,1000],[-6700,2000,h],[-7000,2400,h]]})
    north_points=result[-1]['points'][:-2]
    for x,y in [(-7400,2900),(-7500,3000),(-7600,3000)]:
        result.append({'id':f'NorthStepLanding_{abs(x)}','points':north_points+[
            [-6700,2000,1000],[-7000,2400,1000],[x,y,g(x,y)+50]]})
    return result

def rock_parts():
    config,raw,kept,catalog,geometry=base.load_inputs()
    sources={'01b':('ASM_Z1_CrownSouth',(-3200,-1450,-850,20)),
             '01c':('ASM_Z1_PlateauSouthCrown',(-4600,-4400,300,30)),
             '02c':('ASM_Z1_NorthRimWall',(-1950,3800,-2900,-86)),
             '02d':('ASM_Z1_WallNorth',(-2000,2300,-4400,6)),
             '02e':('ASM_Z1_WallCenter',(-2150,-200,-4100,-8))}
    parts=[]
    def add(row,mesh,owner):
        name,lo,hi=raw['meshes'][mesh]
        if name not in catalog:return
        if any(s in name.lower() for s in ('vine','ivy','plant','fern','shrub','tree','grass')):return
        dims=sorted(abs((hi[i]-lo[i])*row[9+i]) for i in range(3))
        if not (dims[0]>=100 and dims[2]>=300 or dims[1]>=300 and dims[2]>=500):return
        corners=np.asarray([[x,y,z] for x in (lo[0],hi[0]) for y in (lo[1],hi[1]) for z in (lo[2],hi[2])])
        rot=base.rotation(row[5:9]);world=(corners*row[9:12])@rot.T+row[2:5];bounds=base.local(world)
        if bounds[:,0].max() < -11500 or bounds[:,0].min()>-6500:return
        if bounds[:,1].max() < -4300 or bounds[:,1].min()>5000:return
        parts.append({'mesh':name,'owner':owner,'origin':np.asarray(row[2:5]),'rot':rot,
                      'scale':np.asarray(row[9:12]),'bounds':[world.min(0),world.max(0)]})
    for _,row,owner,mesh in kept:add(row,mesh,owner)
    # 채택 추가 소유자는 원본 조립체 전체를 채택 자세로 옮겨 읽는다. Cull된 Z1과 구별한다.
    for path in config['authoring_files']:
        if not path['path'].endswith('.json'):continue
        zone=read(ROOT/path['path'])
        for target in zone.get('level_instances',[]):
            source=target['source_level'].split('_')[-1]
            if source not in sources:continue
            owner,pose=sources[source];op,oq=env['frame'](*pose)
            tp,tq=env['frame'](*target['local_xyh_cm'],target['rotation_deg'][1])
            delta=base.rotation(env['mul'](tq,env['inv'](oq)))
            assert max(abs(s-1) for s in target['scale'])<1e-5,target['label']
            for original in raw['instances']:
                comp=raw['components'][original[0]]
                if comp[0]!=owner:continue
                row=list(original);row[2:5]=((np.asarray(original[2:5])-op)@delta.T+tp).tolist()
                row[5:9]=env['mul'](env['mul'](tq,env['inv'](oq)),original[5:9])
                add(row,comp[3],target['label'])
    cache={}
    for part in parts:
        mesh=part['mesh']
        if mesh not in cache:cache[mesh]=base.tree(*geometry(mesh))
        part['tree']=cache[mesh]
        part['geometry']=geometry(mesh)
    print('ROUTE_ROCK_PARTS '+str(len(parts)),flush=True)
    return parts

def clearance(stations,parts,half=150):
    minimum=1e10;hits=[];points=[]
    for index,p in enumerate(stations):
        tangent=stations[min(index+1,len(stations)-1),:2]-stations[max(index-1,0),:2]
        normal=np.array([-tangent[1],tangent[0]])/np.linalg.norm(tangent)
        for offset in (-half,0,half):
            xy=p[:2]+normal*offset
            for up in (100,170,250):points.append((index,offset,up,base.world(*xy,p[2]+up)))
    xyz=np.asarray([p[-1] for p in points])
    for part in parts:
        lo,hi=part['bounds'];selected=np.where(((xyz>=lo-60)&(xyz<=hi+60)).all(axis=1))[0]
        for k in selected:
            p=xyz[k];local=((p-part['origin'])@part['rot'])/part['scale']
            nearest=part['tree'].find_nearest(Vector(local))
            if nearest[0] is None:continue
            # 非등방 크기에 대해 보수적인 하한. 통과 판정은 50cm보다 큰 하한만 허용한다.
            distance=float(nearest[3]*np.min(np.abs(part['scale'])))
            if distance<50:
                # 납작하게 늘린 지표 판은 min(scale) 하한만으로 오검출이 많다.
                # 경계 후보에 한해서 실제 월드 삼각형 BVH 거리로 다시 확인한다.
                if 'world_tree' not in part:
                    vertices,triangles=part['geometry']
                    vertices=(vertices*part['scale'])@part['rot'].T+part['origin']
                    part['world_tree']=base.tree(vertices,triangles)
                distance=float(part['world_tree'].find_nearest(Vector(p))[3])
            minimum=min(minimum,distance)
            if distance<50:
                index,offset,up,_=points[k]
                hits.append({'station':index,'offset_cm':offset,'up_cm':up,'owner':part['owner'],
                             'mesh':part['mesh'],'distance_lower_bound_cm':distance})
    return {'samples':len(points),'minimum_nearby_surface_lower_bound_cm':minimum if minimum<1e9 else None,
            'hits':hits,'pass':not hits,'method':'mesh-local fallback BVH lower bound; boundary candidates exact world BVH; 50cm padding, 50cm stations, width3m, up100/170/250cm'}

def survey():
    hashes=protected_inputs();parts=rock_parts();results=[]
    for row in candidate_points()[-3:]:
        points=polyline(row['points']);grades=[];ground_above=[]
        for a,b in zip(points,points[1:]):
            pa,pb=base.world(*a),base.world(*b);up=-pa/np.linalg.norm(pa);d=pb-pa
            grades.append(math.degrees(math.atan2(abs(np.dot(d,up)),np.linalg.norm(d-up*np.dot(d,up)))))
        for p in points:ground_above.append(base.spec.massif_ramp_ground(*p[:2])-p[2])
        result={**row,'stations':points.tolist(),'max_grade_deg':max(grades),
                'max_ground_over_route_cm':max(ground_above),'clearance':clearance(points,parts)}
        results.append(result)
        print('ROUTE_CANDIDATE '+json.dumps({'id':row['id'],'grade':max(grades),'ground_over':max(ground_above),
            'hits':len(result['clearance']['hits'])}),flush=True)
    write('ConnectionSurvey.json',{'results':results,'preserved_files':len(hashes),
        'note':'Surface clearance only; no closed-rock interior, swept capsule, collision, bake or PIE approval'})
    footings=[]
    for x,y in [(-7000,2400),(-6700,2000),(-8500,-3600)]:
        nominal=base.spec.massif_ramp_ground(x,y);origin=base.world(x,y,nominal+1800)
        direction=origin/np.linalg.norm(origin);hits=[]
        for part in parts:
            lo,hi=part['bounds']
            other=base.world(x,y,nominal-500)
            if (np.maximum(origin,other)<lo).any() or (np.minimum(origin,other)>hi).any():continue
            if 'world_tree' not in part:
                verts,tris=part['geometry']
                part['world_tree']=base.tree((verts*part['scale'])@part['rot'].T+part['origin'],tris)
            hit=part['world_tree'].ray_cast(Vector(origin),Vector(direction),2300)
            if hit[0] is not None:
                h=base.R-hit[0].length;up=-direction
                hits.append({'h_cm':h,'slope_deg':math.degrees(math.acos(np.clip(np.dot(hit[1],up),-1,1))),
                    'owner':part['owner'],'mesh':part['mesh']})
        footings.append({'xy_cm':[x,y],'ground_spec_h_cm':nominal,'surface_hits':sorted(hits,key=lambda r:-r['h_cm'])})
    write('ArrivalSurfaceProbe.json',footings)

def refined_bodies():
    protected_inputs()
    assert not (GEOM/'Build.json').exists(),'Use a new geometry version'
    GEOM.mkdir(parents=True,exist_ok=True)
    for obj in list(bpy.context.scene.objects):bpy.data.objects.remove(obj,do_unlink=True)
    outputs=[];stats=[]
    for old in read(PRIOR/'Build.json')['outputs']:
        v,t=binary(PRIOR/(old['name']+'.bin'))
        if 'UnderSurface' in old['name']:
            coords=base.local(v);a=np.array([-7463.,343.]);b=np.array([-7863.,1591.]);d=b-a
            changed=0;maximum=0
            for i,(x,y,h) in enumerate(coords):
                ground=base.spec.massif_ramp_ground(x,y)-50
                if h<ground+5:continue
                point=np.array([x,y]);u=np.clip(np.dot(point-a,d)/np.dot(d,d),0,1)
                distance=np.linalg.norm(point-a-u*d)
                weight=np.clip((350-distance)/125,0,1);weight=weight*weight*(3-2*weight)
                target=ground+65+(h-ground-65)*weight
                if target<h-.001:
                    v[i]=base.world(x,y,target);changed+=1;maximum=max(maximum,h-target)
            stats.append({'name':old['name'],'boundary_taper_cm':125,'core_radius_cm':225,
                'changed_vertices':changed,'maximum_height_reduction_cm':maximum,
                'note':'R03/R04 center and core unchanged; outer toe lowered to ground+15cm'})
        obj=base.make(old['name'].replace('V006','V007'),v,t);base.normalize(obj)
        outputs.append(base.export(obj))
    return outputs,stats

def build():
    base.OUT=GEOM
    outputs,stats=refined_bodies()
    for row in outputs:
        assert row['topology']['nonmanifold_edges']==row['topology']['boundary_edges']==0,row['name']
    GEOM.mkdir(parents=True,exist_ok=True)
    base.write('Build.json',{'unit':'U5a-2','status':'generated_pending_editor_and_manual_review',
        'source_level':read(base.AUTHOR/'BodyTrial.json')['source_level'],'outputs':outputs,
        'body_refinements':stats,'connection_status':'manual landing selection required; no connection mesh placed',
        'source_geometry':{'path':(PRIOR/'Build.json').relative_to(ROOT).as_posix(),'sha256':base.sha(PRIOR/'Build.json')},
        'authoring_script_sha256':base.sha(Path(__file__)),
        'collision':'visual NoCollision; no derived collision, crust edits, bake or PIE'})
    bpy.ops.wm.save_as_mainfile(filepath=str(GEOM/'BodyTrial.blend'))
    base.check()
    geometry=read(GEOM/'GeometryCheck.json')
    print('BODY_REFINEMENT_READY '+json.dumps({'outputs':len(outputs),
        'protected_hits':[{'id':r['id'],'hits':len(r['hits'])} for r in geometry['overlaps'] if r['hits']]}),flush=True)


if __name__=='__main__':
    mode=sys.argv[sys.argv.index('--')+1]
    if mode=='survey':survey()
    elif mode=='build':build()
    else:raise ValueError(mode)
