"""채택 배치를 보존하는 U5a 제한 몸체 생성 시험. 기존 생성물을 호출하지 않는다.

Blender --background --factory-startup --python <이 파일> -- survey|build
입력은 보존된 Cull 전 덤프+당시 제외 결정과 고유 fallback 메시다.
열린 세로 외피의 경계 닫기와 하부 공동의 구면 높이장 몸체를 각각 시험한다.
결과는 시각 검토용 후보이며 보호 공간/PIE 검증 전 채택하지 않는다.
"""
import hashlib
import json
import math
from pathlib import Path
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'Scripts')]
import RidgeZ1CompositionSpec as spec

AUTHOR=ROOT/'Scripts/Profiling/Graphics/ClosedBodyTrial_V001'
VERSION=sys.argv[sys.argv.index('--')+2] if '--' in sys.argv and len(sys.argv)>sys.argv.index('--')+2 else 'V001'
assert len(VERSION)==4 and VERSION[0]=='V' and VERSION[1:].isdigit(),'Use V001-style version'
OUT=ROOT/('Saved/Profiling/Graphics/Phase01/U5aBody_'+VERSION)
RAW=ROOT/'Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/Cull/Export_V001.json'
DECIDE=RAW.with_name('Decide_V001.json')
GEOMETRY=ROOT/'Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/Collision/Export_V037'
GRID=25.0
INSET=5.0
GAP_CELLS=20 if VERSION=='V003' else 4
PATCH=(-8500.,-6500.,-700.,2200.)
S2,S3,S6=math.sqrt(2),math.sqrt(3),math.sqrt(6)
R=spec.ridge.RADIUS

def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(name,data):
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/name).write_text(json.dumps(data,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')

def world(x,y,h):return np.asarray(spec.ridge.point(float(x),float(y),float(h)),dtype=np.float64)
def local(v):
    depth=v.sum(axis=1)/S3
    return np.column_stack((R*(-v[:,0]-v[:,1]+2*v[:,2])/S6/depth,
                            R*(-v[:,0]+v[:,1])/S2/depth,R-np.linalg.norm(v,axis=1)))
def rotation(q):
    x,y,z,w=np.asarray(q)/np.linalg.norm(q)
    return np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],
                     [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],
                     [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])
def bl(v):return v*np.array([.01,-.01,.01])
def ue(v):return v*np.array([100.,-100.,100.])

def make(name,v,t):
    mesh=bpy.data.meshes.new(name)
    mesh.from_pydata(bl(np.asarray(v)).tolist(),[],np.asarray(t)[:,::-1].tolist())
    mesh.update()
    obj=bpy.data.objects.new(name,mesh);bpy.context.scene.collection.objects.link(obj)
    return obj

def arrays(obj):
    m=obj.data;m.calc_loop_triangles()
    v=np.asarray([p.co[:] for p in m.vertices],dtype=np.float64)
    t=np.asarray([p.vertices[:] for p in m.loop_triangles],dtype=np.int64)
    return ue(v),t[:,::-1]

def topology(obj):
    bm=bmesh.new();bm.from_mesh(obj.data)
    result={'boundary_edges':sum(e.is_boundary for e in bm.edges),
      'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges),
      'vertices':len(bm.verts),'faces':len(bm.faces),'signed_volume_m3':bm.calc_volume(signed=True)}
    bm.free();return result

def normalize(obj,inset=0):
    bm=bmesh.new();bm.from_mesh(obj.data)
    bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001)
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    if bm.calc_volume(signed=True)<0:bmesh.ops.reverse_faces(bm,faces=list(bm.faces))
    bm.normal_update()
    if inset:
        for v in bm.verts:v.co-=v.normal*(inset/100)
    bm.to_mesh(obj.data);bm.free();obj.data.update()

