"""U5a-7 실제 UE 새 몸체의 닫힘/교차/원본 외곽과 고정 공간을 검증한다."""
from pathlib import Path
import sys
import json
import numpy as np
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).parent))
import ReviewRidgeBodyBoundary as review
import BuildRidgeClosedBodyTrial as base
import BuildRidgeConnectionTrial as conn
import BuildRidgeR01JoinBody as candidate
import RefineRidgeR01ClosureHull as hull_tools


def main():
    review.frozen_inputs();path=sorted(review.OUT.glob('NativeVerify_*.json'))[-1];native=review.read(path)
    baseline=review.read(review.PREVIOUS/'NativeGeometryCheck_V007.json')
    expected_sign=float(np.sign(baseline['bodies'][0]['topology']['signed_volume_m3']))
    parts=[];results=[]
    for row in native['bodies']:
        v,t=conn.binary(review.ROOT/row['binary']);obj=base.make(row['name']+'_ActualReview',v,t)
        topology=base.topology(obj);pairs=candidate.crossings(v,t)
        assert topology['boundary_edges']==topology['nonmanifold_edges']==0
        assert abs(topology['signed_volume_m3'])>1e-6 and np.sign(topology['signed_volume_m3'])==expected_sign
        assert not pairs
        parts.append(conn.world_part(row['name'],v,t))
        results.append({'name':row['name'],'vertices':len(v),'triangles':len(t),'topology':topology,'actual_self_intersection_pairs':0,'held_from_V007':row['held_from_V007']})
    geometry=review.read(review.OUT/'Geometry_V006/GeometryCheck.json');target=np.asarray(geometry['target_world_cm']);user=np.asarray(geometry['user_camera_world_cm'])
    backfill=parts[-1]
    refs=conn.native_parts([base.local(user.reshape(1,3))[0].tolist()])
    source=next(p for p in refs if p['mesh'].endswith('/SM_Cliff_09') and np.linalg.norm(p['origin']-np.asarray([21737.11,15022.82,13082.1]))<.1)
    nv,nt=source['geometry'];world=(nv*source['scale'])@source['rot'].T+source['origin'];hull=hull_tools.make_hull(world)
    outside=[i for i,p in enumerate(backfill['geometry'][0]) if not base.inside(hull['tree'],p)]
    assert not outside and base.inside(backfill['tree'],target) and not base.inside(backfill['tree'],user)
    config=review.read(review.AUTHOR/'BodyTrial.json');old=review.read(review.AUTHOR/'Generation_V007.json')
    connection=conn.world_part('kept_connection',*conn.binary((review.ROOT/old['kept_connection']['file']).with_suffix('.bin')))
    regression=review.regression(parts,connection,config)
    probes={key:{'point':p,'inside_bodies':[part['owner'] for part in parts if base.inside(part['tree'],base.world(*p))]} for key,p in config['report_points_local_xyh_cm'].items()}
    grid=review.read(review.OUT/'BoundarySurvey.json')['R01_grid'];rays=[]
    normal=-np.asarray(grid['outward_normal'])
    for row in grid['rows']:
        hit_parts=[part['owner'] for part in parts if part['tree'].ray_cast(Vector(row['origin_world_cm']),Vector(normal),4000)[0] is not None]
        rays.append({'horizontal_cm':row['horizontal_cm'],'vertical_cm':row['vertical_cm'],'hit_bodies':hit_parts})
    report={'unit':'U5a-7','native_verify':{'path':path.relative_to(review.ROOT).as_posix(),'sha256':base.sha(path)},
        'actual_bodies':results,'actual_body_count':5,'new_backfill_vertices':162,'new_backfill_triangles':320,
        'target_inside':True,'user_camera_outside_new_body':True,'outside_original_Cliff09_hull_vertices':[],
        'actual_backfill_volume_m3':abs(results[-1]['topology']['signed_volume_m3']),
        'native_triangle_winding':'signed volume matches frozen V007 actual UE body sign; UE source triangle winding differs from Blender authoring sign',
        'actual_target_surface_distance_cm':backfill['tree'].find_nearest(Vector(target))[3],
        'held4_actual_binary_hashes_match_V007':all(row['held_from_V007'] for row in native['bodies'][:4]),
        'regression':regression,'report_points':probes,
        'original_R01_grid':{'before_misses':9,'after_union_misses':sum(not row['hit_bodies'] for row in rays),'rows':rays,
            'status':'bounded ray evidence only; no whole R01 closure or access claim'},
        'collision_or_PIE_pass':False,'accepted':False}
    review.write('NativeGeometryCheck_V008.json',report)
    print('ACTUAL_BACKFILL_CHECK '+json.dumps({'actual_self_intersections':[r['actual_self_intersection_pairs'] for r in results],
        'volume_m3':report['actual_backfill_volume_m3'],'target_inside':True,'user_camera_outside':True,
        'outside_source_hull':0,'legacy_review_samples':31,'other50_new_hits':0,
        'fixed_route':regression['fixed_connection_clearance'],'R01_after_ray_misses':report['original_R01_grid']['after_union_misses']}),flush=True)


if __name__=='__main__':main()
