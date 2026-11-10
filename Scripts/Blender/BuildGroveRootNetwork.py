"""나무만/큰 덩어리/절삭 후를 각각 생성한다. 이전 뿌리 형상은 호출하지 않는다."""
import json
import math
import sys
from pathlib import Path
import bpy
from mathutils import Matrix, Vector
from mathutils.geometry import delaunay_2d_cdt
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Scripts'))
sys.path.insert(0,str(ROOT/'Scripts/Blender'))
import GroveRootNetworkSpec as spec
import BuildGroveOctantV002 as geo

STAGE=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'VariedRoots'
assert STAGE in ('BareTrees','SolidRoots','VariedRoots')
FINAL=ROOT/'Saved/SliceOctant/Grove_00'/spec.VERSION
OUT=FINAL if STAGE=='VariedRoots' else FINAL/STAGE
ART=ROOT/'Art/Octants/Grove_00'
geo.spec=spec
geo.VERSION=spec.VERSION
geo.OUT=OUT
geo.EXPORTS=[]
geo.SOURCES=[]

def keep(obj,name):
    copy=obj.copy()
    copy.data=obj.data.copy()
    bpy.context.scene.collection.objects.link(copy)
    copy.name=name
    copy.hide_render=True
    copy.hide_set(True)
    return copy

def tree_body(tree):
    x,y=tree['xy']
    base=spec.ground(x,y)
    height=tree['height']
    sections=[-180,0,1500,height*.25,height*.4,height*.6]
    vertices=[]
    n=spec.SEGMENTS
    bottom=min(spec.ground(x,y),*(spec.ground(x+tree['radius']*math.cos(i*math.tau/n),y+tree['radius']*math.sin(i*math.tau/n)) for i in range(n)))-500
    for h in sections:
        radius=tree['radius']*(1-.35*max(0,h)/height)
        sway=math.sin(max(h,0)/height*2)*tree['radius']*.1
        for i in range(n):
            a=i*math.tau/n
            xx,yy=x+radius*math.cos(a)+sway,y+radius*math.sin(a)
            fit=(spec.ground(xx,yy)-base)*max(0,1-max(h,0)/1800)
            vertices.append((xx,yy,bottom if h<0 else base+h+fit))
    faces=[tuple(reversed(range(n))),tuple(range(len(vertices)-n,len(vertices)))]
    faces.extend((r*n+i,r*n+(i+1)%n,(r+1)*n+(i+1)%n,(r+1)*n+i) for r in range(len(sections)-1) for i in range(n))
    body=geo.mesh('Trunk_'+tree['id'],vertices,faces)
    for branch,direction in enumerate((tree['fork'],tree['fork']+145,tree['fork']+245)):
        a=math.radians(direction)
        reach=tree['radius']*(1.8 if branch==0 else 1.5)
        maximum=height*(1 if branch==0 else .91 if branch==1 else .82)
        limb=geo.loft('Limb_'+tree['id'],[(x,y,base+height*.38,tree['radius']*.62),
            (x+reach*.55*math.cos(a),y+reach*.55*math.sin(a),base+height*.68,tree['radius']*.42),
            (x+reach*math.cos(a),y+reach*math.sin(a),base+maximum,tree['radius']*.17)],12)
        geo.boolean(body,limb,'UNION')
    return body

