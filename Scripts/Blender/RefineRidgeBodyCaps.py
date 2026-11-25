"""U5a-5: 고정 몸체의 닫힌 캡과 하부 경계를 새 판에서만 다듬는다.

기존 생성기를 실행하지 않고 V007 이진 형상과 원래 외피 삼각형을 읽는다.
Blender --background --factory-startup --python <파일> -- survey|build V001
"""
import json
import math
from pathlib import Path
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(Path(__file__).parent))
import BuildRidgeClosedBodyTrial as base
import BuildRidgeConnectionTrial as conn

UNIT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_5_V001'
VERSION=sys.argv[sys.argv.index('--')+2]
OUT=UNIT/('Geometry_'+VERSION)
SOURCE=ROOT/'Saved/Profiling/Graphics/Phase01/U5aBody_V007'
read=base.read

def write(name,data):
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/name).write_text(json.dumps(data,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')

def inputs():
    hashes=read(UNIT/'PreservationHashes.json')
    assert not [r['path'] for r in hashes if base.sha(ROOT/r['path'])!=r['sha256']]
    _,raw,_,_,geometry=base.load_inputs()
    refs={}
    for index in (35430,116462,116490):
        row=raw['instances'][index]
        mesh=raw['components'][row[0]][3]
        name=raw['meshes'][mesh][0]
        v,t=base.transformed(row,name,geometry)
        refs[index]=base.tree(v,t)
    return hashes,refs

def cap_faces(v,t,ref):
    centers=v[t].mean(1)
    return np.asarray([ref.find_nearest(Vector(p))[3]>12 for p in centers])

def survey():
    hashes,refs=inputs();result=[]
    for row in read(SOURCE/'Build.json')['outputs']:
        if 'ScanBody' not in row['name']:continue
        index=int(row['name'].split('_')[-2]);v,t=conn.binary(SOURCE/(row['name']+'.bin'))
        mask=cap_faces(v,t,refs[index])
        area=np.linalg.norm(np.cross(v[t[:,1]]-v[t[:,0]],v[t[:,2]]-v[t[:,0]]),axis=1)/2
        result.append({'scan':index,'triangles':len(t),'cap_triangles':int(mask.sum()),
            'cap_area_m2':float(area[mask].sum()/10000),'largest_cap_triangle_m2':float(area[mask].max()/10000)})
    write('CapSurvey.json',{'preserved_files':len(hashes),'caps':result})
    print('CAP_SURVEY '+json.dumps(result),flush=True)

def refine_scan(row,ref):
    v,t=conn.binary(SOURCE/(row['name']+'.bin'))
    obj=base.make(row['name'].replace('ScanBody','FacetedBody').replace('V007',VERSION),v,t)
    before=base.topology(obj)
    mask=cap_faces(v,t,ref)
    area=np.linalg.norm(np.cross(v[t[:,1]]-v[t[:,0]],v[t[:,2]]-v[t[:,0]]),axis=1)/2
    # 작은 원본 외피 균열까지 장식 대상으로 넓히지 않는다. 넓은 닫힘 캡 두 개만 세분한다.
    if float(area[mask].sum())<10000:
        return obj,{'cap_area_m2':float(area[mask].sum()/10000),'changed_vertices':0,'method':'source geometry held; UV/material only'}
    # 기존 캡의 자기 교차를 세분해 확대하지 않고 새 파생본에서만 복셀 수리한다.
    bpy.context.view_layer.objects.active=obj
    modifier=obj.modifiers.new('RepairClosedCap','REMESH')
    modifier.mode='VOXEL';modifier.voxel_size=.25;modifier.adaptivity=0
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    bm=bmesh.new();bm.from_mesh(obj.data)
    bmesh.ops.triangulate(bm,faces=list(bm.faces))
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    bm.to_mesh(obj.data);bm.free();obj.data.update()
    bm=bmesh.new();bm.from_mesh(obj.data)
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.normal_update()
    changed=0;maximum=0;source_held=0
    # 원래 외피 근처에는 추가 굴곡을 주지 않고 캡 안쪽만 완만하게 후퇴시킨다.
    for vertex in bm.verts:
        point=base.ue(np.asarray(vertex.co))
        distance=float(ref.find_nearest(Vector(point))[3])
        if distance<=35:
            source_held+=1;continue
        weight=np.clip((distance-35)/200,0,1);weight=weight*weight*(3-2*weight)
        x,y,z=point
        modulation=.50+.22*math.sin((x+.61*y)/190)+.16*math.sin((z-.35*x)/95)+.10*math.sin((y+z)/310)
        depth=float(weight*(10+28*np.clip(modulation,0,1)))
        vertex.co-=vertex.normal*(depth/100)
        changed+=1;maximum=max(maximum,depth)
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    bm.to_mesh(obj.data);bm.free();obj.data.update()
    return obj,{'cap_area_m2':float(area[mask].sum()/10000),'before':before,
        'changed_vertices':changed,'maximum_inset_cm':maximum,'source_rim_held_vertices':source_held,
        'repair_voxel_cm':25,'source_rim_additional_relief_exclusion_cm':35,
        'method':'repair old self intersecting cap by25cm voxel; cap inward relief10..38cm; source rim no added relief'}

def refine_under(row):
    v,t=conn.binary(SOURCE/(row['name']+'.bin'));coords=base.local(v);changed=0;maximum=0
    a=np.asarray([-7463.,343.]);b=np.asarray([-7863.,1591.]);delta=b-a
    for i,(x,y,h) in enumerate(coords):
        ground=base.spec.massif_ramp_ground(x,y)-50
        u=np.clip(np.dot(np.asarray([x,y])-a,delta)/np.dot(delta,delta),0,1)
        distance=np.linalg.norm(np.asarray([x,y])-a-u*delta)
        if h<ground+10 or distance<=225:continue
        weight=np.clip((distance-225)/100,0,1)
        ripple=.5+.27*math.sin(x/72+y/131)+.20*math.sin(y/49-x/157)
        reduction=min((h-ground-10)*.45,weight*(12+38*ripple))
        v[i]=base.world(x,y,h-reduction);changed+=1;maximum=max(maximum,reduction)
    obj=base.make('SM_Ridge_U5a_UnderFacetedBody_'+VERSION,v,t)
    return obj,{'changed_vertices':changed,'maximum_height_reduction_cm':maximum,
        'core_radius_held_cm':225,'method':'lower outer top relief only; bottom and R03/R04 core fixed'}

def export(obj):
    mesh=obj.data
    # 복셀 수리의 거의 겹친 정점/극소 변을 반입 전에 합쳐 FBX 반올림의 퇴화 면을 막는다.
    bm=bmesh.new();bm.from_mesh(mesh)
    bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0005)
    bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0005)
    wire=[e for e in bm.edges if not e.link_faces]
    if wire:bmesh.ops.delete(bm,geom=wire,context='EDGES')
    isolated=[v for v in bm.verts if not v.link_faces]
    if isolated:bmesh.ops.delete(bm,geom=isolated,context='VERTS')
    bmesh.ops.triangulate(bm,faces=list(bm.faces))
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    bm.to_mesh(mesh);bm.free();mesh.update()
    material=bpy.data.materials.get('RidgeBody_Rock') or bpy.data.materials.new('RidgeBody_Rock')
    mesh.materials.append(material)
    uv=mesh.uv_layers.new(name='BodyProjection4m')
    for poly in mesh.polygons:
        axis=max(range(3),key=lambda i:abs(poly.normal[i]));axes=[i for i in range(3) if i!=axis]
        for loop in poly.loop_indices:
            p=mesh.vertices[mesh.loops[loop].vertex_index].co
            uv.data[loop].uv=(p[axes[0]]/4,p[axes[1]]/4)
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    path=OUT/(obj.name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},
        apply_scale_options='FBX_SCALE_NONE',global_scale=1,axis_forward='-Z',axis_up='Y',
        mesh_smooth_type='FACE',add_leaf_bones=False,bake_anim=False)
    v,t=base.arrays(obj)
    (OUT/(obj.name+'.bin')).write_bytes(np.asarray([len(v),len(t)],np.int32).tobytes()+v.astype(np.float32).tobytes()+t.astype(np.int32).tobytes())
    return {'name':obj.name,'file':path.relative_to(ROOT).as_posix(),'sha256':base.sha(path),
        'vertices':len(v),'triangles':len(t),'bounds_cm':[v.min(0).tolist(),v.max(0).tolist()],
        'topology':base.topology(obj),'material_slots':['RidgeBody_Rock'],'role':'visual NoCollision; cap refinement only'}

