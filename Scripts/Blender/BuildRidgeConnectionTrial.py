"""U5a-3: 고정한 몸체/배치 사이에 별도 보행 연결로 시각 초안을 만든다.

기존 생성기를 재실행하지 않는다. 보존 형상과 조사안을 읽고 새 판만 출력한다.
Blender --background --factory-startup --python <파일> -- build V001
"""
import json
import math
from pathlib import Path
import sys
import bpy
import numpy as np
from mathutils import Vector
from mathutils.geometry import tessellate_polygon

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(Path(__file__).parent))
import BuildRidgeClosedBodyTrial as base
import RefineRidgeBodyTrial as prior

UNIT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_3_V001'
VERSION=sys.argv[sys.argv.index('--')+2]
assert len(VERSION)==4 and VERSION[0]=='V' and VERSION[1:].isdigit()
OUT=UNIT/('Geometry_'+VERSION)
READ=base.read
END_XY=None
END_DELTAS=None

def walking_h(p,axis,slope,offset):
    h=p[2]+float(np.dot(slope,axis*offset))
    if END_XY is not None:
        weight=np.clip(1-np.linalg.norm(p[:2]-END_XY)/500,0,1)
        weight=weight*weight*(3-2*weight)
        h+=float(np.interp(offset,[-150,0,150],END_DELTAS))*weight
    return h