def root_body(root,varied):
    length=spec.basis(root)[3]
    knots=[0,1]
    if varied and root['feature']=='ZRamp':
        knots=[0,.23,.25,.30,.70,.82,.84,1]
    samples=[]
    for left,right in zip(knots,knots[1:]):
        count=max(1,math.ceil((right-left)*length/300))
        samples.extend(left+(right-left)*i/count for i in range(count))
    samples.append(1)
    vertices=[]
    for t in samples:
        half=root['base_width']/2
        across=max(2,math.ceil(root['base_width']/250))
        bottom=min(spec.ground(*spec.root_xy(root,t,-half+2*half*i/across)) for i in range(across+1))-400
        top=root['top_width']/2 if varied else half
        lean=root['lean'] if varied else 0
        if varied and root['feature']=='ZRamp':
            if t<.25:
                u=spec.smooth((t-.23)/.02)
                lower=upper=1-u*.5
            elif t<.30:
                lower=upper=.5
            elif t<.70:
                u=(t-.30)/.40
                lower=.5*(1-u)
                upper=.5+.5*u
            else:
                lower=spec.smooth((t-.82)/.02)
                upper=1
            profile=[(-half,-1),(half,-1),(half,lower),(1000,lower),(1000,upper),(350,upper),(350,1),(-half,1)]
        elif varied and (top<half or lean!=0):
            profile=[(-half,-1),(half,-1),(half,0),(top+lean,1),(-top+lean,1),(-half,0)]
        else:
            profile=[(-half,-1),(half,-1),(top+lean,1),(-top+lean,1)]
        for lateral,fraction in profile:
            xy=spec.root_xy(root,t,lateral)
            h=bottom if fraction<0 else spec.cut_height(root,t,lateral,fraction)
            vertices.append((*xy,h))
    n=len(profile)
    faces=[tuple(reversed(range(n))),tuple(range(len(vertices)-n,len(vertices)))]
    faces.extend((r*n+i,r*n+(i+1)%n,(r+1)*n+(i+1)%n,(r+1)*n+i) for r in range(len(samples)-1) for i in range(n))
    return geo.mesh('Root_'+root['id'],vertices,faces)

def cut_volume(root,name,t0,t1,bottom,top):
    length=spec.basis(root)[3]
    count=max(1,math.ceil((t1-t0)*length/200))
    width=root['base_width']/2+200
    across=max(2,math.ceil(width*2/300))
    vertices=[]
    for layer in (0,1):
        for i in range(count+1):
            t=t0+(t1-t0)*i/count
            for j in range(across+1):
                lateral=-width+2*width*j/across
                xy=spec.root_xy(root,t,lateral)
                h=spec.ground(*xy)+(top(t) if layer else bottom(t))
                vertices.append((*xy,h))
    row=across+1
    size=row*(count+1)
    faces=[]
    for i in range(count):
        for j in range(across):
            a=i*row+j
            faces.extend(((a,a+row,a+row+1,a+1),(a+size,a+size+1,a+row+size+1,a+row+size)))
    border=list(range(row))+[i*row+across for i in range(1,count+1)]+list(range(count*row+across-1,count*row-1,-1))+[i*row for i in range(count-1,0,-1)]
    faces.extend((a,b,b+size,a+size) for a,b in zip(border,border[1:]+border[:1]))
    return geo.mesh(name,vertices,faces)

def vary(root,body):
    for index,(center,width,height) in enumerate(root.get('tunnels',[])):
        half=width/spec.basis(root)[3]/2
        left,right=center-half,center+half
        def ceiling(t):
            u=min((t-left)/(right-left),(right-t)/(right-left))*2
            return height*(.65+.35*min(1,u*2))
        cutter=cut_volume(root,'Tunnel_'+root['id'],left,right,lambda t:10,ceiling)
        keep(cutter,'Construction_Tunnel_'+root['id']+'_'+str(index))
        geo.boolean(body,cutter,'DIFFERENCE')
    for index,(center,width) in enumerate(root.get('notches',[])):
        half=width/spec.basis(root)[3]/2
        cutter=cut_volume(root,'Breach_'+root['id'],center-half,center+half,lambda t:10,lambda t:root['height']+4000)
        keep(cutter,'Construction_Breach_'+root['id']+'_'+str(index))
        geo.boolean(body,cutter,'DIFFERENCE')
    if root['feature']=='ChippedArch':
        a=spec.root_xy(root,.30,400)
        b=spec.root_xy(root,.42,400)
        c=spec.root_xy(root,.42,root['base_width']/2+200)
        d=spec.root_xy(root,.30,root['base_width']/2+200)
        h=min(spec.root_top(root,.30),spec.root_top(root,.42))-400
        cutter=geo.prism('ChippedCrown',[a,b,c,d],h,h+4500)
        keep(cutter,'Construction_ChippedCrown')
        geo.boolean(body,cutter,'DIFFERENCE')
    return body

def circle(xy,radius,count=16):
    return [[xy[0]+radius*math.cos(i*math.tau/count),xy[1]+radius*math.sin(i*math.tau/count)] for i in range(count)]