def close_scan(name,v,t):
    obj=make(name,v,t);normalize(obj)
    before=topology(obj)
    bm=bmesh.new();bm.from_mesh(obj.data)
    # 원본은 그대로 두고 파생 몸체에서만 미세 중복 면/비다양체 분기를 정리한다.
    repaired=0
    for _ in range(8):
        bad=[e for e in bm.edges if len(e.link_faces)>2]
        if not bad:break
        remove=set()
        for edge in bad:
            remove.update(sorted(edge.link_faces,key=lambda f:f.calc_area())[:len(edge.link_faces)-2])
        repaired+=len(remove);bmesh.ops.delete(bm,geom=list(remove),context='FACES')
    count=0
    for _ in range(3):
        added=bmesh.ops.holes_fill(bm,edges=[e for e in bm.edges if e.is_boundary],sides=0)['faces']
        count+=len(added)
        bmesh.ops.triangulate(bm,faces=added)
        if not added:break
    wire=[e for e in bm.edges if not e.link_faces]
    if wire:bmesh.ops.delete(bm,geom=wire,context='EDGES')
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.normal_update()
    if bm.calc_volume(signed=True)<0:bmesh.ops.reverse_faces(bm,faces=list(bm.faces))
    bm.to_mesh(obj.data);bm.free();obj.data.update()
    after=topology(obj);voxel=None
    if after['nonmanifold_edges']:
        modifier=obj.modifiers.new('RepairDerivedBody','REMESH')
        modifier.mode='VOXEL';modifier.voxel_size=.10;modifier.adaptivity=0
        bpy.context.view_layer.objects.active=obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
        normalize(obj);after=topology(obj);voxel=10.0
    if not after['nonmanifold_edges'] and after['signed_volume_m3']>0:
        normalize(obj,INSET)
    return obj,{'before':before,'after':topology(obj),'added_caps':count,
                'derived_duplicate_faces_removed':repaired,'surface_inset_cm':INSET,'repair_voxel_cm':voxel}

def load_inputs():
    c=read(AUTHOR/'BodyTrial.json')
    for row in c['authoring_files']+c['previous_authoring_files']:
        assert sha(ROOT/row['path'])==row['sha256'],row['path']
    level=ROOT/'Content'/(c['source_level'].removeprefix('/Game/')+'.umap')
    assert sha(level)==c['source_level_sha256']
    raw,decision=read(RAW),read(DECIDE)
    removed={o:{k:set(v) for k,v in groups.items()} for o,groups in decision['remove'].items()}
    kept=[]
    for i,row in enumerate(raw['instances']):
        owner,actor,comp,mesh=raw['components'][row[0]]
        if row[1] in removed.get(owner,{}).get(actor+'|'+comp,set()):continue
        kept.append((i,row,owner,mesh))
    assert len(kept)==185555
    manifest=read(GEOMETRY/'manifest.json')
    assert sha(GEOMETRY/'instances.json')==manifest['instances_sha256']
    catalog={m['mesh']:m for m in manifest['meshes']};cache={}
    def geometry(mesh):
        if mesh not in cache:
            m=catalog[mesh];p=GEOMETRY/'meshes'/m['file'];data=p.read_bytes()
            assert sha(p)==m['sha256'],p
            nv,nt=np.frombuffer(data[:8],dtype=np.int32)
            verts=np.frombuffer(data[8:8+nv*12],dtype=np.float32).reshape(-1,3).astype(np.float64)
            tris=np.frombuffer(data[8+nv*12:],dtype=np.int32).reshape(-1,3).astype(np.int64)
            assert len(tris)==nt
            cache[mesh]=(verts,tris)
        return cache[mesh]
    return c,raw,kept,catalog,geometry

def transformed(row,mesh,geometry):
    v,t=geometry(mesh);v=(v*row[9:12])@rotation(row[5:9]).T+row[2:5]
    if np.prod(row[9:12])<0:t=t[:,::-1]
    return v,t

def tree(v,t):return BVHTree.FromPolygons(v.tolist(),t.tolist(),all_triangles=True)

def inside(bvh,p):
    # 세 방향 홀짝 판정의 다수결. 열린 입력이 아니라 생성한 닫힌 몸체에만 쓴다.
    votes=[]
    for direction in [(1,.171,.319),(.217,1,.391),(.113,.293,1)]:
        direction=Vector(direction).normalized();origin=Vector(p);n=0
        for _ in range(500):
            hit=bvh.ray_cast(origin,direction,100000)[0]
            if hit is None:break
            n+=1;origin=hit+direction*.1
        votes.append(n%2==1)
    return sum(votes)>=2

