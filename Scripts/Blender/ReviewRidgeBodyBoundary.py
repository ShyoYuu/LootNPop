"""U5a-7 채택 V007의 R01 경계와 기존 Ramp3 표본을 읽기 전용 정밀 검토한다."""
import hashlib
import json
import math
from pathlib import Path
import sys
import numpy as np
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).parent))
import BuildRidgeClosedBodyTrial as base
import BuildRidgeConnectionTrial as conn

ROOT=Path(__file__).resolve().parents[2]
AUTHOR=ROOT/'Scripts/Profiling/Graphics/ClosedBodyTrial_V001'
PREVIOUS=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_6_V001'
OUT=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_7_V001'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(name,data):
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/name).write_text(json.dumps(data,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')


def frozen_inputs():
    frozen=read(AUTHOR/'FrozenInputs_V007.json')['files']
    handoff=read(PREVIOUS/'AcceptedHandoffHashes_V007.json')
    combined={}
    for row in frozen+handoff:
        key=row['path'].casefold()
        assert key not in combined or combined[key]['sha256']==row['sha256']
        combined[key]={'path':row['path'],'sha256':row['sha256']}
    for row in combined.values():assert sha(ROOT/row['path'])==row['sha256'],row['path']
    assert len(combined)==534
    for row in handoff:assert sha(ROOT/row['backup'])==row['sha256']
    write('PreservationHashes.json',list(combined.values()))
    return list(combined.values())


def native_parts():
    report=read(sorted(PREVIOUS.glob('NativeVerify_*.json'))[-1])
    parts=[]
    for row in report['bodies']:
        v,t=conn.binary(ROOT/row['binary'])
        part=conn.world_part(row['name'],v,t);part['label']=row['label'];parts.append(part)
    return parts


def ray_hits(tree,origin,direction,maximum):
    origin=np.asarray(origin,float);direction=np.asarray(direction,float)
    cursor=origin.copy();hits=[]
    for _ in range(100):
        hit,normal,face,distance=tree.ray_cast(Vector(cursor),Vector(direction),maximum)
        if hit is None:break
        along=float(np.dot(np.asarray(hit)-origin,direction))
        if along>maximum:break
        hits.append({'distance_cm':along,'point_world_cm':list(hit),'face':face})
        cursor=np.asarray(hit)+direction*.1
    return hits


def r01_grid(parts,point,reference_tree):
    target=min(parts,key=lambda p:p['tree'].find_nearest(Vector(point))[3])
    q,n,face,distance=target['tree'].find_nearest(Vector(point))
    q=np.asarray(q,float);normal=np.asarray(n,float);normal/=np.linalg.norm(normal)
    if not base.inside(target['tree'],q-normal*10):
        assert base.inside(target['tree'],q+normal*10)
        normal=-normal
    up=-q/np.linalg.norm(q)
    horizontal=np.cross(up,normal);horizontal/=np.linalg.norm(horizontal)
    vertical=np.cross(normal,horizontal);vertical/=np.linalg.norm(vertical)
    maximum=float(np.linalg.norm(target['bounds'][1]-target['bounds'][0])*1.2+200)
    rows=[]
    for y in range(-100,101,25):
        for x in range(-100,101,25):
            center=q+horizontal*x+vertical*y;origin=center+normal*100
            body=ray_hits(target['tree'],origin,-normal,maximum)
            ed=ray_hits(reference_tree,origin,-normal,maximum)
            local=base.local(center.reshape(1,3))[0]
            ground=base.spec.massif_ramp_ground(float(local[0]),float(local[1]))
            for hit in ed:
                edlocal=base.local(np.asarray(hit['point_world_cm']).reshape(1,3))[0]
                hit['local_xyh_cm']=edlocal.tolist()
                hit['above_analytic_ground_cm']=float(edlocal[2]-base.spec.massif_ramp_ground(float(edlocal[0]),float(edlocal[1])))
            thickness=body[1]['distance_cm']-body[0]['distance_cm'] if len(body)>=2 else None
            rows.append({'horizontal_cm':x,'vertical_cm':y,'origin_world_cm':origin.tolist(),
                'center_local_xyh_cm':local.tolist(),'center_above_analytic_ground_cm':float(local[2]-ground),
                'body_hits':body,'ED_hits':ed,'body_hit_count':len(body),
                'first_segment_thickness_cm':thickness,
                'body_first_entry_from_plane_cm':body[0]['distance_cm']-100 if body else None})
    return {'target':target['owner'],'center_world_cm':q.tolist(),'outward_normal':normal.tolist(),
        'horizontal':horizontal.tolist(),'vertical':vertical.tolist(),'spacing_cm':25,'span_cm':[-100,100],
        'ray_count':len(rows),'body_ray_misses':sum(not r['body_hits'] for r in rows),
        'odd_hit_counts':sum(r['body_hit_count']%2!=0 for r in rows),
        'minimum_first_segment_thickness_cm':min((r['first_segment_thickness_cm'] for r in rows if r['first_segment_thickness_cm'] is not None),default=None),
        'capsule_reference':{'radius_cm':42,'half_height_cm':96,'source':'Source/LootNPop/Character/LNPCharacterBase.cpp:41','use':'dimension context only; no swept capsule or physics validation'},
        'rows':rows}


def fixed_connection_samples():
    path=ROOT/'Saved/Profiling/Graphics/Phase01/U5a_3_V001/Geometry_V011/SM_Ridge_StairRamp3_Connection_V011.bin'
    v,_=conn.binary(path);rows=v.reshape(-1,7,3);stations=[]
    for i,(a,b) in enumerate(zip(rows,rows[1:])):
        count=max(1,math.ceil(np.linalg.norm(b[2]-a[2])/10))
        for j in range(count):
            u=j/count
            for offset,index in ((-150,1),(0,2),(150,3)):
                p=a[index]*(1-u)+b[index]*u
                for h in (100,170,250):stations.append((i,offset,h,p-p/np.linalg.norm(p)*h))
    for offset,index in ((-150,1),(0,2),(150,3)):
        p=rows[-1,index]
        for h in (100,170,250):stations.append((len(rows)-1,offset,h,p-p/np.linalg.norm(p)*h))
    assert len(stations)==13221
    return stations


def regression(parts,connection,config):
    overlaps=[]
    for space in config['protected_spaces']:
        points=conn.prior.env['samples'](space);hits=[]
        for part in parts+[connection]:
            lo,hi=part['bounds']
            for i,p in enumerate(points):
                p=np.asarray(p)
                if (p>=lo).all() and (p<=hi).all() and base.inside(part['tree'],p):
                    hits.append({'body':part['owner'],'sample_index':i})
        overlaps.append({'id':space['id'],'samples':len(points),'hits':hits})
    assert len(overlaps)==51 and sum(r['samples'] for r in overlaps)==16552
    others=[r for r in overlaps if r['hits'] and r['id']!='StairRamp3CentralPassage']
    assert not others
    assert sum(len(r['hits']) for r in overlaps)==31
    stations=fixed_connection_samples();clearance=conn.clearance(stations,parts);volume=[]
    for part in parts:
        lo,hi=part['bounds']
        for i,offset,h,p in stations:
            if (p>=lo).all() and (p<=hi).all() and base.inside(part['tree'],p):volume.append({'body':part['owner'],'station':i})
    assert clearance['pass'] and not volume
    return {'spaces':51,'samples':16552,'overlaps':overlaps,'other50_new_hits':0,
        'legacy_review_samples':31,'fixed_connection_clearance':clearance,'fixed_connection_body_volume_hits':volume}


def neighborhood(config,name='SourceNeighborhood.json'):
    point=config['report_points_local_xyh_cm']['R01'];world=base.world(*point)
    parts=conn.native_parts([point]);records=[]
    for part in parts:
        p=(world-part['origin'])@part['rot']/part['scale']
        hit,normal,face,distance=part['tree'].find_nearest(Vector(p))
        actual=(np.asarray(hit)*part['scale'])@part['rot'].T+part['origin']
        v,t=part['geometry']
        records.append({'owner':part['owner'],'mesh':part['mesh'],'origin_world_cm':part['origin'].tolist(),
            'distance_to_R01_cm':float(np.linalg.norm(actual-world)),'nearest_world_cm':actual.tolist(),
            'nearest_local_xyh_cm':base.local(actual.reshape(1,3))[0].tolist(),
            'bounds_world_cm':[np.asarray(p).tolist() for p in part['bounds']],
            'vertices':len(v),'triangles':len(t)})
    records.sort(key=lambda r:r['distance_to_R01_cm'])
    write(name,{'method':'frozen kept Z1 and21 adopted owner transforms; original fallback geometry; nearest local view candidates only',
        'parts':records})
    print('R01_NEIGHBORHOOD '+json.dumps(records[:10]),flush=True)


def user_gap():
    camera=read(OUT/'UserGapCamera.json');point=np.asarray([camera['location'][k] for k in ('x','y','z')])
    local=base.local(point.reshape(1,3))[0]
    bodies=[]
    for part in native_parts():
        hit=part['tree'].find_nearest(Vector(point))
        bodies.append({'owner':part['owner'],'inside':base.inside(part['tree'],point),'distance_cm':hit[3],
            'nearest_world_cm':list(hit[0]),'face':hit[2]})
    write('UserGapDiagnosis.json',{'camera':camera,'local_xyh_cm':local.tolist(),'bodies':sorted(bodies,key=lambda r:r['distance_cm']),
        'note':'user placed editor camera at observed opening; no physics inference'})
    config=read(AUTHOR/'BodyTrial.json');config['report_points_local_xyh_cm']['R01']=local.tolist()
    neighborhood(config,'UserGapNeighborhood.json')


def main():
    hashes=frozen_inputs();parts=native_parts();config=read(AUTHOR/'BodyTrial.json')
    point=base.world(*config['report_points_local_xyh_cm']['R01'])
    nearest=[]
    for part in parts:
        hit=part['tree'].find_nearest(Vector(point))
        nearest.append({'owner':part['owner'],'label':part['label'],'distance_cm':hit[3],
            'nearest_world_cm':list(hit[0]),'normal':list(hit[1]),'face':hit[2],
            'inside':base.inside(part['tree'],point)})
    _,raw,_,_,geometry=base.load_inputs();references=[];reference_trees={}
    for index in (35430,116462,116490):
        row=raw['instances'][index];mesh=raw['components'][row[0]][3];name=raw['meshes'][mesh][0]
        v,t=base.transformed(row,name,geometry);tree=base.tree(v,t);hit=tree.find_nearest(Vector(point))
        reference_trees[index]=tree
        references.append({'scan':index,'mesh':name,'distance_cm':hit[3],
            'nearest_world_cm':list(hit[0]),'normal':list(hit[1]),'face':hit[2]})
    space=next(s for s in config['protected_spaces'] if s['id']=='StairRamp3CentralPassage')
    points=conn.prior.env['samples'](space);legacy=[]
    c=read(AUTHOR/'Generation_V007.json')['kept_connection']
    cv,ct=conn.binary((ROOT/c['file']).with_suffix('.bin'))
    connection=conn.world_part('kept_connection',cv,ct)
    for part in parts+[connection]:
        lo,hi=part['bounds']
        for i,p in enumerate(points):
            p=np.asarray(p)
            if (p>=lo).all() and (p<=hi).all() and base.inside(part['tree'],p):
                near=part['tree'].find_nearest(Vector(p))
                legacy.append({'owner':part['owner'],'sample_index':i,'station':i//6,
                    'station_fraction':(i//6)/(len(points)//6-1),'lateral_offset_cm':(-150,0,150)[(i%6)//2],
                    'height_offset_cm':(100,250)[i%2],'world_cm':p.tolist(),
                    'local_xyh_cm':base.local(p.reshape(1,3))[0].tolist(),'surface_distance_cm':near[3]})
    grid=r01_grid(parts,point,reference_trees[min(references,key=lambda r:r['distance_cm'])['scan']])
    checks=regression(parts,connection,config)
    report={'unit':'U5a-7','method':'read accepted actual UE bodies and frozen source ED fallback geometry; no map changes',
        'preserved_files':len(hashes),'R01_record_local_xyh_cm':config['report_points_local_xyh_cm']['R01'],
        'R01_world_cm':point.tolist(),'R01_bodies':sorted(nearest,key=lambda r:r['distance_cm']),
        'R01_ED_references':sorted(references,key=lambda r:r['distance_cm']),
        'legacy_space':space,'legacy_samples':len(points),'legacy_hits':legacy,
        'R01_grid':grid,'regression':checks,
        'collision_or_PIE_pass':False,'new_V008_map_created':False}
    write('BoundarySurvey.json',report)
    print('BOUNDARY_SURVEY '+json.dumps({'R01_body':report['R01_bodies'][0],
        'R01_ED':report['R01_ED_references'][0],'legacy_hits':len(legacy),
        'legacy_owner_counts':{name:sum(r['owner']==name for r in legacy) for name in sorted({r['owner'] for r in legacy})},
        'R01_ray_misses':grid['body_ray_misses'],'R01_min_thickness_cm':grid['minimum_first_segment_thickness_cm'],
        'route_clearance':checks['fixed_connection_clearance']}),flush=True)


if __name__=='__main__':main()