def crust_ground_sampler():
    bpy.ops.import_scene.fbx(filepath=str(FINAL.parent/'Baseline/MeadowSeam.fbx'))
    obj=next(o for o in bpy.context.selected_objects if o.type=='MESH')
    obj.data.calc_loop_triangles();positions=[]
    radial=Vector((1,1,1)).normalized();u=Vector((-1,-1,2)).normalized();v=Vector((-1,1,0)).normalized()
    for vertex in obj.data.vertices:
        p=geo.ue(obj.matrix_world@vertex.co);depth=p.dot(radial)
        positions.append(Vector((spec.RADIUS*p.dot(u)/depth,spec.RADIUS*p.dot(v)/depth,spec.RADIUS-p.length)))
    tree=BVHTree.FromPolygons(positions,[tuple(t.vertices) for t in obj.data.loop_triangles],all_triangles=True)
    bpy.data.objects.remove(obj,do_unlink=True)
    def sample(x,y):
        hit=tree.ray_cast(Vector((x,y,3000)),Vector((0,0,-1)),8000)
        return geo.surface_height(x,y,hit[0].z if hit[0] is not None else 0)+10
    return sample

def terrain_patch(name,outlines,bottom=None,top=None,features=()):
    # 상부 지각부터 지하 바닥 밑까지 한 외피로 소유하고, 그 안을 굴착한다.
    points=[];edges=[];regions=[]
    for outline in outlines:
        start=len(points)
        for a,b in zip(outline,outline[1:]+outline[:1]):
            count=math.ceil(math.dist(a,b)/250)
            points.extend(Vector((a[0]+(b[0]-a[0])*i/count,a[1]+(b[1]-a[1])*i/count)) for i in range(count))
        count=len(points)-start
        edges.extend((start+i,start+(i+1)%count) for i in range(count))
        regions.append(list(range(start,start+count)))
    for feature in features:
        start=len(points);points.extend(Vector(p) for p in feature)
        edges.extend((start+i,start+i+1) for i in range(len(feature)-1))
    boundary=[p for outline in outlines for p in outline]
    for x in range(math.floor(min(p[0] for p in boundary)/250)*250,math.ceil(max(p[0] for p in boundary)/250)*250+1,250):
        for y in range(math.floor(min(p[1] for p in boundary)/250)*250,math.ceil(max(p[1] for p in boundary)/250)*250+1,250):
            if any(geo.inside(x,y,p) for p in outlines):points.append(Vector((x,y)))
    verts,_,triangles,_,_,_=delaunay_2d_cdt(points,edges,regions,1,.1)
    bottom=bottom or (lambda x,y:spec.UNDERGROUND['floor']-500)
    top=top or (lambda x,y:spec.ground(x,y)+10)
    vertices=[(*p,bottom(*p)) for p in verts]+[(*p,top(*p)) for p in verts]
    n=len(verts);faces=[];boundary={}
    for tri in triangles:
        faces.extend((tuple(reversed(tri)),tuple(i+n for i in tri)))
        for a,b in zip(tri,tri[1:]+tri[:1]):
            key=tuple(sorted((a,b)));boundary[key]=boundary.get(key,0)+1
    faces.extend((a,b,b+n,a+n) for (a,b),uses in boundary.items() if uses==1)
    return geo.mesh(name,vertices,faces)

