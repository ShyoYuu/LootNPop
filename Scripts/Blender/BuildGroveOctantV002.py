"""큰 저폴리 외피·열린 외곽·지하/가지 공간. 실패한 V001 생성기는 보존한다."""
import json
import math
import sys
from pathlib import Path
import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import delaunay_2d_cdt

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"Scripts"))
import GroveOctantSpecV002 as spec
VERSION=spec.VERSION
OUT=ROOT/"Saved/SliceOctant/Grove_00"/VERSION
ART=ROOT/"Art/Octants/Grove_00"
EXPORTS=[]
SOURCES=[]
N=spec.SEGMENTS

def bl(p):
    return Vector((p[0]/100,-p[1]/100,p[2]/100))

def ue(p):
    return Vector((p.x*100,-p.y*100,p.z*100))

def mesh(name,vertices,faces,recalc=True):
    data=bpy.data.meshes.new(name)
    data.from_pydata(vertices,[],faces)
    obj=bpy.data.objects.new(name,data)
    bpy.context.scene.collection.objects.link(obj)
    if recalc:
        bm=bmesh.new()
        bm.from_mesh(data)
        bmesh.ops.remove_doubles(bm,verts=bm.verts[:],dist=.0001)
        bmesh.ops.dissolve_degenerate(bm,edges=bm.edges[:],dist=.0001)
        bmesh.ops.recalc_face_normals(bm,faces=bm.faces[:])
        bmesh.ops.triangulate(bm,faces=bm.faces[:])
        bmesh.ops.dissolve_degenerate(bm,edges=bm.edges[:],dist=.0001)
        bm.to_mesh(data)
        bm.free()
    return obj

def stats(bm,closed=True):
    bm.verts.index_update()
    keys=[tuple(sorted(v.index for v in f.verts)) for f in bm.faces]
    result={"vertices":len(bm.verts),"triangles":len(bm.faces),"boundary_edges":sum(e.is_boundary for e in bm.edges),
        "non_manifold_non_boundary":sum(not e.is_manifold and not e.is_boundary for e in bm.edges),
        "inconsistent_edges":sum(e.is_manifold and not e.is_contiguous for e in bm.edges),
        "duplicate_faces":len(keys)-len(set(keys)),"degenerate_faces":sum(f.calc_area()<1e-9 for f in bm.faces)}
    assert not any(result[k] for k in ("non_manifold_non_boundary","inconsistent_edges","duplicate_faces","degenerate_faces")),result
    assert not closed or result["boundary_edges"]==0,result
    return result

def boolean(target,operand,operation):
    for obj in (target,operand):
        bm=bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.remove_doubles(bm,verts=bm.verts[:],dist=.02)
        bmesh.ops.dissolve_degenerate(bm,edges=bm.edges[:],dist=.02)
        stats(bm)
        bm.to_mesh(obj.data)
        bm.free()
    bpy.context.view_layer.objects.active=target
    modifier=target.modifiers.new(operation,"BOOLEAN")
    modifier.operation=operation
    modifier.solver="EXACT"
    modifier.object=operand
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    bpy.data.objects.remove(operand,do_unlink=True)

def prism(name,outline,bottom,top):
    n=len(outline)
    vertices=[(*p,h) for h in (bottom,top) for p in outline]
    faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]
    faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    return mesh(name,vertices,faces)

def box(name,bounds):
    x0,x1,y0,y1,z0,z1=bounds
    return prism(name,[(x0,y0),(x1,y0),(x1,y1),(x0,y1)],z0,z1)

def loft(name,sections,segments=16):
    vertices=[]
    for x,y,h,radius in sections:
        for i in range(segments):
            a=i*math.tau/segments
            r=radius*(1+.045*math.sin(i*5+len(sections)))
            vertices.append((x+r*math.cos(a),y+r*math.sin(a),h))
    faces=[tuple(reversed(range(segments))),tuple(range(len(vertices)-segments,len(vertices)))]
    for j in range(len(sections)-1):
        for i in range(segments):
            k=(i+1)%segments
            faces.append((j*segments+i,j*segments+k,(j+1)*segments+k,(j+1)*segments+i))
    return mesh(name,vertices,faces)

