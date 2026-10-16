"""삼각쌍뿔 원본의 통행 표본·면 방향·장치 여유와 검토 이미지를 기록한다."""
import json
import math
from pathlib import Path
import bpy
import bmesh
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[2]
VERSION='V058'
OUT=ROOT/'Saved/SliceOctant/Pyramid_00'/VERSION
ns={'__file__':str(ROOT/'Scripts/Blender/PyramidOctantCommon.py'),'__name__':'pyramid_helpers'}
exec(compile(Path(ns['__file__']).read_text(encoding='utf-8'),ns['__file__'],'exec'),ns)
scene=bpy.data.scenes['Pyramid_00_'+VERSION]
bpy.context.window.scene=scene
body=scene.objects['ProjectedEnvelope']
bm=bmesh.new()
bm.from_mesh(body.data)
print('PYRAMID_SIGNED_VOLUME '+str(bm.calc_volume(signed=True)),flush=True)
faces=[]
for h in ns['LEVELS']:
    selected=[f for f in bm.faces if all(abs(30000-ns['to_ue'](v.co).length-h)<0.15 for v in f.verts)]
    dots=[f.normal.dot(-f.calc_center_median().normalized()) for f in selected]
    faces.append({'h':h,'count':len(dots),'min':min(dots,default=0),'max':max(dots,default=0),
                  'up':sum(v>0.71 for v in dots),'down':sum(v<-0.71 for v in dots)})
print('PYRAMID_FLOOR_NORMALS '+json.dumps(faces),flush=True)
tree=BVHTree.FromBMesh(bm)
placement=json.loads((OUT/'placement.json').read_text(encoding='utf-8'))
assert not any(p.get('native_candidate') for p in placement['pods']), \
    'Native placement already certified; source relocation must run before PlacePyramidPodsFromBake.py'
def ray(local0,local1,source=None):
    a,b=ns['to_blender'](ns['point'](*local0)),ns['to_blender'](ns['point'](*local1))
    delta=b-a
    return (source or tree).ray_cast(a,delta.normalized(),delta.length)


def floor_valid(local):
    x,y,h=local
    scale=math.sqrt(30000**2+x*x+y*y)/(30000-h)
    for radius in (0,150*scale,250*scale):
        for i in range(16 if radius else 1):
            a=i*math.tau/16
            xx,yy=x+radius*math.cos(a),y+radius*math.sin(a)
            hit,normal,index,distance=ray([xx,yy,h+50],[xx,yy,h-100])
            if hit is None or abs(distance*100-50)>6 or normal.dot(-hit.normalized())<0.71:
                return False
    for radius in (0,200*scale):
        for i in range(8 if radius else 1):
            a=i*math.tau/8
            xx,yy=x+radius*math.cos(a),y+radius*math.sin(a)
            if ray([xx,yy,h+100],[xx,yy,h+2635])[0] is not None:
                return False
    return True


# 복층 홀의 중심을 우선하고 파손 후에도 같은 층의 드론 전투 공간을 찾는다.
used=[]
retired=[]
manifest=json.loads((OUT/'manifest.json').read_text(encoding='utf-8'))
for pod in placement['pods']:
    h=pod['local'][2]
    candidates=[pod['local']]
    hall=next(v for v in manifest['double_height_halls'] if v['id']==pod['id'])
    center=Vector(hall['center'][:2])
    for outline in [hall['outline']]:
        for vertex in outline:
            for t in (0.2,0.4,0.6):
                candidates.append([*center.lerp(Vector(vertex),t),h])
    r=ns['radius'](h)
    for ix in range(-10,21):
        for iy in range(-17,18):
            x,y=r*ix/22,r*iy/22
            if ns['extent']((x,y))<r*0.85:
                candidates.append([x,y,h])
    found=None
    for candidate in candidates:
        if any((ns['point'](*q)-ns['point'](*candidate)).length<2200 for q in used):
            continue
        if floor_valid(candidate):
            found=candidate
            break
    if found is None:
        assert any(abs(q[2]-h)<1 for q in used),('No double-height combat space on storey',pod)
        retired.append(pod['id'])
        continue
    pod['local']=found
    used.append(found)