def patch_body(v,t):
    bvh=tree(v,t)
    xs=np.arange(PATCH[0],PATCH[1]+GRID*.5,GRID);ys=np.arange(PATCH[2],PATCH[3]+GRID*.5,GRID)
    heights=np.full((len(xs),len(ys)),np.nan)
    ground=np.zeros_like(heights)
    for i,x in enumerate(xs):
        for j,y in enumerate(ys):
            g=spec.massif_ramp_ground(x,y);ground[i,j]=g-50
            if VERSION>='V006':
                start=world(x,y,g+50);direction=Vector(-start).normalized()
            else:
                start=world(x,y,9000);direction=Vector(start).normalized()
            hit=bvh.ray_cast(Vector(start),direction,15000)[0]
            if hit is not None:
                h=R-hit.length
                if h>g+50:heights[i,j]=h-INSET
    initial=int(np.isfinite(heights).sum())
    report_samples={key:{'ground_h_cm':spec.massif_ramp_ground(p[0],p[1]),
      'nearest_grid_xy_cm':[float(xs[np.argmin(abs(xs-p[0]))]),float(ys[np.argmin(abs(ys-p[1]))])],
      'raw_top_h_cm':(float(heights[np.argmin(abs(xs-p[0])),np.argmin(abs(ys-p[1]))])
        if np.isfinite(heights[np.argmin(abs(xs-p[0])),np.argmin(abs(ys-p[1]))]) else None)}
      for key,p in read(AUTHOR/'BodyTrial.json')['report_points_local_xyh_cm'].items()
      if PATCH[0]<=p[0]<=PATCH[1] and PATCH[2]<=p[1]<=PATCH[3]}
    # 유효 표면으로 둘러싸인 틈만 네 방향 표본으로 제한해 닫는다. V003은 하부 공동용5m 시험이다.
    old=heights.copy();filled=0
    for i,j in np.argwhere(~np.isfinite(old)):
        nearby=[]
        for di,dj in [(1,0),(-1,0),(0,1),(0,-1)]:
            found=None
            for d in range(1,GAP_CELLS+1):
                a,b=i+di*d,j+dj*d
                if 0<=a<len(xs) and 0<=b<len(ys) and np.isfinite(old[a,b]):found=old[a,b];break
            nearby.append(found)
        if all(h is not None for h in nearby):
            heights[i,j]=max(min(nearby),ground[i,j]+100);filled+=1
    seeded=0;seed_mask=np.zeros_like(heights,dtype=bool)
    if VERSION>='V004':
        # R03 내부점과 R04 별도 진입점 사이의 제한된 공동 봉투를 명시한다.
        # 오래된 경사로와의 충돌은 감추지 않고 보호 검사에서 별도 보고한다.
        a=np.array([-7463.,343.]);b=np.array([-7863.,1591.]);delta=b-a
        overhead=np.argwhere(np.isfinite(old)&(old>ground+300))
        overhead_xy=np.array([[xs[i],ys[j]] for i,j in overhead])
        for i,x in enumerate(xs):
            for j,y in enumerate(ys):
                point=np.array([x,y]);u=np.clip(np.dot(point-a,delta)/np.dot(delta,delta),0,1)
                if np.linalg.norm(point-(a+u*delta))>350:continue
                seed_mask[i,j]=True
                distances=np.linalg.norm(overhead_xy-point,axis=1)
                choices=np.argsort(distances)[:16];choices=[k for k in choices if distances[k]<=500]
                if not choices:continue
                roof=float(np.percentile([old[tuple(overhead[k])] for k in choices],25))
                roof=max(roof,ground[i,j]+300)
                if not np.isfinite(heights[i,j]) or heights[i,j]<roof:
                    heights[i,j]=roof;seeded+=1
    active={(i,j) for i in range(len(xs)-1) for j in range(len(ys)-1)
            if np.isfinite(heights[i:i+2,j:j+2]).all()
            and (VERSION<'V006' or seed_mask[i:i+2,j:j+2].all())}
    verts=[];faces=[];ids={}
    def vertex(i,j,top):
        key=(i,j,top)
        if key not in ids:
            ids[key]=len(verts);verts.append(world(xs[i],ys[j],heights[i,j] if top else ground[i,j]))
        return ids[key]
    for i,j in sorted(active):
        corners=[(i,j),(i+1,j),(i+1,j+1),(i,j+1)]
        top=[vertex(a,b,True) for a,b in corners];bottom=[vertex(a,b,False) for a,b in corners]
        faces += [(top[0],top[1],top[2]),(top[0],top[2],top[3]),
                  (bottom[2],bottom[1],bottom[0]),(bottom[3],bottom[2],bottom[0])]
        for edge,neighbor in enumerate([(i,j-1),(i+1,j),(i,j+1),(i-1,j)]):
            if neighbor not in active:
                a,b=edge,(edge+1)%4
                faces += [(top[a],bottom[a],bottom[b]),(top[a],bottom[b],top[b])]
    assert faces,'No patch body'
    obj=make('SM_Ridge_U5a_UnderSurfaceBody_'+VERSION,verts,faces);normalize(obj)
    return obj,{'grid_cm':GRID,'patch_xy_cm':list(PATCH),'raw_valid_nodes':initial,
       'small_gap_nodes_filled':filled,'gap_search_cm':GRID*GAP_CELLS,'seeded_shared_void_nodes':seeded,
       'seed_capsule_radius_cm':350 if VERSION>='V004' else None,
       'envelope':'nearest overhead ceiling in seeded shared void only' if VERSION>='V006' else 'highest surface over patch',
       'cells':len(active),'topology':topology(obj),'report_grid_samples':report_samples}

