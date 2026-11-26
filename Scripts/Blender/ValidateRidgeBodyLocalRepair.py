"""U5a-6 오프라인 국소 보정안의 위상·고정 보호 입력·통로 회귀를 검사한다."""
import json
from pathlib import Path
import sys
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
sys.path.insert(0, str(Path(__file__).parent))
import ReviewRidgeBodyIntersections as review
import BuildRidgeClosedBodyTrial as base
import BuildRidgeConnectionTrial as conn


def centered_crossings(v, t):
    center = v.mean(axis=0)
    tree = BVHTree.FromPolygons((v-center).tolist(), t.tolist(), all_triangles=True)
    raw = [(a, b) for a, b in tree.overlap(tree) if a < b]
    return [(a, b) for a, b in raw if review.classify(v[t[a]], v[t[b]])['category'] in ('proper_crossing', 'coplanar_overlap')]


def main():
    exported='--' in sys.argv and sys.argv[sys.argv.index('--')+1]=='exported'
    native='--' in sys.argv and sys.argv[sys.argv.index('--')+1]=='native'
    exports=review.read(review.OUT/'Geometry_V001/Build.json')['outputs'] if exported else []
    native_report=review.read(sorted(review.OUT.glob('NativeVerify_*.json'))[-1]) if native else None
    classification = review.read(review.OUT/'CandidateClassification.json')
    parts = []
    source_parts = []
    results = []
    for body_index,body in enumerate(classification['body_results']):
        source, source_t = review.binary(review.ROOT/body['binary'])
        t=source_t
        proposal = review.OUT/(body['name']+'_ConstrainedProposal.npz')
        v = np.load(proposal)['vertices'] if proposal.exists() else source.copy()
        if exported:
            match=next((r for r in exports if r['source_name']==body['name']),None)
            if match:
                v,export_t=review.binary(review.OUT/'Geometry_V001'/(match['name']+'.bin'))
                assert np.array_equal(export_t,t)
        if native:
            v,t=review.binary(review.ROOT/native_report['bodies'][body_index]['binary'])
        # FBX/bin에 저장할 단정도 좌표까지 포함한 최종 후보를 검사한다.
        v = v.astype(np.float32).astype(np.float64)
        # 실제 UE 정점은 순서가 다를 수 있다. 위치 오차는 NativeVerify의 공간 해시 검사로 판정한다.
        displacement = np.linalg.norm(v-source, axis=1) if not native else np.zeros(len(v))
        moved = np.where(displacement>1e-8)[0]
        topology = base.topology(base.make(body['name']+'_ReviewOnly', v, t))
        remaining = centered_crossings(v, t)
        original_crossings = centered_crossings(source, source_t)
        areas = np.linalg.norm(np.cross(v[t[:,1]]-v[t[:,0]], v[t[:,2]]-v[t[:,0]]), axis=1)/2
        result = {'name':body['name'], 'vertices':len(v), 'triangles':len(t), 'topology':topology,
            'source_centered_crossing_pairs':len(original_crossings), 'remaining_centered_crossing_pairs':len(remaining),
            'changed_vertices':None if native else len(moved), 'max_displacement_cm':None if native else float(displacement.max()),
            'bounds_delta_cm':float(max(abs(v.min(0)-source.min(0)).max(), abs(v.max(0)-source.max(0)).max())),
            'min_triangle_area_cm2':float(areas.min()),
            'sparse_vertex_changes':[{'vertex':int(i), 'before_cm':source[i].tolist(), 'after_cm':v[i].tolist()} for i in moved]}
        assert not remaining, result['name']
        assert topology['boundary_edges']==topology['nonmanifold_edges']==0
        results.append(result)
        parts.append(conn.world_part(body['name'],v,t))
        source_parts.append(conn.world_part(body['name'],source,source_t))
    config = review.read(base.AUTHOR/'BodyTrial.json')
    overlaps = []
    for space in config['protected_spaces']:
        points = conn.prior.env['samples'](space)
        hits = []
        for part in parts:
            lo,hi=part['bounds']
            for i,p in enumerate(points):
                p=np.asarray(p)
                if (p>=lo).all() and (p<=hi).all() and base.inside(part['tree'],p):
                    hits.append({'body':part['owner'],'sample_index':i})
        overlaps.append({'id':space['id'],'samples':len(points),'hits':hits})
    previous=review.read(review.SOURCE/'Geometry_V005/GeometryCheck.json')
    connection=review.read(review.ROOT/'Saved/Profiling/Graphics/Phase01/U5a_4_V001/Geometry_V001/GeometryCheck.json')
    by={r['id']:r for r in connection['new_connection_protection']}
    combined=[dict(r,hits=r['hits']+[dict(h,body='kept_connection') for h in by[r['id']]['hits']]) for r in overlaps]
    assert sum(r['samples'] for r in combined)==16552 and len(combined)==51
    assert not [r for r in combined if r['hits'] and r['id']!='StairRamp3CentralPassage']
    assert [(r['id'],r['hits']) for r in combined]==[(r['id'],r['hits']) for r in previous['overlaps']]
    route=review.ROOT/'Saved/Profiling/Graphics/Phase01/U5a_3_V001/Geometry_V011/SM_Ridge_StairRamp3_Connection_V011.bin'
    route_v,_=review.binary(route)
    stations=[]
    route_rows=route_v.reshape(-1,7,3)
    import math
    for i,(a,b) in enumerate(zip(route_rows,route_rows[1:])):
        count=max(1,math.ceil(np.linalg.norm(b[2]-a[2])/10))
        for j in range(count):
            u=j/count
            for offset,index in ((-150,1),(0,2),(150,3)):
                p=a[index]*(1-u)+b[index]*u
                for up in (100,170,250):
                    stations.append((i,offset,up,p-p/np.linalg.norm(p)*up))
    for i,row in [(len(route_rows)-1,route_rows[-1])]:
        for offset,index in ((-150,1),(0,2),(150,3)):
            for up in (100,170,250):
                p=row[index];stations.append((i,offset,up,p-p/np.linalg.norm(p)*up))
    clearance=conn.clearance(stations,parts)
    assert clearance['pass'] and clearance['samples']==13221
    volume=[]
    for part in parts:
        lo,hi=part['bounds']
        for i,off,up,p in stations:
            if (p>=lo).all() and (p<=hi).all() and base.inside(part['tree'],p):
                volume.append({'body':part['owner'],'station':i,'offset':off,'up':up})
    assert not volume
    probes={}
    for key,p in config['report_points_local_xyh_cm'].items():
        world=base.world(*p)
        probes[key]={'point':p,'inside_bodies':[part['owner'] for part in parts if base.inside(part['tree'],world)],
                     'nearest_surface_cm':min(part['tree'].find_nearest(Vector(world))[3] for part in parts),
                     'source_inside_bodies':[part['owner'] for part in source_parts if base.inside(part['tree'],world)],
                     'source_nearest_surface_cm':min(part['tree'].find_nearest(Vector(world))[3] for part in source_parts)}
    r01=base.world(*config['report_points_local_xyh_cm']['R01'])
    sensitivity=[]
    nearest=min((part['tree'].find_nearest(Vector(r01)) for part in source_parts),key=lambda hit:hit[3])
    normal=np.asarray(nearest[1],dtype=float)
    normal_sensitivity=[]
    for delta in (-10,-5,-2,-1,0,1,2,5,10):
        p=r01-r01/np.linalg.norm(r01)*delta
        sensitivity.append({'local_up_offset_cm':delta,'inside_bodies':[part['owner'] for part in parts if base.inside(part['tree'],p)],
                            'source_inside_bodies':[part['owner'] for part in source_parts if base.inside(part['tree'],p)]})
        normal_point=r01+normal*delta
        normal_sensitivity.append({'surface_normal_offset_cm':delta,
            'inside_bodies':[part['owner'] for part in parts if base.inside(part['tree'],normal_point)],
            'source_inside_bodies':[part['owner'] for part in source_parts if base.inside(part['tree'],normal_point)]})
    report={'unit':'U5a-6','status':'actual UE geometry intersection and protection checked' if native else ('exported geometry checked; native verification pending' if exported else 'offline proposal only; live baseline pending'),'bodies':results,
        'protected_spaces':51,'samples':16552,'overlaps':combined,'legacy_review_samples':31,
        'other50_new_hits':0,'connection_clearance':clearance,'connection_body_volume_hits':volume,
        'probes':probes,'R01_local_up_sensitivity':sensitivity,
        'R01_surface_normal_sensitivity':normal_sensitivity,
        'seam_min_world_component_cm':float(min(p['bounds'][0].min() for p in parts)),
        'collision_or_PIE_pass':False,'new_map_created':False}
    report['topology_pass']=True
    report['protected_pass']=False
    report['new_interferences_other50']=[]
    if native:
        report['native_verify']={'path':sorted(review.OUT.glob('NativeVerify_*.json'))[-1].relative_to(review.ROOT).as_posix(),
                                 'total_vertices':native_report['total_vertices']}
    review.write('NativeGeometryCheck_V007.json' if native else ('Geometry_V001/GeometryCheck.json' if exported else 'LocalRepairValidation.json'),report)
    print('LOCAL_VALIDATION '+json.dumps({'changed':[r['changed_vertices'] for r in results],
        'max_cm':[r['max_displacement_cm'] for r in results], 'bounds_delta_cm':[r['bounds_delta_cm'] for r in results],
        'remaining_pairs':[r['remaining_centered_crossing_pairs'] for r in results],
        'route':clearance,'probes':probes}),flush=True)


if __name__=='__main__':
    main()
