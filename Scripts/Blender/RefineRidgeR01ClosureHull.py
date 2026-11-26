"""U5a-7 접합 몸체를 기존 두 암석의 외곽 안쪽에 제한한다. V003 초안은 보존한다."""
from pathlib import Path
import sys
import json
import math
import bpy
import bmesh
import numpy as np
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).parent))
import ReviewRidgeBodyBoundary as review
import BuildRidgeClosedBodyTrial as base
import BuildRidgeConnectionTrial as conn
import BuildRidgeR01JoinBody as candidate
import BuildRidgeR01LocalClosure as prior

OUT=review.OUT/'Geometry_V004'


def make_hull(vertices):
    mesh=bpy.data.meshes.new('SourceVisualHull');bm=bmesh.new()
    for p in base.bl(vertices):bm.verts.new(p)
    bmesh.ops.convex_hull(bm,input=list(bm.verts),use_existing_faces=False)
    unused=[v for v in bm.verts if not v.link_faces]
    if unused:bmesh.ops.delete(bm,geom=unused,context='VERTS')
    bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    bm.to_mesh(mesh);bm.free();obj=bpy.data.objects.new('SourceVisualHull',mesh);bpy.context.scene.collection.objects.link(obj)
    v,t=base.arrays(obj);return conn.world_part('SourceVisualHull',v,t)