def tree_body(tree):
    x,y=tree["xy"]
    base=spec.ground(x,y)
    height=tree["height"]
    sections=[(-120,1),(0,1),(60,.78),(250,.42),(650,.12),(1500,0)]
    sections += [(height*t,0) for t in (.25,.40,.52,.60)]
    vertices=[]
    for h,root in sections:
        for i in range(N):
            a=i*math.tau/N
            r=tree["radius"]*(1-.35*max(0,h)/height)+(spec.footprint(tree,a)-tree["radius"])*root
            sway=math.sin(max(h,0)/height*2)*tree["radius"]*.1
            xx,yy=x+r*math.cos(a)+sway,y+r*math.sin(a)
            fitted=(spec.ground(xx,yy)-base)*max(0,1-max(h,0)/1800)
            vertices.append((xx,yy,base+h+fitted))
    faces=[tuple(reversed(range(N))),tuple(range(len(vertices)-N,len(vertices)))]
    for ring in range(len(sections)-1):
        for i in range(N):
            j=(i+1)%N
            faces.append((ring*N+i,ring*N+j,(ring+1)*N+j,(ring+1)*N+i))
    body=mesh(tree["id"],vertices,faces)
    for branch,direction in enumerate((tree["fork"],tree["fork"]+145,tree["fork"]+245)):
        a=math.radians(direction)
        reach=tree["radius"]*(1.8 if branch==0 else 1.5)
        maximum=height*(1 if branch==0 else .91 if branch==1 else .82)
        limb=loft(tree["id"]+"_Limb",[(x,y,base+height*.38,tree["radius"]*.62),
            (x+reach*.55*math.cos(a),y+reach*.55*math.sin(a),base+height*.68,tree["radius"]*.42),
            (x+reach*math.cos(a),y+reach*math.sin(a),base+maximum,tree["radius"]*.17)],12)
        boolean(body,limb,"UNION")
    return body

def root_wall(wall):
    if wall.get("style")=="Switchback":
        return switchback_wall(wall)
    a,b=Vector(wall["from"]),Vector(wall["to"])
    delta=b-a
    side=Vector((-delta.y,delta.x)).normalized()
    count=math.ceil(delta.length/400)
    vertices=[]
    for i in range(count+1):
        t=i/count
        center=a.lerp(b,t)
        height=max(10,wall["height"]*min(1,max(0,(1-t)/.25)))
        half=wall["ramp"]+250
        left,right=center-half*side,center+half*side
        terrain=[spec.ground(*left),spec.ground(*right)]
        # 양쪽 지면의 높이 차이까지 포함해 경사로의 횡단 경사를 제한한다.
        crest=max(max(terrain)-wall["ramp"]*.4,
                  min(spec.ground(*center)+height,min(terrain)+wall["ramp"]*.4))
        for lateral,h in ((-half,0),(-250,height),(250,height),(half,0),(half,-120),(-half,-120)):
            xy=center+lateral*side
            z=spec.ground(*xy)+h
            if abs(lateral)==250:
                z=spec.ground(*xy)+10 if i==count else crest
            vertices.append((*xy,z))
    faces=[tuple(reversed(range(6))),tuple(range(len(vertices)-6,len(vertices)))]
    for ring in range(count):
        for i in range(6):
            j=(i+1)%6
            faces.append((ring*6+i,ring*6+j,(ring+1)*6+j,(ring+1)*6+i))
    return mesh(wall["id"],vertices,faces)