placement['pods']=[p for p in placement['pods'] if p['id'] not in retired]
by_id={p['id']:p for p in placement['pods']}
for review,storey in zip(placement['reviews'],(4,8,0,12)):
    pod=next(p for p in placement['pods'] if p['storey']==storey)
    review['local']=[pod['local'][0],pod['local'][1],pod['local'][2]+200]
placement['anchors']=[a for a in placement['anchors'] if a.get('route_group') or a.get('rupture_id') or a['id'] in by_id]
for anchor in placement['anchors']:
    if anchor.get('route_group') or anchor.get('rupture_id'):
        continue
    pod=by_id[anchor['id']]
    x,y,h=pod['local']
    candidate=[x,y,h]
    anchor['local']=[*candidate[:2],h+800] if floor_valid(candidate) else [x,y,h+800]

ramp_checks=[]
failures=[]
for ramp in placement['ramps']:
    path=ramp['path']
    for segment,(a,b) in enumerate(zip(path,path[1:])):
        aa,bb=Vector(a),Vector(b)
        tangent=(bb-aa).xy.normalized()
        side=Vector((-tangent.y,tangent.x))*ramp['width_cm']*0.30
        for i in range(21):
            local=aa.lerp(bb,i/20)
            for offset in (-1,0,1):
                p=local+Vector((side.x*offset,side.y*offset,0))
                hit,normal,index,distance=ray([p.x,p.y,p.z+50],[p.x,p.y,p.z-100])
                slope=math.degrees(math.acos(max(-1,min(1,normal.dot(-hit.normalized()))))) if hit is not None else 180
                ceiling=ray([p.x,p.y,p.z+180],[p.x,p.y,p.z+450])[0]
                check={'ramp':ramp['id'],'segment':segment,'t':i/20,'offset':offset,
                       'local':list(p),'floor_error_cm':None if hit is None else distance*100-50,
                       'slope_deg':slope,'blocked_headroom':ceiling is not None}
                ramp_checks.append(check)
                if hit is None or abs(distance*100-50)>8 or slope>44.765 or ceiling is not None:
                    failures.append(check)
panel_checks=[]
panel_failures=[]
for panel in placement['panels']:
    a,b=(ns['point'](*panel[k]) for k in ('from','to'))
    middle=(a+b)/2
    up=-middle.normalized()
    forward=ns['BITANGENT']-up*ns['BITANGENT'].dot(up)
    forward.normalize()
    right=up.cross(forward).normalized()
    start_rotation=up.rotation_difference(-a.normalized())
    end_rotation=(-a.normalized()).rotation_difference(-b.normalized()) @ start_rotation
    width=panel.get('width_cm',1200)
    for step in range(101):
        t=step/100
        location=a.lerp(b,t)
        rotation=start_rotation.slerp(end_rotation,t)
        panel_up=rotation@up
        error=math.degrees(panel_up.angle(-location.normalized()))
        for x in (-width/2+80,0,width/2-80):
            for y in (-width/2+80,0,width/2-80):
                stand=location+(rotation@forward)*x+(rotation@right)*y
                start=ns['to_blender'](stand+panel_up*35)
                end=ns['to_blender'](stand+panel_up*235)
                delta=end-start
                hit=tree.ray_cast(start,delta.normalized(),delta.length)[0]
                check={'panel':panel['id'],'t':t,'offset':[x,y],'up_error_deg':error,'blocked':hit is not None}
                panel_checks.append(check)
                if hit is not None or error>1:
                    panel_failures.append(check)