def build():
    assert not (OUT/'Build.json').exists(),'New geometry version only'
    hashes,refs=inputs()
    for obj in list(bpy.context.scene.objects):bpy.data.objects.remove(obj,do_unlink=True)
    OUT.mkdir(parents=True,exist_ok=True)
    objects=[];outputs=[];stats=[];parts=[]
    for row in read(SOURCE/'Build.json')['outputs']:
        if 'ScanBody' in row['name']:
            obj,stat=refine_scan(row,refs[int(row['name'].split('_')[-2])])
        else:obj,stat=refine_under(row)
        result=export(obj)
        assert result['topology']['nonmanifold_edges']==result['topology']['boundary_edges']==0,result['name']
        objects.append(obj);outputs.append(result);stats.append(dict(stat,name=obj.name))
        v,t=base.arrays(obj);part=conn.world_part(obj.name,v,t);parts.append(part)
        pairs=[(a,b) for a,b in part['tree'].overlap(part['tree']) if a<b and not set(t[a]).intersection(t[b])]
        stats[-1]['self_intersection_candidate_pairs']=len(pairs)
        print('CAP_BODY '+json.dumps({'name':obj.name,'triangles':len(t),'pairs':len(pairs),'changed':stat['changed_vertices']}),flush=True)
    config=read(base.AUTHOR/'BodyTrial.json');overlaps=[];probes={}
    for space in config['protected_spaces']:
        points=conn.prior.env['samples'](space);hits=[]
        for part in parts:
            lo,hi=part['bounds']
            for i,p in enumerate(points):
                p=np.asarray(p)
                if (p>=lo).all() and (p<=hi).all() and base.inside(part['tree'],p):hits.append({'body':part['owner'],'sample_index':i})
        overlaps.append({'id':space['id'],'samples':len(points),'hits':hits,'pass':not hits})
    for key,p in config['report_points_local_xyh_cm'].items():
        probes[key]={'point':p,'inside_bodies':[part['owner'] for part in parts if base.inside(part['tree'],base.world(*p))]}
    # 유지한 V005 연결의 실제 단면을 그대로 검사 입력으로 읽는다.
    oldgeo=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_3_V001/Geometry_V011'
    v,t=conn.binary(oldgeo/'SM_Ridge_StairRamp3_Connection_V011.bin')
    rows=v.reshape(-1,7,3)
    stations=[]
    for i,row in enumerate(rows):
        for offset,index in ((-150,1),(0,2),(150,3)):
            for up in (100,170,250):
                p=row[index];stations.append((i,offset,up,p-p/np.linalg.norm(p)*up))
    clearance=conn.clearance(stations,parts)
    volume_hits=[]
    for part in parts:
        lo,hi=part['bounds']
        for i,off,up,p in stations:
            if (p>=lo).all() and (p<=hi).all() and base.inside(part['tree'],p):volume_hits.append({'body':part['owner'],'station':i,'offset':off,'up':up})
    prior=read(ROOT/'Saved/Profiling/Graphics/Phase01/U5a_4_V001/Geometry_V001/GeometryCheck.json')
    by={r['id']:r for r in prior['new_connection_protection']}
    combined=[{'id':r['id'],'samples':r['samples'],'hits':r['hits']+[dict(h,body='kept_connection') for h in by[r['id']]['hits']]} for r in overlaps]
    other=[{'id':r['id'],'hits':len(r['hits'])} for r in combined if r['hits'] and r['id']!='StairRamp3CentralPassage']
    report={'unit':'U5a-5','version':VERSION,'topology_pass':True,'protected_spaces':51,'samples':16552,
        'protected_pass':not any(r['hits'] for r in combined),'overlaps':combined,
        'new_interferences_other50':other,'body_only_overlaps':overlaps,'probes':probes,
        'connection_clearance':clearance,'connection_body_volume_hits':volume_hits,
        'self_intersection_candidate_pairs':sum(s['self_intersection_candidate_pairs'] for s in stats),
        'seam_min_cm':float(min(p['bounds'][0].min() for p in parts)),
        'collision_or_PIE_pass':False,'accepted':False}
    write('GeometryCheck.json',report)
    write('Build.json',{'unit':'U5a-5','version':VERSION,'outputs':outputs,'refinements':stats,
        'source_geometry':(SOURCE/'Build.json').relative_to(ROOT).as_posix(),
        'source_geometry_sha256':base.sha(SOURCE/'Build.json'),'preserved_files':len(hashes),
        'authoring_script_sha256':base.sha(Path(__file__)),'connection':'V005 mesh/material/pose held',
        'collision':'visual NoCollision; no crust, collision, bake or PIE changes'})
    (OUT/'BuildSource.py').write_bytes(Path(__file__).read_bytes())
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'BodyCaps.blend'))
    print('CAP_REFINEMENT_READY '+json.dumps({'triangles':sum(r['triangles'] for r in outputs),
        'protected_hits':[{'id':r['id'],'hits':len(r['hits'])} for r in combined if r['hits']],
        'other50':other,'route_hits':len(clearance['hits']),'route_volume_hits':len(volume_hits),
        'probes':probes}),flush=True)

if __name__=='__main__':
    mode=sys.argv[sys.argv.index('--')+1]
    if mode=='survey':survey()
    elif mode=='build':build()
    else:raise ValueError(mode)