def switchback_wall(wall):
    a,b=Vector(wall["from"]),Vector(wall["to"])
    side=Vector((-(b-a).y,(b-a).x)).normalized()
    height=wall["height"]
    # 하부는 바깥 띠, 상부는 벽에 붙은 안쪽 띠다. 상승하면서 벽 쪽으로 들어온다.
    ts=[0,.44,.47,.53,.96,1]
    samples=[]
    for t0,t1 in zip(ts,ts[1:]):
        count=max(1,math.ceil((b-a).length*(t1-t0)/300))
        samples.extend(t0+(t1-t0)*i/count for i in range(count))
    samples.append(1)
    vertices=[]
    for t in samples:
        if t<.47:
            u=max(0,(t-.44)/.03)
            lower=upper=10+(height*.5-10)*u
        elif t<.53:
            lower=upper=height*.5
        else:
            u=min(1,(t-.53)/.43)
            lower=height*.5*(1-u)+10*u
            upper=height*.5*(1-u)+height*u
        profile=[(-1900,-240),(1900,-240),(1900,-.5),(1670,-.5),
            (1670,lower),(950,lower),(950,upper),(250,upper),(250,height),
            (-250,height),(-250,upper),(-950,upper),(-950,lower),(-1670,lower),(-1670,-.5),(-1900,-.5)]
        for lateral,h in profile:
            xy=a.lerp(b,t)+lateral*side
            vertices.append((*xy,spec.ground(*xy)+h))
    n=len(profile)
    faces=[tuple(reversed(range(n))),tuple(range(len(vertices)-n,len(vertices)))]
    faces.extend((n*r+j,n*r+(j+1)%n,n*(r+1)+(j+1)%n,n*(r+1)+j) for r in range(len(samples)-1) for j in range(n))
    result=mesh(wall["id"],vertices,faces)
    return result

def connecting_wall(wall):
    a,b=Vector(wall["from"]),Vector(wall["to"])
    side=Vector((-(b-a).y,(b-a).x)).normalized()
    def volume(name,t0,t1,half,bottom,top,notches=False):
        count=max(1,math.ceil((b-a).length*(t1-t0)/200))
        samples={t0+(t1-t0)*i/count for i in range(count+1)}
        if notches:
            for left,right in wall["gates"]:
                samples.update((max(t0,left-.001),left,right,min(t1,right+.001)))
        vertices=[]
        for t in sorted(samples):
            ceiling=10 if notches and any(left<=t<=right for left,right in wall["gates"]) else top
            for lateral,h in ((-half,bottom),(half,bottom),(half,ceiling),(-half,ceiling)):
                xy=a.lerp(b,t)+lateral*side
                vertices.append((*xy,spec.ground(*xy)+h))
        faces=[(3,2,1,0),tuple(range(len(vertices)-4,len(vertices)))]
        faces.extend((4*r+j,4*r+(j+1)%4,4*(r+1)+(j+1)%4,4*(r+1)+j) for r in range(len(samples)-1) for j in range(4))
        return mesh(name,vertices,faces)
    result=volume(wall["id"],0,1,wall["half_width"],-120,wall["height"],wall["style"]=="Notch")
    if wall["style"]=="Arch":
        for left,right in wall["gates"]:
            boolean(result,volume("PassageCut",left,right,150,10,260),"DIFFERENCE")
    return result

def export(obj,role):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active=obj
    obj.data.calc_loop_triangles()
    positions=[ue(v.co) for v in obj.data.vertices]
    path=OUT/(obj.name+".fbx")
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={"MESH"},apply_scale_options="FBX_SCALE_NONE",global_scale=1,
        axis_forward="-Z",axis_up="Y",mesh_smooth_type="FACE",add_leaf_bones=False,bake_anim=False)
    EXPORTS.append({"name":obj.name,"role":role,"file":str(path),"triangles":len(obj.data.loop_triangles),
        "bounds_cm":{side:[function(v[i] for v in positions) for i in range(3)] for side,function in (("min",min),("max",max))}})

