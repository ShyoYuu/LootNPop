"""U5a-5 새 몸체의 실제 정점·재질과 기존 배치 보존을 읽기 전용으로 검사한다.

UE Cmd: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/RidgeBodyCapRefinement.py" verify V006
"""
import itertools
import math
from pathlib import Path
import struct
import sys

import unreal

HERE=Path(__file__).resolve().parent
sys.path[:0]=[str(HERE),str(HERE.parents[2]/'Scripts')]
import RidgeClosedBodyTrial as core
import AssembleRidgeOctant as assembly

def verify():
    core.CONFIG=HERE/'ClosedBodyTrial_V001/Generation_V006.json'
    config,_=core.inputs()
    before=set(core.OUT.glob('Verify_*.json'))
    core.verify()
    created=set(core.OUT.glob('Verify_*.json'))-before
    assert len(created)==1,'Retry after level instances finish loading'
    fullpath=created.pop();full=core.read(fullpath)
    assert full['preservation_pass'] and not full['dirty'],full['issues']
    rows=[];tolerance=.05
    geometry=core.ROOT/Path(config['build_report']).parent
    for row in config['new_outputs']:
        mesh=unreal.load_asset(row['asset']);assert mesh
        assert mesh.get_num_triangles(0)==row['triangles']
        assert core.layout.test.package(mesh.get_material(0))==row['material']
        binary=(geometry/(row['name']+'.bin')).read_bytes()
        count,_=struct.unpack_from('<ii',binary)
        source=list(struct.iter_unpack('<fff',binary[8:8+count*12]));lookup={}
        for p in source:
            key=tuple(math.floor(v/tolerance) for v in p)
            lookup.setdefault(key,[]).append(p)
        _,positions=assembly.native_mesh(mesh);errors=[]
        for p in positions:
            point=(p.x,p.y,p.z);key=tuple(math.floor(v/tolerance) for v in point)
            candidates=[q for delta in itertools.product((-1,0,1),repeat=3)
                        for q in lookup.get(tuple(a+b for a,b in zip(key,delta)),[])]
            assert candidates,('Missing exported vertex',row['name'],point)
            error=min(math.dist(point,q) for q in candidates)
            assert error<tolerance,(row['name'],error)
            errors.append(error)
        bounds=[[f(getattr(p,axis) for p in positions) for axis in ('x','y','z')] for f in (min,max)]
        bounds_error=max(abs(a-b) for actual,expected in zip(bounds,row['bounds_cm']) for a,b in zip(actual,expected))
        assert bounds_error<tolerance,(row['name'],bounds_error)
        rows.append({'mesh':row['asset'],'source_vertices':count,'native_vertices':len(positions),
            'maximum_vertex_error_cm':max(errors),'bounds_error_cm':bounds_error,
            'triangles':mesh.get_num_triangles(0),'material':row['material']})
    kept=config['kept_connection'];mesh=unreal.load_asset(kept['asset']);materials={}
    for name,expected in kept['material_assignments'].items():
        index=mesh.get_material_index(name);assert index>=0
        actual=core.layout.test.package(mesh.get_material(index));assert actual==expected
        materials[name]=actual
    assert not core.layout.test.dirty()
    core.evidence('CapVerify',{'preservation_pass':True,'level':config['target_level'],
        'full_verify':fullpath.relative_to(core.ROOT).as_posix(),'new_bodies':rows,
        'kept_connection':kept['asset'],'kept_connection_materials':materials,
        'dirty':[],'protected_pass':False,'collision_or_PIE_pass':False,'accepted':False,
        'status':'PRESERVATION_AND_IMPORTED_CAP_VERTICES_PASS; visual and self-intersection REVIEW'})
    unreal.log('RIDGE_CAP_VERIFY PRESERVATION_AND_IMPORTED_CAP_VERTICES_PASS')

if __name__=='__main__':
    assert sys.argv[1:]==['verify','V006'],'Use verify V006 only'
    verify()