def cave_void():
    outlines=[circle(spec.UNDERGROUND['xy'],spec.UNDERGROUND['radius'])]
    for entrance in spec.UNDERGROUND['entrances']:
        outlines.append(spec.cave_outline(entrance,800,0))
        if entrance['type']=='Shaft':outlines.append(circle(entrance['xy'],1300))
    def bounds(x,y):
        floors=[];roofs=[]
        if math.dist((x,y),spec.UNDERGROUND['xy'])<=spec.UNDERGROUND['radius']+1:
            floors.append(spec.UNDERGROUND['floor']);roofs.append(spec.cavern_roof(x,y))
        for entrance in spec.UNDERGROUND['entrances']:
            distance,t=spec.cave_nearest(entrance,x,y)
            if distance<=801:
                floors.append(spec.underground_floor(entrance,t));roofs.append(spec.underground_roof(entrance,t))
            if entrance['type']=='Shaft' and math.dist((x,y),entrance['xy'])<=1301:
                floors.append(entrance.get('floor',spec.UNDERGROUND['floor'])-2);roofs.append(spec.ground(x,y)+1200)
        assert floors,(x,y)
        return min(floors),max(roofs)
    features=[]
    for entrance in spec.UNDERGROUND['entrances']:
        distances=([spec.SHAFT_FLAT_LENGTH,entrance['length']-spec.UNDERGROUND['radius']-spec.SHAFT_JOIN_MARGIN] if entrance['type']=='Shaft' else [entrance['length']-spec.UNDERGROUND['radius']-800])
        for distance in distances:
            t=distance/entrance['length'];x,y=spec.underground_xy(entrance,t)
            a,b=spec.underground_xy(entrance,t-.0001),spec.underground_xy(entrance,t+.0001)
            dx,dy=b[0]-a[0],b[1]-a[1];length=math.hypot(dx,dy)
            features.append([[x-dy/length*side,y+dx/length*side] for side in (-800,0,800)])
    void=terrain_patch('UndergroundVoid',outlines,lambda x,y:bounds(x,y)[0],lambda x,y:bounds(x,y)[1],features)
    # 착지턱과 엄폐물 부피를 굴착에서 제외해 지각/공동과 한 외피로 남긴다.
    for entrance in spec.UNDERGROUND['entrances']:
        if entrance['type']!='Shaft':continue
        u,v=entrance['axis'];x,y=entrance['xy']
        lip=[[x+u*along-v*side,y+v*along+u*side] for along,side in ((950,-450),(1800,-450),(1800,450),(950,450))]
        body=terrain_patch('ShaftLip_'+entrance['id'],[lip],lambda x,y:spec.ground(x,y)-500)
        keep(body,'Construction_'+body.name);geo.boolean(void,body,'DIFFERENCE')
        xx,yy,h=spec.shaft_point(entrance,-900,-5500)
        body=geo.loft('ShaftLedge_'+entrance['id'],[(xx,yy,h-600,400),(xx,yy,h-200,800),(xx,yy,h,800)],16)
        keep(body,'Construction_'+body.name);geo.boolean(void,body,'DIFFERENCE')
    for cover in spec.CAVE_COVER:
        x,y=cover['xy'];r=cover['radius'];h=spec.UNDERGROUND['floor']
        body=geo.loft('CaveCover_'+cover['id'],[(x,y,h-300,r),(x,y,h,r),(x,y,h+cover['height'],r if cover['type']=='Pillar' else r*.65)],12 if cover['type']=='Pillar' else 8)
        keep(body,'Construction_'+body.name);geo.boolean(void,body,'DIFFERENCE')
    return void

def crown_deck(space):
    x,y,h=space['local'];r=space['radius']
    # 넓은 착지 상면과 아래로 좁아지는 단순 가지 덩어리다.
    body=geo.loft('Crown_'+space['id'],[(x,y,h-700,r*.48),(x,y,h-220,r),(x,y,h,r)],16)
    if not space['id'].endswith('Summit'):
        ramp=next((r for r in spec.BRANCH_RAMPS if r['id']==space['id']),None)
        if ramp:
            vertices=[]
            count=math.ceil(math.dist(ramp['start'],ramp['end'])/250)
            samples=sorted({i/count for i in range(count+1)}|{ramp['crest']})
            for t in samples:
                for lateral,z in ((-650,-650),(650,-650),(650,0),(-650,0)):
                    xx,yy,hh=spec.branch_ramp_point(ramp,t,lateral)
                    vertices.append((xx,yy,hh+z))
            faces=[(3,2,1,0),tuple(range(len(vertices)-4,len(vertices)))]
            faces.extend((i*4+j,i*4+(j+1)%4,(i+1)*4+(j+1)%4,(i+1)*4+j) for i in range(len(samples)-1) for j in range(4))
            geo.boolean(body,geo.mesh('BranchRamp_'+space['id'],vertices,faces),'UNION')
            return body
        tree=spec.TREE_BY_ID[space['tree']];tx,ty=tree['xy']
        dx,dy=x-tx,y-ty;length=math.hypot(dx,dy);side=(-dy/length*650,dx/length*650)
        start=[tx+dx/length*tree['radius']*.25,ty+dy/length*tree['radius']*.25]
        end=[x-dx/length*r*.5,y-dy/length*r*.5]
        polygon=[[p[0]+s*side[0],p[1]+s*side[1]] for p,s in ((start,-1),(end,-1),(end,1),(start,1))]
        geo.boolean(body,geo.prism('BranchNeck_'+space['id'],polygon,h-650,h),'UNION')
    return body