def check():
    config=read(AUTHOR/'BodyTrial.json');build=read(OUT/'Build.json')
    objects=[];trees=[];all_v=[]
    for row in build['outputs']:
        data=(OUT/(row['name']+'.bin')).read_bytes();nv,nt=np.frombuffer(data[:8],np.int32)
        v=np.frombuffer(data[8:8+nv*12],np.float32).reshape(-1,3).astype(float)
        t=np.frombuffer(data[8+nv*12:],np.int32).reshape(-1,3)
        objects.append(row);trees.append(tree(v,t));all_v.append(v)
    helper=ROOT/'Saved/Profiling/Graphics/Phase01/U4_2_V002/DiagnosePlacement.py'
    env={};exec(compile(helper.read_text(encoding='utf-8').split('\nraw =')[0],str(helper),'exec'),env)
    overlaps=[]
    for corridor in config['protected_spaces']:
        points=env['samples'](corridor);hits=[]
        for obj,bvh,v in zip(objects,trees,all_v):
            lo,hi=v.min(axis=0),v.max(axis=0)
            for i,p in enumerate(points):
                p=np.asarray(p)
                if (p<lo).any() or (p>hi).any():continue
                if inside(bvh,p):hits.append({'body':obj['name'],'sample_index':i,'local_xyh_cm':spec.ridge.local(p.tolist())})
        overlaps.append({'id':corridor['id'],'samples':len(points),'hits':hits,'pass':not hits})
    probes={}
    for key,p in config['report_points_local_xyh_cm'].items():
        point=world(*p)
        probes[key]={'point_local_xyh_cm':p,'ground_h_cm':spec.massif_ramp_ground(*p[:2]),
                    'bodies':[{'body':o['name'],'inside':inside(bvh,point),
                     'distance_cm':float(bvh.find_nearest(Vector(point))[3])} for o,bvh in zip(objects,trees)]}
    report={'version':VERSION,'topology_pass':all(not o['topology']['nonmanifold_edges'] for o in objects),
      'protected_spaces':len(overlaps),'samples':sum(c['samples'] for c in overlaps),
      'protected_pass':not any(c['hits'] for c in overlaps),'overlaps':overlaps,'probes':probes,
      'seam_min_cm':float(np.concatenate(all_v).min()),'collision_or_PIE_pass':False}
    write('GeometryCheck.json',report)
    print('CLOSED_BODY_CHECK '+json.dumps({k:v for k,v in report.items() if k!='overlaps'}),flush=True)
    print('PROTECTED_HITS '+json.dumps([{'id':r['id'],'hits':len(r['hits'])} for r in overlaps if r['hits']]),flush=True)

def route_profile():
    row=next(r for r in read(OUT/'Build.json')['outputs'] if 'UnderSurface' in r['name'])
    data=(OUT/(row['name']+'.bin')).read_bytes();nv,nt=np.frombuffer(data[:8],np.int32)
    v=np.frombuffer(data[8:8+nv*12],np.float32).reshape(-1,3).astype(float)
    t=np.frombuffer(data[8+nv*12:],np.int32).reshape(-1,3);bvh=tree(v,t)
    a=np.array([-8500.,-3600.]);b=np.array([-7000.,2400.]);stations=[]
    for i in range(125):
        p=a+(b-a)*i/124;g=spec.massif_ramp_ground(*p);start=world(*p,10000)
        hit=bvh.ray_cast(Vector(start),Vector(start).normalized(),30000)[0]
        roof=R-hit.length if hit is not None else None
        stations.append({'xy_cm':p.tolist(),'distance_cm':float(np.linalg.norm(p-a)),
                         'existing_ground_h_cm':g,'body_roof_h_cm':roof})
    write('StairRamp3Profile.json',{'id':'StairRamp3','stations':stations,
       'note':'Body top height only; not a proposed or validated replacement ramp'})
    conflicts=[p for p in stations if p['body_roof_h_cm'] is not None and p['body_roof_h_cm']>p['existing_ground_h_cm']+100]
    print('STAIR_PROFILE '+json.dumps({'conflicting_stations':len(conflicts),
       'max_roof_over_old_ground_cm':max(p['body_roof_h_cm']-p['existing_ground_h_cm'] for p in conflicts)}),flush=True)

