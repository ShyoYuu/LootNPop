"""U5a-6 기존 저작 UV/면 연결을 유지해 국소 정점 보정3메시만 새 파일로 출력한다."""
from pathlib import Path
import sys
import json
import hashlib
import bpy
import numpy as np
sys.path.insert(0,str(Path(__file__).parent))
import ReviewRidgeBodyIntersections as review
import BuildRidgeClosedBodyTrial as base

OUT=review.OUT/'Geometry_V001'


def uv_hash(mesh):
    return hashlib.sha256(b''.join(np.asarray([v.uv[:] for v in layer.data],np.float32).tobytes() for layer in mesh.uv_layers)).hexdigest()


def main():
    assert not (OUT/'Build.json').exists(),'New output only'
    assert not (review.ROOT/'Scripts/Profiling/Graphics/ClosedBodyTrial_V001/Generation_V007.json').exists()
    review.preservation()
    old=review.read(review.SOURCE/'Geometry_V005/Build.json')
    blend=review.SOURCE/'Geometry_V005/BodyCaps.blend'
    OUT.mkdir(parents=True,exist_ok=True)
    for obj in list(bpy.data.objects):bpy.data.objects.remove(obj,do_unlink=True)
    names=[r['name'] for r in old['outputs'][:3]]
    with bpy.data.libraries.load(str(blend),link=False) as (source,target):
        assert all(n in source.objects for n in names)
        target.objects=names
    outputs=[]
    for obj,row in zip(target.objects,old['outputs'][:3]):
        bpy.context.scene.collection.objects.link(obj)
        source_v,source_t=review.binary(review.SOURCE/'Geometry_V005'/(row['name']+'.bin'))
        before_v,before_t=base.arrays(obj)
        assert np.array_equal(before_t,source_t)
        assert np.max(abs(before_v-source_v))<.003
        uv_before=uv_hash(obj.data)
        proposal=np.load(review.OUT/(row['name']+'_ConstrainedProposal.npz'))['vertices'].astype(np.float32).astype(float)
        moved=np.where(np.linalg.norm(proposal-source_v,axis=1)>1e-8)[0]
        for i in moved:obj.data.vertices[int(i)].co=base.bl(proposal[i]).tolist()
        obj.data.update()
        assert uv_hash(obj.data)==uv_before
        name=row['name'].replace('_V005','_Repair_V001');obj.name=name;obj.data.name=name
        bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
        path=OUT/(name+'.fbx')
        bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},
            apply_scale_options='FBX_SCALE_NONE',global_scale=1,axis_forward='-Z',axis_up='Y',
            mesh_smooth_type='FACE',add_leaf_bones=False,bake_anim=False)
        v,t=base.arrays(obj)
        assert np.array_equal(t,source_t)
        (OUT/(name+'.bin')).write_bytes(np.asarray([len(v),len(t)],np.int32).tobytes()+v.astype(np.float32).tobytes()+t.astype(np.int32).tobytes())
        topology=base.topology(obj)
        assert topology['boundary_edges']==topology['nonmanifold_edges']==0
        outputs.append({'name':name,'source_name':row['name'],'file':path.relative_to(review.ROOT).as_posix(),
            'sha256':base.sha(path),'vertices':len(v),'triangles':len(t),'bounds_cm':[v.min(0).tolist(),v.max(0).tolist()],
            'topology':topology,'moved_vertices':len(moved),'uv_sha256_before':uv_before,'uv_sha256_after':uv_hash(obj.data),
            'material_slots':row['material_slots'],'role':'visual NoCollision; sparse local intersection repair'})
        print('REPAIR_EXPORT '+json.dumps({'name':name,'moved_vertices':len(moved),'uv_held':True}),flush=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'IntersectionRepair.blend'))
    (OUT/'Build.json').write_text(json.dumps({'unit':'U5a-6','version':'V001','outputs':outputs,
        'source_blend':blend.relative_to(review.ROOT).as_posix(),'source_blend_sha256':base.sha(blend),
        'source_build':(review.SOURCE/'Geometry_V005/Build.json').relative_to(review.ROOT).as_posix(),
        'method':'modify sparse vertices only; original UV loops and triangle indices held; underbody and connection held',
        'authoring_script_sha256':base.sha(Path(__file__))},indent=1)+'\n',encoding='utf-8')
    (OUT/'BuildSource.py').write_bytes(Path(__file__).read_bytes())


if __name__=='__main__':main()
