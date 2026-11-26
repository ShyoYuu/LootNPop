"""U5a-7 사용자 카메라로 확인한 R01 인접 암석의 닫힌 보조 몸체 후보를 만든다.

기존 V007/원본 ED를 수정하지 않으며 복셀 재구성은 사용하지 않는다.
"""
import json
from pathlib import Path
import sys
import bmesh
import bpy
import numpy as np
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).parent))
import ReviewRidgeBodyBoundary as review
import BuildRidgeClosedBodyTrial as base
import BuildRidgeConnectionTrial as conn
import ReviewRidgeBodyIntersections as intersections

OUT=review.OUT/'Geometry_V001'


def crossings(v,t):
    center=v.mean(0);tree=base.tree(v-center,t)
    raw=[(a,b) for a,b in tree.overlap(tree) if a<b]
    return [(a,b) for a,b in raw if intersections.classify(v[t[a]],v[t[b]])['category'] in ('proper_crossing','coplanar_overlap')]


def close_source(obj):
    base.normalize(obj)
    bm=bmesh.new();bm.from_mesh(obj.data)
    repaired=0
    for _ in range(8):
        bad=[e for e in bm.edges if len(e.link_faces)>2]
        if not bad:break
        remove=set()
        for edge in bad:remove.update(sorted(edge.link_faces,key=lambda f:f.calc_area())[:len(edge.link_faces)-2])
        repaired+=len(remove);bmesh.ops.delete(bm,geom=list(remove),context='FACES')
    cap_count=0
    for _ in range(3):
        faces=bmesh.ops.holes_fill(bm,edges=[e for e in bm.edges if e.is_boundary],sides=0)['faces']
        cap_count+=len(faces);bmesh.ops.triangulate(bm,faces=faces)
        if not faces:break
    wire=[e for e in bm.edges if not e.link_faces]
    if wire:bmesh.ops.delete(bm,geom=wire,context='EDGES')
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free();obj.data.update()
    topology=base.topology(obj)
    if topology['signed_volume_m3']<0:
        bm=bmesh.new();bm.from_mesh(obj.data);bmesh.ops.reverse_faces(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
    assert topology['boundary_edges']==topology['nonmanifold_edges']==0,topology
    base.normalize(obj,5)
    return {'removed_source_branch_faces':repaired,'new_cap_faces':cap_count,'source_inset_cm':5,'voxel_rebuild':False}


def main():
    assert not (OUT/'Build.json').exists(),'New version only'
    review.frozen_inputs()
    camera=review.read(review.OUT/'UserGapCamera.json')
    point=np.asarray([camera['location'][k] for k in ('x','y','z')])
    local=base.local(point.reshape(1,3))[0]
    candidates=conn.native_parts([local.tolist()])
    part=next(p for p in candidates if p['mesh'].endswith('/SM_Cliff_09') and np.linalg.norm(p['origin']-np.asarray([21737.11,15022.82,13082.1]))<.1)
    source_v,source_t=part['geometry'];v=(source_v*part['scale'])@part['rot'].T+part['origin']
    name='SM_Ridge_U5a_R01JoinBody_V001'
    obj=base.make(name,v,source_t);before=base.topology(obj);method=close_source(obj)
    v,t=base.arrays(obj);pairs=crossings(v,t);initial=len(pairs)
    OUT.mkdir(parents=True,exist_ok=True)
    report={'unit':'U5a-7','source_mesh':part['mesh'],'source_owner':part['owner'],'source_origin_world_cm':part['origin'].tolist(),
        'source_topology':before,'closed_topology':base.topology(obj),'method':method,
        'self_intersection_pairs':len(pairs),'user_gap_inside':base.inside(base.tree(v,t),point),
        'user_gap_surface_distance_cm':base.tree(v,t).find_nearest(Vector(point))[3],
        'vertices':len(v),'triangles':len(t),'bounds_world_cm':[v.min(0).tolist(),v.max(0).tolist()],
        'source_vertices_moved_by_local_repair':0}
    (OUT/'CandidateSurvey.json').write_text(json.dumps(report,indent=1)+'\n',encoding='utf-8')
    (OUT/(name+'.bin')).write_bytes(np.asarray([len(v),len(t)],np.int32).tobytes()+v.astype(np.float32).tobytes()+t.astype(np.int32).tobytes())
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'R01JoinCandidate.blend'))
    print('R01_JOIN_CANDIDATE '+json.dumps(report),flush=True)


if __name__=='__main__':main()