def finish(obj,name,forest=False):
    bm=bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.remove_doubles(bm,verts=bm.verts[:],dist=.1)
    bmesh.ops.dissolve_degenerate(bm,edges=bm.edges[:],dist=.1)
    bmesh.ops.triangulate(bm,faces=bm.faces[:])
    bmesh.ops.dissolve_degenerate(bm,edges=bm.edges[:],dist=.1)
    bmesh.ops.triangulate(bm,faces=bm.faces[:])
    stats(bm)
    assert all(len(f.verts)==3 for f in bm.faces),"Subdivision requires triangles"
    # 공유 변의 중점을 재사용해 부분 분할에서 중복 삼각형이 생기지 않게 한다.
    bm.verts.index_update()
    positions=[v.co.copy() for v in bm.verts]
    triangles=[tuple(v.index for v in f.verts) for f in bm.faces]
    for _ in range(9):
        selected={tuple(sorted((a,b))) for f in triangles for a,b in zip(f,f[1:]+f[:1]) if (positions[a]-positions[b]).length>600}
        if not selected:
            break
        middle={}
        for a,b in selected:
            middle[(a,b)]=len(positions)
            positions.append((positions[a]+positions[b])*.5)
        result=[]
        for f in triangles:
            flags=[tuple(sorted((f[i],f[(i+1)%3]))) in selected for i in range(3)]
            count=sum(flags)
            if count==0:
                result.append(f)
                continue
            start=flags.index(True) if count==1 else (flags.index(False)+1)%3 if count==2 else 0
            a,b,c=(f[(start+i)%3] for i in range(3))
            ab=middle.get(tuple(sorted((a,b))))
            bc=middle.get(tuple(sorted((b,c))))
            ca=middle.get(tuple(sorted((c,a))))
            if count==1:
                result.extend(((a,ab,c),(ab,b,c)))
            elif count==2:
                result.extend(((b,bc,ab),(a,ab,bc),(a,bc,c)))
            else:
                result.extend(((a,ab,ca),(ab,b,bc),(ca,bc,c),(ab,bc,ca)))
        triangles=result
    bm.clear()
    vertices=[bm.verts.new(bl(spec.point(*p))) for p in positions]
    for face in triangles:
        bm.faces.new([vertices[i] for i in face])
    # 공유 연결은 투영 전에 정리한다. 투영 뒤 가까워진 별개 정점을 다시 합치지 않는다.
    bmesh.ops.triangulate(bm,faces=bm.faces[:])
    bmesh.ops.recalc_face_normals(bm,faces=bm.faces[:])
    bm.normal_update()
    tiny=[f for f in bm.faces if f.calc_area()<1e-9]
    if tiny:
        bmesh.ops.dissolve_degenerate(bm,edges=list({e for f in tiny for e in f.edges}),dist=.00001)
        bmesh.ops.triangulate(bm,faces=bm.faces[:])
        bmesh.ops.recalc_face_normals(bm,faces=bm.faces[:])
        bm.normal_update()
    report=stats(bm)
    bm.verts.index_update()
    coordinates=[v.co.copy() for v in bm.verts]
    floor_faces=set()
    for f in bm.faces:
        center=f.calc_center_median()
        h=spec.RADIUS-ue(center).length
        p=ue(center)
        radial=Vector((1,1,1)).normalized()
        depth=p.dot(radial)
        x=30000*p.dot(Vector((-1,-1,2)).normalized())/depth
        y=30000*p.dot(Vector((-1,1,0)).normalized())/depth
        eligible=(not forest or spec.support_surface(x,y,h)) if hasattr(spec,"support_surface") else (not forest or h<spec.ground(x,y)+getattr(spec,"SUPPORT_HEIGHT",2100) or 2200<h<3000)
        if eligible and f.normal.dot(-center.normalized())>=.71:floor_faces.add(f)
    excluded=[]
    minimum=getattr(spec,"SUPPORT_MIN_PATCH_AREA_CM2",0) if forest else 0
    if minimum:
        remaining=set(floor_faces)
        while remaining:
            seed=remaining.pop();patch={seed};queue=[seed]
            while queue:
                face=queue.pop()
                for edge in face.edges:
                    for neighbor in edge.link_faces:
                        if neighbor in remaining:
                            remaining.remove(neighbor);patch.add(neighbor);queue.append(neighbor)
            area=sum(f.calc_area() for f in patch)*10000
            if area<minimum:
                floor_faces.difference_update(patch)
                excluded.append({"triangles":len(patch),"area_cm2":area})
    groups={"Floor":[],"Shell":[]}
    for f in bm.faces:
        groups["Floor" if f in floor_faces else "Shell"].append(tuple(v.index for v in f.verts))
    assert sum(map(len,groups.values()))==report["triangles"]
    bm.free()
    for role,faces in groups.items():
        if not faces:
            continue
        used=sorted({v for f in faces for v in f})
        indices={v:i for i,v in enumerate(used)}
        output=mesh(f"SM_Grove_00_{VERSION}_{name}_{role}",[coordinates[v] for v in used],
                    [tuple(indices[v] for v in f) for f in faces],False)
        export(output,role)
    SOURCES.append({"id":name,"topology":report,"roles":{k:len(v) for k,v in groups.items()},"excluded_tiny_support_patches":excluded})