def main():
    assert not (OUT/'Build.json').exists(),'New geometry version only'
    review.frozen_inputs();config=review.read(review.AUTHOR/'BodyTrial.json')
    camera=review.read(review.OUT/'UserGapCamera.json');user=np.asarray([camera['location'][k] for k in ('x','y','z')])
    source=review.read(review.OUT/'BoundarySurvey.json');frame=source['R01_grid']
    lower=np.asarray(frame['center_world_cm'])-100*np.asarray(frame['vertical'])
    bodies=review.native_parts();ed=conn.native_parts([base.local(user.reshape(1,3))[0].tolist()])
    neighbor=next(p for p in ed if p['mesh'].endswith('/SM_Cliff_09') and np.linalg.norm(p['origin']-np.asarray([21737.11,15022.82,13082.1]))<.1)
    nv,nt=neighbor['geometry'];nv=(nv*neighbor['scale'])@neighbor['rot'].T+neighbor['origin']
    hull=make_hull(np.concatenate([bodies[0]['geometry'][0],nv]));centroid=hull['geometry'][0].mean(0)
    inside_camera=base.inside(hull['tree'],user)
    target=user.copy()
    if not inside_camera:
        pitch=math.radians(camera['rotation']['pitch']);yaw=math.radians(camera['rotation']['yaw'])
        forward=np.asarray([math.cos(pitch)*math.cos(yaw),math.cos(pitch)*math.sin(yaw),math.sin(pitch)])
        hit=hull['tree'].ray_cast(Vector(user),Vector(forward),3000)[0]
        assert hit is not None,'User view does not point into source envelope'
        target=np.asarray(hit)+forward*80
        assert base.inside(hull['tree'],target)
    if not base.inside(hull['tree'],lower):
        hit=hull['tree'].find_nearest(Vector(lower))[0];direction=centroid-np.asarray(hit);direction/=np.linalg.norm(direction)
        lower=np.asarray(hit)+direction*25
    center=(target+lower)/2
    assert base.inside(hull['tree'],center)
    up=-center/np.linalg.norm(center);horizontal=np.asarray(frame['horizontal'])
    horizontal-=up*np.dot(horizontal,up);horizontal/=np.linalg.norm(horizontal)
    depth=np.cross(up,horizontal);basis=np.column_stack((horizontal,depth,up));radii=np.asarray([350.,450.,200.])
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3,radius=1)
    ico=bpy.context.object;directions=np.asarray([v.co[:] for v in ico.data.vertices]);t=np.asarray([p.vertices[:] for p in ico.data.polygons],int)
    bpy.data.objects.remove(ico,do_unlink=True)
    vertices=[];rays=[]
    for direction in directions:
        vector=basis@(direction*radii);ellipsoid=float(np.linalg.norm(vector));axis=vector/ellipsoid
        boundary=hull['tree'].ray_cast(Vector(center),Vector(axis),5000)[3]
        assert boundary is not None
        maximum=min(ellipsoid,boundary-5);assert maximum>1
        body_hits=[d for p in bodies if (d:=prior.native_ray(p,center,axis,maximum+15)) is not None]
        ed_hits=[d for p in ed if (d:=prior.native_ray(p,center,axis,maximum+30)) is not None]
        b=min(body_hits,default=None);e=min(ed_hits,default=None)
        if b is not None and (e is None or b<=e+40):distance=min(maximum,b+15);kind='body overlap'
        elif e is not None:distance=min(maximum,e-5);kind='ED inset'
        else:distance=maximum;kind='source hull cap'
        distance=max(1.,distance);vertices.append(center+axis*distance)
        rays.append({'radius_cm':distance,'maximum_cm':maximum,'kind':kind})
    obj=base.make('SM_Ridge_U5a_R01LocalClosure_V004',np.asarray(vertices),t);v,t=base.arrays(obj)
    topology=base.topology(obj);pairs=candidate.crossings(v,t);part=conn.world_part(obj.name,v,t)
    outside=[i for i,p in enumerate(v) if not base.inside(hull['tree'],p)]
    inside_target=base.inside(part['tree'],target)
    overlap=sum(base.inside(bodies[0]['tree'],p) for p in v)
    OUT.mkdir(parents=True,exist_ok=True)
    report={'unit':'U5a-7','method':'local closed join bounded5cm inside convex envelope of accepted35430 body and original adjacentCliff09;15cm overlap into old body and5cm behind ED skin',
        'user_camera_world_cm':user.tolist(),'user_camera_inside_source_hull':inside_camera,'target_world_cm':target.tolist(),
        'target_policy':'camera point if inside original envelope; otherwise point80cm beyond first source envelope hit along user view',
        'user_camera_inside_new_body':base.inside(part['tree'],user),'target_inside':inside_target,
        'center_world_cm':center.tolist(),'vertices':len(v),'triangles':len(t),'topology':topology,'self_intersection_pairs':len(pairs),
        'outside_source_hull_vertices':outside,'overlap_vertices_with_accepted_body':overlap,'rays':rays,
        'bounds_world_cm':[v.min(0).tolist(),v.max(0).tolist()],'collision_or_PIE_pass':False}
    (OUT/'CandidateCheck.json').write_text(json.dumps(report,indent=1)+'\n',encoding='utf-8')
    print('HULL_CLOSURE '+json.dumps({k:report[k] for k in ['user_camera_inside_source_hull','target_world_cm','user_camera_inside_new_body','target_inside','self_intersection_pairs','outside_source_hull_vertices','overlap_vertices_with_accepted_body','topology']}),flush=True)
    assert not pairs and not outside and inside_target and overlap
    assert topology['boundary_edges']==topology['nonmanifold_edges']==0
    regression=review.regression(bodies+[part],conn.world_part('kept_connection',*conn.binary((review.ROOT/review.read(review.AUTHOR/'Generation_V007.json')['kept_connection']['file']).with_suffix('.bin'))),config)
    material=bpy.data.materials.new('RidgeBody_Rock');obj.data.materials.append(material);uv=obj.data.uv_layers.new(name='BodyProjection4m')
    for poly in obj.data.polygons:
        axis=max(range(3),key=lambda i:abs(poly.normal[i]));axes=[i for i in range(3) if i!=axis]
        for loop in poly.loop_indices:
            p=obj.data.vertices[obj.data.loops[loop].vertex_index].co;uv.data[loop].uv=(p[axes[0]]/4,p[axes[1]]/4)
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    path=OUT/(obj.name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},apply_scale_options='FBX_SCALE_NONE',global_scale=1,
        axis_forward='-Z',axis_up='Y',mesh_smooth_type='FACE',add_leaf_bones=False,bake_anim=False)
    (OUT/(obj.name+'.bin')).write_bytes(np.asarray([len(v),len(t)],np.int32).tobytes()+v.astype(np.float32).tobytes()+t.astype(np.int32).tobytes())
    output={'name':obj.name,'file':path.relative_to(review.ROOT).as_posix(),'sha256':base.sha(path),'vertices':len(v),'triangles':len(t),
        'bounds_cm':[v.min(0).tolist(),v.max(0).tolist()],'topology':topology,'material_slots':['RidgeBody_Rock'],'role':'visual Static identity NoCollision; R01 bounded local join'}
    (OUT/'GeometryCheck.json').write_text(json.dumps(dict(report,topology_pass=True,protected_pass=False,regression=regression,overlaps=regression['overlaps'],new_interferences_other50=[]),indent=1)+'\n',encoding='utf-8')
    (OUT/'Build.json').write_text(json.dumps({'unit':'U5a-7','version':'V004','outputs':[output],'source_preserved_files':534,'method':report['method'],'authoring_script_sha256':base.sha(Path(__file__))},indent=1)+'\n',encoding='utf-8')
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'R01LocalClosure.blend'))
    (OUT/'BuildSource.py').write_bytes(Path(__file__).read_bytes())


if __name__=='__main__':main()