def render():
    scene=bpy.context.scene
    meshes=[o for o in scene.objects if o.type=='MESH' and o.name.startswith('SM_')]
    for o in scene.objects:o.hide_render=o not in meshes
    for o in meshes:o.color=(.28,.38,.23,1) if 'Octant' in o.name else (.40,.29,.20,1)
    scene.render.engine='BLENDER_WORKBENCH'
    scene.display.shading.light='STUDIO'
    scene.display.shading.color_type='OBJECT'
    scene.display.shading.show_shadows=True
    scene.display.shading.show_cavity=True
    scene.display.shading.cavity_type='BOTH'
    scene.display.shading.background_type='WORLD'
    scene.world.color=(.16,.19,.22)
    scene.render.resolution_x=1600;scene.render.resolution_y=1100;scene.render.resolution_percentage=100
    camera=bpy.data.objects.new('ReviewCamera',bpy.data.cameras.new('ReviewCamera'))
    scene.collection.objects.link(camera);scene.camera=camera;camera.data.clip_end=2000
    views=[('Overview',[0,-8000,24000],[1000,-4000,2000],470),
        ('GroundA',[-12500,-2500,-700],[-6500,500,500],0),
        ('RootScale',[1000,-12000,-700],[3500,-4000,1600],0)]
    if STAGE=='VariedRoots':
        views.extend([('Vault',[-13900,2400,spec.ground(-13900,2400)+280],[*spec.VAULT_XY,spec.ground(*spec.VAULT_XY)+400],0),
            ('Crown',[9200,6000,3500],[8750,0,1700],0),
            ('ZCarve',[-12000,-6500,2500],[-10000,-12000,0],110),
            ('Underground',[-12500,-1400,-9100],[-8000,2200,-7600],0),
            ('UndergroundHigh',[-11500,-1800,-7600],[-7000,2200,-7000],0),
            ('Vertical',[-6500,-13000,13000],[1000,2000,7500],230),
            ('Summit',[18500,11000,18000],[18500,4500,16000],0)])
        for entrance in spec.UNDERGROUND['entrances']:
            if entrance['type']=='Shaft':
                eye=spec.shaft_point(entrance,3000,entrance['h']+2500)
                views.append((entrance['id']+'Shaft',eye,[*entrance['xy'],-5500],0))
        for ramp in spec.BRANCH_RAMPS:
            p=spec.branch_ramp_point(ramp,.5);a,b=ramp['start'],ramp['end']
            dx,dy=b[0]-a[0],b[1]-a[1];length=math.hypot(dx,dy)
            eye=[p[0]-dy/length*6500+dx/length*1500,p[1]+dx/length*6500+dy/length*1500,p[2]+3500]
            views.append((ramp['tree']+'BranchRamp',eye,p,95))
    for name,eye,target,scale in views:
        p=geo.bl(spec.point(*eye));aim=geo.bl(spec.point(*target))
        forward=(aim-p).normalized();up=-p.normalized();right=forward.cross(up).normalized();up=right.cross(forward).normalized()
        camera.location=p;camera.rotation_euler=Matrix((right,up,-forward)).transposed().to_euler()
        camera.data.type='ORTHO' if scale else 'PERSP';camera.data.ortho_scale=scale or 300;camera.data.lens=22
        scene.render.filepath=str(OUT/(name+'.png'));bpy.ops.render.render(write_still=True)

