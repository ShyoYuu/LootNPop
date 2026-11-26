"""U5a-6 V007 실제 전체 보존·73,111정점/삼각형·연결858정점을 읽기 전용 검사한다.

UE Cmd: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/VerifyRidgeBodyIntersectionRepair.py"
배치/반입/저장/베이크/PIE는 실행하지 않는다. 실제 형상을 Saved에만 덤프한다.
"""
import datetime
import itertools
import json
import math
from pathlib import Path
import struct
import sys
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
HERE=ROOT/'Scripts/Profiling/Graphics'
sys.path[:0]=[str(HERE),str(ROOT/'Scripts')]
import RidgeClosedBodyTrial as core
import AssembleRidgeOctant as assembly


def points_from_binary(path):
    data=path.read_bytes();count,_=struct.unpack_from('<ii',data)
    return list(struct.iter_unpack('<fff',data[8:8+count*12]))


def match_points(source,actual,tolerance=.05):
    lookup={}
    for q in source:
        lookup.setdefault(tuple(math.floor(v/tolerance) for v in q),[]).append(q)
    errors=[]
    for p in actual:
        key=tuple(math.floor(v/tolerance) for v in p)
        candidates=[q for delta in itertools.product((-1,0,1),repeat=3)
                    for q in lookup.get(tuple(a+b for a,b in zip(key,delta)),[])]
        assert candidates,('Missing native vertex',p)
        error=min(math.dist(p,q) for q in candidates)
        assert error<tolerance,('Native vertex mismatch',error)
        errors.append(error)
    return max(errors,default=0)


def main():
    core.CONFIG=HERE/'ClosedBodyTrial_V001/Generation_V007.json'
    config,_=core.inputs()
    before=set(core.OUT.glob('Verify_*.json'));core.verify()
    created=set(core.OUT.glob('Verify_*.json'))-before
    assert len(created)==1,'Retry after level instance load finishes'
    fullpath=created.pop();full=core.read(fullpath)
    assert full['preservation_pass'] and not full['dirty'],full['issues']
    out=core.OUT/'NativeGeometry_V007';out.mkdir(parents=True,exist_ok=True)
    bodies=[]
    for row in config['body_outputs']:
        mesh=unreal.load_asset(row['asset']);assert mesh
        assert mesh.get_num_triangles(0)==row['triangles']
        dm,positions=assembly.native_mesh(mesh)
        points=[(p.x,p.y,p.z) for p in positions]
        source=points_from_binary((ROOT/row['file']).with_suffix('.bin'))
        assert len(source)==len(points)==row['vertices']
        error=match_points(source,points)
        _,triangle_list,gaps=unreal.GeometryScript_MeshQueries.get_all_triangle_indices(dm,False)
        assert not gaps
        indices=unreal.GeometryScript_List.convert_triangle_list_to_array(triangle_list)
        triangles=[(t.x,t.y,t.z) for t in indices]
        assert len(triangles)==row['triangles']
        path=out/(row['name']+'.bin')
        with path.open('wb') as f:
            f.write(struct.pack('<ii',len(points),len(triangles)))
            for p in points:f.write(struct.pack('<fff',*p))
            for t in triangles:f.write(struct.pack('<iii',*t))
        assert core.layout.test.package(mesh.get_material(0))==row['material']
        bodies.append({'label':row['actor_label'],'asset':row['asset'],'name':row['name'],
            'vertices':len(points),'triangles':len(triangles),'maximum_vertex_error_cm':error,
            'binary':path.relative_to(ROOT).as_posix(),'sha256':core.sha(path),'material':row['material']})
    kept=config['kept_connection'];mesh=unreal.load_asset(kept['asset'])
    _,positions=assembly.native_mesh(mesh);points=[(p.x,p.y,p.z) for p in positions]
    route=points_from_binary(ROOT/'Saved/Profiling/Graphics/Phase01/U5a_3_V001/Geometry_V011/SM_Ridge_StairRamp3_Connection_V011.bin')
    walking=[route[i+j] for i in range(0,len(route),7) for j in (1,2,3)]
    assert len(walking)==858
    walking_error=match_points(points,walking)
    materials={}
    for slot,expected in kept['material_assignments'].items():
        index=mesh.get_material_index(slot);assert index>=0
        actual=core.layout.test.package(mesh.get_material(index));assert actual==expected
        materials[slot]=actual
    assert not core.layout.test.dirty(),core.layout.test.dirty()
    assert sum(r['vertices'] for r in bodies)==73111
    report={'unit':'U5a-6','level':config['target_level'],'preservation_pass':True,
        'full_verify':fullpath.relative_to(ROOT).as_posix(),'bodies':bodies,
        'total_vertices':73111,'total_triangles':sum(r['triangles'] for r in bodies),
        'connection':{'asset':kept['asset'],'walking_vertices':858,'maximum_vertex_error_cm':walking_error,
            'triangles':mesh.get_num_triangles(0),'materials':materials},
        'dirty':[],'collision_or_PIE_pass':False,'accepted':False,
        'status':'NATIVE_PRESERVATION_AND_VERTICES_PASS; offline native intersection check follows'}
    path=core.OUT/('NativeVerify_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'.json')
    path.write_text(json.dumps(report,indent=1)+'\n',encoding='utf-8')
    unreal.log('U5A6_NATIVE_VERIFY_PASS '+str(path))


if __name__=='__main__':main()
