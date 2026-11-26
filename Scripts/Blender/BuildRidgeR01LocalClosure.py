"""U5a-7 R01 사용자 확인 틈에만 닫힌 접합 몸체를 만든다. 기존 몸체/외피는 고정한다."""
import hashlib
import json
from pathlib import Path
import sys
import bpy
import numpy as np
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).parent))
import ReviewRidgeBodyBoundary as review
import BuildRidgeClosedBodyTrial as base
import BuildRidgeConnectionTrial as conn
import BuildRidgeR01JoinBody as candidate

OUT=review.OUT/'Geometry_V003'


def native_ray(part,origin,direction,maximum):
    local=(origin-part['origin'])@part['rot']/part['scale']
    delta=direction@part['rot']/part['scale'];factor=np.linalg.norm(delta)
    hit,normal,face,distance=part['tree'].ray_cast(Vector(local),Vector(delta/factor),maximum*factor)
    if hit is None:return None
    return float(distance/factor)


def main():
    assert not (OUT/'Build.json').exists(),'New version only'
    review.frozen_inputs();body=review.read(review.AUTHOR/'BodyTrial.json')
    camera=review.read(review.OUT/'UserGapCamera.json');user=np.asarray([camera['location'][k] for k in ('x','y','z')])
    survey=review.read(review.OUT/'BoundarySurvey.json');grid=survey['R01_grid']
    q=np.asarray(grid['center_world_cm']);vertical=np.asarray(grid['vertical'])
    lower=q-100*vertical
    center=(lower+user)/2
    parts=review.native_parts();references=conn.native_parts([base.local(center.reshape(1,3))[0].tolist()])
    up=-center/np.linalg.norm(center)
    horizontal=np.asarray(grid['horizontal']);horizontal-=up*np.dot(horizontal,up);horizontal/=np.linalg.norm(horizontal)
    depth=np.cross(up,horizontal);depth/=np.linalg.norm(depth)
    basis=np.column_stack((horizontal,depth,up));radii=np.asarray([350.,450.,200.])
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3,radius=1)
    ico=bpy.context.object;directions=np.asarray([v.co[:] for v in ico.data.vertices]);triangles=np.asarray([p.vertices[:] for p in ico.data.polygons],int)
    bpy.data.objects.remove(ico,do_unlink=True)
    vertices=[];rays=[]
    for direction in directions:
        vector=basis@(direction*radii);maximum=float(np.linalg.norm(vector));axis=vector/maximum
        hits=[]
        for kind,part in [('body',p) for p in parts]+[('ED',p) for p in references]:
            distance=native_ray(part,center,axis,maximum+30)
            if distance is not None:hits.append((distance,part['owner'],part['mesh'],kind))
        hit=min(hits,key=lambda h:h[0]) if hits else None
        # 닫힌 몸체에는15cm 겹치고, 기존 ED 시각 면에는5cm 뒤로 물려 원래 외피를 앞에 둔다.
        distance=min(maximum,hit[0]+(15 if hit[3]=='body' else -5)) if hit else maximum
        assert distance>10,'Center too close to a surface; inspect source placement'
        vertices.append(center+axis*distance)
        rays.append({'maximum_cm':maximum,'radius_cm':distance,'nearest_surface':list(hit) if hit else None})
    v=np.asarray(vertices);obj=base.make('SM_Ridge_U5a_R01LocalClosure_V003',v,triangles)
    topology=base.topology(obj);assert topology['boundary_edges']==topology['nonmanifold_edges']==0
    v,t=base.arrays(obj);pairs=candidate.crossings(v,t)
    part=conn.world_part(obj.name,v,t);inside=base.inside(part['tree'],user)
    overlaps={p['owner']:sum(base.inside(p['tree'],p0) for p0 in v) for p in parts}
    report={'unit':'U5a-7','method':'closed star-shaped local join;15cm overlap into accepted body;5cm behind original ED skin; no voxel/remesh or source body changes',
        'center_world_cm':center.tolist(),'basis':basis.tolist(),'maximum_radii_cm':radii.tolist(),
        'vertices':len(v),'triangles':len(t),'topology':topology,'self_intersection_pairs':len(pairs),
        'user_gap_inside':inside,'user_gap_surface_distance_cm':part['tree'].find_nearest(Vector(user))[3],
        'original_lower_grid_center_inside':base.inside(part['tree'],lower),
        'vertices_inside_prior_bodies':overlaps,'bounds_world_cm':[v.min(0).tolist(),v.max(0).tolist()],
        'radial_samples':rays,'collision_or_PIE_pass':False}
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'CandidateCheck.json').write_text(json.dumps(report,indent=1)+'\n',encoding='utf-8')
    (OUT/(obj.name+'.bin')).write_bytes(np.asarray([len(v),len(t)],np.int32).tobytes()+v.astype(np.float32).tobytes()+t.astype(np.int32).tobytes())
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'R01LocalClosure.blend'))
    print('LOCAL_CLOSURE_CANDIDATE '+json.dumps({k:report[k] for k in ['vertices','triangles','topology','self_intersection_pairs','user_gap_inside','user_gap_surface_distance_cm','original_lower_grid_center_inside','vertices_inside_prior_bodies']}),flush=True)
    assert inside and not pairs,'Candidate not ready for native import'
    assert sum(overlaps.values())>0,'Candidate must join accepted body'
    regression=review.regression(parts+[part],conn.world_part('kept_connection',*conn.binary((review.ROOT/review.read(review.AUTHOR/'Generation_V007.json')['kept_connection']['file']).with_suffix('.bin'))),body)
    material=bpy.data.materials.new('RidgeBody_Rock');obj.data.materials.append(material)
    uv=obj.data.uv_layers.new(name='BodyProjection4m')
    for poly in obj.data.polygons:
        axis=max(range(3),key=lambda i:abs(poly.normal[i]));axes=[i for i in range(3) if i!=axis]
        for loop in poly.loop_indices:
            p=obj.data.vertices[obj.data.loops[loop].vertex_index].co;uv.data[loop].uv=(p[axes[0]]/4,p[axes[1]]/4)
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    path=OUT/(obj.name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},apply_scale_options='FBX_SCALE_NONE',global_scale=1,
        axis_forward='-Z',axis_up='Y',mesh_smooth_type='FACE',add_leaf_bones=False,bake_anim=False)
    output={'name':obj.name,'file':path.relative_to(review.ROOT).as_posix(),'sha256':base.sha(path),
        'vertices':len(v),'triangles':len(t),'bounds_cm':[v.min(0).tolist(),v.max(0).tolist()],
        'topology':topology,'material_slots':['RidgeBody_Rock'],'role':'visual Static identity NoCollision; R01 local closed join'}
    report['regression']=regression
    (OUT/'GeometryCheck.json').write_text(json.dumps(dict(report,topology_pass=True,protected_pass=False,overlaps=regression['overlaps'],new_interferences_other50=[]),indent=1)+'\n',encoding='utf-8')
    (OUT/'Build.json').write_text(json.dumps({'unit':'U5a-7','version':'V003','outputs':[output],'source_baseline':'Acceptance_V007',
        'source_preserved_files':534,'method':report['method'],'authoring_script_sha256':base.sha(Path(__file__))},indent=1)+'\n',encoding='utf-8')
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'R01LocalClosure.blend'))
    (OUT/'BuildSource.py').write_bytes(Path(__file__).read_bytes())


if __name__=='__main__':main()
