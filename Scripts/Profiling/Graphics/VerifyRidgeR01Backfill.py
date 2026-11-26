"""U5a-7 V008의 전체 보존·기존 몸체 불변·새 국소 몸체 정점을 읽기 전용 검사한다.

UE Cmd: py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/VerifyRidgeR01Backfill.py"
"""
import datetime
import json
from pathlib import Path
import struct
import sys
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
HERE=ROOT/'Scripts/Profiling/Graphics'
sys.path[:0]=[str(HERE),str(ROOT/'Scripts')]
import RidgeClosedBodyTrial as core
import AssembleRidgeOctant as assembly
import VerifyRidgeBodyIntersectionRepair as helpers


def main():
    core.CONFIG=HERE/'ClosedBodyTrial_V001/Generation_V008.json'
    c,_=core.inputs()
    before=set(core.OUT.glob('Verify_*.json'));core.verify()
    created=set(core.OUT.glob('Verify_*.json'))-before
    assert len(created)==1,'Retry after level load finishes'
    fullpath=created.pop();full=core.read(fullpath)
    assert full['preservation_pass'] and not full['dirty'],full['issues']
    previous=core.read(sorted((ROOT/'Saved/Profiling/Graphics/Phase01/U5a_6_V001').glob('NativeVerify_*.json'))[-1])
    by_asset={r['asset']:r for r in previous['bodies']}
    out=core.OUT/'NativeGeometry_V008';out.mkdir(parents=True,exist_ok=True);bodies=[]
    for row in c['body_outputs']:
        asset=unreal.load_asset(row['asset']);assert asset
        dm,positions=assembly.native_mesh(asset);points=[(p.x,p.y,p.z) for p in positions]
        source=helpers.points_from_binary((ROOT/row['file']).with_suffix('.bin'))
        assert len(points)==len(source)==row['vertices']
        error=helpers.match_points(source,points)
        _,triangle_list,gaps=unreal.GeometryScript_MeshQueries.get_all_triangle_indices(dm,False);assert not gaps
        triangles=[(t.x,t.y,t.z) for t in unreal.GeometryScript_List.convert_triangle_list_to_array(triangle_list)]
        assert len(triangles)==asset.get_num_triangles(0)==row['triangles']
        path=out/(row['name']+'.bin')
        with path.open('wb') as f:
            f.write(struct.pack('<ii',len(points),len(triangles)))
            for p in points:f.write(struct.pack('<fff',*p))
            for t in triangles:f.write(struct.pack('<iii',*t))
        digest=core.sha(path)
        if row['asset'] in by_asset:assert digest==by_asset[row['asset']]['sha256'],row['name']
        assert core.layout.test.package(asset.get_material(0))==row['material']
        bodies.append({'asset':row['asset'],'name':row['name'],'label':row['actor_label'],
            'vertices':len(points),'triangles':len(triangles),'maximum_vertex_error_cm':error,
            'binary':path.relative_to(ROOT).as_posix(),'sha256':digest,'held_from_V007':row['asset'] in by_asset})
    kept=c['kept_connection'];mesh=unreal.load_asset(kept['asset'])
    _,positions=assembly.native_mesh(mesh);points=[(p.x,p.y,p.z) for p in positions]
    route=helpers.points_from_binary(ROOT/'Saved/Profiling/Graphics/Phase01/U5a_3_V001/Geometry_V011/SM_Ridge_StairRamp3_Connection_V011.bin')
    walking=[route[i+j] for i in range(0,len(route),7) for j in (1,2,3)]
    assert len(walking)==858
    walking_error=helpers.match_points(points,walking);materials={}
    for slot,expected in kept['material_assignments'].items():
        actual=core.layout.test.package(mesh.get_material(mesh.get_material_index(slot)));assert actual==expected
        materials[slot]=actual
    assert not core.layout.test.dirty()
    assert sum(r['vertices'] for r in bodies)==73273
    report={'unit':'U5a-7','level':c['target_level'],'preservation_pass':True,
        'full_verify':fullpath.relative_to(ROOT).as_posix(),'bodies':bodies,
        'total_vertices':73273,'total_triangles':146126,'held_body_count':4,
        'connection':{'asset':kept['asset'],'walking_vertices':858,'maximum_vertex_error_cm':walking_error,'materials':materials,'triangles':6288},
        'dirty':[],'accepted':False,'collision_or_PIE_pass':False,
        'status':'NATIVE_PRESERVATION_AND_VERTICES_PASS; actual backfill geometry check follows'}
    path=core.OUT/('NativeVerify_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'.json')
    path.write_text(json.dumps(report,indent=1)+'\n',encoding='utf-8')
    unreal.log('U5A7_NATIVE_VERIFY_PASS '+str(path))


if __name__=='__main__':main()