def write(name,data):
    (OUT/name).write_text(json.dumps(data,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')

def inputs():
    hashes=READ(UNIT/'PreservationHashes.json')
    assert not [r['path'] for r in hashes if base.sha(ROOT/r['path'])!=r['sha256']]
    survey=READ(ROOT/'Saved/Profiling/Graphics/Phase01/U5a_2_V001/ConnectionSurvey.json')
    chosen=next(r for r in survey['results'] if r['id']=='NorthStepLanding_7600')
    return chosen,hashes

def binary(path):return prior.binary(path)

def native_parts(points):
    config,raw,kept,catalog,geometry=base.load_inputs()
    sample=np.asarray([base.world(*p) for p in points]);qlo=sample.min(0)-1200;qhi=sample.max(0)+1200
    parts=[]
    def add(row,mesh,owner):
        name,lo,hi=raw['meshes'][mesh]
        if name not in catalog:return
        if any(s in name.lower() for s in ('vine','ivy','plant','fern','shrub','tree','grass')):return
        dims=sorted(abs((hi[i]-lo[i])*row[9+i]) for i in range(3))
        if not (dims[0]>=100 and dims[2]>=300 or dims[1]>=300 and dims[2]>=500):return
        rot=base.rotation(row[5:9]);origin=np.asarray(row[2:5]);scale=np.asarray(row[9:12])
        corners=np.asarray([[x,y,z] for x in (lo[0],hi[0]) for y in (lo[1],hi[1]) for z in (lo[2],hi[2])])
        world=(corners*scale)@rot.T+origin;lo=world.min(0);hi=world.max(0)
        if (hi<qlo).any() or (lo>qhi).any():return
        parts.append({'mesh':name,'owner':owner,'origin':origin,'rot':rot,'scale':scale,
            'bounds':[lo,hi],'geometry':geometry(name)})
    for _,row,owner,mesh in kept:add(row,mesh,owner)
    sources={'01b':('ASM_Z1_CrownSouth',(-3200,-1450,-850,20)),
        '01c':('ASM_Z1_PlateauSouthCrown',(-4600,-4400,300,30)),
        '02c':('ASM_Z1_NorthRimWall',(-1950,3800,-2900,-86)),
        '02d':('ASM_Z1_WallNorth',(-2000,2300,-4400,6)),
        '02e':('ASM_Z1_WallCenter',(-2150,-200,-4100,-8))}
    for file in config['authoring_files']:
        if not file['path'].endswith('.json'):continue
        for target in READ(ROOT/file['path']).get('level_instances',[]):
            source=target['source_level'].split('_')[-1]
            if source not in sources:continue
            owner,pose=sources[source];op,oq=prior.env['frame'](*pose)
            tp,tq=prior.env['frame'](*target['local_xyh_cm'],target['rotation_deg'][1])
            dq=prior.env['mul'](tq,prior.env['inv'](oq));delta=base.rotation(dq)
            assert max(abs(s-1) for s in target['scale'])<1e-5
            for original in raw['instances']:
                comp=raw['components'][original[0]]
                if comp[0]!=owner:continue
                row=list(original);row[2:5]=((np.asarray(original[2:5])-op)@delta.T+tp).tolist()
                row[5:9]=prior.env['mul'](dq,original[5:9]);add(row,comp[3],target['label'])
    cache={}
    for part in parts:
        mesh=part['mesh']
        if mesh not in cache:cache[mesh]=base.tree(*part['geometry'])
        part['tree']=cache[mesh]
    return parts

def world_part(name,v,t):
    return {'mesh':name,'owner':name,'origin':np.zeros(3),'rot':np.eye(3),'scale':np.ones(3),
        'bounds':[v.min(0),v.max(0)],'geometry':(v,t),'tree':base.tree(v,t)}

def sections(points,spacing):
    points=np.asarray(points,float);axes=[];slopes=[]
    gradients=[(b[2]-a[2])*(b[:2]-a[:2])/np.dot(b[:2]-a[:2],b[:2]-a[:2]) for a,b in zip(points,points[1:])]
    for i,p in enumerate(points):
        before=points[i,:2]-points[max(0,i-1),:2];after=points[min(i+1,len(points)-1),:2]-p[:2]
        if i==0:before=after
        if i==len(points)-1:after=before
        before/=np.linalg.norm(before);after/=np.linalg.norm(after)
        a=np.asarray([-before[1],before[0]]);b=np.asarray([-after[1],after[0]])
        m=a+b;m/=np.linalg.norm(m);axes.append(m/np.dot(m,a))
        slopes.append(np.zeros(2) if VERSION>='V003' else (gradients[max(0,i-1)]+gradients[min(i,len(gradients)-1)])/2)
    rows=[]
    for i,(a,b) in enumerate(zip(points,points[1:])):
        count=max(1,math.ceil(np.linalg.norm(b[:2]-a[:2])/spacing))
        for j in range(count):
            u=j/count;rows.append((a+(b-a)*u,axes[i]*(1-u)+axes[i+1]*u,slopes[i]*(1-u)+slopes[i+1]*u))
    return rows+[(points[-1],axes[-1],slopes[-1])]

def floor_height(x,y,reference,terrain):
    origin=base.world(x,y,reference+120);heights=[]
    direction=origin/np.linalg.norm(origin);other=origin+direction*1500
    for part in terrain:
        lo,hi=part['bounds']
        if (np.maximum(origin,other)<lo).any() or (np.minimum(origin,other)>hi).any():continue
        start=((origin-part['origin'])@part['rot'])/part['scale']
        local_dir=(direction@part['rot'])/part['scale'];factor=np.linalg.norm(local_dir);local_dir/=factor
        hit=part['tree'].ray_cast(Vector(start),Vector(local_dir),1500*factor)
        if hit[0] is not None:
            world=(np.asarray(hit[0])*part['scale'])@part['rot'].T+part['origin']
            normal=(np.asarray(hit[1])/part['scale'])@part['rot'].T;normal/=np.linalg.norm(normal)
            if abs(np.dot(normal,-direction))>=math.cos(math.radians(45)):
                heights.append(base.R-np.linalg.norm(world))
    return max(heights) if heights else None

def corridor_samples(rows):
    return [(i,off,up,base.world(*(p[:2]+axis*off),walking_h(p,axis,slope,off)+up))
        for i,(p,axis,slope) in enumerate(rows) for off in (-150,0,150) for up in (100,170,250)]

def clearance(samples,parts):
    xyz=np.asarray([r[-1] for r in samples]);hits=[];minimum=1e10
    for part in parts:
        lo,hi=part['bounds'];selected=np.where(((xyz>=lo-60)&(xyz<=hi+60)).all(axis=1))[0]
        for k in selected:
            p=xyz[k];local=((p-part['origin'])@part['rot'])/part['scale']
            nearest=part['tree'].find_nearest(Vector(local))
            if nearest[0] is None:continue
            distance=float(nearest[3]*min(abs(part['scale'])))
            if distance<60:
                if 'world_tree' not in part:
                    v,t=part['geometry'];part['world_tree']=base.tree((v*part['scale'])@part['rot'].T+part['origin'],t)
                distance=float(part['world_tree'].find_nearest(Vector(p))[3])
            minimum=min(minimum,distance)
            if distance<50:
                i,off,up,_=samples[k];hits.append({'station':i,'offset_cm':off,'up_cm':up,
                    'owner':part['owner'],'mesh':part['mesh'],'distance_cm':distance})
    return {'samples':len(samples),'minimum_surface_distance_lower_bound_cm':minimum,'hits':hits,'pass':not hits,
        'method':'10cm station, actual miter width3m, up100/170/250cm; fallback BVH lower bound, exact world distance below60cm'}

def build():
    global END_XY,END_DELTAS
    assert not (OUT/'Build.json').exists(),'Use a new geometry version'
    chosen,hashes=inputs();OUT.mkdir(parents=True,exist_ok=True)
    points=np.asarray(chosen['points'],float)
    if VERSION>='V002':points[-1,0]=-7800
    if VERSION>='V003':
        points=np.asarray([points[0],[-8500,-1600,4300],[-8500,1800,3500],[-8100,3000,2700],
            [-5200,3100,1700],[-5200,1800,1300],[-7000,2100,1000],[-7400,2800,650]],float)
    if VERSION>='V004':
        points[3,2]=2850;points[4,2]=2000;points[5,2]=1800;points[6,2]=1100
    if VERSION>='V005':
        points[4,2]=2300;points[5,2]=2000;points[6,2]=1200
    if VERSION>='V006':
        points=np.asarray([points[0],[-8500,-1600,4300],[-8500,1800,3500],[-8100,3000,2850],
            [-6500,3100,2000],[-6500,1400,1200],[-7000,2100,1200],[-7400,2800,820]],float)
    if VERSION>='V007':
        points=np.asarray([points[0],[-8500,-1600,4300],[-8500,1800,3500],[-8100,3000,2850],
            [-6500,3100,2000],[-6500,1900,1400],[-6500,1400,1400],[-7000,1300,1400],
            [-7500,1900,1200],[-7400,2800,820]],float)
    if VERSION>='V008':
        points[4,2]=2200;points[5,2]=1600;points[6,2]=1600;points[7,2]=1600
        points[8,2]=1250;points[-1,0]=-7200
    if VERSION>='V009':points=np.vstack(([-7200,-4400,3796.56435],points))
    if VERSION>='V010':points=np.vstack((points[0],[-7600,-4400,3796.56435],points[1:]))
    if VERSION>='V011':
        points[0]=[-7200,-4800,3801.62341];points[1]=[-7600,-4800,3801.62341]
    terrain=[world_part(name,*binary(UNIT/file)) for name,file in [('NativeCrust','NativeCrust.bin'),
        ('PreservedFloor','NativePreservedFloor.bin'),('PreservedShell','NativePreservedShell.bin')]]
    retained_parts=native_parts(points)
    if VERSION>='V004':terrain+=retained_parts
    # 양 끝 중심을 실제 지각/보존 Floor 바로 위5cm로 맞춘다.
    endpoint_report=[]
    for index in (0,len(points)-1):
        x,y,h=points[index]
        floor_parts=[p for p in terrain if p['owner']=='NativeCrust'] if VERSION>='V009' and index==0 else terrain
        reference=h+600 if VERSION>='V009' and index==0 else base.spec.massif_ramp_ground(x,y)+600
        floor=floor_height(x,y,reference,floor_parts);assert floor is not None
        endpoint_report.append({'xy_cm':[x,y],'previous_h_cm':h,'actual_floor_h_cm':floor,'new_h_cm':floor+5})
        points[index,2]=floor+5
    for obj in list(bpy.context.scene.objects):bpy.data.objects.remove(obj,do_unlink=True)
    rows=sections(points,50);verts=[];faces=[];walk_faces=[]
    END_XY=points[-1,:2].copy();p,axis,_=rows[-1]
    END_DELTAS=[]
    for off in (-150,0,150):
        xy=p[:2]+axis*off;floor=floor_height(*xy,base.spec.massif_ramp_ground(*xy)+600,terrain)
        assert floor is not None
        END_DELTAS.append(floor+5-p[2])
    profile=[(-250,-20),(-150,0),(0,0),(150,0),(250,-20),(220,-90),(-220,-90)]
    width=len(profile)
    for i,(p,axis,slope) in enumerate(rows):
        for offset,height in profile:
            verts.append(base.world(*(p[:2]+axis*offset),walking_h(p,axis,slope,offset)+height))
        if i:
            for j in range(width):
                a=(i-1)*width+j;b=(i-1)*width+(j+1)%width;c=i*width+(j+1)%width;d=i*width+j
                tris=[(a,b,c),(a,c,d)]
                if j in (1,2):walk_faces.extend(tris)
                faces.extend(tris)
    for offset in (0,(len(rows)-1)*width):
        polygon=[Vector(verts[offset+j]) for j in range(width)]
        faces.extend(tuple(offset+(int(v) if isinstance(v,int) else polygon.index(v)) for v in triangle)
            for triangle in tessellate_polygon([polygon]))
    obj=base.make('SM_Ridge_StairRamp3_Connection_'+VERSION,verts,faces);base.normalize(obj)
    base.OUT=OUT;base.VERSION='V008';output=base.export(obj)
    v,t=base.arrays(obj);bvh=base.tree(v,t)
    pairs=[(a,b) for a,b in bvh.overlap(bvh) if a<b and not set(t[a]).intersection(t[b])]
    grades=[]
    for a,b,c in walk_faces:
        p,q,r=np.asarray(verts)[[a,b,c]];n=np.cross(q-p,r-p);n/=np.linalg.norm(n);up=-(p+q+r);up/=np.linalg.norm(up)
        grades.append(math.degrees(math.acos(np.clip(abs(np.dot(n,up)),0,1))))
    sample_rows=sections(points,10);samples=corridor_samples(sample_rows)
    parts=retained_parts+[p for p in terrain if p['owner'] in ('NativeCrust','PreservedFloor','PreservedShell')]
    inherited_body=[]
    for row in READ(ROOT/'Saved/Profiling/Graphics/Phase01/U5aBody_V007/Build.json')['outputs']:
        part=world_part(row['name'],*binary(ROOT/'Saved/Profiling/Graphics/Phase01/U5aBody_V007'/(row['name']+'.bin')))
        inherited_body.append(part)
    clear=clearance(samples,parts+inherited_body+[world_part('NewConnectionSelf',v,t)])
    body_volume_hits=[]
    for part in inherited_body:
        lo,hi=part['bounds']
        for i,off,up,p in samples:
            if (p>=lo).all() and (p<=hi).all() and base.inside(part['tree'],p):
                body_volume_hits.append({'body':part['owner'],'station':i,'offset_cm':off,'up_cm':up})
    protected=[]
    config=READ(base.AUTHOR/'BodyTrial.json')
    for corridor in config['protected_spaces']:
        old_points=prior.env['samples'](corridor);hits=[]
        for i,p in enumerate(old_points):
            p=np.asarray(p)
            if (p>=v.min(0)).all() and (p<=v.max(0)).all() and base.inside(bvh,p):
                hits.append({'sample_index':i,'local_xyh_cm':base.spec.ridge.local(p)})
        protected.append({'id':corridor['id'],'samples':len(old_points),'hits':hits,'pass':not hits})
    footings=[]
    for index in (0,len(rows)-1):
        p,axis,slope=rows[index];measure=[]
        for off in (-150,0,150):
            # 보존 구조물 가장자리에서 높이장 마스크가 낮아져도 실제 노면 아래에서 ray를 시작하지 않는다.
            xy=p[:2]+axis*off;h=floor_height(*xy,walking_h(p,axis,slope,off)+200,terrain)
            measure.append({'offset_cm':off,'floor_h_cm':h,'gap_cm':float(walking_h(p,axis,slope,off)-h) if h is not None else None})
        footings.append({'end':'start' if index==0 else 'north','measurements':measure})
    prior_geo=READ(ROOT/'Saved/Profiling/Graphics/Phase01/U5aBody_V007/GeometryCheck.json')
    prior_by={r['id']:r for r in prior_geo['overlaps']};combined=[]
    for row in protected:
        old=prior_by[row['id']];combined.append({'id':row['id'],'samples':row['samples'],
            'hits':[dict(h,source='preserved_body') for h in old['hits']]+[dict(h,source='new_connection') for h in row['hits']]})
    report={'unit':'U5a-3','version':VERSION,'topology_pass':output['topology']['nonmanifold_edges']==0,
        'self_intersection_candidate_pairs':len(pairs),'self_intersection_pairs_preview':pairs[:30],
        'walk_width_cm':300,'visual_width_cm':500,'walk_max_triangle_grade_deg':max(grades),
        'route_clearance':clear,'route_body_volume_hits':body_volume_hits,
        'new_connection_protection':protected,'protected_spaces':51,'samples':16552,
        'overlaps':combined,'protected_pass':not any(r['hits'] for r in combined),
        'new_interferences_other50':[{'id':r['id'],'hits':len(r['hits'])} for r in protected if r['hits'] and r['id']!='StairRamp3CentralPassage'],
        'endpoint_center_adjustments':endpoint_report,'footings':footings,'seam_min_cm':float(v.min()),
        'collision_or_PIE_pass':False,'accepted':False}
    write('GeometryCheck.json',report)
    length=sum(np.linalg.norm(base.world(*b)-base.world(*a)) for a,b in zip([r[0] for r in sample_rows],[r[0] for r in sample_rows][1:]))
    write('Build.json',{'unit':'U5a-3','version':VERSION,'outputs':[output],'points_xyh_cm':points.tolist(),
        'route_length_cm':float(length),'preserved_files':len(hashes),'source_map':'U5aBody_V003',
        'old_StairRamp3':'Original51 inputs and old28 body hits retained; separate replacement connection preview',
        'role':'visual NoCollision; collision/crust/support/bake/PIE deferred','authoring_script_sha256':base.sha(Path(__file__))})
    (OUT/'BuildSource.py').write_bytes(Path(__file__).read_bytes())
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'ConnectionTrial.blend'))
    print('CONNECTION_TRIAL_READY '+json.dumps({'version':VERSION,'triangles':output['triangles'],
        'walk_grade':max(grades),'route_hits':len(clear['hits']),'body_volume_hits':len(body_volume_hits),
        'self_pairs':len(pairs),'new_protected_hits':[{'id':r['id'],'hits':len(r['hits'])} for r in protected if r['hits']],
        'footings':footings,'length_m':float(length/100)}),flush=True)

if __name__=='__main__':build()
