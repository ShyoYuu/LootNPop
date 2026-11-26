"""U5a-7 카메라 앞 공간을 보존하고 시선이 가리키는 Cliff09 내부만 국소 보강한다."""
import json
import math
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
import BuildRidgeR01LocalClosure as radial
import RefineRidgeR01ClosureHull as hull_tools

OUT=review.OUT/'Geometry_V006'


def main():
    assert not (OUT/'Build.json').exists(),'New geometry version only'
    review.frozen_inputs();config=review.read(review.AUTHOR/'BodyTrial.json')
    camera=review.read(review.OUT/'UserGapCamera.json');user=np.asarray([camera['location'][k] for k in ('x','y','z')])
    pitch=math.radians(camera['rotation']['pitch']);yaw=math.radians(camera['rotation']['yaw'])
    forward=np.asarray([math.cos(pitch)*math.cos(yaw),math.cos(pitch)*math.sin(yaw),math.sin(pitch)])
    bodies=review.native_parts();ed=conn.native_parts([base.local(user.reshape(1,3))[0].tolist()])
    source=next(p for p in ed if p['mesh'].endswith('/SM_Cliff_09') and np.linalg.norm(p['origin']-np.asarray([21737.11,15022.82,13082.1]))<.1)
    nv,nt=source['geometry'];world=(nv*source['scale'])@source['rot'].T+source['origin']
    hull=hull_tools.make_hull(world)
    hull_hits=review.ray_hits(hull['tree'],user,forward,4000)
    assert len(hull_hits)>=2 and not base.inside(hull['tree'],user)
    entry=hull_hits[0]['distance_cm'];exit_distance=hull_hits[1]['distance_cm']
    surface=radial.native_ray(source,user,forward,4000);assert surface is not None
    target_distance=(entry+min(surface,exit_distance))/2
    center=user+forward*target_distance;assert base.inside(hull['tree'],center)
    up=-center/np.linalg.norm(center);side=np.cross(up,forward);side/=np.linalg.norm(side)
    axis=np.cross(side,up);axis/=np.linalg.norm(axis);basis=np.column_stack((side,axis,up));radii=np.asarray([230.,200.,200.])
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3,radius=1)
    ico=bpy.context.object;directions=np.asarray([v.co[:] for v in ico.data.vertices]);t=np.asarray([p.vertices[:] for p in ico.data.polygons],int)
    bpy.data.objects.remove(ico,do_unlink=True)
    if np.linalg.det(basis)<0:t=t[:,::-1]
    vertices=[]
    for direction in directions:
        vector=basis@(direction*radii);maximum=float(np.linalg.norm(vector));ray=vector/maximum
        limit=hull['tree'].ray_cast(Vector(center),Vector(ray),4000)[3]
        assert limit is not None
        distance=min(maximum,limit-5)
        skin=radial.native_ray(source,center,ray,distance+30)
        if skin is not None:distance=min(distance,skin-5)
        distance=max(1.,distance);vertices.append(center+ray*distance)
    obj=base.make('SM_Ridge_U5a_R01RockBackfill_V006',np.asarray(vertices),t);v,t=base.arrays(obj)
    topology=base.topology(obj);pairs=candidate.crossings(v,t);part=conn.world_part(obj.name,v,t)
    outside=[i for i,p in enumerate(v) if not base.inside(hull['tree'],p)]
    report={'unit':'U5a-7','method':'local closed backfill inside adjacent original Cliff09 hull and5cm behind source skin; natural air gap and accepted4 bodies retained',
        'source_owner':source['owner'],'source_mesh':source['mesh'],'source_origin_world_cm':source['origin'].tolist(),
        'user_camera_world_cm':user.tolist(),'user_camera_inside_source_hull':False,
        'view_hull_entry_cm':entry,'view_first_ED_surface_cm':surface,'target_along_view_cm':target_distance,
        'target_world_cm':center.tolist(),'target_inside':base.inside(part['tree'],center),
        'user_camera_inside_new_body':base.inside(part['tree'],user),'outside_source_hull_vertices':outside,
        'self_intersection_pairs':len(pairs),'vertices':len(v),'triangles':len(t),'topology':topology,
        'bounds_world_cm':[v.min(0).tolist(),v.max(0).tolist()],'collision_or_PIE_pass':False,
        'previous_drafts':'V001 whole asset closure failed;V003/V004 filled outside air gap and were rejected/preserved'}
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'CandidateCheck.json').write_text(json.dumps(report,indent=1)+'\n',encoding='utf-8')
    print('ROCK_BACKFILL '+json.dumps({k:report[k] for k in ['view_hull_entry_cm','view_first_ED_surface_cm','target_along_view_cm','target_world_cm','target_inside','user_camera_inside_new_body','self_intersection_pairs','outside_source_hull_vertices','topology']}),flush=True)
    assert not pairs and not outside and report['target_inside'] and not report['user_camera_inside_new_body']
    assert topology['boundary_edges']==topology['nonmanifold_edges']==0 and topology['signed_volume_m3']>0
    regression=review.regression(bodies+[part],conn.world_part('kept_connection',*conn.binary((review.ROOT/review.read(review.AUTHOR/'Generation_V007.json')['kept_connection']['file']).with_suffix('.bin'))),config)
    material=bpy.data.materials.new('RidgeBody_Rock');obj.data.materials.append(material);uv=obj.data.uv_layers.new(name='BodyProjection4m')
    for poly in obj.data.polygons:
        a=max(range(3),key=lambda i:abs(poly.normal[i]));axes=[i for i in range(3) if i!=a]
        for loop in poly.loop_indices:
            p=obj.data.vertices[obj.data.loops[loop].vertex_index].co;uv.data[loop].uv=(p[axes[0]]/4,p[axes[1]]/4)
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    path=OUT/(obj.name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},apply_scale_options='FBX_SCALE_NONE',global_scale=1,axis_forward='-Z',axis_up='Y',mesh_smooth_type='FACE',add_leaf_bones=False,bake_anim=False)
    (OUT/(obj.name+'.bin')).write_bytes(np.asarray([len(v),len(t)],np.int32).tobytes()+v.astype(np.float32).tobytes()+t.astype(np.int32).tobytes())
    output={'name':obj.name,'file':path.relative_to(review.ROOT).as_posix(),'sha256':base.sha(path),'vertices':len(v),'triangles':len(t),'bounds_cm':report['bounds_world_cm'],
        'topology':topology,'material_slots':['RidgeBody_Rock'],'role':'visual Static identity NoCollision; local backfill for R01 adjacentCliff09'}
    (OUT/'GeometryCheck.json').write_text(json.dumps(dict(report,topology_pass=True,protected_pass=False,regression=regression,overlaps=regression['overlaps'],new_interferences_other50=[]),indent=1)+'\n',encoding='utf-8')
    (OUT/'Build.json').write_text(json.dumps({'unit':'U5a-7','version':'V006','outputs':[output],'source_preserved_files':534,'method':report['method'],'authoring_script_sha256':base.sha(Path(__file__))},indent=1)+'\n',encoding='utf-8')
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'R01RockBackfill.blend'))
    (OUT/'BuildSource.py').write_bytes(Path(__file__).read_bytes())


if __name__=='__main__':main()