def build():
    assert getattr(spec,'GEOMETRY_VERSION',spec.VERSION)==spec.VERSION,'Frozen geometry: use UpdateGroveAnchors.py'
    assert not (OUT/'manifest.json').exists(),'Choose a new version'
    OUT.mkdir(parents=True,exist_ok=True);ART.mkdir(parents=True,exist_ok=True)
    solids=[tree_body(tree) for tree in spec.TREES]
    if STAGE!='BareTrees':
        for root in spec.ROOTS:
            solid=root_body(root,False)
            keep(solid,'Construction_Solid_'+root['id'])
            if STAGE=='VariedRoots':
                bpy.data.objects.remove(solid,do_unlink=True)
                solid=vary(root,root_body(root,True))
            solids.append(solid)
    forest=solids[0]
    for solid in solids[1:]:geo.boolean(forest,solid,'UNION')
    underground_outlines=[]
    if STAGE=='VariedRoots':
        for space in spec.CROWN_SPACES:
            print('GROVE_ADD_CROWN',space['id'],flush=True)
            geo.boolean(forest,crown_deck(space),'UNION')
        underground_outlines=[circle(spec.UNDERGROUND['xy'],spec.UNDERGROUND['shell_radius'])]
        underground_outlines.extend(spec.cave_outline(e) for e in spec.UNDERGROUND['entrances'])
        underground_outlines.extend(circle(e['xy'],1800) for e in spec.UNDERGROUND['entrances'] if e['type']=='Shaft')
        print('GROVE_ADD_UNDERGROUND',flush=True)
        geo.boolean(forest,terrain_patch('UndergroundShell',underground_outlines,top=crust_ground_sampler()),'UNION')
        cutter=cave_void()
        keep(cutter,'Construction_UndergroundVoid')
        geo.boolean(forest,cutter,'DIFFERENCE')
    forest.name='FreshForestSource'
    parent=list(range(len(forest.data.vertices)))
    def find(i):
        while parent[i]!=i:
            parent[i]=parent[parent[i]];i=parent[i]
        return i
    for edge in forest.data.edges:
        a,b=(find(i) for i in edge.vertices);parent[a]=b
    components=len({find(v.index) for v in forest.data.vertices})
    if STAGE=='VariedRoots':assert components==1,('Disconnected forest geometry',components)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Unprojected.blend'),check_existing=False)
    geo.finish(forest,'Forest',True)
    cuts=[spec.tree_outline(t) for t in spec.TREES]
    if STAGE!='BareTrees':cuts.extend(spec.root_outline(r,2) for r in spec.ROOTS)
    if underground_outlines:
        # 최종 외피의 바깥 벽을 지각 아래에 매입한다. 지각 절단선은 외피보다 50cm 안쪽이다.
        cuts.append(circle(spec.UNDERGROUND['xy'],spec.UNDERGROUND['shell_radius']-50))
        cuts.extend(spec.cave_outline(e,950,450) for e in spec.UNDERGROUND['entrances'])
        cuts.extend(circle(e['xy'],1750) for e in spec.UNDERGROUND['entrances'] if e['type']=='Shaft')
    geo.crust(cuts,FINAL.parent/'Baseline/MeadowSeam.fbx')
    forest.hide_render=True;forest.hide_set(True)
    manifest={'name':'Grove_00','version':spec.VERSION,'stage':STAGE,'trees':spec.TREES,
        'roots':[] if STAGE=='BareTrees' else spec.ROOTS,'exports':geo.EXPORTS,'sources':geo.SOURCES,
        'legacy_root_geometry':False,'legacy_branch_or_vault_geometry':False,
        'underground':spec.UNDERGROUND if STAGE=='VariedRoots' else None,
        'cave_cover':spec.CAVE_COVER if STAGE=='VariedRoots' else [],
        'crown_spaces':spec.CROWN_SPACES if STAGE=='VariedRoots' else [],
        'branch_ramps':spec.BRANCH_RAMPS if STAGE=='VariedRoots' else [],'source_connected_components':components,
        'checkpoints':{s:str(FINAL.parent/spec.BASELINE_VERSION/s/'manifest.json') for s in ('BareTrees','SolidRoots')},
        'seam_margin_cm':spec.SEAM_MARGIN,'seam_transition_cm':spec.SEAM_BAND,
        'limits':['No engine collision, bake or actual play validation yet']}
    (OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    if STAGE=='VariedRoots':(OUT/'placement.json').write_text(json.dumps(spec.placement(),indent=2),encoding='utf-8')
    suffix='' if STAGE=='VariedRoots' else '_'+STAGE
    bpy.ops.wm.save_as_mainfile(filepath=str(ART/('Grove_00_'+spec.VERSION+suffix+'.blend')),check_existing=False)
    render()
    print('GROVE_FRESH_STAGE',STAGE,json.dumps({'meshes':len(geo.EXPORTS),'triangles':sum(e['triangles'] for e in geo.EXPORTS)}),flush=True)

if __name__=='__main__':build()