(OUT/'placement.json').write_text(json.dumps(placement,indent=2),encoding='utf-8')
report={'floor_normals':faces,'pod_floor_rays':len(placement['pods'])*33,
        'min_pod_distance_cm':min((ns['point'](*a['local'])-ns['point'](*b['local'])).length
                                  for i,a in enumerate(placement['pods']) for b in placement['pods'][i+1:]),
        'retired_pod_candidates':retired,
        'ramp_floor_tracks':len(ramp_checks),'ramp_failures':failures,
        'panel_standing_rays':len(panel_checks),'panel_failures':panel_failures,
        'max_panel_up_error_deg':max(c['up_error_deg'] for c in panel_checks),
        'max_ramp_slope_deg':max(c['slope_deg'] for c in ramp_checks),
        'limits':['BVH rays are not Chaos capsule sweeps or Mover traversal']}
(OUT/'geometry-verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('PYRAMID_RAMP_CHECKS '+json.dumps({'checks':len(ramp_checks),'failures':len(failures),'first':failures[:2]}),flush=True)
print('PYRAMID_PANEL_CHECKS '+json.dumps({'checks':len(panel_checks),'failures':len(panel_failures),'first':panel_failures[:2],
                                       'max_up_error_deg':report['max_panel_up_error_deg']}),flush=True)
assert not failures and not panel_failures,'Source traversal samples failed'
bm.free()
# 완성 건물/파손 건물의 같은 높이 단면을 나란히 기록한다.
def section_lines(obj,h):
    lines=[]
    obj.data.calc_loop_triangles()
    for triangle in obj.data.loop_triangles:
        vertices=[obj.data.vertices[i].co for i in triangle.vertices]
        points=[]
        for a,b in zip(vertices,vertices[1:]+vertices[:1]):
            if (a.z-h)*(b.z-h)<0:
                points.append(a.lerp(b,(h-a.z)/(b.z-a.z)))
        if len(points)==2:
            lines.append(points)
    return lines
svg=['<svg xmlns="http://www.w3.org/2000/svg" width="1400" height="4200" viewBox="0 0 1400 4200">',
     '<rect width="1400" height="4200" fill="#f4eee2"/>',
     '<g font-family="Malgun Gothic, sans-serif" fill="#28231d">',
     '<text x="40" y="34" font-size="24">피라미드 층별 단면 — 완성 건물 / 입체 파손 후</text>']
for i,h in enumerate(ns['LEVELS']+[12600]):
    row=65+i*290
    r=ns['radius'](h)
    scale=245/(r*1.75)
    layout=manifest['storey_layouts'][i] if i<len(ns['LEVELS']) else '최상부 복층 홀의 부분 중층'
    svg.append(f'<text x="30" y="{row+20}" font-size="18">{i+1}층 H={h/100:g}m · {layout}</text>')
    for column,name in enumerate(('IntactBuildingBeforeDestruction','UnifiedPyramid')):
        cx,cy=380+column*680,row+150
        for a,b in section_lines(scene.objects[name],h+250):
            svg.append(f'<path d="M {cx+a.x*scale:.2f} {cy-a.y*scale:.2f} L {cx+b.x*scale:.2f} {cy-b.y*scale:.2f}" stroke="#55432d" stroke-width="1" fill="none"/>')
        if column:
            for pod in placement['pods']:
                if pod['storey']==i:
                    x,y,_=pod['local']
                    svg.append(f'<circle cx="{cx+x*scale:.2f}" cy="{cy-y*scale:.2f}" r="4" fill="#217c99"/>')
            for anchor in placement['anchors']:
                if anchor.get('route_group') and anchor['storey']==i:
                    x,y,_=anchor['local']
                    color='#16926b' if anchor['region_kind']=='hall' else '#9b377b'
                    svg.append(f'<circle cx="{cx+x*scale:.2f}" cy="{cy-y*scale:.2f}" r="5" fill="{color}"/>')
svg.extend(['</g>','</svg>'])
(OUT/'FloorSections.svg').write_text('\n'.join(svg),encoding='utf-8')
if (OUT/'Overview.png').exists():
    print('PYRAMID_VERIFICATION_READY_EXISTING_GEOMETRY_PREVIEWS',flush=True)
    raise SystemExit(0)
materials=[]
for name,color in [('Floor',(0.32,0.23,0.13,1)),('Shell',(0.62,0.47,0.30,1)),('Ground',(0.14,0.18,0.15,1))]:
    mat=bpy.data.materials.new('Preview'+name)
    mat.diffuse_color=color
    mat.use_nodes=True
    mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=color
    mat.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value=0.85
    materials.append(mat)
for obj in scene.objects:
    if obj.type!='MESH':
        continue
    visible=obj.name.startswith(('SM_Pyramid_00_','SM_Octant_Pyramid_00_'))
    obj.hide_render=not visible
    if visible:
        obj.data.materials.clear()
        obj.data.materials.append(materials[2 if 'SM_Octant' in obj.name else 1 if obj.name.endswith('Shell') else 0])
world=bpy.data.worlds.new('PyramidPreviewWorld')
world.use_nodes=True
world.node_tree.nodes['Background'].inputs[0].default_value=(0.19,0.22,0.28,1)
world.node_tree.nodes['Background'].inputs[1].default_value=0.7
scene.world=world
for i,rotation in enumerate(((0.4,0.7,0.2),(2.0,-0.6,-1.2))):
    data=bpy.data.lights.new('PreviewSun'+str(i),'SUN')
    data.energy=3 if not i else 1.5
    light=bpy.data.objects.new(data.name,data)
    scene.collection.objects.link(light)
    light.rotation_euler=rotation
data=bpy.data.cameras.new('PyramidPreviewCamera')
camera=bpy.data.objects.new(data.name,data)
scene.collection.objects.link(camera)
scene.camera=camera
scene.render.engine='CYCLES'
scene.cycles.samples=12
scene.render.resolution_x=1280
scene.render.resolution_y=960
scene.render.resolution_percentage=100
camera.data.lens=24
# 닫힌 실내에서도 벽·문의 연결을 검토할 수 있도록 카메라에 검토용 조명을 둔다.
fill_data=bpy.data.lights.new('PreviewInteriorFill','POINT')
fill_data.shadow_soft_size=2
fill=bpy.data.objects.new(fill_data.name,fill_data)
scene.collection.objects.link(fill)
for name,local,focus,hide_shell in [('Overview',(10000,-26000,18000),(0,0,3000),False),
                                  ('Cutaway',(2000,-15000,18000),(0,0,3000),True),
                                  ('Interior',(6000,2000,300),(2000,2000,500),False)]:
    camera.data.type='ORTHO' if name!='Interior' else 'PERSP'
    camera.data.ortho_scale=620
    camera.location=ns['to_blender'](ns['point'](*local)) if name=='Interior' else ns['to_blender'](
        ns['RADIAL']*(-8000)+ns['BITANGENT']*22000+ns['TANGENT']*(-35000))
    target=ns['to_blender'](ns['point'](*focus))
    camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
    fill.location=camera.location
    fill_data.energy=50000 if name=='Interior' else 0
    scene.objects['SM_Pyramid_00_Shell'].hide_render=hide_shell
    scene.objects['SM_Octant_Pyramid_00_R30000'].hide_render=hide_shell
    scene.render.filepath=str(OUT/(name+'.png'))
    bpy.ops.render.render(write_still=True)
print('PYRAMID_PREVIEW_READY',flush=True)
for storey in (0,4,6,8,12):
    for obj in scene.objects:
        if obj.type=='MESH':
            obj.hide_render=not (obj.name==f'Floor_{storey}' or any(obj.name.startswith(f'{kind}_{storey}_')
                for kind in ('WingWall','RoomBoundary','SuiteReturn','SuiteBack','Column')))
            if not obj.hide_render:
                obj.data.materials.clear()
                obj.data.materials.append(materials[0 if obj.name.startswith('Floor_') else 1])
    camera.data.type='ORTHO'
    camera.data.clip_end=100000
    camera.data.ortho_scale=ns['radius'](ns['LEVELS'][storey])*2.15
    camera.location=(0,0,ns['LEVELS'][storey]+30000)
    camera.rotation_euler=(0,0,0)
    scene.render.filepath=str(OUT/f'Storey{storey}Plan.png')
    bpy.ops.render.render(write_still=True)
print('PYRAMID_FLOOR_PLANS_READY',flush=True)
