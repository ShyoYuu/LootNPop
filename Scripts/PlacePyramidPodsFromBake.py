"""같은 실제 지형의 임시 베이크에서 검증된 Pod 후보를 골라 원본 LVI에 저장한다."""
import gc
import json
import math
import struct
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
ns={'__name__':'pyramid_native_placement'}
exec(compile((ROOT/'Scripts/AssemblePyramidOctant.py').read_text(encoding='utf-8'),
             'AssemblePyramidOctant.py','exec'),ns)
OUT=ns['OUT']
ROOT_LEVEL=ns['LEVEL']
placement=ns['placement']
PROBE='/Game/Maps/_WorkInProgress/Pyramid_00/SpawnProbe/'+ns['VERSION']
PROBE_LEVEL=PROBE+'/LVI_Octant_Pyramid_00'
PROBE_DATA=PROBE+'/DA_OctantSurface_Pyramid_00'


def local_point(position):
    x,y,z=position
    depth=(x+y+z)/math.sqrt(3)
    return [30000*(-x-y+2*z)/math.sqrt(6)/depth,
            30000*(-x+y)/math.sqrt(2)/depth,
            30000-math.sqrt(x*x+y*y+z*z)]


def probe_candidates():
    # 최종 메시·태그·충돌 설정은 그대로 사용하고 수동 스폰점만 없는 작업 맵을 만든다.
    ns['LEVEL']=PROBE_LEVEL
    ns['placement']={'pods':[],'anchors':[],'panels':[],'reviews':[]}
    ns['assemble']()
    data_file=ROOT/'Content'/PROBE_DATA.removeprefix('/Game/')
    data_file=data_file.with_suffix('.uasset')
    before=data_file.stat().st_mtime_ns if data_file.exists() else None
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world,'LNP.SurfaceNav.BakeOctant '+PROBE_LEVEL)
    assert data_file.exists() and data_file.stat().st_mtime_ns!=before,'Native candidate probe bake failed'
    data=unreal.load_asset(PROBE_DATA)
    layers=struct.unpack_from('<H',bytes(data.get_editor_property('support_payload')),2)[0]
    assert layers<=256,('Runtime layer budget exceeded',layers)
    payload=bytes(data.get_editor_property('spawn_payload'))
    version,reserved,authored,count=struct.unpack_from('<HHII',payload)
    assert version==3 and authored==0 and len(payload)==12+40*count
    candidates=[]
    for i in range(count):
        values=struct.unpack_from('<IfffhhHBBffff',payload,12+i*40)
        if values[7]&3!=3 or values[10]<300 or values[12]<2700:
            continue
        local=local_point(values[1:4])
        r=31500*(1-local[2]/18000) if local[2]>=0 else 31500*(1+local[2]/9000)
        extent=max(-2*local[0],local[0]+math.sqrt(3)*local[1],local[0]-math.sqrt(3)*local[1])
        if extent>r*0.90:
            continue
        candidates.append({'index':values[0],'world':list(values[1:4]),'local':local,
                           'layer':values[6],'edge_clearance_cm':values[10],'headroom_cm':values[12]})
    return candidates,layers


def clear_pose(local):
    x,y,h=local
    scale=math.sqrt(30000**2+x*x+y*y)/(30000-h)
    for radius in (0,150*scale,250*scale):
        for i in range(16 if radius else 1):
            a=i*math.tau/16
            xx,yy=x+radius*math.cos(a),y+radius*math.sin(a)
            hit=ns['SceneTools'].trace_world(ns['point']([xx,yy,h+50]),ns['point']([xx,yy,h-100]))
            if hit is None or abs(hit-50)>6:
                return False
    for radius in (0,200*scale):
        for i in range(8 if radius else 1):
            a=i*math.tau/8
            xx,yy=x+radius*math.cos(a),y+radius*math.sin(a)
            if ns['SceneTools'].trace_world(ns['point']([xx,yy,h+100]),ns['point']([xx,yy,h+2635])) is not None:
                return False
    return True


def choose(candidates):
    selected=[]
    retired=[]
    for pod in placement['pods']:
        h=ns['manifest']['levels_cm'][pod['storey']]
        origin=ns['point'](pod['local'])
        pool=[c for c in candidates if abs(c['local'][2]-h)<15]
        pool.sort(key=lambda c:(sum((v-getattr(origin,axis))**2 for v,axis in zip(c['world'],('x','y','z'))),c['index']))
        found=None
        for candidate in pool:
            if any(sum((a-b)**2 for a,b in zip(candidate['world'],p['world']))<2200**2 for p in selected):
                continue
            if clear_pose(candidate['local']):
                found=candidate
                break
        if found is None:
            assert any(p['storey']==pod['storey'] for p in selected),('No native combat spawn on storey',pod['id'])
            retired.append(pod['id'])
            continue
        pod['local']=found['local']
        pod['native_candidate']={k:found[k] for k in ('index','layer','edge_clearance_cm','headroom_cm')}
        selected.append({'id':pod['id'],'storey':pod['storey'],'world':found['world']})
    placement['pods']=[p for p in placement['pods'] if p['id'] not in retired]
    by_id={p['id']:p for p in placement['pods']}
    placement['anchors']=[a for a in placement['anchors'] if a.get('route_group') or a.get('rupture_id') or a['id'] in by_id]
    for anchor in placement['anchors']:
        if anchor.get('route_group') or anchor.get('rupture_id'):
            continue
        x,y,h=by_id[anchor['id']]['local']
        anchor['local']=[x,y,h+800]
    for review,storey in zip(placement['reviews'],(4,8,0,12)):
        pod=next(p for p in placement['pods'] if p['storey']==storey)
        x,y,h=pod['local']
        review['local']=[x,y,h+200]
    (OUT/'placement.json').write_text(json.dumps(placement,indent=2),encoding='utf-8')
    (OUT/'native-placement.json').write_text(json.dumps({'probe_surface_data':PROBE_DATA,
        'candidate_count':len(candidates),'support_layers':layers,'retired':retired,
        'selected':selected,'pods':len(placement['pods'])},indent=2),encoding='utf-8')


if all(p.get('native_candidate') for p in placement['pods']):
    unreal.log('PYRAMID_NATIVE_PLACEMENT_READY')
else:
    candidates,layers=probe_candidates()
    choose(candidates)
    ns['LEVEL']=ROOT_LEVEL
    ns['placement']=placement
    gc.collect()
    ns['assemble']()
    gc.collect()
    ns['verify']()
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/Maps/TestMap03')
    unreal.log('PYRAMID_NATIVE_PLACEMENT_READY')