def surface_fit():
    config,raw,kept,catalog,geometry=load_inputs();report=[]
    rows={i:(row,mesh) for i,row,owner,mesh in kept};rng=np.random.default_rng(55001)
    for output in read(OUT/'Build.json')['outputs']:
        if 'ScanBody' not in output['name']:continue
        index=int(output['name'].split('_')[-2]);row,mesh_id=rows[index]
        original,_=transformed(row,raw['meshes'][mesh_id][0],geometry)
        data=(OUT/(output['name']+'.bin')).read_bytes();nv,nt=np.frombuffer(data[:8],np.int32)
        v=np.frombuffer(data[8:8+nv*12],np.float32).reshape(-1,3).astype(float)
        t=np.frombuffer(data[8+nv*12:],np.int32).reshape(-1,3);bvh=tree(v,t)
        points=original[rng.choice(len(original),min(3000,len(original)),replace=False)]
        distances=[bvh.find_nearest(Vector(p))[3] for p in points]
        report.append({'body':output['name'],'samples':len(points),
            'source_surface_to_body_cm':dict(zip(['p50','p95','max'],map(float,np.percentile(distances,[50,95,100]))))})
    write('SurfaceFit.json',{'seed':55001,'scans':report,
         'note':'Original fallback vertices to derived body only; not Floor collision fit or cap appearance approval'})
    print('SURFACE_FIT '+json.dumps(report),flush=True)

def export(obj):
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    # 몸체 검토용 UV. 원본 외피의 UV/메시/재질은 건드리지 않는다.
    if VERSION>='V005':
        # 전 표면을 0~1에 조밀하게 패킹하면 작은 격자 삼각형의 UV가 수치적으로 퇴화한다.
        # 새 몸체에만 월드4m 단위의 우세 축 투영을 써서 탄젠트의 UV 면적을 확보한다.
        while obj.data.uv_layers:obj.data.uv_layers.remove(obj.data.uv_layers[0])
        layer=obj.data.uv_layers.new(name='BodyProjection')
        for poly in obj.data.polygons:
            axis=max(range(3),key=lambda i:abs(poly.normal[i]));uv_axes=[i for i in range(3) if i!=axis]
            for loop_index in poly.loop_indices:
                p=obj.data.vertices[obj.data.loops[loop_index].vertex_index].co
                layer.data[loop_index].uv=(p[uv_axes[0]]/4,p[uv_axes[1]]/4)
    else:
        bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.02)
        bpy.ops.object.mode_set(mode='OBJECT')
    p=OUT/(obj.name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(p),use_selection=True,object_types={'MESH'},
       apply_scale_options='FBX_SCALE_NONE',global_scale=1,axis_forward='-Z',axis_up='Y',
       mesh_smooth_type='FACE',add_leaf_bones=False,bake_anim=False)
    v,t=arrays(obj)
    binary=np.array([len(v),len(t)],np.int32).tobytes()+v.astype(np.float32).tobytes()+t.astype(np.int32).tobytes()
    (OUT/(obj.name+'.bin')).write_bytes(binary)
    return {'name':obj.name,'file':p.relative_to(ROOT).as_posix(),'sha256':sha(p),
       'vertices':len(v),'triangles':len(t),'bounds_cm':[v.min(axis=0).tolist(),v.max(axis=0).tolist()],
       'topology':topology(obj),'role':'trial_visual_NoCollision; common closed body candidate, collision not yet derived'}