def inside(x,y,polygon):
    result=False
    for a,b in zip(polygon,polygon[1:]+polygon[:1]):
        if (a[1]>y)!=(b[1]>y) and x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:
            result=not result
    return result

def surface_height(x,y,source_h):
    t=max(0,min(1,(min(spec.point(x,y,0))-spec.SEAM_MARGIN)/spec.SEAM_BAND))
    return source_h+(spec.ground(x,y)-source_h)*t*t*(3-2*t)

def crust(cuts,seam_path=None):
    bpy.ops.import_scene.fbx(filepath=str(seam_path or OUT.parent/"Baseline/MeadowSeam.fbx"))
    obj=next(o for o in bpy.context.selected_objects if o.type=="MESH")
    bm=bmesh.new()
    bm.from_mesh(obj.data)
    radial,tangent,bitangent=Vector((1,1,1)).normalized(),Vector((-1,1,0)).normalized(),Vector((-1,-1,2)).normalized()
    native={}
    for v in bm.verts:
        p=ue(obj.matrix_world@v.co)
        if min(p)<=spec.SEAM_MARGIN+.01:
            native[v]=p.copy()
        depth=p.dot(radial)
        v.co=Vector((spec.RADIUS*p.dot(bitangent)/depth,spec.RADIUS*p.dot(tangent)/depth,spec.RADIUS-p.length))
    bmesh.ops.remove_doubles(bm,verts=bm.verts[:],dist=.001)
    bmesh.ops.delete(bm,geom=[f for f in bm.faces if f.calc_area()<.0001],context="FACES")
    bm.verts.index_update()
    source_vertices=[v.co.copy() for v in bm.verts]
    bmesh.ops.triangulate(bm,faces=bm.faces[:])
    source=BVHTree.FromPolygons(source_vertices,[tuple(v.index for v in f.verts) for f in bm.faces],all_triangles=True)
    bmesh.ops.delete(bm,geom=[f for f in bm.faces if all(min(spec.point(*v.co))>2000 for v in f.verts)],context="FACES")
    bmesh.ops.delete(bm,geom=[v for v in bm.verts if not v.link_faces],context="VERTS")
    boundary=[e for e in bm.edges if e.is_boundary and all(min(spec.point(*v.co))>1500 for v in e.verts)]
    adjacent={}
    for e in boundary:
        for v in e.verts:
            adjacent.setdefault(v,[]).append(e)
    assert all(len(edges)==2 for edges in adjacent.values()),"Interior seam must be a loop"
    start=next(iter(adjacent))
    loop=[start]
    previous=None
    current=start
    while True:
        edge=next(e for e in adjacent[current] if e!=previous)
        vertex=edge.other_vert(current)
        if vertex==start:
            break
        loop.append(vertex)
        current,previous=vertex,edge
    assert len(loop)==len(adjacent),"Unexpected interior hole"
    polygon=[tuple(v.co[:2]) for v in loop]
    if sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(polygon,polygon[1:]+polygon[:1]))<0:
        loop.reverse()
        polygon.reverse()
    coordinates=[Vector(p) for p in polygon]
    edges=[(i,(i+1)%len(loop)) for i in range(len(loop))]
    for cut in cuts:
        offset=len(coordinates)
        dense=[]
        for a,b in zip(cut,cut[1:]+cut[:1]):
            a,b=Vector(a),Vector(b)
            count=max(1,math.ceil((b-a).length/300))
            dense.extend(a.lerp(b,i/count) for i in range(count))
        coordinates.extend(dense)
        edges.extend((offset+i,offset+(i+1)%len(dense)) for i in range(len(dense)))
    # 원본 이음매 띠는 보존하고 내부만 4m 격자로 다시 만든다.
    for x in range(-21200,42500,400):
        for y in range(-36800,36900,400):
            if inside(x,y,polygon) and not any(inside(x,y,p) for p in cuts):
                coordinates.append(Vector((x,y)))
    spacing=getattr(spec,"PAD_GRID_SPACING",0)
    if spacing:
        # 공터 평탄부와 굴곡 지면의 전이 띠만 국소 보강한다.
        for pad in spec.GROUND_PADS:
            cx,cy=pad["xy"]
            for x in range(math.floor((cx-3400)/spacing)*spacing,math.ceil((cx+3400)/spacing)*spacing+1,spacing):
                for y in range(math.floor((cy-3400)/spacing)*spacing,math.ceil((cy+3400)/spacing)*spacing+1,spacing):
                    if 1200<math.hypot(x-cx,y-cy)<3400 and inside(x,y,polygon) and not any(inside(x,y,p) for p in cuts):
                        coordinates.append(Vector((x,y)))
    verts,_,faces,orig,_,_=delaunay_2d_cdt(coordinates,edges,[list(range(len(loop)))],1,.001)
    new=[]
    for p,ids in zip(verts,orig):
        existing=[i for i in ids if i<len(loop)]
        if existing:
            new.append(loop[existing[0]])
        else:
            hit=source.ray_cast(Vector((*p,3000)),Vector((0,0,-1)),8000)
            new.append(bm.verts.new((*p,hit[0].z if hit[0] is not None else 0)))
    for face in faces:
        center=sum((verts[i] for i in face),Vector((0,0)))/len(face)
        if not any(inside(*center,p) for p in cuts):
            bm.faces.new([new[i] for i in face])
    bmesh.ops.delete(bm,geom=[v for v in bm.verts if not v.link_faces],context="VERTS")
    for v in bm.verts:
        if v in native:
            v.co=bl(native[v])
        else:
            x,y,h=v.co
            h=surface_height(x,y,h)
            v.co=bl(spec.point(x,y,h))
    bmesh.ops.triangulate(bm,faces=bm.faces[:])
    bmesh.ops.recalc_face_normals(bm,faces=bm.faces[:])
    bm.normal_update()
    if sum(f.normal.dot(-f.calc_center_median().normalized()) for f in bm.faces)<0:
        for f in bm.faces:
            f.normal_flip()
    report=stats(bm,False)
    bm.to_mesh(obj.data)
    bm.free()
    obj.matrix_world.identity()
    obj.name=f"SM_Octant_Grove_00_{VERSION}_R30000"
    export(obj,"Crust")
    SOURCES.append({"id":"Crust","topology":report,"interior_grid_cm":400})

