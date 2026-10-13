"""Pocket_05 신고 지점의 열린 측면을 이전 원본과 비교하고 엔진 검사 광선을 기록한다."""

import hashlib
import json
import struct
from pathlib import Path

import bpy
from mathutils import Matrix,Vector
from mathutils.bvhtree import BVHTree


ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Saved/SliceOctant/Acropolis_00/V015'
RADIAL=Vector((1,1,1)).normalized()
BITANGENT=Vector((-1,-1,2)).normalized()
TANGENT=Vector((-1,1,0)).normalized()


def point(x,y,h):
    return (30000*RADIAL+x*BITANGENT+y*TANGENT).normalized()*(30000-h)


def bl(p):
    return Vector((p.x/100,-p.y/100,p.z/100))


def tree():
    vertices,faces=[],[]
    for o in bpy.context.scene.objects:
        if o.type!='MESH':
            continue
        n=len(vertices)
        vertices.extend(v.co.copy() for v in o.data.vertices)
        faces.extend(tuple(n+i for i in f.vertices) for f in o.data.polygons)
    return BVHTree.FromPolygons(vertices,faces)


def fingerprints():
    return {o.name:hashlib.sha256(b''.join(struct.pack('<fff',*p) for p in
                sorted(tuple(v.co) for v in o.data.vertices))).hexdigest()
            for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('SM_Octant')}


previous=ROOT/'Saved/SliceOctant/Acropolis_00/V009/Acropolis_00_V009.blend'
if not previous.exists():
    previous=ROOT/'Art/Octants/Acropolis_00/Acropolis_00_V009.blend'
bpy.ops.wm.open_mainfile(filepath=str(previous))
old_tree,old_fingerprints=tree(),fingerprints()
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'Art/Octants/Acropolis_00/Acropolis_00_V015.blend'))
new_tree=tree()
assert fingerprints()==old_fingerprints,'Unrelated authored geometry changed'
diagnosis=json.loads((OUT/'gap-before.json').read_text(encoding='utf-8'))
manifest=json.loads((OUT/'manifest.json').read_text(encoding='utf-8'))
outline=next(p['outline_xy'] for p in manifest['pockets'] if p['id']=='Pocket_05')


def inside(xy):
    x,y=xy
    result=False
    for a,b in zip(outline,outline[1:]+outline[:1]):
        if (a[1]>y)!=(b[1]>y) and x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:
            result=not result
    return result


probes=[]
for edge in diagnosis['boundary_edges_near_report']:
    a,b=Vector(edge['a'][:2]),Vector(edge['b'][:2])
    normal=Vector((b.y-a.y,a.x-b.x)).normalized()
    for t in (0.1,0.5,0.9):
        xy=a.lerp(b,t)
        top=edge['a'][2]+t*(edge['b'][2]-edge['a'][2])
        if top<-3480:
            continue
        for fraction in (0.15,0.5,0.85):
            h=-3500+(top+3500)*fraction
            start_xy,end_xy=xy-normal*200,xy+normal*200
            if not inside(start_xy):
                start_xy,end_xy=end_xy,start_xy
            assert inside(start_xy) and not inside(end_xy)
            # 엔진의 단면 충돌과 같은 방향: 지붕 위에서 열린 지각 측면을 바라본다.
            start,end=point(*start_xy,h),point(*end_xy,h)
            delta=bl(end)-bl(start)
            old=old_tree.ray_cast(bl(start),delta.normalized(),delta.length)
            new=new_tree.ray_cast(bl(start),delta.normalized(),delta.length)
            probes.append({'start':list(start),'end':list(end),'previous_miss':old[0] is None,
                           'new_miss':new[0] is None,'nearest_cm':new_tree.find_nearest(bl((start+end)/2))[3]*100})
assert probes and any(p['previous_miss'] for p in probes),'Original gap was not reproduced'
report={'reported_local':[6567,-12668,-3402],'unchanged_authored_meshes':len(old_fingerprints),
        'previous_visibility_misses':sum(p['previous_miss'] for p in probes),
        'fixed_visibility_probes':len(probes),'probes':probes}
report['remaining_misses']=sum(p['new_miss'] for p in probes)
(OUT/'gap-verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
scene=bpy.context.scene;scene.render.engine='BLENDER_WORKBENCH'
scene.display.shading.light='STUDIO';scene.display.shading.color_type='OBJECT'
scene.display.shading.show_cavity=True;scene.display.shading.cavity_type='BOTH'
scene.display.shading.show_shadows=True
for o in scene.objects:
    if o.type=='MESH':
        o.color=(.2,.28,.27,1) if o.name.startswith('SM_Octant') else (.6,.45,.3,1)
scene.world=bpy.data.worlds.new('GapWorld');scene.world.color=(.003,.003,.003)
scene.display.shading.background_type='WORLD'
camera=bpy.data.objects.new('GapCamera',bpy.data.cameras.new('GapCamera'))
scene.collection.objects.link(camera);scene.camera=camera
eye=point(6400,-12300,-3000);target=point(6580,-12680,-3500)
camera.location=bl(eye);forward=(bl(target)-camera.location).normalized()
up=bl(-eye.normalized()).normalized();right=forward.cross(up).normalized();up=right.cross(forward).normalized()
camera.rotation_euler=Matrix((right,up,-forward)).transposed().to_euler()
camera.data.lens=28;camera.data.clip_end=1000
scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.render.filepath=str(OUT/'GapAfter.png');bpy.ops.render.render(write_still=True)
print('ACROPOLIS_GAP_VERIFIED',json.dumps({k:v for k,v in report.items() if k!='probes'}))
assert not report['remaining_misses'],[p for p in probes if p['new_miss']][:3]