def run(mode):
    assert mode in ('survey','build'),mode
    assert not (OUT/'Build.json').exists(),'Use a new output version; do not rebuild existing trial'
    config,raw,kept,catalog,geometry=load_inputs()
    assert bpy.context.mode=='OBJECT'
    assert not any(o.type=='MESH' and o.name!='Cube' for o in bpy.context.scene.objects),'Use factory startup'
    # factory-startup의 기본 큐브만 새 임시 프로세스에서 제외한다.
    for obj in list(bpy.context.scene.objects):bpy.data.objects.remove(obj,do_unlink=True)
    OUT.mkdir(parents=True,exist_ok=True)
    selected=[];input_v=[];input_t=[];offset=0
    primary=[35430,116490,116462]
    objects=[];scans=[]
    for index,row,owner,mesh_index in kept:
        mesh,low,high=raw['meshes'][mesh_index]
        if index in primary:
            v,t=transformed(row,mesh,geometry)
            obj,stats=close_scan('SM_Ridge_U5a_ScanBody_'+str(index)+'_'+VERSION,v,t)
            objects.append(obj);scans.append({'raw_row':index,'owner':owner,'mesh':mesh,**stats})
        if owner not in ('CR_Z1_LargeAssembly','CR_Z1_Drape'):continue
        if any(s in mesh.lower() for s in ('vine','ivy','plant','fern','shrub','tree','grass')):continue
        if not any(s in mesh.lower() for s in ('cliff','boulder','rock','embankment','terrain','ground','mesa')):continue
        if mesh not in catalog:continue
        dims=sorted(abs((high[i]-low[i])*row[9+i]) for i in range(3))
        if not (dims[0]>=100 and dims[-1]>=300 or dims[1]>=300 and dims[2]>=500):continue
        # 상자로 먼저 솎고 실제 표면은 fallback 삼각형을 쓴다.
        corners=np.array([[x,y,z] for x in (low[0],high[0]) for y in (low[1],high[1]) for z in (low[2],high[2])])
        bounds=local((corners*row[9:12])@rotation(row[5:9]).T+row[2:5])
        if bounds[:,0].max()<PATCH[0]-200 or bounds[:,0].min()>PATCH[1]+200:continue
        if bounds[:,1].max()<PATCH[2]-200 or bounds[:,1].min()>PATCH[3]+200:continue
        v,t=transformed(row,mesh,geometry)
        input_v.append(v);input_t.append(t+offset);offset+=len(v)
        selected.append({'raw_row':index,'owner':owner,'mesh':mesh,'triangles':len(t)})
    assert len(scans)==3
    write('InputSurvey.json',{'kept_instances':len(kept),'scans':scans,'patch_inputs':selected,
      'patch_triangles':sum(r['triangles'] for r in selected),
      'input_files':[{'path':p.relative_to(ROOT).as_posix(),'sha256':sha(p)} for p in [RAW,DECIDE,GEOMETRY/'manifest.json',AUTHOR/'BodyTrial.json']]})
    if mode=='survey':
        print('CLOSED_BODY_SURVEY '+json.dumps({'scans':scans,'patch_inputs':len(selected),'patch_triangles':sum(r['triangles'] for r in selected)}),flush=True)
        return
    assert not (OUT/'Build.json').exists(),'Use a new output version'
    body,stats=patch_body(np.concatenate(input_v),np.concatenate(input_t));objects.append(body)
    outputs=[export(obj) for obj in objects]
    trees=[tree(*arrays(obj)) for obj in objects]
    issue_points=config['report_points_local_xyh_cm']
    occupancy={key:{'point':value,'inside_bodies':[obj.name for obj,bvh in zip(objects,trees) if inside(bvh,world(*value))]} for key,value in issue_points.items()}
    write('Build.json',{'status':'generated_pending_clearance_and_visual_review','source_level':config['source_level'],
        'source_level_sha256':config['source_level_sha256'],'outputs':outputs,'scan_stats':scans,'patch_stats':stats,
        'report_point_occupancy':occupancy,'protected_spaces':'pending exact body surface/volume checks',
        'collision':'not derived; trial visuals NoCollision','notes':['Single report points are camera positions, not complete hole boundaries',
          'Closed topology alone does not prove self-intersection free or playable body']})
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'BodyTrial.blend'))
    print('CLOSED_BODY_BUILD '+json.dumps({'outputs':len(outputs),'patch':stats,'occupancy':occupancy}),flush=True)

if __name__=='__main__':
    mode=sys.argv[sys.argv.index('--')+1]
    if mode=='check':check()
    elif mode=='profile':route_profile()
    elif mode=='fit':surface_fit()
    else:run(mode)