def build():
    assert not (OUT/"manifest.json").exists(),"Choose a new version"
    OUT.mkdir(parents=True,exist_ok=True)
    ART.mkdir(parents=True,exist_ok=True)
    solids=[tree_body(tree) for tree in spec.TREES]
    boolean(solids[1],prism("CentralBranch",spec.BRANCH["outline"],spec.BRANCH["bottom"],spec.BRANCH["floor"]),"UNION")
    forest=solids[0]
    for solid in solids[1:]:
        boolean(forest,solid,"UNION")
    def verify_trunks(stage):
        tree=BVHTree.FromPolygons([v.co for v in forest.data.vertices],[tuple(p.vertices) for p in forest.data.polygons])
        tops=[]
        for item in spec.TREES:
            x,y=item["xy"]
            base=spec.ground(x,y)
            hit=tree.ray_cast(Vector((x,y,base+22000)),Vector((0,0,-1)),26000)
            top=hit[0].z-base if hit[0] is not None else None
            tops.append([item["id"],top])
            assert top is not None and top>item["height"]*.35,("Trunk lost during union",stage,tops)
        print("GROVE_UNION_TRUNKS",stage,json.dumps(tops),flush=True)
    verify_trunks("Trees")
    for wall in spec.ROOT_WALLS:
        boolean(forest,root_wall(wall),"UNION")
        verify_trunks(wall["id"])
    for wall in spec.CONNECTING_WALLS:
        boolean(forest,connecting_wall(wall),"UNION")
        verify_trunks(wall["id"])
    forest.name="ForestSource"
    cave=box("RootVaultSource",spec.CAVE["outer"])
    boolean(cave,box("RootVaultChamber",spec.CAVE["chamber"]),"DIFFERENCE")
    boolean(cave,box("RootVaultShaft",spec.CAVE["shaft"]),"DIFFERENCE")
    operand=forest.copy()
    operand.data=forest.data.copy()
    bpy.context.scene.collection.objects.link(operand)
    boolean(cave,operand,"DIFFERENCE")
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/"BeforeProjection.blend"),check_existing=False)
    finish(forest,"Forest",True)
    finish(cave,"RootVault")
    x0,x1,y0,y1,_,_=spec.CAVE["outer"]
    cuts=[spec.outline(tree,.995) for tree in spec.TREES]
    cuts.extend(spec.wall_outline(wall,2) for wall in spec.ROOT_WALLS)
    cuts.extend(spec.wall_outline({**wall,"ramp":wall["half_width"]-250},2) for wall in spec.CONNECTING_WALLS)
    cuts.append([(x0+2,y0+2),(x1-2,y0+2),(x1-2,y1-2),(x0+2,y1-2)])
    crust(cuts)
    for source in (forest,cave):
        source.hide_render=True
        source.hide_set(True)
    manifest={"name":"Grove_00","version":VERSION,"stage":"FullLayout","radius_cm":spec.RADIUS,
        "seam_margin_cm":spec.SEAM_MARGIN,"seam_transition_cm":spec.SEAM_BAND,"trees":spec.TREES,
        "branch":spec.BRANCH,"cave":spec.CAVE,"root_walls":spec.ROOT_WALLS,"connecting_walls":spec.CONNECTING_WALLS,"clearings":spec.CLEARINGS,"exports":EXPORTS,"sources":SOURCES,
        "limits":["Native seam snap required","No engine capsule, bake or PIE validation yet"]}
    (OUT/"manifest.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")
    (OUT/"placement.json").write_text(json.dumps(spec.placement(),indent=2),encoding="utf-8")
    bpy.ops.wm.save_as_mainfile(filepath=str(ART/f"Grove_00_{VERSION}.blend"),check_existing=False)
    print("GROVE_GEOMETRY_SUCCESS",json.dumps({"exports":len(EXPORTS),"triangles":sum(e["triangles"] for e in EXPORTS)}))

if __name__=="__main__":
    build()
