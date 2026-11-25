"""U5a-4: 채택 연결 동선/중앙3m 면을 보존하고 바깥 석재 외형을 다듬는다.

이전 생성기를 호출하지 않고 고정한 실제 이진 형상을 읽는다.
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
import BuildRidgeConnectionTrial as conn

UNIT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_4_V001'
VERSION=sys.argv[sys.argv.index('--')+2]
assert len(VERSION)==4 and VERSION[0]=='V' and VERSION[1:].isdigit()
OUT=UNIT/('Geometry_'+VERSION)
PREVIOUS=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_3_V001'
SOURCE=PREVIOUS/'Geometry_V011'
read=base.read
def write(name,data):(OUT/name).write_text(json.dumps(data,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')

def export(obj):
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    mesh=obj.data
    uv=mesh.uv_layers.new(name='StoneProjection')
    for poly in mesh.polygons:
        axis=max(range(3),key=lambda i:abs(poly.normal[i]));axes=[i for i in range(3) if i!=axis]
        tiling=4 if poly.material_index==0 else 2.5
        for loop in poly.loop_indices:
            p=mesh.vertices[mesh.loops[loop].vertex_index].co
            uv.data[loop].uv=(p[axes[0]]/tiling,p[axes[1]]/tiling)
    path=OUT/(obj.name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},
        apply_scale_options='FBX_SCALE_NONE',global_scale=1,axis_forward='-Z',axis_up='Y',
        mesh_smooth_type='FACE',add_leaf_bones=False,bake_anim=False)
    v,t=base.arrays(obj)
    (OUT/(obj.name+'.bin')).write_bytes(np.asarray([len(v),len(t)],np.int32).tobytes()+v.astype(np.float32).tobytes()+t.astype(np.int32).tobytes())
    return {'name':obj.name,'file':path.relative_to(ROOT).as_posix(),'sha256':base.sha(path),
        'vertices':len(v),'triangles':len(t),'bounds_cm':[v.min(0).tolist(),v.max(0).tolist()],
        'topology':base.topology(obj),'role':'closed stone form visual NoCollision; walking surface held',
        'material_slots':['RidgeWalk_Surface','RidgeStone_Sides']}

def visible_hit(point,parts,maximum=650):
    direction=point/np.linalg.norm(point);origin=point+direction*100;end=point+direction*maximum;best=None
    for part in parts:
        lo,hi=part['bounds']
        if (np.maximum(origin,end)<lo).any() or (np.minimum(origin,end)>hi).any():continue
        p=((origin-part['origin'])@part['rot'])/part['scale']
        d=(direction@part['rot'])/part['scale'];factor=np.linalg.norm(d);d/=factor
        hit=part['tree'].ray_cast(Vector(p),Vector(d),(maximum-100)*factor)
        if hit[0] is None:continue
        world=(np.asarray(hit[0])*part['scale'])@part['rot'].T+part['origin']
        distance=float(np.dot(world-point,direction))
        if 120<distance<maximum and (best is None or distance<best['distance_cm']):
            best={'distance_cm':distance,'owner':part['owner'],'mesh':part['mesh'],'point_world_cm':world.tolist()}
    return best

def build():
    assert not (OUT/'Build.json').exists(),'Use a new output version'
    frozen=read(UNIT/'PreservationHashes.json')
    assert not [r['path'] for r in frozen if base.sha(ROOT/r['path'])!=r['sha256']]
    previous=read(SOURCE/'Build.json');row=previous['outputs'][0]
    source_v,source_t=conn.binary(SOURCE/(row['name']+'.bin'))
    assert len(source_v)%7==0
    source_rows=source_v.reshape(-1,7,3);centers=source_rows[:,2]
    locals_=base.local(centers)
    arc=np.r_[0,np.cumsum(np.linalg.norm(np.diff(centers,axis=0),axis=1))]
    parts=conn.native_parts(previous['points_xyh_cm'])
    terrain=[conn.world_part(n,*conn.binary(PREVIOUS/f)) for n,f in [('NativeCrust','NativeCrust.bin'),
        ('PreservedFloor','NativePreservedFloor.bin'),('PreservedShell','NativePreservedShell.bin')]]
    bodies=[]
    for r in read(ROOT/'Saved/Profiling/Graphics/Phase01/U5aBody_V007/Build.json')['outputs']:
        bodies.append(conn.world_part(r['name'],*conn.binary(ROOT/'Saved/Profiling/Graphics/Phase01/U5aBody_V007'/(r['name']+'.bin'))))
    # 받침은 보이는 지각/구조 바위와 기존 몸체에만 닿게 한다. 충돌 전용 Floor/Shell은 앵커에서 제외한다.
    visible=parts+[terrain[0]]+bodies
    candidates=[]
    for i,p in enumerate(centers):
        if i%3 or arc[i]<450 or arc[-1]-arc[i]<450:continue
        hit=visible_hit(p,visible)
        if hit:candidates.append(dict(hit,row=i,arc_cm=float(arc[i])))
    supports=[]
    for candidate in sorted(candidates,key=lambda r:abs(r['distance_cm']-360)):
        if all(abs(candidate['arc_cm']-s['arc_cm'])>=850 for s in supports):supports.append(candidate)
        if len(supports)>=5:break
    supports.sort(key=lambda r:r['arc_cm'])
    verts=[];faces=[];mats=[];walk_faces=[];core_ids=[];width=11;depths=[]
    for i,(core,center,local,s) in enumerate(zip(source_rows,centers,locals_,arc)):
        x,y,h=local;left=base.spec.ridge.local(core[1]);right=base.spec.ridge.local(core[3])
        axis=(np.asarray(right[:2])-left[:2])/300
        toe=min(1,s/300,(arc[-1]-s)/300)
        depth=90*(1-toe)+(175+25*math.sin(s/155)+15*math.sin(s/71))*toe
        for support in supports:
            weight=math.exp(-((s-support['arc_cm'])/330)**2)
            depth=max(depth,90+(support['distance_cm']+25-90)*weight)
        depth=min(depth,670);depths.append(float(depth))
        wl=250+toe*(30+20*math.sin(s/93)+13*math.sin(s/211))
        wr=250+toe*(30+18*math.sin(s/107+1)+15*math.sin(s/199))
        profiles=[(-wl,-20-35*toe),(None,None),(None,None),(None,None),(wr,-20-35*toe),
            (wr+15*toe,-90-35*toe),(wr-20,-depth*.58),(110,-depth),(-110,-depth),(-wl+20,-depth*.62),(-wl-12*toe,-90-25*toe)]
        for j,(offset,delta_h) in enumerate(profiles):
            if j in (1,2,3):
                core_ids.append((len(verts),i*7+j));verts.append(core[j].copy());continue
            # 끝부분의 횡경사도 원래 보행면에 맞춘다.
            cross_h=float(np.interp(offset,[-150,0,150],[left[2]-h,0,right[2]-h]))
            verts.append(base.world(*(np.asarray([x,y])+axis*offset),h+cross_h+delta_h))
        if i:
            for j in range(width):
                a=(i-1)*width+j;b=(i-1)*width+(j+1)%width;c=i*width+(j+1)%width;d=i*width+j
                tris=[(a,b,c),(a,c,d)];faces.extend(tris);mats.extend([0 if j in (1,2) else 1]*2)
                if j in (1,2):walk_faces.extend(tris)
    for offset in (0,(len(source_rows)-1)*width):
        polygon=[Vector(verts[offset+j]) for j in range(width)]
        for tri in tessellate_polygon([polygon]):
            faces.append(tuple(offset+(int(v) if isinstance(v,int) else polygon.index(v)) for v in tri));mats.append(1)
    for obj in list(bpy.context.scene.objects):bpy.data.objects.remove(obj,do_unlink=True)
    OUT.mkdir(parents=True,exist_ok=True)
    obj=base.make('SM_Ridge_StairRamp3_Stone_'+VERSION,verts,faces)
    for name in ('RidgeWalk_Surface','RidgeStone_Sides'):obj.data.materials.append(bpy.data.materials.new(name))
    for poly,mat in zip(obj.data.polygons,mats):poly.material_index=mat
    base.normalize(obj)
    output=export(obj);v,t=base.arrays(obj);tree=base.tree(v,t)
    assert len(v)==len(verts),'Unexpected vertex merge'
    core_error=max(float(np.linalg.norm(v[a]-source_v[b])) for a,b in core_ids)
    assert core_error<.01
    self_pairs=[(a,b) for a,b in tree.overlap(tree) if a<b and not set(t[a]).intersection(t[b])]
    grades=[]
    for a,b,c in walk_faces:
        p,q,r=v[[a,b,c]];n=np.cross(q-p,r-p);n/=np.linalg.norm(n);up=-(p+q+r);up/=np.linalg.norm(up)
        grades.append(math.degrees(math.acos(np.clip(abs(np.dot(n,up)),0,1))))
    samples=[]
    for i in range(len(source_rows)-1):
        distance=np.linalg.norm(locals_[i+1,:2]-locals_[i,:2]);count=max(1,math.ceil(distance/10))
        for j in range(count):
            u=j/count
            for column,off in ((1,-150),(2,0),(3,150)):
                p=source_rows[i,column]*(1-u)+source_rows[i+1,column]*u;up=-p/np.linalg.norm(p)
                for height in (100,170,250):samples.append((len(samples)//9,off,height,p+up*height))
    for column,off in ((1,-150),(2,0),(3,150)):
        p=source_rows[-1,column];up=-p/np.linalg.norm(p)
        for height in (100,170,250):samples.append((len(samples)//9,off,height,p+up*height))
    clear=conn.clearance(samples,parts+terrain+bodies+[conn.world_part('NewStoneConnection',v,t)])
    config=read(base.AUTHOR/'BodyTrial.json');protected=[]
    for corridor in config['protected_spaces']:
        points=conn.prior.env['samples'](corridor);hits=[]
        for index,p in enumerate(points):
            p=np.asarray(p)
            if (p>=v.min(0)).all() and (p<=v.max(0)).all() and base.inside(tree,p):
                hits.append({'sample_index':index,'local_xyh_cm':base.spec.ridge.local(p)})
        protected.append({'id':corridor['id'],'samples':len(points),'hits':hits,'pass':not hits})
    previous_body=read(ROOT/'Saved/Profiling/Graphics/Phase01/U5aBody_V007/GeometryCheck.json')
    old={r['id']:r for r in previous_body['overlaps']};combined=[]
    for p in protected:
        combined.append({'id':p['id'],'samples':p['samples'],'hits':[dict(h,source='preserved_body') for h in old[p['id']]['hits']]+[dict(h,source='stone_connection') for h in p['hits']]})
    report={'unit':'U5a-4','version':VERSION,'topology_pass':output['topology']['nonmanifold_edges']==0,
        'self_intersection_candidate_pairs':len(self_pairs),'self_pairs_preview':self_pairs[:20],
        'walking_surface_vertex_count':len(core_ids),'walking_surface_max_error_cm':core_error,
        'walk_max_triangle_grade_deg':max(grades),'route_clearance':clear,
        'new_connection_protection':protected,'protected_spaces':51,'samples':16552,'overlaps':combined,
        'protected_pass':not any(p['hits'] for p in combined),
        'new_interferences_other50':[{'id':p['id'],'hits':len(p['hits'])} for p in protected if p['hits'] and p['id']!='StairRamp3CentralPassage'],
        'supports':supports,'minimum_depth_cm':min(depths),'maximum_depth_cm':max(depths),
        'seam_min_cm':float(v.min()),'accepted':False,'collision_or_PIE_pass':False}
    write('GeometryCheck.json',report)
    write('Build.json',{'unit':'U5a-4','version':VERSION,'outputs':[output],
        'source_build':(SOURCE/'Build.json').relative_to(ROOT).as_posix(),'source_build_sha256':base.sha(SOURCE/'Build.json'),
        'source_walk_vertices_preserved':len(core_ids),'core_error_cm':core_error,'supports':supports,
        'material_slots':{'RidgeWalk_Surface':'keep adopted top material','RidgeStone_Sides':'existing MI_Cliff_01, 2.5m projection'},
        'authoring_script_sha256':base.sha(Path(__file__)),'role':'visual NoCollision; walking surface fixed; no collision/bake/PIE'})
    (OUT/'BuildSource.py').write_bytes(Path(__file__).read_bytes())
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'StoneTrial.blend'))
    print('STONE_REFINEMENT '+json.dumps({'version':VERSION,'triangles':output['triangles'],'core_error_cm':core_error,
        'max_grade':max(grades),'self_pairs':len(self_pairs),'route_hits':len(clear['hits']),
        'new_protected_hits':[{'id':p['id'],'hits':len(p['hits'])} for p in protected if p['hits']],
        'supports':len(supports),'depth_cm':[min(depths),max(depths)]}),flush=True)

if __name__=='__main__':build()
